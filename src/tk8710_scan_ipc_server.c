#include "tk8710_scan_ipc_server.h"

#include "ipc_command_server.h"
#include "tk8710_scan_service.h"
#include "trm/trm_api.h"

#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define SCAN_SOCKET_PATH "/tmp/data_collect.sock"
#ifndef SCAN_STATUS_FILE
#define SCAN_STATUS_FILE "/tmp/data_collect_status.json"
#endif
#ifndef SCAN_STATUS_TMP_FILE
#define SCAN_STATUS_TMP_FILE "/tmp/data_collect_status.json.tmp"
#endif
#define SCAN_COMMAND_BUFFER_LEN 256
#define SCAN_TASK_DIR "/userdata/mqtt_scan"

typedef struct {
    uint32_t start_freq;
    uint32_t end_freq;
    uint8_t sweep_mode;
    uint8_t rate_mode;
    char mode_text[16];
    char freq_start_text[32];
    char freq_stop_text[32];
    char request_id[49];
} ScanTaskParams;

typedef struct {
    pthread_mutex_t lock;
    int initialized;
    int task_running;
    int stopping;
    ScanTaskParams current_params;
} ScanIpcContext;

static ScanIpcContext g_scan_ipc_ctx;
static pthread_mutex_t g_status_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_config_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_task_done = PTHREAD_COND_INITIALIZER;
static int (*g_start_handler)(uint32_t, uint32_t, int);
static int (*g_restore_handler)(void);

void TK8710ScanSetHandlers(int (*start)(uint32_t, uint32_t, int), int (*restore)(void))
{
    g_start_handler = start;
    g_restore_handler = restore;
}
void TK8710ScanConfigLock(void) { pthread_mutex_lock(&g_config_lock); }
void TK8710ScanConfigUnlock(void) { pthread_mutex_unlock(&g_config_lock); }
int TK8710ScanTaskIsActive(void)
{
    if (!g_scan_ipc_ctx.initialized) return 0;
    pthread_mutex_lock(&g_scan_ipc_ctx.lock);
    int active = g_scan_ipc_ctx.task_running;
    pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    return active;
}

static int ValidRequestId(const char *id)
{
    if (!id || !*id || strlen(id) > 48) return 0;
    for (; *id; ++id) {
        if (!((*id >= 'a' && *id <= 'z') || (*id >= 'A' && *id <= 'Z') ||
              (*id >= '0' && *id <= '9') || *id == '-' || *id == '_')) return 0;
    }
    return 1;
}

static int CopyDurableFile(const char *source, const char *destination)
{
    char tmp[256], data[4096];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", destination);
    if (n < 0 || (size_t)n >= sizeof(tmp)) return -1;
    FILE *in = fopen(source, "rb");
    if (!in) return -1;
    FILE *out = fopen(tmp, "wb");
    if (!out) { fclose(in); return -1; }
    size_t size;
    int failed = 0;
    while ((size = fread(data, 1, sizeof(data), in)) > 0) {
        if (fwrite(data, 1, size, out) != size) { failed = 1; break; }
    }
    if (ferror(in)) failed = 1;
    fclose(in);
    if (fflush(out) || fsync(fileno(out))) failed = 1;
    if (fclose(out)) failed = 1;
    if (!failed && rename(tmp, destination)) failed = 1;
    if (failed) { remove(tmp); return -1; }
    int dir = open(SCAN_TASK_DIR, O_RDONLY | O_DIRECTORY);
    if (dir < 0) return -1;
    int ret = fsync(dir);
    close(dir);
    return ret;
}

static int WriteStatusFile(const char* state, const char* message, int result_ready)
{
    ScanTaskParams params;
    pthread_mutex_lock(&g_scan_ipc_ctx.lock);
    params = g_scan_ipc_ctx.current_params;
    pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    pthread_mutex_lock(&g_status_lock);
    FILE* fp = fopen(SCAN_STATUS_TMP_FILE, "w");
    if (fp == NULL) {
        pthread_mutex_unlock(&g_status_lock);
        return -1;
    }

    fprintf(fp,
            "{\n"
            "  \"state\": \"%s\",\n"
            "  \"message\": \"%s\",\n"
            "  \"request_id\": \"%s\",\n"
            "  \"start_freq_hz\": %u,\n"
            "  \"end_freq_hz\": %u,\n"
            "  \"mode\": \"%s\",\n"
            "  \"result_ready\": %s\n"
            "}\n",
            state,
            message,
            params.request_id, params.start_freq, params.end_freq, params.mode_text,
            result_ready ? "true" : "false");

    int failed = ferror(fp) || fflush(fp) != 0 || fsync(fileno(fp)) != 0;
    if (fclose(fp)) failed = 1;
    if (failed) {
        remove(SCAN_STATUS_TMP_FILE);
        pthread_mutex_unlock(&g_status_lock);
        return -1;
    }
    if (rename(SCAN_STATUS_TMP_FILE, SCAN_STATUS_FILE) != 0) {
        remove(SCAN_STATUS_TMP_FILE);
        pthread_mutex_unlock(&g_status_lock);
        return -1;
    }
    int ret = 0;
    if (params.request_id[0]) {
        char path[256];
        snprintf(path, sizeof(path), SCAN_TASK_DIR "/exec-%s.json", params.request_id);
        ret = CopyDurableFile(SCAN_STATUS_FILE, path);
    }
    pthread_mutex_unlock(&g_status_lock);
    return ret;
}

static int ParseFreqToHz(const char* text, uint32_t* freq_hz)
{
    double mhz;
    char* end_ptr;

    if (text == NULL || freq_hz == NULL || text[0] == '\0') {
        return -1;
    }

    mhz = strtod(text, &end_ptr);
    if (end_ptr == text || *end_ptr != 'M' || end_ptr[1] != '\0' ||
        !isfinite(mhz) || mhz <= 0.0 || mhz * 1000000.0 > UINT32_MAX - 500000.0) {
        return -1;
    }

    *freq_hz = (uint32_t)(mhz * 1000000.0 + 0.5);
    return 0;
}

static int SetModeParams(const char* mode_text, ScanTaskParams* params)
{
    if (mode_text == NULL || params == NULL) {
        return -1;
    }

    if (strcmp(mode_text, "62.5k") == 0) {
        params->sweep_mode = 0;
        params->rate_mode = TK8710_RATE_MODE_5;
    } else if (strcmp(mode_text, "125k") == 0) {
        params->sweep_mode = 1;
        params->rate_mode = TK8710_RATE_MODE_6;
    } else if (strcmp(mode_text, "250k") == 0) {
        params->sweep_mode = 2;
        params->rate_mode = TK8710_RATE_MODE_7;
    } else if (strcmp(mode_text, "500k") == 0) {
        params->sweep_mode = 3;
        params->rate_mode = TK8710_RATE_MODE_8;
    } else {
        return -1;
    }

    snprintf(params->mode_text, sizeof(params->mode_text), "%s", mode_text);
    return 0;
}

static void SetDefaultParams(ScanTaskParams* params)
{
    if (params == NULL) {
        return;
    }

    memset(params, 0, sizeof(*params));
    params->start_freq = 470500000;
    params->end_freq = 471300000;
    SetModeParams("125k", params);
    snprintf(params->freq_start_text, sizeof(params->freq_start_text), "%s", "470.5M");
    snprintf(params->freq_stop_text, sizeof(params->freq_stop_text), "%s", "471.3M");
}

static int ParseStartParams(const char* cmd, ScanTaskParams* params)
{
    char buffer[SCAN_COMMAND_BUFFER_LEN + 1];
    char* token;
    char* context;
    unsigned seen = 0;

    if (cmd == NULL || params == NULL) {
        return -1;
    }

    SetDefaultParams(params);
    if (strlen(cmd) > SCAN_COMMAND_BUFFER_LEN) return -1;
    if (strncmp(cmd, "START", 5) != 0) {
        return -1;
    }

    if (cmd[5] == '\0') {
        return 0;
    }

    if (cmd[5] != ' ') {
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%s", cmd + 6);
    token = strtok_r(buffer, " ", &context);
    while (token != NULL) {
        char* equal_pos = strchr(token, '=');
        const char* value;

        if (equal_pos == NULL || equal_pos == token || equal_pos[1] == '\0') {
            return -1;
        }

        *equal_pos = '\0';
        value = equal_pos + 1;

        if (strcmp(token, "request_id") == 0) {
            if (params->request_id[0] || !ValidRequestId(value)) return -1;
            strcpy(params->request_id, value);
        } else if (strcmp(token, "mode") == 0) {
            if (seen & 1) return -1;
            seen |= 1;
            if (SetModeParams(value, params) != 0) {
                return -1;
            }
        } else if (strcmp(token, "freq_start") == 0) {
            if (seen & 2) return -1;
            seen |= 2;
            if (ParseFreqToHz(value, &params->start_freq) != 0) {
                return -1;
            }
            snprintf(params->freq_start_text, sizeof(params->freq_start_text), "%s", value);
        } else if (strcmp(token, "freq_stop") == 0) {
            if (seen & 4) return -1;
            seen |= 4;
            if (ParseFreqToHz(value, &params->end_freq) != 0) {
                return -1;
            }
            snprintf(params->freq_stop_text, sizeof(params->freq_stop_text), "%s", value);
        } else if (strcmp(token, "value") == 0) {
            /* value 是 Web demo 保留参数，当前业务不使用。 */
        } else {
            return -1;
        }

        token = strtok_r(NULL, " ", &context);
    }

    if ((params->request_id[0] && seen != 7) || params->start_freq > params->end_freq) {
        return -1;
    }
    const uint32_t steps[] = {62500, 125000, 250000, 500000};
    if ((params->end_freq - params->start_freq) / steps[params->sweep_mode] + 1 > 4096)
        return -1;

    return 0;
}

static void* ScanTaskThread(void* arg)
{
    ScanIpcContext* ctx = (ScanIpcContext*)arg;
    ScanTaskParams params;
    TRM_SweepState sweep_state;
    int ret;

    pthread_mutex_lock(&ctx->lock);
    params = ctx->current_params;
    pthread_mutex_unlock(&ctx->lock);

    WriteStatusFile("running", "正在扫频", 0);

    TK8710ScanConfigLock();
    ret = (g_start_handler ? g_start_handler : TK8710ScanStart)(params.start_freq,
                          params.end_freq,
                          params.sweep_mode);
    int started = ret == 0;
    struct timespec begin, now;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    while (started && ret == TRM_OK) {
        usleep(200000);
        clock_gettime(CLOCK_MONOTONIC, &now);
        pthread_mutex_lock(&ctx->lock);
        int stopping = ctx->stopping;
        pthread_mutex_unlock(&ctx->lock);
        if (stopping || now.tv_sec - begin.tv_sec >= 540) {
            TRM_StopFrequencySweep();
            ret = -1;
            break;
        }
        memset(&sweep_state, 0, sizeof(sweep_state));
        ret = TRM_GetSweepState(&sweep_state);
        if (ret != TRM_OK || !sweep_state.sweep_active) break;
    }

    int snapshot = 0;
    if (ret == TRM_OK && params.request_id[0]) {
        char path[256];
        snprintf(path, sizeof(path), SCAN_TASK_DIR "/exec-%s.txt", params.request_id);
        snapshot = CopyDurableFile("SweepFreqResult/Result.txt", path);
    }
    int restored = g_restore_handler ? g_restore_handler() : 0;
    TK8710ScanConfigUnlock();

    if (restored) {
        WriteStatusFile("failed", "restore_failed", 0);
    } else if (ret != TRM_OK) {
        WriteStatusFile("failed", started ? "scan_failed_or_timeout" : "scan_start_failed", 0);
    } else {
        if (snapshot) WriteStatusFile("failed", "result_snapshot_failed", 0);
        else WriteStatusFile("done", "扫频完成", 1);
    }

    pthread_mutex_lock(&ctx->lock);
    ctx->task_running = 0;
    pthread_cond_broadcast(&g_task_done);
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

static ipc_cmd_result_t HandleScanCommand(const char* cmd, void* user_data)
{
    ScanIpcContext* ctx = (ScanIpcContext*)user_data;
    ScanTaskParams params;
    pthread_t worker;

    if (cmd == NULL || ctx == NULL) {
        return IPC_CMD_RESULT_ERROR;
    }

    if (strcmp(cmd, "PING") == 0) {
        return IPC_CMD_RESULT_OK;
    }

    if (strncmp(cmd, "START", 5) != 0) {
        return IPC_CMD_RESULT_ERROR;
    }

    if (ParseStartParams(cmd, &params) != 0) {
        return IPC_CMD_RESULT_ERROR;
    }

    pthread_mutex_lock(&ctx->lock);
    if (ctx->task_running || ctx->stopping) {
        pthread_mutex_unlock(&ctx->lock);
        return IPC_CMD_RESULT_BUSY;
    }

    ctx->task_running = 1;
    if (params.request_id[0]) {
        char path[256];
        snprintf(path, sizeof(path), SCAN_TASK_DIR "/exec-%s.json", params.request_id);
        if (access(path, F_OK) == 0) {
            ctx->task_running = 0;
            pthread_mutex_unlock(&ctx->lock);
            return IPC_CMD_RESULT_ERROR;
        }
    }
    ctx->current_params = params;
    pthread_mutex_unlock(&ctx->lock);

    if (params.request_id[0] && mkdir(SCAN_TASK_DIR, 0700) && errno != EEXIST) {
        pthread_mutex_lock(&ctx->lock);
        ctx->task_running = 0;
        pthread_mutex_unlock(&ctx->lock);
        return IPC_CMD_RESULT_ERROR;
    }
    if (WriteStatusFile("running", "task_accepted", 0)) {
        pthread_mutex_lock(&ctx->lock);
        ctx->task_running = 0;
        pthread_mutex_unlock(&ctx->lock);
        return IPC_CMD_RESULT_ERROR;
    }

    if (pthread_create(&worker, NULL, ScanTaskThread, ctx) != 0) {
        WriteStatusFile("failed", "worker_start_failed", 0);
        pthread_mutex_lock(&ctx->lock);
        ctx->task_running = 0;
        pthread_mutex_unlock(&ctx->lock);
        return IPC_CMD_RESULT_ERROR;
    }

    pthread_detach(worker);
    return IPC_CMD_RESULT_OK;
}

int TK8710ScanIpcServerStart(void)
{
    ipc_server_config_t config;

    if (!g_scan_ipc_ctx.initialized) {
        memset(&g_scan_ipc_ctx, 0, sizeof(g_scan_ipc_ctx));
        pthread_mutex_init(&g_scan_ipc_ctx.lock, NULL);
        g_scan_ipc_ctx.initialized = 1;
    }

    if (ipc_server_is_running()) return 0;
    g_scan_ipc_ctx.stopping = 0;
    WriteStatusFile("idle", "尚未执行扫频", 0);

    config.socket_path = SCAN_SOCKET_PATH;
    config.on_command = HandleScanCommand;
    config.user_data = &g_scan_ipc_ctx;

    return ipc_server_start(&config);
}

void TK8710ScanIpcServerStop(void)
{
    if (g_scan_ipc_ctx.initialized) {
        pthread_mutex_lock(&g_scan_ipc_ctx.lock);
        g_scan_ipc_ctx.stopping = 1;
        pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    }
    if (ipc_server_is_running()) {
        ipc_server_stop();
    }
    if (g_scan_ipc_ctx.initialized) {
        pthread_mutex_lock(&g_scan_ipc_ctx.lock);
        while (g_scan_ipc_ctx.task_running)
            pthread_cond_wait(&g_task_done, &g_scan_ipc_ctx.lock);
        pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    }
}

int TK8710ScanSubmit(uint32_t start, uint32_t end, int mode)
{
    static const char *modes[] = {"62.5k", "125k", "250k", "500k"};
    if (!g_scan_ipc_ctx.initialized || mode < 0 || mode > 3) return -1;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "START freq_start=%.6fM freq_stop=%.6fM mode=%s",
             start / 1000000.0, end / 1000000.0, modes[mode]);
    return HandleScanCommand(cmd, &g_scan_ipc_ctx);
}

int TK8710ScanIpcServerIsRunning(void)
{
    return ipc_server_is_running();
}

void TK8710ScanIpcNotifySweepRunning(void)
{
    if (!g_scan_ipc_ctx.initialized) return;
    pthread_mutex_lock(&g_scan_ipc_ctx.lock);
    int managed = g_scan_ipc_ctx.task_running;
    pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    if (managed) return;
    WriteStatusFile("running", "正在扫频", 0);
}

void TK8710ScanIpcNotifySweepDone(void)
{
    if (!g_scan_ipc_ctx.initialized) return;
    pthread_mutex_lock(&g_scan_ipc_ctx.lock);
    int managed = g_scan_ipc_ctx.task_running;
    pthread_mutex_unlock(&g_scan_ipc_ctx.lock);
    if (managed) return;
    WriteStatusFile("done", "扫频完成", 1);
}
