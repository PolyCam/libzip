/*
  libzip_file_set_comment.c -- set comment for file in archive
  Copyright (C) 2006-2021 Dieter Baron and Thomas Klausner

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
libzip_file_set_comment(libzip_t *za, libzip_uint64_t idx, const char *comment, libzip_uint16_t len, libzip_flags_t flags) {
    libzip_entry_t *e;
    libzip_string_t *cstr;
    int changed;

    if (_libzip_get_dirent(za, idx, 0, NULL) == NULL)
        return -1;

    if (LIBZIP_IS_RDONLY(za)) {
        libzip_error_set(&za->error, LIBZIP_ER_RDONLY, 0);
        return -1;
    }
    if (LIBZIP_WANT_TORRENTZIP(za)) {
        libzip_error_set(&za->error, LIBZIP_ER_NOT_ALLOWED, 0);
        return -1;
    }

    if (len > 0 && comment == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
        return -1;
    }

    if (len > 0) {
        if ((cstr = _libzip_string_new((const libzip_uint8_t *)comment, len, flags, &za->error)) == NULL)
            return -1;
        if ((flags & LIBZIP_FL_ENCODING_ALL) == LIBZIP_FL_ENC_GUESS && _libzip_guess_encoding(cstr, LIBZIP_ENCODING_UNKNOWN) == LIBZIP_ENCODING_UTF8_GUESSED)
            cstr->encoding = LIBZIP_ENCODING_UTF8_KNOWN;
    }
    else
        cstr = NULL;

    e = za->entry + idx;

    if (e->changes) {
        _libzip_string_free(e->changes->comment);
        e->changes->comment = NULL;
        e->changes->changed &= ~LIBZIP_DIRENT_COMMENT;
    }

    if (e->orig && e->orig->comment)
        changed = !_libzip_string_equal(e->orig->comment, cstr);
    else
        changed = (cstr != NULL);

    if (changed) {
        if (e->changes == NULL) {
            if ((e->changes = _libzip_dirent_clone(e->orig)) == NULL) {
                libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
                _libzip_string_free(cstr);
                return -1;
            }
        }
        e->changes->comment = cstr;
        e->changes->changed |= LIBZIP_DIRENT_COMMENT;
    }
    else {
        _libzip_string_free(cstr);
        if (e->changes && e->changes->changed == 0) {
            _libzip_dirent_free(e->changes);
            e->changes = NULL;
        }
    }

    return 0;
}
