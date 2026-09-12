#define _POSIX_C_SOURCE 200809L
#include "tk8710_pps_api.h"
#include "tk8710_hal.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define EXPECTED_PERIOD_S 10U
#define POLL_INTERVAL_S 2U
#define STARTUP_GRACE_S 60U
#define FAILURE_THRESHOLD 3U
#define RESET_COOLDOWN_S 30U
#define MAX_RECOVERY_ATTEMPTS 3U

static volatile sig_atomic_t g_stop_requested;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    g_stop_requested = 1;
}

static uint64_t monotonic_seconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec;
}

static int configure_device(TK8710PpsContext* context)
{
    TK8710PpsError error;

    printf("[PPS] command: OUT ON\n");
    error = TK8710PpsSetOutput(context, 1);
    if (error != TK8710_PPS_OK) {
        fprintf(stderr, "[PPS] OUT ON failed: %d\n", error);
        return -1;
    }
    printf("[PPS] command: SET PERIOD %u\n", EXPECTED_PERIOD_S);
    error = TK8710PpsSetPeriod(context, EXPECTED_PERIOD_S);
    if (error != TK8710_PPS_OK) {
        fprintf(stderr, "[PPS] SET PERIOD failed: %d\n", error);
        return -1;
    }
    return 0;
}

static void print_status(const TK8710PpsStatus* status, TK8710PpsHealth health)
{
    printf("[PPS] state=%s period=%u out=%u pending=%u gps=%u pps=%u "
           "fix=%u rmc=%c align=%u hold=%u bad=%u resync=%u health=%d\n",
        status->state, status->period, status->output_enabled, status->pending,
        status->gps_online, status->pps_seen, status->fix_valid,
        status->rmc_status ? status->rmc_status : '?', status->aligned,
        status->holdover, status->bad_period, status->resync_pending, health);
}

static void print_diagnostics(TK8710PpsContext* context)
{
    TK8710PpsDiagnostics diagnostics;
    TK8710PpsError error = TK8710PpsGetDiagnostics(context, &diagnostics);

    fprintf(stderr, "[PPS] diagnostics result=%d mask=0x%02x\n",
        error, diagnostics.success_mask);
    if (diagnostics.pps[0]) fprintf(stderr, "[PPS] %s\n", diagnostics.pps);
    if (diagnostics.output[0]) fprintf(stderr, "[PPS] %s\n", diagnostics.output);
    if (diagnostics.sync[0]) fprintf(stderr, "[PPS] %s\n", diagnostics.sync);
    if (diagnostics.gps[0]) fprintf(stderr, "[PPS] %s\n", diagnostics.gps);
    if (diagnostics.rmc[0]) fprintf(stderr, "[PPS] %s\n", diagnostics.rmc);
}

int main(void)
{
    TK8710PpsConfig config;
    TK8710PpsContext context;
    uint64_t startup_deadline;
    uint64_t last_reset = 0;
    uint32_t consecutive_failures = 0;
    uint32_t recovery_attempts = 0;
    uint8_t was_healthy = 0;
    char version[TK8710_PPS_DIAG_TEXT_MAX];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    TK8710PpsGetDefaultConfig(&config);
    if (TK8710GpioSet(config.reset_gpio_chip, config.reset_gpio_line, 1) != 0) {
        fprintf(stderr, "[PPS] failed to deassert reset GPIO\n");
        return 1;
    }
    if (TK8710PpsInit(&context, &config) != TK8710_PPS_OK) {
        fprintf(stderr, "[PPS] UART initialization failed\n");
        return 1;
    }
    if (TK8710PpsGetVersion(&context, version, sizeof(version)) == TK8710_PPS_OK)
        printf("[PPS] %s\n", version);
    if (configure_device(&context) != 0) consecutive_failures = FAILURE_THRESHOLD;
    startup_deadline = monotonic_seconds() + STARTUP_GRACE_S;

    while (!g_stop_requested) {
        TK8710PpsStatus status;
        TK8710PpsError error;
        TK8710PpsHealth health = TK8710_PPS_UNHEALTHY;
        uint64_t now;
        uint8_t need_recovery = 0;

        if (consecutive_failures < FAILURE_THRESHOLD) {
            sleep(POLL_INTERVAL_S);
            error = TK8710PpsGetStatus(&context, &status);
            now = monotonic_seconds();
            if (error != TK8710_PPS_OK) {
                fprintf(stderr, "[PPS] GET STATUS failed: %d\n", error);
                ++consecutive_failures;
            } else {
                health = TK8710PpsEvaluateHealth(&status, EXPECTED_PERIOD_S);
                print_status(&status, health);
                if (health == TK8710_PPS_HEALTHY) {
                    was_healthy = 1;
                    consecutive_failures = 0;
                    recovery_attempts = 0;
                } else if (!was_healthy && now < startup_deadline) {
                    consecutive_failures = 0;
                } else {
                    ++consecutive_failures;
                }
            }
            if (!was_healthy && now >= startup_deadline) need_recovery = 1;
            if (consecutive_failures >= FAILURE_THRESHOLD) need_recovery = 1;
        } else {
            need_recovery = 1;
            now = monotonic_seconds();
        }
        if (!need_recovery) continue;

        print_diagnostics(&context);
        if (recovery_attempts >= MAX_RECOVERY_ATTEMPTS) {
            fprintf(stderr, "[PPS] recovery failed after %u attempts\n",
                recovery_attempts);
            TK8710PpsClose(&context);
            return 2;
        }
        now = monotonic_seconds();
        if (last_reset != 0 && now - last_reset < RESET_COOLDOWN_S) {
            sleep((unsigned int)(RESET_COOLDOWN_S - (now - last_reset)));
        }
        if (g_stop_requested) break;
        ++recovery_attempts;
        fprintf(stderr, "[PPS] reset attempt %u/%u\n", recovery_attempts,
            MAX_RECOVERY_ATTEMPTS);
        if (TK8710PpsResetDevice(&context) != TK8710_PPS_OK) {
            fprintf(stderr, "[PPS] device reset/reopen failed\n");
            consecutive_failures = FAILURE_THRESHOLD;
            last_reset = monotonic_seconds();
            continue;
        }
        last_reset = monotonic_seconds();
        consecutive_failures = configure_device(&context) == 0 ? 0 : FAILURE_THRESHOLD;
        was_healthy = 0;
        startup_deadline = monotonic_seconds() + STARTUP_GRACE_S;
    }

    printf("[PPS] stop requested; closing UART without OUT OFF\n");
    TK8710PpsClose(&context);
    return 0;
}
