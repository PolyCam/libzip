/*
  libzip_name_locate.c -- get index by name
  Copyright (C) 1999-2022 Dieter Baron and Thomas Klausner

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


#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "zipint.h"


LIBZIP_EXTERN libzip_int64_t
libzip_name_locate(libzip_t *za, const char *fname, libzip_flags_t flags) {
    return _libzip_name_locate(za, fname, flags, &za->error);
}


libzip_int64_t
_libzip_name_locate(libzip_t *za, const char *fname, libzip_flags_t flags, libzip_error_t *error) {
    int (*cmp)(const char *, const char *);
    size_t fname_length;
    libzip_string_t *str = NULL;
    const char *fn, *p;
    libzip_uint64_t i;

    if (za == NULL) {
        return -1;
    }

    if (fname == NULL) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return -1;
    }

    fname_length = strlen(fname);

    if (fname_length > LIBZIP_UINT16_MAX) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return -1;
    }

    if ((flags & (LIBZIP_FL_ENC_UTF_8 | LIBZIP_FL_ENC_RAW)) == 0 && fname[0] != '\0') {
        if ((str = _libzip_string_new((const libzip_uint8_t *)fname, (libzip_uint16_t)strlen(fname), flags, error)) == NULL) {
            return -1;
        }
        if ((fname = (const char *)_libzip_string_get(str, NULL, 0, error)) == NULL) {
            _libzip_string_free(str);
            return -1;
        }
    }

    if (flags & (LIBZIP_FL_NOCASE | LIBZIP_FL_NODIR | LIBZIP_FL_ENC_RAW | LIBZIP_FL_ENC_STRICT)) {
        /* can't use hash table */
        cmp = (flags & LIBZIP_FL_NOCASE) ? strcasecmp : strcmp;

        for (i = 0; i < za->nentry; i++) {
            fn = _libzip_get_name(za, i, flags, error);

            /* newly added (partially filled) entry or error */
            if (fn == NULL)
                continue;

            if (flags & LIBZIP_FL_NODIR) {
                p = strrchr(fn, '/');
                if (p)
                    fn = p + 1;
            }

            if (cmp(fname, fn) == 0) {
                _libzip_error_clear(error);
                _libzip_string_free(str);
                return (libzip_int64_t)i;
            }
        }

        libzip_error_set(error, LIBZIP_ER_NOENT, 0);
        _libzip_string_free(str);
        return -1;
    }
    else {
        libzip_int64_t ret = _libzip_hash_lookup(za->names, (const libzip_uint8_t *)fname, flags, error);
        _libzip_string_free(str);
        return ret;
    }
}
