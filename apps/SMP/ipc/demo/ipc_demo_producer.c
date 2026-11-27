#define _GNU_SOURCE
#include "ipc_smp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
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
    printf("[%ld.%03ld] %s ver=%u type=%u len=%u seq=%u flags=0x%08x ts_ns=%llu crc=0x%08x data=%.*s\n",
           (long)ts.tv_sec,
           ts.tv_nsec / 1000000L,
           tag,
           hdr->version,
           hdr->type,
           hdr->len,
           hdr->seq,
           hdr->flags,
           (unsigned long long)hdr->ts_ns,
           hdr->crc32,
           (int)len,
           (const char *)payload);
}

int main(int argc, char **argv) {
    const char *shm_name = "/ipc_smp_demo_q1";
    int total = 10;
    int cpu_id = 2;
    if (argc > 1) total = atoi(argv[1]);
    if (argc > 2) cpu_id = atoi(argv[2]);
    if (argc > 3) shm_name = argv[3];
    if (total == 0) total = 10;

    bind_cpu(cpu_id);
    printf("producer start total=%d cpu=%d shm=%s\n", total, cpu_id, shm_name);

    ipc_smp_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.shm_name = shm_name;
    cfg.ring_size = 1024 * 1024;
    cfg.max_msg_size = 32 * 1024;
    cfg.batch_count = 8;
    cfg.flush_timeout_ms = 10;
    cfg.create = 1;
    cfg.role = IPC_SMP_ROLE_PRODUCER;

    ipc_smp_handle_t *h = NULL;
    int rc = ipc_smp_channel_init(&cfg, &h);
    if (rc != IPC_SMP_OK) {
        printf("producer init failed: %d\n", rc);
        return 1;
    }
    ipc_smp_channel_reset(h);

    srand((unsigned int)time(NULL));
    for (int i = 1; total < 0 || i <= total; ++i) {
        char buf[128];
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        uint32_t max_len = (uint32_t)(sizeof(buf) - 1);
        uint32_t n = (uint32_t)(1 + (rand() % max_len));
        for (uint32_t k = 0; k < n; ++k) {
            buf[k] = alphabet[rand() % (sizeof(alphabet) - 1)];
        }
        buf[n] = '\0';
        rc = ipc_smp_send(h, 1, buf, n, 0);
        if (rc == IPC_SMP_ERR_FULL) {
            usleep(1000);
            --i;
            continue;
        }
        if (rc != IPC_SMP_OK) {
            printf("producer send failed: %d\n", rc);
            ipc_smp_channel_close(h);
            return 1;
        }
        ipc_smp_periodic_check(h);
        ipc_smp_msg_hdr_t hdr;
        int hr = ipc_smp_get_last_hdr(h, &hdr);
        if (hr == IPC_SMP_OK) {
            print_ts("producer send", &hdr, (const uint8_t *)buf, n);
        }
    }
    ipc_smp_periodic_check(h);

    ipc_smp_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ipc_smp_get_stats(h, &stats);
    printf("producer done send_ok=%u full=%u wakeups=%u\n",
           stats.send_ok,
           stats.send_full,
           stats.wakeups);

    ipc_smp_channel_close(h);
    return 0;
}
