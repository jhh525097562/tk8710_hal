#ifndef IPC_COMMAND_SERVER_H
#define IPC_COMMAND_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IPC_CMD_RESULT_OK = 0,
    IPC_CMD_RESULT_BUSY = 1,
    IPC_CMD_RESULT_ERROR = 2,
} ipc_cmd_result_t;

typedef ipc_cmd_result_t (*ipc_cmd_callback_t)(const char* cmd, void* user_data);

typedef struct {
    const char* socket_path;
    ipc_cmd_callback_t on_command;
    void* user_data;
} ipc_server_config_t;

int ipc_server_start(const ipc_server_config_t* config);
void ipc_server_stop(void);
int ipc_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* IPC_COMMAND_SERVER_H */
