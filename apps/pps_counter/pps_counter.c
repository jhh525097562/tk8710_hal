#define _POSIX_C_SOURCE 200809L
#include <gpiod.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t stopped;
static void stop(int sig) { (void)sig; stopped = 1; }
static double seconds(struct timespec t) { return t.tv_sec + t.tv_nsec / 1e9; }
static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) { perror("clock_gettime"); exit(1); }
    return seconds(t);
}
int main(int argc, char **argv) {
    long duration = 60;
    if (argc > 2) { fprintf(stderr, "Usage: %s [seconds: 0=until Ctrl-C, default=60]\n", argv[0]); return 2; }
    if (argc == 2) {
        char *end;
        errno = 0; duration = strtol(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || duration < 0 || duration > 86400) {
            fprintf(stderr, "seconds must be 0..86400\n"); return 2;
        }
    }
    struct sigaction sa = {0};
    sa.sa_handler = stop; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);
    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip1");
    if (!chip) { perror("open gpiochip1"); return 1; }
    struct gpiod_line *line = gpiod_chip_get_line(chip, 18);
    if (!line || gpiod_line_request_rising_edge_events(line, "rtk-pps-counter")) {
        perror("request GPIO1_C2 rising input"); gpiod_chip_close(chip); return 1;
    }
    printf("START chip=gpiochip1 line=18 GPIO1_C2 edge=rising duration_s=%ld\n", duration);
    uint64_t count = 0;
    double start = now(), previous = 0;
    int rc = 0;
    while (!stopped && (!duration || now() - start < duration)) {
        struct timespec timeout = {0, 200000000};
        int ready = gpiod_line_event_wait(line, &timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("event_wait"); rc = 1; break;
        }
        if (!ready) continue;
        struct gpiod_line_event event;
        if (gpiod_line_event_read(line, &event)) {
            if (errno == EINTR) continue;
            perror("event_read"); rc = 1; break;
        }
        if (event.event_type != GPIOD_LINE_EVENT_RISING_EDGE) continue;
        double stamp = seconds(event.ts);
        ++count;
        printf("RISING count=%" PRIu64 " monotonic_s=%.9f interval_s=%.9f\n",
               count, stamp, count > 1 ? stamp - previous : 0.0);
        previous = stamp;
    }
    printf("SUMMARY count=%" PRIu64 " elapsed_s=%.3f result=%s\n", count, now()-start, rc ? "error" : "ok");
    gpiod_line_release(line); gpiod_chip_close(chip);
    return rc;
}
