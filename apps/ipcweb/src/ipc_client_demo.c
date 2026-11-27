#include "ipc_command_client.h"

#include <stdio.h>
#include <string.h>

#define DEMO_SOCKET_PATH "/tmp/data_collect.sock"

static void print_usage(const char *prog)
{
    printf("用法: %s ping\n", prog);
    printf("      %s start\n", prog);
    printf("      %s start key=value [key=value ...]\n", prog);
    printf("支持参数: value, mode, freq_start, freq_stop\n");
    printf("mode 仅支持: 62.5k, 125k, 250k, 500k\n");
    printf("示例: %s start value=123 mode=125k freq_start=470.5M freq_stop=471.3M\n", prog);
}

int main(int argc, char *argv[])
{
    char response[IPC_CLIENT_MAX_RESPONSE_LEN];
    int ret;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    memset(response, 0, sizeof(response));

    if (strcmp(argv[1], "ping") == 0) {
        ret = ipc_client_ping(DEMO_SOCKET_PATH,
                              response,
                              sizeof(response),
                              IPC_CLIENT_DEFAULT_TIMEOUT_MS);
    } else if (strcmp(argv[1], "start") == 0) {
        if (argc == 2) {
            ret = ipc_client_start_task(DEMO_SOCKET_PATH,
                                        response,
                                        sizeof(response),
                                        IPC_CLIENT_DEFAULT_TIMEOUT_MS);
        } else {
            char args[IPC_CLIENT_MAX_COMMAND_LEN + 1];
            int i;
            size_t used_len;

            memset(args, 0, sizeof(args));
            used_len = 0;

            for (i = 2; i < argc; ++i) {
                int written;

                written = snprintf(args + used_len,
                                   sizeof(args) - used_len,
                                   "%s%s",
                                   used_len == 0 ? "" : " ",
                                   argv[i]);
                if (written < 0 || (size_t)written >= sizeof(args) - used_len) {
                    printf("参数过长，超过命令长度限制\n");
                    return 1;
                }
                used_len += (size_t)written;
            }

            ret = ipc_client_start_task_with_args(DEMO_SOCKET_PATH,
                                                  args,
                                                  response,
                                                  sizeof(response),
                                                  IPC_CLIENT_DEFAULT_TIMEOUT_MS);
        }
    } else {
        print_usage(argv[0]);
        return 1;
    }

    if (ret != 0) {
        printf("请求失败，无法连接到 %s\n", DEMO_SOCKET_PATH);
        return 1;
    }

    printf("服务端响应: %s\n", response);
    return 0;
}
