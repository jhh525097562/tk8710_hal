#include "log_udp.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>

typedef struct {
    int port;
    char buf[65536];
    ssize_t n;
} recv_ctx_t;

static void sleep_us(unsigned int usec) {
    struct timespec ts;
    ts.tv_sec = (time_t)(usec / 1000000u);
    ts.tv_nsec = (long)((usec % 1000000u) * 1000u);
    while (nanosleep(&ts, &ts) != 0) {
        if (errno != EINTR) {
            break;
        }
    }
}

typedef struct {
    int port;
    int expect;
    int unique;
    int total;
    int seen_cap;
    unsigned char *seen;
} recv_many_ctx_t;

static void *recv_thread(void *arg) {
    recv_ctx_t *ctx = (recv_ctx_t *)arg;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a;
    socklen_t alen = sizeof(a);
    if (s < 0) {
        ctx->n = -1;
        return NULL;
    }
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)ctx->port);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        ctx->n = -2;
        close(s);
        return NULL;
    }
    ctx->n = recvfrom(s, ctx->buf, sizeof(ctx->buf) - 1, 0, (struct sockaddr *)&a, &alen);
    if (ctx->n > 0) {
        ctx->buf[ctx->n] = '\0';
    }
    close(s);
    return NULL;
}

static int parse_id(const char *buf) {
    const char *p = strstr(buf, "id=");
    if (!p) return -1;
    return atoi(p + 3);
}

static void *recv_many_thread(void *arg) {
    recv_many_ctx_t *ctx = (recv_many_ctx_t *)arg;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a;
    socklen_t alen = sizeof(a);
    struct timeval tv;
    if (s < 0) return NULL;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)ctx->port);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(s);
        return NULL;
    }
    while (ctx->unique < ctx->expect) {
        char buf[8192];
        ssize_t n = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&a, &alen);
        if (n <= 0) break;
        buf[n] = '\0';
        ctx->total++;
        int id = parse_id(buf);
        if (id >= 0 && id < ctx->seen_cap && ctx->seen[id] == 0) {
            ctx->seen[id] = 1;
            ctx->unique++;
        }
    }
    close(s);
    return NULL;
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static int ts_format_valid(const char *s) {
    if (!s) return 0;
    if (strlen(s) < 23) return 0;
    return is_digit(s[0]) && is_digit(s[1]) && is_digit(s[2]) && is_digit(s[3]) &&
           s[4] == '-' &&
           is_digit(s[5]) && is_digit(s[6]) &&
           s[7] == '-' &&
           is_digit(s[8]) && is_digit(s[9]) &&
           s[10] == ' ' &&
           is_digit(s[11]) && is_digit(s[12]) &&
           s[13] == ':' &&
           is_digit(s[14]) && is_digit(s[15]) &&
           s[16] == ':' &&
           is_digit(s[17]) && is_digit(s[18]) &&
           s[19] == '.' &&
           is_digit(s[20]) && is_digit(s[21]) && is_digit(s[22]);
}

static void get_self_comm(char *buf, size_t cap) {
    FILE *fp = fopen("/proc/self/comm", "r");
    if (fp) {
        size_t n = fread(buf, 1, cap - 1, fp);
        fclose(fp);
        if (n > 0) {
            buf[n] = '\0';
            for (size_t i = 0; i < n; ++i) {
                if (buf[i] == '\n' || buf[i] == '\r') {
                    buf[i] = '\0';
                    break;
                }
            }
            return;
        }
    }
    snprintf(buf, cap, "pid-%ld", (long)getpid());
}

static void test_invalid_params(void) {
    int rc = log_send(1, NULL, time(NULL));
    assert(rc == LOG_ERR_PARAM);
    rc = log_send(-1, "x", time(NULL));
    assert(rc == LOG_ERR_PARAM);
}

static void test_too_long(void) {
    char *big = (char *)malloc(5000);
    memset(big, 'A', 4999);
    big[4999] = '\0';
    int rc = log_send(1, big, time(NULL));
    assert(rc == LOG_ERR_TOO_LONG);
    free(big);
}

static void test_normal_send(void) {
    setenv("LOG_UDP_ADDR", "127.0.0.1", 1);
    setenv("LOG_UDP_PORT", "5514", 1);
    setenv("LOG_SEND_THRESHOLD", "64", 1);
    recv_ctx_t ctx;
    pthread_t th;
    ctx.port = 5514;
    ctx.n = 0;
    pthread_create(&th, NULL, recv_thread, &ctx);
    sleep_us(1000);
    int rc = log_send(2, "hello", time(NULL));
    assert(rc == LOG_OK);
    rc = log_flush();
    assert(rc == LOG_OK);
    pthread_join(th, NULL);
    assert(ctx.n > 0);
    char *p1 = strchr(ctx.buf, '|');
    assert(p1 != NULL);
    char ts[64];
    size_t len = (size_t)(p1 - ctx.buf);
    assert(len < sizeof(ts));
    memcpy(ts, ctx.buf, len);
    ts[len] = '\0';
    assert(ts_format_valid(ts));
    char *p2 = strchr(p1 + 1, '|');
    assert(p2 != NULL);
    char proc[64];
    size_t plen = (size_t)(p2 - (p1 + 1));
    assert(plen < sizeof(proc));
    memcpy(proc, p1 + 1, plen);
    proc[plen] = '\0';
    char selfp[64];
    get_self_comm(selfp, sizeof(selfp));
    assert(strcmp(proc, selfp) == 0);
    log_close();
}

static void test_network_error(void) {
    setenv("LOG_UDP_ADDR", "300.1.1.1", 1);
    setenv("LOG_UDP_PORT", "5514", 1);
    log_close();
    int rc = log_send(1, "x", time(NULL));
    assert(rc == LOG_ERR_PARAM);
    log_close();
}

typedef struct {
    int base;
    int count;
} send_args_t;

static void *send_worker(void *arg) {
    send_args_t *sa = (send_args_t *)arg;
    for (int i = 0; i < sa->count; ++i) {
        char msg[128];
        int id = sa->base + i;
        snprintf(msg, sizeof(msg), "id=%d payload=test", id);
        log_send(1, msg, 0);
        if ((i & 15) == 0) log_flush();
    }
    log_flush();
    return NULL;
}

static void test_concurrent_integrity(void) {
    int port = 5514;
    setenv("LOG_UDP_ADDR", "127.0.0.1", 1);
    setenv("LOG_UDP_PORT", "5514", 1);
    setenv("LOG_SEND_THRESHOLD", "64", 1);
    int threads = 4;
    int per = 50;
    int total = threads * per;
    unsigned char *seen = (unsigned char *)calloc((size_t)total, 1);
    recv_many_ctx_t rctx;
    pthread_t rth;
    rctx.port = port;
    rctx.expect = total;
    rctx.unique = 0;
    rctx.total = 0;
    rctx.seen_cap = total;
    rctx.seen = seen;
    pthread_create(&rth, NULL, recv_many_thread, &rctx);
    sleep_us(1000);
    pthread_t th[8];
    send_args_t sa[8];
    for (int t = 0; t < threads; ++t) {
        sa[t].base = t * per;
        sa[t].count = per;
        pthread_create(&th[t], NULL, send_worker, &sa[t]);
    }
    for (int t = 0; t < threads; ++t) pthread_join(th[t], NULL);
    pthread_join(rth, NULL);
    int uniq = 0;
    for (int i = 0; i < total; ++i) uniq += seen[i] ? 1 : 0;
    free(seen);
    assert(rctx.unique == total);
    assert(rctx.total == total);
    assert(uniq == total);
}

int main(void) {
    test_invalid_params();
    test_too_long();
    test_normal_send();
    test_network_error();
    test_concurrent_integrity();
    printf("test_log_udp passed\n");
    return 0;
}
