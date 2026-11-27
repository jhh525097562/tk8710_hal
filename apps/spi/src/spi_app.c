#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sched.h>
#include "spi_app.h"
#include "file_logger.h"

#define SPI_FILE_LOG "/var/log/tk8710_gw.log"
#define printf(...) file_logger_printf(SPI_FILE_LOG, "INFO", __VA_ARGS__)
#define fprintf(stream, ...) file_logger_fprintf(SPI_FILE_LOG, stream, __VA_ARGS__)
#define perror(prefix) file_logger_perror(SPI_FILE_LOG, prefix)

// Global Context
typedef struct {
    ipc_smp_handle_t *ch_mqtt_to_spi; // Consumer
    ipc_smp_handle_t *ch_spi_to_mqtt; // Producer
    int spi_fd;
    int running;
} SpiAppContext;

static SpiAppContext g_ctx;

// Helper to get current time in milliseconds
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int get_random_freq_offset(void) {
    return (rand() % 40001) - 20000;
}

// Print Message Header + Payload
static void print_msg_full(const char *prefix, const ipc_smp_msg_hdr_t *hdr, const void *payload, uint32_t payload_len) {
    printf("[%s] HeaderParsed: ver=%u type=%u len=%u seq=%u reserved=%u ts_ns=%llu flags=%u crc32=0x%08X\n",
           prefix,
           (unsigned)hdr->version,
           (unsigned)hdr->type,
           (unsigned)hdr->len,
           (unsigned)hdr->seq,
           (unsigned)hdr->reserved,
           (unsigned long long)hdr->ts_ns,
           (unsigned)hdr->flags,
           (unsigned)hdr->crc32);
    printf("  Length: %u\n", payload_len);
    
    if (payload && payload_len >= sizeof(MacMsgType_e)) {
        MacMsgType_e *msg_type_ptr = (MacMsgType_e *)payload;
        if (*msg_type_ptr == MSG_TYPE_NS_CONFIG_DOWN && payload_len >= sizeof(NsConfigDown_t)) {
            const NsConfigDown_t *ns_config = (const NsConfigDown_t *)payload;
            printf("  NsConfigDown_t:\n");
            printf("    msg_type: %d\n", ns_config->msg_type);
            printf("    freq: %u\n", ns_config->freq);
            printf("    nwk_num: %d\n", ns_config->nwk_num);
            printf("    tdd_num: %d\n", ns_config->tdd_num);
            printf("    slot_cfg: %d\n", ns_config->slot_cfg);
            printf("    rate_num: %d\n", ns_config->rate_num);
            for (int i = 0; i < ns_config->rate_num && i < MAX_RATE_CFGS; i++) {
                printf("    rate_cfgs[%d]: rate=%d uplink_pkt=%d downlink_pkt=%d\n", 
                       i, ns_config->rate_cfgs[i].rate, ns_config->rate_cfgs[i].uplink_pkt, ns_config->rate_cfgs[i].downlink_pkt);
            }
        } else if (*msg_type_ptr == MSG_TYPE_NS_DATA_DOWN && payload_len >= offsetof(NsDataDown_t, payload)) {
            const NsDataDown_t *ns_data = (const NsDataDown_t *)payload;
            printf("  NsDataDown_t:\n");
            printf("    msg_type: %d\n", ns_data->msg_type);
            printf("    tdd: %d\n", ns_data->tdd);
            printf("    rate: %d\n", ns_data->rate);
            printf("    slot: %d\n", ns_data->slot);
            printf("    payload_len: %zu\n", ns_data->payload_len);
            printf("    payload: ");
            for (size_t i = 0; i < ns_data->payload_len && i < MAX_PAYLOAD_LEN; i++) {
                printf("%02X ", ns_data->payload[i]);
            }
            printf("\n");
        } else if (*msg_type_ptr == MSG_TYPE_GW_DATA_UP && payload_len >= offsetof(GwBatchDataUp_t, data_list)) {
            const GwBatchDataUp_t *batch_data = (const GwBatchDataUp_t *)payload;
            printf("  GwBatchDataUp_t (Batch):\n");
            printf("    msg_type: %d\n", batch_data->msg_type);
            printf("    total_count: %d\n", batch_data->total_count);
            for (int i = 0; i < batch_data->total_count && i < MAX_UP_USER_NUM; i++) {
                const GwDataUp_t *gw_data = &batch_data->data_list[i];
                printf("    [%d] GwDataUp_t:\n", i);
                printf("      tdd: %d, rate: %d, slot: %d, rssi: %d, snr: %d, freq_offset: %d\n", 
                       gw_data->tdd, gw_data->rate, gw_data->slot, gw_data->rssi, gw_data->snr, gw_data->freq_offset);
                printf("      payload_len: %zu\n", gw_data->payload_len);
                printf("      payload: ");
                for (size_t j = 0; j < gw_data->payload_len && j < MAX_PAYLOAD_LEN; j++) {
                    printf("%02X ", gw_data->payload[j]);
                }
                printf("\n");
            }
        } else if (*msg_type_ptr == MSG_TYPE_CONFIG_REQ && payload_len >= sizeof(ConfigReq_t)) {
            const ConfigReq_t *req_data = (const ConfigReq_t *)payload;
            printf("  ConfigReq_t:\n");
            printf("    msg_type: %d\n", req_data->msg_type);
            printf("    req_data_type: %d\n", req_data->req_data_type);
        } else {
            printf("  Unknown struct or length mismatch (type=%d). Raw payload: ", *msg_type_ptr);
            const uint8_t *p = (const uint8_t *)payload;
            for (uint32_t i = 0; i < payload_len; i++) {
                printf("%02X ", p[i]);
            }
            printf("\n");
        }
    } else {
        printf("  Payload is null or too short for struct parsing.\n");
    }
}

// Open Attach Then Create Strategy
static int ipc_open_attach_then_create(const ipc_smp_cfg_t *base_cfg, ipc_smp_handle_t **out) {
    ipc_smp_cfg_t cfg = *base_cfg;
    int rc;

    // 1. Try to attach (create=0)
    cfg.create = 0;
    rc = ipc_smp_channel_init(&cfg, out);
    if (rc == IPC_SMP_OK) {
        printf("Attached to existing shared memory: %s\n", cfg.shm_name);
        return IPC_SMP_OK;
    }

    // 2. If failed (e.g., ENOENT), create new (create=1)
    printf("Attach failed, creating new shared memory: %s\n", cfg.shm_name);
    cfg.create = 1;
    rc = ipc_smp_channel_init(&cfg, out);
    if (rc != IPC_SMP_OK) {
        fprintf(stderr, "Failed to create shared memory %s: %d\n", cfg.shm_name, rc);
    }
    return rc;
}

static int bind_cpu2(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(2, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        fprintf(stderr, "bind cpu2 failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int spi_app_init(void) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.running = 1;
    g_ctx.spi_fd = -1;
    srand((unsigned int)time(NULL));

    // 1. Initialize IPC Channels
    ipc_smp_cfg_t rx_cfg = {0}; // MQTT -> SPI (Consumer)
    rx_cfg.shm_name = IPC_CH_MQTT_TO_SPI;
    rx_cfg.ring_size = IPC_RING_SIZE;
    rx_cfg.max_msg_size = IPC_MAX_MSG_SIZE;
    rx_cfg.batch_count = IPC_BATCH_COUNT;
    rx_cfg.flush_timeout_ms = IPC_FLUSH_TIMEOUT_MS;
    rx_cfg.role = IPC_SMP_ROLE_CONSUMER;

    if (ipc_open_attach_then_create(&rx_cfg, &g_ctx.ch_mqtt_to_spi) != IPC_SMP_OK) {
        return -1;
    }

    ipc_smp_cfg_t tx_cfg = {0}; // SPI -> MQTT (Producer)
    tx_cfg.shm_name = IPC_CH_SPI_TO_MQTT;
    tx_cfg.ring_size = IPC_RING_SIZE;
    tx_cfg.max_msg_size = IPC_MAX_MSG_SIZE;
    tx_cfg.batch_count = IPC_BATCH_COUNT;
    tx_cfg.flush_timeout_ms = IPC_FLUSH_TIMEOUT_MS;
    tx_cfg.role = IPC_SMP_ROLE_PRODUCER;

    if (ipc_open_attach_then_create(&tx_cfg, &g_ctx.ch_spi_to_mqtt) != IPC_SMP_OK) {
        return -1;
    }

    // 2. Open SPI Device
    // Using O_RDWR | O_NONBLOCK for simulation if device doesn't exist, 
    // but in real env it should be just open(SPI_DEV_PATH, O_RDWR)
    g_ctx.spi_fd = open(SPI_DEV_PATH, O_RDWR);
    if (g_ctx.spi_fd < 0) {
        fprintf(stderr, "Warning: Failed to open SPI device %s: %s (Simulating...)\n", 
                SPI_DEV_PATH, strerror(errno));
        // Not returning error here to allow simulation on non-embedded env
    } else {
        printf("Opened SPI device: %s\n", SPI_DEV_PATH);
    }

    return 0;
}

// Process incoming messages from MQTT
static void process_incoming_messages(void) {
    ipc_smp_msg_t msg;
    int rc;

    while (1) {
        rc = ipc_smp_recv(g_ctx.ch_mqtt_to_spi, &msg, 0); // Non-blocking
        if (rc == IPC_SMP_ERR_EMPTY) {
            break;
        }
        if (rc != IPC_SMP_OK) {
            fprintf(stderr, "IPC recv failed: %d\n", rc);
            break;
        }

        // Get header for printing
        // Note: For Consumer, we calculate header address from payload pointer
        const ipc_smp_msg_hdr_t *hdr = (const ipc_smp_msg_hdr_t *)((const uint8_t *)msg.payload - sizeof(ipc_smp_msg_hdr_t));
        print_msg_full("RX: MQTT->SPI", hdr, msg.payload, msg.len);

        // "Write to SPI device" logic
        if (g_ctx.spi_fd >= 0) {
            // In real scenario: write(g_ctx.spi_fd, msg.payload, msg.len);
            printf("Writing %u bytes to SPI device...\n", msg.len);
        } else {
            printf("[Sim] Writing %u bytes to SPI device...\n", msg.len);
        }

        ipc_smp_ack(g_ctx.ch_mqtt_to_spi, msg.seq);
    }
}

// Simulate reading from SPI and sending to MQTT
static void process_outgoing_simulation(void) {
    static uint64_t last_send_time = 0;
    static uint8_t cycle_byte = 0;
    static int toggle_count = 0;
    uint64_t now = get_time_ms();

    // Send every 2000ms
    if (now - last_send_time >= 2000) {
        last_send_time = now;

        // 随机决定是否发送配置请求 (例如 10% 的概率)
        // 在实际应用中可以使用更严谨的随机数，这里简单用 toggle_count 和 cycle_byte 组合模拟
        if ((toggle_count + cycle_byte) % 10 == 0) {
            ConfigReq_t req_data;
            memset(&req_data, 0, sizeof(ConfigReq_t));
            req_data.msg_type = MSG_TYPE_CONFIG_REQ;
            req_data.req_data_type = 1; // 1是请求配置

            uint32_t len = sizeof(ConfigReq_t);
            int rc = ipc_smp_send(g_ctx.ch_spi_to_mqtt, IPC_MSG_TYPE_SPI_TO_MQTT, &req_data, len, 0);
            if (rc == IPC_SMP_OK) {
                ipc_smp_periodic_check(g_ctx.ch_spi_to_mqtt);
                ipc_smp_msg_hdr_t hdr;
                if (ipc_smp_get_last_hdr(g_ctx.ch_spi_to_mqtt, &hdr) == IPC_SMP_OK) {
                    print_msg_full("TX: SPI->MQTT", &hdr, &req_data, len);
                }
            } else {
                 fprintf(stderr, "IPC send failed (ConfigReq): %d\n", rc);
            }
            
            toggle_count++; // 确保下次状态变化
            return; // 发送了配置请求后，本次周期就不发普通数据了
        }

        // Use static allocation to avoid stack overflow for large struct
        static GwBatchDataUp_t batch_data;
        memset(&batch_data, 0, sizeof(GwBatchDataUp_t));

        batch_data.msg_type = MSG_TYPE_GW_DATA_UP;
        
        // Alternate between 1 and 127 items to test large arrays
        batch_data.total_count = (toggle_count % 2 == 0) ? 1 : 127;
        toggle_count++;

        for (int i = 0; i < batch_data.total_count; i++) {
            GwDataUp_t *gw_data = &batch_data.data_list[i];
            gw_data->tdd = 0;
            gw_data->rate = 2;
            gw_data->slot = 3 + i; // Slightly modify slot based on index for distinction
            gw_data->rssi = -45;
            gw_data->snr = 18;
            gw_data->freq_offset = get_random_freq_offset();

            // Payload(Hex) : 40 80 00 01 00 00 00 00 00 11 01 13 01 02 03 04 05 06 07 08 09 10
            // And first byte cycles from 00 to FF
            uint8_t payload_hex[] = {
                (uint8_t)(cycle_byte + i), 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 
                0x01, 0x13, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10
            };
            gw_data->payload_len = sizeof(payload_hex);
            memcpy(gw_data->payload, payload_hex, gw_data->payload_len);
        }
        cycle_byte++;

        // Calculate actual required length instead of full struct size to save memory and transfer time
        // length = size of batch header + (actual number of items * size of one item)
        // Note: For inner items we still send the full struct size of GwDataUp_t to maintain array alignment
        uint32_t len = offsetof(GwBatchDataUp_t, data_list) + (batch_data.total_count * sizeof(GwDataUp_t));
        
        int rc = ipc_smp_send(g_ctx.ch_spi_to_mqtt, IPC_MSG_TYPE_SPI_TO_MQTT, &batch_data, len, 0);
        if (rc == IPC_SMP_OK) {
            ipc_smp_periodic_check(g_ctx.ch_spi_to_mqtt);
            
            ipc_smp_msg_hdr_t hdr;
            if (ipc_smp_get_last_hdr(g_ctx.ch_spi_to_mqtt, &hdr) == IPC_SMP_OK) {
                print_msg_full("TX: SPI->MQTT", &hdr, &batch_data, len);
            }
        } else {
             fprintf(stderr, "IPC send failed: %d\n", rc);
        }
    }
}

void spi_app_run(void) {
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return;
    }

    // Add Notify FD for RX Channel
    int notify_fd = ipc_smp_get_eventfd(g_ctx.ch_mqtt_to_spi);
    if (notify_fd >= 0) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = notify_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &ev);
    }

    printf("SPI Application Started. Loop...\n");

    struct epoll_event events[4];
    while (g_ctx.running) {
        // Wait for events with 100ms timeout (to allow periodic tasks)
        int nfds = epoll_wait(epfd, events, 4, 100);

        if (nfds > 0) {
            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd == notify_fd) {
                    ipc_smp_clear_notify(g_ctx.ch_mqtt_to_spi);
                    process_incoming_messages();
                }
            }
        }

        // Periodic Task: Simulate SPI data generation
        process_outgoing_simulation();
        
        // Periodic Check for TX channel (flush if needed)
        ipc_smp_periodic_check(g_ctx.ch_spi_to_mqtt);
    }

    close(epfd);
}

void spi_app_cleanup(void) {
    if (g_ctx.ch_mqtt_to_spi) ipc_smp_channel_close(g_ctx.ch_mqtt_to_spi);
    if (g_ctx.ch_spi_to_mqtt) ipc_smp_channel_close(g_ctx.ch_spi_to_mqtt);
    if (g_ctx.spi_fd >= 0) close(g_ctx.spi_fd);
}

int main(void) {
    if (bind_cpu2() != 0) {
        return 1;
    }
    if (spi_app_init() != 0) {
        return 1;
    }
    spi_app_run();
    spi_app_cleanup();
    return 0;
}
