#define _GNU_SOURCE
#include "ipc_smp.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "file_logger.h"

#define IPC_SMP_FILE_LOG "/var/log/ipc_smp.log"
#define printf(...) file_logger_printf(IPC_SMP_FILE_LOG, "INFO", __VA_ARGS__)

#define IPC_SMP_MAGIC 0x534D5049u
#define IPC_SMP_CACHELINE 64u
#define IPC_SMP_ALIGN8(v) (((v) + 7u) & ~7u)
#define IPC_SMP_ALIGN64(v) (((v) + 63u) & ~63u)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t ring_size;
    uint32_t max_msg_size;
    atomic_uint_fast32_t head;
    atomic_uint_fast32_t tail;
    atomic_uint_fast32_t send_ok;
    atomic_uint_fast32_t send_full;
    atomic_uint_fast32_t recv_ok;
    atomic_uint_fast32_t recv_empty;
    atomic_uint_fast32_t dropped_low_prio;
    atomic_uint_fast32_t wakeups;
    uint8_t reserved[IPC_SMP_CACHELINE];
} ipc_smp_ctrl_t;

struct ipc_smp_handle {
    ipc_smp_cfg_t cfg;
    int shm_fd;
    int notify_fd;
    struct sockaddr_un notify_addr;
    socklen_t notify_addr_len;
    void *map_base;
    size_t map_len;
    ipc_smp_ctrl_t *ctrl;
    uint8_t *ring;
    uint32_t pending_count;
    uint64_t last_flush_ns;
    uint32_t inflight_seq;
    uint32_t inflight_size;
    uint32_t inflight_tail;
    int has_inflight;
    ipc_smp_msg_hdr_t last_hdr;
    int has_last_hdr;
};

static int ipc_smp_make_notify_addr(const char *shm_name, struct sockaddr_un *addr, socklen_t *addr_len) {
    if (!shm_name || !addr || !addr_len) return IPC_SMP_ERR_INVAL;
    const char *name = shm_name;
    while (*name == '/') name++;
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    addr->sun_path[0] = '\0';
    int n = snprintf(addr->sun_path + 1, sizeof(addr->sun_path) - 1, "ipc_smp_notify_%s", name);
    if (n <= 0 || n >= (int)(sizeof(addr->sun_path) - 1)) {
        return IPC_SMP_ERR_INVAL;
    }
    *addr_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + n);
    return IPC_SMP_OK;
}

static int ipc_smp_notify_open(ipc_smp_handle_t *h) {
    h->notify_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (h->notify_fd < 0) {
        printf("ipc_smp notify socket failed errno=%d\n", errno);
        return IPC_SMP_ERR_SYS;
    }
    int flags = fcntl(h->notify_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(h->notify_fd, F_SETFL, flags | O_NONBLOCK);
    }
    flags = fcntl(h->notify_fd, F_GETFD, 0);
    if (flags >= 0) {
        fcntl(h->notify_fd, F_SETFD, flags | FD_CLOEXEC);
    }
    int rc = ipc_smp_make_notify_addr(h->cfg.shm_name, &h->notify_addr, &h->notify_addr_len);
    if (rc != IPC_SMP_OK) {
        printf("ipc_smp notify addr failed rc=%d\n", rc);
        return rc;
    }
    if (h->cfg.role == IPC_SMP_ROLE_CONSUMER) {
        if (bind(h->notify_fd, (const struct sockaddr *)&h->notify_addr, h->notify_addr_len) != 0) {
            printf("ipc_smp notify bind failed errno=%d\n", errno);
            return IPC_SMP_ERR_SYS;
        }
        printf("ipc_smp notify bind ok\n");
    }
    if (h->cfg.role == IPC_SMP_ROLE_PRODUCER) {
        printf("ipc_smp notify producer ready\n");
    }
    return IPC_SMP_OK;
}

static uint64_t ipc_smp_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint32_t ipc_smp_crc32_seed(uint32_t crc, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            uint32_t mask = -(int)(crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static __attribute__((unused)) uint32_t ipc_smp_crc32(const uint8_t *data, uint32_t len) {
    return ~ipc_smp_crc32_seed(0xFFFFFFFFu, data, len);
}

static int ipc_smp_flush_event(ipc_smp_handle_t *h) {
    if (!h || h->pending_count == 0) {
        return IPC_SMP_OK;
    }
    if (h->notify_fd >= 0) {
        char b = 1;
        ssize_t n = sendto(h->notify_fd, &b, sizeof(b), MSG_DONTWAIT | MSG_NOSIGNAL,
                           (const struct sockaddr *)&h->notify_addr, h->notify_addr_len);
        if (n < 0 &&
            errno != EAGAIN &&
            errno != ENOBUFS &&
            errno != ECONNREFUSED &&
            errno != ENOENT) {
            printf("ipc_smp notify send failed errno=%d\n", errno);
            return IPC_SMP_ERR_SYS;
        }
    }
    h->pending_count = 0;
    h->last_flush_ns = ipc_smp_now_ns();
    atomic_fetch_add_explicit(&h->ctrl->wakeups, 1, memory_order_relaxed);
    return IPC_SMP_OK;
}

static int ipc_smp_ring_write(ipc_smp_handle_t *h, const uint8_t *data, uint32_t len, uint16_t type, uint32_t flags) {
    ipc_smp_ctrl_t *c = h->ctrl;
    uint32_t head = atomic_load_explicit(&c->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&c->tail, memory_order_acquire);
    uint32_t ring_size = c->ring_size;
    uint32_t rec_size = (uint32_t)IPC_SMP_ALIGN8(sizeof(ipc_smp_msg_hdr_t) + len);
    uint32_t used = head - tail;
    if (used + rec_size > ring_size) {
        atomic_fetch_add_explicit(&c->send_full, 1, memory_order_relaxed);
        if (!(flags & IPC_SMP_FLAG_HIGH_PRIO)) {
            atomic_fetch_add_explicit(&c->dropped_low_prio, 1, memory_order_relaxed);
        }
        return IPC_SMP_ERR_FULL;
    }

    uint32_t off = (uint32_t)(head % ring_size);
    uint32_t remain = ring_size - off;
    if (remain < rec_size) {
        if (remain >= sizeof(ipc_smp_msg_hdr_t)) {
            ipc_smp_msg_hdr_t pad;
            memset(&pad, 0, sizeof(pad));
            pad.version = IPC_SMP_PROTO_VERSION;
            pad.type = IPC_SMP_MSG_PADDING;
            pad.len = remain - (uint32_t)sizeof(ipc_smp_msg_hdr_t);
            memcpy(h->ring + off, &pad, sizeof(pad));
        }
        head += remain;
        off = 0;
    }

    ipc_smp_msg_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version = IPC_SMP_PROTO_VERSION;
    hdr.type = type;
    hdr.len = len;
    hdr.seq = (uint32_t)(head / 8u);
    hdr.ts_ns = ipc_smp_now_ns();
    hdr.flags = flags;
    hdr.crc32 = 0;

    memcpy(h->ring + off, &hdr, sizeof(hdr));
    if (len > 0) {
        memcpy(h->ring + off + sizeof(hdr), data, len);
    }
    if (rec_size > sizeof(hdr) + len) {
        memset(h->ring + off + sizeof(hdr) + len, 0, rec_size - sizeof(hdr) - len);
    }
    uint32_t crc = ipc_smp_crc32_seed(0xFFFFFFFFu, (const uint8_t *)&hdr, (uint32_t)offsetof(ipc_smp_msg_hdr_t, crc32));
    crc = ipc_smp_crc32_seed(crc, h->ring + off + sizeof(hdr), len);
    hdr.crc32 = ~crc;
    memcpy(h->ring + off + offsetof(ipc_smp_msg_hdr_t, crc32), &hdr.crc32, sizeof(hdr.crc32));
    h->last_hdr = hdr;
    h->has_last_hdr = 1;

    atomic_store_explicit(&c->head, (uint32_t)(head + rec_size), memory_order_release);
    atomic_fetch_add_explicit(&c->send_ok, 1, memory_order_relaxed);
    return IPC_SMP_OK;
}

int ipc_smp_channel_init(const ipc_smp_cfg_t *cfg, ipc_smp_handle_t **out) {
    if (!cfg || !out || !cfg->shm_name || cfg->ring_size < 4096u || cfg->max_msg_size == 0 || cfg->max_msg_size > (1024u * 1024u)) {
        return IPC_SMP_ERR_INVAL;
    }
    ipc_smp_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return IPC_SMP_ERR_SYS;
    h->cfg = *cfg;
    h->cfg.batch_count = h->cfg.batch_count == 0 ? 8u : h->cfg.batch_count;
    h->cfg.flush_timeout_ms = h->cfg.flush_timeout_ms == 0 ? 10u : h->cfg.flush_timeout_ms;
    h->shm_fd = -1;
    h->notify_fd = -1;

    int oflag = O_RDWR | (cfg->create ? O_CREAT : 0);
    h->shm_fd = shm_open(cfg->shm_name, oflag, 0600);
    if (h->shm_fd < 0) {
        printf("ipc_smp shm_open failed errno=%d\n", errno);
        free(h);
        return IPC_SMP_ERR_SYS;
    }

    size_t ctrl_len = IPC_SMP_ALIGN64(sizeof(ipc_smp_ctrl_t));
    size_t map_len = ctrl_len + IPC_SMP_ALIGN64(cfg->ring_size);
    if (cfg->create) {
        if (ftruncate(h->shm_fd, (off_t)map_len) != 0) {
            close(h->shm_fd);
            free(h);
            return IPC_SMP_ERR_SYS;
        }
    }

    h->map_base = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, h->shm_fd, 0);
    if (h->map_base == MAP_FAILED) {
        printf("ipc_smp mmap failed errno=%d\n", errno);
        close(h->shm_fd);
        free(h);
        return IPC_SMP_ERR_SYS;
    }
    h->map_len = map_len;
    h->ctrl = (ipc_smp_ctrl_t *)h->map_base;
    h->ring = (uint8_t *)h->map_base + ctrl_len;

    if (cfg->create || h->ctrl->magic != IPC_SMP_MAGIC) {
        memset(h->map_base, 0, map_len);
        h->ctrl->magic = IPC_SMP_MAGIC;
        h->ctrl->version = IPC_SMP_PROTO_VERSION;
        h->ctrl->ring_size = (uint32_t)IPC_SMP_ALIGN64(cfg->ring_size);
        h->ctrl->max_msg_size = cfg->max_msg_size;
        atomic_store_explicit(&h->ctrl->head, 0, memory_order_release);
        atomic_store_explicit(&h->ctrl->tail, 0, memory_order_release);
    }

    int notify_rc = ipc_smp_notify_open(h);
    if (notify_rc != IPC_SMP_OK) {
        printf("ipc_smp notify open failed rc=%d\n", notify_rc);
        munmap(h->map_base, h->map_len);
        close(h->shm_fd);
        if (h->notify_fd >= 0) close(h->notify_fd);
        free(h);
        return notify_rc;
    }
    printf("ipc_smp channel init ok role=%d shm=%s\n", h->cfg.role, h->cfg.shm_name);
    h->last_flush_ns = ipc_smp_now_ns();
    *out = h;
    return IPC_SMP_OK;
}

int ipc_smp_send(ipc_smp_handle_t *h, uint16_t type, const void *buf, uint32_t len, uint32_t flags) {
    if (!h || (!buf && len > 0)) return IPC_SMP_ERR_INVAL;
    if (len > h->ctrl->max_msg_size) return IPC_SMP_ERR_INVAL;
    int rc = ipc_smp_ring_write(h, (const uint8_t *)buf, len, type, flags);
    if (rc != IPC_SMP_OK) return rc;
    h->pending_count++;
    uint64_t now = ipc_smp_now_ns();
    uint64_t elapsed_ms = (now - h->last_flush_ns) / 1000000ull;
    if ((flags & IPC_SMP_FLAG_HIGH_PRIO) || h->pending_count >= h->cfg.batch_count || elapsed_ms >= h->cfg.flush_timeout_ms) {
        return ipc_smp_flush_event(h);
    }
    return IPC_SMP_OK;
}

int ipc_smp_periodic_check(ipc_smp_handle_t *h) {
    if (!h) return IPC_SMP_ERR_INVAL;
    uint64_t now = ipc_smp_now_ns();
    uint64_t elapsed_ms = (now - h->last_flush_ns) / 1000000ull;
    if (h->pending_count > 0 && elapsed_ms >= h->cfg.flush_timeout_ms) {
        return ipc_smp_flush_event(h);
    }
    return IPC_SMP_OK;
}

int ipc_smp_recv(ipc_smp_handle_t *h, ipc_smp_msg_t *msg, int timeout_ms) {
    if (!h || !msg) return IPC_SMP_ERR_INVAL;
    if (h->has_inflight) return IPC_SMP_ERR_BUSY;

    ipc_smp_ctrl_t *c = h->ctrl;
    uint64_t deadline_ns = timeout_ms > 0 ? ipc_smp_now_ns() + (uint64_t)timeout_ms * 1000000ull : 0;
    static uint64_t last_empty_log_ns = 0;

    while (1) {
        uint32_t head = atomic_load_explicit(&c->head, memory_order_acquire);
        uint32_t tail = atomic_load_explicit(&c->tail, memory_order_relaxed);
        if (head == tail) {
            uint64_t now_ns = ipc_smp_now_ns();
            if (now_ns - last_empty_log_ns > 1000000000ull) {
                printf("ipc_smp empty head=%u tail=%u\n", head, tail);
                last_empty_log_ns = now_ns;
            }
            atomic_fetch_add_explicit(&c->recv_empty, 1, memory_order_relaxed);
            if (timeout_ms == 0) return IPC_SMP_ERR_EMPTY;
            if (timeout_ms < 0) {
                struct timespec ts = {0, 1000000};
                nanosleep(&ts, NULL);
                continue;
            }
            if (ipc_smp_now_ns() >= deadline_ns) return IPC_SMP_ERR_TIMEOUT;
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
            continue;
        }

        uint32_t off = (uint32_t)(tail % c->ring_size);
        uint32_t remain = c->ring_size - off;
        if (remain < sizeof(ipc_smp_msg_hdr_t)) {
            atomic_store_explicit(&c->tail, (uint32_t)(tail + remain), memory_order_release);
            continue;
        }
        ipc_smp_msg_hdr_t hdr;
        memcpy(&hdr, h->ring + off, sizeof(hdr));
        if (hdr.type == IPC_SMP_MSG_PADDING) {
            uint32_t pad_step = (uint32_t)(sizeof(hdr) + hdr.len);
            atomic_store_explicit(&c->tail, (uint32_t)(tail + pad_step), memory_order_release);
            continue;
        }
        uint32_t rec_size = (uint32_t)IPC_SMP_ALIGN8(sizeof(hdr) + hdr.len);
        uint32_t available = (uint32_t)(head - tail);
        if (rec_size > available) return IPC_SMP_ERR_EMPTY;

        uint32_t calc_crc = ipc_smp_crc32_seed(0xFFFFFFFFu, h->ring + off, (uint32_t)offsetof(ipc_smp_msg_hdr_t, crc32));
        calc_crc = ipc_smp_crc32_seed(calc_crc, h->ring + off + sizeof(hdr), hdr.len);
        calc_crc = ~calc_crc;
        if (calc_crc != hdr.crc32) {
            printf("ipc_smp crc mismatch seq=%u calc=%08x got=%08x\n",
                   hdr.seq, calc_crc, hdr.crc32);
            atomic_store_explicit(&c->tail, (uint32_t)(tail + rec_size), memory_order_release);
            continue;
        }

        msg->type = hdr.type;
        msg->len = hdr.len;
        msg->seq = hdr.seq;
        msg->flags = hdr.flags;
        msg->ts_ns = hdr.ts_ns;
        msg->payload = h->ring + off + sizeof(hdr);
        h->inflight_seq = hdr.seq;
        h->inflight_size = rec_size;
        h->inflight_tail = tail;
        h->has_inflight = 1;
        atomic_fetch_add_explicit(&c->recv_ok, 1, memory_order_relaxed);
        return IPC_SMP_OK;
    }
}

int ipc_smp_ack(ipc_smp_handle_t *h, uint32_t seq) {
    if (!h) return IPC_SMP_ERR_INVAL;
    if (!h->has_inflight) return IPC_SMP_ERR_INVAL;
    if (seq != h->inflight_seq) return IPC_SMP_ERR_INVAL;
    atomic_store_explicit(&h->ctrl->tail, (uint32_t)(h->inflight_tail + h->inflight_size), memory_order_release);
    h->has_inflight = 0;
    h->inflight_seq = 0;
    h->inflight_size = 0;
    h->inflight_tail = 0;
    return IPC_SMP_OK;
}

int ipc_smp_get_eventfd(ipc_smp_handle_t *h) {
    if (!h) return IPC_SMP_ERR_INVAL;
    return h->notify_fd;
}

int ipc_smp_get_last_hdr(ipc_smp_handle_t *h, ipc_smp_msg_hdr_t *hdr) {
    if (!h || !hdr) return IPC_SMP_ERR_INVAL;
    if (!h->has_last_hdr) return IPC_SMP_ERR_EMPTY;
    *hdr = h->last_hdr;
    return IPC_SMP_OK;
}

int ipc_smp_clear_notify(ipc_smp_handle_t *h) {
    if (!h || h->notify_fd < 0) return IPC_SMP_ERR_INVAL;
    char buf[64];
    while (recv(h->notify_fd, buf, sizeof(buf), MSG_DONTWAIT) > 0) {
    }
    return IPC_SMP_OK;
}

int ipc_smp_get_stats(ipc_smp_handle_t *h, ipc_smp_stats_t *stats) {
    if (!h || !stats) return IPC_SMP_ERR_INVAL;
    stats->send_ok = atomic_load_explicit(&h->ctrl->send_ok, memory_order_relaxed);
    stats->send_full = atomic_load_explicit(&h->ctrl->send_full, memory_order_relaxed);
    stats->recv_ok = atomic_load_explicit(&h->ctrl->recv_ok, memory_order_relaxed);
    stats->recv_empty = atomic_load_explicit(&h->ctrl->recv_empty, memory_order_relaxed);
    stats->dropped_low_prio = atomic_load_explicit(&h->ctrl->dropped_low_prio, memory_order_relaxed);
    stats->wakeups = atomic_load_explicit(&h->ctrl->wakeups, memory_order_relaxed);
    return IPC_SMP_OK;
}

int ipc_smp_channel_reset(ipc_smp_handle_t *h) {
    if (!h) return IPC_SMP_ERR_INVAL;
    atomic_store_explicit(&h->ctrl->head, 0, memory_order_release);
    atomic_store_explicit(&h->ctrl->tail, 0, memory_order_release);
    h->pending_count = 0;
    h->has_inflight = 0;
    h->last_flush_ns = ipc_smp_now_ns();
    return IPC_SMP_OK;
}

void ipc_smp_channel_close(ipc_smp_handle_t *h) {
    if (!h) return;
    if (h->notify_fd >= 0) close(h->notify_fd);
    if (h->map_base && h->map_base != MAP_FAILED) munmap(h->map_base, h->map_len);
    if (h->shm_fd >= 0) close(h->shm_fd);
    free(h);
}
