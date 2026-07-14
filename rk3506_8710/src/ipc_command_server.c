#if defined(PLATFORM_TMS570)
/* Unix socket IPC is not part of the TMS570 target build. */
#else

#include "ipc_command_server.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define IPC_SERVER_BACKLOG 4
#define IPC_SERVER_MAX_COMMAND_LEN 128
#define IPC_SERVER_MAX_RESPONSE_LEN 256

typedef struct {
    pthread_t thread;
    int server_fd;
    int running;
    char socket_path[108];
    ipc_cmd_callback_t on_command;
    void* user_data;
} IpcServerContext;

static IpcServerContext g_ipc_server;

static void TrimCommand(char* cmd)
{
    size_t len;

    if (cmd == NULL) {
        return;
    }

    len = strlen(cmd);
    while (len > 0 &&
           (cmd[len - 1] == '\n' || cmd[len - 1] == '\r' ||
            cmd[len - 1] == ' ' || cmd[len - 1] == '\t')) {
        cmd[len - 1] = '\0';
        len--;
    }
}

static void BuildResponse(ipc_cmd_result_t result, char* response, int response_size)
{
    if (response == NULL || response_size <= 0) {
        return;
    }

    if (result == IPC_CMD_RESULT_OK) {
        snprintf(response, response_size, "OK\n");
        return;
    }

    if (result == IPC_CMD_RESULT_BUSY) {
        snprintf(response, response_size, "BUSY\n");
        return;
    }

    snprintf(response, response_size, "ERROR unknown command or internal failure\n");
}

static void HandleClient(int client_fd)
{
    char command[IPC_SERVER_MAX_COMMAND_LEN + 1];
    char response[IPC_SERVER_MAX_RESPONSE_LEN];
    ssize_t recv_len;
    ipc_cmd_result_t result;

    memset(command, 0, sizeof(command));
    memset(response, 0, sizeof(response));

    recv_len = recv(client_fd, command, IPC_SERVER_MAX_COMMAND_LEN, 0);
    if (recv_len <= 0) {
        return;
    }

    command[recv_len] = '\0';
    TrimCommand(command);

    if (g_ipc_server.on_command == NULL) {
        snprintf(response, sizeof(response), "ERROR callback not registered\n");
    } else {
        result = g_ipc_server.on_command(command, g_ipc_server.user_data);
        BuildResponse(result, response, sizeof(response));
    }

    if (send(client_fd, response, strlen(response), 0) < 0) {
        return;
    }
}

static void* IpcServerThread(void* arg)
{
    IpcServerContext* ctx = (IpcServerContext*)arg;

    while (ctx->running) {
        int client_fd = accept(ctx->server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (!ctx->running) {
                break;
            }
            continue;
        }

        HandleClient(client_fd);
        close(client_fd);
    }

    return NULL;
}

int ipc_server_start(const ipc_server_config_t* config)
{
    struct sockaddr_un addr;
    size_t path_len;

    if (config == NULL || config->socket_path == NULL || config->on_command == NULL) {
        return -1;
    }

    path_len = strlen(config->socket_path);
    if (path_len == 0 || path_len >= sizeof(addr.sun_path)) {
        return -1;
    }

    if (g_ipc_server.running) {
        return -1;
    }

    memset(&g_ipc_server, 0, sizeof(g_ipc_server));
    g_ipc_server.server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_ipc_server.server_fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", config->socket_path);

    unlink(config->socket_path);

    if (bind(g_ipc_server.server_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(g_ipc_server.server_fd);
        g_ipc_server.server_fd = -1;
        return -1;
    }

    if (listen(g_ipc_server.server_fd, IPC_SERVER_BACKLOG) != 0) {
        unlink(config->socket_path);
        close(g_ipc_server.server_fd);
        g_ipc_server.server_fd = -1;
        return -1;
    }

    snprintf(g_ipc_server.socket_path, sizeof(g_ipc_server.socket_path), "%s", config->socket_path);
    g_ipc_server.on_command = config->on_command;
    g_ipc_server.user_data = config->user_data;
    g_ipc_server.running = 1;

    if (pthread_create(&g_ipc_server.thread, NULL, IpcServerThread, &g_ipc_server) != 0) {
        unlink(config->socket_path);
        close(g_ipc_server.server_fd);
        memset(&g_ipc_server, 0, sizeof(g_ipc_server));
        g_ipc_server.server_fd = -1;
        return -1;
    }

    return 0;
}

void ipc_server_stop(void)
{
    if (!g_ipc_server.running) {
        return;
    }

    g_ipc_server.running = 0;
    shutdown(g_ipc_server.server_fd, SHUT_RDWR);
    close(g_ipc_server.server_fd);
    g_ipc_server.server_fd = -1;
    pthread_join(g_ipc_server.thread, NULL);
    unlink(g_ipc_server.socket_path);
    memset(&g_ipc_server, 0, sizeof(g_ipc_server));
    g_ipc_server.server_fd = -1;
}

int ipc_server_is_running(void)
{
    return g_ipc_server.running;
}

#endif /* PLATFORM_TMS570 */
