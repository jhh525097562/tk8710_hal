#include "network_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "file_logger.h"

#define INTERFACES_FILE "/etc/network/interfaces"
#define WEB_FILE_LOG "/var/log/web_server.log"

static bool get_runtime_ip_mask(const char *ifname, char *ip, char *mask) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    // Get IP
    if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        strcpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    } else {
        close(fd);
        return false;
    }

    // Get Netmask
    if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
        strcpy(mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    }

    close(fd);
    return true;
}

static bool get_runtime_gateway(char *gateway) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return false;

    char line[256];
    char iface[16];
    unsigned long dest, gw;
    
    // Skip header
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%s %lx %lx", iface, &dest, &gw) == 3) {
            if (strcmp(iface, "eth0") == 0 && dest == 0) { // Default route on eth0
                struct in_addr addr;
                addr.s_addr = gw;
                strcpy(gateway, inet_ntoa(addr));
                fclose(fp);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

bool network_load_config(NetworkConfig *config) {
    memset(config, 0, sizeof(NetworkConfig));
    FILE *fp = fopen(INTERFACES_FILE, "r");
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "open %s failed: %s", INTERFACES_FILE, strerror(errno));
        return false;
    }

    char line[256];
    bool parsing_eth0 = false;
    bool found_eth0 = false;

    while (fgets(line, sizeof(line), fp)) {
        // Skip comments
        if (line[0] == '#') continue;

        // Check for interface definition start
        if (strncmp(line, "iface", 5) == 0) {
            char ifname[32], method[32];
            // parsing: iface eth0 inet [static|dhcp]
            if (sscanf(line, "iface %s inet %s", ifname, method) >= 1) {
                if (strcmp(ifname, "eth0") == 0) {
                    parsing_eth0 = true;
                    found_eth0 = true;
                    if (strstr(method, "dhcp")) {
                        strcpy(config->type, "dhcp");
                    } else {
                        strcpy(config->type, "static");
                    }
                } else {
                    parsing_eth0 = false;
                }
            }
        }

        if (parsing_eth0) {
            if (strstr(line, "address")) {
                sscanf(line, " address %s", config->ip);
            }
            if (strstr(line, "netmask")) {
                sscanf(line, " netmask %s", config->mask);
            }
            if (strstr(line, "gateway")) {
                sscanf(line, " gateway %s", config->gateway);
            }
        }
    }
    fclose(fp);

    // If DHCP, try to get runtime values
    if (found_eth0 && strcmp(config->type, "dhcp") == 0) {
        // Clear potential garbage or old values
        memset(config->ip, 0, sizeof(config->ip));
        memset(config->mask, 0, sizeof(config->mask));
        memset(config->gateway, 0, sizeof(config->gateway));

        get_runtime_ip_mask("eth0", config->ip, config->mask);
        get_runtime_gateway(config->gateway);
    }

    return found_eth0;
}

bool network_save_config(const NetworkConfig *config) {
    FILE *fp = fopen(INTERFACES_FILE, "w");
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "open %s for write failed: %s", INTERFACES_FILE, strerror(errno));
        return false;
    }

    fprintf(fp, "auto lo\niface lo inet loopback\n\n");
    
    // eth0 config
    fprintf(fp, "auto eth0\n");
    if (strcmp(config->type, "dhcp") == 0) {
        fprintf(fp, "iface eth0 inet dhcp\n");
    } else {
        fprintf(fp, "iface eth0 inet static\n");
        fprintf(fp, " address %s\n", config->ip);
        fprintf(fp, " netmask %s\n", config->mask);
        fprintf(fp, " gateway %s\n", config->gateway);
    }
    
    // eth0:0 config (Preserve user's fixed IP)
    fprintf(fp, "\n#固定的IP用于直连外部网口设备\n");
    fprintf(fp, "auto eth0:0\n");
    fprintf(fp, "iface eth0:0 inet static\n");
    fprintf(fp, " address 12.13.14.16\n");
    fprintf(fp, " netmask 255.255.255.0\n");

    fclose(fp);
    return true;
}

bool network_ping(const char *target, char *result, size_t len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c 4 %s", target);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "popen ping failed target=%s errno=%s", target, strerror(errno));
        return false;
    }

    size_t offset = 0;
    char buf[128];
    while (fgets(buf, sizeof(buf), fp) && offset < len - 1) {
        size_t n = strlen(buf);
        if (offset + n >= len - 1) break;
        strcpy(result + offset, buf);
        offset += n;
    }
    result[offset] = '\0';
    pclose(fp);
    return true;
}

static bool parse_config_value(const char *line, const char *key, char *out, size_t out_len) {
    size_t key_len = strlen(key);
    const char *pos = strstr(line, key);
    if (!pos) return false;
    pos += key_len;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '=') return false;
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos == '\"') {
        pos++;
        const char *end = strchr(pos, '\"');
        if (!end) return false;
        size_t len = (size_t)(end - pos);
        if (len >= out_len) len = out_len - 1;
        strncpy(out, pos, len);
        out[len] = '\0';
        return true;
    }
    size_t len = 0;
    while (pos[len] != '\0' && pos[len] != '\n' && pos[len] != '\r' && len + 1 < out_len) len++;
    strncpy(out, pos, len);
    out[len] = '\0';
    return len > 0;
}

bool wifi_load_config(char *ssid, size_t ssid_len, char *psk, size_t psk_len) {
    if (ssid_len > 0) ssid[0] = '\0';
    if (psk_len > 0) psk[0] = '\0';
    FILE *fp = fopen("/etc/wpa_supplicant.conf", "r");
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "ERROR", "open /etc/wpa_supplicant.conf failed: %s", strerror(errno));
        return false;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (parse_config_value(line, "ssid", ssid, ssid_len)) continue;
        if (parse_config_value(line, "psk", psk, psk_len)) continue;
    }
    fclose(fp);
    return true;
}

static void build_default_wpa_header(char *country, size_t country_len, char *ctrl, size_t ctrl_len, char *update, size_t update_len) {
    strncpy(country, "CN", country_len);
    country[country_len - 1] = '\0';
    strncpy(ctrl, "/var/run/wpa_supplicant", ctrl_len);
    ctrl[ctrl_len - 1] = '\0';
    strncpy(update, "1", update_len);
    update[update_len - 1] = '\0';
}

bool wifi_save_config(const char *ssid, const char *psk, char *err, size_t err_len) {
    if (!ssid || ssid[0] == '\0') {
        snprintf(err, err_len, "WiFi 名称不能为空");
        file_logger_write(WEB_FILE_LOG, "WARN", "wifi save failed: empty ssid");
        return false;
    }
    if (!psk || strlen(psk) < 8 || strlen(psk) > 63) {
        snprintf(err, err_len, "WiFi 密码长度需要 8-63 位");
        file_logger_write(WEB_FILE_LOG, "WARN", "wifi save failed: invalid password length");
        return false;
    }
    if (strchr(ssid, '\"') || strchr(psk, '\"') || strchr(ssid, '\n') || strchr(psk, '\n')) {
        snprintf(err, err_len, "WiFi 配置包含非法字符");
        file_logger_write(WEB_FILE_LOG, "WARN", "wifi save failed: invalid chars");
        return false;
    }

    const char *path = "/etc/wpa_supplicant.conf";
    bool exists = (access(path, F_OK) == 0);
    if (exists && access(path, W_OK) != 0) {
        snprintf(err, err_len, "配置文件无写权限: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "wpa_supplicant.conf no write permission: %s", strerror(errno));
        return false;
    }
    if (!exists && access("/etc", W_OK) != 0) {
        snprintf(err, err_len, "/etc 无写权限: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "/etc no write permission: %s", strerror(errno));
        return false;
    }

    char country[8], ctrl[128], update[8];
    build_default_wpa_header(country, sizeof(country), ctrl, sizeof(ctrl), update, sizeof(update));

    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (parse_config_value(line, "country", country, sizeof(country))) continue;
            if (parse_config_value(line, "ctrl_interface", ctrl, sizeof(ctrl))) continue;
            if (parse_config_value(line, "update_config", update, sizeof(update))) continue;
        }
        fclose(fp);
    }

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, (int)getpid());
    FILE *out = fopen(tmp_path, "w");
    if (!out) {
        snprintf(err, err_len, "创建临时文件失败: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "create wifi temp config failed: %s", strerror(errno));
        return false;
    }

    if (fchmod(fileno(out), 0600) != 0) {
        snprintf(err, err_len, "设置文件权限失败: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "chmod wifi temp config failed: %s", strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }

    fprintf(out, "country=%s\n", country);
    fprintf(out, "ctrl_interface=%s\n", ctrl);
    fprintf(out, "update_config=%s\n\n", update);
    fprintf(out, "network={\n");
    fprintf(out, "\tssid=\"%s\"\n", ssid);
    fprintf(out, "\tpsk=\"%s\"\n", psk);
    fprintf(out, "\tkey_mgmt=WPA-PSK\n");
    fprintf(out, "\tpriority=1\n");
    fprintf(out, "}\n");

    if (fflush(out) != 0) {
        snprintf(err, err_len, "写入配置失败: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "flush wifi config failed: %s", strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }
    if (fsync(fileno(out)) != 0) {
        snprintf(err, err_len, "同步配置失败: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "fsync wifi config failed: %s", strerror(errno));
        fclose(out);
        remove(tmp_path);
        return false;
    }
    fclose(out);

    if (rename(tmp_path, path) != 0) {
        snprintf(err, err_len, "替换配置失败: %s", strerror(errno));
        file_logger_write(WEB_FILE_LOG, "ERROR", "rename wifi config failed: %s", strerror(errno));
        remove(tmp_path);
        return false;
    }

    return true;
}

bool wifi_is_connected(void) {
    FILE *fp = popen("ifconfig wlan0 | grep \"inet \"", "r");
    if (!fp) return false;
    char line[128] = {0};
    bool has_inet = (fgets(line, sizeof(line), fp) != NULL);
    pclose(fp);
    return has_inet;
}
