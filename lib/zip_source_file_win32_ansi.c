/*
  libzip_source_file_win32_ansi.c -- source for Windows file opened by ANSI name
  Copyright (C) 1999-2020 Dieter Baron and Thomas Klausner

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

#include "zip_source_file_win32.h"

static char *ansi_allocate_tempname(const char *name, size_t extra_chars, size_t *lengthp);
static void ansi_make_tempname(char *buf, size_t len, const char *name, libzip_uint32_t i);

/* clang-format off */
DONT_WARN_INCOMPATIBLE_FN_PTR_BEGIN

libzip_win32_file_operations_t ops_ansi = {
    ansi_allocate_tempname,
    CreateFileA,
    DeleteFileA,
    GetFileAttributesA,
    GetFileAttributesExA,
    ansi_make_tempname,
    MoveFileExA,
    SetFileAttributesA,
    strdup
};

DONT_WARN_INCOMPATIBLE_FN_PTR_END
/* clang-format on */

LIBZIP_EXTERN libzip_source_t *
libzip_source_win32a(libzip_t *za, const char *fname, libzip_uint64_t start, libzip_int64_t len) {
    if (za == NULL)
        return NULL;

    return libzip_source_win32a_create(fname, start, len, &za->error);
}


LIBZIP_EXTERN libzip_source_t *
libzip_source_win32a_create(const char *fname, libzip_uint64_t start, libzip_int64_t length, libzip_error_t *error) {
    if (fname == NULL || length < LIBZIP_LENGTH_UNCHECKED) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    return libzip_source_file_common_new(fname, NULL, start, length, NULL, &_libzip_source_file_win32_named_ops, &ops_ansi, error);
}


static char *
ansi_allocate_tempname(const char *name, size_t extra_chars, size_t *lengthp) {
    *lengthp = strlen(name) + extra_chars;
    return (char *)malloc(*lengthp);
}


static void
ansi_make_tempname(char *buf, size_t len, const char *name, libzip_uint32_t i) {
    snprintf_s(buf, len, "%s.%08x", name, i);
}
