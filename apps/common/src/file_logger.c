#include "file_logger.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void file_logger_timestamp(char *buf, size_t cap) {
    struct timeval tv;
    struct tm tmv;
    if (cap == 0) return;
    if (gettimeofday(&tv, NULL) != 0) {
        snprintf(buf, cap, "1970-01-01 00:00:00.000");
        return;
    }
#if defined(_WIN32)
    {
        struct tm *tmp = localtime(&tv.tv_sec);
        if (tmp) tmv = *tmp;
        else memset(&tmv, 0, sizeof(tmv));
    }
#else
    localtime_r(&tv.tv_sec, &tmv);
#endif
    snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long)(tv.tv_usec / 1000));
}

static int file_logger_rotate(const char *path, size_t max_size, size_t keep_size) {
    struct stat st;
    FILE *in;
    FILE *out;
    char *buf;
    long start;
    size_t n;
    char tmp_path[256];

    if (stat(path, &st) != 0) return 0;
    if (st.st_size <= (off_t)max_size) return 0;

    if (keep_size == 0 || keep_size >= max_size) keep_size = max_size / 2;
    start = st.st_size > (off_t)keep_size ? (long)(st.st_size - (off_t)keep_size) : 0;

    in = fopen(path, "rb");
    if (!in) return -1;
    if (fseek(in, start, SEEK_SET) != 0) {
        fclose(in);
        return -1;
    }

    buf = (char *)malloc(keep_size + 1);
    if (!buf) {
        fclose(in);
        return -1;
    }
    n = fread(buf, 1, keep_size, in);
    fclose(in);

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)getpid());
    out = fopen(tmp_path, "wb");
    if (!out) {
        free(buf);
        return -1;
    }
    fprintf(out, "--- log truncated, kept last %lu bytes ---\n", (unsigned long)n);
    if (n > 0) fwrite(buf, 1, n, out);
    free(buf);
    if (fclose(out) != 0) {
        unlink(tmp_path);
        return -1;
    }
#if defined(_WIN32)
    unlink(path);
#endif
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

static void file_logger_ensure_parent(const char *path) {
    char dir[256];
    char *slash;

    if (!path) return;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return;
    if ((size_t)(slash - path) >= sizeof(dir)) return;
    memcpy(dir, path, (size_t)(slash - path));
    dir[slash - path] = '\0';
#if defined(_WIN32)
    mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static int file_logger_vwrite_limit(const char *path, size_t max_size, size_t keep_size,
                                    const char *level, const char *fmt, va_list ap) {
    FILE *fp;
    char ts[64];
    char fast_buf[4096];
    char *msg = fast_buf;
    va_list ap_copy;
    int n;
    size_t msg_len;
    int rc = 0;

    if (!path || !fmt || max_size == 0) return -1;
    if (!level) level = "INFO";

    va_copy(ap_copy, ap);
    n = vsnprintf(fast_buf, sizeof(fast_buf), fmt, ap);
    if (n < 0) {
        va_end(ap_copy);
        return -1;
    }
    if ((size_t)n >= sizeof(fast_buf)) {
        msg = (char *)malloc((size_t)n + 1);
        if (!msg) {
            va_end(ap_copy);
            return -1;
        }
        if (vsnprintf(msg, (size_t)n + 1, fmt, ap_copy) < 0) {
            free(msg);
            va_end(ap_copy);
            return -1;
        }
    }
    va_end(ap_copy);

    msg_len = strlen(msg);
    while (msg_len > 0 && (msg[msg_len - 1] == '\n' || msg[msg_len - 1] == '\r')) {
        msg[--msg_len] = '\0';
    }

    file_logger_ensure_parent(path);
    file_logger_rotate(path, max_size, keep_size);

    fp = fopen(path, "ab");
    if (!fp) {
        if (msg != fast_buf) free(msg);
        return -1;
    }
    file_logger_timestamp(ts, sizeof(ts));
    if (fprintf(fp, "%s [%s] %s\n", ts, level, msg) < 0) rc = -1;
    if (fclose(fp) != 0) rc = -1;
    if (rc == 0) file_logger_rotate(path, max_size, keep_size);
    if (msg != fast_buf) free(msg);
    return rc;
}

static int file_logger_vprintf(const char *path, const char *level, const char *fmt, va_list ap) {
    return file_logger_vwrite_limit(path, FILE_LOGGER_DEFAULT_MAX_SIZE,
                                    FILE_LOGGER_DEFAULT_KEEP_SIZE, level, fmt, ap);
}

int file_logger_write_limit(const char *path, size_t max_size, size_t keep_size,
                            const char *level, const char *fmt, ...) {
    va_list ap;
    int rc;
    va_start(ap, fmt);
    rc = file_logger_vwrite_limit(path, max_size, keep_size, level, fmt, ap);
    va_end(ap);
    return rc;
}

int file_logger_write(const char *path, const char *level, const char *fmt, ...) {
    va_list ap;
    int rc;
    va_start(ap, fmt);
    rc = file_logger_vwrite_limit(path, FILE_LOGGER_DEFAULT_MAX_SIZE,
                                  FILE_LOGGER_DEFAULT_KEEP_SIZE, level, fmt, ap);
    va_end(ap);
    return rc;
}

int file_logger_printf(const char *path, const char *level, const char *fmt, ...) {
    va_list ap;
    va_list ap_copy;
    int rc;
    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    vprintf(fmt, ap);
    rc = file_logger_vprintf(path, level, fmt, ap_copy);
    va_end(ap_copy);
    va_end(ap);
    return rc;
}

int file_logger_fprintf(const char *path, FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_list ap_copy;
    int rc;

    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    rc = vfprintf(stream, fmt, ap);
    file_logger_vprintf(path, stream == stderr ? "ERROR" : "INFO", fmt, ap_copy);
    va_end(ap_copy);
    va_end(ap);
    return rc;
}

void file_logger_perror(const char *path, const char *prefix) {
    int saved_errno = errno;
    errno = saved_errno;
    perror(prefix);
    file_logger_write(path, "ERROR", "%s: %s", prefix ? prefix : "error", strerror(saved_errno));
}
