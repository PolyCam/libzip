/*
  libzip_source_file_stdio.c -- read-only stdio file source implementation
  Copyright (C) 2020 Dieter Baron and Thomas Klausner

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

#include "zipint.h"

#include "zip_source_file.h"
#include "zip_source_file_stdio.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#endif

/* clang-format off */
static libzip_source_file_operations_t ops_stdio_read = {
    _libzip_stdio_op_close,
    NULL,
    NULL,
    NULL,
    NULL,
    _libzip_stdio_op_read,
    NULL,
    NULL,
    _libzip_stdio_op_seek,
    _libzip_stdio_op_stat,
    NULL,
    _libzip_stdio_op_tell,
    NULL
};
/* clang-format on */


LIBZIP_EXTERN libzip_source_t *
libzip_source_filep(libzip_t *za, FILE *file, libzip_uint64_t start, libzip_int64_t len) {
    if (za == NULL) {
        return NULL;
    }

    return libzip_source_filep_create(file, start, len, &za->error);
}


LIBZIP_EXTERN libzip_source_t *
libzip_source_filep_create(FILE *file, libzip_uint64_t start, libzip_int64_t length, libzip_error_t *error) {
    if (file == NULL || length < LIBZIP_LENGTH_UNCHECKED) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    return libzip_source_file_common_new(NULL, file, start, length, NULL, &ops_stdio_read, NULL, error);
}


void
_libzip_stdio_op_close(libzip_source_file_context_t *ctx) {
    fclose((FILE *)ctx->f);
}


libzip_int64_t
_libzip_stdio_op_read(libzip_source_file_context_t *ctx, void *buf, libzip_uint64_t len) {
    size_t i;
    if (len > SIZE_MAX) {
        len = SIZE_MAX;
    }

    if ((i = fread(buf, 1, (size_t)len, ctx->f)) == 0) {
        if (ferror((FILE *)ctx->f)) {
            libzip_error_set(&ctx->error, LIBZIP_ER_READ, errno);
            return -1;
        }
    }

    return (libzip_int64_t)i;
}


bool
_libzip_stdio_op_seek(libzip_source_file_context_t *ctx, void *f, libzip_int64_t offset, int whence) {
#if LIBZIP_FSEEK_MAX > LIBZIP_INT64_MAX
    if (offset > LIBZIP_FSEEK_MAX || offset < LIBZIP_FSEEK_MIN) {
        libzip_error_set(&ctx->error, LIBZIP_ER_SEEK, EOVERFLOW);
        return false;
    }
#endif

    if (fseeko((FILE *)f, (off_t)offset, whence) < 0) {
        libzip_error_set(&ctx->error, LIBZIP_ER_SEEK, errno);
        return false;
    }
    return true;
}


bool
_libzip_stdio_op_stat(libzip_source_file_context_t *ctx, libzip_source_file_stat_t *st) {
    struct stat sb;

    int ret;

    if (ctx->fname) {
        ret = stat(ctx->fname, &sb);
    }
    else {
        ret = fstat(fileno((FILE *)ctx->f), &sb);
    }

    if (ret < 0) {
        if (errno == ENOENT) {
            st->exists = false;
            return true;
        }
        libzip_error_set(&ctx->error, LIBZIP_ER_READ, errno);
        return false;
    }

    st->size = (libzip_uint64_t)sb.st_size;
    st->mtime = sb.st_mtime;

    st->regular_file = S_ISREG(sb.st_mode);
    st->exists = true;

    /* We're using UNIX file API, even on Windows; thus, we supply external file attributes with Unix values. */
    /* TODO: This could be improved on Windows by providing Windows-specific file attributes */
    ctx->attributes.valid = LIBZIP_FILE_ATTRIBUTES_HOST_SYSTEM | LIBZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES;
    ctx->attributes.host_system = LIBZIP_OPSYS_UNIX;
    ctx->attributes.external_file_attributes = (((libzip_uint32_t)sb.st_mode) << 16) | ((sb.st_mode & S_IWUSR) ? 0 : 1);

    return true;
}


libzip_int64_t
_libzip_stdio_op_tell(libzip_source_file_context_t *ctx, void *f) {
    off_t offset = ftello((FILE *)f);

    if (offset < 0) {
        libzip_error_set(&ctx->error, LIBZIP_ER_SEEK, errno);
    }

    return offset;
}
