/*
  libzip_source_winlibzip_aes_encode.c -- Winzip AES encryption routines
  Copyright (C) 2009-2021 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.
  3. The names of the authors may not be used to endorse or promote
  products derived from this software without specific prior
  written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdlib.h>
#include <string.h>

#include "zipint.h"
#include "zip_crypto.h"

struct winlibzip_aes {
    char *password;
    libzip_uint16_t encryption_method;

    libzip_uint8_t data[LIBZIP_MAX(WINLIBZIP_AES_MAX_HEADER_LENGTH, LIBZIP_CRYPTO_SHA1_LENGTH)];
    libzip_buffer_t *buffer;

    libzip_winlibzip_aes_t *aes_ctx;
    bool eof;
    libzip_error_t error;
};


static int encrypt_header(libzip_source_t *src, struct winlibzip_aes *ctx);
static void winlibzip_aes_free(struct winlibzip_aes *);
static libzip_int64_t winlibzip_aes_encrypt(libzip_source_t *src, void *ud, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd);
static struct winlibzip_aes *winlibzip_aes_new(libzip_uint16_t encryption_method, const char *password, libzip_error_t *error);


libzip_source_t *
libzip_source_winlibzip_aes_encode(libzip_t *za, libzip_source_t *src, libzip_uint16_t encryption_method, int flags, const char *password) {
    libzip_source_t *s2;
    struct winlibzip_aes *ctx;

    if ((encryption_method != LIBZIP_EM_AES_128 && encryption_method != LIBZIP_EM_AES_192 && encryption_method != LIBZIP_EM_AES_256) || password == NULL || src == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((ctx = winlibzip_aes_new(encryption_method, password, &za->error)) == NULL) {
        return NULL;
    }

    if ((s2 = libzip_source_layered(za, src, winlibzip_aes_encrypt, ctx)) == NULL) {
        winlibzip_aes_free(ctx);
        return NULL;
    }

    return s2;
}


static int
encrypt_header(libzip_source_t *src, struct winlibzip_aes *ctx) {
    libzip_uint16_t salt_length = SALT_LENGTH(ctx->encryption_method);
    /* TODO: The memset() is just for testing the memory sanitizer,
       libzip_secure_random() will overwrite the same bytes */
    memset(ctx->data, 0xff, salt_length);
    if (!libzip_secure_random(ctx->data, salt_length)) {
        libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
        return -1;
    }

    /* TODO: The memset() is just for testing the memory sanitizer,
       _libzip_winlibzip_aes_new() will overwrite the same bytes */
    memset(ctx->data + salt_length, 0xff, WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH);
    if ((ctx->aes_ctx = _libzip_winlibzip_aes_new((libzip_uint8_t *)ctx->password, strlen(ctx->password), ctx->data, ctx->encryption_method, ctx->data + salt_length, &ctx->error)) == NULL) {
        return -1;
    }

    if ((ctx->buffer = _libzip_buffer_new(ctx->data, salt_length + WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH)) == NULL) {
        _libzip_winlibzip_aes_free(ctx->aes_ctx);
        ctx->aes_ctx = NULL;
        libzip_error_set(&ctx->error, LIBZIP_ER_MEMORY, 0);
        return -1;
    }

    return 0;
}


static libzip_int64_t
winlibzip_aes_encrypt(libzip_source_t *src, void *ud, void *data, libzip_uint64_t length, libzip_source_cmd_t cmd) {
    struct winlibzip_aes *ctx;
    libzip_int64_t ret;
    libzip_uint64_t buffer_n;

    ctx = (struct winlibzip_aes *)ud;

    switch (cmd) {
    case LIBZIP_SOURCE_OPEN:
        ctx->eof = false;
        if (encrypt_header(src, ctx) < 0) {
            return -1;
        }
        return 0;

    case LIBZIP_SOURCE_READ:
        buffer_n = 0;

        if (ctx->buffer) {
            buffer_n = _libzip_buffer_read(ctx->buffer, data, length);

            data = (libzip_uint8_t *)data + buffer_n;
            length -= buffer_n;

            if (_libzip_buffer_eof(ctx->buffer)) {
                _libzip_buffer_free(ctx->buffer);
                ctx->buffer = NULL;
            }
        }

        if (ctx->eof) {
            return (libzip_int64_t)buffer_n;
        }

        if ((ret = libzip_source_read(src, data, length)) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        if (!_libzip_winlibzip_aes_encrypt(ctx->aes_ctx, data, (libzip_uint64_t)ret)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
            /* TODO: return partial read? */
            return -1;
        }

        if ((libzip_uint64_t)ret < length) {
            ctx->eof = true;
            if (!_libzip_winlibzip_aes_finish(ctx->aes_ctx, ctx->data)) {
                libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
                /* TODO: return partial read? */
                return -1;
            }
            _libzip_winlibzip_aes_free(ctx->aes_ctx);
            ctx->aes_ctx = NULL;
            if ((ctx->buffer = _libzip_buffer_new(ctx->data, HMAC_LENGTH)) == NULL) {
                libzip_error_set(&ctx->error, LIBZIP_ER_MEMORY, 0);
                /* TODO: return partial read? */
                return -1;
            }
            buffer_n += _libzip_buffer_read(ctx->buffer, (libzip_uint8_t *)data + ret, length - (libzip_uint64_t)ret);
        }

        return (libzip_int64_t)(buffer_n + (libzip_uint64_t)ret);

    case LIBZIP_SOURCE_CLOSE:
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;
        st->encryption_method = ctx->encryption_method;
        st->valid |= LIBZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & LIBZIP_STAT_COMP_SIZE) {
            st->comp_size += 12 + SALT_LENGTH(ctx->encryption_method);
        }

        return 0;
    }

    case LIBZIP_SOURCE_GET_FILE_ATTRIBUTES: {
        libzip_file_attributes_t *attributes = (libzip_file_attributes_t *)data;
        if (length < sizeof(*attributes)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_INVAL, 0);
            return -1;
        }
        attributes->valid |= LIBZIP_FILE_ATTRIBUTES_VERSION_NEEDED;
        attributes->version_needed = 51;

        return 0;
    }

    case LIBZIP_SOURCE_SUPPORTS:
        return libzip_source_make_command_bitmap(LIBZIP_SOURCE_OPEN, LIBZIP_SOURCE_READ, LIBZIP_SOURCE_CLOSE, LIBZIP_SOURCE_STAT, LIBZIP_SOURCE_ERROR, LIBZIP_SOURCE_FREE, LIBZIP_SOURCE_GET_FILE_ATTRIBUTES, -1);

    case LIBZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, length);

    case LIBZIP_SOURCE_FREE:
        winlibzip_aes_free(ctx);
        return 0;

    default:
        return libzip_source_pass_to_lower_layer(src, data, length, cmd);
    }
}


static void
winlibzip_aes_free(struct winlibzip_aes *ctx) {
    if (ctx == NULL) {
        return;
    }

    _libzip_crypto_clear(ctx->password, strlen(ctx->password));
    free(ctx->password);
    libzip_error_fini(&ctx->error);
    _libzip_buffer_free(ctx->buffer);
    _libzip_winlibzip_aes_free(ctx->aes_ctx);
    free(ctx);
}


static struct winlibzip_aes *
winlibzip_aes_new(libzip_uint16_t encryption_method, const char *password, libzip_error_t *error) {
    struct winlibzip_aes *ctx;

    if ((ctx = (struct winlibzip_aes *)malloc(sizeof(*ctx))) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->password = strdup(password)) == NULL) {
        free(ctx);
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    ctx->encryption_method = encryption_method;
    ctx->buffer = NULL;
    ctx->aes_ctx = NULL;

    libzip_error_init(&ctx->error);

    ctx->eof = false;
    return ctx;
}
