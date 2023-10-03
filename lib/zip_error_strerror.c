/*
  libzip_error_sterror.c -- get string representation of struct libzip_error
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "zipint.h"

ZIP_EXTERN const char *
libzip_error_strerror(libzip_error_t *err) {
    const char *libzip_error_string, *system_error_string;
    char *s;
    char *system_error_buffer = NULL;

    libzip_error_fini(err);

    if (err->libzip_err < 0 || err->libzip_err >= _libzip_err_str_count) {
        system_error_buffer = (char *)malloc(128);
        if (system_error_buffer == NULL) {
            return _libzip_err_str[ZIP_ER_MEMORY].description;
        }
        snprintf_s(system_error_buffer, 128, "Unknown error %d", err->libzip_err);
        system_error_buffer[128 - 1] = '\0'; /* make sure string is NUL-terminated */
        libzip_error_string = NULL;
        system_error_string = system_error_buffer;
    }
    else {
        libzip_error_string = _libzip_err_str[err->libzip_err].description;

        switch (_libzip_err_str[err->libzip_err].type) {
            case ZIP_ET_SYS: {
                size_t len = strerrorlen_s(err->sys_err) + 1;
                system_error_buffer = malloc(len);
                if (system_error_buffer == NULL) {
                    return _libzip_err_str[ZIP_ER_MEMORY].description;
                }
                strerror_s(system_error_buffer, len, err->sys_err);
                system_error_string = system_error_buffer;
                break;
            }
                
            case ZIP_ET_ZLIB:
                system_error_string = zError(err->sys_err);
                break;
                
            case ZIP_ET_LIBZIP: {
                libzip_uint8_t error = GET_ERROR_FROM_DETAIL(err->sys_err);
                int index = GET_INDEX_FROM_DETAIL(err->sys_err);
                
                if (error == 0) {
                    system_error_string = NULL;
                }
                else if (error >= _libzip_err_details_count) {
                    system_error_buffer = (char *)malloc(128);
                    if (system_error_buffer == NULL) {
                        return _libzip_err_str[ZIP_ER_MEMORY].description;
                    }
                    snprintf_s(system_error_buffer, 128, "invalid detail error %u", error);
                    system_error_buffer[128 - 1] = '\0'; /* make sure string is NUL-terminated */
                    system_error_string = system_error_buffer;
                }
                else if (_libzip_err_details[error].type == ZIP_DETAIL_ET_ENTRY && index < MAX_DETAIL_INDEX) {
                    system_error_buffer = (char *)malloc(128);
                    if (system_error_buffer == NULL) {
                        return _libzip_err_str[ZIP_ER_MEMORY].description;
                    }
                    snprintf_s(system_error_buffer, 128, "entry %d: %s", index, _libzip_err_details[error].description);
                    system_error_buffer[128 - 1] = '\0'; /* make sure string is NUL-terminated */
                    system_error_string = system_error_buffer;
                }
                else {
                    system_error_string = _libzip_err_details[error].description;
                }
                break;
            }
                
            default:
                system_error_string = NULL;
        }
    }

    if (system_error_string == NULL) {
        free(system_error_buffer);
        return libzip_error_string;
    }
    else {
        size_t length = strlen(system_error_string);
        if (libzip_error_string) {
            size_t length_error = strlen(libzip_error_string);
            if (length + length_error + 2 < length) {
                free(system_error_buffer);
                return _libzip_err_str[ZIP_ER_MEMORY].description;
            }
            length += length_error + 2;
        }
        if (length == SIZE_MAX || (s = (char *)malloc(length + 1)) == NULL) {
            free(system_error_buffer);
            return _libzip_err_str[ZIP_ER_MEMORY].description;
        }

        snprintf_s(s, length + 1, "%s%s%s", (libzip_error_string ? libzip_error_string : ""), (libzip_error_string ? ": " : ""), system_error_string);
        err->str = s;

        free(system_error_buffer);
        return s;
    }
}
