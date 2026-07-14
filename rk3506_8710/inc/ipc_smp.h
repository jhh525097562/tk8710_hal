/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-03 17:50:31
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-05 14:17:45
 * @FilePath: \SMP\ipc\include\ipc_smp.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef IPC_SMP_H
#define IPC_SMP_H

#include <stdint.h>
#include <stddef.h>
#include "driver/tk8710_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_SMP_OK 0
#define IPC_SMP_ERR_INVAL -1
#define IPC_SMP_ERR_SYS -2
#define IPC_SMP_ERR_FULL -3
#define IPC_SMP_ERR_EMPTY -4
#define IPC_SMP_ERR_BUSY -5
#define IPC_SMP_ERR_TIMEOUT -6

#define IPC_SMP_FLAG_HIGH_PRIO (1u << 0)
#define IPC_SMP_MSG_PADDING 0xFFFFu
#define IPC_SMP_PROTO_VERSION 1u

typedef enum {
    IPC_SMP_ROLE_PRODUCER = 1,
    IPC_SMP_ROLE_CONSUMER = 2
} ipc_smp_role_t;

typedef struct {
    uint16_t version;
    uint16_t type;
    uint32_t len;
    uint32_t seq;
    uint32_t reserved;
    uint64_t ts_ns;
    uint32_t flags;
    uint32_t crc32;
} TK8710_ALIGNED(8) ipc_smp_msg_hdr_t;

typedef struct {
    const char *shm_name;
    size_t ring_size;
    uint32_t max_msg_size;
    uint32_t batch_count;
    uint32_t flush_timeout_ms;
    uint8_t create;
    ipc_smp_role_t role;
} ipc_smp_cfg_t;

typedef struct {
    uint32_t type;
    uint32_t len;
    uint32_t seq;
    uint32_t flags;
    uint64_t ts_ns;
    const uint8_t *payload;
} ipc_smp_msg_t;

typedef struct {
    uint32_t send_ok;
    uint32_t send_full;
    uint32_t recv_ok;
    uint32_t recv_empty;
    uint32_t dropped_low_prio;
    uint32_t wakeups;
} ipc_smp_stats_t;

typedef struct ipc_smp_handle ipc_smp_handle_t;

int ipc_smp_channel_init(const ipc_smp_cfg_t *cfg, ipc_smp_handle_t **out);
int ipc_smp_send(ipc_smp_handle_t *h, uint16_t type, const void *buf, uint32_t len, uint32_t flags);
int ipc_smp_recv(ipc_smp_handle_t *h, ipc_smp_msg_t *msg, int timeout_ms);
int ipc_smp_ack(ipc_smp_handle_t *h, uint32_t seq);
int ipc_smp_get_eventfd(ipc_smp_handle_t *h);
int ipc_smp_get_last_hdr(ipc_smp_handle_t *h, ipc_smp_msg_hdr_t *hdr);
int ipc_smp_clear_notify(ipc_smp_handle_t *h);
int ipc_smp_periodic_check(ipc_smp_handle_t *h);
int ipc_smp_get_stats(ipc_smp_handle_t *h, ipc_smp_stats_t *stats);
int ipc_smp_channel_reset(ipc_smp_handle_t *h);
void ipc_smp_channel_close(ipc_smp_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif
