#ifndef MQTT_GPS_INFO_H
#define MQTT_GPS_INFO_H

#include <stdint.h>

typedef struct {
    char lon[32];
    char lat[32];
    int64_t status;
    /* -1 denotes unavailable (old file, missing or invalid field). */
    int64_t count;
    int64_t avgsnr;
} GpsInfo;

int gps_info_load(const char *path, GpsInfo *info);

#endif
