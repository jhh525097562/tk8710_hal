#include "log_udp.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#define LOG_DEFAULT_ADDR "127.0.0.1"
#define LOG_DEFAULT_PORT 5514
#define LOG_DEFAULT_THRESHOLD 256
#define LOG_MAX_CONTENT 4096
#define LOG_CLIENT_BUFFER 65536
#define LOG_SEND_RETRY 3

typedef struct {
    int inited;
    int sockfd;
    struct sockaddr_in remote;
    size_t threshold;
    char pending[LOG_CLIENT_BUFFER];
    size_t pending_len;
    pthread_mutex_t lock;
} log_client_t;

static log_client_t g_client = {
    .inited = 0,
    .sockfd = -1,
    .threshold = LOG_DEFAULT_THRESHOLD,
    .pending_len = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static int format_ts(char *buf, size_t cap) {
    struct timeval tv;
    struct tm tmv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tmv);
    return snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                    tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                    tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long)(tv.tv_usec / 1000));
}

static void get_proc_name(char *buf, size_t cap) {
    const char *override = getenv("LOG_PROC_NAME");
    if (override && *override) {
        size_t n = strlen(override);
        if (n >= cap) n = cap - 1;
        memcpy(buf, override, n);
        buf[n] = '\0';
        return;
    }
    FILE *fp = fopen("/proc/self/comm", "r");
    if (fp) {
        size_t n = fread(buf, 1, cap - 1, fp);
        fclose(fp);
        if (n > 0) {
            size_t i;
            buf[n] = '\0';
            for (i = 0; i < n; ++i) {
                if (buf[i] == '\n' || buf[i] == '\r') {
                    buf[i] = '\0';
                    break;
                }
            }
            if (buf[0] != '\0') {
                return;
            }
        }
    }
    snprintf(buf, cap, "pid-%ld", (long)getpid());
}
static int parse_env_u16(const char *name, uint16_t def_val) {
    const char *s = getenv(name);
    long v;
    char *end = NULL;
    if (s == NULL || *s == '\0') {
        return def_val;
    }
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0 || v > 65535) {
        return def_val;
    }
    return (int)v;
}

static size_t parse_env_size(const char *name, size_t def_val, size_t min_val, size_t max_val) {
    const char *s = getenv(name);
    long long v;
    char *end = NULL;
    if (s == NULL || *s == '\0') {
        return def_val;
    }
    v = strtoll(s, &end, 10);
    if (end == s || *end != '\0') {
        return def_val;
    }
    if ((size_t)v < min_val || (size_t)v > max_val) {
        return def_val;
    }
    return (size_t)v;
}

static int log_client_init_locked(void) {
    const char *addr = getenv("LOG_UDP_ADDR");
    int port = parse_env_u16("LOG_UDP_PORT", LOG_DEFAULT_PORT);
    int rc;

    if (g_client.inited) {
        return LOG_OK;
    }

    g_client.threshold = parse_env_size("LOG_SEND_THRESHOLD", LOG_DEFAULT_THRESHOLD, 64, LOG_CLIENT_BUFFER / 2);
    if (addr == NULL || *addr == '\0') {
        addr = LOG_DEFAULT_ADDR;
    }

    g_client.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_client.sockfd < 0) {
        return LOG_ERR_NETWORK;
    }

    memset(&g_client.remote, 0, sizeof(g_client.remote));
    g_client.remote.sin_family = AF_INET;
    g_client.remote.sin_port = htons((uint16_t)port);
    rc = inet_pton(AF_INET, addr, &g_client.remote.sin_addr);
    if (rc != 1) {
        close(g_client.sockfd);
        g_client.sockfd = -1;
        return LOG_ERR_PARAM;
    }

    g_client.inited = 1;
    return LOG_OK;
}

static int send_with_retry_locked(const char *data, size_t len) {
    int i;
    for (i = 0; i < LOG_SEND_RETRY; ++i) {
        ssize_t n = sendto(g_client.sockfd, data, len, 0, (const struct sockaddr *)&g_client.remote, sizeof(g_client.remote));
        if (n == (ssize_t)len) {
            return LOG_OK;
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            continue;
        }
    }
    return LOG_ERR_NETWORK;
}

int log_flush(void) {
    int rc = LOG_OK;
    pthread_mutex_lock(&g_client.lock);
    if (!g_client.inited || g_client.pending_len == 0) {
        pthread_mutex_unlock(&g_client.lock);
        return LOG_OK;
    }
    rc = send_with_retry_locked(g_client.pending, g_client.pending_len);
    if (rc == LOG_OK) {
        g_client.pending_len = 0;
    }
    pthread_mutex_unlock(&g_client.lock);
    return rc;
}

int log_send(int level, const char *content, time_t timestamp) {
    char line[LOG_MAX_CONTENT + 128];
    char ts[32];
    char proc[64];
    int n;
    int rc;
    size_t line_len;

    (void)timestamp;

    if (content == NULL || level < 0) {
        return LOG_ERR_PARAM;
    }

    if (strlen(content) > LOG_MAX_CONTENT) {
        return LOG_ERR_TOO_LONG;
    }

    if (format_ts(ts, sizeof(ts)) <= 0) {
        return LOG_ERR_NETWORK;
    }
    get_proc_name(proc, sizeof(proc));
    n = snprintf(line, sizeof(line), "%s|%s|%d|%s\n", ts, proc, level, content);
    if (n <= 0 || (size_t)n >= sizeof(line)) {
        return LOG_ERR_TOO_LONG;
    }
    line_len = (size_t)n;

    pthread_mutex_lock(&g_client.lock);
    rc = log_client_init_locked();
    if (rc != LOG_OK) {
        pthread_mutex_unlock(&g_client.lock);
        return rc;
    }

    if (line_len > sizeof(g_client.pending)) {
        pthread_mutex_unlock(&g_client.lock);
        return LOG_ERR_TOO_LONG;
    }

    if (g_client.pending_len + line_len > sizeof(g_client.pending)) {
        rc = send_with_retry_locked(g_client.pending, g_client.pending_len);
        if (rc != LOG_OK) {
            pthread_mutex_unlock(&g_client.lock);
            return rc;
        }
        g_client.pending_len = 0;
    }

    memcpy(g_client.pending + g_client.pending_len, line, line_len);
    g_client.pending_len += line_len;

    if (g_client.pending_len >= g_client.threshold) {
        rc = send_with_retry_locked(g_client.pending, g_client.pending_len);
        if (rc != LOG_OK) {
            pthread_mutex_unlock(&g_client.lock);
            return rc;
        }
        g_client.pending_len = 0;
    }

    pthread_mutex_unlock(&g_client.lock);
    return LOG_OK;
}

void log_close(void) {
    pthread_mutex_lock(&g_client.lock);
    if (g_client.inited && g_client.pending_len > 0) {
        send_with_retry_locked(g_client.pending, g_client.pending_len);
        g_client.pending_len = 0;
    }
    if (g_client.sockfd >= 0) {
        close(g_client.sockfd);
        g_client.sockfd = -1;
    }
    g_client.inited = 0;
    pthread_mutex_unlock(&g_client.lock);
}
