/*
 libzip_file_set_mtime.c -- set modification time of entry.
 Copyright (C) 2014-2022 Dieter Baron and Thomas Klausner

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
libzip_file_set_dostime(libzip_t *za, libzip_uint64_t idx, libzip_uint16_t dtime, libzip_uint16_t ddate, libzip_flags_t flags) {
    time_t mtime;
    mtime = _libzip_d2u_time(dtime, ddate);
    return libzip_file_set_mtime(za, idx, mtime, flags);
}

LIBZIP_EXTERN int
libzip_file_set_mtime(libzip_t *za, libzip_uint64_t idx, time_t mtime, libzip_flags_t flags) {
    libzip_entry_t *e;

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

    e = za->entry + idx;

    if (e->orig != NULL && e->orig->encryption_method == LIBZIP_EM_TRAD_PKWARE && !LIBZIP_ENTRY_CHANGED(e, LIBZIP_DIRENT_ENCRYPTION_METHOD) && !LIBZIP_ENTRY_DATA_CHANGED(e)) {
        libzip_error_set(&za->error, LIBZIP_ER_OPNOTSUPP, 0);
        return -1;
    }

    if (e->changes == NULL) {
        if ((e->changes = _libzip_dirent_clone(e->orig)) == NULL) {
            libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
            return -1;
        }
    }

    e->changes->last_mod = mtime;
    e->changes->changed |= LIBZIP_DIRENT_LAST_MOD;

    return 0;
}
