/*
  in-memory.c -- modify zip file in memory
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libzip.h>

static int
get_data(void **datap, size_t *sizep, const char *archive) {
    /* example implementation that reads data from file */
    struct stat st;
    FILE *fp;

    if ((fp = fopen(archive, "rb")) == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr, "can't open %s: %s\n", archive, strerror(errno));
            return -1;
        }

        *datap = NULL;
        *sizep = 0;

        return 0;
    }

    if (fstat(fileno(fp), &st) < 0) {
        fprintf(stderr, "can't stat %s: %s\n", archive, strerror(errno));
        fclose(fp);
        return -1;
    }

    if ((*datap = malloc((size_t)st.st_size)) == NULL) {
        fprintf(stderr, "can't allocate buffer\n");
        fclose(fp);
        return -1;
    }

    if (fread(*datap, 1, (size_t)st.st_size, fp) < (size_t)st.st_size) {
        fprintf(stderr, "can't read %s: %s\n", archive, strerror(errno));
        free(*datap);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    *sizep = (size_t)st.st_size;
    return 0;
}

static int
modify_archive(libzip_t *za) {
    /* modify the archive */
    return 0;
}


static int
use_data(void *data, size_t size, const char *archive) {
    /* example implementation that writes data to file */
    FILE *fp;

    if (data == NULL) {
        if (remove(archive) < 0 && errno != ENOENT) {
            fprintf(stderr, "can't remove %s: %s\n", archive, strerror(errno));
            return -1;
        }
        return 0;
    }

    if ((fp = fopen(archive, "wb")) == NULL) {
        fprintf(stderr, "can't open %s: %s\n", archive, strerror(errno));
        return -1;
    }
    if (fwrite(data, 1, size, fp) < size) {
        fprintf(stderr, "can't write %s: %s\n", archive, strerror(errno));
        fclose(fp);
        return -1;
    }
    if (fclose(fp) < 0) {
        fprintf(stderr, "can't write %s: %s\n", archive, strerror(errno));
        return -1;
    }

    return 0;
}


int
main(int argc, char *argv[]) {
    const char *archive;
    libzip_source_t *src;
    libzip_t *za;
    libzip_error_t error;
    void *data;
    size_t size;

    if (argc < 2) {
        fprintf(stderr, "usage: %s archive\n", argv[0]);
        return 1;
    }
    archive = argv[1];

    /* get buffer with zip archive inside */
    if (get_data(&data, &size, archive) < 0) {
        return 1;
    }

    libzip_error_init(&error);
    /* create source from buffer */
    if ((src = libzip_source_buffer_create(data, size, 1, &error)) == NULL) {
        fprintf(stderr, "can't create source: %s\n", libzip_error_strerror(&error));
        free(data);
        libzip_error_fini(&error);
        return 1;
    }

    /* open zip archive from source */
    if ((za = libzip_open_from_source(src, 0, &error)) == NULL) {
        fprintf(stderr, "can't open zip from source: %s\n", libzip_error_strerror(&error));
        libzip_source_free(src);
        libzip_error_fini(&error);
        return 1;
    }
    libzip_error_fini(&error);

    /* we'll want to read the data back after libzip_close */
    libzip_source_keep(src);

    /* modify archive */
    modify_archive(za);

    /* close archive */
    if (libzip_close(za) < 0) {
        fprintf(stderr, "can't close zip archive '%s': %s\n", archive, libzip_strerror(za));
        return 1;
    }


    /* copy new archive to buffer */

    if (libzip_source_is_deleted(src)) {
        /* new archive is empty, thus no data */
        data = NULL;
    }
    else {
        libzip_stat_t zst;

        if (libzip_source_stat(src, &zst) < 0) {
            fprintf(stderr, "can't stat source: %s\n", libzip_error_strerror(libzip_source_error(src)));
            return 1;
        }

        size = zst.size;

        if (libzip_source_open(src) < 0) {
            fprintf(stderr, "can't open source: %s\n", libzip_error_strerror(libzip_source_error(src)));
            return 1;
        }
        if ((data = malloc(size)) == NULL) {
            fprintf(stderr, "malloc failed: %s\n", strerror(errno));
            libzip_source_close(src);
            return 1;
        }
        if ((libzip_uint64_t)libzip_source_read(src, data, size) < size) {
            fprintf(stderr, "can't read data from source: %s\n", libzip_error_strerror(libzip_source_error(src)));
            libzip_source_close(src);
            free(data);
            return 1;
        }
        libzip_source_close(src);
    }

    /* we're done with src */
    libzip_source_free(src);

    /* use new data */
    use_data(data, size, archive);

    free(data);

    return 0;
}
