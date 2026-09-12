#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_gps.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_RK3506) || defined(TK8710_GPS_TEST_HOOKS)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef TK8710_GPS_TEST_HOOKS
#include <ctype.h>
#endif

#define GW_GPS_DETECT_TIMEOUT_MS 1000u
#define GW_GPS_DETECT_RETRY_DELAY_MS 200u
#define GW_GPS_PERIOD_UNIT_US 1000000ull
#define GW_GPS_INFO_DIR "/userdata/GPS"
#define GW_GPS_INFO_FILE GW_GPS_INFO_DIR "/GPSinfo.txt"
#define GW_GPS_INFO_TMP_FORMAT GW_GPS_INFO_DIR "/GPSinfo.txt.tmp.%ld"

#ifdef TK8710_GPS_TEST_HOOKS
#define GW_GPS_TEST_MARKER_FORMAT "/tmp/tk8710_gps_test_%s.done"
#define GW_GPS_TEST_MARKER_PATH_MAX 128

static GwGpsTestScenario g_test_scenario = GW_GPS_TEST_PASSTHROUGH;
static uint8_t g_test_released;
static char g_test_marker_path[GW_GPS_TEST_MARKER_PATH_MAX];

const char* GwGpsTestScenarioName(GwGpsTestScenario scenario)
{
    switch (scenario) {
        case GW_GPS_TEST_PASSTHROUGH: return "passthrough";
        case GW_GPS_TEST_NO_MODULE: return "no-module";
        case GW_GPS_TEST_NO_PPS: return "no-pps";
        case GW_GPS_TEST_NO_PPS_THEN_RECOVER: return "no-pps-then-recover";
        default: return "unknown";
    }
}

int GwGpsTestParseScenario(const char* name, GwGpsTestScenario* scenario)
{
    GwGpsTestScenario value;

    if (name == NULL || scenario == NULL) {
        return -1;
    }
    for (value = GW_GPS_TEST_PASSTHROUGH;
         value <= GW_GPS_TEST_NO_PPS_THEN_RECOVER; value++) {
        if (strcmp(name, GwGpsTestScenarioName(value)) == 0) {
            *scenario = value;
            return 0;
        }
    }
    return -1;
}

static int build_test_marker_path(const char* run_id, char* path, size_t path_size)
{
    size_t index;
    int count;

    if (run_id == NULL || run_id[0] == '\0' ||
        strlen(run_id) > GW_GPS_TEST_RUN_ID_MAX || path == NULL || path_size == 0) {
        return -1;
    }
    for (index = 0; run_id[index] != '\0'; ++index) {
        unsigned char character = (unsigned char)run_id[index];

        if (!isalnum(character) && character != '-' && character != '_' && character != '.') {
            return -1;
        }
    }
    count = snprintf(path, path_size, GW_GPS_TEST_MARKER_FORMAT, run_id);
    return count > 0 && (size_t)count < path_size ? 0 : -1;
}

void GwGpsTestReset(void)
{
    g_test_scenario = GW_GPS_TEST_PASSTHROUGH;
    g_test_released = 0;
    g_test_marker_path[0] = '\0';
}

int GwGpsTestSetScenario(GwGpsTestScenario scenario, const char* run_id)
{
    struct stat marker_stat;

    GwGpsTestReset();
    if (scenario < GW_GPS_TEST_PASSTHROUGH ||
        scenario > GW_GPS_TEST_NO_PPS_THEN_RECOVER) {
        return -1;
    }
    if (scenario != GW_GPS_TEST_NO_PPS_THEN_RECOVER && run_id != NULL) {
        return -1;
    }
    if (scenario == GW_GPS_TEST_NO_PPS_THEN_RECOVER) {
        if (build_test_marker_path(run_id, g_test_marker_path,
            sizeof(g_test_marker_path)) != 0) {
            return -1;
        }
        if (lstat(g_test_marker_path, &marker_stat) == 0) {
            if (!S_ISREG(marker_stat.st_mode) || marker_stat.st_uid != geteuid()) {
                fprintf(stderr, "GPS TEST HOOK: unsafe marker rejected: %s\n",
                    g_test_marker_path);
                GwGpsTestReset();
                return -1;
            }
            g_test_released = 1;
            printf("GPS TEST HOOK: run-id=%s already consumed; scenario=passthrough\n",
                run_id);
            return 0;
        } else if (errno != ENOENT) {
            GwGpsTestReset();
            return -1;
        }
    }
    g_test_scenario = scenario;
    printf("GPS TEST HOOK: scenario=%s%s%s\n", GwGpsTestScenarioName(scenario),
        run_id != NULL ? " run-id=" : "", run_id != NULL ? run_id : "");
    return 0;
}

GwGpsTestScenario GwGpsTestGetScenario(void)
{
    return g_test_scenario;
}

int GwGpsTestClearMarker(const char* run_id)
{
    char path[GW_GPS_TEST_MARKER_PATH_MAX];
    struct stat marker_stat;

    if (build_test_marker_path(run_id, path, sizeof(path)) != 0) {
        return -1;
    }
    if (lstat(path, &marker_stat) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    if (!S_ISREG(marker_stat.st_mode) || marker_stat.st_uid != geteuid()) {
        return -1;
    }
    if (unlink(path) != 0) {
        return -1;
    }
    return 0;
}

static void release_recovery_scenario(void)
{
    if (g_test_scenario != GW_GPS_TEST_NO_PPS_THEN_RECOVER || g_test_released) {
        return;
    }
    g_test_released = 1;
    printf("GPS TEST HOOK: PPS injection released after local degradation\n");
}

static int persist_recovery_marker(void)
{
    int flags = O_WRONLY | O_CREAT | O_EXCL;
    int fd;
    ssize_t write_count;
    int close_result;
    static const char marker_text[] = "no-pps-then-recover completed\n";

    if (g_test_scenario != GW_GPS_TEST_NO_PPS_THEN_RECOVER || !g_test_released) {
        return 0;
    }
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    fd = open(g_test_marker_path, flags, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            return 0;
        }
        fprintf(stderr, "GPS TEST HOOK: cannot create marker %s: %s\n",
            g_test_marker_path, strerror(errno));
        return -1;
    }
    write_count = write(fd, marker_text, sizeof(marker_text) - 1);
    close_result = close(fd);
    if (write_count != (ssize_t)(sizeof(marker_text) - 1) || close_result != 0) {
        fprintf(stderr, "GPS TEST HOOK: cannot persist marker %s\n",
            g_test_marker_path);
        (void)unlink(g_test_marker_path);
        return -1;
    }
    printf("GPS TEST HOOK: recovery confirmed; restart marker=%s\n",
        g_test_marker_path);
    return 0;
}

static TK8710PpsError get_version_for_gateway(TK8710PpsContext* context,
    char* version, uint32_t version_size)
{
    char raw_version[TK8710_PPS_DIAG_TEXT_MAX];
    TK8710PpsError raw_error;

    raw_version[0] = '\0';
    raw_error = TK8710PpsGetVersion(context, raw_version, sizeof(raw_version));
    if (g_test_scenario == GW_GPS_TEST_NO_MODULE) {
        version[0] = '\0';
        printf("GPS TEST HOOK: VERSION raw_error=%d raw=\"%s\" "
               "effective_error=%d effective=NO_MODULE\n",
               raw_error, raw_version, TK8710_PPS_ERROR_TIMEOUT);
        return TK8710_PPS_ERROR_TIMEOUT;
    }
    if (raw_error == TK8710_PPS_OK) {
        size_t length = strlen(raw_version);

        if (length + 1 > version_size) {
            return TK8710_PPS_ERROR_OVERFLOW;
        }
        memcpy(version, raw_version, length + 1);
    }
    return raw_error;
}

static TK8710PpsError get_effective_status(TK8710PpsContext* context,
    TK8710PpsStatus* status)
{
    TK8710PpsStatus raw_status;
    TK8710PpsError raw_error = TK8710PpsGetStatus(context, &raw_status);
    uint8_t inject_no_pps = g_test_scenario == GW_GPS_TEST_NO_PPS ||
        (g_test_scenario == GW_GPS_TEST_NO_PPS_THEN_RECOVER && !g_test_released);

    if (raw_error != TK8710_PPS_OK) {
        printf("GPS TEST HOOK: STATUS raw_error=%d effective_error=%d\n",
            raw_error, raw_error);
        return raw_error;
    }
    *status = raw_status;
    if (inject_no_pps) {
        status->pps_seen = 0;
    }
    if (g_test_scenario != GW_GPS_TEST_PASSTHROUGH) {
        printf("GPS TEST HOOK: STATUS raw GPS=%u PPS=%u FIX=%u RMC=%c; "
               "effective GPS=%u PPS=%u FIX=%u RMC=%c\n",
               raw_status.gps_online, raw_status.pps_seen, raw_status.fix_valid,
               raw_status.rmc_status != '\0' ? raw_status.rmc_status : '?',
               status->gps_online, status->pps_seen, status->fix_valid,
               status->rmc_status != '\0' ? status->rmc_status : '?');
    }
    return TK8710_PPS_OK;
}
#else
static TK8710PpsError get_version_for_gateway(TK8710PpsContext* context,
    char* version, uint32_t version_size)
{
    return TK8710PpsGetVersion(context, version, version_size);
}

static TK8710PpsError get_effective_status(TK8710PpsContext* context,
    TK8710PpsStatus* status)
{
    return TK8710PpsGetStatus(context, status);
}
#endif

static int write_gps_info(const TK8710PpsPosition* position, uint64_t status)
{
#ifdef PLATFORM_RK3506
    double latitude = position != NULL && position->valid ? position->latitude : 0.0;
    double longitude = position != NULL && position->valid ? position->longitude : 0.0;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd;
    FILE* file;
    int result = -1;
    int write_ok;
    int path_length;
    char temporary_path[128];

    if (mkdir(GW_GPS_INFO_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (chmod(GW_GPS_INFO_DIR, 0755) != 0) {
        return -1;
    }
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    path_length = snprintf(temporary_path, sizeof(temporary_path),
        GW_GPS_INFO_TMP_FORMAT, (long)getpid());
    if (path_length <= 0 || (size_t)path_length >= sizeof(temporary_path)) {
        return -1;
    }
    fd = open(temporary_path, flags, 0644);
    if (fd < 0) {
        return -1;
    }
    if (fchmod(fd, 0644) != 0) {
        close(fd);
        (void)unlink(temporary_path);
        return -1;
    }
    file = fdopen(fd, "w");
    if (file == NULL) {
        close(fd);
        (void)unlink(temporary_path);
        return -1;
    }
    write_ok = fprintf(file, "lon=%.6f\nlat=%.6f\nstatus=%llu\n",
        longitude, latitude, (unsigned long long)status) > 0;
    if (write_ok && (fflush(file) != 0 || fsync(fd) != 0)) {
        write_ok = 0;
    }
    if (fclose(file) != 0) {
        write_ok = 0;
    }
    if (write_ok && rename(temporary_path, GW_GPS_INFO_FILE) == 0) {
        result = 0;
    }
    if (result != 0) {
        (void)unlink(temporary_path);
    }
    return result;
#else
    (void)position;
    (void)status;
    return 0;
#endif
}

static void record_gps_unavailable(GwGpsManager* manager)
{
    if (manager == NULL) {
        return;
    }
    manager->consecutive_healthy_status = 0;
    if (write_gps_info(NULL, 0) != 0) {
        fprintf(stderr, "GPS info file update failed: %s\n", GW_GPS_INFO_FILE);
    }
}

static TK8710PpsError get_status_for_gateway(GwGpsManager* manager,
    TK8710PpsStatus* status)
{
    TK8710PpsPosition position;
    TK8710PpsError status_error;
    TK8710PpsError position_error = TK8710_PPS_ERROR_NOT_INITIALIZED;
    uint8_t healthy = 0;

    if (manager == NULL) {
        return TK8710_PPS_ERROR_PARAM;
    }
    memset(&position, 0, sizeof(position));
    status_error = get_effective_status(&manager->pps, status);
    if (status_error == TK8710_PPS_OK) {
        position_error = TK8710PpsGetPosition(&manager->pps, &position);
        healthy = position_error == TK8710_PPS_OK && position.valid &&
            GwGpsBasicReady(status);
    }
    if (healthy) {
        if (manager->consecutive_healthy_status < UINT64_MAX) {
            manager->consecutive_healthy_status++;
        }
    } else {
        manager->consecutive_healthy_status = 0;
    }
    if (write_gps_info(position_error == TK8710_PPS_OK ? &position : NULL,
            manager->consecutive_healthy_status) != 0) {
        fprintf(stderr, "GPS info file update failed: %s\n", GW_GPS_INFO_FILE);
    }
    if (status_error == TK8710_PPS_OK && position_error != TK8710_PPS_OK) {
        fprintf(stderr, "GPS position query failed: %d\n", position_error);
    }
    return status_error;
}

static uint64_t get_monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
}

static void delay_ms(uint32_t delay)
{
    struct timespec request;

    request.tv_sec = (time_t)(delay / 1000u);
    request.tv_nsec = (long)(delay % 1000u) * 1000000L;
    (void)nanosleep(&request, NULL);
}

static int reopen_uart(GwGpsManager* manager)
{
    TK8710PpsError error;

    if (manager->uart_initialized) {
        TK8710PpsClose(&manager->pps);
        manager->uart_initialized = 0;
    }
    error = TK8710PpsInit(&manager->pps, &manager->pps_config);
    if (error != TK8710_PPS_OK) {
        return -1;
    }
    manager->uart_initialized = 1;
    return 0;
}

void GwGpsGetDefaultPolicy(GwGpsPolicy* policy)
{
    if (policy == NULL) {
        return;
    }
    policy->detect_attempts = 2;
    policy->search_timeout_ms = 300000;
    policy->poll_interval_ms = 10000;
    policy->align_timeout_ms = 60000;
    policy->consecutive_limit = 3;
}

uint8_t GwGpsBasicReady(const TK8710PpsStatus* status)
{
    return status != NULL && status->gps_online && status->pps_seen &&
        status->fix_valid && status->rmc_status == 'A';
}

uint8_t GwGpsFullHealthy(const TK8710PpsStatus* status, uint32_t expected_period_s)
{
    return status != NULL && expected_period_s != 0 &&
        TK8710PpsEvaluateHealth(status, expected_period_s) == TK8710_PPS_HEALTHY;
}

int GwGpsPeriodFromSlotCalc(const TRM_MultiRateSlotCalcOutput* output,
    uint32_t* period_s)
{
    uint64_t total_us;
    uint64_t seconds;

    if (output == NULL || period_s == NULL || output->framePeriod == 0 ||
        output->frameCount == 0) {
        return -1;
    }
    total_us = (uint64_t)output->framePeriod * output->frameCount;
    if (total_us % GW_GPS_PERIOD_UNIT_US != 0) {
        return -1;
    }
    seconds = total_us / GW_GPS_PERIOD_UNIT_US;
    if (seconds == 0 || seconds > UINT32_MAX) {
        return -1;
    }
    *period_s = (uint32_t)seconds;
    return 0;
}

GwGpsAction GwGpsUpdateMonitor(GwGpsManager* manager, uint8_t query_ok,
    const TK8710PpsStatus* status)
{
    if (manager == NULL || manager->policy.consecutive_limit == 0) {
        return GW_GPS_ACTION_NONE;
    }

    if (manager->mode == GW_GPS_MODE_EXTERNAL_ACTIVE) {
        if (query_ok && GwGpsFullHealthy(status, manager->expected_period_s)) {
            manager->consecutive_abnormal = 0;
            return GW_GPS_ACTION_NONE;
        }
        if (manager->consecutive_abnormal < UINT32_MAX) {
            manager->consecutive_abnormal++;
        }
        if (manager->consecutive_abnormal >= manager->policy.consecutive_limit) {
            manager->mode = GW_GPS_MODE_FAULT;
            return GW_GPS_ACTION_FATAL_EXIT;
        }
    } else if (manager->mode == GW_GPS_MODE_LOCAL_DEGRADED) {
        if (query_ok && GwGpsBasicReady(status)) {
            if (manager->consecutive_recovered < UINT32_MAX) {
                manager->consecutive_recovered++;
            }
            if (manager->consecutive_recovered >= manager->policy.consecutive_limit) {
                return GW_GPS_ACTION_RESTART_EXIT;
            }
        } else {
            manager->consecutive_recovered = 0;
        }
    }
    return GW_GPS_ACTION_NONE;
}

void GwGpsPrintDiagnostics(GwGpsManager* manager, const char* reason)
{
    TK8710PpsDiagnostics diagnostics;
    TK8710PpsError error;

    fprintf(stderr, "GPS/PPS diagnostics: %s\n", reason != NULL ? reason : "unknown");
    if (manager == NULL || !manager->uart_initialized) {
        fprintf(stderr, "GPS/PPS diagnostics unavailable: UART is closed\n");
        return;
    }
    memset(&diagnostics, 0, sizeof(diagnostics));
    error = TK8710PpsGetDiagnostics(&manager->pps, &diagnostics);
    fprintf(stderr, "  GET PPS: %s\n", diagnostics.pps);
    fprintf(stderr, "  GET OUT: %s\n", diagnostics.output);
    fprintf(stderr, "  GET SYNC: %s\n", diagnostics.sync);
    fprintf(stderr, "  GET GPS: %s\n", diagnostics.gps);
    fprintf(stderr, "  GET RMC DIAG: %s\n", diagnostics.rmc);
    if (error != TK8710_PPS_OK) {
        fprintf(stderr, "  diagnostics error=%d success_mask=0x%02X\n",
            error, diagnostics.success_mask);
    }
}

int GwGpsStartup(GwGpsManager* manager, const TK8710PpsConfig* pps_config,
    const GwGpsPolicy* policy, const volatile int* running,
    char* version, uint32_t version_size)
{
    TK8710PpsConfig default_config;
    TK8710PpsStatus status;
    uint64_t start_ms;
    uint32_t attempt;

    if (manager == NULL || version == NULL || version_size == 0) {
        return -1;
    }
    memset(manager, 0, sizeof(*manager));
    manager->running = running;
    record_gps_unavailable(manager);
    version[0] = '\0';
    if (policy != NULL) {
        manager->policy = *policy;
    } else {
        GwGpsGetDefaultPolicy(&manager->policy);
    }
    if (manager->policy.detect_attempts == 0 || manager->policy.poll_interval_ms == 0 ||
        manager->policy.consecutive_limit == 0) {
        return -1;
    }
    if (pps_config != NULL) {
        manager->pps_config = *pps_config;
    } else {
        if (TK8710PpsGetDefaultConfig(&default_config) != TK8710_PPS_OK) {
            return -1;
        }
        manager->pps_config = default_config;
    }
    manager->pps_config.command_timeout_ms = GW_GPS_DETECT_TIMEOUT_MS;

    for (attempt = 0; attempt < manager->policy.detect_attempts; ++attempt) {
        if (reopen_uart(manager) == 0 &&
            get_version_for_gateway(&manager->pps, version, version_size) == TK8710_PPS_OK) {
            break;
        }
        if (attempt + 1 < manager->policy.detect_attempts) {
            delay_ms(GW_GPS_DETECT_RETRY_DELAY_MS);
        }
    }
    if (attempt == manager->policy.detect_attempts) {
        GwGpsClose(manager);
        manager->mode = GW_GPS_MODE_ABSENT;
        return 0;
    }

    manager->mode = GW_GPS_MODE_SEARCHING;
    start_ms = get_monotonic_ms();
    for (;;) {
        TK8710PpsError status_error = get_status_for_gateway(manager, &status);

        if (manager->running != NULL && !*manager->running) {
            return -1;
        }

        if (status_error == TK8710_PPS_OK) {
            printf("GPS search: GPS=%u PPS=%u FIX=%u RMC=%c SATS=%u SNR=%u\n",
                status.gps_online, status.pps_seen, status.fix_valid,
                status.rmc_status != '\0' ? status.rmc_status : '?',
                status.satellites, status.snr);
            if (GwGpsBasicReady(&status)) {
                manager->mode = GW_GPS_MODE_EXTERNAL_READY;
                return 0;
            }
        } else {
            fprintf(stderr, "GPS search status query failed: %d\n", status_error);
        }
        if (get_monotonic_ms() - start_ms >= manager->policy.search_timeout_ms) {
            GwGpsPrintDiagnostics(manager, "GPS cold-start timeout");
            manager->mode = GW_GPS_MODE_LOCAL_DEGRADED;
#ifdef TK8710_GPS_TEST_HOOKS
            release_recovery_scenario();
#endif
            return 0;
        }
        if (status_error != TK8710_PPS_OK) {
            reopen_uart(manager);
        }
        delay_ms(manager->policy.poll_interval_ms);
    }
}

uint8_t GwGpsCanConfigurePeriod(const GwGpsManager* manager)
{
    if (manager == NULL) {
        return 0;
    }

    return manager->mode == GW_GPS_MODE_EXTERNAL_READY ||
        manager->mode == GW_GPS_MODE_EXTERNAL_ACTIVE ||
        manager->mode == GW_GPS_MODE_LOCAL_DEGRADED ||
        manager->mode == GW_GPS_MODE_LOCAL_STATIC;
}

int GwGpsConfigurePeriod(GwGpsManager* manager, uint32_t period_s)
{
    TK8710PpsStatus status;
    uint64_t start_ms;

    if (!GwGpsCanConfigurePeriod(manager) || period_s == 0) {
        return -1;
    }
    if (!manager->uart_initialized && reopen_uart(manager) != 0) {
        manager->mode = GW_GPS_MODE_LOCAL_DEGRADED;
        return -1;
    }
    manager->expected_period_s = period_s;
    printf("GPS/PPS command: OUT ON\n");
    if (TK8710PpsSetOutput(&manager->pps, 1) != TK8710_PPS_OK) {
        GwGpsPrintDiagnostics(manager, "OUT ON failed");
        manager->mode = GW_GPS_MODE_LOCAL_DEGRADED;
        return -1;
    }
    printf("GPS/PPS command: SET PERIOD %u\n", period_s);
    if (TK8710PpsSetPeriod(&manager->pps, period_s) != TK8710_PPS_OK) {
        GwGpsPrintDiagnostics(manager, "SET PERIOD failed");
        manager->mode = GW_GPS_MODE_LOCAL_DEGRADED;
        return -1;
    }

    start_ms = get_monotonic_ms();
    for (;;) {
        TK8710PpsError status_error = get_status_for_gateway(manager, &status);

        if (manager->running != NULL && !*manager->running) {
            return -1;
        }

        if (status_error == TK8710_PPS_OK) {
            printf("PPS alignment: STATE=%s PERIOD=%u OUT=%u PENDING=%u "
                   "ALIGN=%u HOLD=%u BAD=%u RESYNC=%u\n",
                   status.state, status.period, status.output_enabled, status.pending,
                   status.aligned, status.holdover, status.bad_period,
                   status.resync_pending);
            if (GwGpsFullHealthy(&status, period_s)) {
                manager->mode = GW_GPS_MODE_EXTERNAL_ACTIVE;
                manager->consecutive_abnormal = 0;
                return 0;
            }
        } else {
            fprintf(stderr, "PPS alignment status query failed: %d\n", status_error);
            reopen_uart(manager);
        }
        if (get_monotonic_ms() - start_ms >= manager->policy.align_timeout_ms) {
            GwGpsPrintDiagnostics(manager, "PPS alignment timeout");
            manager->mode = GW_GPS_MODE_LOCAL_DEGRADED;
            return -1;
        }
        delay_ms(manager->policy.poll_interval_ms);
    }
}

GwGpsAction GwGpsPoll(GwGpsManager* manager)
{
    TK8710PpsStatus status;
    TK8710PpsError error;
    GwGpsAction action;

    if (manager == NULL || (manager->mode != GW_GPS_MODE_EXTERNAL_ACTIVE &&
        manager->mode != GW_GPS_MODE_LOCAL_DEGRADED)) {
        return GW_GPS_ACTION_NONE;
    }
    if (!manager->uart_initialized && reopen_uart(manager) != 0) {
        record_gps_unavailable(manager);
        return GwGpsUpdateMonitor(manager, 0, NULL);
    }
    error = get_status_for_gateway(manager, &status);
    action = GwGpsUpdateMonitor(manager, error == TK8710_PPS_OK,
        error == TK8710_PPS_OK ? &status : NULL);
#ifdef TK8710_GPS_TEST_HOOKS
    if (action == GW_GPS_ACTION_RESTART_EXIT && persist_recovery_marker() != 0) {
        manager->mode = GW_GPS_MODE_FAULT;
        action = GW_GPS_ACTION_FATAL_EXIT;
    }
#endif
    if (error == TK8710_PPS_OK && manager->mode == GW_GPS_MODE_EXTERNAL_ACTIVE) {
        printf("GPS/PPS monitor: STATE=%s PERIOD=%u GPS=%u PPS=%u FIX=%u "
               "RMC=%c ALIGN=%u HOLD=%u BAD=%u RESYNC=%u failures=%u\n",
               status.state, status.period, status.gps_online, status.pps_seen,
               status.fix_valid, status.rmc_status != '\0' ? status.rmc_status : '?',
               status.aligned, status.holdover, status.bad_period,
               status.resync_pending, manager->consecutive_abnormal);
    } else if (error == TK8710_PPS_OK &&
               manager->mode == GW_GPS_MODE_LOCAL_DEGRADED) {
        printf("GPS recovery monitor: GPS=%u PPS=%u FIX=%u RMC=%c recovered=%u/%u\n",
               status.gps_online, status.pps_seen, status.fix_valid,
               status.rmc_status != '\0' ? status.rmc_status : '?',
               manager->consecutive_recovered, manager->policy.consecutive_limit);
    }
    if (error != TK8710_PPS_OK) {
        fprintf(stderr, "GPS/PPS status query failed: %d\n", error);
        TK8710PpsClose(&manager->pps);
        manager->uart_initialized = 0;
    }
    if (action == GW_GPS_ACTION_FATAL_EXIT) {
        if (!manager->uart_initialized) {
            reopen_uart(manager);
        }
        GwGpsPrintDiagnostics(manager, "three consecutive runtime failures");
    }
    return action;
}

void GwGpsUseLocalWithoutRecovery(GwGpsManager* manager)
{
    if (manager == NULL) {
        return;
    }
    manager->mode = GW_GPS_MODE_LOCAL_STATIC;
    manager->consecutive_abnormal = 0;
    manager->consecutive_recovered = 0;
}

void GwGpsClose(GwGpsManager* manager)
{
    if (manager == NULL) {
        return;
    }
    if (manager->uart_initialized) {
        TK8710PpsClose(&manager->pps);
        manager->uart_initialized = 0;
    }
}

const char* GwGpsModeName(GwGpsMode mode)
{
    switch (mode) {
        case GW_GPS_MODE_ABSENT: return "ABSENT";
        case GW_GPS_MODE_SEARCHING: return "SEARCHING";
        case GW_GPS_MODE_LOCAL_DEGRADED: return "LOCAL_DEGRADED";
        case GW_GPS_MODE_EXTERNAL_READY: return "EXTERNAL_READY";
        case GW_GPS_MODE_EXTERNAL_ACTIVE: return "EXTERNAL_ACTIVE";
        case GW_GPS_MODE_LOCAL_STATIC: return "LOCAL_STATIC";
        case GW_GPS_MODE_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}
