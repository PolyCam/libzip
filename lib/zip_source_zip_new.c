/*
  libzip_source_libzip_new.c -- prepare data structures for libzip_fopen/libzip_source_zip
  Copyright (C) 2012-2021 Dieter Baron and Thomas Klausner

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

#include "zipint.h"

static void _libzip_file_attributes_from_dirent(libzip_file_attributes_t *attributes, libzip_dirent_t *de);

LIBZIP_EXTERN libzip_source_t *libzip_source_libzip_file(libzip_t* za, libzip_t *srcza, libzip_uint64_t srcidx, libzip_flags_t flags, libzip_uint64_t start, libzip_int64_t len, const char *password) {
    return libzip_source_libzip_file_create(srcza, srcidx, flags, start, len, password, &za->error);
}


LIBZIP_EXTERN libzip_source_t *libzip_source_libzip_file_create(libzip_t *srcza, libzip_uint64_t srcidx, libzip_flags_t flags, libzip_uint64_t start, libzip_int64_t len, const char *password, libzip_error_t *error) {
    /* TODO: We need to make sure that the returned source is invalidated when srcza is closed. */
    libzip_source_t *src, *s2;
    libzip_stat_t st;
    libzip_file_attributes_t attributes;
    libzip_dirent_t *de;
    bool partial_data, needs_crc, encrypted, needs_decrypt, compressed, needs_decompress, changed_data, have_size, have_comp_size;
    libzip_flags_t stat_flags;
    libzip_int64_t data_len;
    bool take_ownership = false;
    bool empty_data = false;

    if (srcza == NULL || srcidx >= srcza->nentry || len < -1) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if (flags & LIBZIP_FL_ENCRYPTED) {
        flags |= LIBZIP_FL_COMPRESSED;
    }

    changed_data = false;
    if ((flags & LIBZIP_FL_UNCHANGED) == 0) {
        libzip_entry_t *entry = srcza->entry + srcidx;
        if (LIBZIP_ENTRY_DATA_CHANGED(entry)) {
            if ((flags & LIBZIP_FL_COMPRESSED) || !libzip_source_supports_reopen(entry->source)) {
                libzip_error_set(error, LIBZIP_ER_CHANGED, 0);
                return NULL;
            }

            changed_data = true;
        }
        else if (entry->deleted) {
            libzip_error_set(error, LIBZIP_ER_CHANGED, 0);
            return NULL;
        }
    }

    stat_flags = flags;
    if (!changed_data) {
        stat_flags |= LIBZIP_FL_UNCHANGED;
    }

    if (libzip_stat_index(srcza, srcidx, stat_flags, &st) < 0) {
        libzip_error_set(error, LIBZIP_ER_INTERNAL, 0);
        return NULL;
    }

    if ((start > 0 || len >= 0) && (flags & LIBZIP_FL_COMPRESSED)) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    have_size = (st.valid & LIBZIP_STAT_SIZE) != 0;
    /* overflow or past end of file */
    if (len >= 0 && ((start > 0 && start + len < start) || (have_size && start + len > st.size))) {
        libzip_error_set(error, LIBZIP_ER_INVAL, 0);
        return NULL;
    }

    if (len == -1) {
        if (have_size) {
            if (st.size - start > LIBZIP_INT64_MAX) {
                libzip_error_set(error, LIBZIP_ER_INVAL, 0);
                return NULL;
            }
            data_len = (libzip_int64_t)(st.size - start);
        }
        else {
            data_len = -1;
        }
    }
    else {
           data_len = len;
    }

    if (have_size) {
        partial_data = (libzip_uint64_t)(data_len) < st.size;
    }
    else {
        partial_data = true;
    }
    encrypted = (st.valid & LIBZIP_STAT_ENCRYPTION_METHOD) && (st.encryption_method != LIBZIP_EM_NONE);
    needs_decrypt = ((flags & LIBZIP_FL_ENCRYPTED) == 0) && encrypted;
    compressed = (st.valid & LIBZIP_STAT_COMP_METHOD) && (st.comp_method != LIBZIP_CM_STORE);
    needs_decompress = ((flags & LIBZIP_FL_COMPRESSED) == 0) && compressed;
    /* when reading the whole file, check for CRC errors */
    needs_crc = ((flags & LIBZIP_FL_COMPRESSED) == 0 || !compressed) && !partial_data && (st.valid & LIBZIP_STAT_CRC) != 0;

    if (needs_decrypt) {
        if (password == NULL) {
            password = srcza->default_password;
        }
        if (password == NULL) {
            libzip_error_set(error, LIBZIP_ER_NOPASSWD, 0);
            return NULL;
        }
    }

    if ((de = _libzip_get_dirent(srcza, srcidx, flags, error)) == NULL) {
        return NULL;
    }
    _libzip_file_attributes_from_dirent(&attributes, de);

    have_comp_size = (st.valid & LIBZIP_STAT_COMP_SIZE) != 0;
    if (needs_decrypt || needs_decompress) {
        empty_data = (have_comp_size && st.comp_size == 0);
    }
    else {
        empty_data = (have_size && st.size == 0);
    }
    if (empty_data) {
        src = libzip_source_buffer_with_attributes_create(NULL, 0, 0, &attributes, error);
    }
    else {
        src = NULL;
    }


    /* If we created source buffer above, we want the window source to take ownership of it. */
    take_ownership = src != NULL;
    /* if we created a buffer source above, then treat it as if
       reading the changed data - that way we don't need add another
       special case to the code below that wraps it in the window
       source */
    changed_data = changed_data || (src != NULL);

    if (partial_data && !needs_decrypt && !needs_decompress) {
        struct libzip_stat st2;
        libzip_t *source_archive;
        libzip_uint64_t source_index;

        if (changed_data) {
            if (src == NULL) {
                src = srcza->entry[srcidx].source;
            }
            source_archive = NULL;
            source_index = 0;
        }
        else {
            src = srcza->src;
            source_archive = srcza;
            source_index = srcidx;
        }

        st2.comp_method = LIBZIP_CM_STORE;
        st2.valid = LIBZIP_STAT_COMP_METHOD;
        if (data_len >= 0) {
            st2.size = (libzip_uint64_t)data_len;
            st2.comp_size = (libzip_uint64_t)data_len;
            st2.valid |= LIBZIP_STAT_SIZE | LIBZIP_STAT_COMP_SIZE;
        }
        if (st.valid & LIBZIP_STAT_MTIME) {
            st2.mtime = st.mtime;
            st2.valid |= LIBZIP_STAT_MTIME;
        }

        if ((src = _libzip_source_window_new(src, start, data_len, &st2, LIBZIP_STAT_NAME, &attributes, source_archive, source_index, take_ownership, error)) == NULL) {
            return NULL;
        }
    }
    /* here we restrict src to file data, so no point in doing it for
       source that already represents only the file data */
    else if (!changed_data) {
        /* this branch is executed only for archive sources; we know
           that stat data come from the archive too, so it's safe to
           assume that st has a comp_size specified */
        if (st.comp_size > LIBZIP_INT64_MAX) {
            libzip_error_set(error, LIBZIP_ER_INVAL, 0);
            return NULL;
        }
        /* despite the fact that we want the whole data file, we still
           wrap the source into a window source to add st and
           attributes and to have a source that positions the read
           offset properly before each read for multiple libzip_file_t
           referring to the same underlying source */
        if ((src =  _libzip_source_window_new(srcza->src, 0, (libzip_int64_t)st.comp_size, &st, LIBZIP_STAT_NAME, &attributes, srcza, srcidx, take_ownership, error)) == NULL) {
            return NULL;
        }
    }
    else {
        /* this branch gets executed when reading the whole changed
           data file or when "reading" from a zero-sized source buffer
           that we created above */
        if (src == NULL) {
            src = srcza->entry[srcidx].source;
        }
        /* despite the fact that we want the whole data file, we still
           wrap the source into a window source to add st and
           attributes and to have a source that positions the read
           offset properly before each read for multiple libzip_file_t
           referring to the same underlying source */
        if ((src = _libzip_source_window_new(src, 0, data_len, &st, LIBZIP_STAT_NAME, &attributes, NULL, 0, take_ownership, error)) == NULL) {
            return NULL;
        }
    }

    /* In all cases, src is a window source and therefore is owned by this function. */

    if (_libzip_source_set_source_archive(src, srcza) < 0) {
        libzip_source_free(src);
        return NULL;
    }

    /* creating a layered source calls libzip_keep() on the lower layer, so we free it */

    if (needs_decrypt) {
        libzip_encryption_implementation enc_impl;

        if ((enc_impl = _libzip_get_encryption_implementation(st.encryption_method, LIBZIP_CODEC_DECODE)) == NULL) {
            libzip_source_free(src);
            libzip_error_set(error, LIBZIP_ER_ENCRNOTSUPP, 0);
            return NULL;
        }

        s2 = enc_impl(srcza, src, st.encryption_method, 0, password);
        if (s2 == NULL) {
            libzip_source_free(src);
            return NULL;
        }

        src = s2;
    }
    if (needs_decompress) {
        s2 = libzip_source_decompress(srcza, src, st.comp_method);
        if (s2 == NULL) {
            libzip_source_free(src);
            return NULL;
        }
        src = s2;
    }
    if (needs_crc) {
        s2 = libzip_source_crc_create(src, 1, error);
        if (s2 == NULL) {
            libzip_source_free(src);
            return NULL;
        }
        src = s2;
    }

    if (partial_data && (needs_decrypt || needs_decompress)) {
        libzip_stat_t st2;
        libzip_stat_init(&st2);
        if (data_len >= 0) {
            st2.valid = LIBZIP_STAT_SIZE;
            st2.size = (libzip_uint64_t)data_len;
        }
        s2 = _libzip_source_window_new(src, start, data_len, &st2, LIBZIP_STAT_NAME, NULL, NULL, 0, true, error);
        if (s2 == NULL) {
            libzip_source_free(src);
            return NULL;
        }
        src = s2;
    }

    return src;
}

static void
_libzip_file_attributes_from_dirent(libzip_file_attributes_t *attributes, libzip_dirent_t *de) {
    libzip_file_attributes_init(attributes);
    attributes->valid = LIBZIP_FILE_ATTRIBUTES_ASCII | LIBZIP_FILE_ATTRIBUTES_HOST_SYSTEM | LIBZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES | LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS;
    attributes->ascii = de->int_attrib & 1;
    attributes->host_system = de->version_madeby >> 8;
    attributes->external_file_attributes = de->ext_attrib;
    attributes->general_purpose_bit_flags = de->bitflags;
    attributes->general_purpose_bit_mask = LIBZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS_ALLOWED_MASK;
}
