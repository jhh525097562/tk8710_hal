#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_gps.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void wait_seconds(uint32_t seconds)
{
    struct timespec request;

    request.tv_sec = (time_t)seconds;
    request.tv_nsec = 0;
    (void)nanosleep(&request, NULL);
}

int main(void)
{
    GwGpsManager manager;
    GwGpsPolicy policy;
    TK8710PpsConfig pps_config;
    TRM_MultiRateSlotCalcOutput slot_output;
    char version[TK8710_PPS_DIAG_TEXT_MAX];
    uint32_t period_s;
    uint32_t index;

    GwGpsGetDefaultPolicy(&policy);
    if (TK8710PpsGetDefaultConfig(&pps_config) != TK8710_PPS_OK) {
        return 1;
    }
    if (GwGpsStartup(&manager, &pps_config, &policy, NULL,
                     version, sizeof(version)) != 0 ||
        manager.mode != GW_GPS_MODE_EXTERNAL_READY) {
        fprintf(stderr, "GPS/PPS device is not ready\n");
        GwGpsClose(&manager);
        return 1;
    }
    printf("GPS/PPS version: %s\n", version);

    memset(&slot_output, 0, sizeof(slot_output));
    slot_output.framePeriod = 1000000;
    slot_output.frameCount = 10;
    if (GwGpsPeriodFromSlotCalc(&slot_output, &period_s) != 0 || period_s != 10) {
        GwGpsClose(&manager);
        return 1;
    }
    printf("Calculated PPS period: %u s\n", period_s);
    if (GwGpsConfigurePeriod(&manager, period_s) != 0) {
        GwGpsClose(&manager);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        wait_seconds(10);
        if (GwGpsPoll(&manager) != GW_GPS_ACTION_NONE ||
            manager.mode != GW_GPS_MODE_EXTERNAL_ACTIVE) {
            GwGpsClose(&manager);
            return 1;
        }
    }
    GwGpsClose(&manager);
    puts("TK8710 gateway GPS device smoke test passed");
    return 0;
}
