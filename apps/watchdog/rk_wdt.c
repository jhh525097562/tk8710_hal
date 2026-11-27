#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Defaults are fixed to the RK3506 wdt0 firmware contract, not user input. */
#ifndef WDT_RUN
#define WDT_RUN "/run/rk-watchdog"
#endif
#ifndef WDT_SYS
#define WDT_SYS "/sys/class/watchdog/watchdog0"
#endif
#ifndef WDT_DEV
#define WDT_DEV "/dev/watchdog0"
#endif
#ifndef WDT_LOG
#define WDT_LOG "/var/log/rk_watchdog.log"
#endif
#ifndef WDT_OOM_FILE
#define WDT_OOM_FILE "/proc/self/oom_score_adj"
#endif
#define WAIT_SECONDS 30
#define FEED_SECONDS 5
#define REQUEST_SECONDS 60
static volatile sig_atomic_t terminating;
static int actual_timeout, armed, configured;
static double last_feed;
static char boot_id[64];
static int oom_protected, priority_protected;

static double uptime_s(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_BOOTTIME, &ts)) return 0;
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int read_text(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int ok = fgets(buf, size, f) != NULL;
    fclose(f);
    if (!ok) return -1;
    buf[strcspn(buf, "\r\n")] = 0;
    return 0;
}

static void note(const char *reason)
{
    int saved_errno = errno;
    struct stat st;
    if (!stat(WDT_LOG, &st) && st.st_size > 262144) rename(WDT_LOG, WDT_LOG ".1");
    FILE *f = fopen(WDT_LOG, "a");
    if (!f) return;
    fprintf(f, "uptime=%.3f pid=%ld reason=%s errno=%d\n", uptime_s(), (long)getpid(), reason, saved_errno);
    fclose(f);
}

/* Best effort only: resource policy failures must never prevent feeding.
 * Apply before opening hardware. Keep the normal scheduler (no SCHED_FIFO). */
static void protect_resources(void)
{
    FILE *f = fopen(WDT_OOM_FILE, "w");
    int bad = 1;
    if (f) {
        bad = fprintf(f, "-900\n") < 0;
        if (fclose(f)) bad = 1;
    }
    char value[32];
    oom_protected = !bad && !read_text(WDT_OOM_FILE, value, sizeof(value)) &&
                    !strcmp(value, "-900");
    if (!oom_protected) note("warning_oom_protection_failed");
    int rc = setpriority(PRIO_PROCESS, 0, -10);
    errno = 0;
    int effective = getpriority(PRIO_PROCESS, 0);
    priority_protected = !rc && !errno && effective == -10;
    if (!priority_protected) note("warning_priority_protection_failed");
}

static void publish(const char *state, const char *reason)
{
    FILE *f = fopen(WDT_RUN "/status.tmp", "w");
    if (!f) return;
    fprintf(f, "{\"state\":\"%s\",\"reason\":\"%s\",\"pid\":%ld,"
            "\"boot_id\":\"%s\",\"updated_uptime_s\":%.3f,\"last_feed_uptime_s\":%.3f,"
            "\"armed\":%s,\"requested_timeout_s\":60,\"actual_timeout_s\":%d,\"feed_interval_s\":5,"
            "\"oom_protected\":%s,\"priority_protected\":%s}\n",
            state, reason, (long)getpid(), boot_id, uptime_s(), last_feed,
            armed ? "true" : "false", actual_timeout,
            oom_protected ? "true" : "false", priority_protected ? "true" : "false");
    int bad = ferror(f);
    if (fclose(f)) bad = 1;
    if (!bad) rename(WDT_RUN "/status.tmp", WDT_RUN "/status.json");
}

static void on_signal(int sig) { (void)sig; terminating = 1; }

static void pause_ms(long ms)
{
    struct timespec delay = {ms / 1000, (ms % 1000) * 1000000};
    while (nanosleep(&delay, &delay) && errno == EINTR && !terminating) {}
}

static int keepalive(int fd)
{
    int rc;
    do { rc = ioctl(fd, WDIOC_KEEPALIVE, 0); } while (rc && errno == EINTR && !terminating);
    if (!rc) last_feed = uptime_s();
    return rc;
}

/* Only reached AFTER opening/arming hardware. Never used for missing-device,
 * permission, busy-device, identity or nowayout preflight failures. */
_Noreturn static void armed_fault(int fd, const char *reason)
{
    note(reason);
    publish("fault", reason);
    /* nowayout hardware is the fallback if the software reboot syscall fails.
     * Keep fd open and stop feeding; do not hand a fault back to init respawn. */
    sync();
    if (reboot(RB_AUTOBOOT)) note("software_reboot_failed");
    for (;;) {
        (void)fd;
        pause_ms(1000);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (argc == 2 && !strcmp(argv[1], "--help")) {
            puts("rk_wdt: RK3506 wdt0 feeder; no arguments starts hardware protection.\n"
                 "Wait up to 30s, request 60s timeout, feed every 5s. See S15rk-watchdog status.");
            return 0;
        }
        fputs("Unsupported option; use --help (does not open the watchdog).\n", stderr);
        return 2;
    }
    if (mkdir(WDT_RUN, 0755) && errno != EEXIST) return 1;
    int lockfd = open(WDT_RUN "/service.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lockfd < 0) return 1;
    if (flock(lockfd, LOCK_EX | LOCK_NB)) { close(lockfd); return 0; }
    protect_resources();
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    if (read_text("/proc/sys/kernel/random/boot_id", boot_id, sizeof(boot_id))) strcpy(boot_id, "unknown");
    publish("waiting", "device_pending");
    double deadline = uptime_s() + WAIT_SECONDS;
    struct stat st;
    while (stat(WDT_DEV, &st)) {
        if (errno != ENOENT && errno != ENODEV) {
            publish("failed", "device_access_failed"); note("device_access_failed"); return 1;
        }
        if (terminating) { publish("stopped", "stopped_before_open"); return 0; }
        if (uptime_s() >= deadline) {
            publish("failed", "device_missing"); note("device_missing"); return 1;
        }
        publish("waiting", "device_pending");
        pause_ms(1000);
    }
    if (!S_ISCHR(st.st_mode)) {
        publish("failed", "not_character_device"); note("not_character_device"); return 1;
    }
    char value[80];
    if (read_text(WDT_SYS "/identity", value, sizeof(value)) || strcmp(value, "Synopsys DesignWare Watchdog")) {
        publish("failed", "wrong_device_or_sysfs_missing"); note("wrong_device_or_sysfs_missing"); return 1;
    }
    if (read_text(WDT_SYS "/nowayout", value, sizeof(value)) || strcmp(value, "1")) {
        publish("failed", "nowayout_required"); note("nowayout_required"); return 1;
    }
    if (terminating) { publish("stopped", "stopped_before_open"); return 0; }
    int fd = open(WDT_DEV, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        const char *reason = errno == EBUSY ? "device_busy" : "device_open_failed";
        publish("failed", reason); note(reason); return 1;
    }
    armed = 1; /* open can start the counter, even if later ioctl calls fail. */
    int set_failed = 1;
    for (int i = 0; i < 3 && !terminating; i++) {
        int requested = REQUEST_SECONDS;
        set_failed = ioctl(fd, WDIOC_SETTIMEOUT, &requested);
        int got = ioctl(fd, WDIOC_GETTIMEOUT, &actual_timeout);
        int fed = keepalive(fd);
        if (!got && !fed && actual_timeout >= 3 * FEED_SECONDS && actual_timeout <= 600) {
            configured = 1;
            break;
        }
        pause_ms(200);
    }
    if (!configured && !terminating) armed_fault(fd, "armed_setup_failed");
    const char *state = set_failed ? "degraded" : "feeding";
    const char *reason = set_failed ? "using_verified_existing_timeout" : "ok";
    if (!terminating) { publish(state, reason); note(reason); }
    while (!terminating) {
        pause_ms(FEED_SECONDS * 1000);
        if (terminating) break;
        int ok = 0;
        for (int i = 0; i < 3; i++) {
            if (!keepalive(fd)) { ok = 1; break; }
            pause_ms(200);
        }
        if (!ok) armed_fault(fd, "keepalive_failed");
        publish(state, reason);
    }
    publish("stopped", "armed_service_terminated");
    note("armed_service_terminated");
    /* No magic close and no DISABLECARD. nowayout preserves timeout protection. */
    close(fd);
    close(lockfd);
    return 0;
}
