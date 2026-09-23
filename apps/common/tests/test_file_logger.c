#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_logger.h"

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;
    char *buf;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    fread(buf, 1, (size_t)size, fp);
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

int main(void) {
    const char *path = "app/common/tests/test_file_logger.log";
    char *content;
    int i;

    unlink(path);
    for (i = 0; i < 40; i++) {
        file_logger_write_limit(path, 256, 128, "INFO", "old-line-%02d abcdefghijklmnopqrstuvwxyz", i);
    }
    file_logger_write_limit(path, 256, 128, "ERROR", "final-line");

    if (file_size(path) <= 0 || file_size(path) > 256) {
        fprintf(stderr, "unexpected log size: %ld\n", file_size(path));
        return 1;
    }

    content = read_file(path);
    if (!content) {
        fprintf(stderr, "failed to read log\n");
        return 1;
    }
    if (strstr(content, "final-line") == NULL) {
        fprintf(stderr, "missing final line\n");
        free(content);
        return 1;
    }
    if (strstr(content, "log truncated") == NULL) {
        fprintf(stderr, "missing truncation marker\n");
        free(content);
        return 1;
    }

    free(content);
    unlink(path);
    return 0;
}
