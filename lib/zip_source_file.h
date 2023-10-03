/*
  libzip_source_file.h -- header for common file operations
  Copyright (C) 2020-2022 Dieter Baron and Thomas Klausner

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

struct libzip_source_file_stat {
    libzip_uint64_t size; /* must be valid for regular files */
    time_t mtime;      /* must always be valid, is initialized to current time */
    bool exists;       /* must always be valid */
    bool regular_file; /* must always be valid */
};

typedef struct libzip_source_file_context libzip_source_file_context_t;
typedef struct libzip_source_file_operations libzip_source_file_operations_t;
typedef struct libzip_source_file_stat libzip_source_file_stat_t;

struct libzip_source_file_context {
    libzip_error_t error; /* last error information */
    libzip_int64_t supports;

    /* reading */
    char *fname;                      /* name of file to read from */
    void *f;                          /* file to read from */
    libzip_stat_t st;                    /* stat information passed in */
    libzip_file_attributes_t attributes; /* additional file attributes */
    libzip_error_t stat_error;           /* error returned for stat */
    libzip_uint64_t start;               /* start offset of data to read */
    libzip_uint64_t len;                 /* length of the file, 0 for up to EOF */
    libzip_uint64_t offset;              /* current offset relative to start (0 is beginning of part we read) */

    /* writing */
    char *tmpname;
    void *fout;

    libzip_source_file_operations_t *ops;
    void *ops_userdata;
};


/* The following methods must be implemented to support each feature:
   - close, read, seek, and stat must always be implemented.
   - To support specifying the file by name, open, and strdup must be implemented.
   - For write support, the file must be specified by name and close, commit_write, create_temp_output, remove, rollback_write, and tell must be implemented.
   - create_temp_output_cloning is always optional. */

struct libzip_source_file_operations {
    void (*close)(libzip_source_file_context_t *ctx);
    libzip_int64_t (*commit_write)(libzip_source_file_context_t *ctx);
    libzip_int64_t (*create_temp_output)(libzip_source_file_context_t *ctx);
    libzip_int64_t (*create_temp_output_cloning)(libzip_source_file_context_t *ctx, libzip_uint64_t len);
    bool (*open)(libzip_source_file_context_t *ctx);
    libzip_int64_t (*read)(libzip_source_file_context_t *ctx, void *buf, libzip_uint64_t len);
    libzip_int64_t (*remove)(libzip_source_file_context_t *ctx);
    void (*rollback_write)(libzip_source_file_context_t *ctx);
    bool (*seek)(libzip_source_file_context_t *ctx, void *f, libzip_int64_t offset, int whence);
    bool (*stat)(libzip_source_file_context_t *ctx, libzip_source_file_stat_t *st);
    char *(*string_duplicate)(libzip_source_file_context_t *ctx, const char *);
    libzip_int64_t (*tell)(libzip_source_file_context_t *ctx, void *f);
    libzip_int64_t (*write)(libzip_source_file_context_t *ctx, const void *data, libzip_uint64_t len);
};

libzip_source_t *libzip_source_file_common_new(const char *fname, void *file, libzip_uint64_t start, libzip_int64_t len, const libzip_stat_t *st, libzip_source_file_operations_t *ops, void *ops_userdata, libzip_error_t *error);
