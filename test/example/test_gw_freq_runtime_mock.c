/* Host-only runtime for scheduler tests. Never link into the RK3506 binary. */
#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_freq_runtime.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
static int cancelled;
static unsigned long long configured_at;
static unsigned int configure_count;
static unsigned long long Now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec*1000+ts.tv_nsec/1000000;
}
int GwFreqOpen(const char* dir, uint8_t gain) { (void)dir; (void)gain; return 0; }
int GwFreqConfigure(const NsConfigDown_t* config)
{
    if (config->gps_enable != 1 || config->rate_num != 1) return -1;
    const char* expected_rate = getenv("MOCK_NS_RATE");
    if (expected_rate && config->rate_cfgs[0].rate != atoi(expected_rate)) return -1;
    configured_at = Now();
    ++configure_count;
    return getenv("MOCK_CONFIG_FAIL") ? -1 : 0;
}
int GwFreqPoll(uint8_t check_gps)
{
    (void)check_gps;
    if (cancelled || getenv("MOCK_GPS_FAIL")) return -1;
    if (getenv("MOCK_PPS_ALWAYS_FAIL")) return -2;
    if (getenv("MOCK_PPS_STARTUP_ONCE") && configure_count == 1) return -2;
    if (getenv("MOCK_PPS_AFTER_START") && Now()-configured_at >= 700) return -2;
    return Now()-configured_at >= 200;
}
void GwFreqCancel(void) { cancelled = 1; }
int GwFreqClose(void) { puts("MOCK_CLEANUP"); return 0; }
