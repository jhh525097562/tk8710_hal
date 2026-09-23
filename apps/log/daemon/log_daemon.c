#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "file_logger.h"

#define DAEMON_DEFAULT_PORT 5514
#define DAEMON_DEFAULT_RING (256 * 1024)
#define DAEMON_DEFAULT_FLUSH_MS 500
#define DAEMON_DEFAULT_MAX_FILE (10 * 1024 * 1024)
#define DAEMON_DEFAULT_MAX_FILES 0
#define DAEMON_DEFAULT_OUTPUT "./logs"
#define DAEMON_PACKET_MAX 65507
#define LOG_DAEMON_FILE_LOG "/var/log/log_daemon.log"

typedef struct {
    char *buf;
    size_t cap;
    size_t head;
    size_t tail;
    size_t size;
} ring_buffer_t;

typedef struct {
    FILE *fp;
    char out_dir[512];
    char current_path[1024];
    size_t file_size;
    size_t max_file_size;
    int max_files;
} writer_t;

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int ring_init(ring_buffer_t *r, size_t cap) {
    r->buf = (char *)malloc(cap);
    if (r->buf == NULL) {
        return -1;
    }
    r->cap = cap;
    r->head = 0;
    r->tail = 0;
    r->size = 0;
    return 0;
}

static void ring_free(ring_buffer_t *r) {
    free(r->buf);
    r->buf = NULL;
    r->cap = 0;
    r->head = r->tail = r->size = 0;
}

static size_t ring_write(ring_buffer_t *r, const char *data, size_t len) {
    size_t can_write = (len <= (r->cap - r->size)) ? len : (r->cap - r->size);
    size_t first = (can_write <= (r->cap - r->tail)) ? can_write : (r->cap - r->tail);
    size_t second = can_write - first;
    if (can_write == 0) {
        return 0;
    }
    memcpy(r->buf + r->tail, data, first);
    if (second > 0) {
        memcpy(r->buf, data + first, second);
    }
    r->tail = (r->tail + can_write) % r->cap;
    r->size += can_write;
    return can_write;
}

static size_t ring_read(ring_buffer_t *r, char *out, size_t len) {
    size_t can_read = (len <= r->size) ? len : r->size;
    size_t first = (can_read <= (r->cap - r->head)) ? can_read : (r->cap - r->head);
    size_t second = can_read - first;
    if (can_read == 0) {
        return 0;
    }
    memcpy(out, r->buf + r->head, first);
    if (second > 0) {
        memcpy(out + first, r->buf, second);
    }
    r->head = (r->head + can_read) % r->cap;
    r->size -= can_read;
    return can_read;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }
    return mkdir(path, 0755);
}

typedef struct {
    char path[1024];
    time_t mtime;
} file_item_t;

static int has_prefix(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int has_suffix(const char *s, const char *suffix) {
    size_t sl = strlen(s);
    size_t tl = strlen(suffix);
    if (sl < tl) return 0;
    return memcmp(s + (sl - tl), suffix, tl) == 0;
}

static int file_item_cmp(const void *a, const void *b) {
    const file_item_t *fa = (const file_item_t *)a;
    const file_item_t *fb = (const file_item_t *)b;
    if (fa->mtime < fb->mtime) return -1;
    if (fa->mtime > fb->mtime) return 1;
    return strcmp(fa->path, fb->path);
}

static int prune_old_files(writer_t *w) {
    if (w->max_files <= 0) {
        return 0;
    }

    DIR *d = opendir(w->out_dir);
    if (d == NULL) {
        return -1;
    }

    file_item_t *items = NULL;
    size_t count = 0;
    size_t cap = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!has_prefix(ent->d_name, "log_") || !has_suffix(ent->d_name, ".dat")) {
            continue;
        }
        char path[1024];
        struct stat st;
        if (snprintf(path, sizeof(path), "%s/%s", w->out_dir, ent->d_name) >= (int)sizeof(path)) {
            continue;
        }
        if (stat(path, &st) != 0) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        if (cap == count) {
            size_t new_cap = (cap == 0) ? 32 : cap * 2;
            file_item_t *new_items = (file_item_t *)realloc(items, new_cap * sizeof(*items));
            if (new_items == NULL) {
                free(items);
                closedir(d);
                return -1;
            }
            items = new_items;
            cap = new_cap;
        }
        memset(&items[count], 0, sizeof(items[count]));
        if (snprintf(items[count].path, sizeof(items[count].path), "%s", path) >= (int)sizeof(items[count].path)) {
            continue;
        }
        items[count].mtime = st.st_mtime;
        count++;
    }
    closedir(d);

    if (count <= (size_t)w->max_files) {
        free(items);
        return 0;
    }

    qsort(items, count, sizeof(*items), file_item_cmp);

    size_t orig_count = count;
    size_t i = 0;
    while (count > (size_t)w->max_files && i < orig_count) {
        if (strcmp(items[i].path, w->current_path) != 0) {
            if (unlink(items[i].path) == 0) {
                count--;
            }
        }
        i++;
    }

    free(items);
    return 0;
}

static int writer_build_new_path(writer_t *w, char *out, size_t out_cap) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    for (int i = 0; i < 10000; ++i) {
        if (i == 0) {
            if (snprintf(out, out_cap, "%s/log_%04d%02d%02d_%02d%02d%02d.dat",
                         w->out_dir,
                         tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                         tmv.tm_hour, tmv.tm_min, tmv.tm_sec) >= (int)out_cap) {
                return -1;
            }
        } else {
            if (snprintf(out, out_cap, "%s/log_%04d%02d%02d_%02d%02d%02d_%d.dat",
                         w->out_dir,
                         tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                         tmv.tm_hour, tmv.tm_min, tmv.tm_sec, i) >= (int)out_cap) {
                return -1;
            }
        }
        struct stat st;
        if (stat(out, &st) != 0) {
            return 0;
        }
    }
    return -1;
}

static int writer_open_new(writer_t *w) {
    char name[1024];
    if (ensure_dir(w->out_dir) != 0) {
        return -1;
    }
    if (writer_build_new_path(w, name, sizeof(name)) != 0) {
        return -1;
    }
    if (w->fp != NULL) {
        fclose(w->fp);
        w->fp = NULL;
    }
    w->fp = fopen(name, "ab");
    if (w->fp == NULL) {
        return -1;
    }
    if (snprintf(w->current_path, sizeof(w->current_path), "%s", name) >= (int)sizeof(w->current_path)) {
        fclose(w->fp);
        w->fp = NULL;
        return -1;
    }
    w->file_size = 0;
    prune_old_files(w);
    return 0;
}

static int writer_init(writer_t *w, const char *out_dir, size_t max_file_size, int max_files) {
    memset(w, 0, sizeof(*w));
    if (snprintf(w->out_dir, sizeof(w->out_dir), "%s", out_dir) >= (int)sizeof(w->out_dir)) {
        return -1;
    }
    w->max_file_size = max_file_size;
    w->max_files = max_files;
    if (ensure_dir(out_dir) != 0) {
        return -1;
    }
    return writer_open_new(w);
}

static int writer_write(writer_t *w, const char *data, size_t len) {
    size_t off = 0;
    struct stat st;
    if (w->current_path[0] == '\0' || stat(w->current_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (writer_open_new(w) != 0) {
            return -1;
        }
    }
    while (off < len) {
        size_t space = (w->max_file_size > w->file_size) ? (w->max_file_size - w->file_size) : 0;
        size_t chunk = len - off;
        if (space == 0) {
            if (writer_open_new(w) != 0) {
                return -1;
            }
            space = w->max_file_size;
        }
        if (chunk > space) {
            chunk = space;
        }
        if (fwrite(data + off, 1, chunk, w->fp) != chunk) {
            return -1;
        }
        w->file_size += chunk;
        off += chunk;
    }
    return 0;
}

static int flush_ring(ring_buffer_t *ring, writer_t *writer) {
    char tmp[64 * 1024];
    while (ring->size > 0) {
        size_t n = ring_read(ring, tmp, sizeof(tmp));
        if (n == 0) {
            break;
        }
        if (writer_write(writer, tmp, n) != 0) {
            return -1;
        }
    }
    fflush(writer->fp);
    return 0;
}

static int build_record(char *out, size_t out_cap, const char *payload, size_t payload_len) {
    if (payload_len == 0) {
        if (out_cap < 1) return -1;
        out[0] = '\n';
        return 1;
    }
    /* 透传客户端内容；若无换行则补 '\n' */
    if (payload[payload_len - 1] == '\n') {
        if (payload_len > out_cap) return -1;
        memcpy(out, payload, payload_len);
        return (int)payload_len;
    } else {
        if (payload_len + 1 > out_cap) return -1;
        memcpy(out, payload, payload_len);
        out[payload_len] = '\n';
        return (int)(payload_len + 1);
    }
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in addr;
    int port = DAEMON_DEFAULT_PORT;
    size_t ring_size = DAEMON_DEFAULT_RING;
    size_t max_file = DAEMON_DEFAULT_MAX_FILE;
    int flush_ms = DAEMON_DEFAULT_FLUSH_MS;
    const char *out_dir = DAEMON_DEFAULT_OUTPUT;
    int max_files = DAEMON_DEFAULT_MAX_FILES;
    ring_buffer_t ring;
    writer_t writer;
    uint64_t last_flush;
    char packet[DAEMON_PACKET_MAX];
    char record[DAEMON_PACKET_MAX + 64];

    if (argc > 1) {
        port = atoi(argv[1]);
    }
    if (argc > 2) {
        out_dir = argv[2];
    }
    if (argc > 3) {
        ring_size = (size_t)atoll(argv[3]);
    }
    if (argc > 4) {
        flush_ms = atoi(argv[4]);
    }
    if (argc > 5) {
        max_file = (size_t)atoll(argv[5]);
    }
    if (argc > 6) {
        max_files = atoi(argv[6]);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (ring_init(&ring, ring_size) != 0) {
        file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "ring init failed\n");
        return 1;
    }
    if (writer_init(&writer, out_dir, max_file, max_files) != 0) {
        file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "writer init failed\n");
        ring_free(&ring);
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "socket failed: %s\n", strerror(errno));
        ring_free(&ring);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "bind failed: %s\n", strerror(errno));
        close(sockfd);
        ring_free(&ring);
        return 1;
    }

    last_flush = now_ms();
    while (g_running) {
        fd_set rfds;
        struct timeval tv;
        int rv;
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;

        rv = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(sockfd, &rfds)) {
            ssize_t n = recvfrom(sockfd, packet, sizeof(packet), 0, NULL, NULL);
            if (n > 0) {
                int rec_len = build_record(record, sizeof(record), packet, (size_t)n);
                if (rec_len > 0) {
                    if ((size_t)rec_len > (ring.cap - ring.size)) {
                        if (flush_ring(&ring, &writer) != 0) {
                            file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "flush error\n");
                            break;
                        }
                        last_flush = now_ms();
                    }
                    if ((size_t)rec_len <= ring.cap) {
                        ring_write(&ring, record, (size_t)rec_len);
                    } else {
                        if (writer_write(&writer, record, (size_t)rec_len) != 0) {
                            file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "direct write error\n");
                            break;
                        }
                    }
                }
            }
        }

        if (ring.size >= ring.cap) {
            if (flush_ring(&ring, &writer) != 0) {
                file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "flush full error\n");
                break;
            }
            last_flush = now_ms();
        } else if (ring.size > 0 && (now_ms() - last_flush >= (uint64_t)flush_ms)) {
            if (flush_ring(&ring, &writer) != 0) {
                file_logger_fprintf(LOG_DAEMON_FILE_LOG, stderr, "flush timeout error\n");
                break;
            }
            last_flush = now_ms();
        }
    }

    if (ring.size > 0) {
        flush_ring(&ring, &writer);
    }
    if (writer.fp) {
        fclose(writer.fp);
    }
    close(sockfd);
    ring_free(&ring);
    return 0;
}
