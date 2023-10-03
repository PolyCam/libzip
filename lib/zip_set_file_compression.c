/*
  libzip_set_file_compression.c -- set compression for file in archive
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


#include "zipint.h"


LIBZIP_EXTERN int
libzip_set_file_compression(libzip_t *za, libzip_uint64_t idx, libzip_int32_t method, libzip_uint32_t flags) {
    libzip_entry_t *e;
    libzip_int32_t old_method;

    if (idx >= za->nentry) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return -1;
    }

    if (LIBZIP_IS_RDONLY(za)) {
        libzip_error_set(&za->error, LIBZIP_ER_RDONLY, 0);
        return -1;
    }
    if (LIBZIP_WANT_TORRENTZIP(za)) {
        libzip_error_set(&za->error, LIBZIP_ER_NOT_ALLOWED, 0);
        return -1;
    }

    if (!libzip_compression_method_supported(method, true)) {
        libzip_error_set(&za->error, LIBZIP_ER_COMPNOTSUPP, 0);
        return -1;
    }

    e = za->entry + idx;

    old_method = (e->orig == NULL ? LIBZIP_CM_DEFAULT : e->orig->comp_method);

    /* TODO: do we want to recompress if level is set? Only if it's
     * different than what bit flags tell us, but those are not
     * defined for all compression methods, or not directly mappable
     * to levels */

    if (method == old_method) {
        if (e->changes) {
            e->changes->changed &= ~LIBZIP_DIRENT_COMP_METHOD;
            e->changes->compression_level = 0;
            if (e->changes->changed == 0) {
                _libzip_dirent_free(e->changes);
                e->changes = NULL;
            }
        }
    }
    else {
        if (e->changes == NULL) {
            if ((e->changes = _libzip_dirent_clone(e->orig)) == NULL) {
                libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        e->changes->comp_method = method;
        e->changes->compression_level = (libzip_uint16_t)flags;
        e->changes->changed |= LIBZIP_DIRENT_COMP_METHOD;
    }

    return 0;
}
