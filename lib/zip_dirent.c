/*
  libzip_dirent.c -- read directory entry (local or central), clean dirent
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
#include <sys/types.h>
#include <time.h>
#include <zlib.h>

#include "zipint.h"

static libzip_string_t *_libzip_dirent_process_ef_utf_8(const libzip_dirent_t *de, libzip_uint16_t id, libzip_string_t *str);
static libzip_extra_field_t *_libzip_ef_utf8(libzip_uint16_t, libzip_string_t *, libzip_error_t *);
static bool _libzip_dirent_process_winlibzip_aes(libzip_dirent_t *de, libzip_error_t *error);


void
_libzip_cdir_free(libzip_cdir_t *cd) {
    libzip_uint64_t i;

    if (!cd)
        return;

    for (i = 0; i < cd->nentry; i++)
        _libzip_entry_finalize(cd->entry + i);
    free(cd->entry);
    _libzip_string_free(cd->comment);
    free(cd);
}


libzip_cdir_t *
_libzip_cdir_new(libzip_uint64_t nentry, libzip_error_t *error) {
    libzip_cdir_t *cd;

    if ((cd = (libzip_cdir_t *)malloc(sizeof(*cd))) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    cd->entry = NULL;
    cd->nentry = cd->nentry_alloc = 0;
    cd->size = cd->offset = 0;
    cd->comment = NULL;
    cd->is_zip64 = false;

    if (!_libzip_cdir_grow(cd, nentry, error)) {
        _libzip_cdir_free(cd);
        return NULL;
    }

    return cd;
}


bool
_libzip_cdir_grow(libzip_cdir_t *cd, libzip_uint64_t additional_entries, libzip_error_t *error) {
    libzip_uint64_t i, new_alloc;
    libzip_entry_t *new_entry;

    if (additional_entries == 0) {
        return true;
    }

    new_alloc = cd->nentry_alloc + additional_entries;

    if (new_alloc < additional_entries || new_alloc > SIZE_MAX / sizeof(*(cd->entry))) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return false;
    }

    if ((new_entry = (libzip_entry_t *)realloc(cd->entry, sizeof(*(cd->entry)) * (size_t)new_alloc)) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return false;
    }

    cd->entry = new_entry;

    for (i = cd->nentry; i < new_alloc; i++) {
        _libzip_entry_init(cd->entry + i);
    }

    cd->nentry = cd->nentry_alloc = new_alloc;

    return true;
}


libzip_int64_t
_libzip_cdir_write(libzip_t *za, const libzip_filelist_t *filelist, libzip_uint64_t survivors) {
    libzip_uint64_t offset, size;
    libzip_string_t *comment;
    libzip_uint8_t buf[EOCDLEN + EOCD64LEN + EOCD64LOCLEN];
    libzip_buffer_t *buffer;
    libzip_int64_t off;
    libzip_uint64_t i;
    bool is_zip64;
    int ret;
    libzip_uint32_t cdir_crc;

    if ((off = libzip_source_tell_write(za->src)) < 0) {
        libzip_error_set_from_source(&za->error, za->src);
        return -1;
    }
    offset = (libzip_uint64_t)off;

    is_zip64 = false;

    if (LIBZIP_WANT_TORRENTZIP(za)) {
        cdir_crc = (libzip_uint32_t)crc32(0, NULL, 0);
        za->write_crc = &cdir_crc;
    }

    for (i = 0; i < survivors; i++) {
        libzip_entry_t *entry = za->entry + filelist[i].idx;

        if ((ret = _libzip_dirent_write(za, entry->changes ? entry->changes : entry->orig, LIBZIP_FL_CENTRAL)) < 0)
            return -1;
        if (ret)
            is_zip64 = true;
    }

    za->write_crc = NULL;

    if ((off = libzip_source_tell_write(za->src)) < 0) {
        libzip_error_set_from_source(&za->error, za->src);
        return -1;
    }
    size = (libzip_uint64_t)off - offset;

    if (offset > LIBZIP_UINT32_MAX || survivors > LIBZIP_UINT16_MAX) {
        is_zip64 = true;
    }

    if ((buffer = _libzip_buffer_new(buf, sizeof(buf))) == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
        return -1;
    }

    if (is_zip64) {
        _libzip_buffer_put(buffer, EOCD64_MAGIC, 4);
        _libzip_buffer_put_64(buffer, EOCD64LEN - 12);
        _libzip_buffer_put_16(buffer, 45);
        _libzip_buffer_put_16(buffer, 45);
        _libzip_buffer_put_32(buffer, 0);
        _libzip_buffer_put_32(buffer, 0);
        _libzip_buffer_put_64(buffer, survivors);
        _libzip_buffer_put_64(buffer, survivors);
        _libzip_buffer_put_64(buffer, size);
        _libzip_buffer_put_64(buffer, offset);
        _libzip_buffer_put(buffer, EOCD64LOC_MAGIC, 4);
        _libzip_buffer_put_32(buffer, 0);
        _libzip_buffer_put_64(buffer, offset + size);
        _libzip_buffer_put_32(buffer, 1);
    }

    _libzip_buffer_put(buffer, EOCD_MAGIC, 4);
    _libzip_buffer_put_32(buffer, 0);
    _libzip_buffer_put_16(buffer, (libzip_uint16_t)(survivors >= LIBZIP_UINT16_MAX ? LIBZIP_UINT16_MAX : survivors));
    _libzip_buffer_put_16(buffer, (libzip_uint16_t)(survivors >= LIBZIP_UINT16_MAX ? LIBZIP_UINT16_MAX : survivors));
    _libzip_buffer_put_32(buffer, size >= LIBZIP_UINT32_MAX ? LIBZIP_UINT32_MAX : (libzip_uint32_t)size);
    _libzip_buffer_put_32(buffer, offset >= LIBZIP_UINT32_MAX ? LIBZIP_UINT32_MAX : (libzip_uint32_t)offset);

    comment = za->comment_changed ? za->comment_changes : za->comment_orig;

    if (LIBZIP_WANT_TORRENTZIP(za)) {
        _libzip_buffer_put_16(buffer, TORRENTLIBZIP_SIGNATURE_LENGTH + TORRENTLIBZIP_CRC_LENGTH);
    }
    else {
        _libzip_buffer_put_16(buffer, (libzip_uint16_t)(comment ? comment->length : 0));
    }


    if (!_libzip_buffer_ok(buffer)) {
        libzip_error_set(&za->error, LIBZIP_ER_INTERNAL, 0);
        _libzip_buffer_free(buffer);
        return -1;
    }

    if (_libzip_write(za, _libzip_buffer_data(buffer), _libzip_buffer_offset(buffer)) < 0) {
        _libzip_buffer_free(buffer);
        return -1;
    }

    _libzip_buffer_free(buffer);

    if (LIBZIP_WANT_TORRENTZIP(za)) {
        char torrentlibzip_comment[TORRENTLIBZIP_SIGNATURE_LENGTH + TORRENTLIBZIP_CRC_LENGTH + 1];
        snprintf(torrentlibzip_comment, sizeof(torrentlibzip_comment), TORRENTLIBZIP_SIGNATURE "%08X", cdir_crc);

        if (_libzip_write(za, torrentlibzip_comment, strlen(torrentlibzip_comment)) < 0) {
            return -1;
        }
    }
    else if (comment != NULL) {
        if (_libzip_write(za, comment->raw, comment->length) < 0) {
            return -1;
        }
    }

    return (libzip_int64_t)size;
}


libzip_dirent_t *
_libzip_dirent_clone(const libzip_dirent_t *sde) {
    libzip_dirent_t *tde;

    if ((tde = (libzip_dirent_t *)malloc(sizeof(*tde))) == NULL)
        return NULL;

    if (sde)
        (void)memcpy_s(tde, sizeof(*tde), sde, sizeof(*sde));
    else
        _libzip_dirent_init(tde);

    tde->changed = 0;
    tde->cloned = 1;

    return tde;
}


void
_libzip_dirent_finalize(libzip_dirent_t *zde) {
    if (!zde->cloned || zde->changed & LIBZIP_DIRENT_FILENAME) {
        _libzip_string_free(zde->filename);
        zde->filename = NULL;
    }
    if (!zde->cloned || zde->changed & LIBZIP_DIRENT_EXTRA_FIELD) {
        _libzip_ef_free(zde->extra_fields);
        zde->extra_fields = NULL;
    }
    if (!zde->cloned || zde->changed & LIBZIP_DIRENT_COMMENT) {
        _libzip_string_free(zde->comment);
        zde->comment = NULL;
    }
    if (!zde->cloned || zde->changed & LIBZIP_DIRENT_PASSWORD) {
        if (zde->password) {
            _libzip_crypto_clear(zde->password, strlen(zde->password));
        }
        free(zde->password);
        zde->password = NULL;
    }
}


void
_libzip_dirent_free(libzip_dirent_t *zde) {
    if (zde == NULL)
        return;

    _libzip_dirent_finalize(zde);
    free(zde);
}


void
_libzip_dirent_init(libzip_dirent_t *de) {
    de->changed = 0;
    de->local_extra_fields_read = 0;
    de->cloned = 0;

    de->crc_valid = true;
    de->version_madeby = 63 | (LIBZIP_OPSYS_DEFAULT << 8);
    de->version_needed = 10; /* 1.0 */
    de->bitflags = 0;
    de->comp_method = LIBZIP_CM_DEFAULT;
    de->last_mod = 0;
    de->crc = 0;
    de->comp_size = 0;
    de->uncomp_size = 0;
    de->filename = NULL;
    de->extra_fields = NULL;
    de->comment = NULL;
    de->disk_number = 0;
    de->int_attrib = 0;
    de->ext_attrib = LIBZIP_EXT_ATTRIB_DEFAULT;
    de->offset = 0;
    de->compression_level = 0;
    de->encryption_method = LIBZIP_EM_NONE;
    de->password = NULL;
}


bool
_libzip_dirent_needs_zip64(const libzip_dirent_t *de, libzip_flags_t flags) {
    if (de->uncomp_size >= LIBZIP_UINT32_MAX || de->comp_size >= LIBZIP_UINT32_MAX || ((flags & LIBZIP_FL_CENTRAL) && de->offset >= LIBZIP_UINT32_MAX))
        return true;

    return false;
}


libzip_dirent_t *
_libzip_dirent_new(void) {
    libzip_dirent_t *de;

    if ((de = (libzip_dirent_t *)malloc(sizeof(*de))) == NULL)
        return NULL;

    _libzip_dirent_init(de);
    return de;
}


/* _libzip_dirent_read(zde, fp, bufp, left, localp, error):
   Fills the zip directory entry zde.

   If buffer is non-NULL, data is taken from there; otherwise data is read from fp as needed.

   If local is true, it reads a local header instead of a central directory entry.

   Returns size of dirent read if successful. On error, error is filled in and -1 is returned.
*/

libzip_int64_t
_libzip_dirent_read(libzip_dirent_t *zde, libzip_source_t *src, libzip_buffer_t *buffer, bool local, libzip_error_t *error) {
    libzip_uint8_t buf[CDENTRYSIZE];
    libzip_uint16_t dostime, dosdate;
    libzip_uint32_t size, variable_size;
    libzip_uint16_t filename_len, comment_len, ef_len;

    bool from_buffer = (buffer != NULL);

    size = local ? LENTRYSIZE : CDENTRYSIZE;

    if (buffer) {
        if (_libzip_buffer_left(buffer) < size) {
            libzip_error_set(error, LIBZIP_ER_NOZIP, 0);
            return -1;
        }
    }
    else {
        if ((buffer = _libzip_buffer_new_from_source(src, size, buf, error)) == NULL) {
            return -1;
        }
    }

    if (memcmp(_libzip_buffer_get(buffer, 4), (local ? LOCAL_MAGIC : CENTRAL_MAGIC), 4) != 0) {
        libzip_error_set(error, LIBZIP_ER_NOZIP, 0);
        if (!from_buffer) {
            _libzip_buffer_free(buffer);
        }
        return -1;
    }

    /* convert buffercontents to libzip_dirent */

    _libzip_dirent_init(zde);
    if (!local)
        zde->version_madeby = _libzip_buffer_get_16(buffer);
    else
        zde->version_madeby = 0;
    zde->version_needed = _libzip_buffer_get_16(buffer);
    zde->bitflags = _libzip_buffer_get_16(buffer);
    zde->comp_method = _libzip_buffer_get_16(buffer);

    /* convert to time_t */
    dostime = _libzip_buffer_get_16(buffer);
    dosdate = _libzip_buffer_get_16(buffer);
    zde->last_mod = _libzip_d2u_time(dostime, dosdate);

    zde->crc = _libzip_buffer_get_32(buffer);
    zde->comp_size = _libzip_buffer_get_32(buffer);
    zde->uncomp_size = _libzip_buffer_get_32(buffer);

    filename_len = _libzip_buffer_get_16(buffer);
    ef_len = _libzip_buffer_get_16(buffer);

    if (local) {
        comment_len = 0;
        zde->disk_number = 0;
        zde->int_attrib = 0;
        zde->ext_attrib = 0;
        zde->offset = 0;
    }
    else {
        comment_len = _libzip_buffer_get_16(buffer);
        zde->disk_number = _libzip_buffer_get_16(buffer);
        zde->int_attrib = _libzip_buffer_get_16(buffer);
        zde->ext_attrib = _libzip_buffer_get_32(buffer);
        zde->offset = _libzip_buffer_get_32(buffer);
    }

    if (!_libzip_buffer_ok(buffer)) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        if (!from_buffer) {
            _libzip_buffer_free(buffer);
        }
        return -1;
    }

    if (zde->bitflags & LIBZIP_GPBF_ENCRYPTED) {
        if (zde->bitflags & LIBZIP_GPBF_STRONG_ENCRYPTION) {
            /* TODO */
            zde->encryption_method = LIBZIP_EM_UNKNOWN;
        }
        else {
            zde->encryption_method = LIBZIP_EM_TRAD_PKWARE;
        }
    }
    else {
        zde->encryption_method = LIBZIP_EM_NONE;
    }

    zde->filename = NULL;
    zde->extra_fields = NULL;
    zde->comment = NULL;

    variable_size = (libzip_uint32_t)filename_len + (libzip_uint32_t)ef_len + (libzip_uint32_t)comment_len;

    if (from_buffer) {
        if (_libzip_buffer_left(buffer) < variable_size) {
            libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_VARIABLE_SIZE_OVERFLOW);
            return -1;
        }
    }
    else {
        _libzip_buffer_free(buffer);

        if ((buffer = _libzip_buffer_new_from_source(src, variable_size, NULL, error)) == NULL) {
            return -1;
        }
    }

    if (filename_len) {
        zde->filename = _libzip_read_string(buffer, src, filename_len, 1, error);
        if (!zde->filename) {
            if (libzip_error_code_zip(error) == LIBZIP_ER_EOF) {
                libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_VARIABLE_SIZE_OVERFLOW);
            }
            if (!from_buffer) {
                _libzip_buffer_free(buffer);
            }
            return -1;
        }

        if (zde->bitflags & LIBZIP_GPBF_ENCODING_UTF_8) {
            if (_libzip_guess_encoding(zde->filename, LIBZIP_ENCODING_UTF8_KNOWN) == LIBZIP_ENCODING_ERROR) {
                libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_INVALID_UTF8_IN_FILENAME);
                if (!from_buffer) {
                    _libzip_buffer_free(buffer);
                }
                return -1;
            }
        }
    }

    if (ef_len) {
        libzip_uint8_t *ef = _libzip_read_data(buffer, src, ef_len, 0, error);

        if (ef == NULL) {
            if (!from_buffer) {
                _libzip_buffer_free(buffer);
            }
            return -1;
        }
        if (!_libzip_ef_parse(ef, ef_len, local ? LIBZIP_EF_LOCAL : LIBZIP_EF_CENTRAL, &zde->extra_fields, error)) {
            free(ef);
            if (!from_buffer) {
                _libzip_buffer_free(buffer);
            }
            return -1;
        }
        free(ef);
        if (local)
            zde->local_extra_fields_read = 1;
    }

    if (comment_len) {
        zde->comment = _libzip_read_string(buffer, src, comment_len, 0, error);
        if (!zde->comment) {
            if (!from_buffer) {
                _libzip_buffer_free(buffer);
            }
            return -1;
        }
        if (zde->bitflags & LIBZIP_GPBF_ENCODING_UTF_8) {
            if (_libzip_guess_encoding(zde->comment, LIBZIP_ENCODING_UTF8_KNOWN) == LIBZIP_ENCODING_ERROR) {
                libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_INVALID_UTF8_IN_COMMENT);
                if (!from_buffer) {
                    _libzip_buffer_free(buffer);
                }
                return -1;
            }
        }
    }

    zde->filename = _libzip_dirent_process_ef_utf_8(zde, LIBZIP_EF_UTF_8_NAME, zde->filename);
    zde->comment = _libzip_dirent_process_ef_utf_8(zde, LIBZIP_EF_UTF_8_COMMENT, zde->comment);

    /* Zip64 */

    if (zde->uncomp_size == LIBZIP_UINT32_MAX || zde->comp_size == LIBZIP_UINT32_MAX || zde->offset == LIBZIP_UINT32_MAX) {
        libzip_uint16_t got_len;
        const libzip_uint8_t *ef = _libzip_ef_get_by_id(zde->extra_fields, &got_len, LIBZIP_EF_ZIP64, 0, local ? LIBZIP_EF_LOCAL : LIBZIP_EF_CENTRAL, error);
        if (ef != NULL) {
            if (!libzip_dirent_process_ef_zip64(zde, ef, got_len, local, error)) {
                if (!from_buffer) {
                    _libzip_buffer_free(buffer);
                }
                return -1;
            }
        }
    }


    if (!_libzip_buffer_ok(buffer)) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        if (!from_buffer) {
            _libzip_buffer_free(buffer);
        }
        return -1;
    }
    if (!from_buffer) {
        _libzip_buffer_free(buffer);
    }

    /* libzip_source_seek / libzip_source_tell don't support values > LIBZIP_INT64_MAX */
    if (zde->offset > LIBZIP_INT64_MAX) {
        libzip_error_set(error, LIBZIP_ER_SEEK, EFBIG);
        return -1;
    }

    if (!_libzip_dirent_process_winlibzip_aes(zde, error)) {
        return -1;
    }

    zde->extra_fields = _libzip_ef_remove_internal(zde->extra_fields);

    return (libzip_int64_t)size + (libzip_int64_t)variable_size;
}

bool libzip_dirent_process_ef_zip64(libzip_dirent_t* zde, const libzip_uint8_t* ef, libzip_uint64_t got_len, bool local, libzip_error_t* error) {
    libzip_buffer_t *ef_buffer;

    if ((ef_buffer = _libzip_buffer_new((libzip_uint8_t *)ef, got_len)) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return false;
    }

    if (zde->uncomp_size == LIBZIP_UINT32_MAX) {
        zde->uncomp_size = _libzip_buffer_get_64(ef_buffer);
    }
    else if (local) {
        /* From appnote.txt: This entry in the Local header MUST
           include BOTH original and compressed file size fields. */
        (void)_libzip_buffer_skip(ef_buffer, 8); /* error is caught by _libzip_buffer_eof() call */
    }
    if (zde->comp_size == LIBZIP_UINT32_MAX) {
        zde->comp_size = _libzip_buffer_get_64(ef_buffer);
    }
    if (!local) {
        if (zde->offset == LIBZIP_UINT32_MAX) {
            zde->offset = _libzip_buffer_get_64(ef_buffer);
        }
        if (zde->disk_number == LIBZIP_UINT16_MAX) {
            zde->disk_number = _libzip_buffer_get_32(ef_buffer);
        }
    }

    if (!_libzip_buffer_eof(ef_buffer)) {
        /* accept additional fields if values match */
        bool ok = true;
        switch (got_len) {
        case 28:
            _libzip_buffer_set_offset(ef_buffer, 24);
            if (zde->disk_number != _libzip_buffer_get_32(ef_buffer)) {
                ok = false;
            }
            /* fallthrough */
        case 24:
            _libzip_buffer_set_offset(ef_buffer, 0);
            if ((zde->uncomp_size != _libzip_buffer_get_64(ef_buffer)) || (zde->comp_size != _libzip_buffer_get_64(ef_buffer)) || (zde->offset != _libzip_buffer_get_64(ef_buffer))) {
                ok = false;
            }
            break;

        default:
            ok = false;
        }
        if (!ok) {
            libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_INVALID_ZIP64_EF);
            _libzip_buffer_free(ef_buffer);
            return false;
        }
    }
    _libzip_buffer_free(ef_buffer);

    return true;
}


static libzip_string_t *
_libzip_dirent_process_ef_utf_8(const libzip_dirent_t *de, libzip_uint16_t id, libzip_string_t *str) {
    libzip_uint16_t ef_len;
    libzip_uint32_t ef_crc;
    libzip_buffer_t *buffer;

    const libzip_uint8_t *ef = _libzip_ef_get_by_id(de->extra_fields, &ef_len, id, 0, LIBZIP_EF_BOTH, NULL);

    if (ef == NULL || ef_len < 5 || ef[0] != 1) {
        return str;
    }

    if ((buffer = _libzip_buffer_new((libzip_uint8_t *)ef, ef_len)) == NULL) {
        return str;
    }

    _libzip_buffer_get_8(buffer);
    ef_crc = _libzip_buffer_get_32(buffer);

    if (_libzip_string_crc32(str) == ef_crc) {
        libzip_uint16_t len = (libzip_uint16_t)_libzip_buffer_left(buffer);
        libzip_string_t *ef_str = _libzip_string_new(_libzip_buffer_get(buffer, len), len, LIBZIP_FL_ENC_UTF_8, NULL);

        if (ef_str != NULL) {
            _libzip_string_free(str);
            str = ef_str;
        }
    }

    _libzip_buffer_free(buffer);

    return str;
}


static bool
_libzip_dirent_process_winlibzip_aes(libzip_dirent_t *de, libzip_error_t *error) {
    libzip_uint16_t ef_len;
    libzip_buffer_t *buffer;
    const libzip_uint8_t *ef;
    bool crc_valid;
    libzip_uint16_t enc_method;


    if (de->comp_method != LIBZIP_CM_WINLIBZIP_AES) {
        return true;
    }

    ef = _libzip_ef_get_by_id(de->extra_fields, &ef_len, LIBZIP_EF_WINLIBZIP_AES, 0, LIBZIP_EF_BOTH, NULL);

    if (ef == NULL || ef_len < 7) {
        libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_INVALID_WINZIPAES_EF);
        return false;
    }

    if ((buffer = _libzip_buffer_new((libzip_uint8_t *)ef, ef_len)) == NULL) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        return false;
    }

    /* version */

    crc_valid = true;
    switch (_libzip_buffer_get_16(buffer)) {
        case 1:
            break;

        case 2:
            crc_valid = false;
            /* TODO: When checking consistency, check that crc is 0. */
            break;
            
        default:
            libzip_error_set(error, LIBZIP_ER_ENCRNOTSUPP, 0);
            _libzip_buffer_free(buffer);
            return false;
    }

    /* vendor */
    if (memcmp(_libzip_buffer_get(buffer, 2), "AE", 2) != 0) {
        libzip_error_set(error, LIBZIP_ER_ENCRNOTSUPP, 0);
        _libzip_buffer_free(buffer);
        return false;
    }

    /* mode */
    switch (_libzip_buffer_get_8(buffer)) {
    case 1:
        enc_method = LIBZIP_EM_AES_128;
        break;
    case 2:
        enc_method = LIBZIP_EM_AES_192;
        break;
    case 3:
        enc_method = LIBZIP_EM_AES_256;
        break;
    default:
        libzip_error_set(error, LIBZIP_ER_ENCRNOTSUPP, 0);
        _libzip_buffer_free(buffer);
        return false;
    }

    if (ef_len != 7) {
        libzip_error_set(error, LIBZIP_ER_INCONS, LIBZIP_ER_DETAIL_INVALID_WINZIPAES_EF);
        _libzip_buffer_free(buffer);
        return false;
    }

    de->crc_valid = crc_valid;
    de->encryption_method = enc_method;
    de->comp_method = _libzip_buffer_get_16(buffer);

    _libzip_buffer_free(buffer);
    return true;
}


libzip_int32_t
_libzip_dirent_size(libzip_source_t *src, libzip_uint16_t flags, libzip_error_t *error) {
    libzip_int32_t size;
    bool local = (flags & LIBZIP_EF_LOCAL) != 0;
    int i;
    libzip_uint8_t b[6];
    libzip_buffer_t *buffer;

    size = local ? LENTRYSIZE : CDENTRYSIZE;

    if (libzip_source_seek(src, local ? 26 : 28, SEEK_CUR) < 0) {
        libzip_error_set_from_source(error, src);
        return -1;
    }

    if ((buffer = _libzip_buffer_new_from_source(src, local ? 4 : 6, b, error)) == NULL) {
        return -1;
    }

    for (i = 0; i < (local ? 2 : 3); i++) {
        size += _libzip_buffer_get_16(buffer);
    }

    if (!_libzip_buffer_eof(buffer)) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        _libzip_buffer_free(buffer);
        return -1;
    }

    _libzip_buffer_free(buffer);
    return size;
}


/* _libzip_dirent_write
   Writes zip directory entry.

   If flags & LIBZIP_EF_LOCAL, it writes a local header instead of a central
   directory entry.  If flags & LIBZIP_EF_FORCE_ZIP64, a ZIP64 extra field is written, even if not needed.

   Returns 0 if successful, 1 if successful and wrote ZIP64 extra field. On error, error is filled in and -1 is
   returned.
*/

int
_libzip_dirent_write(libzip_t *za, libzip_dirent_t *de, libzip_flags_t flags) {
    libzip_uint16_t dostime, dosdate;
    libzip_encoding_type_t com_enc, name_enc;
    libzip_extra_field_t *ef;
    libzip_extra_field_t *ef64;
    libzip_uint32_t ef_total_size;
    bool is_zip64;
    bool is_really_zip64;
    bool is_winlibzip_aes;
    libzip_uint8_t buf[CDENTRYSIZE];
    libzip_buffer_t *buffer;

    ef = NULL;

    name_enc = _libzip_guess_encoding(de->filename, LIBZIP_ENCODING_UNKNOWN);
    com_enc = _libzip_guess_encoding(de->comment, LIBZIP_ENCODING_UNKNOWN);

    if ((name_enc == LIBZIP_ENCODING_UTF8_KNOWN && com_enc == LIBZIP_ENCODING_ASCII) || (name_enc == LIBZIP_ENCODING_ASCII && com_enc == LIBZIP_ENCODING_UTF8_KNOWN) || (name_enc == LIBZIP_ENCODING_UTF8_KNOWN && com_enc == LIBZIP_ENCODING_UTF8_KNOWN))
        de->bitflags |= LIBZIP_GPBF_ENCODING_UTF_8;
    else {
        de->bitflags &= (libzip_uint16_t)~LIBZIP_GPBF_ENCODING_UTF_8;
        if (name_enc == LIBZIP_ENCODING_UTF8_KNOWN) {
            ef = _libzip_ef_utf8(LIBZIP_EF_UTF_8_NAME, de->filename, &za->error);
            if (ef == NULL)
                return -1;
        }
        if ((flags & LIBZIP_FL_LOCAL) == 0 && com_enc == LIBZIP_ENCODING_UTF8_KNOWN) {
            libzip_extra_field_t *ef2 = _libzip_ef_utf8(LIBZIP_EF_UTF_8_COMMENT, de->comment, &za->error);
            if (ef2 == NULL) {
                _libzip_ef_free(ef);
                return -1;
            }
            ef2->next = ef;
            ef = ef2;
        }
    }

    if (de->encryption_method == LIBZIP_EM_NONE) {
        de->bitflags &= (libzip_uint16_t)~LIBZIP_GPBF_ENCRYPTED;
    }
    else {
        de->bitflags |= (libzip_uint16_t)LIBZIP_GPBF_ENCRYPTED;
    }

    is_really_zip64 = _libzip_dirent_needs_zip64(de, flags);
    is_zip64 = (flags & (LIBZIP_FL_LOCAL | LIBZIP_FL_FORCE_ZIP64)) == (LIBZIP_FL_LOCAL | LIBZIP_FL_FORCE_ZIP64) || is_really_zip64;
    is_winlibzip_aes = de->encryption_method == LIBZIP_EM_AES_128 || de->encryption_method == LIBZIP_EM_AES_192 || de->encryption_method == LIBZIP_EM_AES_256;

    if (is_zip64) {
        libzip_uint8_t ef_zip64[EFZIP64SIZE];
        libzip_buffer_t *ef_buffer = _libzip_buffer_new(ef_zip64, sizeof(ef_zip64));
        if (ef_buffer == NULL) {
            libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
            _libzip_ef_free(ef);
            return -1;
        }

        if (flags & LIBZIP_FL_LOCAL) {
            if ((flags & LIBZIP_FL_FORCE_ZIP64) || de->comp_size > LIBZIP_UINT32_MAX || de->uncomp_size > LIBZIP_UINT32_MAX) {
                _libzip_buffer_put_64(ef_buffer, de->uncomp_size);
                _libzip_buffer_put_64(ef_buffer, de->comp_size);
            }
        }
        else {
            if ((flags & LIBZIP_FL_FORCE_ZIP64) || de->comp_size > LIBZIP_UINT32_MAX || de->uncomp_size > LIBZIP_UINT32_MAX || de->offset > LIBZIP_UINT32_MAX) {
                if (de->uncomp_size >= LIBZIP_UINT32_MAX) {
                    _libzip_buffer_put_64(ef_buffer, de->uncomp_size);
                }
                if (de->comp_size >= LIBZIP_UINT32_MAX) {
                    _libzip_buffer_put_64(ef_buffer, de->comp_size);
                }
                if (de->offset >= LIBZIP_UINT32_MAX) {
                    _libzip_buffer_put_64(ef_buffer, de->offset);
                }
            }
        }

        if (!_libzip_buffer_ok(ef_buffer)) {
            libzip_error_set(&za->error, LIBZIP_ER_INTERNAL, 0);
            _libzip_buffer_free(ef_buffer);
            _libzip_ef_free(ef);
            return -1;
        }

        ef64 = _libzip_ef_new(LIBZIP_EF_ZIP64, (libzip_uint16_t)(_libzip_buffer_offset(ef_buffer)), ef_zip64, LIBZIP_EF_BOTH);
        _libzip_buffer_free(ef_buffer);
        ef64->next = ef;
        ef = ef64;
    }

    if (is_winlibzip_aes) {
        libzip_uint8_t data[EF_WINLIBZIP_AES_SIZE];
        libzip_buffer_t *ef_buffer = _libzip_buffer_new(data, sizeof(data));
        libzip_extra_field_t *ef_winzip;

        if (ef_buffer == NULL) {
            libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
            _libzip_ef_free(ef);
            return -1;
        }

        _libzip_buffer_put_16(ef_buffer, 2);
        _libzip_buffer_put(ef_buffer, "AE", 2);
        _libzip_buffer_put_8(ef_buffer, (libzip_uint8_t)(de->encryption_method & 0xff));
        _libzip_buffer_put_16(ef_buffer, (libzip_uint16_t)de->comp_method);

        if (!_libzip_buffer_ok(ef_buffer)) {
            libzip_error_set(&za->error, LIBZIP_ER_INTERNAL, 0);
            _libzip_buffer_free(ef_buffer);
            _libzip_ef_free(ef);
            return -1;
        }

        ef_winzip = _libzip_ef_new(LIBZIP_EF_WINLIBZIP_AES, EF_WINLIBZIP_AES_SIZE, data, LIBZIP_EF_BOTH);
        _libzip_buffer_free(ef_buffer);
        ef_winzip->next = ef;
        ef = ef_winzip;
    }

    if ((buffer = _libzip_buffer_new(buf, sizeof(buf))) == NULL) {
        libzip_error_set(&za->error, LIBZIP_ER_MEMORY, 0);
        _libzip_ef_free(ef);
        return -1;
    }

    _libzip_buffer_put(buffer, (flags & LIBZIP_FL_LOCAL) ? LOCAL_MAGIC : CENTRAL_MAGIC, 4);

    if ((flags & LIBZIP_FL_LOCAL) == 0) {
        _libzip_buffer_put_16(buffer, de->version_madeby);
    }
    _libzip_buffer_put_16(buffer, LIBZIP_MAX(is_really_zip64 ? 45 : 0, de->version_needed));
    _libzip_buffer_put_16(buffer, de->bitflags);
    if (is_winlibzip_aes) {
        _libzip_buffer_put_16(buffer, LIBZIP_CM_WINLIBZIP_AES);
    }
    else {
        _libzip_buffer_put_16(buffer, (libzip_uint16_t)de->comp_method);
    }

    if (LIBZIP_WANT_TORRENTZIP(za)) {
        dostime = 0xbc00;
        dosdate = 0x2198;
    }
    else {
        _libzip_u2d_time(de->last_mod, &dostime, &dosdate);
    }
    _libzip_buffer_put_16(buffer, dostime);
    _libzip_buffer_put_16(buffer, dosdate);

    if (is_winlibzip_aes && de->uncomp_size < 20) {
        _libzip_buffer_put_32(buffer, 0);
    }
    else {
        _libzip_buffer_put_32(buffer, de->crc);
    }

    if (((flags & LIBZIP_FL_LOCAL) == LIBZIP_FL_LOCAL) && ((de->comp_size >= LIBZIP_UINT32_MAX) || (de->uncomp_size >= LIBZIP_UINT32_MAX))) {
        /* In local headers, if a ZIP64 EF is written, it MUST contain
         * both compressed and uncompressed sizes (even if one of the
         * two is smaller than 0xFFFFFFFF); on the other hand, those
         * may only appear when the corresponding standard entry is
         * 0xFFFFFFFF.  (appnote.txt 4.5.3) */
        _libzip_buffer_put_32(buffer, LIBZIP_UINT32_MAX);
        _libzip_buffer_put_32(buffer, LIBZIP_UINT32_MAX);
    }
    else {
        if (de->comp_size < LIBZIP_UINT32_MAX) {
            _libzip_buffer_put_32(buffer, (libzip_uint32_t)de->comp_size);
        }
        else {
            _libzip_buffer_put_32(buffer, LIBZIP_UINT32_MAX);
        }
        if (de->uncomp_size < LIBZIP_UINT32_MAX) {
            _libzip_buffer_put_32(buffer, (libzip_uint32_t)de->uncomp_size);
        }
        else {
            _libzip_buffer_put_32(buffer, LIBZIP_UINT32_MAX);
        }
    }

    _libzip_buffer_put_16(buffer, _libzip_string_length(de->filename));
    ef_total_size = (libzip_uint32_t)_libzip_ef_size(ef, LIBZIP_EF_BOTH);
    if (!LIBZIP_WANT_TORRENTZIP(za)) {
        /* TODO: check for overflow */
        ef_total_size += (libzip_uint32_t)_libzip_ef_size(de->extra_fields, flags);
    }
    _libzip_buffer_put_16(buffer, (libzip_uint16_t)ef_total_size);

    if ((flags & LIBZIP_FL_LOCAL) == 0) {
        _libzip_buffer_put_16(buffer, LIBZIP_WANT_TORRENTZIP(za) ? 0 : _libzip_string_length(de->comment));
        _libzip_buffer_put_16(buffer, (libzip_uint16_t)de->disk_number);
        _libzip_buffer_put_16(buffer, de->int_attrib);
        _libzip_buffer_put_32(buffer, de->ext_attrib);
        if (de->offset < LIBZIP_UINT32_MAX)
            _libzip_buffer_put_32(buffer, (libzip_uint32_t)de->offset);
        else
            _libzip_buffer_put_32(buffer, LIBZIP_UINT32_MAX);
    }

    if (!_libzip_buffer_ok(buffer)) {
        libzip_error_set(&za->error, LIBZIP_ER_INTERNAL, 0);
        _libzip_buffer_free(buffer);
        _libzip_ef_free(ef);
        return -1;
    }

    if (_libzip_write(za, buf, _libzip_buffer_offset(buffer)) < 0) {
        _libzip_buffer_free(buffer);
        _libzip_ef_free(ef);
        return -1;
    }

    _libzip_buffer_free(buffer);

    if (de->filename) {
        if (_libzip_string_write(za, de->filename) < 0) {
            _libzip_ef_free(ef);
            return -1;
        }
    }

    if (ef) {
        if (_libzip_ef_write(za, ef, LIBZIP_EF_BOTH) < 0) {
            _libzip_ef_free(ef);
            return -1;
        }
    }
    _libzip_ef_free(ef);
    if (de->extra_fields && !LIBZIP_WANT_TORRENTZIP(za)) {
        if (_libzip_ef_write(za, de->extra_fields, flags) < 0) {
            return -1;
        }
    }

    if ((flags & LIBZIP_FL_LOCAL) == 0 && !LIBZIP_WANT_TORRENTZIP(za)) {
        if (de->comment) {
            if (_libzip_string_write(za, de->comment) < 0) {
                return -1;
            }
        }
    }


    return is_zip64;
}


time_t
_libzip_d2u_time(libzip_uint16_t dtime, libzip_uint16_t ddate) {
    struct tm tm;

    memset(&tm, 0, sizeof(tm));

    /* let mktime decide if DST is in effect */
    tm.tm_isdst = -1;

    tm.tm_year = ((ddate >> 9) & 127) + 1980 - 1900;
    tm.tm_mon = ((ddate >> 5) & 15) - 1;
    tm.tm_mday = ddate & 31;

    tm.tm_hour = (dtime >> 11) & 31;
    tm.tm_min = (dtime >> 5) & 63;
    tm.tm_sec = (dtime << 1) & 62;

    return mktime(&tm);
}


static libzip_extra_field_t *
_libzip_ef_utf8(libzip_uint16_t id, libzip_string_t *str, libzip_error_t *error) {
    const libzip_uint8_t *raw;
    libzip_uint32_t len;
    libzip_buffer_t *buffer;
    libzip_extra_field_t *ef;

    if ((raw = _libzip_string_get(str, &len, LIBZIP_FL_ENC_RAW, NULL)) == NULL) {
        /* error already set */
        return NULL;
    }

    if (len + 5 > LIBZIP_UINT16_MAX) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0); /* TODO: better error code? */
        return NULL;
    }

    if ((buffer = _libzip_buffer_new(NULL, len + 5)) == NULL) {
        libzip_error_set(error, LIBZIP_ER_MEMORY, 0);
        return NULL;
    }

    _libzip_buffer_put_8(buffer, 1);
    _libzip_buffer_put_32(buffer, _libzip_string_crc32(str));
    _libzip_buffer_put(buffer, raw, len);

    if (!_libzip_buffer_ok(buffer)) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        _libzip_buffer_free(buffer);
        return NULL;
    }

    ef = _libzip_ef_new(id, (libzip_uint16_t)(_libzip_buffer_offset(buffer)), _libzip_buffer_data(buffer), LIBZIP_EF_BOTH);
    _libzip_buffer_free(buffer);

    return ef;
}


libzip_dirent_t *
_libzip_get_dirent(libzip_t *za, libzip_uint64_t idx, libzip_flags_t flags, libzip_error_t *error) {
    if (error == NULL)
        error = &za->error;

    if (idx >= za->nentry) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((flags & LIBZIP_FL_UNCHANGED) || za->entry[idx].changes == NULL) {
        if (za->entry[idx].orig == NULL) {
            libzip_error_set(error, LIBZIP_ER_INVAL, 0);
            return NULL;
        }
        if (za->entry[idx].deleted && (flags & LIBZIP_FL_UNCHANGED) == 0) {
            libzip_error_set(error, LIBZIP_ER_DELETED, 0);
            return NULL;
        }
        return za->entry[idx].orig;
    }
    else
        return za->entry[idx].changes;
}


void
_libzip_u2d_time(time_t intime, libzip_uint16_t *dtime, libzip_uint16_t *ddate) {
    struct tm *tpm;
    struct tm tm;
    tpm = libzip_localtime(&intime, &tm);
    if (tpm == NULL) {
        /* if localtime fails, return an arbitrary date (1980-01-01 00:00:00) */
        *ddate = (1 << 5) + 1;
        *dtime = 0;
        return;
    }
    if (tpm->tm_year < 80) {
        tpm->tm_year = 80;
    }

    *ddate = (libzip_uint16_t)(((tpm->tm_year + 1900 - 1980) << 9) + ((tpm->tm_mon + 1) << 5) + tpm->tm_mday);
    *dtime = (libzip_uint16_t)(((tpm->tm_hour) << 11) + ((tpm->tm_min) << 5) + ((tpm->tm_sec) >> 1));
}


void
_libzip_dirent_apply_attributes(libzip_dirent_t *de, libzip_file_attributes_t *attributes, bool force_zip64, libzip_uint32_t changed) {
    libzip_uint16_t length;

    if (attributes->valid & LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS) {
        libzip_uint16_t mask = attributes->general_purpose_bit_mask & LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS_ALLOWED_MASK;
        de->bitflags = (de->bitflags & ~mask) | (attributes->general_purpose_bit_flags & mask);
    }
    if (attributes->valid & LIBZIP_FILE_ATTRIBUTES_ASCII) {
        de->int_attrib = (de->int_attrib & ~0x1) | (attributes->ascii ? 1 : 0);
    }
    /* manually set attributes are preferred over attributes provided by source */
    if ((changed & LIBZIP_DIRENT_ATTRIBUTES) == 0 && (attributes->valid & LIBZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES)) {
        de->ext_attrib = attributes->external_file_attributes;
    }

    if (de->comp_method == LIBZIP_CM_LZMA) {
        de->version_needed = 63;
    }
    else if (de->encryption_method == LIBZIP_EM_AES_128 || de->encryption_method == LIBZIP_EM_AES_192 || de->encryption_method == LIBZIP_EM_AES_256) {
        de->version_needed = 51;
    }
    else if (de->comp_method == LIBZIP_CM_BZIP2) {
        de->version_needed = 46;
    }
    else if (force_zip64 || _libzip_dirent_needs_zip64(de, 0)) {
        de->version_needed = 45;
    }
    else if (de->comp_method == LIBZIP_CM_DEFLATE || de->encryption_method == LIBZIP_EM_TRAD_PKWARE) {
        de->version_needed = 20;
    }
    else if ((length = _libzip_string_length(de->filename)) > 0 && de->filename->raw[length - 1] == '/') {
        de->version_needed = 20;
    }
    else {
        de->version_needed = 10;
    }

    if (attributes->valid & LIBZIP_FILE_ATTRIBUTES_VERSION_NEEDED) {
        de->version_needed = LIBZIP_MAX(de->version_needed, attributes->version_needed);
    }

    de->version_madeby = 63 | (de->version_madeby & 0xff00);
    if ((changed & LIBZIP_DIRENT_ATTRIBUTES) == 0 && (attributes->valid & LIBZIP_FILE_ATTRIBUTES_HOST_SYSTEM)) {
        de->version_madeby = (de->version_madeby & 0xff) | (libzip_uint16_t)(attributes->host_system << 8);
    }
}


/* _libzip_dirent_torrent_normalize(de);
   Set values suitable for torrentzip.
*/

void libzip_dirent_torrentlibzip_normalize(libzip_dirent_t *de) {
    de->version_madeby = 0;
    de->version_needed = 20; /* 2.0 */
    de->bitflags = 2; /* maximum compression */
    de->comp_method = LIBZIP_CM_DEFLATE;
    de->compression_level = TORRENTLIBZIP_COMPRESSION_FLAGS;
    de->disk_number = 0;
    de->int_attrib = 0;
    de->ext_attrib = 0;

    /* last_mod, extra_fields, and comment are normalized in libzip_dirent_write() directly */

}
