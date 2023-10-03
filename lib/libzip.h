#ifndef _HAD_ZIP_H
#define _HAD_ZIP_H

/*
  zip.h -- exported declarations.
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


#ifdef __cplusplus
extern "C" {
#if 0
} /* fix autoindent */
#endif
#endif

#include <zipconf.h>

#ifndef ZIP_EXTERN
#ifndef ZIP_STATIC
#ifdef _WIN32
#define ZIP_EXTERN __declspec(dllimport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define ZIP_EXTERN __attribute__((visibility("default")))
#else
#define ZIP_EXTERN
#endif
#else
#define ZIP_EXTERN
#endif
#endif

#ifndef ZIP_DEPRECATED
#if defined(__GNUC__) || defined(__clang__)
#define ZIP_DEPRECATED(x) __attribute__((deprecated(x)))
#elif defined(_MSC_VER)
#define ZIP_DEPRECATED(x) __declspec(deprecated(x))
#else
#define ZIP_DEPRECATED(x)
#endif
#endif

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

/* flags for libzip_open */

#define ZIP_CREATE 1
#define ZIP_EXCL 2
#define ZIP_CHECKCONS 4
#define ZIP_TRUNCATE 8
#define ZIP_RDONLY 16


/* flags for libzip_name_locate, libzip_fopen, libzip_stat, ... */

#define ZIP_FL_NOCASE 1u       /* ignore case on name lookup */
#define ZIP_FL_NODIR 2u        /* ignore directory component */
#define ZIP_FL_COMPRESSED 4u   /* read compressed data */
#define ZIP_FL_UNCHANGED 8u    /* use original data, ignoring changes */
/* 16u was ZIP_FL_RECOMPRESS, which is deprecated */
#define ZIP_FL_ENCRYPTED 32u   /* read encrypted data (implies ZIP_FL_COMPRESSED) */
#define ZIP_FL_ENC_GUESS 0u    /* guess string encoding (is default) */
#define ZIP_FL_ENC_RAW 64u     /* get unmodified string */
#define ZIP_FL_ENC_STRICT 128u /* follow specification strictly */
#define ZIP_FL_LOCAL 256u      /* in local header */
#define ZIP_FL_CENTRAL 512u    /* in central directory */
/*                           1024u    reserved for internal use */
#define ZIP_FL_ENC_UTF_8 2048u /* string is UTF-8 encoded */
#define ZIP_FL_ENC_CP437 4096u /* string is CP437 encoded */
#define ZIP_FL_OVERWRITE 8192u /* libzip_file_add: if file with name exists, overwrite (replace) it */

/* archive global flags flags */

#define ZIP_AFL_RDONLY  2u /* read only -- cannot be cleared */
#define ZIP_AFL_IS_TORRENTZIP	4u /* current archive is torrentzipped */
#define ZIP_AFL_WANT_TORRENTZIP	8u /* write archive in torrentzip format */
#define ZIP_AFL_CREATE_OR_KEEP_FILE_FOR_EMPTY_ARCHIVE 16u /* don't remove file if archive is empty */


/* create a new extra field */

#define ZIP_EXTRA_FIELD_ALL ZIP_UINT16_MAX
#define ZIP_EXTRA_FIELD_NEW ZIP_UINT16_MAX

/* length parameter to various functions */

#define ZIP_LENGTH_TO_END 0
#define ZIP_LENGTH_UNCHECKED (-2) /* only supported by libzip_source_file and its variants */

/* libzip error codes */

#define ZIP_ER_OK 0               /* N No error */
#define ZIP_ER_MULTIDISK 1        /* N Multi-disk zip archives not supported */
#define ZIP_ER_RENAME 2           /* S Renaming temporary file failed */
#define ZIP_ER_CLOSE 3            /* S Closing zip archive failed */
#define ZIP_ER_SEEK 4             /* S Seek error */
#define ZIP_ER_READ 5             /* S Read error */
#define ZIP_ER_WRITE 6            /* S Write error */
#define ZIP_ER_CRC 7              /* N CRC error */
#define ZIP_ER_ZIPCLOSED 8        /* N Containing zip archive was closed */
#define ZIP_ER_NOENT 9            /* N No such file */
#define ZIP_ER_EXISTS 10          /* N File already exists */
#define ZIP_ER_OPEN 11            /* S Can't open file */
#define ZIP_ER_TMPOPEN 12         /* S Failure to create temporary file */
#define ZIP_ER_ZLIB 13            /* Z Zlib error */
#define ZIP_ER_MEMORY 14          /* N Malloc failure */
#define ZIP_ER_CHANGED 15         /* N Entry has been changed */
#define ZIP_ER_COMPNOTSUPP 16     /* N Compression method not supported */
#define ZIP_ER_EOF 17             /* N Premature end of file */
#define ZIP_ER_INVAL 18           /* N Invalid argument */
#define ZIP_ER_NOZIP 19           /* N Not a zip archive */
#define ZIP_ER_INTERNAL 20        /* N Internal error */
#define ZIP_ER_INCONS 21          /* L Zip archive inconsistent */
#define ZIP_ER_REMOVE 22          /* S Can't remove file */
#define ZIP_ER_DELETED 23         /* N Entry has been deleted */
#define ZIP_ER_ENCRNOTSUPP 24     /* N Encryption method not supported */
#define ZIP_ER_RDONLY 25          /* N Read-only archive */
#define ZIP_ER_NOPASSWD 26        /* N No password provided */
#define ZIP_ER_WRONGPASSWD 27     /* N Wrong password provided */
#define ZIP_ER_OPNOTSUPP 28       /* N Operation not supported */
#define ZIP_ER_INUSE 29           /* N Resource still in use */
#define ZIP_ER_TELL 30            /* S Tell error */
#define ZIP_ER_COMPRESSED_DATA 31 /* N Compressed data invalid */
#define ZIP_ER_CANCELLED 32       /* N Operation cancelled */
#define ZIP_ER_DATA_LENGTH 33     /* N Unexpected length of data */
#define ZIP_ER_NOT_ALLOWED 34     /* N Not allowed in torrentzip */

/* type of system error value */

#define ZIP_ET_NONE 0   /* sys_err unused */
#define ZIP_ET_SYS 1    /* sys_err is errno */
#define ZIP_ET_ZLIB 2   /* sys_err is zlib error code */
#define ZIP_ET_LIBZIP 3 /* sys_err is libzip error code */

/* compression methods */

#define ZIP_CM_DEFAULT -1 /* better of deflate or store */
#define ZIP_CM_STORE 0    /* stored (uncompressed) */
#define ZIP_CM_SHRINK 1   /* shrunk */
#define ZIP_CM_REDUCE_1 2 /* reduced with factor 1 */
#define ZIP_CM_REDUCE_2 3 /* reduced with factor 2 */
#define ZIP_CM_REDUCE_3 4 /* reduced with factor 3 */
#define ZIP_CM_REDUCE_4 5 /* reduced with factor 4 */
#define ZIP_CM_IMPLODE 6  /* imploded */
/* 7 - Reserved for Tokenizing compression algorithm */
#define ZIP_CM_DEFLATE 8         /* deflated */
#define ZIP_CM_DEFLATE64 9       /* deflate64 */
#define ZIP_CM_PKWARE_IMPLODE 10 /* PKWARE imploding */
/* 11 - Reserved by PKWARE */
#define ZIP_CM_BZIP2 12 /* compressed using BZIP2 algorithm */
/* 13 - Reserved by PKWARE */
#define ZIP_CM_LZMA 14 /* LZMA (EFS) */
/* 15-17 - Reserved by PKWARE */
#define ZIP_CM_TERSE 18 /* compressed using IBM TERSE (new) */
#define ZIP_CM_LZ77 19  /* IBM LZ77 z Architecture (PFS) */
/* 20 - old value for Zstandard */
#define ZIP_CM_LZMA2 33
#define ZIP_CM_ZSTD 93    /* Zstandard compressed data */
#define ZIP_CM_XZ 95      /* XZ compressed data */
#define ZIP_CM_JPEG 96    /* Compressed Jpeg data */
#define ZIP_CM_WAVPACK 97 /* WavPack compressed data */
#define ZIP_CM_PPMD 98    /* PPMd version I, Rev 1 */

/* encryption methods */

#define ZIP_EM_NONE 0         /* not encrypted */
#define ZIP_EM_TRAD_PKWARE 1  /* traditional PKWARE encryption */
#if 0                         /* Strong Encryption Header not parsed yet */
#define ZIP_EM_DES 0x6601     /* strong encryption: DES */
#define ZIP_EM_RC2_OLD 0x6602 /* strong encryption: RC2, version < 5.2 */
#define ZIP_EM_3DES_168 0x6603
#define ZIP_EM_3DES_112 0x6609
#define ZIP_EM_PKZIP_AES_128 0x660e
#define ZIP_EM_PKZIP_AES_192 0x660f
#define ZIP_EM_PKZIP_AES_256 0x6610
#define ZIP_EM_RC2 0x6702 /* strong encryption: RC2, version >= 5.2 */
#define ZIP_EM_RC4 0x6801
#endif
#define ZIP_EM_AES_128 0x0101 /* Winzip AES encryption */
#define ZIP_EM_AES_192 0x0102
#define ZIP_EM_AES_256 0x0103
#define ZIP_EM_UNKNOWN 0xffff /* unknown algorithm */

#define ZIP_OPSYS_DOS 0x00u
#define ZIP_OPSYS_AMIGA 0x01u
#define ZIP_OPSYS_OPENVMS 0x02u
#define ZIP_OPSYS_UNIX 0x03u
#define ZIP_OPSYS_VM_CMS 0x04u
#define ZIP_OPSYS_ATARI_ST 0x05u
#define ZIP_OPSYS_OS_2 0x06u
#define ZIP_OPSYS_MACINTOSH 0x07u
#define ZIP_OPSYS_Z_SYSTEM 0x08u
#define ZIP_OPSYS_CPM 0x09u
#define ZIP_OPSYS_WINDOWS_NTFS 0x0au
#define ZIP_OPSYS_MVS 0x0bu
#define ZIP_OPSYS_VSE 0x0cu
#define ZIP_OPSYS_ACORN_RISC 0x0du
#define ZIP_OPSYS_VFAT 0x0eu
#define ZIP_OPSYS_ALTERNATE_MVS 0x0fu
#define ZIP_OPSYS_BEOS 0x10u
#define ZIP_OPSYS_TANDEM 0x11u
#define ZIP_OPSYS_OS_400 0x12u
#define ZIP_OPSYS_OS_X 0x13u

#define ZIP_OPSYS_DEFAULT ZIP_OPSYS_UNIX


enum libzip_source_cmd {
    ZIP_SOURCE_OPEN,                /* prepare for reading */
    ZIP_SOURCE_READ,                /* read data */
    ZIP_SOURCE_CLOSE,               /* reading is done */
    ZIP_SOURCE_STAT,                /* get meta information */
    ZIP_SOURCE_ERROR,               /* get error information */
    ZIP_SOURCE_FREE,                /* cleanup and free resources */
    ZIP_SOURCE_SEEK,                /* set position for reading */
    ZIP_SOURCE_TELL,                /* get read position */
    ZIP_SOURCE_BEGIN_WRITE,         /* prepare for writing */
    ZIP_SOURCE_COMMIT_WRITE,        /* writing is done */
    ZIP_SOURCE_ROLLBACK_WRITE,      /* discard written changes */
    ZIP_SOURCE_WRITE,               /* write data */
    ZIP_SOURCE_SEEK_WRITE,          /* set position for writing */
    ZIP_SOURCE_TELL_WRITE,          /* get write position */
    ZIP_SOURCE_SUPPORTS,            /* check whether source supports command */
    ZIP_SOURCE_REMOVE,              /* remove file */
    ZIP_SOURCE_RESERVED_1,          /* previously used internally */
    ZIP_SOURCE_BEGIN_WRITE_CLONING, /* like ZIP_SOURCE_BEGIN_WRITE, but keep part of original file */
    ZIP_SOURCE_ACCEPT_EMPTY,        /* whether empty files are valid archives */
    ZIP_SOURCE_GET_FILE_ATTRIBUTES, /* get additional file attributes */
    ZIP_SOURCE_SUPPORTS_REOPEN      /* allow reading from changed entry */
};
typedef enum libzip_source_cmd libzip_source_cmd_t;

#define ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd) (((libzip_int64_t)1) << (cmd))

#define ZIP_SOURCE_CHECK_SUPPORTED(supported, cmd)  (((supported) & ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd)) != 0)

/* clang-format off */

#define ZIP_SOURCE_SUPPORTS_READABLE	(ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_OPEN) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_READ) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_CLOSE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_STAT) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ERROR) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_FREE))

#define ZIP_SOURCE_SUPPORTS_SEEKABLE	(ZIP_SOURCE_SUPPORTS_READABLE \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SEEK) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_TELL) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SUPPORTS))

#define ZIP_SOURCE_SUPPORTS_WRITABLE    (ZIP_SOURCE_SUPPORTS_SEEKABLE \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_BEGIN_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_COMMIT_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ROLLBACK_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SEEK_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_TELL_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_REMOVE))

/* clang-format on */

/* for use by sources */
struct libzip_source_args_seek {
    libzip_int64_t offset;
    int whence;
};

typedef struct libzip_source_args_seek libzip_source_args_seek_t;
#define ZIP_SOURCE_GET_ARGS(type, data, len, error) ((len) < sizeof(type) ? libzip_error_set((error), ZIP_ER_INVAL, 0), (type *)NULL : (type *)(data))


/* error information */
/* use libzip_error_*() to access */
struct libzip_error {
    int libzip_err;         /* libzip error code (ZIP_ER_*) */
    int sys_err;         /* copy of errno (E*) or zlib error code */
    char *_Nullable str; /* string representation or NULL */
};

#define ZIP_STAT_NAME 0x0001u
#define ZIP_STAT_INDEX 0x0002u
#define ZIP_STAT_SIZE 0x0004u
#define ZIP_STAT_COMP_SIZE 0x0008u
#define ZIP_STAT_MTIME 0x0010u
#define ZIP_STAT_CRC 0x0020u
#define ZIP_STAT_COMP_METHOD 0x0040u
#define ZIP_STAT_ENCRYPTION_METHOD 0x0080u
#define ZIP_STAT_FLAGS 0x0100u

struct libzip_stat {
    libzip_uint64_t valid;             /* which fields have valid values */
    const char *_Nullable name;     /* name of the file */
    libzip_uint64_t index;             /* index within archive */
    libzip_uint64_t size;              /* size of file (uncompressed) */
    libzip_uint64_t comp_size;         /* size of file (compressed) */
    time_t mtime;                   /* modification time */
    libzip_uint32_t crc;               /* crc of file data */
    libzip_uint16_t comp_method;       /* compression method used */
    libzip_uint16_t encryption_method; /* encryption method used */
    libzip_uint32_t flags;             /* reserved for future use */
};

struct libzip_buffer_fragment {
    libzip_uint8_t *_Nonnull data;
    libzip_uint64_t length;
};

struct libzip_file_attributes {
    libzip_uint64_t valid;                     /* which fields have valid values */
    libzip_uint8_t version;                    /* version of this struct, currently 1 */
    libzip_uint8_t host_system;                /* host system on which file was created */
    libzip_uint8_t ascii;                      /* flag whether file is ASCII text */
    libzip_uint8_t version_needed;             /* minimum version needed to extract file */
    libzip_uint32_t external_file_attributes;  /* external file attributes (host-system specific) */
    libzip_uint16_t general_purpose_bit_flags; /* general purpose big flags, only some bits are honored */
    libzip_uint16_t general_purpose_bit_mask;  /* which bits in general_purpose_bit_flags are valid */
};

#define ZIP_FILE_ATTRIBUTES_HOST_SYSTEM 0x0001u
#define ZIP_FILE_ATTRIBUTES_ASCII 0x0002u
#define ZIP_FILE_ATTRIBUTES_VERSION_NEEDED 0x0004u
#define ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES 0x0008u
#define ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS 0x0010u

struct zip;
struct libzip_file;
struct libzip_source;

typedef struct zip libzip_t;
typedef struct libzip_error libzip_error_t;
typedef struct libzip_file libzip_file_t;
typedef struct libzip_file_attributes libzip_file_attributes_t;
typedef struct libzip_source libzip_source_t;
typedef struct libzip_stat libzip_stat_t;
typedef struct libzip_buffer_fragment libzip_buffer_fragment_t;

typedef libzip_uint32_t libzip_flags_t;

typedef libzip_int64_t (*libzip_source_callback)(void *_Nullable, void *_Nullable, libzip_uint64_t, libzip_source_cmd_t);
typedef libzip_int64_t (*libzip_source_layered_callback)(libzip_source_t *_Nonnull, void *_Nullable, void *_Nullable, libzip_uint64_t, enum libzip_source_cmd);
typedef void (*libzip_progress_callback)(libzip_t *_Nonnull, double, void *_Nullable);
typedef int (*libzip_cancel_callback)(libzip_t *_Nonnull, void *_Nullable);

#ifndef ZIP_DISABLE_DEPRECATED
#define ZIP_FL_RECOMPRESS 16u  /* force recompression of data */

typedef void (*libzip_progress_callback_t)(double);
ZIP_DEPRECATED("use 'libzip_register_progress_callback_with_state' instead") ZIP_EXTERN void libzip_register_progress_callback(libzip_t *_Nonnull, libzip_progress_callback_t _Nullable);

ZIP_DEPRECATED("use 'libzip_file_add' instead") ZIP_EXTERN libzip_int64_t libzip_add(libzip_t *_Nonnull, const char *_Nonnull, libzip_source_t *_Nonnull);
ZIP_DEPRECATED("use 'libzip_dir_add' instead") ZIP_EXTERN libzip_int64_t libzip_add_dir(libzip_t *_Nonnull, const char *_Nonnull);
ZIP_DEPRECATED("use 'libzip_file_get_comment' instead") ZIP_EXTERN const char *_Nullable libzip_get_file_comment(libzip_t *_Nonnull, libzip_uint64_t, int *_Nullable, int);
ZIP_DEPRECATED("use 'libzip_get_num_entries' instead") ZIP_EXTERN int libzip_get_num_files(libzip_t *_Nonnull);
ZIP_DEPRECATED("use 'libzip_file_rename' instead") ZIP_EXTERN int libzip_rename(libzip_t *_Nonnull, libzip_uint64_t, const char *_Nonnull);
ZIP_DEPRECATED("use 'libzip_file_replace' instead") ZIP_EXTERN int libzip_replace(libzip_t *_Nonnull, libzip_uint64_t, libzip_source_t *_Nonnull);
ZIP_DEPRECATED("use 'libzip_file_set_comment' instead") ZIP_EXTERN int libzip_set_file_comment(libzip_t *_Nonnull, libzip_uint64_t, const char *_Nullable, int);
ZIP_DEPRECATED("use 'libzip_error_init_with_code' and 'libzip_error_system_type' instead") ZIP_EXTERN int libzip_error_get_sys_type(int);
ZIP_DEPRECATED("use 'libzip_get_error' instead") ZIP_EXTERN void libzip_error_get(libzip_t *_Nonnull, int *_Nullable, int *_Nullable);
ZIP_DEPRECATED("use 'libzip_error_strerror' instead") ZIP_EXTERN int libzip_error_to_str(char *_Nonnull, libzip_uint64_t, int, int);
ZIP_DEPRECATED("use 'libzip_file_get_error' instead") ZIP_EXTERN void libzip_file_error_get(libzip_file_t *_Nonnull, int *_Nullable, int *_Nullable);
ZIP_DEPRECATED("use 'libzip_source_libzip_file' instead") ZIP_EXTERN libzip_source_t *_Nullable libzip_source_zip(libzip_t *_Nonnull, libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint64_t, libzip_int64_t);
ZIP_DEPRECATED("use 'libzip_source_libzip_file_create' instead") ZIP_EXTERN libzip_source_t *_Nullable libzip_source_libzip_create(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
#endif

ZIP_EXTERN int libzip_close(libzip_t *_Nonnull);
ZIP_EXTERN int libzip_delete(libzip_t *_Nonnull, libzip_uint64_t);
ZIP_EXTERN libzip_int64_t libzip_dir_add(libzip_t *_Nonnull, const char *_Nonnull, libzip_flags_t);
ZIP_EXTERN void libzip_discard(libzip_t *_Nonnull);

ZIP_EXTERN libzip_error_t *_Nonnull libzip_get_error(libzip_t *_Nonnull);
ZIP_EXTERN void libzip_error_clear(libzip_t *_Nonnull);
ZIP_EXTERN int libzip_error_code_zip(const libzip_error_t *_Nonnull);
ZIP_EXTERN int libzip_error_code_system(const libzip_error_t *_Nonnull);
ZIP_EXTERN void libzip_error_fini(libzip_error_t *_Nonnull);
ZIP_EXTERN void libzip_error_init(libzip_error_t *_Nonnull);
ZIP_EXTERN void libzip_error_init_with_code(libzip_error_t *_Nonnull, int);
ZIP_EXTERN void libzip_error_set(libzip_error_t *_Nullable, int, int);
ZIP_EXTERN void libzip_error_set_from_source(libzip_error_t *_Nonnull, libzip_source_t *_Nullable);
ZIP_EXTERN const char *_Nonnull libzip_error_strerror(libzip_error_t *_Nonnull);
ZIP_EXTERN int libzip_error_system_type(const libzip_error_t *_Nonnull);
ZIP_EXTERN libzip_int64_t libzip_error_to_data(const libzip_error_t *_Nonnull, void *_Nonnull, libzip_uint64_t);

ZIP_EXTERN int libzip_fclose(libzip_file_t *_Nonnull);
ZIP_EXTERN libzip_t *_Nullable libzip_fdopen(int, int, int *_Nullable);
ZIP_EXTERN libzip_int64_t libzip_file_add(libzip_t *_Nonnull, const char *_Nonnull, libzip_source_t *_Nonnull, libzip_flags_t);
ZIP_EXTERN void libzip_file_attributes_init(libzip_file_attributes_t *_Nonnull);
ZIP_EXTERN void libzip_file_error_clear(libzip_file_t *_Nonnull);
ZIP_EXTERN int libzip_file_extra_field_delete(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN int libzip_file_extra_field_delete_by_id(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN int libzip_file_extra_field_set(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_uint16_t, const libzip_uint8_t *_Nullable, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN libzip_int16_t libzip_file_extra_fields_count(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t);
ZIP_EXTERN libzip_int16_t libzip_file_extra_fields_count_by_id(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN const libzip_uint8_t *_Nullable libzip_file_extra_field_get(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_uint16_t *_Nullable, libzip_uint16_t *_Nullable, libzip_flags_t);
ZIP_EXTERN const libzip_uint8_t *_Nullable libzip_file_extra_field_get_by_id(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_uint16_t, libzip_uint16_t *_Nullable, libzip_flags_t);
ZIP_EXTERN const char *_Nullable libzip_file_get_comment(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint32_t *_Nullable, libzip_flags_t);
ZIP_EXTERN libzip_error_t *_Nonnull libzip_file_get_error(libzip_file_t *_Nonnull);
ZIP_EXTERN int libzip_file_get_external_attributes(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint8_t *_Nullable, libzip_uint32_t *_Nullable);
ZIP_EXTERN int libzip_file_is_seekable(libzip_file_t *_Nonnull);
ZIP_EXTERN int libzip_file_rename(libzip_t *_Nonnull, libzip_uint64_t, const char *_Nonnull, libzip_flags_t);
ZIP_EXTERN int libzip_file_replace(libzip_t *_Nonnull, libzip_uint64_t, libzip_source_t *_Nonnull, libzip_flags_t);
ZIP_EXTERN int libzip_file_set_comment(libzip_t *_Nonnull, libzip_uint64_t, const char *_Nullable, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN int libzip_file_set_dostime(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, libzip_uint16_t, libzip_flags_t);
ZIP_EXTERN int libzip_file_set_encryption(libzip_t *_Nonnull, libzip_uint64_t, libzip_uint16_t, const char *_Nullable);
ZIP_EXTERN int libzip_file_set_external_attributes(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint8_t, libzip_uint32_t);
ZIP_EXTERN int libzip_file_set_mtime(libzip_t *_Nonnull, libzip_uint64_t, time_t, libzip_flags_t);
ZIP_EXTERN const char *_Nonnull libzip_file_strerror(libzip_file_t *_Nonnull);
ZIP_EXTERN libzip_file_t *_Nullable libzip_fopen(libzip_t *_Nonnull, const char *_Nonnull, libzip_flags_t);
ZIP_EXTERN libzip_file_t *_Nullable libzip_fopen_encrypted(libzip_t *_Nonnull, const char *_Nonnull, libzip_flags_t, const char *_Nullable);
ZIP_EXTERN libzip_file_t *_Nullable libzip_fopen_index(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t);
ZIP_EXTERN libzip_file_t *_Nullable libzip_fopen_index_encrypted(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, const char *_Nullable);
ZIP_EXTERN libzip_int64_t libzip_fread(libzip_file_t *_Nonnull, void *_Nonnull, libzip_uint64_t);
ZIP_EXTERN libzip_int8_t libzip_fseek(libzip_file_t *_Nonnull, libzip_int64_t, int);
ZIP_EXTERN libzip_int64_t libzip_ftell(libzip_file_t *_Nonnull);
ZIP_EXTERN const char *_Nullable libzip_get_archive_comment(libzip_t *_Nonnull, int *_Nullable, libzip_flags_t);
ZIP_EXTERN int libzip_get_archive_flag(libzip_t *_Nonnull, libzip_flags_t, libzip_flags_t);
ZIP_EXTERN const char *_Nullable libzip_get_name(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t);
ZIP_EXTERN libzip_int64_t libzip_get_num_entries(libzip_t *_Nonnull, libzip_flags_t);
ZIP_EXTERN const char *_Nonnull libzip_liblibzip_version(void);
ZIP_EXTERN libzip_int64_t libzip_name_locate(libzip_t *_Nonnull, const char *_Nonnull, libzip_flags_t);
ZIP_EXTERN libzip_t *_Nullable libzip_open(const char *_Nonnull, int, int *_Nullable);
ZIP_EXTERN libzip_t *_Nullable libzip_open_from_source(libzip_source_t *_Nonnull, int, libzip_error_t *_Nullable);
ZIP_EXTERN int libzip_register_progress_callback_with_state(libzip_t *_Nonnull, double, libzip_progress_callback _Nullable, void (*_Nullable)(void *_Nullable), void *_Nullable);
ZIP_EXTERN int libzip_register_cancel_callback_with_state(libzip_t *_Nonnull, libzip_cancel_callback _Nullable, void (*_Nullable)(void *_Nullable), void *_Nullable);
ZIP_EXTERN int libzip_set_archive_comment(libzip_t *_Nonnull, const char *_Nullable, libzip_uint16_t);
ZIP_EXTERN int libzip_set_archive_flag(libzip_t *_Nonnull, libzip_flags_t, int);
ZIP_EXTERN int libzip_set_default_password(libzip_t *_Nonnull, const char *_Nullable);
ZIP_EXTERN int libzip_set_file_compression(libzip_t *_Nonnull, libzip_uint64_t, libzip_int32_t, libzip_uint32_t);
ZIP_EXTERN int libzip_source_begin_write(libzip_source_t *_Nonnull);
ZIP_EXTERN int libzip_source_begin_write_cloning(libzip_source_t *_Nonnull, libzip_uint64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_buffer(libzip_t *_Nonnull, const void *_Nullable, libzip_uint64_t, int);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_buffer_create(const void *_Nullable, libzip_uint64_t, int, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_buffer_fragment(libzip_t *_Nonnull, const libzip_buffer_fragment_t *_Nonnull, libzip_uint64_t, int);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_buffer_fragment_create(const libzip_buffer_fragment_t *_Nullable, libzip_uint64_t, int, libzip_error_t *_Nullable);
ZIP_EXTERN int libzip_source_close(libzip_source_t *_Nonnull);
ZIP_EXTERN int libzip_source_commit_write(libzip_source_t *_Nonnull);
ZIP_EXTERN libzip_error_t *_Nonnull libzip_source_error(libzip_source_t *_Nonnull);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_file(libzip_t *_Nonnull, const char *_Nonnull, libzip_uint64_t, libzip_int64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_file_create(const char *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_filep(libzip_t *_Nonnull, FILE *_Nonnull, libzip_uint64_t, libzip_int64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_filep_create(FILE *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
ZIP_EXTERN void libzip_source_free(libzip_source_t *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_function(libzip_t *_Nonnull, libzip_source_callback _Nonnull, void *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_function_create(libzip_source_callback _Nonnull, void *_Nullable, libzip_error_t *_Nullable);
ZIP_EXTERN int libzip_source_get_file_attributes(libzip_source_t *_Nonnull, libzip_file_attributes_t *_Nonnull);
ZIP_EXTERN int libzip_source_is_deleted(libzip_source_t *_Nonnull);
ZIP_EXTERN int libzip_source_is_seekable(libzip_source_t *_Nonnull);
ZIP_EXTERN void libzip_source_keep(libzip_source_t *_Nonnull);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_layered(libzip_t *_Nullable, libzip_source_t *_Nonnull, libzip_source_layered_callback _Nonnull, void *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_layered_create(libzip_source_t *_Nonnull, libzip_source_layered_callback _Nonnull, void *_Nullable, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_int64_t libzip_source_make_command_bitmap(libzip_source_cmd_t, ...);
ZIP_EXTERN int libzip_source_open(libzip_source_t *_Nonnull);
ZIP_EXTERN libzip_int64_t libzip_source_pass_to_lower_layer(libzip_source_t *_Nonnull, void *_Nullable, libzip_uint64_t, libzip_source_cmd_t);
ZIP_EXTERN libzip_int64_t libzip_source_read(libzip_source_t *_Nonnull, void *_Nonnull, libzip_uint64_t);
ZIP_EXTERN void libzip_source_rollback_write(libzip_source_t *_Nonnull);
ZIP_EXTERN int libzip_source_seek(libzip_source_t *_Nonnull, libzip_int64_t, int);
ZIP_EXTERN libzip_int64_t libzip_source_seek_compute_offset(libzip_uint64_t, libzip_uint64_t, void *_Nonnull, libzip_uint64_t, libzip_error_t *_Nullable);
ZIP_EXTERN int libzip_source_seek_write(libzip_source_t *_Nonnull, libzip_int64_t, int);
ZIP_EXTERN int libzip_source_stat(libzip_source_t *_Nonnull, libzip_stat_t *_Nonnull);
ZIP_EXTERN libzip_int64_t libzip_source_tell(libzip_source_t *_Nonnull);
ZIP_EXTERN libzip_int64_t libzip_source_tell_write(libzip_source_t *_Nonnull);
#ifdef _WIN32
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32a(libzip_t *_Nonnull, const char *_Nonnull, libzip_uint64_t, libzip_int64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32a_create(const char *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32handle(libzip_t *_Nonnull, void *_Nonnull, libzip_uint64_t, libzip_int64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32handle_create(void *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32w(libzip_t *_Nonnull, const wchar_t *_Nonnull, libzip_uint64_t, libzip_int64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_win32w_create(const wchar_t *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
#endif
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_window_create(libzip_source_t *_Nonnull, libzip_uint64_t, libzip_int64_t, libzip_error_t *_Nullable);
ZIP_EXTERN libzip_int64_t libzip_source_write(libzip_source_t *_Nonnull, const void *_Nullable, libzip_uint64_t);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_libzip_file(libzip_t *_Nonnull, libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint64_t, libzip_int64_t, const char *_Nullable);
ZIP_EXTERN libzip_source_t *_Nullable libzip_source_libzip_file_create(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_uint64_t, libzip_int64_t, const char *_Nullable, libzip_error_t *_Nullable);
ZIP_EXTERN int libzip_stat(libzip_t *_Nonnull, const char *_Nonnull, libzip_flags_t, libzip_stat_t *_Nonnull);
ZIP_EXTERN int libzip_stat_index(libzip_t *_Nonnull, libzip_uint64_t, libzip_flags_t, libzip_stat_t *_Nonnull);
ZIP_EXTERN void libzip_stat_init(libzip_stat_t *_Nonnull);
ZIP_EXTERN const char *_Nonnull libzip_strerror(libzip_t *_Nonnull);
ZIP_EXTERN int libzip_unchange(libzip_t *_Nonnull, libzip_uint64_t);
ZIP_EXTERN int libzip_unchange_all(libzip_t *_Nonnull);
ZIP_EXTERN int libzip_unchange_archive(libzip_t *_Nonnull);
ZIP_EXTERN int libzip_compression_method_supported(libzip_int32_t method, int compress);
ZIP_EXTERN int libzip_encryption_method_supported(libzip_uint16_t method, int encode);

#ifdef __cplusplus
}
#endif

#endif /* _HAD_ZIP_H */
