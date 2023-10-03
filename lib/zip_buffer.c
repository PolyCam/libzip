/*
 libzip_buffer.c -- bounds checked access to memory buffer
 Copyright (C) 2014-2021 Dieter Baron and Thomas Klausner

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

libzip_uint8_t *
_libzip_buffer_data(libzip_buffer_t *buffer) {
    return buffer->data;
}


void
_libzip_buffer_free(libzip_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }

    if (buffer->free_data) {
        free(buffer->data);
    }

    free(buffer);
}


bool
_libzip_buffer_eof(libzip_buffer_t *buffer) {
    return buffer->ok && buffer->offset == buffer->size;
}


libzip_uint8_t *
_libzip_buffer_get(libzip_buffer_t *buffer, libzip_uint64_t length) {
    libzip_uint8_t *data;

    data = _libzip_buffer_peek(buffer, length);

    if (data != NULL) {
        buffer->offset += length;
    }

    return data;
}


libzip_uint16_t
_libzip_buffer_get_16(libzip_buffer_t *buffer) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 2);

    if (data == NULL) {
        return 0;
    }

    return (libzip_uint16_t)(data[0] + (data[1] << 8));
}


libzip_uint32_t
_libzip_buffer_get_32(libzip_buffer_t *buffer) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 4);

    if (data == NULL) {
        return 0;
    }

    return ((((((libzip_uint32_t)data[3] << 8) + data[2]) << 8) + data[1]) << 8) + data[0];
}


libzip_uint64_t
_libzip_buffer_get_64(libzip_buffer_t *buffer) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 8);

    if (data == NULL) {
        return 0;
    }

    return ((libzip_uint64_t)data[7] << 56) + ((libzip_uint64_t)data[6] << 48) + ((libzip_uint64_t)data[5] << 40) + ((libzip_uint64_t)data[4] << 32) + ((libzip_uint64_t)data[3] << 24) + ((libzip_uint64_t)data[2] << 16) + ((libzip_uint64_t)data[1] << 8) + (libzip_uint64_t)data[0];
}


libzip_uint8_t
_libzip_buffer_get_8(libzip_buffer_t *buffer) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 1);

    if (data == NULL) {
        return 0;
    }

    return data[0];
}


libzip_uint64_t
_libzip_buffer_left(libzip_buffer_t *buffer) {
    return buffer->ok ? buffer->size - buffer->offset : 0;
}


libzip_uint64_t
_libzip_buffer_read(libzip_buffer_t *buffer, libzip_uint8_t *data, libzip_uint64_t length) {
    libzip_uint64_t copied;

    if (_libzip_buffer_left(buffer) < length) {
        length = _libzip_buffer_left(buffer);
    }

    copied = 0;
    while (copied < length) {
        size_t n = ZIP_MIN(length - copied, SIZE_MAX);
        (void)memcpy_s(data + copied, n, _libzip_buffer_get(buffer, n), n);
        copied += n;
    }

    return copied;
}


libzip_buffer_t *
_libzip_buffer_new(libzip_uint8_t *data, libzip_uint64_t size) {
    bool free_data = (data == NULL);
    libzip_buffer_t *buffer;

#if ZIP_UINT64_MAX > SIZE_MAX
    if (size > SIZE_MAX) {
        return NULL;
    }
#endif

    if (data == NULL) {
        if ((data = (libzip_uint8_t *)malloc((size_t)size)) == NULL) {
            return NULL;
        }
    }

    if ((buffer = (libzip_buffer_t *)malloc(sizeof(*buffer))) == NULL) {
        if (free_data) {
            free(data);
        }
        return NULL;
    }

    buffer->ok = true;
    buffer->data = data;
    buffer->size = size;
    buffer->offset = 0;
    buffer->free_data = free_data;

    return buffer;
}


libzip_buffer_t *
_libzip_buffer_new_from_source(libzip_source_t *src, libzip_uint64_t size, libzip_uint8_t *buf, libzip_error_t *error) {
    libzip_buffer_t *buffer;

    if ((buffer = _libzip_buffer_new(buf, size)) == NULL) {
        libzip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if (_libzip_read(src, buffer->data, size, error) < 0) {
        _libzip_buffer_free(buffer);
        return NULL;
    }

    return buffer;
}


libzip_uint64_t
_libzip_buffer_offset(libzip_buffer_t *buffer) {
    return buffer->ok ? buffer->offset : 0;
}


bool
_libzip_buffer_ok(libzip_buffer_t *buffer) {
    return buffer->ok;
}


libzip_uint8_t *
_libzip_buffer_peek(libzip_buffer_t *buffer, libzip_uint64_t length) {
    libzip_uint8_t *data;

    if (!buffer->ok || buffer->offset + length < length || buffer->offset + length > buffer->size) {
        buffer->ok = false;
        return NULL;
    }

    data = buffer->data + buffer->offset;
    return data;
}

int
_libzip_buffer_put(libzip_buffer_t *buffer, const void *src, size_t length) {
    libzip_uint8_t *dst = _libzip_buffer_get(buffer, length);

    if (dst == NULL) {
        return -1;
    }

    (void)memcpy_s(dst, length, src, length);
    return 0;
}


int
_libzip_buffer_put_16(libzip_buffer_t *buffer, libzip_uint16_t i) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 2);

    if (data == NULL) {
        return -1;
    }

    data[0] = (libzip_uint8_t)(i & 0xff);
    data[1] = (libzip_uint8_t)((i >> 8) & 0xff);

    return 0;
}


int
_libzip_buffer_put_32(libzip_buffer_t *buffer, libzip_uint32_t i) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 4);

    if (data == NULL) {
        return -1;
    }

    data[0] = (libzip_uint8_t)(i & 0xff);
    data[1] = (libzip_uint8_t)((i >> 8) & 0xff);
    data[2] = (libzip_uint8_t)((i >> 16) & 0xff);
    data[3] = (libzip_uint8_t)((i >> 24) & 0xff);

    return 0;
}


int
_libzip_buffer_put_64(libzip_buffer_t *buffer, libzip_uint64_t i) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 8);

    if (data == NULL) {
        return -1;
    }

    data[0] = (libzip_uint8_t)(i & 0xff);
    data[1] = (libzip_uint8_t)((i >> 8) & 0xff);
    data[2] = (libzip_uint8_t)((i >> 16) & 0xff);
    data[3] = (libzip_uint8_t)((i >> 24) & 0xff);
    data[4] = (libzip_uint8_t)((i >> 32) & 0xff);
    data[5] = (libzip_uint8_t)((i >> 40) & 0xff);
    data[6] = (libzip_uint8_t)((i >> 48) & 0xff);
    data[7] = (libzip_uint8_t)((i >> 56) & 0xff);

    return 0;
}


int
_libzip_buffer_put_8(libzip_buffer_t *buffer, libzip_uint8_t i) {
    libzip_uint8_t *data = _libzip_buffer_get(buffer, 1);

    if (data == NULL) {
        return -1;
    }

    data[0] = i;

    return 0;
}


int
_libzip_buffer_set_offset(libzip_buffer_t *buffer, libzip_uint64_t offset) {
    if (offset > buffer->size) {
        buffer->ok = false;
        return -1;
    }

    buffer->ok = true;
    buffer->offset = offset;

    return 0;
}


int
_libzip_buffer_skip(libzip_buffer_t *buffer, libzip_uint64_t length) {
    libzip_uint64_t offset = buffer->offset + length;

    if (offset < buffer->offset) {
        buffer->ok = false;
        return -1;
    }
    return _libzip_buffer_set_offset(buffer, offset);
}

libzip_uint64_t
_libzip_buffer_size(libzip_buffer_t *buffer) {
    return buffer->size;
}
