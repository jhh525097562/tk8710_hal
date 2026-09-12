#define _GNU_SOURCE
#include "tk8710_gw_gps.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int master_fd;
    volatile int running;
    uint32_t version_count;
    uint32_t status_count;
} HealthyGpsDevice;

static const char* healthy_status =
    "OK STATUS STATE=RUNNING PERIOD=10 OUT=1 WIDTH=100 PENDING=0 GPS=1 "
    "PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 SATS=12 SNR=35 UTC=NA\n";

static const char* response_for_command(HealthyGpsDevice* device, const char* command)
{
    if (strcmp(command, "GET VERSION") == 0) {
        device->version_count++;
        return "OK APP_VER=2 APP_BUILD=20260902 BOOT_VER=1 CPLD_VER=3\n";
    }
    if (strcmp(command, "GET STATUS") == 0) {
        device->status_count++;
        return healthy_status;
    }
    if (strcmp(command, "GET POS") == 0) {
        return "OK POS LAT=30.0049276 N LON=114.0053766 E\n";
    }
    return "OK TEST=1\n";
}

static void* healthy_gps_main(void* argument)
{
    HealthyGpsDevice* device = argument;
    char line[128];
    size_t used = 0;

    while (device->running) {
        char character;
        ssize_t count = read(device->master_fd, &character, 1);

        if (count < 0 && errno == EIO) {
            usleep(1000);
            continue;
        }
        if (count <= 0) {
            break;
        }
        if (character == '\r' || character == '\n') {
            const char* response;

            if (used == 0) {
                continue;
            }
            line[used] = '\0';
            response = response_for_command(device, line);
            assert(write(device->master_fd, response, strlen(response)) ==
                (ssize_t)strlen(response));
            used = 0;
        } else {
            assert(used + 1 < sizeof(line));
            line[used++] = character;
        }
    }
    return NULL;
}

static void open_healthy_device(HealthyGpsDevice* device, pthread_t* thread,
    TK8710PpsConfig* config)
{
    char* slave_name;

    memset(device, 0, sizeof(*device));
    device->master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    assert(device->master_fd >= 0);
    assert(grantpt(device->master_fd) == 0);
    assert(unlockpt(device->master_fd) == 0);
    slave_name = ptsname(device->master_fd);
    assert(slave_name != NULL);
    assert(TK8710PpsGetDefaultConfig(config) == TK8710_PPS_OK);
    config->uart_device = slave_name;
    device->running = 1;
    assert(pthread_create(thread, NULL, healthy_gps_main, device) == 0);
}

static void close_healthy_device(HealthyGpsDevice* device, pthread_t thread)
{
    device->running = 0;
    close(device->master_fd);
    assert(pthread_join(thread, NULL) == 0);
}

static GwGpsPolicy fast_policy(void)
{
    GwGpsPolicy policy;

    GwGpsGetDefaultPolicy(&policy);
    policy.search_timeout_ms = 5;
    policy.poll_interval_ms = 1;
    policy.align_timeout_ms = 10;
    return policy;
}

static void test_no_module(void)
{
    HealthyGpsDevice device;
    TK8710PpsConfig config;
    GwGpsManager manager;
    GwGpsPolicy policy = fast_policy();
    pthread_t thread;
    char version[TK8710_PPS_DIAG_TEXT_MAX];

    open_healthy_device(&device, &thread, &config);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_MODULE, NULL) == 0);
    assert(GwGpsStartup(&manager, &config, &policy, NULL,
        version, sizeof(version)) == 0);
    assert(manager.mode == GW_GPS_MODE_ABSENT);
    assert(version[0] == '\0');
    GwGpsClose(&manager);
    close_healthy_device(&device, thread);
    assert(device.version_count == policy.detect_attempts);
    GwGpsTestReset();
}

static void test_no_pps(void)
{
    HealthyGpsDevice device;
    TK8710PpsConfig config;
    GwGpsManager manager;
    GwGpsPolicy policy = fast_policy();
    pthread_t thread;
    char version[TK8710_PPS_DIAG_TEXT_MAX];

    open_healthy_device(&device, &thread, &config);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS, NULL) == 0);
    assert(GwGpsStartup(&manager, &config, &policy, NULL,
        version, sizeof(version)) == 0);
    assert(manager.mode == GW_GPS_MODE_LOCAL_DEGRADED);
    assert(strstr(version, "APP_VER=2") != NULL);
    assert(GwGpsPoll(&manager) == GW_GPS_ACTION_NONE);
    assert(manager.consecutive_recovered == 0);
    GwGpsClose(&manager);
    close_healthy_device(&device, thread);
    assert(device.status_count > 0);
    GwGpsTestReset();
}

static void test_no_pps_then_recover(void)
{
    static const char run_id[] = "fault-test-20260907";
    static const char marker_path[] =
        "/tmp/tk8710_gps_test_fault-test-20260907.done";
    HealthyGpsDevice device;
    TK8710PpsConfig config;
    GwGpsManager manager;
    GwGpsPolicy policy = fast_policy();
    pthread_t thread;
    char version[TK8710_PPS_DIAG_TEXT_MAX];
    uint32_t index;

    assert(GwGpsTestClearMarker(run_id) == 0);
    open_healthy_device(&device, &thread, &config);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS_THEN_RECOVER, run_id) == 0);
    assert(GwGpsStartup(&manager, &config, &policy, NULL,
        version, sizeof(version)) == 0);
    assert(manager.mode == GW_GPS_MODE_LOCAL_DEGRADED);
    assert(GwGpsTestGetScenario() == GW_GPS_TEST_NO_PPS_THEN_RECOVER);
    assert(access(marker_path, F_OK) != 0);

    for (index = 1; index < policy.consecutive_limit; ++index) {
        assert(GwGpsPoll(&manager) == GW_GPS_ACTION_NONE);
        assert(manager.consecutive_recovered == index);
    }
    assert(GwGpsPoll(&manager) == GW_GPS_ACTION_RESTART_EXIT);
    assert(access(marker_path, F_OK) == 0);
    GwGpsClose(&manager);

    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS_THEN_RECOVER, run_id) == 0);
    assert(GwGpsTestGetScenario() == GW_GPS_TEST_PASSTHROUGH);
    assert(GwGpsTestClearMarker(run_id) == 0);
    GwGpsTestReset();
    close_healthy_device(&device, thread);
}

static void test_invalid_run_id(void)
{
    GwGpsTestScenario scenario;

    assert(GwGpsTestParseScenario("no-module", &scenario) == 0);
    assert(scenario == GW_GPS_TEST_NO_MODULE);
    assert(GwGpsTestParseScenario("invalid", &scenario) != 0);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS_THEN_RECOVER, "../bad") != 0);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS_THEN_RECOVER, NULL) != 0);
    assert(GwGpsTestSetScenario(GW_GPS_TEST_NO_PPS, "unused") != 0);
    GwGpsTestReset();
}

int main(void)
{
    test_no_module();
    test_no_pps();
    test_no_pps_then_recover();
    test_invalid_run_id();
    puts("TK8710 gateway GPS fault-injection tests passed");
    return 0;
}
