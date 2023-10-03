/*
 hole.c -- convert huge files with mostly NULs to/from source_hole
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "libzip.h"

/* public API */

libzip_source_t *source_hole_create(const char *, int flags, libzip_error_t *);

const char *progname;


static int
copy_source(libzip_source_t *from, libzip_source_t *to) {
    libzip_uint8_t buf[8192];
    libzip_int64_t n;

    if (libzip_source_open(from) < 0) {
        fprintf(stderr, "%s: can't open source for reading: %s\n", progname, libzip_error_strerror(libzip_source_error(from)));
        return -1;
    }

    if (libzip_source_begin_write(to) < 0) {
        fprintf(stderr, "%s: can't open source for writing: %s\n", progname, libzip_error_strerror(libzip_source_error(to)));
        libzip_source_close(from);
        return -1;
    }

    while ((n = libzip_source_read(from, buf, sizeof(buf))) > 0) {
        if (libzip_source_write(to, buf, (libzip_uint64_t)n) != n) {
            fprintf(stderr, "%s: can't write to source: %s\n", progname, libzip_error_strerror(libzip_source_error(to)));
            libzip_source_close(from);
            libzip_source_rollback_write(to);
            return -1;
        }
    }

    if (n < 0) {
        fprintf(stderr, "%s: can't read from source: %s\n", progname, libzip_error_strerror(libzip_source_error(from)));
        libzip_source_close(from);
        libzip_source_rollback_write(to);
        return -1;
    }

    libzip_source_close(from);

    if (libzip_source_commit_write(to) < 0) {
        fprintf(stderr, "%s: can't commit source: %s\n", progname, libzip_error_strerror(libzip_source_error(to)));
        libzip_source_rollback_write(to);
        return -1;
    }

    return 0;
}


static libzip_source_t *
open_compressed(const char *fname, int flags) {
    libzip_error_t error;
    libzip_source_t *src;

    libzip_error_init(&error);

    if ((src = source_hole_create(fname, flags, &error)) == NULL) {
        fprintf(stderr, "%s: can't open compressed file %s: %s\n", progname, fname, libzip_error_strerror(&error));
        libzip_error_fini(&error);
        exit(1);
    }

    return src;
}


static libzip_source_t *
open_file(const char *fname) {
    libzip_error_t error;
    libzip_source_t *src;

    libzip_error_init(&error);

    if ((src = libzip_source_file_create(fname, 0, 0, &error)) == NULL) {
        fprintf(stderr, "%s: can't open file %s: %s\n", progname, fname, libzip_error_strerror(&error));
        libzip_error_fini(&error);
        exit(1);
    }

    return src;
}


static void
usage(void) {
    fprintf(stderr, "usage: %s [-du] in out\n", progname);
    fprintf(stderr, "\nOptions:\n  -d  decompress in\n  -u  update in\n");
    exit(1);
}


int
main(int argc, char **argv) {
    libzip_source_t *from;
    libzip_source_t *to;
    int c, err;
    int compress = 1;
    int decompress = 0;

    progname = argv[0];

    while ((c = getopt(argc, argv, "du")) != -1) {
        switch (c) {
        case 'd':
            compress = 0;
            decompress = 1;
            break;

        case 'u':
            compress = 1;
            decompress = 1;
            break;

        default:
            usage();
            break;
        }
    }

    if (optind + 2 != argc) {
        usage();
    }

    if (decompress) {
        from = open_compressed(argv[optind], 0);
    }
    else {
        from = open_file(argv[optind]);
    }

    if (compress) {
        to = open_compressed(argv[optind + 1], LIBZIP_CREATE);
    }
    else {
        to = open_file(argv[optind + 1]);
    }

    err = copy_source(from, to);

    libzip_source_free(from);
    libzip_source_free(to);

    exit(err < 0 ? 1 : 0);
}
