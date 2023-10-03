/*
  windows-open.c -- open zip archive using Windows UTF-16/Unicode file name
  Copyright (C) 2015-2022 Dieter Baron and Thomas Klausner

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

#include <libzip.h>

libzip_t *
windows_open(const wchar_t *name, int flags) {
    libzip_source_t *src;
    libzip_t *za;
    libzip_error_t error;

    libzip_error_init(&error);
    /* create source from buffer */
    if ((src = libzip_source_win32w_create(name, 0, -1, &error)) == NULL) {
        fprintf(stderr, "can't create source: %s\n", libzip_error_strerror(&error));
        libzip_error_fini(&error);
        return NULL;
    }

    /* open zip archive from source */
    if ((za = libzip_open_from_source(src, flags, &error)) == NULL) {
        fprintf(stderr, "can't open zip from source: %s\n", libzip_error_strerror(&error));
        libzip_source_free(src);
        libzip_error_fini(&error);
        return NULL;
    }
    libzip_error_fini(&error);

    return za;
}
