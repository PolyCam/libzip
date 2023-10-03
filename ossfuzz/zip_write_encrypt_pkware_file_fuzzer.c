#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libzip.h>

/**
This fuzzing target takes input data, creates a ZIP archive, load
it to a buffer, adds a file to it with traditional PKWARE
encryption and a specified password, and then closes and removes
the archive.

The purpose of this fuzzer is to test security of ZIP archive
handling and encryption in the libzip by subjecting it to various
inputs, including potentially malicious or malformed data of
different file types.
**/

void
randomize(char *buf, int count) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i;
    srand(time(NULL));
    for (i = 0; i < count; i++) {
        buf[i] = charset[rand() % sizeof(charset)];
    }
}


#ifdef __cplusplus
extern "C"
#endif
int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char path[20 + 7 + 4 + 1], password[21], file[21];
    int error = 0;
    struct zip *archive;

    snprintf(path, sizeof(path), "XXXXXXXXXXXXXXXXXXXX_pkware.zip");
    snprintf(password, sizeof(password), "XXXXXXXXXXXXXXXXXXXX");
    snprintf(file, sizeof(file), "XXXXXXXXXXXXXXXXXXXX");
    randomize(path, 20);
    randomize(password, 20);
    randomize(file, 20);

    if ((archive = libzip_open(path, ZIP_CREATE, &error)) == NULL) {
        return -1;
    }

    struct libzip_source *source = libzip_source_buffer(archive, data, size, 0);
    if (source == NULL) {
        fprintf(stderr, "failed to create source buffer. %s\n", libzip_strerror(archive));
        libzip_discard(archive);
        return -1;
    }

    int index = (int)libzip_file_add(archive, file, source, ZIP_FL_OVERWRITE);
    if (index < 0) {
        fprintf(stderr, "failed to add file to archive: %s\n", libzip_strerror(archive));
        libzip_discard(archive);
        return -1;
    }
    if (libzip_file_set_encryption(archive, index, ZIP_EM_TRAD_PKWARE, password) < 0) {
        fprintf(stderr, "failed to set file encryption: %s\n", libzip_strerror(archive));
        libzip_discard(archive);
        return -1;
    }
    if (libzip_close(archive) < 0) {
        fprintf(stderr, "error closing archive: %s\n", libzip_strerror(archive));
        libzip_discard(archive);
        return -1;
    }
    (void)remove(path);

    return 0;
}
