#include "gateway_version.h"
#include "gateway_telemetry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

static int number(const char *s, unsigned long long *out)
{
    char *end;
    if (!isdigit((unsigned char)*s)) return -1;
    errno = 0;
    *out = strtoull(s, &end, 10);
    return errno || *end ? -1 : 0;
}

struct json_object *gateway_version_read(const char *path, const char *proc_root)
{
    char line[256], version[128] = {0}, filename[512], stat[4096];
    unsigned long long pid = 0, start = 0;
    unsigned seen = 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "version=", 8)) {
            if ((seen & 1) || !line[8] || strlen(line+8) >= sizeof(version)) goto bad;
            strcpy(version, line+8); seen |= 1;
        } else if (!strncmp(line, "pid=", 4)) {
            if ((seen & 2) || number(line+4, &pid) || !pid || pid > 2147483647) goto bad;
            seen |= 2;
        } else if (!strncmp(line, "start_ticks=", 12)) {
            if ((seen & 4) || number(line+12, &start) || !start) goto bad;
            seen |= 4;
        } else goto bad;
    }
    if (ferror(fp) || seen != 7) goto bad;
    fclose(fp);
    snprintf(filename, sizeof(filename), "%s/%llu/stat", proc_root, pid);
    fp = fopen(filename, "r");
    if (!fp) return NULL;
    if (!fgets(stat, sizeof(stat), fp)) goto bad;
    fclose(fp);
    const char *open = strchr(stat, '('), *close = strrchr(stat, ')');
    uint64_t ticks;
    if (!open || !close || close-open != 10 || strncmp(open+1, "tk8710_gw", 9) ||
        close[1] != ' ' || close[2] == 'Z' || close[2] == 'X' ||
        telemetry_parse_starttime(stat, &ticks) || ticks != start) return NULL;
    return json_object_new_string(version);
bad:
    fclose(fp);
    return NULL;
}
