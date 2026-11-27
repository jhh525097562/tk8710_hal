#ifndef IPC_COMMAND_CLIENT_H
#define IPC_COMMAND_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_CLIENT_DEFAULT_TIMEOUT_MS 1000
#define IPC_CLIENT_MAX_COMMAND_LEN 128
#define IPC_CLIENT_MAX_RESPONSE_LEN 256

int ipc_client_send_command(const char *socket_path,
                            const char *command,
                            char *response,
                            int response_size,
                            int timeout_ms);

int ipc_client_ping(const char *socket_path,
                    char *response,
                    int response_size,
                    int timeout_ms);

int ipc_client_start_task(const char *socket_path,
                          char *response,
                          int response_size,
                          int timeout_ms);

int ipc_client_start_task_with_args(const char *socket_path,
                                    const char *args,
                                    char *response,
                                    int response_size,
                                    int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
