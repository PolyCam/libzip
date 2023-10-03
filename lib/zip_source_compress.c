/*
  libzip_source_compress.c -- (de)compression routines
  Copyright (C) 2017-2021 Dieter Baron and Thomas Klausner

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

struct context {
    libzip_error_t error;

    bool end_of_input;
    bool end_of_stream;
    bool can_store;
    bool is_stored; /* only valid if end_of_stream is true */
    bool compress;
    libzip_int32_t method;

    libzip_uint64_t size;
    libzip_int64_t first_read;
    libzip_uint8_t buffer[BUFSIZE];

    libzip_compression_algorithm_t *algorithm;
    void *ud;
};


struct implementation {
    libzip_uint16_t method;
    libzip_compression_algorithm_t *compress;
    libzip_compression_algorithm_t *decompress;
};

static struct implementation implementations[] = {
    {LIBZIP_CM_DEFLATE, &libzip_algorithm_deflate_compress, &libzip_algorithm_deflate_decompress},
#if defined(HAVE_LIBBZ2)
    {LIBZIP_CM_BZIP2, &libzip_algorithm_bzip2_compress, &libzip_algorithm_bzip2_decompress},
#endif
#if defined(HAVE_LIBLZMA)
    {LIBZIP_CM_LZMA, &libzip_algorithm_xz_compress, &libzip_algorithm_xz_decompress},
    /*  Disabled - because 7z isn't able to unpack ZIP+LZMA2
        archives made this way - and vice versa.

        {LIBZIP_CM_LZMA2, &libzip_algorithm_xz_compress, &libzip_algorithm_xz_decompress},
    */
    {LIBZIP_CM_XZ, &libzip_algorithm_xz_compress, &libzip_algorithm_xz_decompress},
#endif
#if defined(HAVE_LIBZSTD)
    {LIBZIP_CM_ZSTD, &libzip_algorithm_zstd_compress, &libzip_algorithm_zstd_decompress},
#endif

};

static size_t implementations_size = sizeof(implementations) / sizeof(implementations[0]);

static libzip_source_t *compression_source_new(libzip_t *za, libzip_source_t *src, libzip_int32_t method, bool compress, libzip_uint32_t compression_flags);
static libzip_int64_t compress_callback(libzip_source_t *, void *, void *, libzip_uint64_t, libzip_source_cmd_t);
static void context_free(struct context *ctx);
static struct context *context_new(libzip_int32_t method, bool compress, libzip_uint32_t compression_flags, libzip_compression_algorithm_t *algorithm);
static libzip_int64_t compress_read(libzip_source_t *, struct context *, void *, libzip_uint64_t);

libzip_compression_algorithm_t *
_libzip_get_compression_algorithm(libzip_int32_t method, bool compress) {
    size_t i;
    libzip_uint16_t real_method = LIBZIP_CM_ACTUAL(method);

    for (i = 0; i < implementations_size; i++) {
        if (implementations[i].method == real_method) {
            if (compress) {
                return implementations[i].compress;
            }
            else {
                return implementations[i].decompress;
            }
        }
    }

    return NULL;
}

LIBZIP_EXTERN int
libzip_compression_method_supported(libzip_int32_t method, int compress) {
    if (method == LIBZIP_CM_STORE) {
        return 1;
    }
    return _libzip_get_compression_algorithm(method, compress) != NULL;
}

libzip_source_t *
libzip_source_compress(libzip_t *za, libzip_source_t *src, libzip_int32_t method, libzip_uint32_t compression_flags) {
    return compression_source_new(za, src, method, true, compression_flags);
}

libzip_source_t *
libzip_source_decompress(libzip_t *za, libzip_source_t *src, libzip_int32_t method) {
    return compression_source_new(za, src, method, false, 0);
}


static libzip_source_t *
compression_source_new(libzip_t *za, libzip_source_t *src, libzip_int32_t method, bool compress, libzip_uint32_t compression_flags) {
    struct context *ctx;
    libzip_source_t *s2;
    libzip_compression_algorithm_t *algorithm = NULL;

    if (src == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((algorithm = _libzip_get_compression_algorithm(method, compress)) == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_COMPNOTSUPP, 0);
        return NULL;
    }

    if ((ctx = context_new(method, compress, compression_flags, algorithm)) == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((s2 = libzip_source_layered(za, src, compress_callback, ctx)) == NULL) {
        context_free(ctx);
        return NULL;
    }

    return s2;
}


static struct context *
context_new(libzip_int32_t method, bool compress, libzip_uint32_t compression_flags, libzip_compression_algorithm_t *algorithm) {
    struct context *ctx;

    if ((ctx = (struct context *)malloc(sizeof(*ctx))) == NULL) {
        return NULL;
    }
    libzip_error_init(&ctx->error);
    ctx->can_store = compress ? LIBZIP_CM_IS_DEFAULT(method) : false;
    ctx->algorithm = algorithm;
    ctx->method = method;
    ctx->compress = compress;
    ctx->end_of_input = false;
    ctx->end_of_stream = false;
    ctx->is_stored = false;

    if ((ctx->ud = ctx->algorithm->allocate(LIBZIP_CM_ACTUAL(method), compression_flags, &ctx->error)) == NULL) {
        libzip_error_fini(&ctx->error);
        free(ctx);
        return NULL;
    }

    return ctx;
}


static void
context_free(struct context *ctx) {
    if (ctx == NULL) {
        return;
    }

    ctx->algorithm->deallocate(ctx->ud);
    libzip_error_fini(&ctx->error);

    free(ctx);
}


static libzip_int64_t
compress_read(libzip_source_t *src, struct context *ctx, void *data, libzip_uint64_t len) {
    libzip_compression_status_t ret;
    bool end;
    libzip_int64_t n;
    libzip_uint64_t out_offset;
    libzip_uint64_t out_len;

    if (libzip_error_code_zip(&ctx->error) != LIBZIP_ER_OK) {
        return -1;
    }

    if (len == 0 || ctx->end_of_stream) {
        return 0;
    }

    out_offset = 0;

    end = false;
    while (!end && out_offset < len) {
        out_len = len - out_offset;
        ret = ctx->algorithm->process(ctx->ud, (libzip_uint8_t *)data + out_offset, &out_len);

        if (ret != LIBZIP_COMPRESSION_ERROR) {
            out_offset += out_len;
        }

        switch (ret) {
        case LIBZIP_COMPRESSION_END:
            ctx->end_of_stream = true;

            if (!ctx->end_of_input) {
                /* TODO: garbage after stream, or compression ended before all data read */
            }

            if (ctx->first_read < 0) {
                /* we got end of processed stream before reading any input data */
                libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
                end = true;
                break;
            }
            if (ctx->can_store && (libzip_uint64_t)ctx->first_read <= out_offset) {
                ctx->is_stored = true;
                ctx->size = (libzip_uint64_t)ctx->first_read;
                (void)memcpy_s(data, ctx->size, ctx->buffer, ctx->size);
                return (libzip_int64_t)ctx->size;
            }
            end = true;
            break;

        case LIBZIP_COMPRESSION_OK:
            break;

        case LIBZIP_COMPRESSION_NEED_DATA:
            if (ctx->end_of_input) {
                /* TODO: error: stream not ended, but no more input */
                end = true;
                break;
            }

            if ((n = libzip_source_read(src, ctx->buffer, sizeof(ctx->buffer))) < 0) {
                libzip_error_set_from_source(&ctx->error, src);
                end = true;
                break;
            }
            else if (n == 0) {
                ctx->end_of_input = true;
                ctx->algorithm->end_of_input(ctx->ud);
                if (ctx->first_read < 0) {
                    ctx->first_read = 0;
                }
            }
            else {
                if (ctx->first_read >= 0) {
                    /* we overwrote a previously filled ctx->buffer */
                    ctx->can_store = false;
                }
                else {
                    ctx->first_read = n;
                }

                ctx->algorithm->input(ctx->ud, ctx->buffer, (libzip_uint64_t)n);
            }
            break;

        case LIBZIP_COMPRESSION_ERROR:
            /* error set by algorithm */
            if (libzip_error_code_zip(&ctx->error) == LIBZIP_ER_OK) {
                libzip_error_set(&ctx->error, LIBZIP_ER_INTERNAL, 0);
            }
            end = true;
            break;
        }
    }

    if (out_offset > 0) {
        ctx->can_store = false;
        ctx->size += out_offset;
        return (libzip_int64_t)out_offset;
    }

    return (libzip_error_code_zip(&ctx->error) == LIBZIP_ER_OK) ? 0 : -1;
}


static libzip_int64_t
compress_callback(libzip_source_t *src, void *ud, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd) {
    struct context *ctx;

    ctx = (struct context *)ud;

    switch (cmd) {
    case LIBZIP_SOURCE_OPEN: {
        libzip_stat_t st;
        libzip_file_attributes_t attributes;
        
        ctx->size = 0;
        ctx->end_of_input = false;
        ctx->end_of_stream = false;
        ctx->is_stored = false;
        ctx->first_read = -1;
        
        if (libzip_source_stat(src, &st) < 0 || libzip_source_get_file_attributes(src, &attributes) < 0) {
            libzip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        if (!ctx->algorithm->start(ctx->ud, &st, &attributes)) {
            return -1;
        }

        return 0;
    }

    case LIBZIP_SOURCE_READ:
        return compress_read(src, ctx, data, len);

    case LIBZIP_SOURCE_CLOSE:
        if (!ctx->algorithm->end(ctx->ud)) {
            return -1;
        }
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;

        if (ctx->compress) {
            if (ctx->end_of_stream) {
                st->comp_method = ctx->is_stored ? LIBZIP_CM_STORE : LIBZIP_CM_ACTUAL(ctx->method);
                st->comp_size = ctx->size;
                st->valid |= LIBZIP_STAT_COMP_SIZE | LIBZIP_STAT_COMP_METHOD;
            }
            else {
                st->valid &= ~(LIBZIP_STAT_COMP_SIZE | LIBZIP_STAT_COMP_METHOD);
            }
        }
        else {
            st->comp_method = LIBZIP_CM_STORE;
            st->valid |= LIBZIP_STAT_COMP_METHOD;
            st->valid &= ~LIBZIP_STAT_COMP_SIZE;
            if (ctx->end_of_stream) {
                st->size = ctx->size;
                st->valid |= LIBZIP_STAT_SIZE;
            }
        }
    }
        return 0;

    case LIBZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, len);

    case LIBZIP_SOURCE_FREE:
        context_free(ctx);
        return 0;

    case LIBZIP_SOURCE_GET_FILE_ATTRIBUTES: {
        libzip_file_attributes_t *attributes = (libzip_file_attributes_t *)data;

        if (len < sizeof(*attributes)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_INVAL, 0);
            return -1;
        }

        attributes->valid |= LIBZIP_FILE_ATTRIBUTES_VERSION_NEEDED | LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS;
        attributes->version_needed = ctx->algorithm->version_needed;
        attributes->general_purpose_bit_mask = LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS_ALLOWED_MASK;
        attributes->general_purpose_bit_flags = (ctx->is_stored ? 0 : ctx->algorithm->general_purpose_bit_flags(ctx->ud));

        return sizeof(*attributes);
    }

    case LIBZIP_SOURCE_SUPPORTS:
        return LIBZIP_SOURCE_SUPPORTS_READABLE | libzip_source_make_command_bitmap(LIBZIP_SOURCE_GET_FILE_ATTRIBUTES, LIBZIP_SOURCE_SUPPORTS_REOPEN, -1);

    default:
        return libzip_source_pass_to_lower_layer(src, data, len, cmd);
    }
}
