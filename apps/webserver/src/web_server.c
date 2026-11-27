#include "web_server.h"
#include "mongoose.h"
#include "config_manager.h"
#include "system_manager.h"
#include "gateway_telemetry.h"
#include "gateway_version.h"
#include "network_manager.h"
#include "upgrade_manager.h"
#include "ipc_command_client.h"
#include "web_resources.h"
#include "file_logger.h"
#include "watchdog_status.h"
#include "log_export.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <json-c/json.h>

#define WEB_FILE_LOG "/var/log/web_server.log"
#define printf(...) file_logger_printf(WEB_FILE_LOG, "INFO", __VA_ARGS__)
#define fprintf(stream, ...) file_logger_fprintf(WEB_FILE_LOG, stream, __VA_ARGS__)

static struct mg_mgr mgr;
static AppConfig app_config;
static const char *TRAFFIC_STATS_PATH = "/etc/turbotraffic";
static const char *TRAFFIC_STATS_TMP_PATH = "/etc/turbotraffic.web.tmp";
static const char *RF_STATS_PATH = "/var/run/mqtt_stats.json";
static const char *RF_STATS_BASELINE_PATH = "/var/run/mqtt_stats_baseline.json";
static const char *RF_STATS_BASELINE_TMP_PATH = "/var/run/mqtt_stats_baseline.web.tmp";
static const char *RF_SCAN_SOCKET_PATH = "/tmp/data_collect.sock";
static const char *RF_SCAN_STATUS_PATH = "/tmp/data_collect_status.json";
/* tk8710_gw runs with /userdata as its working directory.  The sweep writer
 * stores one frequency line followed by eight antenna noise-floor lines. */
static const char *RF_SCAN_RESULT_PATH = "/userdata/SweepFreqResult/Result.txt";
static const char *RF_ACM_SOCKET_PATH = "/tmp/acm.sock";
static const char *RF_ACM_STATUS_PATH = "/tmp/acm_status.json";
static const char *RF_ACM_RESULT_PATH = "/tmp/acm_result.json";
static const char *UPGRADE_UPLOAD_PATH = "/tmp/image.swu";
static const char *UPGRADE_UPLOAD_FILE = "image.swu";
static const size_t MAX_FIRMWARE_UPLOAD_SIZE = 256UL * 1024UL * 1024UL;

static pthread_mutex_t network_quality_lock = PTHREAD_MUTEX_INITIALIZER;
static bool network_quality_worker_started = false;
static bool cached_network_quality_ready = false;
static bool cached_network_quality_ok = false;
static char cached_network_quality_target[128] = {0};
static char cached_network_quality_status[32] = "pending";
static double cached_network_quality_avg = 0.0;
static double cached_network_quality_jitter = 0.0;
static int cached_network_quality_loss = 100;
static time_t cached_network_quality_timestamp = 0;

// 辅助函数：HTML 响应
static void reply_html(struct mg_connection *c, int code, const char *body) {
  size_t len = strlen(body);
  // 打印日志以便调试
  printf("Replying HTML (len=%d): %.100s...\n", (int)len, body);
  mg_printf(c,
            "HTTP/1.1 %d OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            code, (unsigned long)len);
  mg_send(c, body, len);
  c->is_resp = 0;  /* Manual response is fully queued; permit keep-alive reuse. */
}

// 辅助函数：重定向
static void reply_redirect_with_headers(struct mg_connection *c, const char *location, const char *headers) {
  mg_printf(c,
            "HTTP/1.1 302 Found\r\n"
            "Location: %s\r\n"
            "%s"
            "Content-Length: 0\r\n"
            "\r\n",
            location,
            headers ? headers : "");
  c->is_resp = 0;
}

static void reply_redirect(struct mg_connection *c, const char *location) {
  reply_redirect_with_headers(c, location, NULL);
}

// 辅助函数：检查 Cookie
static bool is_authed(struct mg_http_message *hm) {
  struct mg_str *cookie = mg_http_get_header(hm, "Cookie");
  if (cookie == NULL) return false;
  const struct mg_str session = {"session=admin", sizeof("session=admin") - 1};
  return mg_strstr(*cookie, session) != NULL;
}

// 辅助函数：直接发送 HTML 内容（不格式化）
static void send_html_raw(struct mg_connection *c, const char *content) {
    size_t len = strlen(content);
    if (len > 0) {
        mg_printf(c, "%lX\r\n", (unsigned long)len);
        mg_send(c, content, len);
        mg_send(c, "\r\n", 2);
    }
}

// 辅助函数：分块发送 HTML
static void send_chunk(struct mg_connection *c, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed < 0) {
        va_end(ap);
        return;
    }
    // 仅打印大块，避免日志过多
    if (needed > 8192) {
        fprintf(stderr, "[web] send_chunk large chunk: needed=%d bytes\n", needed);
    }

    char *buf = (char *)malloc((size_t)needed + 1);
    if (!buf) {
        va_end(ap);
        return;
    }
    vsnprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    send_html_raw(c, buf);
    free(buf);
}

static void json_escape(const char *in, char *out, size_t out_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '\"' || c == '\\') {
            if (j + 2 < out_len) {
                out[j++] = '\\';
                out[j++] = (char)c;
            }
        } else if (c == '\n') {
            if (j + 2 < out_len) { out[j++] = '\\'; out[j++] = 'n'; }
        } else if (c == '\r') {
            if (j + 2 < out_len) { out[j++] = '\\'; out[j++] = 'r'; }
        } else if (c == '\t') {
            if (j + 2 < out_len) { out[j++] = '\\'; out[j++] = 't'; }
        } else if (c < 0x20) {
            if (j + 6 < out_len) {
                j += snprintf(out + j, out_len - j, "\\u%04x", c);
            }
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

static void html_escape(const char *in, char *out, size_t out_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_len; i++) {
        char c = in[i];
        const char *rep = NULL;
        if (c == '&') rep = "&amp;";
        else if (c == '<') rep = "&lt;";
        else if (c == '>') rep = "&gt;";
        else if (c == '"') rep = "&quot;";
        else if (c == '\'') rep = "&#39;";
        if (rep) {
            size_t len = strlen(rep);
            if (j + len >= out_len) break;
            memcpy(out + j, rep, len);
            j += len;
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

static bool parse_query_long(struct mg_str *query, const char *name, long *value) {
    char buf[32];
    char *end = NULL;
    long parsed = 0;

    if (!query || !value) return false;
    if (mg_http_get_var(query, name, buf, sizeof(buf)) <= 0) return false;

    errno = 0;
    parsed = strtol(buf, &end, 10);
    if (errno != 0 || end == buf || *end != '\0') return false;

    *value = parsed;
    return true;
}

static bool normalize_rf_scan_freq(const char *input, char *output, size_t output_len) {
    char sanitized[32] = {0};
    char *end = NULL;
    double value = 0.0;
    size_t j = 0;
    size_t i;

    if (!input || !output || output_len < 4) return false;

    for (i = 0; input[i] != '\0' && j + 1 < sizeof(sanitized); ++i) {
        unsigned char ch = (unsigned char)input[i];
        if (isspace(ch)) continue;
        if (ch == 'm' || ch == 'M') break;
        if ((ch >= '0' && ch <= '9') || ch == '.') {
            sanitized[j++] = (char)ch;
        } else {
            return false;
        }
    }

    if (j == 0) return false;

    errno = 0;
    value = strtod(sanitized, &end);
    if (errno != 0 || end == sanitized || *end != '\0') return false;
    if (value < 100.0 || value > 1000.0) return false;

    if (snprintf(output, output_len, "%.1fM", value) >= (int)output_len) return false;
    return true;
}

static bool parse_rf_scan_freq_value(const char *input, double *value) {
    char normalized[32] = {0};
    char *end = NULL;

    if (!value) return false;
    if (!normalize_rf_scan_freq(input, normalized, sizeof(normalized))) return false;

    errno = 0;
    *value = strtod(normalized, &end);
    if (errno != 0 || end == normalized) return false;
    return true;
}

static bool parse_rf_scan_mode(const char *input, char *mode, size_t mode_len) {
    if (!input || !mode || mode_len == 0) return false;
    if (strcmp(input, "62.5k") != 0 &&
        strcmp(input, "125k") != 0 &&
        strcmp(input, "250k") != 0 &&
        strcmp(input, "500k") != 0) {
        return false;
    }

    strncpy(mode, input, mode_len - 1);
    mode[mode_len - 1] = '\0';
    return true;
}

static bool rf_acm_result_item_extract(struct json_object *item_obj,
                                       int *channel_out,
                                       const char **status_out) {
    struct json_object *channel_obj = NULL;
    struct json_object *status_obj = NULL;
    int channel = 0;
    const char *status = NULL;

    if (!item_obj || !channel_out || !status_out) return false;
    if (!json_object_object_get_ex(item_obj, "channel", &channel_obj) ||
        !json_object_object_get_ex(item_obj, "status", &status_obj)) {
        return false;
    }

    channel = json_object_get_int(channel_obj);
    status = json_object_get_string(status_obj);
    if (channel < 1 || channel > 8 || !status) return false;
    if (strcmp(status, "normal") != 0 && strcmp(status, "error") != 0) return false;

    *channel_out = channel;
    *status_out = status;
    return true;
}

static void handle_api_rf_scan_start(struct mg_connection *c, struct mg_http_message *hm) {
    char start_input[32] = {0};
    char stop_input[32] = {0};
    char mode_input[16] = {0};
    char mode[16] = "125k";
    char freq_start[32] = {0};
    char freq_stop[32] = {0};
    char args[IPC_CLIENT_MAX_COMMAND_LEN + 1] = {0};
    char response[IPC_CLIENT_MAX_RESPONSE_LEN] = {0};
    char escaped[IPC_CLIENT_MAX_RESPONSE_LEN * 2] = {0};
    double start_value = 0.0;
    double stop_value = 0.0;
    int ret;

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
        return;
    }

    if (mg_http_get_var(&hm->body, "freq_start", start_input, sizeof(start_input)) <= 0 ||
        mg_http_get_var(&hm->body, "freq_stop", stop_input, sizeof(stop_input)) <= 0) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"频点参数不能为空\"}");
        return;
    }

    if (mg_http_get_var(&hm->body, "mode", mode_input, sizeof(mode_input)) > 0 &&
        !parse_rf_scan_mode(mode_input, mode, sizeof(mode))) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"模式参数非法，必须是 62.5k、125k、250k、500k 之一\"}");
        return;
    }

    if (!normalize_rf_scan_freq(start_input, freq_start, sizeof(freq_start)) ||
        !normalize_rf_scan_freq(stop_input, freq_stop, sizeof(freq_stop)) ||
        !parse_rf_scan_freq_value(start_input, &start_value) ||
        !parse_rf_scan_freq_value(stop_input, &stop_value)) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"频点格式非法，请输入类似 470.5M\"}");
        return;
    }

    if (start_value > stop_value) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"起始频点不能大于结束频点\"}");
        return;
    }

    if (snprintf(args, sizeof(args), "freq_start=%s freq_stop=%s mode=%s", freq_start, freq_stop, mode) >= (int)sizeof(args)) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"扫频参数过长\"}");
        return;
    }

    ret = ipc_client_start_task_with_args(RF_SCAN_SOCKET_PATH,
                                          args,
                                          response,
                                          sizeof(response),
                                          IPC_CLIENT_DEFAULT_TIMEOUT_MS);
    if (ret != 0) {
        mg_http_reply(c, 502, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"业务进程未启动或本地通信失败\"}");
        return;
    }

    json_escape(response, escaped, sizeof(escaped));
    if (strcmp(response, "OK") == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"ok\",\"state\":\"running\",\"message\":\"任务已启动\",\"response\":\"%s\",\"freq_start\":\"%s\",\"freq_stop\":\"%s\",\"mode\":\"%s\"}",
                      escaped, freq_start, freq_stop, mode);
        return;
    }

    if (strcmp(response, "BUSY") == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"busy\",\"state\":\"running\",\"message\":\"扫频任务正在执行中\",\"response\":\"%s\"}",
                      escaped);
        return;
    }

    mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"error\",\"message\":\"%s\",\"response\":\"%s\"}",
                  escaped[0] ? escaped : "服务端返回错误",
                  escaped);
}

static void handle_api_rf_scan_status(struct mg_connection *c) {
    struct json_object *root = json_object_from_file(RF_SCAN_STATUS_PATH);
    struct json_object *state_obj = NULL;
    struct json_object *message_obj = NULL;
    struct json_object *ready_obj = NULL;
    const char *state = "idle";
    const char *message = "暂无扫频任务";
    int result_ready = 0;
    char escaped_state[64] = {0};
    char escaped_message[256] = {0};

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"ok\",\"data\":{\"state\":\"idle\",\"message\":\"暂无扫频任务\",\"result_ready\":false}}");
        return;
    }

    if (json_object_object_get_ex(root, "state", &state_obj)) {
        const char *tmp = json_object_get_string(state_obj);
        if (tmp && tmp[0] != '\0') state = tmp;
    }
    if (json_object_object_get_ex(root, "message", &message_obj)) {
        const char *tmp = json_object_get_string(message_obj);
        if (tmp && tmp[0] != '\0') message = tmp;
    }
    if (json_object_object_get_ex(root, "result_ready", &ready_obj)) {
        result_ready = json_object_get_boolean(ready_obj) ? 1 : 0;
    }

    json_escape(state, escaped_state, sizeof(escaped_state));
    json_escape(message, escaped_message, sizeof(escaped_message));
    json_object_put(root);

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"ok\",\"data\":{\"state\":\"%s\",\"message\":\"%s\",\"result_ready\":%s}}",
                  escaped_state,
                  escaped_message,
                  result_ready ? "true" : "false");
}

static void handle_api_rf_scan_result(struct mg_connection *c) {
    enum { RF_SCAN_ANTENNA_COUNT = 8 };
    struct stat st;
    FILE *fp = NULL;
    char first_freq[32] = {0};
    char last_freq[32] = {0};
    size_t payload_size = 4096;
    size_t used = 0;
    int length = 0;
    int count = 0;
    char *payload = NULL;
    const char *error_message = NULL;
    int http_status = 200;

    if (stat(RF_SCAN_RESULT_PATH, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        error_message = "扫频结果文件不存在或为空";
        http_status = 500;
        goto cleanup;
    }

    fp = fopen(RF_SCAN_RESULT_PATH, "r");
    if (!fp) {
        error_message = "无法打开扫频结果文件";
        http_status = 500;
        goto cleanup;
    }

    /* The JSON representation is smaller than four times the text file size. */
    if ((size_t)st.st_size <= (SIZE_MAX - 512U) / 4U) {
        size_t estimated_size = (size_t)st.st_size * 4U + 512U;
        if (estimated_size > payload_size) payload_size = estimated_size;
    }
    payload = (char *)malloc(payload_size);
    if (!payload) {
        error_message = "内存不足，无法生成扫频结果";
        http_status = 500;
        goto cleanup;
    }
    payload[0] = '\0';

    while (1) {
        unsigned int frequency_hz = 0;
        float antenna_noise[RF_SCAN_ANTENNA_COUNT];
        double sum = 0.0;
        char freq[32] = {0};
        int complete = 1;
        int rssi;

        if (fscanf(fp, "%u", &frequency_hz) != 1) break;
        for (int ant = 0; ant < RF_SCAN_ANTENNA_COUNT; ++ant) {
            if (fscanf(fp, "%f", &antenna_noise[ant]) != 1) {
                complete = 0;
                break;
            }
            sum += antenna_noise[ant];
        }
        if (!complete) break;  /* Ignore an incomplete record being written. */

        snprintf(freq, sizeof(freq), "%.6gM", (double)frequency_hz / 1000000.0);
        rssi = (int)(sum / RF_SCAN_ANTENNA_COUNT - 0.5);
        if (count == 0) snprintf(first_freq, sizeof(first_freq), "%s", freq);
        snprintf(last_freq, sizeof(last_freq), "%s", freq);
        length = snprintf(payload + used,
                          payload_size - used,
                          "%s{\"freq\":\"%s\",\"rssi\":%d}",
                          count == 0 ? "" : ",",
                          freq,
                          rssi);
        if (length < 0 || (size_t)length >= payload_size - used) {
            error_message = "扫频结果点数过大";
            http_status = 500;
            goto cleanup;
        }
        used += (size_t)length;
        count++;
    }

    if (count == 0) {
        error_message = "扫频结果中没有完整的频点记录";
        http_status = 500;
        goto cleanup;
    }

    /* Move the point array right and prepend the response envelope. */
    {
        char prefix[256];
        int prefix_len = snprintf(prefix, sizeof(prefix),
                                  "{\"status\":\"ok\",\"data\":{\"success\":true,\"duration\":0,\"freq_start\":\"%s\",\"freq_stop\":\"%s\",\"points\":[",
                                  first_freq, last_freq);
        if (prefix_len < 0 || (size_t)prefix_len + used + 64U >= payload_size) {
            error_message = "扫频结果过大";
            http_status = 500;
            goto cleanup;
        }
        memmove(payload + prefix_len, payload, used);
        memcpy(payload, prefix, (size_t)prefix_len);
        used += (size_t)prefix_len;
    }

    length = snprintf(payload + used, payload_size - used, "],\"count\":%d}}", count);
    if (length < 0 || (size_t)length >= payload_size - used) {
        error_message = "扫频结果拼装失败";
        http_status = 500;
        goto cleanup;
    }

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", "%s", payload);

cleanup:
    if (fp) fclose(fp);
    if (payload) free(payload);
    if (error_message) {
        mg_http_reply(c, http_status, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"%s\"}",
                      error_message);
    }
}

static void handle_api_rf_acm_start(struct mg_connection *c, struct mg_http_message *hm) {
    char response[IPC_CLIENT_MAX_RESPONSE_LEN] = {0};
    char escaped[IPC_CLIENT_MAX_RESPONSE_LEN * 2] = {0};
    int ret;

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
        return;
    }

    ret = ipc_client_send_command(RF_ACM_SOCKET_PATH,
                                  "START acm",
                                  response,
                                  sizeof(response),
                                  IPC_CLIENT_DEFAULT_TIMEOUT_MS);
    if (ret != 0) {
        mg_http_reply(c, 502, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"业务进程未启动或本地通信失败\"}");
        return;
    }

    json_escape(response, escaped, sizeof(escaped));
    if (strcmp(response, "OK") == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"ok\",\"state\":\"running\",\"message\":\"ACM任务已启动\",\"response\":\"%s\"}",
                      escaped);
        return;
    }

    if (strcmp(response, "BUSY") == 0) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"busy\",\"state\":\"running\",\"message\":\"ACM任务正在执行中\",\"response\":\"%s\"}",
                      escaped);
        return;
    }

    mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"error\",\"message\":\"%s\",\"response\":\"%s\"}",
                  escaped[0] ? escaped : "服务端返回错误",
                  escaped);
}

static void handle_api_rf_acm_status(struct mg_connection *c) {
    struct json_object *root = json_object_from_file(RF_ACM_STATUS_PATH);
    struct json_object *state_obj = NULL;
    struct json_object *message_obj = NULL;
    struct json_object *ready_obj = NULL;
    const char *state = "idle";
    const char *message = "暂无ACM任务";
    int result_ready = 0;
    char escaped_state[64] = {0};
    char escaped_message[256] = {0};

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"ok\",\"data\":{\"state\":\"idle\",\"message\":\"暂无ACM任务\",\"result_ready\":false}}");
        return;
    }

    if (json_object_object_get_ex(root, "state", &state_obj)) {
        const char *tmp = json_object_get_string(state_obj);
        if (tmp && tmp[0] != '\0') state = tmp;
    }
    if (json_object_object_get_ex(root, "message", &message_obj)) {
        const char *tmp = json_object_get_string(message_obj);
        if (tmp && tmp[0] != '\0') message = tmp;
    }
    if (json_object_object_get_ex(root, "result_ready", &ready_obj)) {
        result_ready = json_object_get_boolean(ready_obj) ? 1 : 0;
    }

    json_escape(state, escaped_state, sizeof(escaped_state));
    json_escape(message, escaped_message, sizeof(escaped_message));
    json_object_put(root);

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"ok\",\"data\":{\"state\":\"%s\",\"message\":\"%s\",\"result_ready\":%s}}",
                  escaped_state,
                  escaped_message,
                  result_ready ? "true" : "false");
}

static void handle_api_rf_acm_result(struct mg_connection *c) {
    struct json_object *root = json_object_from_file(RF_ACM_RESULT_PATH);
    struct json_object *channels_obj = NULL;
    struct json_object *success_obj = NULL;
    size_t payload_size = 0;
    size_t used = 0;
    int length = 0;
    int count = 0;
    int success = 0;
    char *payload = NULL;
    const char *error_message = NULL;
    int http_status = 200;

    if (!root || !json_object_is_type(root, json_type_object)) {
        error_message = "ACM结果文件不存在或格式错误";
        http_status = 500;
        goto cleanup;
    }

    if (json_object_object_get_ex(root, "success", &success_obj)) {
        success = json_object_get_boolean(success_obj) ? 1 : 0;
    }
    if (!json_object_object_get_ex(root, "channels", &channels_obj) || !json_object_is_type(channels_obj, json_type_array)) {
        error_message = "ACM结果缺少 channels 数组";
        http_status = 500;
        goto cleanup;
    }

    payload_size = 128 + ((size_t)json_object_array_length(channels_obj) * 48);
    if (payload_size < 1024) payload_size = 1024;
    payload = (char *)malloc(payload_size);
    if (!payload) {
        error_message = "内存不足，无法生成ACM结果";
        http_status = 500;
        goto cleanup;
    }
    payload[0] = '\0';

    length = snprintf(payload, payload_size,
                      "{\"status\":\"ok\",\"data\":{\"success\":%s,\"channels\":[",
                      success ? "true" : "false");
    if (length < 0 || (size_t)length >= payload_size) {
        error_message = "ACM结果过大";
        http_status = 500;
        goto cleanup;
    }
    used = (size_t)length;

    for (int i = 0; i < json_object_array_length(channels_obj); ++i) {
        struct json_object *item_obj = json_object_array_get_idx(channels_obj, i);
        int channel = 0;
        const char *status = NULL;

        if (!rf_acm_result_item_extract(item_obj, &channel, &status)) continue;
        length = snprintf(payload + used,
                          payload_size - used,
                          "%s{\"channel\":%d,\"status\":\"%s\"}",
                          count == 0 ? "" : ",",
                          channel,
                          status);
        if (length < 0 || (size_t)length >= payload_size - used) {
            error_message = "ACM结果点数过大";
            http_status = 500;
            goto cleanup;
        }
        used += (size_t)length;
        count++;
    }

    if (count == 0) {
        error_message = "ACM结果 channels 存在，但没有可解析的通道数据";
        http_status = 500;
        goto cleanup;
    }

    length = snprintf(payload + used,
                      payload_size - used,
                      "],\"count\":%d}}",
                      count);
    if (length < 0 || (size_t)length >= payload_size - used) {
        error_message = "ACM结果拼装失败";
        http_status = 500;
        goto cleanup;
    }

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", "%s", payload);

cleanup:
    if (root) json_object_put(root);
    if (payload) free(payload);
    if (error_message) {
        mg_http_reply(c, http_status, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"%s\"}",
                      error_message);
    }
}

static bool is_traffic_iface_allowed(const char *iface) {
    return iface && (strcmp(iface, "eth0") == 0 || strcmp(iface, "usb0") == 0 || strcmp(iface, "wlan0") == 0);
}

static bool read_iface_counters(const char *target_iface, unsigned long long *rx, unsigned long long *tx) {
    FILE *fp = NULL;
    char line[256];

    if (!target_iface || !rx || !tx) return false;
    fp = fopen("/proc/net/dev", "r");
    if (!fp) return false;
    *rx = 0;
    *tx = 0;

    if (fgets(line, sizeof(line), fp) && fgets(line, sizeof(line), fp)) {
        while (fgets(line, sizeof(line), fp)) {
            char iface[32] = {0};
            unsigned long long rx_bytes = 0, tx_bytes = 0;
            unsigned long long skip1 = 0, skip2 = 0, skip3 = 0, skip4 = 0, skip5 = 0, skip6 = 0, skip7 = 0;
            if (sscanf(line, " %31[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       iface, &rx_bytes, &skip1, &skip2, &skip3, &skip4, &skip5, &skip6, &skip7, &tx_bytes) == 10) {
                if (strcmp(iface, target_iface) == 0) {
                    *rx = rx_bytes;
                    *tx = tx_bytes;
                    fclose(fp);
                    return true;
                }
            }
        }
    }

    fclose(fp);
    return false;
}

static bool write_json_atomic(struct json_object *root) {
    if (json_object_to_file_ext(TRAFFIC_STATS_TMP_PATH, root, JSON_C_TO_STRING_PRETTY) != 0) return false;
    if (rename(TRAFFIC_STATS_TMP_PATH, TRAFFIC_STATS_PATH) != 0) {
        remove(TRAFFIC_STATS_TMP_PATH);
        return false;
    }
    return true;
}

static bool write_json_atomic_to_path(struct json_object *root, const char *tmp_path, const char *target_path) {
    if (!root || !tmp_path || !target_path) return false;
    if (json_object_to_file_ext(tmp_path, root, JSON_C_TO_STRING_PRETTY) != 0) return false;
    if (rename(tmp_path, target_path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

static bool ensure_traffic_iface_json(struct json_object *interfaces, const char *iface,
                                      struct json_object **iface_obj) {
    struct json_object *obj = NULL;

    if (!interfaces || !iface || !iface_obj) return false;
    if (!json_object_object_get_ex(interfaces, iface, &obj) || !json_object_is_type(obj, json_type_object)) {
        obj = json_object_new_object();
        if (!obj) return false;
        json_object_object_add(interfaces, iface, obj);
    }
    *iface_obj = obj;
    return true;
}

static unsigned long long json_get_u64(struct json_object *obj, const char *key) {
    struct json_object *val = NULL;
    if (!obj || !key || !json_object_object_get_ex(obj, key, &val)) return 0;
    return (unsigned long long)json_object_get_int64(val);
}

static void json_set_u64(struct json_object *obj, const char *key, unsigned long long value) {
    json_object_object_del(obj, key);
    json_object_object_add(obj, key, json_object_new_int64((int64_t)value));
}

static void recalc_traffic_iface_json(struct json_object *iface_obj) {
    unsigned long long raw_rx = json_get_u64(iface_obj, "raw_rx_bytes");
    unsigned long long raw_tx = json_get_u64(iface_obj, "raw_tx_bytes");
    unsigned long long base_rx = json_get_u64(iface_obj, "base_rx_bytes");
    unsigned long long base_tx = json_get_u64(iface_obj, "base_tx_bytes");
    unsigned long long rx = raw_rx >= base_rx ? raw_rx - base_rx : 0;
    unsigned long long tx = raw_tx >= base_tx ? raw_tx - base_tx : 0;

    json_set_u64(iface_obj, "rx_bytes", rx);
    json_set_u64(iface_obj, "tx_bytes", tx);
    json_set_u64(iface_obj, "total_bytes", rx + tx);
}

static struct json_object *build_traffic_stats_json(void) {
    static const char *const ifaces[] = {"eth0", "usb0", "wlan0"};
    struct json_object *saved = json_object_from_file(TRAFFIC_STATS_PATH);
    struct json_object *saved_ifaces = NULL;
    struct json_object *response = json_object_new_object();
    struct json_object *interfaces = json_object_new_object();

    if (!response || !interfaces) {
        if (saved) json_object_put(saved);
        if (response) json_object_put(response);
        if (interfaces) json_object_put(interfaces);
        return NULL;
    }

    if (saved && json_object_is_type(saved, json_type_object)) {
        json_object_object_get_ex(saved, "interfaces", &saved_ifaces);
        if (saved_ifaces && !json_object_is_type(saved_ifaces, json_type_object)) saved_ifaces = NULL;
    }

    for (size_t i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++) {
        struct json_object *saved_iface = NULL;
        struct json_object *iface_obj = json_object_new_object();
        unsigned long long raw_rx = 0, raw_tx = 0, base_rx = 0, base_tx = 0;
        bool present = read_iface_counters(ifaces[i], &raw_rx, &raw_tx);

        if (!iface_obj) {
            if (saved) json_object_put(saved);
            json_object_put(response);
            json_object_put(interfaces);
            return NULL;
        }
        if (saved_ifaces && json_object_object_get_ex(saved_ifaces, ifaces[i], &saved_iface) &&
            json_object_is_type(saved_iface, json_type_object)) {
            base_rx = json_get_u64(saved_iface, "base_rx_bytes");
            base_tx = json_get_u64(saved_iface, "base_tx_bytes");
        }
        /* Kernel counters restart at zero after a reboot or interface recreation. */
        if (raw_rx < base_rx) base_rx = 0;
        if (raw_tx < base_tx) base_tx = 0;

        json_set_u64(iface_obj, "raw_rx_bytes", raw_rx);
        json_set_u64(iface_obj, "raw_tx_bytes", raw_tx);
        json_set_u64(iface_obj, "base_rx_bytes", base_rx);
        json_set_u64(iface_obj, "base_tx_bytes", base_tx);
        recalc_traffic_iface_json(iface_obj);
        json_object_object_add(iface_obj, "present", json_object_new_boolean(present));
        json_object_object_add(interfaces, ifaces[i], iface_obj);
    }

    json_object_object_add(response, "ok", json_object_new_boolean(true));
    json_object_object_add(response, "source", json_object_new_string("/proc/net/dev"));
    json_object_object_add(response, "timestamp", json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(response, "interfaces", interfaces);
    if (saved) json_object_put(saved);
    return response;
}

static bool reset_traffic_iface_baseline(const char *iface) {
    struct json_object *root = json_object_from_file(TRAFFIC_STATS_PATH);
    struct json_object *interfaces = NULL;
    struct json_object *iface_obj = NULL;
    unsigned long long raw_rx = 0;
    unsigned long long raw_tx = 0;
    bool ok = false;

    if (!is_traffic_iface_allowed(iface)) return false;
    if (!read_iface_counters(iface, &raw_rx, &raw_tx)) return false;

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        root = json_object_new_object();
    }
    if (!root) return false;

    if (!json_object_object_get_ex(root, "interfaces", &interfaces) || !json_object_is_type(interfaces, json_type_object)) {
        json_object_object_del(root, "interfaces");
        interfaces = json_object_new_object();
        json_object_object_add(root, "interfaces", interfaces);
    }

    if (ensure_traffic_iface_json(interfaces, iface, &iface_obj)) {
        json_object_object_del(root, "timestamp");
        json_object_object_add(root, "timestamp", json_object_new_int64((int64_t)time(NULL)));
        json_set_u64(iface_obj, "raw_rx_bytes", raw_rx);
        json_set_u64(iface_obj, "raw_tx_bytes", raw_tx);
        json_set_u64(iface_obj, "base_rx_bytes", raw_rx);
        json_set_u64(iface_obj, "base_tx_bytes", raw_tx);
        recalc_traffic_iface_json(iface_obj);
        ok = write_json_atomic(root);
    }

    json_object_put(root);
    return ok;
}

static bool write_upload_chunk(const char *path, long offset,
                               const void *data, size_t len,
                               long *next_offset, char *err, size_t err_len) {
    struct stat st;
    FILE *fp = NULL;
    long current_size = 0;
    size_t written = 0;

    if (!path || !data || !next_offset || !err || err_len == 0) return false;

    if (offset < 0) {
        snprintf(err, err_len, "invalid offset");
        return false;
    }

    if ((unsigned long) offset + len > MAX_FIRMWARE_UPLOAD_SIZE) {
        snprintf(err, err_len, "file too large");
        return false;
    }

    if (offset == 0) {
        fp = fopen(path, "wb");
    } else {
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            snprintf(err, err_len, "offset mismatch");
            return false;
        }
        current_size = (long) st.st_size;
        if (current_size != offset) {
            snprintf(err, err_len, "offset mismatch");
            return false;
        }
        fp = fopen(path, "r+b");
    }

    if (!fp) {
        snprintf(err, err_len, "open(%s): %d", path, errno);
        return false;
    }

    if (offset > 0 && fseek(fp, offset, SEEK_SET) != 0) {
        snprintf(err, err_len, "seek(%s): %d", path, errno);
        fclose(fp);
        return false;
    }

    written = fwrite(data, 1, len, fp);
    if (written != len || fflush(fp) != 0) {
        snprintf(err, err_len, "write(%s): %d", path, errno);
        fclose(fp);
        return false;
    }

    fclose(fp);
    *next_offset = offset + (long) written;
    err[0] = '\0';
    return true;
}


static bool net_iface_checked(const char *saved, const char *iface) {
    if (!saved || !iface || saved[0] == '\0') return false;
    if (strcmp(saved, iface) == 0) return true;
    if (strcmp(iface, "WiFi") == 0 && strcmp(saved, "WIFI") == 0) return true;

    const char *p = saved;
    size_t iface_len = strlen(iface);
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        size_t len = (size_t)(end - start);
        if (len == iface_len && strncmp(start, iface, iface_len) == 0) return true;
        if (strcmp(iface, "WiFi") == 0 && len == strlen("WIFI") && strncmp(start, "WIFI", strlen("WIFI")) == 0) return true;
    }
    return false;
}

static void append_csv_token(char *dst, size_t dst_size, const char *token) {
    if (dst[0] != '\0') strncat(dst, ",", dst_size - strlen(dst) - 1);
    strncat(dst, token, dst_size - strlen(dst) - 1);
}

static bool get_iface_ipv4(const char *ifname, char *out, size_t out_len) {
    if (!ifname || ifname[0] == '\0') return false;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
        close(fd);
        return false;
    }
    const char *ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    if (!ip) {
        close(fd);
        return false;
    }
    strncpy(out, ip, out_len - 1);
    out[out_len - 1] = '\0';
    close(fd);
    return true;
}

static void format_ipv6(const char *hex, char *out, size_t out_len) {
    unsigned char bytes[16];
    for (int i = 0; i < 16; i++) {
        char part[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        bytes[i] = (unsigned char)strtoul(part, NULL, 16);
    }
    inet_ntop(AF_INET6, bytes, out, out_len);
}

static bool get_iface_ipv6(const char *ifname, char *out, size_t out_len) {
    if (!ifname || ifname[0] == '\0') return false;
    FILE *fp = fopen("/proc/net/if_inet6", "r");
    if (!fp) return false;
    char addr[33] = {0};
    char name[32] = {0};
    unsigned int idx = 0, plen = 0, scope = 0, flags = 0;
    while (fscanf(fp, "%32s %x %x %x %x %31s", addr, &idx, &plen, &scope, &flags, name) == 6) {
        if (strcmp(name, ifname) == 0 && scope == 0) {
            format_ipv6(addr, out, out_len);
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

static bool get_active_iface_ipv4(char *ifname, size_t ifname_len, char *gateway, size_t gateway_len) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return false;
    char line[256];
    fgets(line, sizeof(line), fp);
    while (fgets(line, sizeof(line), fp)) {
        char iface[16];
        unsigned long dest = 0, gw = 0;
        if (sscanf(line, "%15s %lx %lx", iface, &dest, &gw) == 3) {
            if (dest == 0) {
                strncpy(ifname, iface, ifname_len - 1);
                ifname[ifname_len - 1] = '\0';
                struct in_addr addr;
                addr.s_addr = gw;
                const char *gw_str = inet_ntoa(addr);
                if (gw_str) {
                    strncpy(gateway, gw_str, gateway_len - 1);
                    gateway[gateway_len - 1] = '\0';
                } else {
                    if (gateway_len > 0) gateway[0] = '\0';
                }
                fclose(fp);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

static bool get_active_iface_ipv6(char *ifname, size_t ifname_len) {
    FILE *fp = fopen("/proc/net/ipv6_route", "r");
    if (!fp) return false;
    char dest[33], src[33], next_hop[33], iface[32];
    unsigned int dest_plen = 0, src_plen = 0, metric = 0, refcnt = 0, use = 0, flags = 0;
    while (fscanf(fp, "%32s %x %32s %x %32s %x %x %x %x %31s",
                  dest, &dest_plen, src, &src_plen, next_hop, &metric, &refcnt, &use, &flags, iface) == 10) {
        bool is_default = true;
        for (int i = 0; i < 32; i++) {
            if (dest[i] != '0') { is_default = false; break; }
        }
        if (is_default && dest_plen == 0) {
            strncpy(ifname, iface, ifname_len - 1);
            ifname[ifname_len - 1] = '\0';
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

// 页面构建辅助函数：开始
static void send_page_start(struct mg_connection *c, const char *title, const char *active_page) {
    mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nTransfer-Encoding: chunked\r\n\r\n");

    fprintf(stderr,
            "[web] send_page_start: title=%s active=%s header_tpl_len=%zu css_len=%zu\n",
            title ? title : "-",
            active_page ? active_page : "-",
            strlen(RES_HTML_LAYOUT_HEADER_HTML),
            strlen(RES_CSS_STYLE_CSS));

    send_chunk(c, RES_HTML_LAYOUT_HEADER_HTML,
             title,
             (int)strlen(RES_CSS_STYLE_CSS), RES_CSS_STYLE_CSS,
             strcmp(active_page, "status") == 0 ? "active" : "",
             strcmp(active_page, "config") == 0 ? "active" : "",
             strcmp(active_page, "network") == 0 ? "active" : "",
             strcmp(active_page, "upgrade") == 0 ? "active" : "",
             strcmp(active_page, "rf") == 0 ? "active" : "");
}

// 页面构建辅助函数：结束
static void send_page_end(struct mg_connection *c) {
    send_chunk(c, "%s", RES_HTML_LAYOUT_FOOTER_HTML);
    mg_http_printf_chunk(c, ""); // 发送结束块
    c->is_resp = 0;
}

// 登录页面
static void handle_login(struct mg_connection *c, struct mg_http_message *hm) {
    if (mg_vcmp(&hm->method, "POST") == 0) {
        char user[64], pass[64];
        mg_http_get_var(&hm->body, "username", user, sizeof(user));
        mg_http_get_var(&hm->body, "password", pass, sizeof(pass));
        
        if (strcmp(user, "admin") == 0 && strcmp(pass, app_config.web_password) == 0) {
            reply_redirect_with_headers(c, "/status", "Set-Cookie: session=admin; Path=/; HttpOnly\r\n");
        } else {
            reply_html(c, 200, "Login Failed"); // 简化处理
        }
    } else {
        // 使用 chunked 发送，避免大 buffer
        mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nTransfer-Encoding: chunked\r\n\r\n");
        
        send_chunk(c, RES_HTML_LOGIN_HTML, (int)strlen(RES_CSS_STYLE_CSS), RES_CSS_STYLE_CSS);
                 
        mg_http_printf_chunk(c, ""); // End chunk
        c->is_resp = 0;
    }
}

static void handle_manger(struct mg_connection *c, struct mg_http_message *hm) {
    char pwd_message[256] = {0};
    char pwd_message_class[32] = "alert-info";
    char pwd_message_display[8] = "none";

    if (mg_vcmp(&hm->method, "POST") == 0) {
        char action[32] = {0};
        if (mg_http_get_var(&hm->body, "manger_action", action, sizeof(action)) > 0) {
            if (strcmp(action, "password") == 0) {
                char current[64] = {0};
                char new_pass[64] = {0};
                char confirm[64] = {0};
                mg_http_get_var(&hm->body, "current_password", current, sizeof(current));
                mg_http_get_var(&hm->body, "new_password", new_pass, sizeof(new_pass));
                mg_http_get_var(&hm->body, "confirm_password", confirm, sizeof(confirm));
                if (strcmp(current, app_config.web_password) != 0) {
                    snprintf(pwd_message, sizeof(pwd_message), "当前密码不正确");
                    strncpy(pwd_message_class, "alert-danger", sizeof(pwd_message_class));
                } else if (new_pass[0] == '\0') {
                    snprintf(pwd_message, sizeof(pwd_message), "新密码不能为空");
                    strncpy(pwd_message_class, "alert-danger", sizeof(pwd_message_class));
                } else if (strcmp(new_pass, confirm) != 0) {
                    snprintf(pwd_message, sizeof(pwd_message), "两次输入的新密码不一致");
                    strncpy(pwd_message_class, "alert-danger", sizeof(pwd_message_class));
                } else {
                    strncpy(app_config.web_password, new_pass, sizeof(app_config.web_password) - 1);
                    if (config_save(&app_config)) {
                        snprintf(pwd_message, sizeof(pwd_message), "密码已更新");
                        strncpy(pwd_message_class, "alert-success", sizeof(pwd_message_class));
                    } else {
                        snprintf(pwd_message, sizeof(pwd_message), "保存失败");
                        strncpy(pwd_message_class, "alert-danger", sizeof(pwd_message_class));
                    }
                }
                strncpy(pwd_message_display, "block", sizeof(pwd_message_display));
            }
        }
    }

    char pwd_message_html[512] = {0};
    html_escape(pwd_message, pwd_message_html, sizeof(pwd_message_html));

    send_page_start(c, "隐藏管理", "");
    send_chunk(c, RES_HTML_MANGER_HTML,
             pwd_message_display, pwd_message_class, pwd_message_html);
    send_page_end(c);
}

static void handle_active_ip_api(struct mg_connection *c) {
    char iface[16] = {0};
    char ipv4[64] = {0};
    char ipv6[128] = {0};
    char gateway[64] = {0};

    if (get_active_iface_ipv4(iface, sizeof(iface), gateway, sizeof(gateway))) {
        get_iface_ipv4(iface, ipv4, sizeof(ipv4));
        get_iface_ipv6(iface, ipv6, sizeof(ipv6));
    } else if (get_active_iface_ipv6(iface, sizeof(iface))) {
        get_iface_ipv6(iface, ipv6, sizeof(ipv6));
    }

    char iface_json[32] = {0};
    char ipv4_json[128] = {0};
    char ipv6_json[256] = {0};
    char gateway_json[128] = {0};
    json_escape(iface, iface_json, sizeof(iface_json));
    json_escape(ipv4, ipv4_json, sizeof(ipv4_json));
    json_escape(ipv6, ipv6_json, sizeof(ipv6_json));
    json_escape(gateway, gateway_json, sizeof(gateway_json));

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                 "{\"iface\":\"%s\",\"ipv4\":\"%s\",\"ipv6\":\"%s\",\"gateway\":\"%s\"}",
                 iface_json, ipv4_json, ipv6_json, gateway_json);
}

static void handle_api_system_info(struct mg_connection *c) {
    SystemInfo info;
    system_get_info(&info);

    int disk_percent = 0;
    if (info.disk_total > 0) {
        disk_percent = (int)((double)info.disk_used / info.disk_total * 100);
    }
    
    int mem_percent = 0;
    if (info.mem_total > 0) {
        mem_percent = (int)((double)info.mem_used / info.mem_total * 100);
    }

    char json[1024];
    snprintf(json, sizeof(json),
        "{"
        "\"hostname\":\"%s\",\"model\":\"%s\",\"arch\":\"%s\",\"device_coords\":\"%s\","
        "\"firmware_version\":\"%s\",\"kernel_version\":\"%s\",\"local_time\":\"%s\",\"uptime\":\"%s\","
        "\"load_avg\":[%.2f, %.2f, %.2f],\"app_version\":\"%s\","
        "\"disk_percent\":%d,\"disk_used\":%llu,\"disk_total\":%llu,"
        "\"mem_percent\":%d,\"mem_used\":%llu,\"mem_total\":%llu"
        "}",
        info.hostname, info.model, info.arch, info.device_coords,
        info.firmware_version, info.kernel_version, info.local_time, info.uptime,
        info.load_avg[0], info.load_avg[1], info.load_avg[2], info.app_version,
        disk_percent, info.disk_used / 1024 / 1024, info.disk_total / 1024 / 1024,
        mem_percent, info.mem_used / 1024 / 1024, info.mem_total / 1024 / 1024);

    struct json_object *root = json_tokener_parse(json);
    if (!root) {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":\"system_info_failed\"}");
        return;
    }
    struct json_object *live = json_object_new_object(), *process = NULL;
    telemetry_add(live);
    if (json_object_object_get_ex(live, "tk8710_gw", &process)) {
        json_object_object_add(process, "version", gateway_version_read("/var/run/tk8710_gw/version.txt", "/proc"));
        json_object_object_add(root, "tk8710_gw", json_object_get(process));
    }
    json_object_put(live);
    json_object_object_add(root, "rfstatus", telemetry_rf_status("/userdata/RFStatus/RFstatus.txt"));
    json_object_object_add(root, "watchdog", watchdog_status_read());
    json_object_object_add(root, "gps_enabled", NULL);
    FILE *policy = fopen("/var/run/tk8710_gps_policy", "r");
    long pid; int enabled; unsigned long long recorded_ticks;
    if (policy) {
        if (fscanf(policy, "%ld %d %llu", &pid, &enabled, &recorded_ticks) == 3 && pid > 0 && (enabled == 0 || enabled == 1)) {
            char path[64], comm[32] = {0};
            snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
            FILE *fp = fopen(path, "r");
            if (fp) {
                int matches = fgets(comm, sizeof(comm), fp) && strcmp(comm, "tk8710_gw\n") == 0;
                fclose(fp);
                snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
                fp = fopen(path, "r");
                if (fp) {
                    char stat_text[4096]; uint64_t ticks;
                    if (matches && fgets(stat_text, sizeof(stat_text), fp) &&
                        telemetry_parse_starttime(stat_text, &ticks) == 0 && ticks == recorded_ticks)
                        json_object_object_add(root, "gps_enabled", json_object_new_boolean(enabled));
                    fclose(fp);
                }
            }
        }
        fclose(policy);
    }
    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", "%s",
                  json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

static bool is_safe_ping_target(const char *target) {
    size_t len = 0;

    if (!target || target[0] == '\0') return false;

    for (len = 0; target[len] != '\0'; len++) {
        unsigned char ch = (unsigned char)target[len];
        if (!(isalnum(ch) || ch == '.' || ch == '-')) return false;
    }

    return len > 0 && len < 128;
}

static bool parse_ping_rtt(const char *line, double *avg, double *mdev) {
    const char *eq = strchr(line, '=');
    double min_v = 0.0;
    double avg_v = 0.0;
    double max_v = 0.0;
    double mdev_v = 0.0;
    int fields = 0;

    if (!eq || !avg || !mdev) return false;

    fields = sscanf(eq + 1, " %lf/%lf/%lf/%lf", &min_v, &avg_v, &max_v, &mdev_v);
    if (fields == 4) {
        *avg = avg_v;
        *mdev = mdev_v;
        return true;
    }
    if (fields == 3) {
        *avg = avg_v;
        *mdev = max_v >= min_v ? max_v - min_v : 0.0;
        return true;
    }

    return false;
}

static void update_network_quality_cache(void) {
    char target[128] = {0};
    char line[256] = {0};
    char cmd[256] = {0};
    double avg_latency = 0.0;
    double jitter = 0.0;
    int transmitted = 0;
    int received = 0;
    int loss_percent = 100;
    bool has_rtt = false;
    bool ok = false;
    const char *status = "timeout";

    strncpy(target, app_config.mqtt_host, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    if (!is_safe_ping_target(target)) {
        status = "invalid_target";
    } else {
        snprintf(cmd, sizeof(cmd), "ping -c 4 -W 1 %s 2>&1", target);
        FILE *fp = popen(cmd, "r");
        if (!fp) {
            status = "exec_failed";
        } else {
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "packets transmitted")) {
                    if (sscanf(line, "%d packets transmitted, %d packets received", &transmitted, &received) != 2) {
                        sscanf(line, "%d packets transmitted, %d received", &transmitted, &received);
                    }
                } else if (strstr(line, "rtt ") || strstr(line, "round-trip ")) {
                    has_rtt = parse_ping_rtt(line, &avg_latency, &jitter);
                }
            }
            pclose(fp);

            if (transmitted > 0) {
                loss_percent = (int)(((transmitted - received) * 100) / transmitted);
                if (loss_percent < 0) loss_percent = 0;
                if (loss_percent > 100) loss_percent = 100;
            }

            if (has_rtt && received > 0) {
                ok = true;
                status = "ok";
            }
        }
    }

    pthread_mutex_lock(&network_quality_lock);
    cached_network_quality_ready = true;
    cached_network_quality_ok = ok;
    strncpy(cached_network_quality_target, target, sizeof(cached_network_quality_target) - 1);
    cached_network_quality_target[sizeof(cached_network_quality_target) - 1] = '\0';
    strncpy(cached_network_quality_status, status, sizeof(cached_network_quality_status) - 1);
    cached_network_quality_status[sizeof(cached_network_quality_status) - 1] = '\0';
    cached_network_quality_avg = ok ? avg_latency : 0.0;
    cached_network_quality_jitter = ok ? jitter : 0.0;
    cached_network_quality_loss = loss_percent;
    cached_network_quality_timestamp = time(NULL);
    pthread_mutex_unlock(&network_quality_lock);
}

static void *network_quality_worker(void *arg) {
    (void)arg;
    while (1) {
        update_network_quality_cache();
        sleep(8);
    }
    return NULL;
}

static void start_network_quality_worker(void) {
    pthread_t thread;

    if (network_quality_worker_started) return;
    network_quality_worker_started = true;

    if (pthread_create(&thread, NULL, network_quality_worker, NULL) != 0) {
        network_quality_worker_started = false;
        return;
    }
    pthread_detach(thread);
}

static void handle_api_network_quality(struct mg_connection *c) {
    char target[128] = {0};
    char target_json[256] = {0};
    char status[32] = {0};
    bool ready = false;
    bool ok = false;
    double avg_latency = 0.0;
    double jitter = 0.0;
    int loss_percent = 100;

    pthread_mutex_lock(&network_quality_lock);
    ready = cached_network_quality_ready;
    ok = cached_network_quality_ok;
    strncpy(target, cached_network_quality_target, sizeof(target) - 1);
    strncpy(status, cached_network_quality_status, sizeof(status) - 1);
    avg_latency = cached_network_quality_avg;
    jitter = cached_network_quality_jitter;
    loss_percent = cached_network_quality_loss;
    pthread_mutex_unlock(&network_quality_lock);

    target[sizeof(target) - 1] = '\0';
    status[sizeof(status) - 1] = '\0';
    json_escape(target, target_json, sizeof(target_json));

    if (!ready) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"ok\":false,\"target\":\"%s\",\"status\":\"pending\",\"avg_latency_ms\":0,\"jitter_ms\":0,\"loss_percent\":100}",
                      target_json);
        return;
    }

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"ok\":%s,\"target\":\"%s\",\"status\":\"%s\",\"avg_latency_ms\":%.1f,\"jitter_ms\":%.1f,\"loss_percent\":%d}",
                  ok ? "true" : "false", target_json, status, avg_latency, jitter, loss_percent);
}

static void handle_api_traffic(struct mg_connection *c) {
    FILE *fp = fopen("/proc/net/dev", "r");
    unsigned long long rx_total = 0, tx_total = 0;
    if (fp) {
        char line[256];
        // skip 2 header lines
        if (fgets(line, sizeof(line), fp) && fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char iface[32] = {0};
                unsigned long long rx_bytes = 0, tx_bytes = 0;
                unsigned long long skip1 = 0, skip2 = 0, skip3 = 0, skip4 = 0, skip5 = 0, skip6 = 0, skip7 = 0;
                // format: " eth0: 123 1 0 0 0 0 0 0 456 2 0 0 0 0 0 0"
                // /proc/net/dev: iface: rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame rx_compressed rx_multicast tx_bytes ...
                if (sscanf(line, " %31[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           iface, &rx_bytes, &skip1, &skip2, &skip3, &skip4, &skip5, &skip6, &skip7, &tx_bytes) == 10) {
                    if (strcmp(iface, "lo") != 0) {
                        rx_total += rx_bytes;
                        tx_total += tx_bytes;
                    }
                }
            }
        }
        fclose(fp);
    }
    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                 "{\"rx_bytes\":%llu,\"tx_bytes\":%llu}", rx_total, tx_total);
}

static void handle_api_traffic_stats(struct mg_connection *c) {
    struct json_object *root = build_traffic_stats_json();
    if (!root) {
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"ok\":false,\"status\":\"alloc_failed\",\"interfaces\":{}}");
        return;
    }
    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s", json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

static void handle_api_traffic_stats_reset(struct mg_connection *c, struct mg_http_message *hm) {
    char iface[16] = {0};

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "Content-Type: application/json; charset=utf-8\r\n", "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
        return;
    }

    if (mg_http_get_var(&hm->query, "iface", iface, sizeof(iface)) <= 0 || !is_traffic_iface_allowed(iface)) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n", "{\"status\":\"error\",\"message\":\"invalid iface\"}");
        return;
    }

    if (!reset_traffic_iface_baseline(iface)) {
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n", "{\"status\":\"error\",\"message\":\"reset failed\"}");
        return;
    }

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", "{\"status\":\"ok\",\"iface\":\"%s\"}", iface);
}

static bool rf_stats_extract_value(struct json_object *root, const char *key, long long *value) {
    struct json_object *val = NULL;
    if (!root || !key || !value) return false;
    if (!json_object_object_get_ex(root, key, &val)) return false;
    *value = (long long) json_object_get_int64(val);
    return true;
}

static long long rf_stats_apply_baseline(long long current, long long baseline) {
    if (current < baseline) return 0;
    return current - baseline;
}

static void handle_api_rf_stats(struct mg_connection *c) {
    struct json_object *root = json_object_from_file(RF_STATS_PATH);
    struct json_object *baseline_root = json_object_from_file(RF_STATS_BASELINE_PATH);
    long long raw_downlink = 0;
    long long raw_uplink = 0;
    long long baseline_downlink = 0;
    long long baseline_uplink = 0;
    long long downlink = 0;
    long long uplink = 0;

    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        if (baseline_root) json_object_put(baseline_root);
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"stats unavailable\",\"data\":{\"mqtt_sub_msg_count\":0,\"spi_data_total_count\":0}}");
        return;
    }

    rf_stats_extract_value(root, "mqtt_sub_msg_count", &raw_downlink);
    rf_stats_extract_value(root, "spi_data_total_count", &raw_uplink);
    if (baseline_root && json_object_is_type(baseline_root, json_type_object)) {
        rf_stats_extract_value(baseline_root, "mqtt_sub_msg_count", &baseline_downlink);
        rf_stats_extract_value(baseline_root, "spi_data_total_count", &baseline_uplink);
    }
    downlink = rf_stats_apply_baseline(raw_downlink, baseline_downlink);
    uplink = rf_stats_apply_baseline(raw_uplink, baseline_uplink);
    json_object_put(root);
    if (baseline_root) json_object_put(baseline_root);

    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"ok\",\"data\":{\"mqtt_sub_msg_count\":%lld,\"spi_data_total_count\":%lld,\"raw_mqtt_sub_msg_count\":%lld,\"raw_spi_data_total_count\":%lld,\"baseline_mqtt_sub_msg_count\":%lld,\"baseline_spi_data_total_count\":%lld}}",
                  downlink, uplink, raw_downlink, raw_uplink, baseline_downlink, baseline_uplink);
}

static void handle_api_rf_stats_reset(struct mg_connection *c, struct mg_http_message *hm) {
    struct json_object *root = NULL;
    struct json_object *baseline_root = NULL;
    long long raw_downlink = 0;
    long long raw_uplink = 0;

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
        return;
    }

    root = json_object_from_file(RF_STATS_PATH);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"stats unavailable\"}");
        return;
    }

    rf_stats_extract_value(root, "mqtt_sub_msg_count", &raw_downlink);
    rf_stats_extract_value(root, "spi_data_total_count", &raw_uplink);
    json_object_put(root);

    baseline_root = json_object_new_object();
    if (!baseline_root) {
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"alloc failed\"}");
        return;
    }

    json_object_object_add(baseline_root, "mqtt_sub_msg_count", json_object_new_int64(raw_downlink));
    json_object_object_add(baseline_root, "spi_data_total_count", json_object_new_int64(raw_uplink));
    json_object_object_add(baseline_root, "timestamp", json_object_new_int64((int64_t)time(NULL)));

    if (!write_json_atomic_to_path(baseline_root, RF_STATS_BASELINE_TMP_PATH, RF_STATS_BASELINE_PATH)) {
        json_object_put(baseline_root);
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"reset failed\"}");
        return;
    }

    json_object_put(baseline_root);
    mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                  "{\"status\":\"ok\",\"data\":{\"mqtt_sub_msg_count\":0,\"spi_data_total_count\":0}}");
}

static void handle_rf(struct mg_connection *c) {
    send_page_start(c, "射频", "rf");
    send_chunk(c, "%s", RES_HTML_RF_HTML);
    send_page_end(c);
}

// 状态页面
static void handle_status(struct mg_connection *c) {
    SystemInfo info;
    system_get_info(&info);
    
    // 获取网络信息 (模拟 eth0)
    NetworkConfig net_info;
    network_load_config(&net_info);
    
    send_page_start(c, "状态", "status");
    
    // 计算百分比
    int disk_percent = 0;
    if (info.disk_total > 0) {
        disk_percent = (int)((double)info.disk_used / info.disk_total * 100);
    }
    
    int mem_percent = 0;
    if (info.mem_total > 0) {
        mem_percent = (int)((double)info.mem_used / info.mem_total * 100);
    }

    send_chunk(c, "<div class=\"status-layout\">");

    send_chunk(c, "<div class=\"status-top-row\">");
    send_chunk(c, RES_HTML_STATUS_SYSTEM_HTML,
             info.hostname, info.model, info.arch, info.device_coords[0] ? info.device_coords : "--", info.firmware_version, info.kernel_version,
             info.local_time, info.uptime, info.load_avg[0], info.load_avg[1], info.load_avg[2], info.app_version);
    send_chunk(c, RES_HTML_STATUS_TRAFFIC_HTML);
    send_chunk(c, "</div>");

    send_chunk(c, "<div class=\"status-bottom-row\">");

    // 网络信息面板
    send_chunk(c, RES_HTML_STATUS_NETWORK_HTML,
             strcmp(net_info.type, "dhcp") == 0 ? "DHCP 客户端" : "Static (静态地址)");

    send_chunk(c, RES_HTML_STATUS_RESOURCES_HTML,
             disk_percent, disk_percent, info.disk_used / 1024 / 1024, info.disk_total / 1024 / 1024,
             mem_percent, mem_percent, info.mem_used / 1024 / 1024, info.mem_total / 1024 / 1024);

    send_chunk(c, RES_HTML_STATUS_TRAFFIC_STATS_HTML);

    send_chunk(c, "</div></div>");

    send_page_end(c);
}

// 配置页面
static void handle_config(struct mg_connection *c, struct mg_http_message *hm) {
    if (mg_vcmp(&hm->method, "POST") == 0) {
        char buf[128];
        if (mg_http_get_var(&hm->body, "mqtt_host", buf, sizeof(buf)) > 0) strncpy(app_config.mqtt_host, buf, sizeof(app_config.mqtt_host));
        if (mg_http_get_var(&hm->body, "mqtt_port", buf, sizeof(buf)) > 0) app_config.mqtt_port = atoi(buf);
        if (mg_http_get_var(&hm->body, "username", buf, sizeof(buf)) > 0) strncpy(app_config.username, buf, sizeof(app_config.username));
        if (mg_http_get_var(&hm->body, "password", buf, sizeof(buf)) > 0) strncpy(app_config.password, buf, sizeof(app_config.password));
        if (mg_http_get_var(&hm->body, "gateway_id", buf, sizeof(buf)) > 0) strncpy(app_config.gateway_id, buf, sizeof(app_config.gateway_id));
        if (mg_http_get_var(&hm->body, "pub_topic", buf, sizeof(buf)) > 0) strncpy(app_config.pub_topic, buf, sizeof(app_config.pub_topic));
        if (mg_http_get_var(&hm->body, "sub_topic", buf, sizeof(buf)) > 0) strncpy(app_config.sub_topic, buf, sizeof(app_config.sub_topic));
        if (mg_http_get_var(&hm->body, "heartbeat_interval", buf, sizeof(buf)) > 0) app_config.heartbeat_interval = atoi(buf);
        if (mg_http_get_var(&hm->body, "qos", buf, sizeof(buf)) > 0) app_config.qos = atoi(buf);
        if (mg_http_get_var(&hm->body, "antenna_count", buf, sizeof(buf)) > 0) app_config.antenna_count = atoi(buf);

        bool eth_checked = mg_http_get_var(&hm->body, "net_eth", buf, sizeof(buf)) > 0;
        bool wifi_checked = mg_http_get_var(&hm->body, "net_wifi", buf, sizeof(buf)) > 0;
        bool g4_checked = mg_http_get_var(&hm->body, "net_4g", buf, sizeof(buf)) > 0;
        char enabled[32] = {0};
        char disabled[32] = {0};
        if (eth_checked) append_csv_token(enabled, sizeof(enabled), "ETH");
        if (wifi_checked) append_csv_token(enabled, sizeof(enabled), "WiFi");
        if (g4_checked) append_csv_token(enabled, sizeof(enabled), "4G");
        if (!eth_checked) append_csv_token(disabled, sizeof(disabled), "ETH");
        if (!wifi_checked) append_csv_token(disabled, sizeof(disabled), "wlan0");
        if (!g4_checked) append_csv_token(disabled, sizeof(disabled), "usb0");
        strncpy(app_config.net_interface, enabled, sizeof(app_config.net_interface) - 1);
        app_config.net_interface[sizeof(app_config.net_interface) - 1] = '\0';

        if (config_save(&app_config) && config_sync_tdaemon_disabled(disabled)) {
            reply_redirect(c, "/config?saved=1");
        } else {
            reply_html(c, 500, "Failed to save config");
        }
    } else {
        char saved[8] = {0};
        const char *notice_display = "none";

        if (mg_http_get_var(&hm->query, "saved", saved, sizeof(saved)) > 0 && strcmp(saved, "1") == 0) {
            notice_display = "block";
        }

        send_page_start(c, "服务配置", "config");
        
        send_chunk(c, RES_HTML_CONFIG_MQTT_HTML,
                 notice_display,
                 app_config.mqtt_host, app_config.mqtt_port, app_config.username, app_config.password, app_config.gateway_id,
                 app_config.pub_topic, app_config.sub_topic, app_config.heartbeat_interval, app_config.qos,
                 net_iface_checked(app_config.net_interface, "ETH") ? "checked" : "",
                 net_iface_checked(app_config.net_interface, "WiFi") ? "checked" : "",
                 net_iface_checked(app_config.net_interface, "4G") ? "checked" : "",
                 app_config.antenna_count == 1 ? "selected" : "",
                 app_config.antenna_count == 2 ? "selected" : "",
                 app_config.antenna_count == 3 ? "selected" : "",
                 app_config.antenna_count == 4 ? "selected" : "",
                 app_config.antenna_count == 5 ? "selected" : "",
                 app_config.antenna_count == 6 ? "selected" : "",
                 app_config.antenna_count == 7 ? "selected" : "",
                 app_config.antenna_count == 8 ? "selected" : "");

        send_page_end(c);
    }
}

// 网络页面
static void handle_network(struct mg_connection *c, struct mg_http_message *hm) {
    char ping_result[2048] = {0};
    char ping_target[64] = {0};
    NetworkConfig net_config;
    char network_message_display[8] = "none";
    char wifi_ssid[64] = {0};
    char wifi_psk[64] = {0};
    char wifi_status[64] = {0};
    char wifi_message[256] = {0};
    char wifi_message_class[32] = {0};
    char wifi_message_display[8] = "none";
    char wifi_modal_open[2] = "0";
    
    // 默认加载当前配置
    network_load_config(&net_config);
    wifi_load_config(wifi_ssid, sizeof(wifi_ssid), wifi_psk, sizeof(wifi_psk));
    if (wifi_is_connected()) {
        strncpy(wifi_status, "WiFi已连接", sizeof(wifi_status));
    } else {
        strncpy(wifi_status, "WiFi未连接", sizeof(wifi_status));
    }

    if (mg_vcmp(&hm->method, "POST") == 0) {
        // ... (POST 处理逻辑不变) ...
        char action[32] = {0};
        if (mg_http_get_var(&hm->body, "wifi_action", action, sizeof(action)) > 0) {
            char ssid[64] = {0};
            char psk[64] = {0};
            mg_http_get_var(&hm->body, "wifi_ssid", ssid, sizeof(ssid));
            mg_http_get_var(&hm->body, "wifi_psk", psk, sizeof(psk));
            char err[128] = {0};
            if (wifi_save_config(ssid, psk, err, sizeof(err))) {
                strncpy(wifi_message, "WiFi配置已保存，设备重启后生效", sizeof(wifi_message));
                strncpy(wifi_message_class, "alert-success", sizeof(wifi_message_class));
            } else {
                snprintf(wifi_message, sizeof(wifi_message), "保存失败: %s", err);
                strncpy(wifi_message_class, "alert-danger", sizeof(wifi_message_class));
            }
            strncpy(wifi_message_display, "block", sizeof(wifi_message_display));
            strncpy(wifi_modal_open, "1", sizeof(wifi_modal_open));
            strncpy(wifi_ssid, ssid, sizeof(wifi_ssid));
            strncpy(wifi_psk, psk, sizeof(wifi_psk));
            if (wifi_is_connected()) {
                strncpy(wifi_status, "WiFi已连接", sizeof(wifi_status));
            } else {
                strncpy(wifi_status, "WiFi未连接", sizeof(wifi_status));
            }
        } else if (mg_http_get_var(&hm->body, "ping_target", ping_target, sizeof(ping_target)) > 0) {
            if (!network_ping(ping_target, ping_result, sizeof(ping_result))) {
                snprintf(ping_result, sizeof(ping_result), "Ping failed to execute.");
            }
        } 
        else {
            memset(&net_config, 0, sizeof(net_config));
            char buf[64];
            if (mg_http_get_var(&hm->body, "proto", buf, sizeof(buf)) > 0) strncpy(net_config.type, buf, sizeof(net_config.type));
            if (mg_http_get_var(&hm->body, "ip", buf, sizeof(buf)) > 0) strncpy(net_config.ip, buf, sizeof(net_config.ip));
            if (mg_http_get_var(&hm->body, "mask", buf, sizeof(buf)) > 0) strncpy(net_config.mask, buf, sizeof(net_config.mask));
            if (mg_http_get_var(&hm->body, "gateway", buf, sizeof(buf)) > 0) strncpy(net_config.gateway, buf, sizeof(net_config.gateway));
            
            if (network_save_config(&net_config)) {
                 system("(sleep 6; reboot) &");
                 reply_redirect(c, "/network?saved=1");
            } else {
                 reply_html(c, 500, "Failed to save network config");
            }
            return;
        }
    }

    if (mg_http_get_var(&hm->query, "saved", ping_target, sizeof(ping_target)) > 0 && strcmp(ping_target, "1") == 0) {
        strncpy(network_message_display, "block", sizeof(network_message_display));
        ping_target[0] = '\0';
    }

    send_page_start(c, "网络设置", "network");
    
    char wifi_ssid_html[128] = {0};
    char wifi_psk_html[128] = {0};
    char wifi_status_html[128] = {0};
    char wifi_message_html[512] = {0};
    html_escape(wifi_ssid, wifi_ssid_html, sizeof(wifi_ssid_html));
    html_escape(wifi_psk, wifi_psk_html, sizeof(wifi_psk_html));
    html_escape(wifi_status, wifi_status_html, sizeof(wifi_status_html));
    html_escape(wifi_message, wifi_message_html, sizeof(wifi_message_html));

    send_chunk(c, RES_HTML_NETWORK_INTERFACE_HTML,
             network_message_display,
             strcmp(net_config.type, "dhcp") == 0 ? "selected" : "",
             strcmp(net_config.type, "static") == 0 ? "selected" : "",
             net_config.ip, net_config.mask, net_config.gateway,
             wifi_ssid_html, wifi_psk_html, wifi_status_html,
             wifi_message_display, wifi_message_class, wifi_message_html, wifi_modal_open, network_message_display);
             
    send_chunk(c, RES_HTML_NETWORK_DIAGNOSTICS_HTML,
             ping_target, ping_result);

    send_page_end(c);
}

// 升级页面
static void handle_upgrade(struct mg_connection *c, struct mg_http_message *hm) {
    if (mg_vcmp(&hm->method, "POST") == 0) {
        // 检查是否是重启请求
        char buf[32];
        if (mg_http_get_var(&hm->body, "action", buf, sizeof(buf)) > 0 && strcmp(buf, "reboot") == 0) {
             mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\n", "Device is rebooting...");
             system("(sleep 1; reboot) &");
             return;
        }
        
        // POST /upload is handled by upload handler below, not here
    } else {
        send_page_start(c, "系统升级", "upgrade");
        
        // 直接发送 HTML 内容，避免格式化字符干扰和缓冲区限制
        send_html_raw(c, RES_HTML_UPGRADE_FIRMWARE_HTML);
        send_html_raw(c, RES_HTML_UPGRADE_LOGS_HTML);
        send_html_raw(c, RES_HTML_UPGRADE_MAINTENANCE_HTML);
                 
        send_page_end(c);
    }
}

// 固件上传处理：使用分片二进制上传，避免大文件被整体缓存在内存中
static void handle_upload(struct mg_connection *c, struct mg_http_message *hm) {
    UpgradeStatus installation;
    char installation_message[256];
    upgrade_get_status(&installation, installation_message, sizeof(installation_message));
    if (installation == UPGRADE_INSTALLING || installation == UPGRADE_DONE) {
        mg_http_reply(c, 409, "Content-Type: text/plain\r\n", "Upgrade already in progress");
        return;
    }
    char file_name[128] = {0};
    char err[160] = {0};
    long offset = -1;
    long next_offset = 0;

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "", "Method not allowed");
        return;
    }

    if (mg_http_get_var(&hm->query, "file", file_name, sizeof(file_name)) <= 0) {
        mg_http_reply(c, 400, "", "file required");
        return;
    }

    if (strcmp(file_name, UPGRADE_UPLOAD_FILE) != 0) {
        mg_http_reply(c, 400, "", "invalid file");
        return;
    }

    if (!parse_query_long(&hm->query, "offset", &offset) || offset < 0) {
        mg_http_reply(c, 400, "", "offset required");
        return;
    }

    printf("[DEBUG] Upload chunk: file=%s offset=%ld len=%d\n",
           file_name, offset, (int) hm->body.len);

    if (!write_upload_chunk(UPGRADE_UPLOAD_PATH, offset, hm->body.ptr, hm->body.len,
                            &next_offset, err, sizeof(err))) {
        printf("[DEBUG] Upload chunk failed: %s\n", err);
        mg_http_reply(c, 400, "", "%s", err);
        return;
    }

    printf("[DEBUG] Upload chunk written, next_offset=%ld\n", next_offset);
    mg_http_reply(c, 200, "Content-Type: text/plain; charset=utf-8\r\n", "%ld", next_offset);
}

static void handle_upgrade_start(struct mg_connection *c, struct mg_http_message *hm) {
    struct stat st;

    if (mg_vcmp(&hm->method, "POST") != 0) {
        mg_http_reply(c, 405, "", "Method not allowed");
        return;
    }

    if (stat(UPGRADE_UPLOAD_PATH, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        mg_http_reply(c, 400, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"升级文件不存在或为空\"}");
        return;
    }

    printf("[DEBUG] Starting upgrade with file=%s size=%lld\n",
           UPGRADE_UPLOAD_PATH, (long long) st.st_size);

    if (upgrade_start(UPGRADE_UPLOAD_PATH)) {
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"ok\",\"message\":\"升级已开始\"}");
    } else {
        mg_http_reply(c, 500, "Content-Type: application/json; charset=utf-8\r\n",
                      "{\"status\":\"error\",\"message\":\"升级已在进行中或启动失败\"}");
    }
}

// 获取升级状态 API
static void handle_upgrade_status(struct mg_connection *c) {
    UpgradeStatus status;
    char msg[256];
    upgrade_get_status(&status, msg, sizeof(msg));
    char esc[512];
    json_escape(msg, esc, sizeof(esc));
    
    const char *status_str = "idle";
    if (status == UPGRADE_INSTALLING) status_str = "installing";
    else if (status == UPGRADE_DONE) status_str = "done";
    else if (status == UPGRADE_ERROR) status_str = "error";
    
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", 
                 "{\"status\": \"%s\", \"message\": \"%s\"}", status_str, esc);
}

static void event_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_http_match_uri(hm, "/login")) {
            handle_login(c, hm);
        } else if (mg_http_match_uri(hm, "/logout")) {
             reply_redirect_with_headers(c, "/login", "Set-Cookie: session=; Path=/; Max-Age=0\r\n");
        } else if (is_authed(hm)) {
            if (mg_http_match_uri(hm, "/status")) handle_status(c);
            else if (mg_http_match_uri(hm, "/config")) handle_config(c, hm);
            else if (mg_http_match_uri(hm, "/manger")) handle_manger(c, hm);
            else if (mg_http_match_uri(hm, "/api/active_ip")) handle_active_ip_api(c);
            else if (mg_http_match_uri(hm, "/api/system_info")) handle_api_system_info(c);
            else if (mg_http_match_uri(hm, "/api/traffic")) handle_api_traffic(c);
            else if (mg_http_match_uri(hm, "/api/rf_acm/start")) handle_api_rf_acm_start(c, hm);
            else if (mg_http_match_uri(hm, "/api/rf_acm/status")) handle_api_rf_acm_status(c);
            else if (mg_http_match_uri(hm, "/api/rf_acm/result")) handle_api_rf_acm_result(c);
            else if (mg_http_match_uri(hm, "/api/rf_scan/start")) handle_api_rf_scan_start(c, hm);
            else if (mg_http_match_uri(hm, "/api/rf_scan/status")) handle_api_rf_scan_status(c);
            else if (mg_http_match_uri(hm, "/api/rf_scan/result")) handle_api_rf_scan_result(c);
            else if (mg_http_match_uri(hm, "/api/rf_stats/reset")) handle_api_rf_stats_reset(c, hm);
            else if (mg_http_match_uri(hm, "/api/rf_stats")) handle_api_rf_stats(c);
            else if (mg_http_match_uri(hm, "/api/traffic_stats/reset")) handle_api_traffic_stats_reset(c, hm);
            else if (mg_http_match_uri(hm, "/api/traffic_stats")) handle_api_traffic_stats(c);
            else if (mg_http_match_uri(hm, "/api/network_quality")) handle_api_network_quality(c);
            else if (mg_http_match_uri(hm, "/network")) handle_network(c, hm);
            else if (mg_http_match_uri(hm, "/rf")) handle_rf(c);
            else if (mg_http_match_uri(hm, "/upgrade")) handle_upgrade(c, hm);
            else if (mg_http_match_uri(hm, "/upload/start")) handle_upgrade_start(c, hm);
            else if (mg_http_match_uri(hm, "/upload")) handle_upload(c, hm);
            else if (mg_http_match_uri(hm, "/api/logs")) log_export_list(c, hm);
            else if (mg_http_match_uri(hm, "/api/log_export/start")) log_export_start(c, hm);
            else if (mg_http_match_uri(hm, "/api/log_export/status")) log_export_status(c, hm);
            else if (mg_http_match_uri(hm, "/api/log_export/download")) log_export_download(c, hm);
            else if (mg_http_match_uri(hm, "/api/log_download")) mg_http_reply(c, 410, "Content-Type: text/plain; charset=utf-8\r\n", "请刷新系统页面，使用新版分类日志导出");
            else if (mg_http_match_uri(hm, "/api/upgrade_status")) handle_upgrade_status(c);
            else reply_redirect(c, "/status");
        } else {
             // 如果未认证且不是请求登录页面，则重定向到登录
            reply_redirect(c, "/login");
        }
    }
}

void web_server_start(const char *listen_addr) {
    config_load(&app_config); // 加载配置
    start_network_quality_worker();
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, listen_addr, event_handler, NULL);
    printf("Web Server started on %s\n", listen_addr);
    for (;;) mg_mgr_poll(&mgr, 100);
    mg_mgr_free(&mgr);
}
