#include "gps_info.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t parse_nonnegative(const char *text, int64_t limit, int64_t fallback)
{
    char *end;
    long long value;
    if (*text < '0' || *text > '9') return fallback;
    errno = 0;
    value = strtoll(text, &end, 10);
    return errno || *end || value < 0 || value > limit ? fallback : value;
}

int gps_info_load(const char *path, GpsInfo *info)
{
    char line[128];
    int got_lon = 0, got_lat = 0;
    if (!path || !info) return -1;
    memset(info, 0, sizeof(*info));
    strcpy(info->lon, "unknown");
    strcpy(info->lat, "unknown");
    info->count = info->avgsnr = -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    while (fgets(line, sizeof(line), fp)) {
        size_t length = strlen(line);
        /* Reject a truncated record rather than parsing its continuation. */
        if (length == sizeof(line) - 1 && line[length - 1] != '\n') {
            got_lon = got_lat = 0;
            break;
        }
        while (length && strchr("\r\n\t ", line[length - 1])) line[--length] = '\0';
        if (!strncmp(line, "lon=", 4) && line[4] && strlen(line + 4) < sizeof(info->lon)) {
            strcpy(info->lon, line + 4); got_lon = 1;
        } else if (!strncmp(line, "lat=", 4) && line[4] && strlen(line + 4) < sizeof(info->lat)) {
            strcpy(info->lat, line + 4); got_lat = 1;
        } else if (!strncmp(line, "status=", 7)) {
            info->status = parse_nonnegative(line + 7, INT64_MAX, 0);
        } else if (!strncmp(line, "count=", 6)) {
            info->count = parse_nonnegative(line + 6, UINT32_MAX, -1);
        } else if (!strncmp(line, "avgsnr=", 7)) {
            info->avgsnr = parse_nonnegative(line + 7, UINT32_MAX, -1);
        }
    }
    int ok = got_lon && got_lat && !ferror(fp);
    fclose(fp);
    if (!ok) {
        strcpy(info->lon, "unknown"); strcpy(info->lat, "unknown");
        info->status = 0;
        info->count = info->avgsnr = -1;
    }
    return ok ? 0 : -1;
}
