#include "ipc_command_client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

static int wait_fd_ready(int fd, int timeout_ms, int write_ready)
{
    fd_set readfds;
    fd_set writefds;
    struct timeval tv;
    int ret;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    if (write_ready) {
        FD_SET(fd, &writefds);
    } else {
        FD_SET(fd, &readfds);
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(fd + 1,
                 write_ready ? NULL : &readfds,
                 write_ready ? &writefds : NULL,
                 NULL,
                 &tv);
    if (ret <= 0) {
        return -1;
    }

    return 0;
}

int ipc_client_send_command(const char *socket_path,
                            const char *command,
                            char *response,
                            int response_size,
                            int timeout_ms)
{
    int fd;
    struct sockaddr_un addr;
    char command_buf[IPC_CLIENT_MAX_COMMAND_LEN + 2];
    size_t command_len;
    ssize_t sent_len;
    ssize_t recv_len;

    if (socket_path == NULL || command == NULL || response == NULL || response_size <= 1) {
        return -1;
    }

    command_len = strlen(command);
    if (command_len == 0 || command_len > IPC_CLIENT_MAX_COMMAND_LEN) {
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    if (wait_fd_ready(fd, timeout_ms, 1) != 0) {
        close(fd);
        return -1;
    }

    snprintf(command_buf, sizeof(command_buf), "%s\n", command);
    sent_len = send(fd, command_buf, strlen(command_buf), 0);
    if (sent_len <= 0) {
        close(fd);
        return -1;
    }

    if (wait_fd_ready(fd, timeout_ms, 0) != 0) {
        close(fd);
        return -1;
    }

    recv_len = recv(fd, response, response_size - 1, 0);
    if (recv_len <= 0) {
        close(fd);
        return -1;
    }

    response[recv_len] = '\0';

    while (recv_len > 0 && (response[recv_len - 1] == '\n' || response[recv_len - 1] == '\r')) {
        response[recv_len - 1] = '\0';
        recv_len--;
    }

    close(fd);
    return 0;
}

int ipc_client_ping(const char *socket_path,
                    char *response,
                    int response_size,
                    int timeout_ms)
{
    return ipc_client_send_command(socket_path, "PING", response, response_size, timeout_ms);
}

int ipc_client_start_task(const char *socket_path,
                          char *response,
                          int response_size,
                          int timeout_ms)
{
    return ipc_client_send_command(socket_path, "START", response, response_size, timeout_ms);
}

int ipc_client_start_task_with_args(const char *socket_path,
                                    const char *args,
                                    char *response,
                                    int response_size,
                                    int timeout_ms)
{
    char command[IPC_CLIENT_MAX_COMMAND_LEN + 1];

    if (args == NULL || args[0] == '\0') {
        return ipc_client_start_task(socket_path, response, response_size, timeout_ms);
    }

    if (snprintf(command, sizeof(command), "START %s", args) >= (int)sizeof(command)) {
        return -1;
    }

    return ipc_client_send_command(socket_path, command, response, response_size, timeout_ms);
}
