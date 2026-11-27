#define _POSIX_C_SOURCE 200809L

#include "gateway_telemetry.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>

struct json_object *telemetry_rf_status(const char *path)
{
    char text[64];
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    size_t size = fread(text, 1, sizeof(text) - 1, fp);
    int failed = ferror(fp) || size == sizeof(text) - 1;
    fclose(fp);
    if (failed || memchr(text, '\0', size) != NULL) return NULL;
    text[size] = '\0';
    /* Accept LF/CRLF, but reject partial, duplicated or extra records. */
    while (size && (text[size - 1] == '\r' || text[size - 1] == '\n')) --size;
    if (size != 17 || memcmp(text, "rfstatus=", 9) != 0) return NULL;
    for (size_t i = 9; i < 17; ++i) {
        if (text[i] != '0' && text[i] != '1') return NULL;
    }
    return json_object_new_string_len(text + 9, 8);
}

int telemetry_parse_starttime(const char *stat, uint64_t *ticks)
{
    const char *p = strrchr(stat, ')');
    if (p == NULL || ticks == NULL) {
        return -1;
    }
    ++p;
    for (int field = 3; field <= 22; ++field) {
        while (*p == ' ') {
            ++p;
        }
        if (*p == '\0') {
            return -1;
        }
        if (field == 22) {
            char *end;
            unsigned long long value;

            if (!isdigit((unsigned char)*p)) {
                return -1;
            }
            errno = 0;
            value = strtoull(p, &end, 10);
            if (errno || end == p || (*end != '\0' && *end != ' ' && *end != '\n')) {
                return -1;
            }
            *ticks = value;
            return 0;
        }
        while (*p != '\0' && *p != ' ') {
            ++p;
        }
    }
    return -1;
}

static int read_line(const char *path, char *buffer, size_t capacity)
{
    FILE *fp = fopen(path, "r");
    int ok;

    if (fp == NULL) {
        return -1;
    }
    ok = fgets(buffer, capacity, fp) != NULL && !ferror(fp);
    fclose(fp);
    return ok ? 0 : -1;
}

struct json_object *telemetry_process(const char *proc_root, const char *name,
                                      double uptime_s, long hz)
{
    struct json_object *obj = json_object_new_object();
    DIR *dir;
    int found = 0;
    int uncertain = 0;
    uint64_t oldest = UINT64_MAX;
    struct dirent *entry;

    if (obj == NULL) {
        return NULL;
    }
    json_object_object_add(obj, "running", NULL);
    json_object_object_add(obj, "uptime_s", NULL);
    dir = opendir(proc_root);
    if (dir == NULL || hz <= 0 || uptime_s < 0) {
        if (dir != NULL) {
            closedir(dir);
        }
        return obj;
    }
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        char comm[128];
        char stat[4096];
        const char *open;
        const char *close;
        uint64_t ticks;

        if (strspn(entry->d_name, "0123456789") != strlen(entry->d_name)) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s/comm", proc_root, entry->d_name);
        if (read_line(path, comm, sizeof(comm))) {
            if (errno != ENOENT && errno != ESRCH) {
                uncertain = 1;
            }
            errno = 0;
            continue;
        }
        comm[strcspn(comm, "\r\n")] = '\0';
        if (strcmp(comm, name) != 0) {
            errno = 0;
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s/stat", proc_root, entry->d_name);
        if (read_line(path, stat, sizeof(stat))) {
            uncertain = 1;
            errno = 0;
            continue;
        }
        open = strchr(stat, '(');
        close = strrchr(stat, ')');
        if (open == NULL || close == NULL || (size_t)(close - open - 1) != strlen(name) ||
            strncmp(open + 1, name, strlen(name)) ||
            telemetry_parse_starttime(stat, &ticks) || ticks / (double)hz > uptime_s) {
            uncertain = 1;
            errno = 0;
            continue;
        }
        if (close[1] == ' ' && (close[2] == 'Z' || close[2] == 'X')) {
            errno = 0;
            continue;
        }
        found = 1;
        if (ticks < oldest) {
            oldest = ticks;
        }
        errno = 0;
    }
    if (errno) {
        uncertain = 1;
    }
    closedir(dir);
    if (found) {
        json_object_object_add(obj, "running", json_object_new_boolean(1));
        json_object_object_add(obj, "uptime_s", json_object_new_int64((int64_t)(uptime_s - oldest / (double)hz)));
    } else if (!uncertain) {
        json_object_object_add(obj, "running", json_object_new_boolean(0));
    }
    return obj;
}

void telemetry_add(struct json_object *alive)
{
    struct json_object *system = json_object_new_object();
    const char *fields[] = {
        "uptime_s", "ram_total_bytes", "ram_used_bytes", "ram_used_percent",
        "disk_total_bytes", "disk_used_bytes", "disk_used_percent", "load_avg",
    };
    struct sysinfo si;
    double uptime = -1;
    struct statvfs disk;
    char text[128];

    if (system == NULL) {
        return;
    }
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        json_object_object_add(system, fields[i], NULL);
    }
    if (!sysinfo(&si)) {
        uint64_t total = (uint64_t)si.totalram * si.mem_unit;
        uint64_t used = total - (uint64_t)si.freeram * si.mem_unit;
        struct json_object *loads = json_object_new_array();

        uptime = si.uptime;
        json_object_object_add(system, "uptime_s", json_object_new_int64(si.uptime));
        json_object_object_add(system, "ram_total_bytes", json_object_new_int64(total));
        json_object_object_add(system, "ram_used_bytes", json_object_new_int64(used));
        if (total) {
            json_object_object_add(system, "ram_used_percent", json_object_new_double(100.0 * used / total));
        }
        if (loads != NULL) {
            for (int i = 0; i < 3; ++i) {
                json_object_array_add(loads, json_object_new_double(si.loads[i] / 65536.0));
            }
            json_object_object_add(system, "load_avg", loads);
        }
    }
    if (!statvfs("/userdata", &disk)) {
        uint64_t total = (uint64_t)disk.f_blocks * disk.f_frsize;
        uint64_t used = total - (uint64_t)disk.f_bfree * disk.f_frsize;
        json_object_object_add(system, "disk_total_bytes", json_object_new_int64(total));
        json_object_object_add(system, "disk_used_bytes", json_object_new_int64(used));
        if (total) {
            json_object_object_add(system, "disk_used_percent", json_object_new_double(100.0 * used / total));
        }
    }
    if (!read_line("/proc/uptime", text, sizeof(text))) {
        char *end;
        double precise = strtod(text, &end);
        if (end != text && precise >= 0) {
            uptime = precise;
        }
    }
    json_object_object_add(alive, "system", system);
    json_object_object_add(alive, "tk8710_gw",
                           telemetry_process("/proc", "tk8710_gw", uptime, sysconf(_SC_CLK_TCK)));
}
