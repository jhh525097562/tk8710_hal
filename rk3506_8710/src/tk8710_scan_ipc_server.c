#if defined(PLATFORM_TMS570)
/* Sweep IPC server is not part of the TMS570 target build. */
#else

#include "tk8710_scan_ipc_server.h"

#include "ipc_command_server.h"
#include "tk8710_scan_service.h"
#include "trm/trm_api.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCAN_SOCKET_PATH "/tmp/data_collect.sock"
#define SCAN_STATUS_FILE "/tmp/data_collect_status.json"
#define SCAN_STATUS_TMP_FILE "/tmp/data_collect_status.json.tmp"
#define SCAN_COMMAND_BUFFER_LEN 128

typedef struct {
    uint32_t start_freq;
    uint32_t end_freq;
    uint8_t sweep_mode;
    uint8_t rate_mode;
    char mode_text[16];
    char freq_start_text[32];
    char freq_stop_text[32];
} ScanTaskParams;

typedef struct {
    pthread_mutex_t lock;
    int initialized;
    int task_running;
    ScanTaskParams current_params;
} ScanIpcContext;

static ScanIpcContext g_scan_ipc_ctx;

static int WriteStatusFile(const char* state, const char* message, int result_ready)
{
    FILE* fp = fopen(SCAN_STATUS_TMP_FILE, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp,
            "{\n"
            "  \"state\": \"%s\",\n"
            "  \"message\": \"%s\",\n"
            "  \"result_ready\": %s\n"
            "}\n",
            state,
            message,
            result_ready ? "true" : "false");

    fclose(fp);
    if (rename(SCAN_STATUS_TMP_FILE, SCAN_STATUS_FILE) != 0) {
        remove(SCAN_STATUS_TMP_FILE);
        return -1;
    }

    return 0;
}

static int ParseFreqToHz(const char* text, uint32_t* freq_hz)
{
    double mhz;
    char* end_ptr;

    if (text == NULL || freq_hz == NULL || text[0] == '\0') {
        return -1;
    }

    mhz = strtod(text, &end_ptr);
    if (end_ptr == text || *end_ptr != 'M' || end_ptr[1] != '\0' || mhz <= 0.0) {
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

    if (cmd == NULL || params == NULL) {
        return -1;
    }

    SetDefaultParams(params);
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

        if (strcmp(token, "mode") == 0) {
            if (SetModeParams(value, params) != 0) {
                return -1;
            }
        } else if (strcmp(token, "freq_start") == 0) {
            if (ParseFreqToHz(value, &params->start_freq) != 0) {
                return -1;
            }
            snprintf(params->freq_start_text, sizeof(params->freq_start_text), "%s", value);
        } else if (strcmp(token, "freq_stop") == 0) {
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

    if (params->start_freq > params->end_freq) {
        return -1;
    }

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

    WriteStatusFile("running", "sweep running", 0);

    ret = TK8710ScanStart(params.start_freq,
                          params.end_freq,
                          params.sweep_mode);
    if (ret != 0) {
        WriteStatusFile("failed", "sweep start failed", 0);
        pthread_mutex_lock(&ctx->lock);
        ctx->task_running = 0;
        pthread_mutex_unlock(&ctx->lock);
        return NULL;
    }

    do {
        usleep(200000);
        memset(&sweep_state, 0, sizeof(sweep_state));
        ret = TRM_GetSweepState(&sweep_state);
    } while (ret == TRM_OK && sweep_state.sweep_active);

    if (ret != TRM_OK) {
        WriteStatusFile("failed", "sweep status query failed", 0);
    } else {
        WriteStatusFile("done", "sweep done", 1);
    }

    pthread_mutex_lock(&ctx->lock);
    ctx->task_running = 0;
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
    if (ctx->task_running) {
        pthread_mutex_unlock(&ctx->lock);
        return IPC_CMD_RESULT_BUSY;
    }

    ctx->task_running = 1;
    ctx->current_params = params;
    pthread_mutex_unlock(&ctx->lock);

    if (pthread_create(&worker, NULL, ScanTaskThread, ctx) != 0) {
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

    WriteStatusFile("idle", "sweep idle", 0);

    config.socket_path = SCAN_SOCKET_PATH;
    config.on_command = HandleScanCommand;
    config.user_data = &g_scan_ipc_ctx;

    return ipc_server_start(&config);
}

void TK8710ScanIpcServerStop(void)
{
    if (ipc_server_is_running()) {
        ipc_server_stop();
    }
}

int TK8710ScanIpcServerIsRunning(void)
{
    return ipc_server_is_running();
}

void TK8710ScanIpcNotifySweepRunning(void)
{
    WriteStatusFile("running", "sweep running", 0);
}

void TK8710ScanIpcNotifySweepDone(void)
{
    WriteStatusFile("done", "sweep done", 1);
}

#endif /* PLATFORM_TMS570 */
