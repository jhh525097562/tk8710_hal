#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <gpiod.h>
#include <mosquitto.h>
#include <time.h>
#include <json-c/json.h>
#include "config.h"
#include "log_udp.h"
#include "ipc_smp.h"
#include "mac_msg_parser.h"
#include "file_logger.h"
#include "ns_scan_manager.h"
#include "gps_info.h"
#include "mqtt_status.h"

#define IPC_CH_MQTT_TO_SPI "/ipc_mqtt_to_spi"
#define IPC_CH_SPI_TO_MQTT "/ipc_spi_to_mqtt"
#define IPC_RING_SIZE (4u * 1024u * 1024u)
#define IPC_MAX_MSG_SIZE (sizeof(GwBatchDataUp_t) + 256u)
#define IPC_BATCH_COUNT 8u
#define IPC_FLUSH_TIMEOUT_MS 20u
#define IPC_MSG_TYPE_MQTT_TO_SPI 0x1001u
#define CONFIG_FILE_PATH "/etc/config.json"
#define DEVICE_VERSION_FILE_PATH "/etc/device_version.json"
#define BDGPS_FILE_PATH "/var/run/bdgps"
#define GPS_INFO_FILE_PATH "/userdata/GPS/GPSinfo.txt"
#define LED_DIR "/run/led"
#define MQTT_LED_STATE_FILE "/run/led/mqtt.state"
#define MQTT_LED_TMP_FILE "/run/led/mqtt.tmp"
#define MQTT_LED_TXRX_TIMEOUT_MS 2000ULL
#define MQTT_STATS_FILE "/var/run/mqtt_stats.json"
#define MQTT_STATS_TMP_FILE "/var/run/mqtt_stats.tmp"
#define MQTT_STATS_WRITE_INTERVAL_MS 1000ULL
#define MQTT_FILE_LOG "/var/log/mqtt_bridge.log"
#define POWER_GPIO_CHIP_NAME "gpiochip1"
#define POWER_GPIO_OFFSET 17
#define APP_VERSION "V1.0.2"
#define APP_LOG_DEBUG 0
#define APP_LOG_INFO 1
#define APP_LOG_WARN 2
#define APP_LOG_ERROR 3
#define APP_LOG_BUF_SIZE 65536
#define APP_LOG_FAST_BUF_SIZE 4096
#define APP_LOG_UDP_CHUNK_SIZE 3800
#define APP_LOG_MIN_LEVEL APP_LOG_INFO
#define APP_UDP_MIN_LEVEL APP_LOG_WARN
#define APP_CONSOLE_ENABLED(level) (g_debug_log_enabled || ((level) >= APP_LOG_WARN))
#define APP_UDP_ENABLED(level) (g_debug_log_enabled || ((level) >= APP_UDP_MIN_LEVEL))
#define APP_FILE_ENABLED(level) ((level) >= APP_LOG_MIN_LEVEL)
#define APP_LOG_ENABLED(level) (APP_CONSOLE_ENABLED(level) || APP_UDP_ENABLED(level) || APP_FILE_ENABLED(level))

typedef struct {
    AppConfig config;
    struct mosquitto *mosq;
    ns_scan_manager *scan_manager;
    ipc_smp_handle_t *ch_mqtt_to_spi;
    ipc_smp_handle_t *ch_spi_to_mqtt;
    pthread_mutex_t tx_lock;
    pthread_mutex_t led_lock;
    pthread_mutex_t stats_lock;
    time_t last_heartbeat_time;
    time_t start_time;
    int config_ack_received;
    int config_request_triggered;
    time_t last_config_req_time;
    uint64_t last_txrx_ms;
    uint64_t last_led_write_ms;
    char led_status[16];
    char comm_version[64];
    int ns_raw_log_enabled;
    uint64_t mqtt_sub_msg_count;
    uint64_t spi_data_total_count;
    uint64_t last_stats_write_ms;
    uint64_t last_written_mqtt_sub_msg_count;
    uint64_t last_written_spi_data_total_count;
    struct gpiod_chip *power_gpio_chip;
    struct gpiod_line *power_gpio_line;
    int power_gpio_fd;
    int power_level;
    int print_version_and_exit;
    int print_help_and_exit;
    int invalid_arg_found;
} AppContext;

volatile sig_atomic_t g_running = 1;
static int g_debug_log_enabled = 0;

static void app_logf(FILE *stream, int level, const char *fmt, ...);
static void send_heartbeat(AppContext *ctx);
static void send_config_request(AppContext *ctx);
static void parse_runtime_args(AppContext *ctx, int argc, char *argv[]);

static int init_power_gpio(AppContext *ctx) {
    int level;
    if (ctx == NULL) {
        return -1;
    }
    ctx->power_level = 1;
    ctx->power_gpio_fd = -1;
    ctx->power_gpio_chip = gpiod_chip_open_by_name(POWER_GPIO_CHIP_NAME);
    if (ctx->power_gpio_chip == NULL) {
        app_logf(stderr, APP_LOG_WARN, "open %s failed, use default power=%d\n", POWER_GPIO_CHIP_NAME, ctx->power_level);
        return -1;
    }
    ctx->power_gpio_line = gpiod_chip_get_line(ctx->power_gpio_chip, POWER_GPIO_OFFSET);
    if (ctx->power_gpio_line == NULL) {
        app_logf(stderr, APP_LOG_WARN, "get power gpio offset=%d failed, use default power=%d\n", POWER_GPIO_OFFSET, ctx->power_level);
        gpiod_chip_close(ctx->power_gpio_chip);
        ctx->power_gpio_chip = NULL;
        return -1;
    }
    if (gpiod_line_request_both_edges_events(ctx->power_gpio_line, "mqtt_bridge_power") != 0) {
        app_logf(stderr, APP_LOG_WARN, "request power gpio both edge failed, use default power=%d\n", ctx->power_level);
        gpiod_line_release(ctx->power_gpio_line);
        gpiod_chip_close(ctx->power_gpio_chip);
        ctx->power_gpio_line = NULL;
        ctx->power_gpio_chip = NULL;
        return -1;
    }
    ctx->power_gpio_fd = gpiod_line_event_get_fd(ctx->power_gpio_line);
    if (ctx->power_gpio_fd < 0) {
        app_logf(stderr, APP_LOG_WARN, "get power gpio event fd failed, use default power=%d\n", ctx->power_level);
        gpiod_line_release(ctx->power_gpio_line);
        gpiod_chip_close(ctx->power_gpio_chip);
        ctx->power_gpio_line = NULL;
        ctx->power_gpio_chip = NULL;
        return -1;
    }
    level = gpiod_line_get_value(ctx->power_gpio_line);
    if (level >= 0) {
        ctx->power_level = level ? 1 : 0;
    }
    app_logf(stdout, APP_LOG_INFO, "power gpio init %s offset=%d level=%d\n", POWER_GPIO_CHIP_NAME, POWER_GPIO_OFFSET, ctx->power_level);
    return 0;
}

static int read_power_gpio_level(AppContext *ctx, int *out_power) {
    int level;
    if (ctx == NULL || out_power == NULL) {
        return -1;
    }
    if (ctx->power_gpio_line == NULL) {
        *out_power = ctx->power_level;
        return -1;
    }
    level = gpiod_line_get_value(ctx->power_gpio_line);
    if (level < 0) {
        *out_power = ctx->power_level;
        return -1;
    }
    ctx->power_level = level ? 1 : 0;
    *out_power = ctx->power_level;
    return 0;
}

static void close_power_gpio(AppContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    if (ctx->power_gpio_line != NULL) {
        gpiod_line_release(ctx->power_gpio_line);
        ctx->power_gpio_line = NULL;
    }
    if (ctx->power_gpio_chip != NULL) {
        gpiod_chip_close(ctx->power_gpio_chip);
        ctx->power_gpio_chip = NULL;
    }
    ctx->power_gpio_fd = -1;
}

static int get_default_route_iface(char *iface, size_t iface_len) {
    FILE *fp;
    char line[512];
    if (iface == NULL || iface_len == 0) {
        return -1;
    }
    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        return -1;
    }
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char ifname[64];
        unsigned long destination = 0;
        unsigned long gateway = 0;
        unsigned int flags = 0;
        unsigned long mask = 0;
        int matched = sscanf(line, "%63s %lx %lx %X %*s %*s %*s %lx", ifname, &destination, &gateway, &flags, &mask);
        if (matched >= 5 && destination == 0 && mask == 0) {
            strncpy(iface, ifname, iface_len - 1);
            iface[iface_len - 1] = '\0';
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

static uint32_t mac_msg_size_by_type(MacMsgType_e type) {
    switch (type) {
        case MSG_TYPE_NS_CONFIG_DOWN:
            return (uint32_t)sizeof(NsConfigDown_t);
        case MSG_TYPE_NS_DATA_DOWN:
            return (uint32_t)sizeof(NsDataDown_t);
        case MSG_TYPE_GW_DATA_UP:
            return (uint32_t)sizeof(GwBatchDataUp_t);
        case MSG_TYPE_GW_ALIVE_UP:
            return (uint32_t)sizeof(GwAliveUp_t);
        case MSG_TYPE_CONFIG_REQ:
            return (uint32_t)sizeof(ConfigReq_t);
        default:
            return 0;
    }
}

static uint32_t mac_msg_wire_len(const MacMsgBase_t *msg) {
    if (msg == NULL) {
        return 0;
    }
    if (msg->msg_type == MSG_TYPE_NS_DATA_DOWN) {
        const NsDataDown_t *down = (const NsDataDown_t *)msg;
        if (down->payload_len > MAX_PAYLOAD_LEN) {
            return 0;
        }
        return (uint32_t)(offsetof(NsDataDown_t, payload) + down->payload_len);
    }
    return mac_msg_size_by_type(msg->msg_type);
}

static void app_log_send_chunked(int level, const char *msg) {
    size_t len;
    size_t off;
    if (msg == NULL) {
        return;
    }
    len = strlen(msg);
    if (len == 0) {
        log_send(level, msg, 0);
        return;
    }
    off = 0;
    while (off < len) {
        size_t chunk = len - off;
        char part[APP_LOG_UDP_CHUNK_SIZE + 1];
        if (chunk > APP_LOG_UDP_CHUNK_SIZE) {
            chunk = APP_LOG_UDP_CHUNK_SIZE;
        }
        memcpy(part, msg + off, chunk);
        part[chunk] = '\0';
        log_send(level, part, 0);
        off += chunk;
    }
}

static void app_log_emit_buffer(FILE *stream, int level, const char *buf, int force_console, int force_udp) {
    if (buf == NULL) {
        return;
    }
    if (force_console || APP_CONSOLE_ENABLED(level)) {
        fputs(buf, stream);
    }
    if (force_udp || APP_UDP_ENABLED(level)) {
        app_log_send_chunked(level, buf);
    }
    if (APP_FILE_ENABLED(level)) {
        file_logger_write(MQTT_FILE_LOG, level >= APP_LOG_ERROR ? "ERROR" : (level >= APP_LOG_WARN ? "WARN" : "INFO"), "%s", buf);
    }
}

static void app_log_format_and_emit(FILE *stream, int level, int force_console, int force_udp, const char *fmt, va_list ap) {
    char fast_buf[APP_LOG_FAST_BUF_SIZE];
    va_list ap_copy;
    int n;
    if (!(force_console || APP_CONSOLE_ENABLED(level) || force_udp || APP_UDP_ENABLED(level) || APP_FILE_ENABLED(level))) {
        return;
    }
    va_copy(ap_copy, ap);
    n = vsnprintf(fast_buf, sizeof(fast_buf), fmt, ap);
    if (n < 0) {
        va_end(ap_copy);
        return;
    }
    if ((size_t)n < sizeof(fast_buf)) {
        app_log_emit_buffer(stream, level, fast_buf, force_console, force_udp);
        va_end(ap_copy);
        return;
    }
    {
        size_t need = (size_t)n + 1;
        char *full_buf = (char *)malloc(need);
        if (full_buf == NULL) {
            app_log_emit_buffer(stream, level, fast_buf, force_console, force_udp);
            va_end(ap_copy);
            return;
        }
        if (vsnprintf(full_buf, need, fmt, ap_copy) >= 0) {
            app_log_emit_buffer(stream, level, full_buf, force_console, force_udp);
        }
        free(full_buf);
    }
    va_end(ap_copy);
}

static __attribute__((unused)) void app_logf(FILE *stream, int level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    app_log_format_and_emit(stream, level, 0, 0, fmt, ap);
    va_end(ap);
}

static void app_log_ns_raw(AppContext *ctx, FILE *stream, const char *fmt, ...) {
    va_list ap;
    if (ctx == NULL || !ctx->ns_raw_log_enabled) {
        return;
    }
    va_start(ap, fmt);
    app_log_format_and_emit(stream, APP_LOG_INFO, 1, 1, fmt, ap);
    va_end(ap);
}

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000ULL) + ((uint64_t)tv.tv_usec / 1000ULL);
}

static int write_mqtt_stats_file_locked(const AppContext *ctx, uint64_t now_ms) {
    FILE *fp;
    if (ctx == NULL) {
        return -1;
    }
    fp = fopen(MQTT_STATS_TMP_FILE, "w");
    if (fp == NULL) {
        return -1;
    }
    if (fprintf(fp, "{\"mqtt_sub_msg_count\":%llu,\"spi_data_total_count\":%llu,\"ts_ms\":%llu}\n",
                (unsigned long long)ctx->mqtt_sub_msg_count,
                (unsigned long long)ctx->spi_data_total_count,
                (unsigned long long)now_ms) < 0) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    if (rename(MQTT_STATS_TMP_FILE, MQTT_STATS_FILE) != 0) {
        return -1;
    }
    return 0;
}

static void maybe_write_mqtt_stats_locked(AppContext *ctx, int force) {
    int changed;
    uint64_t now_ms;
    if (ctx == NULL) {
        return;
    }
    changed = (ctx->mqtt_sub_msg_count != ctx->last_written_mqtt_sub_msg_count) ||
              (ctx->spi_data_total_count != ctx->last_written_spi_data_total_count);
    if (!force && !changed) {
        return;
    }
    now_ms = get_time_ms();
    if (!force && ctx->last_stats_write_ms > 0 &&
        (now_ms - ctx->last_stats_write_ms) < MQTT_STATS_WRITE_INTERVAL_MS) {
        return;
    }
    if (write_mqtt_stats_file_locked(ctx, now_ms) == 0) {
        ctx->last_stats_write_ms = now_ms;
        ctx->last_written_mqtt_sub_msg_count = ctx->mqtt_sub_msg_count;
        ctx->last_written_spi_data_total_count = ctx->spi_data_total_count;
    } else {
        app_logf(stderr, APP_LOG_WARN, "write mqtt stats file failed errno=%d\n", errno);
    }
}

static void stats_record_mqtt_sub_msg(AppContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->mqtt_sub_msg_count++;
    maybe_write_mqtt_stats_locked(ctx, 0);
    pthread_mutex_unlock(&ctx->stats_lock);
}

static void stats_record_spi_data_total(AppContext *ctx, int total_count) {
    if (ctx == NULL || total_count <= 0) {
        return;
    }
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->spi_data_total_count += (uint64_t)total_count;
    maybe_write_mqtt_stats_locked(ctx, 0);
    pthread_mutex_unlock(&ctx->stats_lock);
}

static void stats_flush(AppContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    pthread_mutex_lock(&ctx->stats_lock);
    maybe_write_mqtt_stats_locked(ctx, 1);
    pthread_mutex_unlock(&ctx->stats_lock);
}

static int load_comm_version(const char *file_path, char *out_ver, size_t out_len) {
    FILE *fp;
    long file_size;
    size_t read_size;
    char *buf = NULL;
    struct json_object *parsed = NULL;
    struct json_object *obj = NULL;
    const char *ver = NULL;
    if (file_path == NULL || out_ver == NULL || out_len == 0) {
        return -1;
    }
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    file_size = ftell(fp);
    if (file_size <= 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    buf = (char *)malloc((size_t)file_size + 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }
    read_size = fread(buf, 1, (size_t)file_size, fp);
    buf[read_size] = '\0';
    fclose(fp);
    parsed = json_tokener_parse(buf);
    free(buf);
    if (parsed == NULL) {
        return -1;
    }
    if (!json_object_object_get_ex(parsed, "comm_version", &obj)) {
        json_object_put(parsed);
        return -1;
    }
    ver = json_object_get_string(obj);
    if (ver == NULL || ver[0] == '\0') {
        json_object_put(parsed);
        return -1;
    }
    strncpy(out_ver, ver, out_len - 1);
    out_ver[out_len - 1] = '\0';
    json_object_put(parsed);
    return 0;
}

static void parse_runtime_args(AppContext *ctx, int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "raw") == 0) {
            ctx->ns_raw_log_enabled = 1;
            g_debug_log_enabled = 1;
        } else if (strcmp(argv[i], "ver") == 0) {
            ctx->print_version_and_exit = 1;
        } else if (strcmp(argv[i], "help") == 0) {
            ctx->print_help_and_exit = 1;
        } else if (strncmp(argv[i], "logip=", 6) == 0) {
            const char *ip = argv[i] + 6;
            if (ip[0] == '\0') {
                ctx->invalid_arg_found = 1;
            } else {
                setenv("LOG_UDP_ADDR", ip, 1);
            }
        } else if (strncmp(argv[i], "logport=", 8) == 0) {
            const char *port = argv[i] + 8;
            if (port[0] == '\0') {
                ctx->invalid_arg_found = 1;
            } else {
                setenv("LOG_UDP_PORT", port, 1);
            }
        } else {
            ctx->invalid_arg_found = 1;
        }
    }
}

static int write_mqtt_led_state_file(const char *status, uint64_t ts_ms) {
    FILE *fp;
    if (status == NULL) {
        return -1;
    }
    if (mkdir(LED_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    fp = fopen(MQTT_LED_TMP_FILE, "w");
    if (fp == NULL) {
        return -1;
    }
    if (fprintf(fp, "STATUS=%s\nTS_MS=%llu\n", status, (unsigned long long)ts_ms) < 0) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    if (rename(MQTT_LED_TMP_FILE, MQTT_LED_STATE_FILE) != 0) {
        return -1;
    }
    return 0;
}

static void mqtt_led_write_locked(AppContext *ctx, const char *status, uint64_t now_ms) {
    if (ctx == NULL || status == NULL) {
        return;
    }
    if (write_mqtt_led_state_file(status, now_ms) != 0) {
        app_logf(stderr, APP_LOG_WARN, "write mqtt led state failed status=%s errno=%d\n", status, errno);
        return;
    }
    strncpy(ctx->led_status, status, sizeof(ctx->led_status) - 1);
    ctx->led_status[sizeof(ctx->led_status) - 1] = '\0';
    ctx->last_led_write_ms = now_ms;
    if (strcmp(status, "txrx") == 0) {
        ctx->last_txrx_ms = now_ms;
    }
}

static void mqtt_led_mark_txrx(AppContext *ctx) {
    uint64_t now_ms = get_time_ms();
    pthread_mutex_lock(&ctx->led_lock);
    mqtt_led_write_locked(ctx, "txrx", now_ms);
    pthread_mutex_unlock(&ctx->led_lock);
}

static void mqtt_led_tick_connected(AppContext *ctx) {
    uint64_t now_ms = get_time_ms();
    uint64_t since_txrx;
    uint64_t since_write;
    pthread_mutex_lock(&ctx->led_lock);
    since_txrx = (now_ms > ctx->last_txrx_ms) ? (now_ms - ctx->last_txrx_ms) : 0;
    since_write = (now_ms > ctx->last_led_write_ms) ? (now_ms - ctx->last_led_write_ms) : 0;
    if (since_txrx >= MQTT_LED_TXRX_TIMEOUT_MS) {
        if ((strcmp(ctx->led_status, "connected") != 0) || (since_write >= MQTT_LED_TXRX_TIMEOUT_MS)) {
            mqtt_led_write_locked(ctx, "connected", now_ms);
        }
    }
    pthread_mutex_unlock(&ctx->led_lock);
}

static void mqtt_led_init_connected(AppContext *ctx) {
    uint64_t now_ms = get_time_ms();
    pthread_mutex_lock(&ctx->led_lock);
    ctx->last_txrx_ms = now_ms;
    mqtt_led_write_locked(ctx, "connected", now_ms);
    pthread_mutex_unlock(&ctx->led_lock);
}

static void handle_signal(int signum) {
    (void)signum;
    g_running = 0;
}

static int ipc_open_attach_then_create(const ipc_smp_cfg_t *base_cfg, ipc_smp_handle_t **out) {
    ipc_smp_cfg_t cfg = *base_cfg;
    int rc;
    cfg.create = 0;
    rc = ipc_smp_channel_init(&cfg, out);
    if (rc == IPC_SMP_OK) {
        return IPC_SMP_OK;
    }
    cfg.create = 1;
    return ipc_smp_channel_init(&cfg, out);
}

static int init_ipc_channels(AppContext *ctx) {
    ipc_smp_cfg_t down_cfg;
    ipc_smp_cfg_t up_cfg;
    memset(&down_cfg, 0, sizeof(down_cfg));
    memset(&up_cfg, 0, sizeof(up_cfg));

    down_cfg.shm_name = IPC_CH_MQTT_TO_SPI;
    down_cfg.ring_size = IPC_RING_SIZE;
    down_cfg.max_msg_size = IPC_MAX_MSG_SIZE;
    down_cfg.batch_count = IPC_BATCH_COUNT;
    down_cfg.flush_timeout_ms = IPC_FLUSH_TIMEOUT_MS;
    down_cfg.role = IPC_SMP_ROLE_PRODUCER;

    up_cfg.shm_name = IPC_CH_SPI_TO_MQTT;
    up_cfg.ring_size = IPC_RING_SIZE;
    up_cfg.max_msg_size = IPC_MAX_MSG_SIZE;
    up_cfg.batch_count = IPC_BATCH_COUNT;
    up_cfg.flush_timeout_ms = IPC_FLUSH_TIMEOUT_MS;
    up_cfg.role = IPC_SMP_ROLE_CONSUMER;

    if (ipc_open_attach_then_create(&down_cfg, &ctx->ch_mqtt_to_spi) != IPC_SMP_OK) {
        app_logf(stderr, APP_LOG_ERROR, "init channel %s failed\n", IPC_CH_MQTT_TO_SPI);
        return -1;
    }

    if (ipc_open_attach_then_create(&up_cfg, &ctx->ch_spi_to_mqtt) != IPC_SMP_OK) {
        app_logf(stderr, APP_LOG_ERROR, "init channel %s failed\n", IPC_CH_SPI_TO_MQTT);
        return -1;
    }

    return 0;
}

static int send_mqtt_payload_to_spi(AppContext *ctx, const void *buf, uint32_t len) {
    int i;
    int rc = IPC_SMP_ERR_SYS;
    for (i = 0; i < 3; i++) {
        pthread_mutex_lock(&ctx->tx_lock);
        rc = ipc_smp_send(ctx->ch_mqtt_to_spi, IPC_MSG_TYPE_MQTT_TO_SPI, buf, len, 0);
        if (rc == IPC_SMP_OK) {
            ipc_smp_periodic_check(ctx->ch_mqtt_to_spi);
            pthread_mutex_unlock(&ctx->tx_lock);
            return IPC_SMP_OK;
        }
        pthread_mutex_unlock(&ctx->tx_lock);
        if (rc != IPC_SMP_ERR_FULL) {
            break;
        }
        usleep(2000);
    }
    return rc;
}

static GwBatchDataUp_t g_spi_batch_rx;

static void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    AppContext *ctx = (AppContext *)obj;
    mqtt_status_connack(rc);
    if (rc != 0) {
        app_logf(stderr, APP_LOG_ERROR, "mqtt connect failed rc=%d\n", rc);
        return;
    }

    if (strlen(ctx->config.sub_topic) == 0) {
        mqtt_status_fault("subscription_topic_missing", MOSQ_ERR_INVAL);
        return;
    }

    int mid = -1;
    rc = mosquitto_subscribe(mosq, &mid, ctx->config.sub_topic, ctx->config.qos);
    mqtt_status_subscribe(mid, rc);
    if (rc != MOSQ_ERR_SUCCESS) {
        app_logf(stderr, APP_LOG_ERROR, "subscribe %s failed: %s\n", ctx->config.sub_topic, mosquitto_strerror(rc));
    } else {
        app_logf(stdout, APP_LOG_INFO, "subscribed: %s\n", ctx->config.sub_topic);
    }

    send_heartbeat(ctx);
    ctx->last_heartbeat_time = time(NULL);
    ctx->config_ack_received = 0;
    ctx->config_request_triggered = 0;
}

static void on_subscribe(struct mosquitto *mosq, void *obj, int mid,
                         int qos_count, const int *granted_qos) {
    (void)mosq;
    (void)obj;
    mqtt_status_suback(mid, qos_count, granted_qos);
}

static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    AppContext *ctx = (AppContext *)obj;
    (void)mosq;
    if (message == NULL || message->payload == NULL || message->payloadlen <= 0) {
        return;
    }
    stats_record_mqtt_sub_msg(ctx);

    if ((uint32_t)message->payloadlen > IPC_MAX_MSG_SIZE) {
        app_logf(stderr, APP_LOG_WARN, "drop mqtt payload too large: %d\n", message->payloadlen);
        return;
    }

    if (APP_LOG_ENABLED(APP_LOG_DEBUG)) {
        app_logf(stdout, APP_LOG_DEBUG, "mqtt raw len=%d topic=%s\n", message->payloadlen, message->topic);
        for (int i = 0; i < message->payloadlen; i++) {
            app_logf(stdout, APP_LOG_DEBUG, "%02X ", ((const unsigned char *)message->payload)[i]);
        }
        app_logf(stdout, APP_LOG_DEBUG, "\n");
        app_logf(stdout, APP_LOG_DEBUG, "mqtt raw str: %.*s\n", message->payloadlen, (const char *)message->payload);
    }

    char *json_buf = (char *)malloc((size_t)message->payloadlen + 1);
    if (json_buf == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "alloc json_buf failed len=%d\n", message->payloadlen);
        return;
    }
    memcpy(json_buf, message->payload, (size_t)message->payloadlen);
    json_buf[message->payloadlen] = '\0';

    struct json_object *scan_json = json_tokener_parse(json_buf);
    int scan_handled = scan_json != NULL && ns_scan_manager_handle(ctx->scan_manager, scan_json);
    json_object_put(scan_json);
    if (scan_handled) {
        free(json_buf);
        return;
    }

    MacMsgBase_t *mac_msg = parse_mac_msg(json_buf);
    if (mac_msg == NULL) {
        app_logf(stderr, APP_LOG_WARN, "drop mqtt payload parse failed topic=%s len=%d\n", message->topic, message->payloadlen);
        free(json_buf);
        return;
    }
    if (mac_msg->msg_type == MSG_TYPE_NS_CONFIG_DOWN) {
        app_log_ns_raw(ctx, stdout, "ns config down raw json: %s\n", json_buf);
    } else if (mac_msg->msg_type == MSG_TYPE_NS_DATA_DOWN) {
        app_log_ns_raw(ctx, stdout, "ns data down raw json: %s\n", json_buf);
    }
    free(json_buf);

    uint32_t struct_len = mac_msg_wire_len(mac_msg);
    if (struct_len == 0) {
        app_logf(stderr, APP_LOG_WARN, "drop mqtt payload unsupported msg_type=%d\n", (int)mac_msg->msg_type);
        free_mac_msg(mac_msg);
        return;
    }
    if (!ctx->config_ack_received && mac_msg->msg_type == MSG_TYPE_NS_CONFIG_DOWN) {
        ctx->config_ack_received = 1;
        app_logf(stdout, APP_LOG_INFO, "config response received\n");
    }

    int rc = send_mqtt_payload_to_spi(ctx, mac_msg, struct_len);
    print_mac_msg(mac_msg);
    if (rc != IPC_SMP_OK) {
        app_logf(stderr, APP_LOG_ERROR, "send mqtt->spi failed rc=%d topic=%s type=%d len=%u\n", rc, message->topic, (int)mac_msg->msg_type, (unsigned)struct_len);
    } else {
        app_logf(stdout, APP_LOG_INFO, "sent mqtt->spi struct type=%d len=%u\n", (int)mac_msg->msg_type, (unsigned)struct_len);
        mqtt_led_mark_txrx(ctx);
    }
    free_mac_msg(mac_msg);
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    (void)mosq;
    (void)obj;
    mqtt_status_disconnect(rc);
    app_logf(stderr, APP_LOG_WARN, "mqtt disconnected rc=%d\n", rc);
}

static void process_spi_uplink(AppContext *ctx) {
    while (g_running) {
        ipc_smp_msg_t msg;
        int rc = ipc_smp_recv(ctx->ch_spi_to_mqtt, &msg, 0);
        if (rc == IPC_SMP_ERR_EMPTY) {
            break;
        }
        if (rc != IPC_SMP_OK) {
            app_logf(stderr, APP_LOG_ERROR, "ipc recv spi->mqtt failed rc=%d\n", rc);
            break;
        }

        if (strlen(ctx->config.pub_topic) > 0) {
            if (msg.len < sizeof(MacMsgType_e)) {
                app_logf(stderr, APP_LOG_WARN, "drop spi payload invalid len=%u\n", (unsigned)msg.len);
            } else {
                MacMsgType_e msg_type = MSG_TYPE_UNKNOWN;
                memcpy(&msg_type, msg.payload, sizeof(MacMsgType_e));
                if (msg_type == MSG_TYPE_GW_DATA_UP) {
                    size_t head_len = offsetof(GwBatchDataUp_t, data_list);
                    if (msg.len < head_len) {
                        app_logf(stderr, APP_LOG_WARN, "drop spi payload gw_batch len=%u need_head=%u\n", (unsigned)msg.len, (unsigned)head_len);
                    } else {
                        int total_count = 0;
                        memcpy(&total_count, (const uint8_t *)msg.payload + offsetof(GwBatchDataUp_t, total_count), sizeof(int));
                        if (total_count <= 0 || total_count > MAX_UP_USER_NUM) {
                            app_logf(stderr, APP_LOG_WARN, "drop spi payload gw_batch invalid total_count=%d\n", total_count);
                        } else {
                            size_t required_len = head_len + ((size_t)total_count * sizeof(GwDataUp_t));
                            if ((size_t)msg.len < required_len) {
                                app_logf(stderr, APP_LOG_WARN, "drop spi payload gw_batch len=%u need=%u total_count=%d\n", (unsigned)msg.len, (unsigned)required_len, total_count);
                            } else {
                                stats_record_spi_data_total(ctx, total_count);
                                memset(&g_spi_batch_rx, 0, sizeof(g_spi_batch_rx));
                                memcpy(&g_spi_batch_rx, msg.payload, required_len);
                                print_mac_msg((MacMsgBase_t *)&g_spi_batch_rx);
                                int has_pub_success = 0;
                                for (int start = 0; start < total_count; start += 64) {
                                    int chunk = total_count - start;
                                    if (chunk > 64) {
                                        chunk = 64;
                                    }
                                    char *json_out = pack_gw_batch_data_up_msg(&g_spi_batch_rx, start, chunk);
                                    if (json_out == NULL) {
                                        app_logf(stderr, APP_LOG_ERROR, "pack gw_batch_data_up to json failed start=%d count=%d\n", start, chunk);
                                    } else {
                                        app_log_ns_raw(ctx, stdout, "packed spi gw_batch json start=%d count=%d: %s\n", start, chunk, json_out);
                                        int pub_rc = mosquitto_publish(ctx->mosq, NULL, ctx->config.pub_topic, (int)strlen(json_out), json_out, ctx->config.qos, false);
                                        if (pub_rc != MOSQ_ERR_SUCCESS) {
                                            app_logf(stderr, APP_LOG_ERROR, "publish spi batch data failed: %s\n", mosquitto_strerror(pub_rc));
                                        } else {
                                            app_logf(stdout, APP_LOG_INFO, "published spi->mqtt batch json start=%d count=%d\n", start, chunk);
                                            has_pub_success = 1;
                                        }
                                        free_json_str(json_out);
                                    }
                                }
                                if (has_pub_success) {
                                    mqtt_led_mark_txrx(ctx);
                                }
                            }
                        }
                    }
                } else if (msg_type == MSG_TYPE_GW_ALIVE_UP) {
                    if (msg.len < sizeof(GwAliveUp_t)) {
                        app_logf(stderr, APP_LOG_WARN, "drop spi payload gw_alive_up len=%u need=%u\n", (unsigned)msg.len, (unsigned)sizeof(GwAliveUp_t));
                    } else {
                        GwAliveUp_t gw_alive;
                        memcpy(&gw_alive, msg.payload, sizeof(GwAliveUp_t));
                        print_mac_msg((MacMsgBase_t *)&gw_alive);
                        char *json_out = pack_gw_alive_up_msg(&gw_alive);
                        if (json_out == NULL) {
                            app_logf(stderr, APP_LOG_ERROR, "pack gw_alive_up to json failed\n");
                        } else {
                            app_log_ns_raw(ctx, stdout, "packed spi gw_alive_up json: %s\n", json_out);
                            int pub_rc = mosquitto_publish(ctx->mosq, NULL, ctx->config.pub_topic, (int)strlen(json_out), json_out, ctx->config.qos, false);
                            if (pub_rc != MOSQ_ERR_SUCCESS) {
                                app_logf(stderr, APP_LOG_ERROR, "publish spi alive failed: %s\n", mosquitto_strerror(pub_rc));
                            } else {
                                app_logf(stdout, APP_LOG_INFO, "published spi->mqtt alive: %s\n", json_out);
                                mqtt_led_mark_txrx(ctx);
                            }
                            free_json_str(json_out);
                        }
                    }
                } else if (msg_type == MSG_TYPE_CONFIG_REQ) {
                    if (msg.len < sizeof(ConfigReq_t)) {
                        app_logf(stderr, APP_LOG_WARN, "drop spi payload config_req len=%u need=%u\n", (unsigned)msg.len, (unsigned)sizeof(ConfigReq_t));
                    } else {
                        ConfigReq_t req;
                        memcpy(&req, msg.payload, sizeof(ConfigReq_t));
                        if (req.req_data_type == 1) {
                            ctx->config_request_triggered = 1;
                            ctx->config_ack_received = 0;
                            send_config_request(ctx);
                            ctx->last_config_req_time = time(NULL);
                            app_logf(stdout, APP_LOG_INFO, "config request triggered by spi struct\n");
                        } else {
                            app_logf(stdout, APP_LOG_INFO, "ignore config_req req_data_type=%d\n", req.req_data_type);
                        }
                    }
                } else {
                    app_logf(stderr, APP_LOG_WARN, "drop spi payload unsupported msg_type=%d len=%u\n", (int)msg_type, (unsigned)msg.len);
                }
            }
        }

        rc = ipc_smp_ack(ctx->ch_spi_to_mqtt, msg.seq);
        if (rc != IPC_SMP_OK) {
            app_logf(stderr, APP_LOG_ERROR, "ipc ack failed seq=%u rc=%d\n", msg.seq, rc);
            break;
        }
    }
}

static void send_heartbeat(AppContext *ctx) {
    char topic[256];
    char net_name[64];
    GpsInfo gps;
    GwAliveUp_t alive_msg;
    char *payload_str;
    int rc;
    int power = 1;

    if (ctx->config.pub_topic[0] == '\0') {
        return;
    }

    snprintf(topic, sizeof(topic), "%s", ctx->config.pub_topic);
    memset(&alive_msg, 0, sizeof(alive_msg));
    alive_msg.msg_type = MSG_TYPE_GW_ALIVE_UP;
    strncpy(alive_msg.ver, ctx->comm_version, sizeof(alive_msg.ver) - 1);
    alive_msg.uptime = (int64_t)time(NULL);
    if (get_default_route_iface(net_name, sizeof(net_name)) == 0) {
        strncpy(alive_msg.net, net_name, sizeof(alive_msg.net) - 1);
    } else {
        strncpy(alive_msg.net, "unknown", sizeof(alive_msg.net) - 1);
    }
    if (read_power_gpio_level(ctx, &power) != 0) {
        power = ctx->power_level;
    }
    alive_msg.power = power;
    if (gps_info_load(GPS_INFO_FILE_PATH, &gps) != 0) {
        (void)gps_info_load(BDGPS_FILE_PATH, &gps);
    }

    payload_str = pack_gw_alive_up_msg_with_gps_metrics(&alive_msg, gps.lon, gps.lat,
        gps.status, gps.count, gps.avgsnr);
    if (payload_str != NULL) {
        rc = mosquitto_publish(ctx->mosq, NULL, topic, (int)strlen(payload_str), payload_str, ctx->config.qos, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            app_logf(stderr, APP_LOG_ERROR, "heartbeat publish failed: %s\n", mosquitto_strerror(rc));
        } else {
            app_logf(stdout, APP_LOG_INFO, "heartbeat sent to %s: %s\n", topic, payload_str);
        }
        free_json_str(payload_str);
    } else {
        app_logf(stderr, APP_LOG_ERROR, "pack heartbeat json failed\n");
    }
}

static void send_config_request(AppContext *ctx) {
    char topic[256];
    struct json_object *jobj = json_object_new_object();
    struct json_object *get_obj = json_object_new_object();
    const char *payload_str;
    int rc;

    if (ctx->config.pub_topic[0] == '\0') {
        json_object_put(jobj);
        json_object_put(get_obj);
        return;
    }

    snprintf(topic, sizeof(topic), "%s", ctx->config.pub_topic);
    json_object_object_add(get_obj, "config", json_object_new_boolean(1));
    json_object_object_add(jobj, "get", get_obj);

    payload_str = json_object_to_json_string(jobj);
    if (payload_str) {
        rc = mosquitto_publish(ctx->mosq, NULL, topic, strlen(payload_str), payload_str, ctx->config.qos, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            app_logf(stderr, APP_LOG_ERROR, "config request publish failed: %s\n", mosquitto_strerror(rc));
        } else {
            app_logf(stdout, APP_LOG_INFO, "config request sent to %s: %s\n", topic, payload_str);
        }
    }

    json_object_put(jobj);
}

static int publish_scan(void *context, const char *payload, int qos) {
    AppContext *ctx = (AppContext *)context;
    if (ctx == NULL || ctx->mosq == NULL || ctx->config.pub_topic[0] == '\0') {
        return -1;
    }
    return mosquitto_publish(ctx->mosq, NULL, ctx->config.pub_topic, (int)strlen(payload), payload, qos, false);
}

int main(int argc, char *argv[]) {
    AppContext ctx;
    int rc;
    int exit_code = 1;
    int mosquitto_initialized = 0;
    int loop_started = 0;
    int epfd = -1;
    int notify_fd;
    struct epoll_event event;
    struct epoll_event events[4];
    memset(&ctx, 0, sizeof(ctx));
    ctx.power_gpio_fd = -1;
    ctx.power_level = 1;
    ctx.start_time = time(NULL);
    strncpy(ctx.comm_version, "unknown", sizeof(ctx.comm_version) - 1);
    parse_runtime_args(&ctx, argc, argv);
    if (ctx.print_version_and_exit) {
        printf("%s\n", APP_VERSION);
        return 0;
    }
    if (ctx.print_help_and_exit) {
        printf("supported args: raw ver help logip=<ip> logport=<port>\n");
        return 0;
    }
    if (ctx.invalid_arg_found) {
        fprintf(stderr, "invalid args, supported args: raw ver help logip=<ip> logport=<port>\n");
        return 1;
    }
    set_mac_msg_print_enabled(ctx.ns_raw_log_enabled);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (mqtt_status_init(LED_DIR) != 0) {
        app_logf(stderr, APP_LOG_WARN, "mqtt status observer init failed; LED status unavailable\n");
    }
    if (load_config(CONFIG_FILE_PATH, &ctx.config) != 0) {
        app_logf(stderr, APP_LOG_ERROR, "load %s failed\n", CONFIG_FILE_PATH);
        mqtt_status_fault("config_load_failed", -1);
        mqtt_status_stop();
        return 1;
    }
    if (load_comm_version(DEVICE_VERSION_FILE_PATH, ctx.comm_version, sizeof(ctx.comm_version)) != 0) {
        app_logf(stderr, APP_LOG_WARN, "load %s comm_version failed, use default=%s\n", DEVICE_VERSION_FILE_PATH, ctx.comm_version);
    } else {
        app_logf(stdout, APP_LOG_INFO, "comm_version=%s\n", ctx.comm_version);
    }

    app_logf(stdout, APP_LOG_INFO, "mqtt=%s:%d gateway=%s net=%s\n", ctx.config.mqtt_host, ctx.config.mqtt_port, ctx.config.gateway_id, get_net_interface_name(ctx.config.net_interface));
    app_logf(stdout, APP_LOG_INFO, "sub_topic=%s pub_topic=%s qos=%d\n", ctx.config.sub_topic, ctx.config.pub_topic, ctx.config.qos);
    app_logf(stdout, APP_LOG_INFO, "ns raw udp log=%s (use arg: raw)\n", ctx.ns_raw_log_enabled ? "on" : "off");
    app_logf(stdout, APP_LOG_INFO, "udp log target=%s:%s (args: logip=<ip> logport=<port>)\n",
             getenv("LOG_UDP_ADDR") ? getenv("LOG_UDP_ADDR") : "127.0.0.1",
             getenv("LOG_UDP_PORT") ? getenv("LOG_UDP_PORT") : "5514");

    pthread_mutex_init(&ctx.tx_lock, NULL);
    pthread_mutex_init(&ctx.led_lock, NULL);
    pthread_mutex_init(&ctx.stats_lock, NULL);

    if (init_power_gpio(&ctx) != 0) {
        app_logf(stderr, APP_LOG_WARN, "power gpio monitor disabled, heartbeat power fallback=%d\n", ctx.power_level);
    }

    if (init_ipc_channels(&ctx) != 0) {
        mqtt_status_fault("ipc_init_failed", -1);
        goto cleanup;
    }

    notify_fd = ipc_smp_get_eventfd(ctx.ch_spi_to_mqtt);
    if (notify_fd < 0) {
        app_logf(stderr, APP_LOG_ERROR, "get ipc notify fd failed\n");
        mqtt_status_fault("ipc_notify_fd_failed", notify_fd);
        goto cleanup;
    }

    epfd = epoll_create1(0);
    if (epfd < 0) {
        int error = errno;
        app_logf(stderr, APP_LOG_ERROR, "epoll_create1 failed errno=%d\n", error);
        mqtt_status_fault("epoll_create_failed", error);
        goto cleanup;
    }

    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = notify_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &event) != 0) {
        int error = errno;
        app_logf(stderr, APP_LOG_ERROR, "epoll_ctl add notify fd failed errno=%d\n", error);
        mqtt_status_fault("ipc_epoll_add_failed", error);
        goto cleanup;
    }
    if (ctx.power_gpio_fd >= 0) {
        memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.fd = ctx.power_gpio_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ctx.power_gpio_fd, &event) != 0) {
            app_logf(stderr, APP_LOG_WARN, "epoll_ctl add power gpio fd failed errno=%d\n", errno);
            close_power_gpio(&ctx);
        }
    }

    rc = mosquitto_lib_init();
    if (rc != MOSQ_ERR_SUCCESS) {
        app_logf(stderr, APP_LOG_ERROR, "mosquitto_lib_init failed\n");
        mqtt_status_fault("mosquitto_init_failed", rc);
        goto cleanup;
    }
    mosquitto_initialized = 1;

    ctx.mosq = mosquitto_new(ctx.config.gateway_id, true, &ctx);
    if (ctx.mosq == NULL) {
        int error = errno;
        app_logf(stderr, APP_LOG_ERROR, "mosquitto_new failed\n");
        mqtt_status_fault("mosquitto_new_failed", error);
        goto cleanup;
    }

    if (strlen(ctx.config.mqtt_username) > 0) {
        rc = mosquitto_username_pw_set(ctx.mosq, ctx.config.mqtt_username, ctx.config.mqtt_password);
        if (rc != MOSQ_ERR_SUCCESS) {
            app_logf(stderr, APP_LOG_ERROR, "set mqtt username/password failed: %s\n", mosquitto_strerror(rc));
            mqtt_status_fault("credentials_setup_failed", rc);
            goto cleanup;
        }
    }

    ctx.scan_manager = ns_scan_manager_open("/userdata/mqtt_scan", "/tmp/data_collect.sock", publish_scan, &ctx);
    if (ctx.scan_manager == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "scan journal initialization failed\n");
        mqtt_status_fault("scan_init_failed", -1);
        goto cleanup;
    }

    mosquitto_connect_callback_set(ctx.mosq, on_connect);
    mosquitto_subscribe_callback_set(ctx.mosq, on_subscribe);
    mosquitto_message_callback_set(ctx.mosq, on_message);
    mosquitto_disconnect_callback_set(ctx.mosq, on_disconnect);

    rc = mosquitto_connect(ctx.mosq, ctx.config.mqtt_host, ctx.config.mqtt_port, ctx.config.heartbeat_interval * 2);
    if (rc != MOSQ_ERR_SUCCESS) {
        app_logf(stderr, APP_LOG_ERROR, "mqtt connect failed: %s\n", mosquitto_strerror(rc));
        mqtt_status_connect_error(rc);
    }

    rc = mosquitto_loop_start(ctx.mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        app_logf(stderr, APP_LOG_ERROR, "mosquitto_loop_start failed: %s\n", mosquitto_strerror(rc));
        mqtt_status_fault("mqtt_loop_start_failed", rc);
        goto cleanup;
    }
    loop_started = 1;
    mqtt_led_init_connected(&ctx);

    exit_code = 0;
    while (g_running) {
        mqtt_status_tick();
        int n = epoll_wait(epfd, events, 4, 100);
        int i;

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            int error = errno;
            app_logf(stderr, APP_LOG_ERROR, "epoll_wait failed errno=%d\n", error);
            mqtt_status_fault("epoll_wait_failed", error);
            exit_code = 1;
            break;
        }

        for (i = 0; i < n; i++) {
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                if (events[i].data.fd == notify_fd) {
                    app_logf(stderr, APP_LOG_ERROR, "epoll error/hup on notify_fd, exiting\n");
                } else if (ctx.power_gpio_fd >= 0 && events[i].data.fd == ctx.power_gpio_fd) {
                    app_logf(stderr, APP_LOG_ERROR, "epoll error/hup on power_gpio_fd, exiting\n");
                } else {
                    app_logf(stderr, APP_LOG_ERROR, "epoll error/hup on unknown fd=%d, exiting\n", events[i].data.fd);
                }
                g_running = 0;
                mqtt_status_fault("epoll_fd_error", (int)events[i].events);
                exit_code = 1;
                goto cleanup;
            }

            if ((events[i].events & EPOLLIN) && events[i].data.fd == notify_fd) {
                if (ipc_smp_clear_notify(ctx.ch_spi_to_mqtt) != IPC_SMP_OK) {
                    app_logf(stderr, APP_LOG_WARN, "ipc clear notify failed\n");
                }
                process_spi_uplink(&ctx);
            } else if ((events[i].events & EPOLLIN) && ctx.power_gpio_fd >= 0 && events[i].data.fd == ctx.power_gpio_fd) {
                struct gpiod_line_event gpio_event;
                if (gpiod_line_event_read(ctx.power_gpio_line, &gpio_event) == 0) {
                    if (gpio_event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
                        ctx.power_level = 1;
                    } else if (gpio_event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
                        ctx.power_level = 0;
                    } else {
                        (void)read_power_gpio_level(&ctx, &ctx.power_level);
                    }

                    static uint64_t last_gpio_hb_ms = 0;
                    static int last_throttled_power = -1;
                    uint64_t now_ms = get_time_ms();
                    // 增加 200ms 的软件去抖限频，防止毛刺导致心跳风暴
                    if (now_ms - last_gpio_hb_ms > 200) {
                        send_heartbeat(&ctx);
                        ctx.last_heartbeat_time = time(NULL);
                        last_gpio_hb_ms = now_ms;
                        last_throttled_power = -1;
                        app_logf(stdout, APP_LOG_INFO, "power gpio changed, heartbeat sent power=%d\n", ctx.power_level);
                    } else {
                        if (last_throttled_power != ctx.power_level) {
                            app_logf(stdout, APP_LOG_INFO, "power gpio changed to %d, heartbeat throttled\n", ctx.power_level);
                            last_throttled_power = ctx.power_level;
                        }
                    }
                } else {
                    app_logf(stderr, APP_LOG_WARN, "read power gpio event failed errno=%d\n", errno);
                }
            }
        }

        pthread_mutex_lock(&ctx.tx_lock);
        ipc_smp_periodic_check(ctx.ch_mqtt_to_spi);
        pthread_mutex_unlock(&ctx.tx_lock);

        ns_scan_manager_tick(ctx.scan_manager);

        time_t now = time(NULL);
        if (difftime(now, ctx.last_heartbeat_time) >= ctx.config.heartbeat_interval) {
            send_heartbeat(&ctx);
            ctx.last_heartbeat_time = now;
        }

        if (ctx.config_request_triggered && !ctx.config_ack_received && difftime(now, ctx.last_config_req_time) >= 10) {
            send_config_request(&ctx);
            ctx.last_config_req_time = now;
        }
        mqtt_led_tick_connected(&ctx);
    }

cleanup:
    g_running = 0;
    /* Revoke readiness before teardown; late callbacks cannot revive it. */
    mqtt_status_stop();
    stats_flush(&ctx);

    if (ctx.mosq != NULL) {
        if (loop_started) mosquitto_loop_stop(ctx.mosq, true);
        mosquitto_destroy(ctx.mosq);
    }
    if (mosquitto_initialized) mosquitto_lib_cleanup();
    ns_scan_manager_close(ctx.scan_manager);

    if (epfd >= 0) {
        close(epfd);
    }
    close_power_gpio(&ctx);
    if (ctx.ch_spi_to_mqtt != NULL) {
        ipc_smp_channel_close(ctx.ch_spi_to_mqtt);
    }
    if (ctx.ch_mqtt_to_spi != NULL) {
        ipc_smp_channel_close(ctx.ch_mqtt_to_spi);
    }

    pthread_mutex_destroy(&ctx.tx_lock);
    pthread_mutex_destroy(&ctx.led_lock);
    pthread_mutex_destroy(&ctx.stats_lock);
    return exit_code;
}
