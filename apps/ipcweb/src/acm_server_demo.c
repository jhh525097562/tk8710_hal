#include "ipc_command_server.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ACM_SOCKET_PATH "/tmp/acm.sock"
#define ACM_STATUS_FILE "/tmp/acm_status.json"
#define ACM_RESULT_FILE "/tmp/acm_result.json"
#define ACM_TASK_SECONDS 10
#define ACM_MAX_CHANNEL_COUNT 8

typedef struct {
    pthread_mutex_t lock;
    int task_running;
} acm_state_t;

static volatile int g_keep_running = 1;

static int write_status_file(const char *state, const char *message, int result_ready)
{
    FILE *fp;

    fp = fopen(ACM_STATUS_FILE, "w");
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
    return 0;
}

static int write_result_file(void)
{
    FILE *fp;
    int channel_count;
    int i;

    fp = fopen(ACM_RESULT_FILE, "w");
    if (fp == NULL) {
        return -1;
    }

    srand((unsigned int)time(NULL));
    channel_count = (rand() % ACM_MAX_CHANNEL_COUNT) + 1;

    fprintf(fp,
            "{\n"
            "  \"success\": true,\n"
            "  \"channels\": [\n");

    for (i = 1; i <= channel_count; ++i) {
        const char *status = (rand() % 5 == 0) ? "error" : "normal";

        fprintf(fp,
                "    { \"channel\": %d, \"status\": \"%s\" }%s\n",
                i,
                status,
                i == channel_count ? "" : ",");
    }

    fprintf(fp,
            "  ]\n"
            "}\n");

    fclose(fp);
    return 0;
}

static void *acm_task_thread(void *arg)
{
    acm_state_t *state = (acm_state_t *)arg;

    if (write_status_file("running", "ACM校准中", 0) != 0) {
        printf("[acm-server-demo] 状态文件写入失败: %s\n", ACM_STATUS_FILE);
    } else {
        printf("[acm-server-demo] 状态文件已更新为 running: %s\n", ACM_STATUS_FILE);
    }

    printf("[acm-server-demo] ACM 校准模拟开始，耗时 %d 秒...\n", ACM_TASK_SECONDS);
    sleep(ACM_TASK_SECONDS);

    if (write_result_file() != 0) {
        printf("[acm-server-demo] 结果文件写入失败: %s\n", ACM_RESULT_FILE);
        write_status_file("failed", "ACM校准失败", 0);
    } else {
        printf("[acm-server-demo] 结果文件已写入: %s\n", ACM_RESULT_FILE);
        write_status_file("done", "ACM校准完成", 1);
        printf("[acm-server-demo] 状态文件已更新为 done: %s\n", ACM_STATUS_FILE);
    }

    pthread_mutex_lock(&state->lock);
    state->task_running = 0;
    pthread_mutex_unlock(&state->lock);

    printf("[acm-server-demo] ACM 校准模拟完成\n");
    return NULL;
}

static ipc_cmd_result_t handle_acm_command(const char *cmd, void *user_data)
{
    acm_state_t *state = (acm_state_t *)user_data;
    pthread_t worker;

    if (strcmp(cmd, "PING") == 0) {
        printf("[acm-server-demo] 收到命令: PING\n");
        return IPC_CMD_RESULT_OK;
    }

    if (strcmp(cmd, "START acm") == 0 || strcmp(cmd, "start acm") == 0) {
        pthread_mutex_lock(&state->lock);
        if (state->task_running) {
            pthread_mutex_unlock(&state->lock);
            printf("[acm-server-demo] 收到命令: %s，当前忙碌\n", cmd);
            return IPC_CMD_RESULT_BUSY;
        }

        state->task_running = 1;
        pthread_mutex_unlock(&state->lock);

        if (pthread_create(&worker, NULL, acm_task_thread, state) != 0) {
            pthread_mutex_lock(&state->lock);
            state->task_running = 0;
            pthread_mutex_unlock(&state->lock);
            return IPC_CMD_RESULT_ERROR;
        }

        pthread_detach(worker);
        printf("[acm-server-demo] 收到命令: %s，ACM任务已启动\n", cmd);
        return IPC_CMD_RESULT_OK;
    }

    printf("[acm-server-demo] 收到未知命令: %s\n", cmd);
    return IPC_CMD_RESULT_ERROR;
}

static void handle_signal(int sig)
{
    (void)sig;
    g_keep_running = 0;
}

int main(void)
{
    ipc_server_config_t config;
    acm_state_t state;

    memset(&state, 0, sizeof(state));
    pthread_mutex_init(&state.lock, NULL);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (write_status_file("idle", "尚未执行ACM校准", 0) != 0) {
        printf("[acm-server-demo] 初始状态文件写入失败: %s\n", ACM_STATUS_FILE);
    }

    config.socket_path = ACM_SOCKET_PATH;
    config.on_command = handle_acm_command;
    config.user_data = &state;

    if (ipc_server_start(&config) != 0) {
        printf("[acm-server-demo] IPC 服务启动失败: %s\n", ACM_SOCKET_PATH);
        pthread_mutex_destroy(&state.lock);
        return 1;
    }

    printf("[acm-server-demo] IPC 服务已启动: %s\n", ACM_SOCKET_PATH);
    printf("[acm-server-demo] 支持命令: PING, START acm, start acm\n");
    printf("[acm-server-demo] 状态文件路径: %s\n", ACM_STATUS_FILE);
    printf("[acm-server-demo] 结果文件路径: %s\n", ACM_RESULT_FILE);

    while (g_keep_running) {
        sleep(1);
    }

    ipc_server_stop();
    pthread_mutex_destroy(&state.lock);
    printf("[acm-server-demo] IPC 服务已停止\n");
    return 0;
}
