/*
  autoclose-archive.c -- automatically close archive when source is closed
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
  This example layered source takes ownership of a zip archive and discards it when the source is freed.
  It can be used to add files from various zip archives without having to keep track of them yourself.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libzip.h>

struct ctx {
    libzip_t* archive;
};

libzip_int64_t callback(libzip_source_t* src, void *ud, void* data, libzip_uint64_t length, libzip_source_cmd_t command) {
    struct ctx* ctx = (struct ctx*)ud;

    switch (command) {
    case LIBZIP_SOURCE_FREE:
        /* Close zip archive we took ownership of */
        libzip_discard(ctx->archive);
        /* Free our own context */
        free(ctx);
        return 0;

    default:
        /* For all other commands, use default implementation */
        return libzip_source_pass_to_lower_layer(src, data, length, command);
    }
}

libzip_source_t* create_layered_autoclose(libzip_source_t* source, libzip_t *archive, libzip_error_t *error) {
    struct ctx* ctx = (struct ctx*)malloc(sizeof(*ctx));
    libzip_source_t *autoclose_source;

    /* Allocate context. */
    if (ctx == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    /* Initialize context */
    ctx->archive = archive;

    /* Create layered source using our callback and context. */
    autoclose_source = libzip_source_layered_create(source, callback, ctx, error);

    /* In case of error, free context. */
    if (autoclose_source == NULL) {
        free(ctx);
    }

    return autoclose_source;
}


int
main(int argc, char *argv[]) {
    const char *destination_archive, *source_archive, *source_file;
    libzip_int64_t index;
    libzip_source_t *src, *src_autoclose;
    libzip_t *z_source, *z_destination;
    int err;

    if (argc != 4) {
        fprintf(stderr, "usage: %s destination-archive source-archive source-file\n", argv[0]);
        return 1;
    }
    destination_archive = argv[1];
    source_archive = argv[2];
    source_file = argv[3];


    if ((z_source = libzip_open(source_archive, 0, &err)) == NULL) {
        libzip_error_t error;
        libzip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: cannot open zip archive '%s': %s\n", argv[0], source_archive, libzip_error_strerror(&error));
        libzip_error_fini(&error);
        exit(1);
    }

    if ((index = libzip_name_locate(z_source, source_file, 0)) < 0) {
        fprintf(stderr, "%s: cannot find file '%s' in '%s': %s\n", argv[0], source_file, source_archive, libzip_strerror(z_source));
        libzip_discard(z_source);
        exit(1);

    }
    if ((src = libzip_source_libzip_file(z_source, z_source, index, 0, 0, -1, NULL)) == NULL) {
        fprintf(stderr, "%s: cannot open file '%s' in '%s': %s\n", argv[0], source_file, source_archive, libzip_strerror(z_source));
        libzip_discard(z_source);
        exit(1);
    }

    libzip_error_t error;
    if ((src_autoclose = create_layered_autoclose(src, z_source, &error)) == NULL) {
        fprintf(stderr, "%s: cannot create layered source: %s\n", argv[0], libzip_error_strerror(&error));
        libzip_source_free(src);
        libzip_discard(z_source);
        exit(1);
    }

    if ((z_destination = libzip_open(destination_archive, LIBZIP_CREATE, &err)) == NULL) {
        libzip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: cannot open zip archive '%s': %s\n", argv[0], destination_archive, libzip_error_strerror(&error));
        libzip_error_fini(&error);
        libzip_source_free(src_autoclose); /* freeing src_autoclose closes z_source */
        exit(1);
    }


    if ((libzip_file_add(z_destination, source_file, src_autoclose, 0)) < 0) {
        fprintf(stderr, "%s: cannot add file: %s\n", argv[0], libzip_strerror(z_source));
        libzip_source_free(src_autoclose);
        libzip_discard(z_destination);
        exit(1);
    }

    if ((libzip_close(z_destination)) < 0) {
        fprintf(stderr, "%s: cannot close archive '%s': %s\n", argv[0], destination_archive, libzip_strerror(z_source));
        libzip_discard(z_destination);
        exit(1);
    }

    exit(0);
}
