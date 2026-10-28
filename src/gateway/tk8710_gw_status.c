#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_status.h"
#ifdef PLATFORM_RK3506
#include "trm/trm_log.h"
#endif

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t status_lock = PTHREAD_MUTEX_INITIALIZER;
static int owner_fd = -1;
static int directory_fd = -1;
static GwStatusPhase current_phase;
static char current_reason[64];
static char boot_id[64];
static unsigned long long start_ticks;
static uint64_t since_ms, last_write_ms;
static int gps_enable = -1, current_error, finished;
static int write_warning;

static const char* const phase_names[] = {
    "starting", "wait_ns_config", "configuring", "wait_gps_sync",
    "wait_pps_alignment", "running", "degraded", "scanning",
    "stopping", "stopped", "fault"
};
/* Consumer display deadlines; they do not change actual business timeouts. */
static const unsigned int phase_timeout_s[] = {
    60, 35, 120, 330, 90, 0, 0, 0, 30, 0, 0
};

static uint64_t MonotonicMs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int ReadIdentity(void)
{
    char buffer[2048];
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld/stat", (long)getpid());
    FILE* fp = fopen(path, "r");
    if (!fp) return -1;
    int ok = fgets(buffer, sizeof(buffer), fp) != NULL;
    fclose(fp);
    if (!ok) return -1;
    char* end = strrchr(buffer, ')');
    if (!end) return -1;
    char* cursor = end + 1;
    for (int field = 3; field < 22; ++field) {
        while (*cursor == ' ') ++cursor;
        if (!*cursor) return -1;
        while (*cursor && *cursor != ' ') ++cursor;
    }
    errno = 0;
    start_ticks = strtoull(cursor, &end, 10);
    if (errno || end == cursor || !start_ticks) return -1;
    fp = fopen("/proc/sys/kernel/random/boot_id", "r");
    if (!fp) return -1;
    ok = fgets(boot_id, sizeof(boot_id), fp) != NULL;
    fclose(fp);
    if (!ok) return -1;
    boot_id[strcspn(boot_id, "\r\n")] = 0;
    return strlen(boot_id) == 36 ? 0 : -1;
}

static void SetReason(const char* reason)
{
    size_t i = 0;
    if (!reason || !*reason) reason = "unspecified";
    for (; reason[i] && i < sizeof(current_reason) - 1; ++i) {
        char c = reason[i];
        current_reason[i] = ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                            c == '_') ? c : '_';
    }
    current_reason[i] = 0;
}

/* Called with status_lock held. Atomic rename, tmpfs only, no IRQ callback I/O. */
static void PublishTo(int force, const char* filename)
{
    uint64_t now = MonotonicMs();
    if (directory_fd < 0 || (!force && last_write_ms && now - last_write_ms < 1000)) return;
    char temporary[64];
    snprintf(temporary, sizeof(temporary), "status.%ld.tmp", (long)getpid());
    int fd = openat(directory_fd, temporary, O_WRONLY | O_CREAT | O_TRUNC |
                    O_CLOEXEC | O_NOFOLLOW, 0644);
    int failed = fd < 0;
    FILE* fp = NULL;
    if (!failed) {
        if (fchmod(fd, 0644) != 0 || (fp = fdopen(fd, "w")) == NULL) {
            close(fd); failed = 1;
        }
    }
    if (fp) {
        failed = fprintf(fp,
            "{\"schema\":1,\"pid\":%ld,\"start_ticks\":%llu,\"boot_id\":\"%s\","
            "\"state\":\"%s\",\"reason\":\"%s\",\"error_code\":%d,"
            "\"gps_enable\":%d,\"business_ready\":%s,\"exited\":%s,"
            "\"state_since_mono_ms\":%llu,\"updated_mono_ms\":%llu,"
            "\"phase_timeout_s\":%u}\n",
            (long)getpid(), start_ticks, boot_id, phase_names[current_phase], current_reason,
            current_error, gps_enable, current_phase == GW_STATUS_RUNNING && !finished ? "true" : "false",
            finished ? "true" : "false", (unsigned long long)since_ms,
            (unsigned long long)now, phase_timeout_s[current_phase]) < 0;
        if (fclose(fp) != 0) failed = 1;
    }
    if (!failed && renameat(directory_fd, temporary, directory_fd, filename) != 0) failed = 1;
    if (failed) {
        int saved = errno;
        (void)unlinkat(directory_fd, temporary, 0);
        if (!write_warning) fprintf(stderr, "Gateway status snapshot write failed: %s\n", strerror(saved));
        write_warning = 1;
    } else {
        write_warning = 0;
        last_write_ms = now;
    }
}

static void Publish(int force)
{
    PublishTo(force, "status.json");
}

int GwStatusInit(const char* directory)
{
    if (!directory || !*directory) return -1;
    pthread_mutex_lock(&status_lock);
    if (owner_fd >= 0) { pthread_mutex_unlock(&status_lock); return -1; }
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) goto fail;
    directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd < 0) goto fail;
    owner_fd = openat(directory_fd, "status.lock", O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (owner_fd < 0 || flock(owner_fd, LOCK_EX | LOCK_NB) != 0 || ReadIdentity() != 0) goto fail;
    current_phase = GW_STATUS_STARTING;
    SetReason("process_start");
    since_ms = MonotonicMs(); last_write_ms = 0;
    finished = current_error = write_warning = 0;
    gps_enable = -1;
    Publish(1);
    pthread_mutex_unlock(&status_lock);
    return 0;
fail:
    if (owner_fd >= 0) close(owner_fd);
    if (directory_fd >= 0) close(directory_fd);
    owner_fd = directory_fd = -1;
    pthread_mutex_unlock(&status_lock);
    return -1;
}

void GwStatusSet(GwStatusPhase phase, const char* reason, int error_code)
{
    if (phase < GW_STATUS_STARTING || phase > GW_STATUS_FAULT) return;
    pthread_mutex_lock(&status_lock);
    /* Preserve first fatal cause; late IPC callbacks cannot resurrect readiness. */
    if (directory_fd >= 0 && !finished && current_phase != GW_STATUS_FAULT &&
        (current_phase != GW_STATUS_STOPPING || phase == GW_STATUS_FAULT || phase == GW_STATUS_STOPPED)) {
        if (phase != current_phase) since_ms = MonotonicMs();
        current_phase = phase;
        current_error = error_code;
        SetReason(reason);
        Publish(1);
#ifdef PLATFORM_RK3506
        if (phase == GW_STATUS_FAULT) {
            TRM_LOG_ERROR("Gateway fault: reason=%s error=%d pid=%ld",
                current_reason, current_error, (long)getpid());
            TRM_LogFlushFile();
        }
#endif
    }
    pthread_mutex_unlock(&status_lock);
}

void GwStatusSetGpsPolicy(int enabled)
{
    if (enabled < -1 || enabled > 1) return;
    pthread_mutex_lock(&status_lock);
    if (!finished && current_phase != GW_STATUS_FAULT && current_phase != GW_STATUS_STOPPING) {
        gps_enable = enabled; Publish(1);
    }
    pthread_mutex_unlock(&status_lock);
}

void GwStatusTouch(void)
{
    pthread_mutex_lock(&status_lock);
    if (!finished) Publish(0);
    pthread_mutex_unlock(&status_lock);
}

void GwStatusFinish(int exit_code)
{
    pthread_mutex_lock(&status_lock);
    if (directory_fd >= 0 && !finished) {
        if (current_phase != GW_STATUS_FAULT) {
            int was_stopping = current_phase == GW_STATUS_STOPPING;
            current_phase = exit_code ? GW_STATUS_FAULT : GW_STATUS_STOPPED;
            current_error = exit_code;
            /* Retain the reason for an intentional restart or signal shutdown. */
            if (exit_code || !was_stopping)
                SetReason(exit_code ? "process_exit_error" : "process_exit");
            since_ms = MonotonicMs();
        }
        finished = 1; Publish(1);
        PublishTo(1, "last_exit.json");
        close(directory_fd); close(owner_fd);
        directory_fd = owner_fd = -1;
    }
    pthread_mutex_unlock(&status_lock);
}
#else
int GwStatusInit(const char* directory) { (void)directory; return 0; }
void GwStatusSet(GwStatusPhase phase, const char* reason, int error_code)
{ (void)phase; (void)reason; (void)error_code; }
void GwStatusSetGpsPolicy(int enabled) { (void)enabled; }
void GwStatusTouch(void) {}
void GwStatusFinish(int exit_code) { (void)exit_code; }
#endif
