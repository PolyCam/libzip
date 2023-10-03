/*
  libzip_file_replace.c -- replace file via callback function
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


#include "zipint.h"


LIBZIP_EXTERN int
libzip_file_replace(libzip_t *za, libzip_uint64_t idx, libzip_source_t *source, libzip_flags_t flags) {
    if (idx >= za->nentry || source == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return -1;
    }

    if (_libzip_file_replace(za, idx, NULL, source, flags) == -1)
        return -1;

    return 0;
}


/* NOTE: Signed due to -1 on error.  See libzip_add.c for more details. */

libzip_int64_t
_libzip_file_replace(libzip_t *za, libzip_uint64_t idx, const char *name, libzip_source_t *source, libzip_flags_t flags) {
    libzip_uint64_t za_nentry_prev;

    if (LIBZIP_IS_RDONLY(za)) {
        libzip_error_set(&za->error, LIBZIP_ER_RDONLY, 0);
        return -1;
    }

    za_nentry_prev = za->nentry;
    if (idx == LIBZIP_UINT64_MAX) {
        libzip_int64_t i = -1;

        if (flags & LIBZIP_FL_OVERWRITE)
            i = _libzip_name_locate(za, name, flags, NULL);

        if (i == -1) {
            /* create and use new entry, used by libzip_add */
            if ((i = _libzip_add_entry(za)) < 0)
                return -1;
        }
        idx = (libzip_uint64_t)i;
    }

    if (name && _libzip_set_name(za, idx, name, flags) != 0) {
        if (za->nentry != za_nentry_prev) {
            _libzip_entry_finalize(za->entry + idx);
            za->nentry = za_nentry_prev;
        }
        return -1;
    }

    /* does not change any name related data, so we can do it here;
     * needed for a double add of the same file name */
    _libzip_unchange_data(za->entry + idx);

    if (za->entry[idx].orig != NULL && (za->entry[idx].changes == NULL || (za->entry[idx].changes->changed & LIBZIP_DIRENT_COMP_METHOD) == 0)) {
        if (za->entry[idx].changes == NULL) {
            if ((za->entry[idx].changes = _libzip_dirent_clone(za->entry[idx].orig)) == NULL) {
                libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        za->entry[idx].changes->comp_method = LIBZIP_CM_REPLACED_DEFAULT;
        za->entry[idx].changes->changed |= LIBZIP_DIRENT_COMP_METHOD;
    }

    za->entry[idx].source = source;

    return (libzip_int64_t)idx;
}
