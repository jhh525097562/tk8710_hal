#ifdef _WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#endif

#include "rf_cal_tcp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

typedef struct {
    uint16_t addr;
    uint32_t value;
    int forceMismatch;
} FakeRegister;

static int fake_read(uint16_t addr, uint32_t* value, void* userData)
{
    FakeRegister* reg = (FakeRegister*)userData;

    if (reg == NULL || value == NULL || addr != reg->addr) {
        return 7;
    }
    *value = reg->forceMismatch ? (reg->value ^ 1u) : reg->value;
    return 0;
}

static int fake_write(uint16_t addr, uint32_t value, void* userData)
{
    FakeRegister* reg = (FakeRegister*)userData;

    if (reg == NULL || addr != reg->addr) {
        return 8;
    }
    reg->value = value;
    return 0;
}

static int fake_stats(char* response, size_t responseSize, void* userData)
{
    (void)userData;
    snprintf(response, responseSize,
             "OK STATS mode=2 total=100 lost=3 period=20 period_lost=1 "
             "loss=3.00 last_user=7 last_rssi=-55 last_snr=18 last_freq=125\n");
    return 0;
}

static int fake_log(char* response, size_t responseSize, void* userData)
{
    (void)userData;
    snprintf(response, responseSize,
             "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 total=100 lost=3\n");
    return 0;
}

static void expect_response(const char* command, const char* expected,
                            RfCalTcpServerConfig* config)
{
    char response[512];
    int closeClient;
    int shutdownServer;

    if (RfCalProcessCommand(command, response, sizeof(response), config,
                            &closeClient, &shutdownServer) != 0 ||
        strcmp(response, expected) != 0) {
        fprintf(stderr, "Command test failed: [%s]\nExpected: [%s]Actual:   [%s]\n",
                command, expected, response);
        exit(1);
    }
}

static void run_protocol_tests(void)
{
    volatile int running = 1;
    FakeRegister reg = {.addr = 0x08C8, .value = 0};
    RfCalTcpServerConfig config = {
        .bindIp = RF_CAL_DEFAULT_BIND_IP,
        .port = RF_CAL_DEFAULT_PORT,
        .readReg = fake_read,
        .writeReg = fake_write,
        .getStats = fake_stats,
        .getLog = fake_log,
        .userData = &reg,
        .running = &running
    };

    expect_response("PING", "OK PONG\n", &config);
    expect_response("READ 0x08C8", "OK READ 0x08C8 0x00000000\n", &config);
    expect_response("WRITE 0x08C8 0x04000000",
                    "OK WRITE 0x08C8 0x04000000 READBACK 0x04000000\n",
                    &config);
    expect_response("READ 2248", "OK READ 0x08C8 0x04000000\n", &config);
    expect_response("STATS",
                    "OK STATS mode=2 total=100 lost=3 period=20 period_lost=1 "
                    "loss=3.00 last_user=7 last_rssi=-55 last_snr=18 last_freq=125\n",
                    &config);
    expect_response("LOG",
                    "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 total=100 lost=3\n",
                    &config);
    expect_response("READ 0x10000", "ERR INVALID_ADDRESS\n", &config);
    expect_response("WRITE 0x08C8 xyz", "ERR INVALID_VALUE\n", &config);
    expect_response("UNKNOWN", "ERR INVALID_COMMAND\n", &config);

    config.getStats = NULL;
    expect_response("STATS", "ERR STATS_UNAVAILABLE\n", &config);
    config.getStats = fake_stats;
    config.getLog = NULL;
    expect_response("LOG",
                    "OK STATS mode=2 total=100 lost=3 period=20 period_lost=1 "
                    "loss=3.00 last_user=7 last_rssi=-55 last_snr=18 last_freq=125\n",
                    &config);
    config.getStats = NULL;
    expect_response("LOG", "ERR LOG_UNAVAILABLE\n", &config);
    config.getStats = fake_stats;
    config.getLog = fake_log;

    reg.forceMismatch = 1;
    expect_response("WRITE 0x08C8 0x12345678",
                    "ERR READBACK_MISMATCH 0x12345678 0x12345679\n", &config);
    reg.forceMismatch = 0;

    if (strcmp(RF_CAL_DEFAULT_BIND_IP, "127.0.0.1") != 0 ||
        RF_CAL_DEFAULT_PORT != 12879u) {
        fprintf(stderr, "Default endpoint test failed\n");
        exit(1);
    }
}

#ifdef _WIN32
typedef struct {
    RfCalTcpServerConfig config;
    int result;
} ServerThreadContext;

static DWORD WINAPI server_thread(LPVOID parameter)
{
    ServerThreadContext* context = (ServerThreadContext*)parameter;
    context->result = RfCalTcpServerRun(&context->config);
    return 0;
}

static int recv_line(SOCKET socketFd, char* buffer, size_t bufferSize)
{
    size_t length = 0;

    while (length + 1 < bufferSize) {
        char ch;
        int received = recv(socketFd, &ch, 1, 0);
        if (received != 1) {
            return -1;
        }
        if (ch == '\r') {
            continue;
        }
        buffer[length++] = ch;
        if (ch == '\n') {
            buffer[length] = '\0';
            return 0;
        }
    }
    return -1;
}

static void send_and_expect(SOCKET socketFd, const char* command, const char* expected)
{
    char response[512];

    if (send(socketFd, command, (int)strlen(command), 0) != (int)strlen(command) ||
        recv_line(socketFd, response, sizeof(response)) != 0 ||
        strcmp(response, expected) != 0) {
        fprintf(stderr, "TCP integration command failed: %s", command);
        exit(1);
    }
}

static void run_tcp_integration_test(void)
{
    WSADATA wsaData;
    volatile int running = 1;
    FakeRegister reg = {.addr = 0x08C8, .value = 0};
    ServerThreadContext context = {
        .config = {
            .bindIp = RF_CAL_DEFAULT_BIND_IP,
            .port = RF_CAL_DEFAULT_PORT + 1,
            .readReg = fake_read,
            .writeReg = fake_write,
            .getStats = fake_stats,
            .getLog = fake_log,
            .userData = &reg,
            .running = &running
        },
        .result = -1
    };
    HANDLE threadHandle;
    SOCKET clientSocket = INVALID_SOCKET;
    struct sockaddr_in address;
    char greeting[512];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Test WSAStartup failed\n");
        exit(1);
    }

    threadHandle = CreateThread(NULL, 0, server_thread, &context, 0, NULL);
    if (threadHandle == NULL) {
        fprintf(stderr, "Failed to create TCP test server thread\n");
        exit(1);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(context.config.port);
    inet_pton(AF_INET, context.config.bindIp, &address.sin_addr);

    for (int attempt = 0; attempt < 50; attempt++) {
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket != INVALID_SOCKET &&
            connect(clientSocket, (const struct sockaddr*)&address,
                    sizeof(address)) == 0) {
            break;
        }
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
        Sleep(20);
    }

    if (clientSocket == INVALID_SOCKET ||
        recv_line(clientSocket, greeting, sizeof(greeting)) != 0 ||
        strcmp(greeting, "OK RF_CAL_SERVER 1\n") != 0) {
        fprintf(stderr, "Failed to connect to local TCP test server\n");
        exit(1);
    }

    send_and_expect(clientSocket, "PING\n", "OK PONG\n");
    send_and_expect(clientSocket, "WRITE 0x08C8 0x04000000\n",
                    "OK WRITE 0x08C8 0x04000000 READBACK 0x04000000\n");
    send_and_expect(clientSocket, "READ 0x08C8\n",
                    "OK READ 0x08C8 0x04000000\n");
    send_and_expect(clientSocket, "STATS\n",
                    "OK STATS mode=2 total=100 lost=3 period=20 period_lost=1 "
                    "loss=3.00 last_user=7 last_rssi=-55 last_snr=18 last_freq=125\n");
    send_and_expect(clientSocket, "LOG\n",
                    "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 total=100 lost=3\n");
    send_and_expect(clientSocket, "SHUTDOWN\n", "OK SHUTDOWN\n");

    closesocket(clientSocket);
    if (WaitForSingleObject(threadHandle, 3000) != WAIT_OBJECT_0 ||
        context.result != 0) {
        fprintf(stderr, "TCP test server did not shut down cleanly\n");
        exit(1);
    }
    CloseHandle(threadHandle);
    WSACleanup();
}
#endif

int main(void)
{
    run_protocol_tests();
#ifdef _WIN32
    run_tcp_integration_test();
#endif
    printf("RF calibration TCP tests passed\n");
    return 0;
}
