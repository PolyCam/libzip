#ifndef _HAD_ZIPINT_H
#define _HAD_ZIPINT_H

/*
  zipint.h -- internal declarations.
  Copyright (C) 1999-2022 Dieter Baron and Thomas Klausner

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

#include "compat.h"

#ifdef LIBZIP_ALLOCATE_BUFFER
#include <stdlib.h>
#endif

#ifdef _LIBZIP_COMPILING_DEPRECATED
#define LIBZIP_DEPRECATED(x)
#else
#define LIBZIP_DISABLE_DEPRECATED
#endif

#include "libzip.h"

#define CENTRAL_MAGIC "PK\1\2"
#define LOCAL_MAGIC "PK\3\4"
#define EOCD_MAGIC "PK\5\6"
#define DATADES_MAGIC "PK\7\10"
#define EOCD64LOC_MAGIC "PK\6\7"
#define EOCD64_MAGIC "PK\6\6"
#define CDENTRYSIZE 46u
#define LENTRYSIZE 30
#define MAXCOMLEN 65536
#define MAXEXTLEN 65536
#define EOCDLEN 22
#define EOCD64LOCLEN 20
#define EOCD64LEN 56
#define CDBUFSIZE (MAXCOMLEN + EOCDLEN + EOCD64LOCLEN)
#define BUFSIZE 8192
#define EFZIP64SIZE 28
#define EF_WINLIBZIP_AES_SIZE 7
#define MAX_DATA_DESCRIPTOR_LENGTH 24

#define TORRENTLIBZIP_SIGNATURE "TORRENTZIPPED-"
#define TORRENTLIBZIP_SIGNATURE_LENGTH 14
#define TORRENTLIBZIP_CRC_LENGTH 8
#define TORRENTLIBZIP_MEM_LEVEL 8
#define TORRENTLIBZIP_COMPRESSION_FLAGS LIBZIP_UINT16_MAX

#define LIBZIP_CRYPTO_PKWARE_HEADERLEN 12

#define LIBZIP_CM_REPLACED_DEFAULT (-2)
#define LIBZIP_CM_WINLIBZIP_AES 99 /* Winzip AES encrypted */

#define WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH 2
#define WINLIBZIP_AES_MAX_HEADER_LENGTH (16 + WINLIBZIP_AES_PASSWORD_VERIFY_LENGTH)
#define AES_BLOCK_SIZE 16
#define HMAC_LENGTH 10
#define SALT_LENGTH(method) ((method) == LIBZIP_EM_AES_128 ? 8 : ((method) == LIBZIP_EM_AES_192 ? 12 : 16))

#define LIBZIP_CM_IS_DEFAULT(x) ((x) == LIBZIP_CM_DEFAULT || (x) == LIBZIP_CM_REPLACED_DEFAULT)
#define LIBZIP_CM_ACTUAL(x) ((libzip_uint16_t)(LIBZIP_CM_IS_DEFAULT(x) ? LIBZIP_CM_DEFLATE : (x)))

#define LIBZIP_EF_UTF_8_COMMENT 0x6375
#define LIBZIP_EF_UTF_8_NAME 0x7075
#define LIBZIP_EF_WINLIBZIP_AES 0x9901
#define LIBZIP_EF_ZIP64 0x0001

#define LIBZIP_EF_IS_INTERNAL(id) ((id) == LIBZIP_EF_UTF_8_COMMENT || (id) == LIBZIP_EF_UTF_8_NAME || (id) == LIBZIP_EF_WINLIBZIP_AES || (id) == LIBZIP_EF_ZIP64)

/* according to unzip-6.0's zipinfo.c, this corresponds to a regular file with rw permissions for everyone */
#define LIBZIP_EXT_ATTRIB_DEFAULT (0100666u << 16)
/* according to unzip-6.0's zipinfo.c, this corresponds to a directory with rwx permissions for everyone */
#define LIBZIP_EXT_ATTRIB_DEFAULT_DIR (0040777u << 16)

#define LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS_ALLOWED_MASK 0x0836

#define LIBZIP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIBZIP_MIN(a, b) ((a) < (b) ? (a) : (b))

/* This section contains API that won't materialize like this.  It's
   placed in the internal section, pending cleanup. */

/* flags for compression and encryption sources */

#define LIBZIP_CODEC_DECODE 0 /* decompress/decrypt (encode flag not set) */
#define LIBZIP_CODEC_ENCODE 1 /* compress/encrypt */

typedef libzip_source_t *(*libzip_encryption_implementation)(libzip_t *, libzip_source_t *, libzip_uint16_t, int, const char *);

libzip_encryption_implementation _libzip_get_encryption_implementation(libzip_uint16_t method, int operation);

/* clang-format off */
enum libzip_compression_status {
    LIBZIP_COMPRESSION_OK,
    LIBZIP_COMPRESSION_END,
    LIBZIP_COMPRESSION_ERROR,
    LIBZIP_COMPRESSION_NEED_DATA
};
/* clang-format on */
typedef enum libzip_compression_status libzip_compression_status_t;

struct libzip_compression_algorithm {
    /* Return maximum compressed size for uncompressed data of given size. */
    libzip_uint64_t (*maximum_compressed_size)(libzip_uint64_t uncompressed_size);

    /* called once to create new context */
    void *(*allocate)(libzip_uint16_t method, libzip_uint32_t compression_flags, libzip_error_t *error);
    /* called once to free context */
    void (*deallocate)(void *ctx);

    /* get compression specific general purpose bitflags */
    libzip_uint16_t (*general_purpose_bit_flags)(void *ctx);
    /* minimum version needed when using this algorithm */
    libzip_uint8_t version_needed;

    /* start processing */
    bool (*start)(void *ctx, libzip_stat_t *st, libzip_file_attributes_t *attributes);
    /* stop processing */
    bool (*end)(void *ctx);

    /* provide new input data, remains valid until next call to input or end */
    bool (*input)(void *ctx, libzip_uint8_t *data, libzip_uint64_t length);

    /* all input data has been provided */
    void (*end_of_input)(void *ctx);

    /* process input data, writing to data, which has room for length bytes, update length to number of bytes written */
    libzip_compression_status_t (*process)(void *ctx, libzip_uint8_t *data, libzip_uint64_t *length);
};
typedef struct libzip_compression_algorithm libzip_compression_algorithm_t;

extern libzip_compression_algorithm_t libzip_algorithm_bzip2_compress;
extern libzip_compression_algorithm_t libzip_algorithm_bzip2_decompress;
extern libzip_compression_algorithm_t libzip_algorithm_deflate_compress;
extern libzip_compression_algorithm_t libzip_algorithm_deflate_decompress;
extern libzip_compression_algorithm_t libzip_algorithm_xz_compress;
extern libzip_compression_algorithm_t libzip_algorithm_xz_decompress;
extern libzip_compression_algorithm_t libzip_algorithm_zstd_compress;
extern libzip_compression_algorithm_t libzip_algorithm_zstd_decompress;

libzip_compression_algorithm_t *_libzip_get_compression_algorithm(libzip_int32_t method, bool compress);

/* This API is not final yet, but we need it internally, so it's private for now. */

const libzip_uint8_t *libzip_get_extra_field_by_id(libzip_t *, int, int, libzip_uint16_t, int, libzip_uint16_t *);

/* This section contains API that is of limited use until support for
   user-supplied compression/encryption implementation is finished.
   Thus we will keep it private for now. */

libzip_source_t *libzip_source_compress(libzip_t *za, libzip_source_t *src, libzip_int32_t cm, libzip_uint32_t compression_flags);
libzip_source_t *libzip_source_crc_create(libzip_source_t *, int, libzip_error_t *error);
libzip_source_t *libzip_source_decompress(libzip_t *za, libzip_source_t *src, libzip_int32_t cm);
libzip_source_t *libzip_source_pkware_decode(libzip_t *, libzip_source_t *, libzip_uint16_t, int, const char *);
libzip_source_t *libzip_source_pkware_encode(libzip_t *, libzip_source_t *, libzip_uint16_t, int, const char *);
int libzip_source_remove(libzip_source_t *);
libzip_int64_t libzip_source_supports(libzip_source_t *src);
bool libzip_source_supports_reopen(libzip_source_t *src);
libzip_source_t *libzip_source_winlibzip_aes_decode(libzip_t *, libzip_source_t *, libzip_uint16_t, int, const char *);
libzip_source_t *libzip_source_winlibzip_aes_encode(libzip_t *, libzip_source_t *, libzip_uint16_t, int, const char *);
libzip_source_t *libzip_source_buffer_with_attributes(libzip_t *za, const void *data, libzip_uint64_t len, int freep, libzip_file_attributes_t *attributes);
libzip_source_t *libzip_source_buffer_with_attributes_create(const void *data, libzip_uint64_t len, int freep, libzip_file_attributes_t *attributes, libzip_error_t *error);

/* error source for layered sources */

enum libzip_les { LIBZIP_LES_NONE, LIBZIP_LES_UPPER, LIBZIP_LES_LOWER, LIBZIP_LES_INVAL };

#define LIBZIP_DETAIL_ET_GLOBAL 0
#define LIBZIP_DETAIL_ET_ENTRY  1

struct _libzip_err_info {
    int type;
    const char *description;
};

extern const struct _libzip_err_info _libzip_err_str[];
extern const int _libzip_err_str_count;
extern const struct _libzip_err_info _libzip_err_details[];
extern const int _libzip_err_details_count;

/* macros for libzip-internal errors */
#define MAX_DETAIL_INDEX 0x7fffff
#define MAKE_DETAIL_WITH_INDEX(error, index) ((((index) > MAX_DETAIL_INDEX) ? MAX_DETAIL_INDEX : (int)(index)) << 8 | (error))
#define GET_INDEX_FROM_DETAIL(error) (((error) >> 8) & MAX_DETAIL_INDEX)
#define GET_ERROR_FROM_DETAIL(error) ((error) & 0xff)
#define ADD_INDEX_TO_DETAIL(error, index) MAKE_DETAIL_WITH_INDEX(GET_ERROR_FROM_DETAIL(error), (index))

/* error code for libzip-internal errors */
#define LIBZIP_ER_DETAIL_NO_DETAIL 0   /* G no detail */
#define LIBZIP_ER_DETAIL_CDIR_OVERLAPS_EOCD 1  /* G central directory overlaps EOCD, or there is space between them */
#define LIBZIP_ER_DETAIL_COMMENT_LENGTH_INVALID 2  /* G archive comment length incorrect */
#define LIBZIP_ER_DETAIL_CDIR_LENGTH_INVALID 3  /* G central directory length invalid */
#define LIBZIP_ER_DETAIL_CDIR_ENTRY_INVALID 4  /* E central header invalid */
#define LIBZIP_ER_DETAIL_CDIR_WRONG_ENTRIES_COUNT 5  /* G central directory count of entries is incorrect */
#define LIBZIP_ER_DETAIL_ENTRY_HEADER_MISMATCH 6  /* E local and central headers do not match */
#define LIBZIP_ER_DETAIL_EOCD_LENGTH_INVALID 7  /* G wrong EOCD length */
#define LIBZIP_ER_DETAIL_EOCD64_OVERLAPS_EOCD 8  /* G EOCD64 overlaps EOCD, or there is space between them */
#define LIBZIP_ER_DETAIL_EOCD64_WRONG_MAGIC 9  /* G EOCD64 magic incorrect */
#define LIBZIP_ER_DETAIL_EOCD64_MISMATCH 10  /* G EOCD64 and EOCD do not match */
#define LIBZIP_ER_DETAIL_CDIR_INVALID 11  /* G invalid value in central directory */
#define LIBZIP_ER_DETAIL_VARIABLE_SIZE_OVERFLOW 12 /* E variable size fields overflow header */
#define LIBZIP_ER_DETAIL_INVALID_UTF8_IN_FILENAME 13 /* E invalid UTF-8 in filename */
#define LIBZIP_ER_DETAIL_INVALID_UTF8_IN_COMMENT 13 /* E invalid UTF-8 in comment */
#define LIBZIP_ER_DETAIL_INVALID_ZIP64_EF 14 /* E invalid Zip64 extra field */
#define LIBZIP_ER_DETAIL_INVALID_WINZIPAES_EF 14 /* E invalid WinZip AES extra field */
#define LIBZIP_ER_DETAIL_EF_TRAILING_GARBAGE 15 /* E garbage at end of extra fields */
#define LIBZIP_ER_DETAIL_INVALID_EF_LENGTH 16 /* E extra field length is invalid */
#define LIBZIP_ER_DETAIL_INVALID_FILE_LENGTH 17 /* E file length in header doesn't match actual file length */

/* directory entry: general purpose bit flags */

#define LIBZIP_GPBF_ENCRYPTED 0x0001u         /* is encrypted */
#define LIBZIP_GPBF_DATA_DESCRIPTOR 0x0008u   /* crc/size after file data */
#define LIBZIP_GPBF_STRONG_ENCRYPTION 0x0040u /* uses strong encryption */
#define LIBZIP_GPBF_ENCODING_UTF_8 0x0800u    /* file name encoding is UTF-8 */


/* extra fields */
#define LIBZIP_EF_LOCAL LIBZIP_FL_LOCAL                   /* include in local header */
#define LIBZIP_EF_CENTRAL LIBZIP_FL_CENTRAL               /* include in central directory */
#define LIBZIP_EF_BOTH (LIBZIP_EF_LOCAL | LIBZIP_EF_CENTRAL) /* include in both */

#define LIBZIP_FL_FORCE_ZIP64 1024 /* force zip64 extra field (_libzip_dirent_write) */

#define LIBZIP_FL_ENCODING_ALL (LIBZIP_FL_ENC_GUESS | LIBZIP_FL_ENC_CP437 | LIBZIP_FL_ENC_UTF_8)


/* encoding type */
enum libzip_encoding_type {
    LIBZIP_ENCODING_UNKNOWN,      /* not yet analyzed */
    LIBZIP_ENCODING_ASCII,        /* plain ASCII */
    LIBZIP_ENCODING_UTF8_KNOWN,   /* is UTF-8 */
    LIBZIP_ENCODING_UTF8_GUESSED, /* possibly UTF-8 */
    LIBZIP_ENCODING_CP437,        /* Code Page 437 */
    LIBZIP_ENCODING_ERROR         /* should be UTF-8 but isn't */
};

typedef enum libzip_encoding_type libzip_encoding_type_t;

struct libzip_hash;
struct libzip_progress;

typedef struct libzip_cdir libzip_cdir_t;
typedef struct libzip_dirent libzip_dirent_t;
typedef struct libzip_entry libzip_entry_t;
typedef struct libzip_extra_field libzip_extra_field_t;
typedef struct libzip_string libzip_string_t;
typedef struct libzip_buffer libzip_buffer_t;
typedef struct libzip_hash libzip_hash_t;
typedef struct libzip_progress libzip_progress_t;

/* zip archive, part of API */

struct zip {
    libzip_source_t *src;       /* data source for archive */
    unsigned int open_flags; /* flags passed to libzip_open */
    libzip_error_t error;       /* error information */

    unsigned int flags;    /* archive global flags */
    unsigned int ch_flags; /* changed archive global flags */

    char *default_password; /* password used when no other supplied */

    libzip_string_t *comment_orig;    /* archive comment */
    libzip_string_t *comment_changes; /* changed archive comment */
    bool comment_changed;          /* whether archive comment was changed */

    libzip_uint64_t nentry;       /* number of entries */
    libzip_uint64_t nentry_alloc; /* number of entries allocated */
    libzip_entry_t *entry;        /* entries */

    unsigned int nopen_source;       /* number of open sources using archive */
    unsigned int nopen_source_alloc; /* number of sources allocated */
    libzip_source_t **open_source;      /* open sources using archive */

    libzip_hash_t *names; /* hash table for name lookup */

    libzip_progress_t *progress; /* progress callback for libzip_close() */

    libzip_uint32_t* write_crc; /* have _libzip_write() compute CRC */
};

/* file in zip archive, part of API */

struct libzip_file {
    libzip_error_t error; /* error information */
    libzip_source_t *src; /* data source */
};

/* zip archive directory entry (central or local) */

#define LIBZIP_DIRENT_COMP_METHOD 0x0001u
#define LIBZIP_DIRENT_FILENAME 0x0002u
#define LIBZIP_DIRENT_COMMENT 0x0004u
#define LIBZIP_DIRENT_EXTRA_FIELD 0x0008u
#define LIBZIP_DIRENT_ATTRIBUTES 0x0010u
#define LIBZIP_DIRENT_LAST_MOD 0x0020u
#define LIBZIP_DIRENT_ENCRYPTION_METHOD 0x0040u
#define LIBZIP_DIRENT_PASSWORD 0x0080u
#define LIBZIP_DIRENT_ALL LIBZIP_UINT32_MAX

struct libzip_dirent {
    libzip_uint32_t changed;
    bool local_extra_fields_read; /*      whether we already read in local header extra fields */
    bool cloned;                  /*      whether this instance is cloned, and thus shares non-changed strings */

    bool crc_valid; /*      if CRC is valid (sometimes not for encrypted archives) */

    libzip_uint16_t version_madeby;     /* (c)  version of creator */
    libzip_uint16_t version_needed;     /* (cl) version needed to extract */
    libzip_uint16_t bitflags;           /* (cl) general purpose bit flag */
    libzip_int32_t comp_method;         /* (cl) compression method used (uint16 and LIBZIP_CM_DEFAULT (-1)) */
    time_t last_mod;                 /* (cl) time of last modification */
    libzip_uint32_t crc;                /* (cl) CRC-32 of uncompressed data */
    libzip_uint64_t comp_size;          /* (cl) size of compressed data */
    libzip_uint64_t uncomp_size;        /* (cl) size of uncompressed data */
    libzip_string_t *filename;          /* (cl) file name (NUL-terminated) */
    libzip_extra_field_t *extra_fields; /* (cl) extra fields, parsed */
    libzip_string_t *comment;           /* (c)  file comment */
    libzip_uint32_t disk_number;        /* (c)  disk number start */
    libzip_uint16_t int_attrib;         /* (c)  internal file attributes */
    libzip_uint32_t ext_attrib;         /* (c)  external file attributes */
    libzip_uint64_t offset;             /* (c)  offset of local header */

    libzip_uint32_t compression_level; /*      level of compression to use (never valid in orig) */
    libzip_uint16_t encryption_method; /*      encryption method, computed from other fields */
    char *password;                 /*      file specific encryption password */
};

/* zip archive central directory */

struct libzip_cdir {
    libzip_entry_t *entry;        /* directory entries */
    libzip_uint64_t nentry;       /* number of entries */
    libzip_uint64_t nentry_alloc; /* number of entries allocated */

    libzip_uint64_t size;     /* size of central directory */
    libzip_uint64_t offset;   /* offset of central directory in file */
    libzip_string_t *comment; /* zip archive comment */
    bool is_zip64;         /* central directory in zip64 format */
};

struct libzip_extra_field {
    libzip_extra_field_t *next;
    libzip_flags_t flags; /* in local/central header */
    libzip_uint16_t id;   /* header id */
    libzip_uint16_t size; /* data size */
    libzip_uint8_t *data;
};

enum libzip_source_write_state {
    LIBZIP_SOURCE_WRITE_CLOSED, /* write is not in progress */
    LIBZIP_SOURCE_WRITE_OPEN,   /* write is in progress */
    LIBZIP_SOURCE_WRITE_FAILED, /* commit failed, only rollback allowed */
    LIBZIP_SOURCE_WRITE_REMOVED /* file was removed */
};
typedef enum libzip_source_write_state libzip_source_write_state_t;

struct libzip_source {
    libzip_source_t *src;
    union {
        libzip_source_callback f;
        libzip_source_layered_callback l;
    } cb;
    void *ud;
    libzip_error_t error;
    libzip_int64_t supports;                 /* supported commands */
    unsigned int open_count;              /* number of times source was opened (directly or as lower layer) */
    libzip_source_write_state_t write_state; /* whether source is open for writing */
    bool source_closed;                   /* set if source archive is closed */
    libzip_t *source_archive;                /* zip archive we're reading from, NULL if not from archive */
    unsigned int refcount;
    bool eof;                /* EOF reached */
    bool had_read_error;     /* a previous LIBZIP_SOURCE_READ reported an error */
    libzip_uint64_t bytes_read; /* for sources that don't support LIBZIP_SOURCE_TELL. */
};

#define LIBZIP_SOURCE_IS_OPEN_READING(src) ((src)->open_count > 0)
#define LIBZIP_SOURCE_IS_OPEN_WRITING(src) ((src)->write_state == LIBZIP_SOURCE_WRITE_OPEN)
#define LIBZIP_SOURCE_IS_LAYERED(src) ((src)->src != NULL)

/* entry in zip archive directory */

struct libzip_entry {
    libzip_dirent_t *orig;
    libzip_dirent_t *changes;
    libzip_source_t *source;
    bool deleted;
};


/* file or archive comment, or filename */

struct libzip_string {
    libzip_uint8_t *raw;                /* raw string */
    libzip_uint16_t length;             /* length of raw string */
    enum libzip_encoding_type encoding; /* autorecognized encoding */
    libzip_uint8_t *converted;          /* autoconverted string */
    libzip_uint32_t converted_length;   /* length of converted */
};


/* byte array */

/* For performance, we usually keep 8k byte arrays on the stack.
   However, there are (embedded) systems with a stack size of 12k;
   for those, use malloc()/free() */

#ifdef LIBZIP_ALLOCATE_BUFFER
#define DEFINE_BYTE_ARRAY(buf, size) libzip_uint8_t *buf
#define byte_array_init(buf, size) (((buf) = (libzip_uint8_t *)malloc(size)) != NULL)
#define byte_array_fini(buf) (free(buf))
#else
#define DEFINE_BYTE_ARRAY(buf, size) libzip_uint8_t buf[size]
#define byte_array_init(buf, size) (1)
#define byte_array_fini(buf) ((void)0)
#endif


/* bounds checked access to memory buffer */

struct libzip_buffer {
    bool ok;
    bool free_data;

    libzip_uint8_t *data;
    libzip_uint64_t size;
    libzip_uint64_t offset;
};

/* which files to write in which order */

struct libzip_filelist {
    libzip_uint64_t idx;
    const char *name;
};

typedef struct libzip_filelist libzip_filelist_t;

struct _libzip_winlibzip_aes;
typedef struct _libzip_winlibzip_aes libzip_winlibzip_aes_t;

struct _libzip_pkware_keys {
    libzip_uint32_t key[3];
};
typedef struct _libzip_pkware_keys libzip_pkware_keys_t;

#define LIBZIP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIBZIP_MIN(a, b) ((a) < (b) ? (a) : (b))

#define LIBZIP_ENTRY_CHANGED(e, f) ((e)->changes && ((e)->changes->changed & (f)))
#define LIBZIP_ENTRY_DATA_CHANGED(x) ((x)->source != NULL)
#define LIBZIP_ENTRY_HAS_CHANGES(e) (LIBZIP_ENTRY_DATA_CHANGED(e) || (e)->deleted || LIBZIP_ENTRY_CHANGED((e), LIBZIP_DIRENT_ALL))

#define LIBZIP_IS_RDONLY(za) ((za)->ch_flags & LIBZIP_AFL_RDONLY)
#define LIBZIP_IS_TORRENTZIP(za) ((za)->flags & LIBZIP_AFL_IS_TORRENTZIP)
#define LIBZIP_WANT_TORRENTZIP(za) ((za)->ch_flags & LIBZIP_AFL_WANT_TORRENTZIP)


#ifdef HAVE_EXPLICIT_MEMSET
#define _libzip_crypto_clear(b, l) explicit_memset((b), 0, (l))
#else
#ifdef HAVE_EXPLICIT_BZERO
#define _libzip_crypto_clear(b, l) explicit_bzero((b), (l))
#else
#include <string.h>
#define _libzip_crypto_clear(b, l) memset((b), 0, (l))
#endif
#endif


libzip_int64_t _libzip_add_entry(libzip_t *);

libzip_uint8_t *_libzip_buffer_data(libzip_buffer_t *buffer);
bool _libzip_buffer_eof(libzip_buffer_t *buffer);
void _libzip_buffer_free(libzip_buffer_t *buffer);
libzip_uint8_t *_libzip_buffer_get(libzip_buffer_t *buffer, libzip_uint64_t length);
libzip_uint16_t _libzip_buffer_get_16(libzip_buffer_t *buffer);
libzip_uint32_t _libzip_buffer_get_32(libzip_buffer_t *buffer);
libzip_uint64_t _libzip_buffer_get_64(libzip_buffer_t *buffer);
libzip_uint8_t _libzip_buffer_get_8(libzip_buffer_t *buffer);
libzip_uint64_t _libzip_buffer_left(libzip_buffer_t *buffer);
libzip_buffer_t *_libzip_buffer_new(libzip_uint8_t *data, libzip_uint64_t size);
libzip_buffer_t *_libzip_buffer_new_from_source(libzip_source_t *src, libzip_uint64_t size, libzip_uint8_t *buf, libzip_error_t *error);
libzip_uint64_t _libzip_buffer_offset(libzip_buffer_t *buffer);
bool _libzip_buffer_ok(libzip_buffer_t *buffer);
libzip_uint8_t *_libzip_buffer_peek(libzip_buffer_t *buffer, libzip_uint64_t length);
int _libzip_buffer_put(libzip_buffer_t *buffer, const void *src, size_t length);
int _libzip_buffer_put_16(libzip_buffer_t *buffer, libzip_uint16_t i);
int _libzip_buffer_put_32(libzip_buffer_t *buffer, libzip_uint32_t i);
int _libzip_buffer_put_64(libzip_buffer_t *buffer, libzip_uint64_t i);
int _libzip_buffer_put_8(libzip_buffer_t *buffer, libzip_uint8_t i);
libzip_uint64_t _libzip_buffer_read(libzip_buffer_t *buffer, libzip_uint8_t *data, libzip_uint64_t length);
int _libzip_buffer_skip(libzip_buffer_t *buffer, libzip_uint64_t length);
int _libzip_buffer_set_offset(libzip_buffer_t *buffer, libzip_uint64_t offset);
libzip_uint64_t _libzip_buffer_size(libzip_buffer_t *buffer);

void _libzip_cdir_free(libzip_cdir_t *);
bool _libzip_cdir_grow(libzip_cdir_t *cd, libzip_uint64_t additional_entries, libzip_error_t *error);
libzip_cdir_t *_libzip_cdir_new(libzip_uint64_t, libzip_error_t *);
libzip_int64_t _libzip_cdir_write(libzip_t *za, const libzip_filelist_t *filelist, libzip_uint64_t survivors);
time_t _libzip_d2u_time(libzip_uint16_t, libzip_uint16_t);
void _libzip_deregister_source(libzip_t *za, libzip_source_t *src);

void _libzip_dirent_apply_attributes(libzip_dirent_t *, libzip_file_attributes_t *, bool, libzip_uint32_t);
libzip_dirent_t *_libzip_dirent_clone(const libzip_dirent_t *);
void _libzip_dirent_free(libzip_dirent_t *);
void _libzip_dirent_finalize(libzip_dirent_t *);
void _libzip_dirent_init(libzip_dirent_t *);
bool _libzip_dirent_needs_zip64(const libzip_dirent_t *, libzip_flags_t);
libzip_dirent_t *_libzip_dirent_new(void);
bool libzip_dirent_process_ef_zip64(libzip_dirent_t * zde, const libzip_uint8_t * ef, libzip_uint64_t got_len, bool local, libzip_error_t * error);
libzip_int64_t _libzip_dirent_read(libzip_dirent_t *zde, libzip_source_t *src, libzip_buffer_t *buffer, bool local, libzip_error_t *error);
void _libzip_dirent_set_version_needed(libzip_dirent_t *de, bool force_zip64);
void libzip_dirent_torrentlibzip_normalize(libzip_dirent_t *de);

libzip_int32_t _libzip_dirent_size(libzip_source_t *src, libzip_uint16_t, libzip_error_t *);
int _libzip_dirent_write(libzip_t *za, libzip_dirent_t *dirent, libzip_flags_t flags);

libzip_extra_field_t *_libzip_ef_clone(const libzip_extra_field_t *, libzip_error_t *);
libzip_extra_field_t *_libzip_ef_delete_by_id(libzip_extra_field_t *, libzip_uint16_t, libzip_uint16_t, libzip_flags_t);
void _libzip_ef_free(libzip_extra_field_t *);
const libzip_uint8_t *_libzip_ef_get_by_id(const libzip_extra_field_t *, libzip_uint16_t *, libzip_uint16_t, libzip_uint16_t, libzip_flags_t, libzip_error_t *);
libzip_extra_field_t *_libzip_ef_merge(libzip_extra_field_t *, libzip_extra_field_t *);
libzip_extra_field_t *_libzip_ef_new(libzip_uint16_t, libzip_uint16_t, const libzip_uint8_t *, libzip_flags_t);
bool _libzip_ef_parse(const libzip_uint8_t *, libzip_uint16_t, libzip_flags_t, libzip_extra_field_t **, libzip_error_t *);
libzip_extra_field_t *_libzip_ef_remove_internal(libzip_extra_field_t *);
libzip_uint16_t _libzip_ef_size(const libzip_extra_field_t *, libzip_flags_t);
int _libzip_ef_write(libzip_t *za, const libzip_extra_field_t *ef, libzip_flags_t flags);

void _libzip_entry_finalize(libzip_entry_t *);
void _libzip_entry_init(libzip_entry_t *);

void _libzip_error_clear(libzip_error_t *);
void _libzip_error_get(const libzip_error_t *, int *, int *);

void _libzip_error_copy(libzip_error_t *dst, const libzip_error_t *src);

const libzip_uint8_t *_libzip_extract_extra_field_by_id(libzip_error_t *, libzip_uint16_t, int, const libzip_uint8_t *, libzip_uint16_t, libzip_uint16_t *);

int _libzip_file_extra_field_prepare_for_change(libzip_t *, libzip_uint64_t);
int _libzip_file_fillbuf(void *, size_t, libzip_file_t *);
libzip_uint64_t _libzip_file_get_end(const libzip_t *za, libzip_uint64_t index, libzip_error_t *error);
libzip_uint64_t _libzip_file_get_offset(const libzip_t *, libzip_uint64_t, libzip_error_t *);

libzip_dirent_t *_libzip_get_dirent(libzip_t *, libzip_uint64_t, libzip_flags_t, libzip_error_t *);

enum libzip_encoding_type _libzip_guess_encoding(libzip_string_t *, enum libzip_encoding_type);
libzip_uint8_t *_libzip_cp437_to_utf8(const libzip_uint8_t *const, libzip_uint32_t, libzip_uint32_t *, libzip_error_t *);

bool _libzip_hash_add(libzip_hash_t *hash, const libzip_uint8_t *name, libzip_uint64_t index, libzip_flags_t flags, libzip_error_t *error);
bool _libzip_hash_delete(libzip_hash_t *hash, const libzip_uint8_t *key, libzip_error_t *error);
void _libzip_hash_free(libzip_hash_t *hash);
libzip_int64_t _libzip_hash_lookup(libzip_hash_t *hash, const libzip_uint8_t *name, libzip_flags_t flags, libzip_error_t *error);
libzip_hash_t *_libzip_hash_new(libzip_error_t *error);
bool _libzip_hash_reserve_capacity(libzip_hash_t *hash, libzip_uint64_t capacity, libzip_error_t *error);
bool _libzip_hash_revert(libzip_hash_t *hash, libzip_error_t *error);

int _libzip_mkstempm(char *path, int mode, bool create_file);

libzip_t *_libzip_open(libzip_source_t *, unsigned int, libzip_error_t *);

void _libzip_progress_end(libzip_progress_t *progress);
void _libzip_progress_free(libzip_progress_t *progress);
int _libzip_progress_start(libzip_progress_t *progress);
int _libzip_progress_subrange(libzip_progress_t *progress, double start, double end);
int _libzip_progress_update(libzip_progress_t *progress, double value);

/* this symbol is extern so it can be overridden for regression testing */
LIBZIP_EXTERN bool libzip_secure_random(libzip_uint8_t *buffer, libzip_uint16_t length);
libzip_uint32_t libzip_random_uint32(void);

int _libzip_read(libzip_source_t *src, libzip_uint8_t *data, libzip_uint64_t length, libzip_error_t *error);
int _libzip_read_at_offset(libzip_source_t *src, libzip_uint64_t offset, unsigned char *b, size_t length, libzip_error_t *error);
libzip_uint8_t *_libzip_read_data(libzip_buffer_t *buffer, libzip_source_t *src, size_t length, bool nulp, libzip_error_t *error);
int _libzip_read_local_ef(libzip_t *, libzip_uint64_t);
libzip_string_t *_libzip_read_string(libzip_buffer_t *buffer, libzip_source_t *src, libzip_uint16_t length, bool nulp, libzip_error_t *error);
int _libzip_register_source(libzip_t *za, libzip_source_t *src);

void _libzip_set_open_error(int *zep, const libzip_error_t *err, int ze);

bool libzip_source_accept_empty(libzip_source_t *src);
libzip_int64_t _libzip_source_call(libzip_source_t *src, void *data, libzip_uint64_t length, libzip_source_cmd_t command);
bool _libzip_source_eof(libzip_source_t *);
libzip_source_t *_libzip_source_file_or_p(const char *, FILE *, libzip_uint64_t, libzip_int64_t, const libzip_stat_t *, libzip_error_t *error);
bool _libzip_source_had_error(libzip_source_t *);
void _libzip_source_invalidate(libzip_source_t *src);
libzip_source_t *_libzip_source_new(libzip_error_t *error);
int _libzip_source_set_source_archive(libzip_source_t *, libzip_t *);
libzip_source_t *_libzip_source_window_new(libzip_source_t *src, libzip_uint64_t start, libzip_int64_t length, libzip_stat_t *st, libzip_uint64_t st_invalid, libzip_file_attributes_t *attributes, libzip_t *source_archive, libzip_uint64_t source_index, bool take_ownership, libzip_error_t *error);

int _libzip_stat_merge(libzip_stat_t *dst, const libzip_stat_t *src, libzip_error_t *error);
int _libzip_string_equal(const libzip_string_t *a, const libzip_string_t *b);
void _libzip_string_free(libzip_string_t *string);
libzip_uint32_t _libzip_string_crc32(const libzip_string_t *string);
const libzip_uint8_t *_libzip_string_get(libzip_string_t *string, libzip_uint32_t *lenp, libzip_flags_t flags, libzip_error_t *error);
libzip_uint16_t _libzip_string_length(const libzip_string_t *string);
libzip_string_t *_libzip_string_new(const libzip_uint8_t *raw, libzip_uint16_t length, libzip_flags_t flags, libzip_error_t *error);
int _libzip_string_write(libzip_t *za, const libzip_string_t *string);
bool _libzip_winlibzip_aes_decrypt(libzip_winlibzip_aes_t *ctx, libzip_uint8_t *data, libzip_uint64_t length);
bool _libzip_winlibzip_aes_encrypt(libzip_winlibzip_aes_t *ctx, libzip_uint8_t *data, libzip_uint64_t length);
bool _libzip_winlibzip_aes_finish(libzip_winlibzip_aes_t *ctx, libzip_uint8_t *hmac);
void _libzip_winlibzip_aes_free(libzip_winlibzip_aes_t *ctx);
libzip_winlibzip_aes_t *_libzip_winlibzip_aes_new(const libzip_uint8_t *password, libzip_uint64_t password_length, const libzip_uint8_t *salt, libzip_uint16_t key_size, libzip_uint8_t *password_verify, libzip_error_t *error);

void _libzip_pkware_encrypt(libzip_pkware_keys_t *keys, libzip_uint8_t *out, const libzip_uint8_t *in, libzip_uint64_t len);
void _libzip_pkware_decrypt(libzip_pkware_keys_t *keys, libzip_uint8_t *out, const libzip_uint8_t *in, libzip_uint64_t len);
libzip_pkware_keys_t *_libzip_pkware_keys_new(libzip_error_t *error);
void _libzip_pkware_keys_free(libzip_pkware_keys_t *keys);
void _libzip_pkware_keys_reset(libzip_pkware_keys_t *keys);

int _libzip_changed(const libzip_t *, libzip_uint64_t *);
const char *_libzip_get_name(libzip_t *, libzip_uint64_t, libzip_flags_t, libzip_error_t *);
int _libzip_local_header_read(libzip_t *, int);
void *_libzip_memdup(const void *, size_t, libzip_error_t *);
libzip_int64_t _libzip_name_locate(libzip_t *, const char *, libzip_flags_t, libzip_error_t *);
libzip_t *_libzip_new(libzip_error_t *);

libzip_int64_t _libzip_file_replace(libzip_t *, libzip_uint64_t, const char *, libzip_source_t *, libzip_flags_t);
int _libzip_set_name(libzip_t *, libzip_uint64_t, const char *, libzip_flags_t);
void _libzip_u2d_time(time_t, libzip_uint16_t *, libzip_uint16_t *);
int _libzip_unchange(libzip_t *, libzip_uint64_t, int);
void _libzip_unchange_data(libzip_entry_t *);
int _libzip_write(libzip_t *za, const void *data, libzip_uint64_t length);

#endif /* zipint.h */
