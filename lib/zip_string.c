/*
  libzip_string.c -- string handling (with encoding)
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
#include <zlib.h>

#include "zipint.h"

libzip_uint32_t
_libzip_string_crc32(const libzip_string_t *s) {
    libzip_uint32_t crc;

    crc = (libzip_uint32_t)crc32(0L, Z_NULL, 0);

    if (s != NULL)
        crc = (libzip_uint32_t)crc32(crc, s->raw, s->length);

    return crc;
}


int
_libzip_string_equal(const libzip_string_t *a, const libzip_string_t *b) {
    if (a == NULL || b == NULL)
        return a == b;

    if (a->length != b->length)
        return 0;

    /* TODO: encoding */

    return (memcmp(a->raw, b->raw, a->length) == 0);
}


void
_libzip_string_free(libzip_string_t *s) {
    if (s == NULL)
        return;

    free(s->raw);
    free(s->converted);
    free(s);
}


const libzip_uint8_t *
_libzip_string_get(libzip_string_t *string, libzip_uint32_t *lenp, libzip_flags_t flags, libzip_error_t *error) {
    static const libzip_uint8_t empty[1] = "";

    if (string == NULL) {
        if (lenp)
            *lenp = 0;
        return empty;
    }

    if ((flags & LIBZIP_FL_ENC_RAW) == 0) {
        /* start guessing */
        if (string->encoding == LIBZIP_ENCODING_UNKNOWN) {
            /* guess encoding, sets string->encoding */
            (void)_libzip_guess_encoding(string, LIBZIP_ENCODING_UNKNOWN);
        }

        if (((flags & LIBZIP_FL_ENC_STRICT) && string->encoding != LIBZIP_ENCODING_ASCII && string->encoding != LIBZIP_ENCODING_UTF8_KNOWN) || (string->encoding == LIBZIP_ENCODING_CP437)) {
            if (string->converted == NULL) {
                if ((string->converted = _libzip_cp437_to_utf8(string->raw, string->length, &string->converted_length, error)) == NULL)
                    return NULL;
            }
            if (lenp)
                *lenp = string->converted_length;
            return string->converted;
        }
    }

    if (lenp)
        *lenp = string->length;
    return string->raw;
}


libzip_uint16_t
_libzip_string_length(const libzip_string_t *s) {
    if (s == NULL)
        return 0;

    return s->length;
}


libzip_string_t *
_libzip_string_new(const libzip_uint8_t *raw, libzip_uint16_t length, libzip_flags_t flags, libzip_error_t *error) {
    libzip_string_t *s;
    libzip_encoding_type_t expected_encoding;

    if (length == 0)
        return NULL;

    switch (flags & LIBZIP_FL_ENCODING_ALL) {
    case LIBZIP_FL_ENC_GUESS:
        expected_encoding = LIBZIP_ENCODING_UNKNOWN;
        break;
    case LIBZIP_FL_ENC_UTF_8:
        expected_encoding = LIBZIP_ENCODING_UTF8_KNOWN;
        break;
    case LIBZIP_FL_ENC_CP437:
        expected_encoding = LIBZIP_ENCODING_CP437;
        break;
    default:
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((s = (libzip_string_t *)malloc(sizeof(*s))) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((s->raw = (libzip_uint8_t *)malloc((size_t)length + 1)) == NULL) {
        free(s);
        return NULL;
    }

    (void)memcpy_s(s->raw, length + 1, raw, length);
    s->raw[length] = '\0';
    s->length = length;
    s->encoding = LIBZIP_ENCODING_UNKNOWN;
    s->converted = NULL;
    s->converted_length = 0;

    if (expected_encoding != LIBZIP_ENCODING_UNKNOWN) {
        if (_libzip_guess_encoding(s, expected_encoding) == LIBZIP_ENCODING_ERROR) {
            _libzip_string_free(s);
            libzip_error_set(error, LIBZIP_ER_INVAL, 0);
            return NULL;
        }
    }

    return s;
}


int
_libzip_string_write(libzip_t *za, const libzip_string_t *s) {
    if (s == NULL)
        return 0;

    return _libzip_write(za, s->raw, s->length);
}
