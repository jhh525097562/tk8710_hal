#include "../../src/ipc_command_server.c"
#include <assert.h>

static int calls;
static ipc_cmd_result_t TestCommand(const char *command, void *data)
{
    (void)data;
    assert(!strcmp(command, "PING"));
    calls++;
    return IPC_CMD_RESULT_OK;
}
static void *Serve(void *arg)
{
    int fd = *(int *)arg;
    HandleClient(fd);
    close(fd);
    return NULL;
}
int main(void)
{
    int pair[2]; pthread_t worker;
    g_ipc_server.on_command = TestCommand;
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    assert(!pthread_create(&worker, NULL, Serve, &pair[1]));
    assert(send(pair[0], "PI", 2, MSG_NOSIGNAL) == 2);
    usleep(10000);
    assert(send(pair[0], "NG\n", 3, MSG_NOSIGNAL) == 3);
    char response[32] = {0};
    assert(recv(pair[0], response, sizeof(response)-1, 0) > 0);
    assert(!strcmp(response, "OK\n"));
    close(pair[0]); pthread_join(worker, NULL); assert(calls == 1);
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    assert(!pthread_create(&worker, NULL, Serve, &pair[1]));
    assert(send(pair[0], "PING\0bad\n", 9, MSG_NOSIGNAL) == 9);
    assert(recv(pair[0], response, sizeof(response), 0) == 0);
    close(pair[0]); pthread_join(worker, NULL); assert(calls == 1);
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    assert(!pthread_create(&worker, NULL, Serve, &pair[1]));
    assert(recv(pair[0], response, sizeof(response), 0) == 0);
    close(pair[0]); pthread_join(worker, NULL); assert(calls == 1);
    puts("PASS scan IPC transport: fragmented command, embedded NUL rejection, idle-client deadline");
    return 0;
}
