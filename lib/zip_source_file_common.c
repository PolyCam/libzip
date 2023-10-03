/*
  libzip_source_file_common.c -- create data source from file
  Copyright (C) 1999-2021 Dieter Baron and Thomas Klausner

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zipint.h"

#include "zip_source_file.h"

static libzip_int64_t read_file(void *state, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd);

static void
libzip_source_file_stat_init(libzip_source_file_stat_t *st) {
    st->size = 0;
    st->mtime = time(NULL);
    st->exists = false;
    st->regular_file = false;
}

libzip_source_t *
libzip_source_file_common_new(const char *fname, void *file, libzip_uint64_t start, libzip_int64_t len, const libzip_stat_t *st, libzip_source_file_operations_t *ops, void *ops_userdata, libzip_error_t *error) {
    libzip_source_file_context_t *ctx;
    libzip_source_t *zs;
    libzip_source_file_stat_t sb;
    libzip_uint64_t length;

    if (ops == NULL) {
        libzip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if (ops->close == NULL || ops->read == NULL || ops->seek == NULL || ops->stat == NULL) {
        libzip_error_set(error, ZIP_ER_INTERNAL, 0);
        return NULL;
    }

    if (ops->write != NULL && (ops->commit_write == NULL || ops->create_temp_output == NULL || ops->remove == NULL || ops->rollback_write == NULL || ops->tell == NULL)) {
        libzip_error_set(error, ZIP_ER_INTERNAL, 0);
        return NULL;
    }

    if (fname != NULL) {
        if (ops->open == NULL || ops->string_duplicate == NULL) {
            libzip_error_set(error, ZIP_ER_INTERNAL, 0);
            return NULL;
        }
    }
    else if (file == NULL) {
        libzip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if (len < 0) {
        if (len == -1) {
            len = ZIP_LENGTH_TO_END;
        }
        // TODO: return ZIP_ER_INVAL if len != ZIP_LENGTH_UNCHECKED?
        length = 0;
    }
    else {
        length = (libzip_uint64_t)len;
    }

    if (start > ZIP_INT64_MAX || start + length < start) {
        libzip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((ctx = (libzip_source_file_context_t *)malloc(sizeof(libzip_source_file_context_t))) == NULL) {
        libzip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    ctx->ops = ops;
    ctx->ops_userdata = ops_userdata;
    ctx->fname = NULL;
    if (fname) {
        if ((ctx->fname = ops->string_duplicate(ctx, fname)) == NULL) {
            libzip_error_set(error, ZIP_ER_MEMORY, 0);
            free(ctx);
            return NULL;
        }
    }
    ctx->f = file;
    ctx->start = start;
    ctx->len = length;
    if (st) {
        (void)memcpy_s(&ctx->st, sizeof(ctx->st), st, sizeof(*st));
        ctx->st.name = NULL;
        ctx->st.valid &= ~ZIP_STAT_NAME;
    }
    else {
        libzip_stat_init(&ctx->st);
    }

    if (ctx->len > 0) {
        ctx->st.size = ctx->len;
        ctx->st.valid |= ZIP_STAT_SIZE;
    }

    libzip_error_init(&ctx->stat_error);

    ctx->tmpname = NULL;
    ctx->fout = NULL;

    libzip_error_init(&ctx->error);
    libzip_file_attributes_init(&ctx->attributes);

    ctx->supports = ZIP_SOURCE_SUPPORTS_READABLE | libzip_source_make_command_bitmap(ZIP_SOURCE_SUPPORTS, ZIP_SOURCE_TELL, ZIP_SOURCE_SUPPORTS_REOPEN, -1);

    libzip_source_file_stat_init(&sb);
    if (!ops->stat(ctx, &sb)) {
        _libzip_error_copy(error, &ctx->error);
        free(ctx->fname);
        free(ctx);
        return NULL;
    }

    if (!sb.exists) {
        if (ctx->fname && ctx->start == 0 && ctx->len == 0 && ops->write != NULL) {
            ctx->supports = ZIP_SOURCE_SUPPORTS_WRITABLE;
            /* libzip_open_from_source checks for this to detect non-existing files */
            libzip_error_set(&ctx->stat_error, ZIP_ER_READ, ENOENT);
        }
        else {
            libzip_error_set(&ctx->stat_error, ZIP_ER_READ, ENOENT);
            free(ctx->fname);
            free(ctx);
            return NULL;
        }
    }
    else {
        if ((ctx->st.valid & ZIP_STAT_MTIME) == 0) {
            ctx->st.mtime = sb.mtime;
            ctx->st.valid |= ZIP_STAT_MTIME;
        }
        if (sb.regular_file) {
            ctx->supports = ZIP_SOURCE_SUPPORTS_SEEKABLE;

            if (ctx->start + ctx->len > sb.size) {
                libzip_error_set(error, ZIP_ER_INVAL, 0);
                free(ctx->fname);
                free(ctx);
                return NULL;
            }

            if (ctx->len == 0) {
                if (len != ZIP_LENGTH_UNCHECKED) {
                    ctx->len = sb.size - ctx->start;
                    ctx->st.size = ctx->len;
                    ctx->st.valid |= ZIP_STAT_SIZE;
                }

                /* when using a partial file, don't allow writing */
                if (ctx->fname && start == 0 && ops->write != NULL) {
                    ctx->supports = ZIP_SOURCE_SUPPORTS_WRITABLE;
                }
            }
        }

        ctx->supports |= ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_GET_FILE_ATTRIBUTES);
    }

    ctx->supports |= ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ACCEPT_EMPTY);
    if (ops->create_temp_output_cloning != NULL) {
        if (ctx->supports & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_BEGIN_WRITE)) {
            ctx->supports |= ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_BEGIN_WRITE_CLONING);
        }
    }

    if ((zs = libzip_source_function_create(read_file, ctx, error)) == NULL) {
        free(ctx->fname);
        free(ctx);
        return NULL;
    }

    return zs;
}


static libzip_int64_t
read_file(void *state, void *data, libzip_uint64_t len, libzip_source_cmd_t cmd) {
    libzip_source_file_context_t *ctx;
    char *buf;

    ctx = (libzip_source_file_context_t *)state;
    buf = (char *)data;

    switch (cmd) {
    case ZIP_SOURCE_ACCEPT_EMPTY:
        return 0;

    case ZIP_SOURCE_BEGIN_WRITE:
        /* write support should not be set if fname is NULL */
        if (ctx->fname == NULL) {
            libzip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }
        return ctx->ops->create_temp_output(ctx);

    case ZIP_SOURCE_BEGIN_WRITE_CLONING:
        /* write support should not be set if fname is NULL */
        if (ctx->fname == NULL) {
            libzip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }
        return ctx->ops->create_temp_output_cloning(ctx, len);

    case ZIP_SOURCE_CLOSE:
        if (ctx->fname) {
            ctx->ops->close(ctx);
            ctx->f = NULL;
        }
        return 0;

    case ZIP_SOURCE_COMMIT_WRITE: {
        libzip_int64_t ret = ctx->ops->commit_write(ctx);
        ctx->fout = NULL;
        if (ret == 0) {
            free(ctx->tmpname);
            ctx->tmpname = NULL;
        }
        return ret;
    }

    case ZIP_SOURCE_ERROR:
        return libzip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
        free(ctx->fname);
        free(ctx->tmpname);
        if (ctx->f) {
            ctx->ops->close(ctx);
        }
        free(ctx);
        return 0;

    case ZIP_SOURCE_GET_FILE_ATTRIBUTES:
        if (len < sizeof(ctx->attributes)) {
            libzip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }
        (void)memcpy_s(data, sizeof(ctx->attributes), &ctx->attributes, sizeof(ctx->attributes));
        return sizeof(ctx->attributes);

    case ZIP_SOURCE_OPEN:
        if (ctx->fname) {
            if (ctx->ops->open(ctx) == false) {
                return -1;
            }
        }

        if (ctx->start > 0) { /* TODO: rewind on re-open */
            if (ctx->ops->seek(ctx, ctx->f, (libzip_int64_t)ctx->start, SEEK_SET) == false) {
                /* TODO: skip by reading */
                return -1;
            }
        }
        ctx->offset = 0;
        return 0;

    case ZIP_SOURCE_READ: {
        libzip_int64_t i;
        libzip_uint64_t n;

        if (ctx->len > 0) {
            n = ZIP_MIN(ctx->len - ctx->offset, len);
        }
        else {
            n = len;
        }

        if ((i = ctx->ops->read(ctx, buf, n)) < 0) {
            libzip_error_set(&ctx->error, ZIP_ER_READ, errno);
            return -1;
        }
        ctx->offset += (libzip_uint64_t)i;

        return i;
    }

    case ZIP_SOURCE_REMOVE:
        return ctx->ops->remove(ctx);

    case ZIP_SOURCE_ROLLBACK_WRITE:
        ctx->ops->rollback_write(ctx);
        ctx->fout = NULL;
        free(ctx->tmpname);
        ctx->tmpname = NULL;
        return 0;

    case ZIP_SOURCE_SEEK: {
        libzip_int64_t new_offset = libzip_source_seek_compute_offset(ctx->offset, ctx->len, data, len, &ctx->error);

        if (new_offset < 0) {
            return -1;
        }

        /* The actual offset inside the file must be representable as libzip_int64_t. */
        if (new_offset > ZIP_INT64_MAX - (libzip_int64_t)ctx->start) {
            libzip_error_set(&ctx->error, ZIP_ER_SEEK, EOVERFLOW);
            return -1;
        }

        ctx->offset = (libzip_uint64_t)new_offset;

        if (ctx->ops->seek(ctx, ctx->f, (libzip_int64_t)(ctx->offset + ctx->start), SEEK_SET) == false) {
            return -1;
        }
        return 0;
    }

    case ZIP_SOURCE_SEEK_WRITE: {
        libzip_source_args_seek_t *args;

        args = ZIP_SOURCE_GET_ARGS(libzip_source_args_seek_t, data, len, &ctx->error);
        if (args == NULL) {
            return -1;
        }

        if (ctx->ops->seek(ctx, ctx->fout, args->offset, args->whence) == false) {
            return -1;
        }
        return 0;
    }

    case ZIP_SOURCE_STAT: {
        if (len < sizeof(ctx->st))
            return -1;

        if (libzip_error_code_zip(&ctx->stat_error) != 0) {
            libzip_error_set(&ctx->error, libzip_error_code_zip(&ctx->stat_error), libzip_error_code_system(&ctx->stat_error));
            return -1;
        }

        (void)memcpy_s(data, sizeof(ctx->st), &ctx->st, sizeof(ctx->st));
        return sizeof(ctx->st);
    }

    case ZIP_SOURCE_SUPPORTS:
        return ctx->supports;

    case ZIP_SOURCE_TELL:
        return (libzip_int64_t)ctx->offset;

    case ZIP_SOURCE_TELL_WRITE:
        return ctx->ops->tell(ctx, ctx->fout);

    case ZIP_SOURCE_WRITE:
        return ctx->ops->write(ctx, data, len);

    default:
        libzip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}
