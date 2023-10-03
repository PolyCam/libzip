/*
  libzip_error.c -- libzip_error_t helper functions
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

#include <stdlib.h>

#include "zipint.h"


LIBZIP_EXTERN int
libzip_error_code_system(const libzip_error_t *error) {
    return error->sys_err;
}


LIBZIP_EXTERN int
libzip_error_code_zip(const libzip_error_t *error) {
    return error->libzip_err;
}


LIBZIP_EXTERN void
libzip_error_fini(libzip_error_t *err) {
    free(err->str);
    err->str = NULL;
}


LIBZIP_EXTERN void
libzip_error_init(libzip_error_t *err) {
    err->libzip_err = LIBZIP_ER_OK;
    err->sys_err = 0;
    err->str = NULL;
}

LIBZIP_EXTERN void
libzip_error_init_with_code(libzip_error_t *error, int ze) {
    libzip_error_init(error);
    error->libzip_err = ze;
    switch (libzip_error_system_type(error)) {
        case LIBZIP_ET_SYS:
        case LIBZIP_ET_LIBZIP:
            error->sys_err = errno;
            break;
            
        default:
            error->sys_err = 0;
            break;
    }
}


LIBZIP_EXTERN int
libzip_error_system_type(const libzip_error_t *error) {
    if (error->libzip_err < 0 || error->libzip_err >= _libzip_err_str_count)
        return LIBZIP_ET_NONE;

    return _libzip_err_str[error->libzip_err].type;
}


void
_libzip_error_clear(libzip_error_t *err) {
    if (err == NULL)
        return;

    err->libzip_err = LIBZIP_ER_OK;
    err->sys_err = 0;
}


void
_libzip_error_copy(libzip_error_t *dst, const libzip_error_t *src) {
    if (dst == NULL) {
        return;
    }

    dst->libzip_err = src->libzip_err;
    dst->sys_err = src->sys_err;
}


void
_libzip_error_get(const libzip_error_t *err, int *zep, int *sep) {
    if (zep)
        *zep = err->libzip_err;
    if (sep) {
        if (libzip_error_system_type(err) != LIBZIP_ET_NONE)
            *sep = err->sys_err;
        else
            *sep = 0;
    }
}


void
libzip_error_set(libzip_error_t *err, int ze, int se) {
    if (err) {
        err->libzip_err = ze;
        err->sys_err = se;
    }
}


void
libzip_error_set_from_source(libzip_error_t *err, libzip_source_t *src) {
    if (src == NULL) {
        libzip_error_set(err, LIBZIP_ER_INVAL, 0);
        return;
    }

    _libzip_error_copy(err, libzip_source_error(src));
}


libzip_int64_t
libzip_error_to_data(const libzip_error_t *error, void *data, libzip_uint64_t length) {
    int *e = (int *)data;

    if (length < sizeof(int) * 2) {
        return -1;
    }

    e[0] = libzip_error_code_zip(error);
    e[1] = libzip_error_code_system(error);
    return sizeof(int) * 2;
}
