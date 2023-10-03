#include <stdint.h>
#include <libzip.h>

#include "zip_read_fuzzer_common.h"

#ifdef __cplusplus
extern "C"
#endif
int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    libzip_source_t *src;
    libzip_error_t error;
    libzip_t *za;

    libzip_error_init(&error);

    if ((src = libzip_source_buffer_create(data, size, 0, &error)) == NULL) {
        libzip_error_fini(&error);
        return 0;
    }

    za = libzip_open_from_source(src, 0, &error);

    fuzzer_read(za, &error, "secretpassword");

    if (za == NULL) {
        libzip_source_free(src);
    }

    return 0;
}
