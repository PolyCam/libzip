/*
 libzip_source_call.c -- invoke callback command on libzip_source
 Copyright (C) 2009-2021 Dieter Baron and Thomas Klausner

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


libzip_int64_t
_libzip_source_call(libzip_source_t *src, void *data, libzip_uint64_t length, libzip_source_cmd_t command) {
    libzip_int64_t ret;

    if ((src->supports & LIBZIP_SOURCE_MAKE_COMMAND_BITMASK(command)) == 0) {
        libzip_error_set(&src->error, LIBZIP_ER_OPNOTSUPP, 0);
        return -1;
    }

    if (src->src == NULL) {
        ret = src->cb.f(src->ud, data, length, command);
    }
    else {
        ret = src->cb.l(src->src, src->ud, data, length, command);
    }

    if (ret < 0) {
        if (command != LIBZIP_SOURCE_ERROR && command != LIBZIP_SOURCE_SUPPORTS) {
            int e[2];

            if (_libzip_source_call(src, e, sizeof(e), LIBZIP_SOURCE_ERROR) < 0) {
                libzip_error_set(&src->error, LIBZIP_ER_INTERNAL, 0);
            }
            else {
                libzip_error_set(&src->error, e[0], e[1]);
            }
        }
    }

    return ret;
}
