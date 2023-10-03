/*
  libzip_source_winlibzip_aes_decode.c -- Winzip AES decryption routines
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

    libzip_uint64_t data_length;
    libzip_uint64_t current_position;

    libzip_winlibzip_aes_t *aes_ctx;
    libzip_error_t error;
};


static int decrypt_header(libzip_source_t *src, struct winlibzip_aes *ctx);
static void winlibzip_aes_free(struct winlibzip_aes *);
static libzip_int64_t winlibzip_aes_decrypt(libzip_source_t *src, void *ud, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd);
static struct winlibzip_aes *winlibzip_aes_new(libzip_uint16_t encryption_method, const char *password, libzip_error_t *error);


libzip_source_t *
libzip_source_winlibzip_aes_decode(libzip_t *za, libzip_source_t *src, libzip_uint16_t encryption_method, int flags, const char *password) {
    libzip_source_t *s2;
    libzip_stat_t st;
    libzip_uint64_t aux_length;
    struct winlibzip_aes *ctx;

    if ((encryption_method != LIBZIP_EM_AES_128 && encryption_method != LIBZIP_EM_AES_192 && encryption_method != LIBZIP_EM_AES_256) || password == NULL || src == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }
    if (flags & LIBZIP_CODEC_ENCODE) {
        libzip_error_set(&za->error, LIBZIP_ER_ENCRNOTSUPP, 0);
        return NULL;
    }

    if (libzip_source_stat(src, &st) != 0) {
        libzip_error_set_from_source(&za->error, src);
        return NULL;
    }

    aux_length = WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH + SALT_LENGTH(encryption_method) + HMAC_LENGTH;

    if ((st.valid & LIBZIP_STAT_COMP_SIZE) == 0 || st.comp_size < aux_length) {
        libzip_error_set(&za->error, LIBZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }

    if ((ctx = winlibzip_aes_new(encryption_method, password, &za->error)) == NULL) {
        return NULL;
    }

    ctx->data_length = st.comp_size - aux_length;

    if ((s2 = libzip_source_layered(za, src, winlibzip_aes_decrypt, ctx)) == NULL) {
        winlibzip_aes_free(ctx);
        return NULL;
    }

    return s2;
}


static int
decrypt_header(libzip_source_t *src, struct winlibzip_aes *ctx) {
    libzip_uint8_t header[WINLIBZIP_AES_MAX_HEADER_LENGTH];
    libzip_uint8_t password_verification[WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH];
    unsigned int headerlen;
    libzip_int64_t n;

    headerlen = WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH + SALT_LENGTH(ctx->encryption_method);
    if ((n = libzip_source_read(src, header, headerlen)) < 0) {
        libzip_error_set_from_source(&ctx->error, src);
        return -1;
    }

    if (n != headerlen) {
        libzip_error_set(&ctx->error, LIBZIP_ER_EOF, 0);
        return -1;
    }

    if ((ctx->aes_ctx = _libzip_winlibzip_aes_new((libzip_uint8_t *)ctx->password, strlen(ctx->password), header, ctx->encryption_method, password_verification, &ctx->error)) == NULL) {
        return -1;
    }
    if (memcmp(password_verification, header + SALT_LENGTH(ctx->encryption_method), WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH) != 0) {
        _libzip_winlibzip_aes_free(ctx->aes_ctx);
        ctx->aes_ctx = NULL;
        libzip_error_set(&ctx->error, LIBZIP_ER_WRONGPASSWD, 0);
        return -1;
    }
    return 0;
}


static bool
verify_hmac(libzip_source_t *src, struct winlibzip_aes *ctx) {
    unsigned char computed[LIBZIP_CRYPTO_SHA1_LENGTH], from_file[HMAC_LENGTH];
    if (libzip_source_read(src, from_file, HMAC_LENGTH) < HMAC_LENGTH) {
        libzip_error_set_from_source(&ctx->error, src);
        return false;
    }

    if (!_libzip_winlibzip_aes_finish(ctx->aes_ctx, computed)) {
        libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
        return false;
    }
    _libzip_winlibzip_aes_free(ctx->aes_ctx);
    ctx->aes_ctx = NULL;

    if (memcmp(from_file, computed, HMAC_LENGTH) != 0) {
        libzip_error_set(&ctx->error, LIBZIP_ER_CRC, 0);
        return false;
    }

    return true;
}


static libzip_int64_t
winlibzip_aes_decrypt(libzip_source_t *src, void *ud, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd) {
    struct winlibzip_aes *ctx;
    libzip_int64_t n;

    ctx = (struct winlibzip_aes *)ud;

    switch (cmd) {
    case LIBZIP_SOURCE_OPEN:
        if (decrypt_header(src, ctx) < 0) {
            return -1;
        }
        ctx->current_position = 0;
        return 0;

    case LIBZIP_SOURCE_READ:
        if (len > ctx->data_length - ctx->current_position) {
            len = ctx->data_length - ctx->current_position;
        }

        if (len == 0) {
            if (!verify_hmac(src, ctx)) {
                return -1;
            }
            return 0;
        }

        if ((n = libzip_source_read(src, data, len)) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }
        ctx->current_position += (libzip_uint64_t)n;

        if (!_libzip_winlibzip_aes_decrypt(ctx->aes_ctx, (libzip_uint8_t *)data, (libzip_uint64_t)n)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
            return -1;
        }

        return n;

    case LIBZIP_SOURCE_CLOSE:
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;

        st->encryption_method = LIBZIP_EM_NONE;
        st->valid |= LIBZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & LIBZIP_STAT_COMP_SIZE) {
            st->comp_size -= 12 + SALT_LENGTH(ctx->encryption_method);
        }

        return 0;
    }

    case LIBZIP_SOURCE_SUPPORTS:
        return libzip_source_make_command_bitmap(LIBZIP_SOURCE_OPEN, LIBZIP_SOURCE_READ, LIBZIP_SOURCE_CLOSE, LIBZIP_SOURCE_STAT, LIBZIP_SOURCE_ERROR, LIBZIP_SOURCE_FREE, LIBZIP_SOURCE_SUPPORTS_REOPEN, -1);

    case LIBZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, len);

    case LIBZIP_SOURCE_FREE:
        winlibzip_aes_free(ctx);
        return 0;

    default:
        return libzip_source_pass_to_lower_layer(src, data, len, cmd);
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
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        free(ctx);
        return NULL;
    }

    ctx->encryption_method = encryption_method;
    ctx->aes_ctx = NULL;

    libzip_error_init(&ctx->error);

    return ctx;
}
