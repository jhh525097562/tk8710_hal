/** @file tk8710_gw_freq_test.c
 * GPS-synchronized, signed frequency-offset sweep. See tools/bcn_frequency_test/README.md.
 */
#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_freq_runtime.h"
#include "driver/tk8710_rf_regs.h"
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    long base_hz, start_hz, end_hz, step_hz, hold_s, ready_s, settle_s;
    long bits, rate, tdd, ul_blocks, dl_blocks, gain, startup_retries;
    const char* work_dir;
    int dry_run, require_controller;
} SweepOptions;

static volatile sig_atomic_t g_stop;

static uint64_t MonotonicMs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) exit(2);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void StopSignal(int signal_number)
{
    (void)signal_number;
    g_stop = 1;
    GwFreqCancel();
}

static int ControllerStopped(int required)
{
    fd_set read_set;
    struct timeval timeout = {0, 0};
    char buffer[64];
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    if (select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout) > 0) {
        ssize_t size = read(STDIN_FILENO, buffer, sizeof(buffer));
        /* Dedicated controller input: any byte requests a graceful stop. */
        if (size > 0 || (size == 0 && required)) StopSignal(0);
    }
    return g_stop;
}

static void Event(const char* event, long index, long offset, long frequency,
    uint64_t elapsed_ms, int status)
{
    uint32_t rf_reg = frequency > 0 ? (uint32_t)(frequency / RF_SX1255_FREQ_STEP) : 0;
    printf("\nFREQ_EVENT {\"event\":\"%s\",\"index\":%ld,\"offset_hz\":%ld,"
        "\"frequency_hz\":%ld,\"rf_register\":%u,\"nominal_rf_hz\":%.6f,"
        "\"monotonic_ms\":%llu,\"elapsed_ms\":%llu,\"status\":%d}\n",
        event, index, offset, frequency, rf_reg, rf_reg * RF_SX1255_FREQ_STEP,
        (unsigned long long)MonotonicMs(), (unsigned long long)elapsed_ms, status);
    fflush(stdout);
}

static int Number(const char* text, long* value)
{
    char* end;
    errno = 0;
    if (!text[0]) return -1;
    *value = strtol(text, &end, 0);
    return errno || *end ? -1 : 0;
}

static int Parse(int argc, char** argv, SweepOptions* options)
{
    *options = (SweepOptions){.start_hz=-1000, .end_hz=1000, .step_hz=100,
        .hold_s=60, .ready_s=600, .settle_s=2, .bits=1, .rate=8, .tdd=10,
        .ul_blocks=2, .dl_blocks=2, .gain=0x2a, .startup_retries=2, .work_dir="."};
    for (int i = 1; i < argc; ++i) {
        long* target = NULL;
        if (!strcmp(argv[i], "--dry-run")) { options->dry_run=1; continue; }
        if (!strcmp(argv[i], "--require-controller")) { options->require_controller=1; continue; }
        if (!strcmp(argv[i], "--help")) {
            puts("tk8710_gw_freq_test --base-hz HZ [--start-hz -1000 --end-hz 1000"
                 " --step-hz 100 --hold-seconds 60 --bcnbits 1 --rate 8 --tdd-count 10"
                 " --ul-blocks 2 --dl-blocks 2 --rf-gain 0x2a --work-dir DIR"
                 " --ready-timeout 600 --settle-seconds 2 --startup-retries 2"
                 " --dry-run --require-controller]");
            return 1;
        }
        if (i + 1 >= argc) return -1;
        if (!strcmp(argv[i], "--work-dir")) { options->work_dir=argv[++i]; continue; }
        if (!strcmp(argv[i], "--base-hz")) target=&options->base_hz;
        else if (!strcmp(argv[i], "--start-hz")) target=&options->start_hz;
        else if (!strcmp(argv[i], "--end-hz")) target=&options->end_hz;
        else if (!strcmp(argv[i], "--step-hz")) target=&options->step_hz;
        else if (!strcmp(argv[i], "--hold-seconds")) target=&options->hold_s;
        else if (!strcmp(argv[i], "--ready-timeout")) target=&options->ready_s;
        else if (!strcmp(argv[i], "--settle-seconds")) target=&options->settle_s;
        else if (!strcmp(argv[i], "--bcnbits")) target=&options->bits;
        else if (!strcmp(argv[i], "--rate")) target=&options->rate;
        else if (!strcmp(argv[i], "--tdd-count")) target=&options->tdd;
        else if (!strcmp(argv[i], "--ul-blocks")) target=&options->ul_blocks;
        else if (!strcmp(argv[i], "--dl-blocks")) target=&options->dl_blocks;
        else if (!strcmp(argv[i], "--rf-gain")) target=&options->gain;
        else if (!strcmp(argv[i], "--startup-retries")) target=&options->startup_retries;
        if (!target || Number(argv[++i], target)) return -1;
    }
    if (options->base_hz < 400000000 || options->base_hz > 510000000 ||
        options->start_hz < -1000000 || options->start_hz > 1000000 ||
        options->end_hz < -1000000 || options->end_hz > 1000000 ||
        options->step_hz < -1000000 || options->step_hz > 1000000 || !options->step_hz ||
        (options->end_hz-options->start_hz) % options->step_hz ||
        (options->end_hz-options->start_hz) / options->step_hz < 0 ||
        (options->end_hz-options->start_hz) / options->step_hz > 9999 ||
        options->base_hz+options->start_hz < 400000000 ||
        options->base_hz+options->start_hz > 510000000 ||
        options->base_hz+options->end_hz < 400000000 ||
        options->base_hz+options->end_hz > 510000000 ||
        options->hold_s < 1 || options->hold_s > 86400 ||
        options->ready_s < 1 || options->ready_s > 3600 ||
        options->settle_s < 0 || options->settle_s > 300 ||
        options->startup_retries < 0 || options->startup_retries > 5 ||
        options->bits < 0 || options->bits > 31 ||
        !((options->rate >= 5 && options->rate <= 11) || options->rate == 18) ||
        options->tdd < 1 || options->tdd > 255 || options->gain < 0 || options->gain > 255 ||
        options->ul_blocks < 1 || options->ul_blocks > 255 ||
        options->dl_blocks < 1 || options->dl_blocks > 255) return -1;
    return 0;
}

int main(int argc, char** argv)
{
    SweepOptions options;
    int ret = Parse(argc, argv, &options);
    if (ret != 0) {
        if (ret < 0) fprintf(stderr, "Invalid sweep options; use --help\n");
        return ret < 0 ? 2 : 0;
    }
    long count = (options.end_hz-options.start_hz) / options.step_hz + 1;
    if (options.dry_run) {
        for (long i=0; i<count; ++i) {
            long offset = options.start_hz+i*options.step_hz;
            Event("PLAN", i, offset, options.base_hz+offset, options.hold_s*1000, 0);
        }
        return 0;
    }
    signal(SIGINT, StopSignal);
    signal(SIGTERM, StopSignal);
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    int exit_code = 1;
    if (GwFreqOpen(options.work_dir, (uint8_t)options.gain) != 0) goto cleanup;
    for (long i=0; i<count; ++i) {
        long offset=options.start_hz+i*options.step_hz;
        long frequency=options.base_hz+offset;
        NsConfigDown_t config = {.msg_type=MSG_TYPE_NS_CONFIG_DOWN,
            .freq=(unsigned int)frequency, .nwk_num=(int)options.bits,
            .tdd_num=(int)options.tdd, .rate_num=1, .gps_enable=1};
        /* NS wire configuration stores rate indices, while CLI uses PHY mode numbers. */
        config.rate_cfgs[0].rate=options.rate == 18 ? 7 : (int)options.rate-5;
        config.rate_cfgs[0].uplink_pkt=(int)options.ul_blocks;
        config.rate_cfgs[0].downlink_pkt=(int)options.dl_blocks;
        uint64_t begin=MonotonicMs(), ready_since=0, active_since=0, next_gps=0;
        long startup_retries = 0;
        Event("POINT_CONFIG", i, offset, frequency, 0, 0);
        if (ControllerStopped(options.require_controller) || GwFreqConfigure(&config) != 0) {
            Event("POINT_FAILED", i, offset, frequency, MonotonicMs()-begin, 1);
            goto cleanup;
        }
        for (;;) {
            uint64_t now=MonotonicMs();
            int check_gps=now >= next_gps;
            if (check_gps) next_gps=now+1000;
            int ready=GwFreqPoll((uint8_t)check_gps);
            now=MonotonicMs();
            if (ready == -2 && !active_since && !ControllerStopped(options.require_controller) &&
                startup_retries < options.startup_retries &&
                now-begin < (uint64_t)options.ready_s*1000) {
                ++startup_retries;
                Event("POINT_RETRY", i, offset, frequency, now-begin, (int)startup_retries);
                fprintf(stderr, "Startup PPS validation failed; retry %ld/%ld before effective timing\n",
                    startup_retries, options.startup_retries);
                if (GwFreqConfigure(&config) != 0) {
                    Event("POINT_FAILED", i, offset, frequency, 0, 1);
                    goto cleanup;
                }
                ready_since=0;
                next_gps=0;
                continue;
            }
            if (ControllerStopped(options.require_controller) || ready < 0 ||
                (active_since && !ready) || (!active_since && now-begin > (uint64_t)options.ready_s*1000)) {
                Event("POINT_FAILED", i, offset, frequency,
                    active_since ? now-active_since : 0, 1);
                goto cleanup;
            }
            if (!ready) ready_since=0;
            else if (!ready_since) ready_since=now;
            if (!active_since && ready_since && now-ready_since >= (uint64_t)options.settle_s*1000) {
                active_since=now;
                Event("POINT_START", i, offset, frequency, 0, 0);
            }
            if (active_since && now-active_since >= (uint64_t)options.hold_s*1000) {
                Event("POINT_END", i, offset, frequency, now-active_since, 0);
                break;
            }
            struct timespec sleep_time={.tv_nsec=20000000};
            nanosleep(&sleep_time, NULL);
        }
    }
    exit_code=0;
cleanup:
    if (GwFreqClose() != 0) exit_code=1;
    Event(exit_code ? "SWEEP_FAILED" : "SWEEP_DONE", -1, 0, 0, 0, exit_code);
    return exit_code;
}
