/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-02 11:43:09
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-02 15:33:41
 * @FilePath: \web\src\system_manager.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "system_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include "config_manager.h"
#include "file_logger.h"

#define WEB_FILE_LOG "/var/log/web_server.log"

static bool read_device_coords(char *out, size_t out_len) {
    FILE *fp = fopen("/userdata/GPS/GPSinfo.txt", "r");
    char line[128];
    char lon[32] = {0};
    char lat[32] = {0};

    if (!out || out_len == 0) return false;
    out[0] = '\0';
    if (!fp) {
        file_logger_write(WEB_FILE_LOG, "WARN", "open /userdata/GPS/GPSinfo.txt failed");
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *key = line;
        char *val = NULL;
        char *end = NULL;
        char *p = NULL;
        size_t key_len = 0;

        if (!eq) continue;
        *eq = '\0';
        val = eq + 1;

        while (*key && isspace((unsigned char)*key)) key++;
        end = key + strlen(key);
        while (end > key && isspace((unsigned char)end[-1])) end--;
        *end = '\0';

        while (*val && isspace((unsigned char)*val)) val++;
        end = val + strlen(val);
        while (end > val && isspace((unsigned char)end[-1])) end--;
        *end = '\0';

        key_len = strlen(key);
        for (p = key; *p; p++) *p = (char)tolower((unsigned char)*p);

        if (key_len == 3 && strcmp(key, "lon") == 0) {
            strncpy(lon, val, sizeof(lon) - 1);
            lon[sizeof(lon) - 1] = '\0';
        } else if (key_len == 3 && strcmp(key, "lat") == 0) {
            strncpy(lat, val, sizeof(lat) - 1);
            lat[sizeof(lat) - 1] = '\0';
        }
    }
    fclose(fp);

    if (lon[0] == '\0' || lat[0] == '\0') return false;
    snprintf(out, out_len, "%s, %s", lon, lat);
    return true;
}

void system_get_info(SystemInfo *info) {
    memset(info, 0, sizeof(*info));
    AppConfig config;
    // 加载配置，如果文件不存在会使用默认值
    config_load(&config);

    // 从 config.json 中读取系统信息
    strncpy(info->hostname, config.hostname, sizeof(info->hostname) - 1);
    strncpy(info->model, config.model, sizeof(info->model) - 1);
    strncpy(info->arch, config.arch, sizeof(info->arch) - 1);
    strncpy(info->platform, config.platform, sizeof(info->platform) - 1);
    info->device_coords[0] = '\0';
    strncpy(info->firmware_version, config.firmware_version, sizeof(info->firmware_version) - 1);
    strncpy(info->kernel_version, config.kernel_version, sizeof(info->kernel_version) - 1);
    strncpy(info->app_version, config.app_version, sizeof(info->app_version) - 1);
    read_device_coords(info->device_coords, sizeof(info->device_coords));

    // 获取本地时间
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(info->local_time, sizeof(info->local_time), "%Y-%m-%d %H:%M:%S", t);

    // 获取运行时间
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        long uptime = si.uptime;
        int h = uptime / 3600;
        int m = (uptime % 3600) / 60;
        int s = uptime % 60;
        snprintf(info->uptime, sizeof(info->uptime), "%dh %dm %ds", h, m, s);
        
        info->load_avg[0] = si.loads[0] / 65536.0;
        info->load_avg[1] = si.loads[1] / 65536.0;
        info->load_avg[2] = si.loads[2] / 65536.0;

        info->mem_total = (uint64_t)si.totalram * si.mem_unit;
        info->mem_free = (uint64_t)si.freeram * si.mem_unit;
        info->mem_used = info->mem_total - info->mem_free;
    }

    // 获取磁盘使用情况 (/userdata)
    struct statvfs disk_info;
    if (statvfs("/userdata", &disk_info) == 0) {
        info->disk_total = disk_info.f_blocks * disk_info.f_frsize;
        info->disk_free = disk_info.f_bfree * disk_info.f_frsize;
        info->disk_used = info->disk_total - info->disk_free;
    } else {
        info->disk_total = 0;
        info->disk_used = 0;
        info->disk_free = 0;
    }
}
