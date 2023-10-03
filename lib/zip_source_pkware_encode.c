/*
  libzip_source_pkware_encode.c -- Traditional PKWARE encryption routines
  Copyright (C) 2009-2020 Dieter Baron and Thomas Klausner

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

struct trad_pkware {
    char *password;
    libzip_pkware_keys_t keys;
    libzip_buffer_t *buffer;
    bool eof;
    bool mtime_set;
    time_t mtime;
    libzip_error_t error;
};


static int encrypt_header(libzip_source_t *, struct trad_pkware *);
static libzip_int64_t pkware_encrypt(libzip_source_t *, void *, void *, libzip_uint64_t, libzip_source_cmd_t);
static void trad_pkware_free(struct trad_pkware *);
static struct trad_pkware *trad_pkware_new(const char *password, libzip_error_t *error);
static void set_mtime(struct trad_pkware* ctx, libzip_stat_t* st);

libzip_source_t *
libzip_source_pkware_encode(libzip_t *za, libzip_source_t *src, libzip_uint16_t em, int flags, const char *password) {
    struct trad_pkware *ctx;
    libzip_source_t *s2;

    if (password == NULL || src == NULL || em != LIBZIP_EM_TRAD_PKWARE) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }
    if (!(flags & LIBZIP_CODEC_ENCODE)) {
        libzip_error_set(&za->error, LIBZIP_ER_ENCRNOTSUPP, 0);
        return NULL;
    }

    if ((ctx = trad_pkware_new(password, &za->error)) == NULL) {
        return NULL;
    }

    if ((s2 = libzip_source_layered(za, src, pkware_encrypt, ctx)) == NULL) {
        trad_pkware_free(ctx);
        return NULL;
    }

    return s2;
}


static int
encrypt_header(libzip_source_t *src, struct trad_pkware *ctx) {
    unsigned short dostime, dosdate;
    libzip_uint8_t *header;

    if (!ctx->mtime_set) {
        struct libzip_stat st;
        if (libzip_source_stat(src, &st) != 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }
        set_mtime(ctx, &st);
    }

    _libzip_u2d_time(ctx->mtime, &dostime, &dosdate);

    if ((ctx->buffer = _libzip_buffer_new(NULL, LIBZIP_CRYPTO_PKWARE_HEADERLEN)) == NULL) {
        libzip_error_set(&ctx->error, LIBZIP_ER_MEMORY, 0);
        return -1;
    }

    header = _libzip_buffer_data(ctx->buffer);

    /* generate header from random bytes and mtime
       see appnote.iz, XIII. Decryption, Step 2, last paragraph */
    if (!libzip_secure_random(header, LIBZIP_CRYPTO_PKWARE_HEADERLEN - 1)) {
        libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
        _libzip_buffer_free(ctx->buffer);
        ctx->buffer = NULL;
        return -1;
    }
    header[LIBZIP_CRYPTO_PKWARE_HEADERLEN - 1] = (libzip_uint8_t)((dostime >> 8) & 0xff);

    _libzip_pkware_encrypt(&ctx->keys, header, header, LIBZIP_CRYPTO_PKWARE_HEADERLEN);

    return 0;
}


static libzip_int64_t
pkware_encrypt(libzip_source_t *src, void *ud, void *data, libzip_uint64_t length, libzip_source_cmd_t cmd) {
    struct trad_pkware *ctx;
    libzip_int64_t n;
    libzip_uint64_t buffer_n;

    ctx = (struct trad_pkware *)ud;

    switch (cmd) {
    case LIBZIP_SOURCE_OPEN:
        ctx->eof = false;

        /* initialize keys */
        _libzip_pkware_keys_reset(&ctx->keys);
        _libzip_pkware_encrypt(&ctx->keys, NULL, (const libzip_uint8_t *)ctx->password, strlen(ctx->password));

        if (encrypt_header(src, ctx) < 0) {
            return -1;
        }
        return 0;

    case LIBZIP_SOURCE_READ:
        buffer_n = 0;

        if (ctx->buffer) {
            /* write header values to data */
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

        if ((n = libzip_source_read(src, data, length)) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        _libzip_pkware_encrypt(&ctx->keys, (libzip_uint8_t *)data, (libzip_uint8_t *)data, (libzip_uint64_t)n);

        if ((libzip_uint64_t)n < length) {
            ctx->eof = true;
        }

        return (libzip_int64_t)buffer_n + n;

    case LIBZIP_SOURCE_CLOSE:
        _libzip_buffer_free(ctx->buffer);
        ctx->buffer = NULL;
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;
        st->encryption_method = LIBZIP_EM_TRAD_PKWARE;
        st->valid |= LIBZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & LIBZIP_STAT_COMP_SIZE) {
            st->comp_size += LIBZIP_CRYPTO_PKWARE_HEADERLEN;
        }
        set_mtime(ctx, st);
        st->mtime = ctx->mtime;
        st->valid |= LIBZIP_STAT_MTIME;

        return 0;
    }

    case LIBZIP_SOURCE_GET_FILE_ATTRIBUTES: {
        libzip_file_attributes_t *attributes = (libzip_file_attributes_t *)data;
        if (length < sizeof(*attributes)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_INVAL, 0);
            return -1;
        }
        attributes->valid |= LIBZIP_FILE_ATTRIBUTES_VERSION_NEEDED;
        attributes->version_needed = 20;

        return 0;
    }

    case LIBZIP_SOURCE_SUPPORTS:
        return libzip_source_make_command_bitmap(LIBZIP_SOURCE_OPEN, LIBZIP_SOURCE_READ, LIBZIP_SOURCE_CLOSE, LIBZIP_SOURCE_STAT, LIBZIP_SOURCE_ERROR, LIBZIP_SOURCE_FREE, LIBZIP_SOURCE_GET_FILE_ATTRIBUTES, -1);

    case LIBZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, length);

    case LIBZIP_SOURCE_FREE:
        trad_pkware_free(ctx);
        return 0;

    default:
        return libzip_source_pass_to_lower_layer(src, data, length, cmd);
    }
}


static struct trad_pkware *
trad_pkware_new(const char *password, libzip_error_t *error) {
    struct trad_pkware *ctx;

    if ((ctx = (struct trad_pkware *)malloc(sizeof(*ctx))) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->password = strdup(password)) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        free(ctx);
        return NULL;
    }
    ctx->buffer = NULL;
    ctx->mtime_set = false;
    ctx->mtime = 0;
    libzip_error_init(&ctx->error);

    return ctx;
}


static void
trad_pkware_free(struct trad_pkware *ctx) {
    if (ctx == NULL) {
        return;
    }

    free(ctx->password);
    _libzip_buffer_free(ctx->buffer);
    libzip_error_fini(&ctx->error);
    free(ctx);
}


static void set_mtime(struct trad_pkware* ctx, libzip_stat_t* st) {
    if (!ctx->mtime_set) {
        if (st->valid & LIBZIP_STAT_MTIME) {
            ctx->mtime = st->mtime;
        }
        else {
            time(&ctx->mtime);
        }
        ctx->mtime_set = true;
    }
}
