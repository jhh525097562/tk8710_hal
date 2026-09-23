#ifndef FILE_LOGGER_H
#define FILE_LOGGER_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_LOGGER_DEFAULT_MAX_SIZE (1024 * 1024)
#define FILE_LOGGER_DEFAULT_KEEP_SIZE (768 * 1024)

int file_logger_write_limit(const char *path, size_t max_size, size_t keep_size,
                            const char *level, const char *fmt, ...);
int file_logger_write(const char *path, const char *level, const char *fmt, ...);
int file_logger_printf(const char *path, const char *level, const char *fmt, ...);
int file_logger_fprintf(const char *path, FILE *stream, const char *fmt, ...);
void file_logger_perror(const char *path, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif
