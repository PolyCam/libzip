/*
  libzip_source_crc.c -- pass-through source that calculates CRC32 and size
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


#include <limits.h>
#include <stdlib.h>
#include <zlib.h>

#include "zipint.h"

struct crc_context {
    int validate;     /* whether to check CRC on EOF and return error on mismatch */
    int crc_complete; /* whether CRC was computed for complete file */
    libzip_error_t error;
    libzip_uint64_t size;
    libzip_uint64_t position;     /* current reading position */
    libzip_uint64_t crc_position; /* how far we've computed the CRC */
    libzip_uint32_t crc;
};

static libzip_int64_t crc_read(libzip_source_t *, void *, void *, libzip_uint64_t, libzip_source_cmd_t);


libzip_source_t *
libzip_source_crc_create(libzip_source_t *src, int validate, libzip_error_t *error) {
    struct crc_context *ctx;

    if (src == NULL) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((ctx = (struct crc_context *)malloc(sizeof(*ctx))) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    libzip_error_init(&ctx->error);
    ctx->validate = validate;
    ctx->crc_complete = 0;
    ctx->crc_position = 0;
    ctx->crc = (libzip_uint32_t)crc32(0, NULL, 0);
    ctx->size = 0;

    return libzip_source_layered_create(src, crc_read, ctx, error);
}


static libzip_int64_t
crc_read(libzip_source_t *src, void *_ctx, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd) {
    struct crc_context *ctx;
    libzip_int64_t n;

    ctx = (struct crc_context *)_ctx;

    switch (cmd) {
    case LIBZIP_SOURCE_OPEN:
        ctx->position = 0;
        return 0;

    case LIBZIP_SOURCE_READ:
        if ((n = libzip_source_read(src, data, len)) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        if (n == 0) {
            if (ctx->crc_position == ctx->position) {
                ctx->crc_complete = 1;
                ctx->size = ctx->position;

                if (ctx->validate) {
                    struct libzip_stat st;

                    if (libzip_source_stat(src, &st) < 0) {
                        libzip_error_set_from_source(&ctx->error, src);
                        return -1;
                    }

                    if ((st.valid & LIBZIP_STAT_CRC) && st.crc != ctx->crc) {
                        libzip_error_set(&ctx->error, LIBZIP_ER_CRC, 0);
                        return -1;
                    }
                    if ((st.valid & LIBZIP_STAT_SIZE) && st.size != ctx->size) {
                        /* We don't have the index here, but the caller should know which file they are reading from. */
                        libzip_error_set(&ctx->error, LIBZIP_ER_INCONS, MAKE_DETAIL_WITH_INDEX(LIBZIP_ER_DETAIL_INVALID_FILE_LENGTH, MAX_DETAIL_INDEX));
                        return -1;
                    }
                }
            }
        }
        else if (!ctx->crc_complete && ctx->position <= ctx->crc_position) {
            libzip_uint64_t i, nn;

            for (i = ctx->crc_position - ctx->position; i < (libzip_uint64_t)n; i += nn) {
                nn = LIBZIP_MIN(UINT_MAX, (libzip_uint64_t)n - i);

                ctx->crc = (libzip_uint32_t)crc32(ctx->crc, (const Bytef *)data + i, (uInt)nn);
                ctx->crc_position += nn;
            }
        }
        ctx->position += (libzip_uint64_t)n;
        return n;

    case LIBZIP_SOURCE_CLOSE:
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;

        if (ctx->crc_complete) {
            if ((st->valid & LIBZIP_STAT_SIZE) && st->size != ctx->size) {
                libzip_error_set(&ctx->error, LIBZIP_ER_DATA_LENGTH, 0);
                return -1;
            }
            /* TODO: Set comp_size, comp_method, encryption_method?
                    After all, this only works for uncompressed data. */
            st->size = ctx->size;
            st->crc = ctx->crc;
            st->comp_size = ctx->size;
            st->comp_method = LIBZIP_CM_STORE;
            st->encryption_method = LIBZIP_EM_NONE;
            st->valid |= LIBZIP_STAT_SIZE | LIBZIP_STAT_CRC | LIBZIP_STAT_COMP_SIZE | LIBZIP_STAT_COMP_METHOD | LIBZIP_STAT_ENCRYPTION_METHOD;
        }
        return 0;
    }

    case LIBZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, len);

    case LIBZIP_SOURCE_FREE:
        free(ctx);
        return 0;

    case LIBZIP_SOURCE_SUPPORTS: {
        libzip_int64_t mask = libzip_source_supports(src);

        if (mask < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        mask &= ~libzip_source_make_command_bitmap(LIBZIP_SOURCE_BEGIN_WRITE, LIBZIP_SOURCE_COMMIT_WRITE, LIBZIP_SOURCE_ROLLBACK_WRITE, LIBZIP_SOURCE_SEEK_WRITE, LIBZIP_SOURCE_TELL_WRITE, LIBZIP_SOURCE_REMOVE, LIBZIP_SOURCE_GET_FILE_ATTRIBUTES, -1);
        mask |= libzip_source_make_command_bitmap(LIBZIP_SOURCE_FREE, -1);
        return mask;
    }

    case LIBZIP_SOURCE_SEEK: {
        libzip_int64_t new_position;
        libzip_source_args_seek_t *args = LIBZIP_SOURCE_GET_ARGS(libzip_source_args_seek_t, data, len, &ctx->error);

        if (args == NULL) {
            return -1;
        }
        if (libzip_source_seek(src, args->offset, args->whence) < 0 || (new_position = libzip_source_tell(src)) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        ctx->position = (libzip_uint64_t)new_position;

        return 0;
    }

    case LIBZIP_SOURCE_TELL:
        return (libzip_int64_t)ctx->position;

    default:
        return libzip_source_pass_to_lower_layer(src, data, len, cmd);
    }
}
