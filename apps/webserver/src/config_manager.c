#include "config_manager.h"
#include "file_logger.h"
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <errno.h>

#define WEB_FILE_LOG "/var/log/web_server.log"
#include <stdlib.h>
#include <sys/stat.h>

#define CONFIG_FILE "/etc/config.json"
#define TDAEMON_CONF_FILE "/etc/tdaemon.conf"
#define DEVICE_VERSION_FILE "/etc/device_version.json"

static const char *get_string(struct json_object *obj, const char *key, const char *def) {
    struct json_object *val;
    if (json_object_object_get_ex(obj, key, &val)) {
        return json_object_get_string(val);
    }
    return def;
}

static int get_int(struct json_object *obj, const char *key, int def) {
    struct json_object *val;
    if (json_object_object_get_ex(obj, key, &val)) {
        return json_object_get_int(val);
    }
    return def;
}

static bool load_device_version(AppConfig *config) {
    struct json_object *root = json_object_from_file(DEVICE_VERSION_FILE);
    if (!root) return false;

    const char *comm = get_string(root, "comm_version", config->comm_version);
    if (comm) strncpy(config->comm_version, comm, sizeof(config->comm_version) - 1);

    const char *app = get_string(root, "app_version", config->app_version);
    if (app) strncpy(config->app_version, app, sizeof(config->app_version) - 1);

    const char *build = get_string(root, "build_time", config->build_time);
    if (build) strncpy(config->build_time, build, sizeof(config->build_time) - 1);

    const char *commit = get_string(root, "git_commit", config->git_commit);
    if (commit) strncpy(config->git_commit, commit, sizeof(config->git_commit) - 1);

    // 向后兼容：旧逻辑使用 firmware_version，映射为 comm_version
    strncpy(config->firmware_version, config->comm_version, sizeof(config->firmware_version) - 1);
    config->firmware_version[sizeof(config->firmware_version) - 1] = '\0';

    json_object_put(root);
    return true;
}

bool config_load(AppConfig *config) {
    // 设置默认值
    memset(config, 0, sizeof(AppConfig));
    strcpy(config->mqtt_host, "127.0.0.1");
    config->mqtt_port = 1883;
    strcpy(config->username, "user");
    strcpy(config->password, "666666");
    strcpy(config->gateway_id, "GW123456");
    strcpy(config->pub_topic, "device/pub");
    strcpy(config->sub_topic, "device/sub");
    config->heartbeat_interval = 60;
    config->qos = 0;
    /* New/unconfigured gateways participate in all three failover candidates. */
    strcpy(config->net_interface, "ETH,WiFi,4G");
    config->antenna_count = 8;
    strcpy(config->web_password, "admin");
    
    // System info defaults
    strcpy(config->hostname, "TurMassLink");
    strcpy(config->model, "RK3506G2 Board");
    strcpy(config->arch, "ARMv7");
    strcpy(config->platform, "imx/cortexa7");
    strcpy(config->kernel_version, "6.1");
    strcpy(config->comm_version, "mqtt_spi_v1.0.0");
    strcpy(config->firmware_version, "v1.0_202601");
    strcpy(config->app_version, "V1.0.0");
    strcpy(config->build_time, "unknown");
    strcpy(config->git_commit, "unknown");
    strncpy(config->firmware_version, config->comm_version, sizeof(config->firmware_version) - 1);
    config->firmware_version[sizeof(config->firmware_version) - 1] = '\0';

    bool loaded_config = true;
    struct json_object *root = json_object_from_file(CONFIG_FILE);
    if (!root) {
        file_logger_printf(WEB_FILE_LOG, "WARN", "Config file not found or invalid, using defaults.");
        loaded_config = false;
    } else {

    const char *host = get_string(root, "mqtt_host", config->mqtt_host);
    if (host) strncpy(config->mqtt_host, host, sizeof(config->mqtt_host) - 1);

    config->mqtt_port = get_int(root, "mqtt_port", config->mqtt_port);

    const char *user = get_string(root, "username", config->username);
    if (user) strncpy(config->username, user, sizeof(config->username) - 1);

    const char *pass = get_string(root, "password", config->password);
    if (pass) strncpy(config->password, pass, sizeof(config->password) - 1);

    const char *gw_id = get_string(root, "gateway_id", config->gateway_id);
    if (gw_id) strncpy(config->gateway_id, gw_id, sizeof(config->gateway_id) - 1);

    const char *pub = get_string(root, "pub_topic", config->pub_topic);
    if (pub) strncpy(config->pub_topic, pub, sizeof(config->pub_topic) - 1);

    const char *sub = get_string(root, "sub_topic", config->sub_topic);
    if (sub) strncpy(config->sub_topic, sub, sizeof(config->sub_topic) - 1);

    config->heartbeat_interval = get_int(root, "heartbeat_interval", config->heartbeat_interval);
    config->qos = get_int(root, "qos", config->qos);

    const char *net = get_string(root, "net_interface", config->net_interface);
    if (net) strncpy(config->net_interface, net, sizeof(config->net_interface) - 1);
    config->antenna_count = get_int(root, "antenna_count", config->antenna_count);
    const char *web_pass = get_string(root, "web_password", config->web_password);
    if (web_pass) strncpy(config->web_password, web_pass, sizeof(config->web_password) - 1);
    
    // Load system info
    const char *hostname = get_string(root, "hostname", config->hostname);
    if (hostname) strncpy(config->hostname, hostname, sizeof(config->hostname) - 1);
    
    const char *model = get_string(root, "model", config->model);
    if (model) strncpy(config->model, model, sizeof(config->model) - 1);
    
    const char *arch = get_string(root, "arch", config->arch);
    if (arch) strncpy(config->arch, arch, sizeof(config->arch) - 1);
    
    const char *platform = get_string(root, "platform", config->platform);
    if (platform) strncpy(config->platform, platform, sizeof(config->platform) - 1);
    
    const char *kver = get_string(root, "kernel_version", config->kernel_version);
    if (kver) strncpy(config->kernel_version, kver, sizeof(config->kernel_version) - 1);
    
    const char *comm = get_string(root, "comm_version", config->comm_version);
    if (comm) strncpy(config->comm_version, comm, sizeof(config->comm_version) - 1);
    const char *fver = get_string(root, "firmware_version", config->firmware_version);
    if (fver) strncpy(config->firmware_version, fver, sizeof(config->firmware_version) - 1);
    
    const char *aver = get_string(root, "app_version", config->app_version);
    if (aver) strncpy(config->app_version, aver, sizeof(config->app_version) - 1);
    const char *build = get_string(root, "build_time", config->build_time);
    if (build) strncpy(config->build_time, build, sizeof(config->build_time) - 1);
    const char *commit = get_string(root, "git_commit", config->git_commit);
    if (commit) strncpy(config->git_commit, commit, sizeof(config->git_commit) - 1);

    json_object_put(root);
    }

    load_device_version(config);
    return loaded_config;
}

bool config_save(const AppConfig *config) {
    struct json_object *root = json_object_new_object();
    if (!root) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "config save failed: alloc json object");
        return false;
    }

    json_object_object_add(root, "mqtt_host", json_object_new_string(config->mqtt_host));
    json_object_object_add(root, "mqtt_port", json_object_new_int(config->mqtt_port));
    json_object_object_add(root, "username", json_object_new_string(config->username));
    json_object_object_add(root, "password", json_object_new_string(config->password));
    json_object_object_add(root, "gateway_id", json_object_new_string(config->gateway_id));
    json_object_object_add(root, "pub_topic", json_object_new_string(config->pub_topic));
    json_object_object_add(root, "sub_topic", json_object_new_string(config->sub_topic));
    json_object_object_add(root, "heartbeat_interval", json_object_new_int(config->heartbeat_interval));
    json_object_object_add(root, "qos", json_object_new_int(config->qos));
    json_object_object_add(root, "net_interface", json_object_new_string(config->net_interface));
    json_object_object_add(root, "antenna_count", json_object_new_int(config->antenna_count));
    json_object_object_add(root, "web_password", json_object_new_string(config->web_password));
    
    // Save system info (optional, usually read-only but saving keeps consistency)
    json_object_object_add(root, "hostname", json_object_new_string(config->hostname));
    json_object_object_add(root, "model", json_object_new_string(config->model));
    json_object_object_add(root, "arch", json_object_new_string(config->arch));
    json_object_object_add(root, "platform", json_object_new_string(config->platform));
    json_object_object_add(root, "kernel_version", json_object_new_string(config->kernel_version));
    json_object_object_add(root, "comm_version", json_object_new_string(config->comm_version));
    json_object_object_add(root, "firmware_version", json_object_new_string(config->firmware_version));
    json_object_object_add(root, "app_version", json_object_new_string(config->app_version));
    json_object_object_add(root, "build_time", json_object_new_string(config->build_time));
    json_object_object_add(root, "git_commit", json_object_new_string(config->git_commit));

    int ret = json_object_to_file_ext(CONFIG_FILE, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    if (ret != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "config save failed path=%s", CONFIG_FILE);
    }
    return (ret == 0);
}

static bool append_text(char **buf, size_t *len, size_t *cap, const char *text, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t new_cap = (*cap == 0) ? 1024 : *cap;
        while (*len + n + 1 > new_cap) new_cap *= 2;
        char *new_buf = (char *)realloc(*buf, new_cap);
        if (!new_buf) return false;
        *buf = new_buf;
        *cap = new_cap;
    }
    memcpy(*buf + *len, text, n);
    *len += n;
    (*buf)[*len] = '\0';
    return true;
}

static bool write_atomic_file(const char *path, const char *content, size_t len) {
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, (int)getpid());

    FILE *out = fopen(tmp_path, "w");
    if (!out) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "open temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        return false;
    }

    if (fchmod(fileno(out), 0644) != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "chmod temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }

    if (fwrite(content, 1, len, out) != len) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "write temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }

    if (fflush(out) != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "flush temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }

    if (fsync(fileno(out)) != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "fsync temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }

    if (fclose(out) != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "close temp file failed path=%s errno=%s", tmp_path, strerror(errno));
        remove(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "rename temp file failed from=%s to=%s errno=%s", tmp_path, path, strerror(errno));
        remove(tmp_path);
        return false;
    }

    return true;
}

bool config_sync_tdaemon_disabled(const char *disabled_ifaces) {
    if (!disabled_ifaces) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "tdaemon sync failed: null disabled_ifaces");
        return false;
    }

    FILE *fp = fopen(TDAEMON_CONF_FILE, "r");
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "open tdaemon config failed path=%s errno=%s", TDAEMON_CONF_FILE, strerror(errno));
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }

    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    char *in = (char *)malloc((size_t)sz + 1);
    if (!in) {
        fclose(fp);
        return false;
    }

    size_t read_n = fread(in, 1, (size_t)sz, fp);
    fclose(fp);
    in[read_n] = '\0';

    char *out = NULL;
    size_t out_len = 0, out_cap = 0;
    bool in_network = false;
    bool found_network = false;
    bool replaced = false;

    size_t pos = 0;
    while (pos < read_n) {
        size_t line_start = pos;
        while (pos < read_n && in[pos] != '\n') pos++;
        size_t line_end = pos;
        bool has_newline = (pos < read_n && in[pos] == '\n');
        if (has_newline) pos++;

        const char *line = in + line_start;
        size_t line_len = line_end - line_start;
        size_t left = 0;
        while (left < line_len && (line[left] == ' ' || line[left] == '\t')) left++;

        if (line_len > left && line[left] == '[') {
            if (in_network && !replaced) {
                char disabled_line[128];
                int n = snprintf(disabled_line, sizeof(disabled_line), "disabled=%s\n", disabled_ifaces);
                if (n < 0 || !append_text(&out, &out_len, &out_cap, disabled_line, (size_t)n)) {
                    free(in);
                    free(out);
                    return false;
                }
                replaced = true;
            }
            if ((line_len - left) == strlen("[network]") && strncmp(line + left, "[network]", strlen("[network]")) == 0) {
                in_network = true;
                found_network = true;
            } else {
                in_network = false;
            }
        }

        if (in_network && (line_len - left) >= strlen("disabled=") &&
            strncmp(line + left, "disabled=", strlen("disabled=")) == 0) {
            char disabled_line[128];
            int n = snprintf(disabled_line, sizeof(disabled_line), "disabled=%s", disabled_ifaces);
            if (n < 0 || !append_text(&out, &out_len, &out_cap, disabled_line, (size_t)n)) {
                free(in);
                free(out);
                return false;
            }
            if (has_newline && !append_text(&out, &out_len, &out_cap, "\n", 1)) {
                free(in);
                free(out);
                return false;
            }
            replaced = true;
            continue;
        }

        if (!append_text(&out, &out_len, &out_cap, line, line_len)) {
            free(in);
            free(out);
            return false;
        }
        if (has_newline && !append_text(&out, &out_len, &out_cap, "\n", 1)) {
            free(in);
            free(out);
            return false;
        }
    }

    if (in_network && !replaced) {
        if (out_len > 0 && out[out_len - 1] != '\n') {
            if (!append_text(&out, &out_len, &out_cap, "\n", 1)) {
                free(in);
                free(out);
                return false;
            }
        }
        char disabled_line[128];
        int n = snprintf(disabled_line, sizeof(disabled_line), "disabled=%s\n", disabled_ifaces);
        if (n < 0 || !append_text(&out, &out_len, &out_cap, disabled_line, (size_t)n)) {
            free(in);
            free(out);
            return false;
        }
        replaced = true;
    }

    bool ok = found_network && replaced && write_atomic_file(TDAEMON_CONF_FILE, out, out_len);
    free(in);
    free(out);
    return ok;
}
