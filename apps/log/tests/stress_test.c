#include "log_udp.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void sleep_us(unsigned int usec) {
    struct timespec ts;
    ts.tv_sec = (time_t)(usec / 1000000u);
    ts.tv_nsec = (long)((usec % 1000000u) * 1000u);
    while (nanosleep(&ts, &ts) != 0) {
        if (errno != EINTR) {
            break;
        }
    }
}

static void run_sender(int idx, int duration_sec) {
    time_t end = time(NULL) + duration_sec;
    char msg[256];
    int i = 0;
    while (time(NULL) < end) {
        snprintf(msg, sizeof(msg), "proc=%d seq=%d payload=abcdefghijklmnopqrstuvwxyz0123456789", idx, i++);
        log_send(idx % 5, msg, 0);
        if ((i % 64) == 0) {
            log_flush();
            sleep_us(1000);
        }
    }
    log_flush();
    log_close();
}

int main(int argc, char **argv) {
    int workers = 20;
    int duration_sec = 60;
    int i;
    if (argc > 1) {
        workers = atoi(argv[1]);
    }
    if (argc > 2) {
        duration_sec = atoi(argv[2]);
    }

    for (i = 0; i < workers; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            run_sender(i, duration_sec);
            return 0;
        }
    }

    for (i = 0; i < workers; ++i) {
        int st;
        wait(&st);
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
            fprintf(stderr, "worker exit abnormal\n");
            return 1;
        }
    }

    printf("stress_test passed workers=%d duration=%d\n", workers, duration_sec);
    return 0;
}
