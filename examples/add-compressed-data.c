/*
  add-compressed-data.c -- add already compressed file to zip archive
  Copyright (C) 2022 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <libzip@nih.at>

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

/*
 This layered source can be used to add pre-compressed data to a zip archive.
 The data is taken from the lower layer source.
 Metadata (uncompressed size, crc, compression method) must be provided by the caller.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libzip.h>

struct ctx {
    libzip_uint64_t uncompressed_size;
    libzip_uint32_t crc;
    libzip_uint32_t compression_method;
};

libzip_int64_t callback(libzip_source_t* src, void *ud, void* data, libzip_uint64_t length, libzip_source_cmd_t command) {
    struct ctx* ctx = (struct ctx*)ud;

    switch (command) {
    case LIBZIP_SOURCE_FREE:
        /* Free our context. */
        free(ctx);
        return 0;

    case LIBZIP_SOURCE_STAT: {
        libzip_stat_t *st = (libzip_stat_t *)data;
        /* Fix metadata with provided values. */
        if (st->valid & LIBZIP_STAT_SIZE) {
            st->comp_size = st->size;
            st->valid |= LIBZIP_STAT_COMP_SIZE;
        }
        st->size = ctx->uncompressed_size;
        st->crc = ctx->crc;
        st->comp_method = ctx->compression_method;
        st->valid |= LIBZIP_STAT_COMP_METHOD | LIBZIP_STAT_SIZE | LIBZIP_STAT_CRC;

        return 0;
    }

    default:
        /* For all other commands, use default implementation */
        return libzip_source_pass_to_lower_layer(src, data, length, command);
    }
}

libzip_source_t* create_layered_compressed_source(libzip_source_t* source, libzip_uint64_t uncompressed_size, libzip_uint32_t crc, libzip_uint32_t compression_method, libzip_error_t *error) {
    struct ctx* ctx = (struct ctx*)malloc(sizeof(*ctx));
    libzip_source_t *compressed_source;

    /* Allocate context. */
    if (ctx == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    /* Initialize context */
    ctx->compression_method = compression_method;
    ctx->uncompressed_size = uncompressed_size;
    ctx->crc = crc;

    /* Create layered source using our callback and context. */
    compressed_source = libzip_source_layered_create(source, callback, ctx, error);

    /* In case of error, free context. */
    if (compressed_source == NULL) {
        free(ctx);
    }

    return compressed_source;
}


/* This is the information needed to add pre-compressed data to a zip archive. data must be compressed in a format compatible with Zip (e.g. no gzip header for deflate). */

libzip_uint16_t compression_method = LIBZIP_CM_DEFLATE;
libzip_uint64_t uncompressed_size = 60;
libzip_uint32_t crc = 0xb0354048;
libzip_uint8_t data[] = {
    0x4B, 0x4C, 0x44, 0x06, 0x5C, 0x49, 0x28, 0x80,
    0x2B, 0x11, 0x55 ,0x36, 0x19, 0x05, 0x70, 0x01,
    0x00
};


int
main(int argc, char *argv[]) {
    const char *archive;
    libzip_source_t *src, *src_comp;
    libzip_t *za;
    int err;

    if (argc != 2) {
        fprintf(stderr, "usage: %s archive\n", argv[0]);
        return 1;
    }
    archive = argv[1];

    if ((za = libzip_open(archive, LIBZIP_CREATE, &err)) == NULL) {
        libzip_error_t error;
        libzip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: cannot open zip archive '%s': %s\n", argv[0], archive, libzip_error_strerror(&error));
        libzip_error_fini(&error);
        exit(1);
    }

    /* The data can come from any source. To keep the example simple, it is provided in a static buffer here. */
    if ((src = libzip_source_buffer(za, data, sizeof(data), 0)) == NULL) {
        fprintf(stderr, "%s: cannot create buffer source: %s\n", argv[0], libzip_strerror(za));
        libzip_discard(za);
        exit(1);
    }

    libzip_error_t error;
    if ((src_comp = create_layered_compressed_source(src, uncompressed_size, crc, compression_method, &error)) == NULL) {
        fprintf(stderr, "%s: cannot create layered source: %s\n", argv[0], libzip_error_strerror(&error));
        libzip_source_free(src);
        libzip_discard(za);
        exit(1);
    }

    if ((libzip_file_add(za, "precompressed", src_comp, 0)) < 0) {
        fprintf(stderr, "%s: cannot add precompressed file: %s\n", argv[0], libzip_strerror(za));
        libzip_source_free(src_comp);
        libzip_discard(za);
        exit(1);
    }

    if ((libzip_close(za)) < 0) {
        fprintf(stderr, "%s: cannot close archive '%s': %s\n", argv[0], archive, libzip_strerror(za));
        libzip_discard(za);
        exit(1);
    }

    exit(0);
}
