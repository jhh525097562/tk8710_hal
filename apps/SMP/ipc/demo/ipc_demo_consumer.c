#define _GNU_SOURCE
#include "ipc_smp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/epoll.h>
#include <time.h>

static void bind_cpu(int cpu_id) {
    if (cpu_id < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

static void print_ts(const char *tag, const ipc_smp_msg_hdr_t *hdr, const uint8_t *payload, uint32_t len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("[%ld.%03ld] %s ver=%u type=%u seq=%u flags=0x%08x ts_ns=%llu crc=0x%08x len=%u data=%.*s\n",
           (long)ts.tv_sec,
           ts.tv_nsec / 1000000L,
           tag,
           hdr->version,
           hdr->type,
           hdr->seq,
           hdr->flags,
           (unsigned long long)hdr->ts_ns,
           hdr->crc32,
           len,
           (int)len,
           (const char *)payload);
}

static int drain_ring(ipc_smp_handle_t *h, int *got, int total) {
    while (total < 0 || *got < total) {
        ipc_smp_msg_t msg;
        int rc = ipc_smp_recv(h, &msg, 0);
        if (rc != IPC_SMP_OK) {
            return rc;
        }
        printf("[%u] ", msg.seq);
        ipc_smp_msg_hdr_t hdr;
        memcpy(&hdr, (const uint8_t *)msg.payload - sizeof(ipc_smp_msg_hdr_t), sizeof(hdr));
        print_ts("consumer recv", &hdr, (const uint8_t *)msg.payload, msg.len);
        ipc_smp_ack(h, msg.seq);
        (*got)++;
    }
    return IPC_SMP_OK;
}

int main(int argc, char **argv) {
    const char *shm_name = "/ipc_smp_demo_q1";
    int total = 10;
    int cpu_id = 0;
    if (argc > 1) cpu_id = atoi(argv[1]);
    if (argc > 2) shm_name = argv[2];
    if (argc > 3) total = atoi(argv[3]);
    if (total == 0) total = 10;

    bind_cpu(cpu_id);
    printf("consumer start total=%d cpu=%d shm=%s\n", total, cpu_id, shm_name);

    ipc_smp_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.shm_name = shm_name;
    cfg.ring_size = 1024 * 1024;
    cfg.max_msg_size = 32 * 1024;
    cfg.batch_count = 8;
    cfg.flush_timeout_ms = 10;
    cfg.create = 0;
    cfg.role = IPC_SMP_ROLE_CONSUMER;

    ipc_smp_handle_t *h = NULL;
    int rc = ipc_smp_channel_init(&cfg, &h);
    if (rc != IPC_SMP_OK) {
        printf("consumer init failed: %d\n", rc);
        return 1;
    }

    int notify_fd = ipc_smp_get_eventfd(h);
    if (notify_fd < 0) {
        ipc_smp_channel_close(h);
        return 1;
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        ipc_smp_channel_close(h);
        return 1;
    }
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = notify_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &ev) != 0) {
        close(epfd);
        ipc_smp_channel_close(h);
        return 1;
    }

    int got = 0;
    const int busy_loops = 1000;
    while (total < 0 || got < total) {
        for (int i = 0; i < busy_loops && (total < 0 || got < total); ++i) {
            ipc_smp_msg_t msg;
            rc = ipc_smp_recv(h, &msg, 0);
            if (rc == IPC_SMP_OK) {
                printf("[%u] ", msg.seq);
                ipc_smp_msg_hdr_t hdr;
                memcpy(&hdr, (const uint8_t *)msg.payload - sizeof(ipc_smp_msg_hdr_t), sizeof(hdr));
                print_ts("consumer recv", &hdr, (const uint8_t *)msg.payload, msg.len);
                ipc_smp_ack(h, msg.seq);
                got++;
                i = -1;
            }
        }

        if (total > 0 && got >= total) break;

        struct epoll_event events[4];
        int n = epoll_wait(epfd, events, 4, 100);
        if (n <= 0) continue;
        for (int i = 0; i < n && (total < 0 || got < total); ++i) {
            if (events[i].data.fd != notify_fd) continue;
            ipc_smp_clear_notify(h);
            int dr = drain_ring(h, &got, total);
            if (dr != IPC_SMP_OK && dr != IPC_SMP_ERR_EMPTY) {
                close(epfd);
                ipc_smp_channel_close(h);
                return 1;
            }
        }
    }

    ipc_smp_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ipc_smp_get_stats(h, &stats);
    printf("consumer done recv_ok=%u empty=%u wakeups=%u\n",
           stats.recv_ok,
           stats.recv_empty,
           stats.wakeups);

    close(epfd);
    ipc_smp_channel_close(h);
    return 0;
}
