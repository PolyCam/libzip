/*
  libzip_set_name.c -- rename helper function
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
#include <string.h>

#include "zipint.h"


int
_libzip_set_name(libzip_t *za, libzip_uint64_t idx, const char *name, libzip_flags_t flags) {
    libzip_entry_t *e;
    libzip_string_t *str;
    bool same_as_orig;
    libzip_int64_t i;
    const libzip_uint8_t *old_name, *new_name;
    libzip_string_t *old_str;

    if (idx >= za->nentry) {
        libzip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    if (ZIP_IS_RDONLY(za)) {
        libzip_error_set(&za->error, ZIP_ER_RDONLY, 0);
        return -1;
    }

    if (name && name[0] != '\0') {
        /* TODO: check for string too long */
        if ((str = _libzip_string_new((const libzip_uint8_t *)name, (libzip_uint16_t)strlen(name), flags, &za->error)) == NULL)
            return -1;
        if ((flags & ZIP_FL_ENCODING_ALL) == ZIP_FL_ENC_GUESS && _libzip_guess_encoding(str, ZIP_ENCODING_UNKNOWN) == ZIP_ENCODING_UTF8_GUESSED)
            str->encoding = ZIP_ENCODING_UTF8_KNOWN;
    }
    else
        str = NULL;

    /* TODO: encoding flags needed for CP437? */
    if ((i = _libzip_name_locate(za, name, 0, NULL)) >= 0 && (libzip_uint64_t)i != idx) {
        _libzip_string_free(str);
        libzip_error_set(&za->error, ZIP_ER_EXISTS, 0);
        return -1;
    }

    /* no effective name change */
    if (i >= 0 && (libzip_uint64_t)i == idx) {
        _libzip_string_free(str);
        return 0;
    }

    e = za->entry + idx;

    if (e->orig)
        same_as_orig = _libzip_string_equal(e->orig->filename, str);
    else
        same_as_orig = false;

    if (!same_as_orig && e->changes == NULL) {
        if ((e->changes = _libzip_dirent_clone(e->orig)) == NULL) {
            libzip_error_set(&za->error, ZIP_ER_MEMORY, 0);
            _libzip_string_free(str);
            return -1;
        }
    }

    if ((new_name = _libzip_string_get(same_as_orig ? e->orig->filename : str, NULL, 0, &za->error)) == NULL) {
        _libzip_string_free(str);
        return -1;
    }

    if (e->changes) {
        old_str = e->changes->filename;
    }
    else if (e->orig) {
        old_str = e->orig->filename;
    }
    else {
        old_str = NULL;
    }

    if (old_str) {
        if ((old_name = _libzip_string_get(old_str, NULL, 0, &za->error)) == NULL) {
            _libzip_string_free(str);
            return -1;
        }
    }
    else {
        old_name = NULL;
    }

    if (_libzip_hash_add(za->names, new_name, idx, 0, &za->error) == false) {
        _libzip_string_free(str);
        return -1;
    }
    if (old_name) {
        _libzip_hash_delete(za->names, old_name, NULL);
    }

    if (same_as_orig) {
        if (e->changes) {
            if (e->changes->changed & ZIP_DIRENT_FILENAME) {
                _libzip_string_free(e->changes->filename);
                e->changes->changed &= ~ZIP_DIRENT_FILENAME;
                if (e->changes->changed == 0) {
                    _libzip_dirent_free(e->changes);
                    e->changes = NULL;
                }
                else {
                    /* TODO: what if not cloned? can that happen? */
                    e->changes->filename = e->orig->filename;
                }
            }
        }
        _libzip_string_free(str);
    }
    else {
        if (e->changes->changed & ZIP_DIRENT_FILENAME) {
            _libzip_string_free(e->changes->filename);
        }
        e->changes->changed |= ZIP_DIRENT_FILENAME;
        e->changes->filename = str;
    }

    return 0;
}
