/*
  libzip_source_window.c -- return part of lower source
  Copyright (C) 2012-2021 Dieter Baron and Thomas Klausner

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

struct window {
    libzip_uint64_t start; /* where in file we start reading */
    libzip_uint64_t end;   /* where in file we stop reading */
    bool end_valid;     /* whether end is set, otherwise read until EOF */

    /* if not NULL, read file data for this file */
    libzip_t *source_archive;
    libzip_uint64_t source_index;

    libzip_uint64_t offset; /* offset in src for next read */

    libzip_stat_t stat;
    libzip_uint64_t stat_invalid;
    libzip_file_attributes_t attributes;
    libzip_error_t error;
    libzip_int64_t supports;
    bool needs_seek;
};

static libzip_int64_t window_read(libzip_source_t *, void *, void *, libzip_uint64_t, libzip_source_cmd_t);


ZIP_EXTERN libzip_source_t *
libzip_source_window_create(libzip_source_t *src, libzip_uint64_t start, libzip_int64_t len, libzip_error_t *error) {
    return _libzip_source_window_new(src, start, len, NULL, 0, NULL, NULL, 0, false, error);
}


libzip_source_t *
_libzip_source_window_new(libzip_source_t *src, libzip_uint64_t start, libzip_int64_t length, libzip_stat_t *st, libzip_uint64_t st_invalid, libzip_file_attributes_t *attributes, libzip_t *source_archive, libzip_uint64_t source_index, bool take_ownership, libzip_error_t *error) {
    libzip_source_t* window_source;
    struct window *ctx;

    if (src == NULL || length < -1 || (source_archive == NULL && source_index != 0)) {
        libzip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    
    if (length >= 0) {
        if (start + (libzip_uint64_t)length < start) {
            libzip_error_set(error, ZIP_ER_INVAL, 0);
            return NULL;
        }
    }

    if ((ctx = (struct window *)malloc(sizeof(*ctx))) == NULL) {
        libzip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    ctx->start = start;
    if (length == -1) {
        ctx->end_valid = false;
    }
    else {
        ctx->end = start + (libzip_uint64_t)length;
        ctx->end_valid = true;
    }
    libzip_stat_init(&ctx->stat);
    ctx->stat_invalid = st_invalid;
    if (attributes != NULL) {
        (void)memcpy_s(&ctx->attributes, sizeof(ctx->attributes), attributes, sizeof(ctx->attributes));
    }
    else {
        libzip_file_attributes_init(&ctx->attributes);
    }
    ctx->source_archive = source_archive;
    ctx->source_index = source_index;
    libzip_error_init(&ctx->error);
    ctx->supports = (libzip_source_supports(src) & (ZIP_SOURCE_SUPPORTS_SEEKABLE | ZIP_SOURCE_SUPPORTS_REOPEN)) | (libzip_source_make_command_bitmap(ZIP_SOURCE_GET_FILE_ATTRIBUTES, ZIP_SOURCE_SUPPORTS, ZIP_SOURCE_TELL, ZIP_SOURCE_FREE, -1));
    ctx->needs_seek = (ctx->supports & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SEEK)) ? true : false;

    if (st) {
        if (_libzip_stat_merge(&ctx->stat, st, error) < 0) {
            free(ctx);
            return NULL;
        }
    }
    
    window_source = libzip_source_layered_create(src, window_read, ctx, error);
    if (window_source != NULL && !take_ownership) {
        libzip_source_keep(src);
    }
    return window_source;
}


int
_libzip_source_set_source_archive(libzip_source_t *src, libzip_t *za) {
    src->source_archive = za;
    return _libzip_register_source(za, src);
}


/* called by libzip_discard to avoid operating on file from closed archive */
void
_libzip_source_invalidate(libzip_source_t *src) {
    src->source_closed = 1;

    if (libzip_error_code_zip(&src->error) == ZIP_ER_OK) {
        libzip_error_set(&src->error, ZIP_ER_ZIPCLOSED, 0);
    }
}


static libzip_int64_t
window_read(libzip_source_t *src, void *_ctx, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd) {
    struct window *ctx;
    libzip_int64_t ret;
    libzip_uint64_t n, i;

    ctx = (struct window *)_ctx;

    switch (cmd) {
    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
        free(ctx);
        return 0;

    case ZIP_SOURCE_OPEN:
        if (ctx->source_archive) {
            libzip_uint64_t offset;

            if ((offset = _libzip_file_get_offset(ctx->source_archive, ctx->source_index, &ctx->error)) == 0) {
                return -1;
            }
            if (ctx->end + offset < ctx->end) {
                /* zip archive data claims end of data past zip64 limits */
                libzip_error_set(&ctx->error, ZIP_ER_INCONS, MAKE_DETAIL_WITH_INDEX(ZIP_ER_DETAIL_CDIR_ENTRY_INVALID, ctx->source_index));
                return -1;
            }
            ctx->start += offset;
            ctx->end += offset;
            ctx->source_archive = NULL;
        }

        if (!ctx->needs_seek) {
            DEFINE_BYTE_ARRAY(b, BUFSIZE);

            if (!byte_array_init(b, BUFSIZE)) {
                libzip_error_set(&ctx->error, ZIP_ER_MEMORY, 0);
                return -1;
            }

            for (n = 0; n < ctx->start; n += (libzip_uint64_t)ret) {
                i = (ctx->start - n > BUFSIZE ? BUFSIZE : ctx->start - n);
                if ((ret = libzip_source_read(src, b, i)) < 0) {
                    libzip_error_set_from_source(&ctx->error, src);
                    byte_array_fini(b);
                    return -1;
                }
                if (ret == 0) {
                    libzip_error_set(&ctx->error, ZIP_ER_EOF, 0);
                    byte_array_fini(b);
                    return -1;
                }
            }

            byte_array_fini(b);
        }

        ctx->offset = ctx->start;
        return 0;

    case ZIP_SOURCE_READ:
        if (ctx->end_valid && len > ctx->end - ctx->offset) {
            len = ctx->end - ctx->offset;
        }

        if (len == 0) {
            return 0;
        }

        if (ctx->needs_seek) {
            if (libzip_source_seek(src, (libzip_int64_t)ctx->offset, SEEK_SET) < 0) {
                libzip_error_set_from_source(&ctx->error, src);
                return -1;
            }
        }

        if ((ret = libzip_source_read(src, data, len)) < 0) {
            libzip_error_set(&ctx->error, ZIP_ER_EOF, 0);
            return -1;
        }

        ctx->offset += (libzip_uint64_t)ret;

        if (ret == 0) {
            if (ctx->end_valid && ctx->offset < ctx->end) {
                libzip_error_set(&ctx->error, ZIP_ER_EOF, 0);
                return -1;
            }
        }
        return ret;

    case ZIP_SOURCE_SEEK: {
        libzip_int64_t new_offset;
        
        if (!ctx->end_valid) {
            libzip_source_args_seek_t *args = ZIP_SOURCE_GET_ARGS(libzip_source_args_seek_t, data, len, &ctx->error);
            
            if (args == NULL) {
                return -1;
            }
            if (args->whence == SEEK_END) {
                if (libzip_source_seek(src, args->offset, args->whence) < 0) {
                    libzip_error_set_from_source(&ctx->error, src);
                    return -1;
                }
                new_offset = libzip_source_tell(src);
                if (new_offset < 0) {
                    libzip_error_set_from_source(&ctx->error, src);
                    return -1;
                }
                if ((libzip_uint64_t)new_offset < ctx->start) {
                    libzip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
                    (void)libzip_source_seek(src, (libzip_int64_t)ctx->offset, SEEK_SET);
                    return -1;
                }
                ctx->offset = (libzip_uint64_t)new_offset;
                return 0;
            }
        }

        new_offset = libzip_source_seek_compute_offset(ctx->offset - ctx->start, ctx->end - ctx->start, data, len, &ctx->error);
        
        if (new_offset < 0) {
            return -1;
        }
        
        ctx->offset = (libzip_uint64_t)new_offset + ctx->start;
        return 0;
    }

    case ZIP_SOURCE_STAT: {
        libzip_stat_t *st;

        st = (libzip_stat_t *)data;

        if (_libzip_stat_merge(st, &ctx->stat, &ctx->error) < 0) {
            return -1;
        }

        if (!(ctx->stat.valid & ZIP_STAT_SIZE)) {
            if (ctx->end_valid) {
                st->valid |= ZIP_STAT_SIZE;
                st->size = ctx->end - ctx->start;
            }
            else if (st->valid & ZIP_STAT_SIZE) {
                st->size -= ctx->start;
            }
        }

        st->valid &= ~ctx->stat_invalid;

        return 0;
    }

    case ZIP_SOURCE_GET_FILE_ATTRIBUTES:
        if (len < sizeof(ctx->attributes)) {
            libzip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }

        (void)memcpy_s(data, sizeof(ctx->attributes), &ctx->attributes, sizeof(ctx->attributes));
        return sizeof(ctx->attributes);

    case ZIP_SOURCE_SUPPORTS:
        return ctx->supports;

    case ZIP_SOURCE_TELL:
        return (libzip_int64_t)(ctx->offset - ctx->start);

    default:
        return libzip_source_pass_to_lower_layer(src, data, len, cmd);
    }
}


void
_libzip_deregister_source(libzip_t *za, libzip_source_t *src) {
    unsigned int i;

    for (i = 0; i < za->nopen_source; i++) {
        if (za->open_source[i] == src) {
            za->open_source[i] = za->open_source[za->nopen_source - 1];
            za->nopen_source--;
            break;
        }
    }
}


int
_libzip_register_source(libzip_t *za, libzip_source_t *src) {
    libzip_source_t **open_source;

    if (za->nopen_source + 1 >= za->nopen_source_alloc) {
        unsigned int n;
        n = za->nopen_source_alloc + 10;
        open_source = (libzip_source_t **)realloc(za->open_source, n * sizeof(libzip_source_t *));
        if (open_source == NULL) {
            libzip_error_set(&za->error, ZIP_ER_MEMORY, 0);
            return -1;
        }
        za->nopen_source_alloc = n;
        za->open_source = open_source;
    }

    za->open_source[za->nopen_source++] = src;

    return 0;
}
