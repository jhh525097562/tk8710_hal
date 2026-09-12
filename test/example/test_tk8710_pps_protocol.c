#define _GNU_SOURCE
#include "tk8710_pps_api.h"

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
} MockDevice;

static unsigned int g_gpio_call_count;
static uint8_t g_gpio_levels[2];

int TK8710GpioSet(const char* chip_path, unsigned int line_offset, uint8_t level)
{
    assert(strcmp(chip_path, "gpiochip1") == 0);
    assert(line_offset == 19);
    if (g_gpio_call_count < 2) g_gpio_levels[g_gpio_call_count] = level;
    ++g_gpio_call_count;
    return 0;
}

static void* mock_device_main(void* argument)
{
    MockDevice* device = argument;
    char line[128];
    size_t command_index = 0;
    size_t used = 0;

    while (command_index < device->count) {
        char character;
        ssize_t count = read(device->master_fd, &character, 1);
        if (count <= 0) continue;
        if (character == '\r' || character == '\n') {
            size_t response_length;
            if (used == 0) continue;
            line[used] = '\0';
            assert(strcmp(line, device->commands[command_index]) == 0);
            response_length = strlen(device->responses[command_index]);
            assert(write(device->master_fd, device->responses[command_index],
                response_length) == (ssize_t)response_length);
            ++command_index;
            used = 0;
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
        "OUT ON", "SET PERIOD 10", "GET PERIOD", "GET STATUS", "GET POS", "GET POS",
        "GET POS", "GET POS",
        "GET PPS", "GET OUT", "GET SYNC", "GET GPS", "GET RMC DIAG",
        "OUT ON", "SET PERIOD 10"
    };
    static const char* responses[] = {
        "EVENT CPLD_ALIGNED RESULT=PASS READY=1\r\nOK OUT EN=1\r\n",
        "OK PERIOD=10 PENDING=1\n",
        "OK PERIOD=10 PENDING=0 PPAUSE=0 PPERIOD=0 PCONF=0\n",
        "OK STATUS STATE=RUNNING PERIOD=10 OUT=1 WIDTH=100 PENDING=0 GPS=1 PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 EXTRA=ignored UTC=2026-09-02 08:00:00.000\n",
        "OK POS LAT=30.0049276 N LON=114.0053766 E\n",
        "OK POS LAT=NA ? LON=NA ?\n",
        "OK POS LAT=23.123456 S LON=113.123456 W\n",
        "OK POS LAT=23.123456 E LON=113.123456 E\n",
        "OK PPS SEEN=1 ALIGN=1\n", "OK OUT EN=1 PERIOD=10\n",
        "OK SYNC STATE=RUNNING\n", "OK GPS ONLINE=1\n",
        "OK RMC_DIAG RMC_OK_SAT=15\n", "OK OUT EN=1\n",
        "OK PERIOD=10\n"
    };
    TK8710PpsConfig config;
    TK8710PpsContext context;
    TK8710PpsPeriodInfo period;
    TK8710PpsStatus status;
    TK8710PpsPosition position;
    TK8710PpsDiagnostics diagnostics;
    MockDevice device;
    pthread_t thread;
    int master_fd;
    char* slave_name;

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
    assert(pthread_create(&thread, NULL, mock_device_main, &device) == 0);

    assert(TK8710PpsGetDefaultConfig(&config) == TK8710_PPS_OK);
    config.uart_device = slave_name;
    config.reset_low_ms = 1;
    config.reset_boot_wait_ms = 1;
    assert(TK8710PpsInit(&context, &config) == TK8710_PPS_OK);
    assert(TK8710PpsSetOutput(&context, 1) == TK8710_PPS_OK);
    assert(strncmp(TK8710PpsGetLastEvent(&context), "EVENT CPLD_ALIGNED", 18) == 0);
    assert(TK8710PpsSetPeriod(&context, 10) == TK8710_PPS_OK);
    assert(TK8710PpsGetPeriod(&context, &period) == TK8710_PPS_OK);
    assert(period.period == 10 && period.pending == 0);
    assert(TK8710PpsGetStatus(&context, &status) == TK8710_PPS_OK);
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_HEALTHY);
    assert(strcmp(status.utc, "2026-09-02 08:00:00.000") == 0);
    assert(TK8710PpsGetPosition(&context, &position) == TK8710_PPS_OK);
    assert(position.valid == 1 && position.latitude > 30.004927 &&
        position.latitude < 30.004928 && position.longitude > 114.005376 &&
        position.longitude < 114.005377);
    assert(TK8710PpsGetPosition(&context, &position) == TK8710_PPS_OK);
    assert(position.valid == 0 && position.latitude == 0.0 &&
        position.longitude == 0.0);
    assert(TK8710PpsGetPosition(&context, &position) == TK8710_PPS_OK);
    assert(position.valid == 1 && position.latitude < -23.123455 &&
        position.latitude > -23.123457 && position.longitude < -113.123455 &&
        position.longitude > -113.123457);
    assert(TK8710PpsGetPosition(&context, &position) == TK8710_PPS_ERROR_PARSE);
    assert(TK8710PpsGetDiagnostics(&context, &diagnostics) == TK8710_PPS_OK);
    assert(diagnostics.success_mask == 0x1f);
    assert(TK8710PpsResetDevice(&context) == TK8710_PPS_OK);
    assert(g_gpio_call_count == 2 && g_gpio_levels[0] == 0 && g_gpio_levels[1] == 1);
    assert(TK8710PpsSetOutput(&context, 1) == TK8710_PPS_OK);
    assert(TK8710PpsSetPeriod(&context, 10) == TK8710_PPS_OK);
    TK8710PpsClose(&context);

    assert(pthread_join(thread, NULL) == 0);
    close(master_fd);
    puts("TK8710 PPS protocol tests passed");
    return 0;
}
