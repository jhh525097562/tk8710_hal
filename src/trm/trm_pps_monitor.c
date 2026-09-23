/* Isolate the SDK libgpiod time64 ABI in this translation unit. */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _TIME_BITS
#define _TIME_BITS 64
#endif
#define _POSIX_C_SOURCE 200809L
#include "trm/trm_pps_monitor.h"
#include "trm/trm_log.h"
#include "trm_pps_model.h"
#ifdef PLATFORM_RK3506
#include <gpiod.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#define PPS_QUEUE 256u
#define PPS_EDGES 32u
#define PPS_DELIVERY_GRACE_US 20000u
typedef struct {
    uint64_t stamp;
    uint32_t cycle;
    uint32_t super_position;
    uint8_t rate;
} FrameObservation;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct gpiod_chip* chip;
static struct gpiod_line* line;
static pthread_t worker;
static uint8_t active, stop;
static TrmPpsModel model;
static FrameObservation frames[PPS_QUEUE];
static unsigned int head, tail;
static uint64_t edges[PPS_EDGES];
static unsigned int edge_index;

uint64_t TrmPpsMonotonicUs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void* monitor_main(void* unused)
{
    (void)unused;
    struct pollfd pfd = {.fd = gpiod_line_event_get_fd(line), .events = POLLIN};
    for (;;) {
        pthread_mutex_lock(&lock);
        int done = stop;
        pthread_mutex_unlock(&lock);
        if (done) break;
        int rc = poll(&pfd, 1, 10);
        if (rc < 0 && errno == EINTR) continue;
        pthread_mutex_lock(&lock);
        if (rc < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            TRM_LOG_ERROR("PPS GPIO event read failed; requesting shutdown");
            model.fatal = 1;
            pthread_mutex_unlock(&lock);
            break;
        }
        /* Drain every pending edge before processing delayed S0 observations. */
        unsigned int drained = 0;
        while (rc > 0 && (pfd.revents & POLLIN)) {
            if (++drained > PPS_QUEUE) {
                TRM_LOG_ERROR("PPS GPIO event flood; requesting shutdown");
                model.fatal = 1;
                break;
            }
            struct gpiod_line_event event;
            if (gpiod_line_event_read(line, &event)) {
                if (errno == EINTR) continue;
                TRM_LOG_ERROR("PPS GPIO event decode failed: %s", strerror(errno));
                model.fatal = 1;
                break;
            }
            uint64_t stamp = (uint64_t)event.ts.tv_sec * 1000000 + event.ts.tv_nsec / 1000;
            if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
                uint64_t previous = model.edge;
                uint32_t was_missing = model.missing;
                int accepted = TrmPpsModelEdge(&model, stamp);
                if (accepted == 1) edges[edge_index++ % PPS_EDGES] = stamp;
                if (accepted == 1 && was_missing) {
                    TRM_LOG_INFO("PPS GPIO recovered after %u missed cycles; waiting for frame validation",
                            was_missing);
                }
                TRM_LOG_INFO("PPS GPIO: monotonic_us=%llu interval_us=%llu accepted=%d",
                    (unsigned long long)stamp,
                    (unsigned long long)(previous && stamp >= previous ? stamp - previous : 0), accepted);
            }
            rc = poll(&pfd, 1, 0);
        }
        if (rc < 0 && errno != EINTR) model.fatal = 1;
        uint64_t now = TrmPpsMonotonicUs();
        uint32_t old_missing = model.missing;
        TrmPpsModelTick(&model, now);
        if (model.missing != old_missing) {
            if (model.fatal) {
                TRM_LOG_ERROR("PPS GPIO missing=%u/5; ACM deferred; requesting shutdown", model.missing);
            } else {
                TRM_LOG_WARN("PPS GPIO missing=%u/5; ACM deferred", model.missing);
            }
        }
        while (tail != head && now >= frames[tail % PPS_QUEUE].stamp + PPS_DELIVERY_GRACE_US) {
            FrameObservation f = frames[tail++ % PPS_QUEUE];
            uint64_t chosen = 0;
            uint64_t frame_start = f.stamp - model.config.s0_us[f.rate];
            for (unsigned int i = 0; i < PPS_EDGES; ++i) {
                if (edges[i] && edges[i] <= frame_start + model.tolerance && edges[i] > chosen)
                    chosen = edges[i];
            }
            int result = TrmPpsModelFrame(&model, f.stamp, chosen, f.cycle, f.rate, f.super_position);
            if (result < 0) model.fatal = 1;
            if (result || (f.cycle == 0 && f.rate == 0)) {
                if (result < 0) {
                    TRM_LOG_ERROR("PPS frame check: cycle=%u/%u rate=%u/%u super=%u result=%d; requesting shutdown",
                        f.cycle + 1, model.config.cycles, f.rate + 1, model.config.rates,
                        f.super_position, result);
                } else if (result > 0) {
                    TRM_LOG_WARN("PPS frame check: cycle=%u/%u rate=%u/%u super=%u result=%d",
                        f.cycle + 1, model.config.cycles, f.rate + 1, model.config.rates,
                        f.super_position, result);
                } else {
                    TRM_LOG_INFO("PPS frame check: cycle=%u/%u rate=%u/%u super=%u result=%d",
                    f.cycle + 1, model.config.cycles, f.rate + 1, model.config.rates,
                    f.super_position, result);
                }
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int TrmPpsMonitorStart(const TrmPpsConfig* config)
{
    TrmPpsMonitorStop();
    if (TrmPpsModelInit(&model, config, TrmPpsMonotonicUs())) return -1;
    chip = gpiod_chip_open("/dev/gpiochip1");
    if (!chip) return -1;
    line = gpiod_chip_get_line(chip, 18);
    if (!line || gpiod_line_request_rising_edge_events(line, "tk8710-pps-sync")) {
        gpiod_chip_close(chip); chip = NULL; line = NULL; return -1;
    }
    head = tail = edge_index = 0;
    memset(edges, 0, sizeof(edges));
    stop = 0;
    active = 1;
    if (pthread_create(&worker, NULL, monitor_main, NULL)) {
        active = 0;
        gpiod_line_release(line); gpiod_chip_close(chip); line = NULL; chip = NULL;
        return -1;
    }
    TRM_LOG_INFO("PPS GPIO started: gpiochip1/18 rising period_us=%llu cycles=%u rates=%u",
        (unsigned long long)model.period, config->cycles, config->rates);
    return 0;
}

void TrmPpsMonitorStop(void)
{
    pthread_mutex_lock(&lock);
    int was_active = active;
    stop = 1;
    pthread_mutex_unlock(&lock);
    if (!was_active) return;
    pthread_join(worker, NULL);
    pthread_mutex_lock(&lock);
    active = 0;
    gpiod_line_release(line); gpiod_chip_close(chip); line = NULL; chip = NULL;
    pthread_mutex_unlock(&lock);
}

int TrmPpsMonitorActive(void)
{
    pthread_mutex_lock(&lock); int value = active; pthread_mutex_unlock(&lock); return value;
}
int TrmPpsMonitorFailed(void)
{
    pthread_mutex_lock(&lock); int value = active && model.fatal; pthread_mutex_unlock(&lock); return value;
}
int TrmPpsMonitorMissing(void)
{
    pthread_mutex_lock(&lock);
    int value = active && model.missing && !model.fatal;
    pthread_mutex_unlock(&lock);
    return value;
}
void TrmPpsMonitorS0(uint32_t cycle, uint8_t rate, uint32_t super_position)
{
    uint64_t stamp = TrmPpsMonotonicUs();
    pthread_mutex_lock(&lock);
    if (active) {
        if (head - tail >= PPS_QUEUE || rate >= model.config.rates) {
            TRM_LOG_ERROR("PPS frame observation overflow/invalid rate; requesting shutdown");
            model.fatal = 1;
        } else {
            frames[head++ % PPS_QUEUE] = (FrameObservation){stamp, cycle, super_position, rate};
        }
    }
    pthread_mutex_unlock(&lock);
}
uint64_t TrmPpsMonitorDeadline(void)
{
    pthread_mutex_lock(&lock);
    uint64_t value = active ? TrmPpsModelDeadline(&model, TrmPpsMonotonicUs()) : 0;
    pthread_mutex_unlock(&lock);
    return value;
}
void TrmPpsMonitorArmRestart(void)
{
    pthread_mutex_lock(&lock);
    if (active) {
        model.restart_wait = 1;
        model.restart_from = TrmPpsMonotonicUs();
    }
    pthread_mutex_unlock(&lock);
}
#else
int TrmPpsMonitorStart(const TrmPpsConfig* c) { (void)c; return -1; }
void TrmPpsMonitorStop(void) {}
int TrmPpsMonitorActive(void) { return 0; }
int TrmPpsMonitorFailed(void) { return 0; }
int TrmPpsMonitorMissing(void) { return 0; }
uint64_t TrmPpsMonotonicUs(void) { return 0; }
void TrmPpsMonitorS0(uint32_t c, uint8_t r, uint32_t s) { (void)c; (void)r; (void)s; }
uint64_t TrmPpsMonitorDeadline(void) { return 0; }
void TrmPpsMonitorArmRestart(void) {}
#endif
