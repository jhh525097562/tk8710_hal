#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <json-c/json.h>
#include "config.h"
#include "log_udp.h"
#include "file_logger.h"

#define APP_LOG_DEBUG 0
#define APP_LOG_INFO 1
#define APP_LOG_WARN 2
#define APP_LOG_ERROR 3
#define APP_LOG_BUF_SIZE 65536
#define APP_LOG_FAST_BUF_SIZE 4096
#define APP_LOG_UDP_CHUNK_SIZE 3800
#define APP_LOG_MIN_LEVEL APP_LOG_INFO
#define APP_LOG_ENABLED(level) ((level) >= APP_LOG_MIN_LEVEL)
#define MQTT_FILE_LOG "/var/log/mqtt_bridge.log"

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

static void app_log_format_and_emit(FILE *stream, int level, const char *fmt, va_list ap) {
    char fast_buf[APP_LOG_FAST_BUF_SIZE];
    va_list ap_copy;
    int n;
    va_copy(ap_copy, ap);
    n = vsnprintf(fast_buf, sizeof(fast_buf), fmt, ap);
    if (n < 0) {
        va_end(ap_copy);
        return;
    }
    if ((size_t)n < sizeof(fast_buf)) {
        fputs(fast_buf, stream);
        app_log_send_chunked(level, fast_buf);
        file_logger_write(MQTT_FILE_LOG, level >= APP_LOG_ERROR ? "ERROR" : (level >= APP_LOG_WARN ? "WARN" : "INFO"), "%s", fast_buf);
        va_end(ap_copy);
        return;
    }
    {
        size_t need = (size_t)n + 1;
        char *full_buf = (char *)malloc(need);
        if (full_buf == NULL) {
            fputs(fast_buf, stream);
            app_log_send_chunked(level, fast_buf);
            file_logger_write(MQTT_FILE_LOG, level >= APP_LOG_ERROR ? "ERROR" : (level >= APP_LOG_WARN ? "WARN" : "INFO"), "%s", fast_buf);
            va_end(ap_copy);
            return;
        }
        if (vsnprintf(full_buf, need, fmt, ap_copy) >= 0) {
            fputs(full_buf, stream);
            app_log_send_chunked(level, full_buf);
            file_logger_write(MQTT_FILE_LOG, level >= APP_LOG_ERROR ? "ERROR" : (level >= APP_LOG_WARN ? "WARN" : "INFO"), "%s", full_buf);
        }
        free(full_buf);
    }
    va_end(ap_copy);
}

static __attribute__((unused)) void app_logf(FILE *stream, int level, const char *fmt, ...) {
    va_list ap;
    if (!APP_LOG_ENABLED(level)) {
        return;
    }
    va_start(ap, fmt);
    app_log_format_and_emit(stream, level, fmt, ap);
    va_end(ap);
}

// Helper function to map string to NetworkInterfaceType
static NetworkInterfaceType parse_net_interface(const char *str) {
    if (str == NULL) return NET_IF_UNKNOWN;
    if (strcasecmp(str, "4G") == 0) return NET_IF_4G;
    if (strcasecmp(str, "ETH") == 0) return NET_IF_ETHERNET;
    if (strcasecmp(str, "WiFi") == 0) return NET_IF_WIFI;
    return NET_IF_UNKNOWN;
}

const char* get_net_interface_name(NetworkInterfaceType type) {
    switch (type) {
        case NET_IF_4G: return "4G";
        case NET_IF_ETHERNET: return "Ethernet";
        case NET_IF_WIFI: return "WIFI";
        default: return "Unknown";
    }
}

int load_config(const char *filename, AppConfig *config) {
    if (filename == NULL || config == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "Error: Invalid arguments to load_config\n");
        return -1;
    }

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "Error opening config file: %s\n", strerror(errno));
        return -1;
    }

    // Determine file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        app_logf(stderr, APP_LOG_ERROR, "Error: Config file is empty\n");
        fclose(fp);
        return -1;
    }

    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "Error: Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    buffer[read_size] = '\0';
    fclose(fp);

    struct json_object *parsed_json = json_tokener_parse(buffer);
    free(buffer);

    if (parsed_json == NULL) {
        app_logf(stderr, APP_LOG_ERROR, "Error: Failed to parse JSON\n");
        return -1;
    }

    struct json_object *obj_host, *obj_port, *obj_user, *obj_pass, *obj_gw_id, *obj_pub, *obj_sub, *obj_hb, *obj_qos, *obj_net, *obj_ver, *obj_kernel, *obj_firmware;

    // Set defaults
    memset(config, 0, sizeof(AppConfig));
    config->mqtt_port = 1883;
    config->heartbeat_interval = 60;
    config->qos = 0; // Default QoS
    config->net_interface = NET_IF_ETHERNET; // Default
    strncpy(config->kernel_version, "unknown", sizeof(config->kernel_version) - 1);
    strncpy(config->firmware_version, "unknown", sizeof(config->firmware_version) - 1);
    strncpy(config->app_version, "V1.0.0", sizeof(config->app_version) - 1);

    if (json_object_object_get_ex(parsed_json, "mqtt_host", &obj_host)) {
        strncpy(config->mqtt_host, json_object_get_string(obj_host), sizeof(config->mqtt_host) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "mqtt_port", &obj_port)) {
        config->mqtt_port = json_object_get_int(obj_port);
    }

    if (json_object_object_get_ex(parsed_json, "username", &obj_user)) {
        strncpy(config->mqtt_username, json_object_get_string(obj_user), sizeof(config->mqtt_username) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "password", &obj_pass)) {
        strncpy(config->mqtt_password, json_object_get_string(obj_pass), sizeof(config->mqtt_password) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "gateway_id", &obj_gw_id)) {
        strncpy(config->gateway_id, json_object_get_string(obj_gw_id), sizeof(config->gateway_id) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "kernel_version", &obj_kernel)) {
        strncpy(config->kernel_version, json_object_get_string(obj_kernel), sizeof(config->kernel_version) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "firmware_version", &obj_firmware)) {
        strncpy(config->firmware_version, json_object_get_string(obj_firmware), sizeof(config->firmware_version) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "app_version", &obj_ver)) {
        strncpy(config->app_version, json_object_get_string(obj_ver), sizeof(config->app_version) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "pub_topic", &obj_pub)) {
        strncpy(config->pub_topic, json_object_get_string(obj_pub), sizeof(config->pub_topic) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "sub_topic", &obj_sub)) {
        strncpy(config->sub_topic, json_object_get_string(obj_sub), sizeof(config->sub_topic) - 1);
    }

    if (json_object_object_get_ex(parsed_json, "heartbeat_interval", &obj_hb)) {
        config->heartbeat_interval = json_object_get_int(obj_hb);
    }

    if (json_object_object_get_ex(parsed_json, "qos", &obj_qos)) {
        config->qos = json_object_get_int(obj_qos);
        if (config->qos < 0 || config->qos > 2) {
            config->qos = 0; // Reset to default if invalid
        }
    }

    if (json_object_object_get_ex(parsed_json, "net_interface", &obj_net)) {
        config->net_interface = parse_net_interface(json_object_get_string(obj_net));
    }

    json_object_put(parsed_json); // Free JSON object
    return 0;
}
