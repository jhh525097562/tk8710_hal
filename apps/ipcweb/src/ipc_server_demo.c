#include "ipc_command_server.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define DEMO_SOCKET_PATH "/tmp/data_collect.sock"
#define DEMO_STATUS_FILE "/tmp/data_collect_status.json"
#define DEMO_RESULT_FILE "/tmp/data_collect_result.json"
#define DEMO_TASK_SECONDS 10
#define DEMO_COMMAND_BUFFER_LEN 128

typedef struct {
    int value;
    double mode_mhz;
    char mode_text[16];
    char freq_start[32];
    char freq_stop[32];
} demo_task_params_t;

typedef struct {
    pthread_mutex_t lock;
    int task_running;
    demo_task_params_t current_params;
} demo_state_t;

static volatile int g_keep_running = 1;

static int write_status_file(const char *state, const char *message, int result_ready)
{
    FILE *fp;

    fp = fopen(DEMO_STATUS_FILE, "w");
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

static int write_result_file(const demo_task_params_t *params)
{
    FILE *fp;
    int value;
    const char *freq_start;
    const char *freq_stop;
    double start_mhz;
    double stop_mhz;
    double mode_mhz;
    double current_mhz;
    int point_count;
    int i;

    if (params == NULL) {
        return -1;
    }

    value = params->value;
    mode_mhz = params->mode_mhz;
    freq_start = params->freq_start;
    freq_stop = params->freq_stop;
    start_mhz = strtod(freq_start, NULL);
    stop_mhz = strtod(freq_stop, NULL);

    if (start_mhz > stop_mhz) {
        double temp = start_mhz;
        start_mhz = stop_mhz;
        stop_mhz = temp;
        freq_start = params->freq_stop;
        freq_stop = params->freq_start;
    }

    if (mode_mhz <= 0.0) {
        mode_mhz = 0.125;
    }

    point_count = (int)((stop_mhz - start_mhz) / mode_mhz) + 1;
    if (point_count < 2) {
        point_count = 2;
    }

    fp = fopen(DEMO_RESULT_FILE, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp,
            "{\n"
            "  \"success\": true,\n"
            "  \"value\": %d,\n"
            "  \"mode\": \"%s\",\n"
            "  \"mode_mhz\": %.4f,\n"
            "  \"freq_start\": \"%s\",\n"
            "  \"freq_stop\": \"%s\",\n"
            "  \"items\": [\n",
            value,
            params->mode_text,
            mode_mhz,
            freq_start,
            freq_stop);

    srand((unsigned int)(time(NULL) + value));
    for (i = 0; i < point_count; ++i) {
        int dbm_value;

        current_mhz = start_mhz + mode_mhz * i;
        if (current_mhz > stop_mhz) {
            current_mhz = stop_mhz;
        }
        dbm_value = -130 + (rand() % 111);

        fprintf(fp,
                "    {\n"
                "      \"name\": \"%.1fM\",\n"
                "      \"value\": \"%ddbm\"\n"
                "    }%s\n",
                current_mhz,
                dbm_value,
                i == point_count - 1 ? "" : ",");
    }

    fprintf(fp,
            "  ]\n"
            "}\n");

    fclose(fp);
    return 0;
}

static void set_default_params(demo_task_params_t *params)
{
    if (params == NULL) {
        return;
    }

    params->value = 0;
    params->mode_mhz = 0.125;
    snprintf(params->mode_text, sizeof(params->mode_text), "%s", "125k");
    snprintf(params->freq_start, sizeof(params->freq_start), "%s", "470.5M");
    snprintf(params->freq_stop, sizeof(params->freq_stop), "%s", "471.3M");
}

static int is_valid_freq_value(const char *value)
{
    const char *p;
    int dot_count;
    int digit_count;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    p = value;
    dot_count = 0;
    digit_count = 0;

    while (*p != '\0') {
        if (*p >= '0' && *p <= '9') {
            digit_count++;
        } else if (*p == '.') {
            dot_count++;
            if (dot_count > 1) {
                return 0;
            }
        } else {
            break;
        }
        p++;
    }

    if (digit_count == 0) {
        return 0;
    }

    if (*p != 'M' || p[1] != '\0') {
        return 0;
    }

    return 1;
}

static int parse_mode_to_mhz(const char *mode_text, double *mode_mhz)
{
    if (mode_text == NULL || mode_mhz == NULL) {
        return -1;
    }

    if (strcmp(mode_text, "62.5k") == 0) {
        *mode_mhz = 0.0625;
        return 0;
    }

    if (strcmp(mode_text, "125k") == 0) {
        *mode_mhz = 0.125;
        return 0;
    }

    if (strcmp(mode_text, "250k") == 0) {
        *mode_mhz = 0.250;
        return 0;
    }

    if (strcmp(mode_text, "500k") == 0) {
        *mode_mhz = 0.500;
        return 0;
    }

    return -1;
}

static int parse_start_params(const char *cmd, demo_task_params_t *params)
{
    char buffer[DEMO_COMMAND_BUFFER_LEN + 1];
    char *token;
    char *context;

    if (cmd == NULL || params == NULL) {
        return -1;
    }

    set_default_params(params);

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
        char *equal_pos = strchr(token, '=');
        const char *param_value;

        if (equal_pos == NULL || equal_pos == token || equal_pos[1] == '\0') {
            return -1;
        }

        *equal_pos = '\0';
        param_value = equal_pos + 1;

        if (strcmp(token, "value") == 0) {
            long parsed_value;
            char *end_ptr;

            parsed_value = strtol(param_value, &end_ptr, 10);
            if (*end_ptr != '\0') {
                return -1;
            }
            params->value = (int)parsed_value;
        } else if (strcmp(token, "mode") == 0) {
            double mode_mhz;

            if (parse_mode_to_mhz(param_value, &mode_mhz) != 0) {
                return -1;
            }
            params->mode_mhz = mode_mhz;
            snprintf(params->mode_text, sizeof(params->mode_text), "%s", param_value);
        } else if (strcmp(token, "freq_start") == 0) {
            if (!is_valid_freq_value(param_value)) {
                return -1;
            }
            snprintf(params->freq_start, sizeof(params->freq_start), "%s", param_value);
        } else if (strcmp(token, "freq_stop") == 0) {
            if (!is_valid_freq_value(param_value)) {
                return -1;
            }
            snprintf(params->freq_stop, sizeof(params->freq_stop), "%s", param_value);
        } else {
            return -1;
        }

        token = strtok_r(NULL, " ", &context);
    }

    return 0;
}

static void *demo_task_thread(void *arg)
{
    demo_state_t *state = (demo_state_t *)arg;
    demo_task_params_t params;

    pthread_mutex_lock(&state->lock);
    params = state->current_params;
    pthread_mutex_unlock(&state->lock);

    if (write_status_file("running", "正在采集数据", 0) != 0) {
        printf("[server-demo] 状态文件写入失败: %s\n", DEMO_STATUS_FILE);
    } else {
        printf("[server-demo] 状态文件已更新为 running: %s\n", DEMO_STATUS_FILE);
    }

    printf("[server-demo] 模拟任务开始执行，duration=%d, value=%d, mode=%s, mode_mhz=%.4f, freq_start=%s, freq_stop=%s\n",
           DEMO_TASK_SECONDS,
           params.value,
           params.mode_text,
           params.mode_mhz,
           params.freq_start,
           params.freq_stop);
    sleep(DEMO_TASK_SECONDS);

    if (write_result_file(&params) != 0) {
        printf("[server-demo] 结果文件写入失败: %s\n", DEMO_RESULT_FILE);
    } else {
        printf("[server-demo] 结果文件已写入: %s\n", DEMO_RESULT_FILE);
    }

    if (write_status_file("done", "任务完成", 1) != 0) {
        printf("[server-demo] 状态文件写入失败: %s\n", DEMO_STATUS_FILE);
    } else {
        printf("[server-demo] 状态文件已更新为 done: %s\n", DEMO_STATUS_FILE);
    }

    pthread_mutex_lock(&state->lock);
    state->task_running = 0;
    pthread_mutex_unlock(&state->lock);

    printf("[server-demo] 模拟任务执行完成\n");
    return NULL;
}

static ipc_cmd_result_t handle_demo_command(const char *cmd, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;
    demo_task_params_t params;
    pthread_t worker;

    if (strcmp(cmd, "PING") == 0) {
        printf("[server-demo] 收到命令: PING\n");
        return IPC_CMD_RESULT_OK;
    }

    if (strncmp(cmd, "START", 5) == 0) {
        if (parse_start_params(cmd, &params) != 0) {
            printf("[server-demo] START 参数非法: %s\n", cmd);
            return IPC_CMD_RESULT_ERROR;
        }

        pthread_mutex_lock(&state->lock);
        if (state->task_running) {
            pthread_mutex_unlock(&state->lock);
            printf("[server-demo] 收到命令: START，当前忙碌\n");
            return IPC_CMD_RESULT_BUSY;
        }

        state->task_running = 1;
        state->current_params = params;
        pthread_mutex_unlock(&state->lock);

        if (pthread_create(&worker, NULL, demo_task_thread, state) != 0) {
            pthread_mutex_lock(&state->lock);
            state->task_running = 0;
            set_default_params(&state->current_params);
            pthread_mutex_unlock(&state->lock);
            return IPC_CMD_RESULT_ERROR;
        }

        pthread_detach(worker);
        printf("[server-demo] 收到命令: %s，任务已启动\n", cmd);
        return IPC_CMD_RESULT_OK;
    }

    printf("[server-demo] 收到未知命令: %s\n", cmd);
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
    demo_state_t state;

    memset(&state, 0, sizeof(state));
    pthread_mutex_init(&state.lock, NULL);
    set_default_params(&state.current_params);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (write_status_file("idle", "尚未执行任务", 0) != 0) {
        printf("[server-demo] 初始状态文件写入失败: %s\n", DEMO_STATUS_FILE);
    }

    config.socket_path = DEMO_SOCKET_PATH;
    config.on_command = handle_demo_command;
    config.user_data = &state;

    if (ipc_server_start(&config) != 0) {
        printf("[server-demo] IPC 服务启动失败: %s\n", DEMO_SOCKET_PATH);
        pthread_mutex_destroy(&state.lock);
        return 1;
    }

    printf("[server-demo] IPC 服务已启动: %s\n", DEMO_SOCKET_PATH);
    printf("[server-demo] 支持命令: PING, START\n");
    printf("[server-demo] 状态文件路径: %s\n", DEMO_STATUS_FILE);
    printf("[server-demo] 结果文件路径: %s\n", DEMO_RESULT_FILE);

    while (g_keep_running) {
        sleep(1);
    }

    ipc_server_stop();
    pthread_mutex_destroy(&state.lock);
    printf("[server-demo] IPC 服务已停止\n");
    return 0;
}
