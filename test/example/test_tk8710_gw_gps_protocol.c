#define _GNU_SOURCE
#include "tk8710_gw_gps.h"

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int master_fd;
    const char** commands;
    const char** responses;
    size_t count;
} MockGpsDevice;

static void* mock_gps_main(void* argument)
{
    MockGpsDevice* device = argument;
    char line[128];
    size_t used = 0;
    size_t index = 0;

    while (index < device->count) {
        char character;
        ssize_t count = read(device->master_fd, &character, 1);

        if (count <= 0) continue;
        if (character == '\r' || character == '\n') {
            if (used == 0) continue;
            line[used] = '\0';
            assert(strcmp(line, device->commands[index]) == 0);
            assert(write(device->master_fd, device->responses[index],
                strlen(device->responses[index])) ==
                (ssize_t)strlen(device->responses[index]));
            used = 0;
            ++index;
        } else {
            assert(used + 1 < sizeof(line));
            line[used++] = character;
        }
    }
    return NULL;
}

int main(void)
{
    static const char* commands[] = {
        "GET VERSION", "GET STATUS", "GET POS",
        "OUT ON", "SET PERIOD 4", "GET STATUS", "GET POS",
        "OUT ON", "SET PERIOD 6", "GET STATUS", "GET POS",
        "OUT ON", "SET PERIOD 7", "GET STATUS", "GET POS"
    };
    static const char* responses[] = {
        "EVENT APP_WDOG TIMEOUT_MS=10000\n"
        "OK APP_VER=2 APP_BUILD=20260902 BOOT_VER=1 CPLD_VER=3\n",
        "OK STATUS STATE=IDLE PERIOD=1 OUT=0 WIDTH=100 PENDING=0 GPS=1 "
        "PPS=1 FIX=1 RMC=A ALIGN=0 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 UTC=NA\n",
        "OK POS LAT=30.0049276 N LON=114.0053766 E\n",
        "OK OUT EN=1\n",
        "OK PERIOD=4 PENDING=1\n",
        "OK STATUS STATE=RUNNING PERIOD=4 OUT=1 WIDTH=100 PENDING=0 GPS=1 "
        "PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 UTC=NA\n",
        "OK POS LAT=30.0049276 N LON=114.0053766 E\n",
        "OK OUT EN=1\n",
        "OK PERIOD=6 PENDING=1\n",
        "OK STATUS STATE=RUNNING PERIOD=6 OUT=1 WIDTH=100 PENDING=0 GPS=1 "
        "PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 UTC=NA\n",
        "OK POS LAT=30.0049276 N LON=114.0053766 E\n",
        "OK OUT EN=1\n",
        "OK PERIOD=7 PENDING=1\n",
        "OK STATUS STATE=RUNNING PERIOD=7 OUT=1 WIDTH=100 PENDING=0 GPS=1 "
        "PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 UTC=NA\n",
        "OK POS LAT=30.0049276 N LON=114.0053766 E\n"
    };
    GwGpsManager manager;
    GwGpsPolicy policy;
    TK8710PpsConfig pps_config;
    MockGpsDevice device;
    pthread_t thread;
    int master_fd;
    char* slave_name;
    char version[TK8710_PPS_DIAG_TEXT_MAX];

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master_fd >= 0);
    assert(grantpt(master_fd) == 0);
    assert(unlockpt(master_fd) == 0);
    slave_name = ptsname(master_fd);
    assert(slave_name != NULL);

    device.master_fd = master_fd;
    device.commands = commands;
    device.responses = responses;
    device.count = sizeof(commands) / sizeof(commands[0]);
    assert(pthread_create(&thread, NULL, mock_gps_main, &device) == 0);

    GwGpsGetDefaultPolicy(&policy);
    policy.search_timeout_ms = 100;
    policy.poll_interval_ms = 1;
    policy.align_timeout_ms = 100;
    assert(TK8710PpsGetDefaultConfig(&pps_config) == TK8710_PPS_OK);
    pps_config.uart_device = slave_name;
    assert(GwGpsStartup(&manager, &pps_config, &policy, NULL,
        version, sizeof(version)) == 0);
    assert(manager.mode == GW_GPS_MODE_EXTERNAL_READY);
    assert(manager.consecutive_healthy_status == 1);
    assert(strstr(version, "APP_VER=2") != NULL);
    assert(GwGpsConfigurePeriod(&manager, 4) == 0);
    assert(manager.mode == GW_GPS_MODE_EXTERNAL_ACTIVE);
    assert(manager.expected_period_s == 4);
    assert(GwGpsConfigurePeriod(&manager, 6) == 0);
    assert(manager.mode == GW_GPS_MODE_EXTERNAL_ACTIVE);
    assert(manager.expected_period_s == 6);
    GwGpsUseLocalWithoutRecovery(&manager);
    assert(manager.mode == GW_GPS_MODE_LOCAL_STATIC);
    assert(GwGpsConfigurePeriod(&manager, 7) == 0);
    assert(manager.mode == GW_GPS_MODE_EXTERNAL_ACTIVE);
    assert(manager.expected_period_s == 7);
    assert(manager.consecutive_healthy_status == 4);
    GwGpsClose(&manager);

    assert(pthread_join(thread, NULL) == 0);
    close(master_fd);
    puts("TK8710 gateway GPS protocol tests passed");
    return 0;
}
