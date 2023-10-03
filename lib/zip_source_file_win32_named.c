/*
  libzip_source_file_win32_named.c -- source for Windows file opened by name
  Copyright (C) 1999-2020 Dieter Baron and Thomas Klausner

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

#include "zip_source_file_win32.h"

static libzip_int64_t _libzip_win32_named_op_commit_write(libzip_source_file_context_t *ctx);
static libzip_int64_t _libzip_win32_named_op_create_temp_output(libzip_source_file_context_t *ctx);
static bool _libzip_win32_named_op_open(libzip_source_file_context_t *ctx);
static libzip_int64_t _libzip_win32_named_op_remove(libzip_source_file_context_t *ctx);
static void _libzip_win32_named_op_rollback_write(libzip_source_file_context_t *ctx);
static bool _libzip_win32_named_op_stat(libzip_source_file_context_t *ctx, libzip_source_file_stat_t *st);
static char *_libzip_win32_named_op_string_duplicate(libzip_source_file_context_t *ctx, const char *string);
static libzip_int64_t _libzip_win32_named_op_write(libzip_source_file_context_t *ctx, const void *data, libzip_uint64_t len);

static HANDLE win32_named_open(libzip_source_file_context_t *ctx, const char *name, bool temporary, PSECURITY_ATTRIBUTES security_attributes);

/* clang-format off */
libzip_source_file_operations_t _libzip_source_file_win32_named_ops = {
    _libzip_win32_op_close,
    _libzip_win32_named_op_commit_write,
    _libzip_win32_named_op_create_temp_output,
    NULL,
    _libzip_win32_named_op_open,
    _libzip_win32_op_read,
    _libzip_win32_named_op_remove,
    _libzip_win32_named_op_rollback_write,
    _libzip_win32_op_seek,
    _libzip_win32_named_op_stat,
    _libzip_win32_named_op_string_duplicate,
    _libzip_win32_op_tell,
    _libzip_win32_named_op_write
};
/* clang-format on */

static libzip_int64_t
_libzip_win32_named_op_commit_write(libzip_source_file_context_t *ctx) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;
    DWORD attributes;

    if (!CloseHandle((HANDLE)ctx->fout)) {
        libzip_error_set(&ctx->error, ZIP_ER_WRITE, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    attributes = file_ops->get_file_attributes(ctx->tmpname);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        libzip_error_set(&ctx->error, ZIP_ER_RENAME, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    if (attributes & FILE_ATTRIBUTE_TEMPORARY) {
        if (!file_ops->set_file_attributes(ctx->tmpname, attributes & ~FILE_ATTRIBUTE_TEMPORARY)) {
            libzip_error_set(&ctx->error, ZIP_ER_RENAME, _libzip_win32_error_to_errno(GetLastError()));
            return -1;
        }
    }

    if (!file_ops->move_file(ctx->tmpname, ctx->fname, MOVEFILE_REPLACE_EXISTING)) {
        libzip_error_set(&ctx->error, ZIP_ER_RENAME, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    return 0;
}

static libzip_int64_t
_libzip_win32_named_op_create_temp_output(libzip_source_file_context_t *ctx) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    libzip_uint32_t value, i;
    HANDLE th = INVALID_HANDLE_VALUE;
    PSECURITY_DESCRIPTOR psd = NULL;
    PSECURITY_ATTRIBUTES psa = NULL;
    SECURITY_ATTRIBUTES sa;
    SECURITY_INFORMATION si;
    DWORD success;
    PACL dacl = NULL;
    char *tempname = NULL;
    size_t tempname_size = 0;

    if ((HANDLE)ctx->f != INVALID_HANDLE_VALUE && GetFileType((HANDLE)ctx->f) == FILE_TYPE_DISK) {
        si = DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION;
        success = GetSecurityInfo((HANDLE)ctx->f, SE_FILE_OBJECT, si, NULL, NULL, &dacl, NULL, &psd);
        if (success == ERROR_SUCCESS) {
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = FALSE;
            sa.lpSecurityDescriptor = psd;
            psa = &sa;
        }
    }

#ifndef MS_UWP
    value = GetTickCount();
#else
    value = (libzip_uint32_t)(GetTickCount64() & 0xffffffff);
#endif

    if ((tempname = file_ops->allocate_tempname(ctx->fname, 10, &tempname_size)) == NULL) {
        libzip_error_set(&ctx->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    for (i = 0; i < 1024 && th == INVALID_HANDLE_VALUE; i++) {
        file_ops->make_tempname(tempname, tempname_size, ctx->fname, value + i);

        th = win32_named_open(ctx, tempname, true, psa);
        if (th == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS)
            break;
    }

    if (th == INVALID_HANDLE_VALUE) {
        free(tempname);
        LocalFree(psd);
        libzip_error_set(&ctx->error, ZIP_ER_TMPOPEN, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    LocalFree(psd);
    ctx->fout = th;
    ctx->tmpname = tempname;

    return 0;
}


static bool
_libzip_win32_named_op_open(libzip_source_file_context_t *ctx) {
    HANDLE h = win32_named_open(ctx, ctx->fname, false, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    ctx->f = h;
    return true;
}


static libzip_int64_t
_libzip_win32_named_op_remove(libzip_source_file_context_t *ctx) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    if (!file_ops->delete_file(ctx->fname)) {
        libzip_error_set(&ctx->error, ZIP_ER_REMOVE, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    return 0;
}


static void
_libzip_win32_named_op_rollback_write(libzip_source_file_context_t *ctx) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    if (ctx->fout) {
        CloseHandle((HANDLE)ctx->fout);
    }
    file_ops->delete_file(ctx->tmpname);
}


static bool
_libzip_win32_named_op_stat(libzip_source_file_context_t *ctx, libzip_source_file_stat_t *st) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    WIN32_FILE_ATTRIBUTE_DATA file_attributes;

    if (!file_ops->get_file_attributes_ex(ctx->fname, GetFileExInfoStandard, &file_attributes)) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            st->exists = false;
            return true;
        }
        libzip_error_set(&ctx->error, ZIP_ER_READ, _libzip_win32_error_to_errno(error));
        return false;
    }

    st->exists = true;
    st->regular_file = false;

    if (file_attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES) {
        if ((file_attributes.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT)) == 0) {
            st->regular_file = true;
        }
    }

    if (!_libzip_filetime_to_time_t(file_attributes.ftLastWriteTime, &st->mtime)) {
        libzip_error_set(&ctx->error, ZIP_ER_READ, ERANGE);
        return false;
    }
    st->size = ((libzip_uint64_t)file_attributes.nFileSizeHigh << 32) | file_attributes.nFileSizeLow;

    /* TODO: fill in ctx->attributes */

    return true;
}


static char *
_libzip_win32_named_op_string_duplicate(libzip_source_file_context_t *ctx, const char *string) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    return file_ops->string_duplicate(string);
}


static libzip_int64_t
_libzip_win32_named_op_write(libzip_source_file_context_t *ctx, const void *data, libzip_uint64_t len) {
    DWORD ret;
    if (!WriteFile((HANDLE)ctx->fout, data, (DWORD)len, &ret, NULL) || ret != len) {
        libzip_error_set(&ctx->error, ZIP_ER_WRITE, _libzip_win32_error_to_errno(GetLastError()));
        return -1;
    }

    return (libzip_int64_t)ret;
}


static HANDLE
win32_named_open(libzip_source_file_context_t *ctx, const char *name, bool temporary, PSECURITY_ATTRIBUTES security_attributes) {
    libzip_win32_file_operations_t *file_ops = (libzip_win32_file_operations_t *)ctx->ops_userdata;

    DWORD access = GENERIC_READ;
    DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD creation_disposition = OPEN_EXISTING;
    DWORD file_attributes = FILE_ATTRIBUTE_NORMAL;
    HANDLE h;

    if (temporary) {
        access = GENERIC_READ | GENERIC_WRITE;
        share_mode = FILE_SHARE_READ;
        creation_disposition = CREATE_NEW;
        file_attributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY;
    }

    h = file_ops->create_file(name, access, share_mode, security_attributes, creation_disposition, file_attributes, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        libzip_error_set(&ctx->error, ZIP_ER_OPEN, _libzip_win32_error_to_errno(GetLastError()));
    }

    return h;
}
