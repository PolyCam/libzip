/*
  libzip_set_archive_comment.c -- set archive comment
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
libzip_set_archive_comment(libzip_t *za, const char *comment, libzip_uint16_t len) {
    libzip_string_t *cstr;

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
        if ((cstr = _libzip_string_new((const libzip_uint8_t *)comment, len, LIBZIP_FL_ENC_GUESS, &za->error)) == NULL)
            return -1;

        if (_libzip_guess_encoding(cstr, LIBZIP_ENCODING_UNKNOWN) == LIBZIP_ENCODING_CP437) {
            _libzip_string_free(cstr);
            libzip_error_set(&za->error, LIBZIP_ER_INVAL, 0);
            return -1;
        }
    }
    else
        cstr = NULL;

    _libzip_string_free(za->comment_changes);
    za->comment_changes = NULL;

    if (((za->comment_orig && _libzip_string_equal(za->comment_orig, cstr)) || (za->comment_orig == NULL && cstr == NULL))) {
        _libzip_string_free(cstr);
        za->comment_changed = 0;
    }
    else {
        za->comment_changes = cstr;
        za->comment_changed = 1;
    }

    return 0;
}
