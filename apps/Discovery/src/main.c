#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <json-c/json.h>
#include "discovery.h"
#include "log_udp.h"
#include "file_logger.h"

#define DISCOVERY_FILE_LOG "/var/log/lan_discoveryd.log"

static void discovery_log_send(int level, const char *msg, time_t ts) {
    log_send(level, msg, ts);
    file_logger_write(DISCOVERY_FILE_LOG, level > 0 ? "ERROR" : "INFO", "%s", msg ? msg : "");
}

// 获取默认路由所在的网卡名称 (例如 eth0 或 wlan0)
int get_default_iface(char *iface_name, size_t max_len) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return -1;

    char line[256];
    char iface[IFNAMSIZ];
    unsigned long dest;
    
    // 跳过第一行表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%s %lx", iface, &dest) == 2) {
            if (dest == 0) { // 目标网络为 00000000 的即为默认路由
                strncpy(iface_name, iface, max_len);
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return -1;
}

// 获取指定网卡的 IP 和 MAC
int get_iface_info(const char *iface, char *ip_buf, char *mac_buf) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    // 获取 IP
    if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
        strcpy(ip_buf, inet_ntoa(sin->sin_addr));
    } else {
        strcpy(ip_buf, "0.0.0.0");
    }

    // 获取 MAC
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
        unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
        sprintf(mac_buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        strcpy(mac_buf, "00:00:00:00:00:00");
    }

    close(fd);
    return 0;
}

// 从配置文件读取 GWID
int get_gwid(char *gwid_buf, size_t max_len) {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) {
        strncpy(gwid_buf, "unknown_gwid", max_len);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = malloc(length + 1);
    if (data) {
        fread(data, 1, length, fp);
        data[length] = '\0';
        
        struct json_object *parsed_json;
        struct json_object *gwid_obj;
        
        parsed_json = json_tokener_parse(data);
        if (parsed_json != NULL) {
            if (json_object_object_get_ex(parsed_json, "gateway_id", &gwid_obj)) {
                strncpy(gwid_buf, json_object_get_string(gwid_obj), max_len);
            } else {
                strncpy(gwid_buf, "unknown_gwid", max_len);
            }
            json_object_put(parsed_json);
        } else {
            strncpy(gwid_buf, "unknown_gwid", max_len);
        }
        free(data);
    }
    fclose(fp);
    return 0;
}

// 构建应答 JSON 字符串，使用完返回的字符串需要 free()
char* build_response_json(const char* iface, const char* ip, const char* mac, const char* gwid) {
    struct json_object *jobj = json_object_new_object();
    
    json_object_object_add(jobj, "cmd", json_object_new_string("discovery_rsp"));
    json_object_object_add(jobj, "gwid", json_object_new_string(gwid));
    json_object_object_add(jobj, "active_iface", json_object_new_string(iface));
    json_object_object_add(jobj, "ip", json_object_new_string(ip));
    json_object_object_add(jobj, "mac", json_object_new_string(mac));
    
    const char *json_str = json_object_to_json_string(jobj);
    char *result = strdup(json_str);
    
    json_object_put(jobj); // 释放 json 对象
    return result;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUF_SIZE];

    // 创建 UDP Socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        discovery_log_send(1, "Socket creation failed", time(NULL));
        exit(EXIT_FAILURE);
    }

    // 设置 SO_REUSEADDR，允许重启后立即绑定端口
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    // 绑定端口
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        discovery_log_send(1, "Bind failed", time(NULL));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char start_log[128];
    snprintf(start_log, sizeof(start_log), "Discovery daemon listening on UDP port %d...", LISTEN_PORT);
    discovery_log_send(0, start_log, time(NULL));

    while (1) {
        memset(buffer, 0, MAX_BUF_SIZE);
        int n = recvfrom(sockfd, buffer, MAX_BUF_SIZE - 1, 0, 
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) continue;

        buffer[n] = '\0';
        
        struct json_object *parsed_json;
        struct json_object *cmd_obj;
        struct json_object *magic_obj;

        // 解析收到的 JSON
        parsed_json = json_tokener_parse(buffer);
        if (parsed_json == NULL) continue;

        // 验证 JSON 中是否包含指定的 key
        if (json_object_object_get_ex(parsed_json, "cmd", &cmd_obj) && 
            json_object_object_get_ex(parsed_json, "magic", &magic_obj)) {
            
            const char *cmd_str = json_object_get_string(cmd_obj);
            const char *magic_str = json_object_get_string(magic_obj);

            if (cmd_str && magic_str && 
                strcmp(cmd_str, "discovery_req") == 0 && 
                strcmp(magic_str, MAGIC_WORD) == 0) {
                
                char log_buf[256];
                snprintf(log_buf, sizeof(log_buf), "Received valid discovery request from %s:%d", 
                         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                discovery_log_send(0, log_buf, time(NULL));

                // 1. 获取默认路由网卡
                char iface[IFNAMSIZ] = "unknown";
                get_default_iface(iface, sizeof(iface));

                // 2. 获取该网卡的 IP 和 MAC
                char ip[32] = "0.0.0.0";
                char mac[32] = "00:00:00:00:00:00";
                if (strcmp(iface, "unknown") != 0) {
                    get_iface_info(iface, ip, mac);
                }

                // 3. 获取 GWID
                char gwid[64] = "unknown";
                get_gwid(gwid, sizeof(gwid));

                // 4. 构建应答报文
                char *rsp_json = build_response_json(iface, ip, mac, gwid);
                
                // 5. 单播回复 3 次，每次间隔 100ms
                if (rsp_json) {
                    for (int i = 0; i < 3; i++) {
                        sendto(sockfd, rsp_json, strlen(rsp_json), 0, 
                               (struct sockaddr *)&client_addr, client_len);
                        usleep(100000); // 100ms (100,000 微秒)
                    }
                    char rsp_log[MAX_BUF_SIZE + 128];
                    snprintf(rsp_log, sizeof(rsp_log), "Replied to %s: %s", inet_ntoa(client_addr.sin_addr), rsp_json);
                    discovery_log_send(0, rsp_log, time(NULL));
                    free(rsp_json);
                }
            }
        }
        json_object_put(parsed_json);
    }

    close(sockfd);
    return 0;
}
