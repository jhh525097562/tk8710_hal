#ifdef _WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#endif

#include "rf_cal_tcp_server.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#define RF_CAL_COMMAND_MAX_LEN  512u
#define RF_CAL_RESPONSE_MAX_LEN 1024u
#define RF_CAL_RECV_BUFFER_LEN  512u

static int rf_cal_text_equal(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int rf_cal_parse_u32(const char* text, uint32_t* value)
{
    char* end;
    unsigned long long parsed;

    if (text == NULL || value == NULL || *text == '\0' || *text == '-') {
        return -1;
    }

    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static size_t rf_cal_tokenize(char* command, char** tokens, size_t maxTokens)
{
    size_t count = 0;
    char* current = command;

    while (*current != '\0') {
        while (isspace((unsigned char)*current)) {
            current++;
        }
        if (*current == '\0') {
            break;
        }
        if (count >= maxTokens) {
            return maxTokens + 1;
        }

        tokens[count++] = current;
        while (*current != '\0' && !isspace((unsigned char)*current)) {
            current++;
        }
        if (*current != '\0') {
            *current++ = '\0';
        }
    }

    return count;
}

int RfCalProcessCommand(const char* command, char* response, size_t responseSize,
                        const RfCalTcpServerConfig* config,
                        int* closeClient, int* shutdownServer)
{
    char commandCopy[RF_CAL_COMMAND_MAX_LEN];
    char* tokens[4];
    size_t tokenCount;
    uint32_t addrValue;
    uint32_t writeValue;
    uint32_t readValue;
    int ret;

    if (response == NULL || responseSize == 0 || config == NULL ||
        closeClient == NULL || shutdownServer == NULL) {
        return -1;
    }

    *closeClient = 0;
    *shutdownServer = 0;

    if (command == NULL || strlen(command) >= sizeof(commandCopy)) {
        snprintf(response, responseSize, "ERR COMMAND_TOO_LONG\n");
        return 0;
    }

    memcpy(commandCopy, command, strlen(command) + 1);
    tokenCount = rf_cal_tokenize(commandCopy, tokens, 4);
    if (tokenCount == 0) {
        snprintf(response, responseSize, "ERR EMPTY_COMMAND\n");
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "PING") && tokenCount == 1) {
        snprintf(response, responseSize, "OK PONG\n");
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "QUIT") && tokenCount == 1) {
        *closeClient = 1;
        snprintf(response, responseSize, "OK BYE\n");
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "SHUTDOWN") && tokenCount == 1) {
        *closeClient = 1;
        *shutdownServer = 1;
        snprintf(response, responseSize, "OK SHUTDOWN\n");
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "STATS") && tokenCount == 1) {
        if (config->getStats == NULL) {
            snprintf(response, responseSize, "ERR STATS_UNAVAILABLE\n");
            return 0;
        }

        ret = config->getStats(response, responseSize, config->userData);
        if (ret != 0) {
            snprintf(response, responseSize, "ERR STATS_FAILED %d\n", ret);
        }
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "LOG") && tokenCount == 1) {
        if (config->getLog != NULL) {
            ret = config->getLog(response, responseSize, config->userData);
            if (ret != 0) {
                snprintf(response, responseSize, "ERR LOG_FAILED %d\n", ret);
            }
            return 0;
        }
        if (config->getStats != NULL) {
            ret = config->getStats(response, responseSize, config->userData);
            if (ret != 0) {
                snprintf(response, responseSize, "ERR STATS_FAILED %d\n", ret);
            }
            return 0;
        }

        snprintf(response, responseSize, "ERR LOG_UNAVAILABLE\n");
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "READ")) {
        if (tokenCount != 2 || rf_cal_parse_u32(tokens[1], &addrValue) != 0 ||
            addrValue > UINT16_MAX) {
            snprintf(response, responseSize, "ERR INVALID_ADDRESS\n");
            return 0;
        }
        if (config->readReg == NULL) {
            snprintf(response, responseSize, "ERR READ_UNAVAILABLE\n");
            return 0;
        }

        ret = config->readReg((uint16_t)addrValue, &readValue, config->userData);
        if (ret != 0) {
            snprintf(response, responseSize, "ERR READ_FAILED %d\n", ret);
            return 0;
        }
        snprintf(response, responseSize, "OK READ 0x%04X 0x%08X\n",
                 (unsigned int)addrValue, (unsigned int)readValue);
        return 0;
    }

    if (rf_cal_text_equal(tokens[0], "WRITE")) {
        if (tokenCount != 3 || rf_cal_parse_u32(tokens[1], &addrValue) != 0 ||
            addrValue > UINT16_MAX) {
            snprintf(response, responseSize, "ERR INVALID_ADDRESS\n");
            return 0;
        }
        if (rf_cal_parse_u32(tokens[2], &writeValue) != 0) {
            snprintf(response, responseSize, "ERR INVALID_VALUE\n");
            return 0;
        }
        if (config->writeReg == NULL || config->readReg == NULL) {
            snprintf(response, responseSize, "ERR WRITE_UNAVAILABLE\n");
            return 0;
        }

        ret = config->writeReg((uint16_t)addrValue, writeValue, config->userData);
        if (ret != 0) {
            snprintf(response, responseSize, "ERR WRITE_FAILED %d\n", ret);
            return 0;
        }

        ret = config->readReg((uint16_t)addrValue, &readValue, config->userData);
        if (ret != 0) {
            snprintf(response, responseSize, "ERR READBACK_FAILED %d\n", ret);
            return 0;
        }
        if (readValue != writeValue) {
            snprintf(response, responseSize,
                     "ERR READBACK_MISMATCH 0x%08X 0x%08X\n",
                     (unsigned int)writeValue, (unsigned int)readValue);
            return 0;
        }

        snprintf(response, responseSize,
                 "OK WRITE 0x%04X 0x%08X READBACK 0x%08X\n",
                 (unsigned int)addrValue, (unsigned int)writeValue,
                 (unsigned int)readValue);
        return 0;
    }

    snprintf(response, responseSize, "ERR INVALID_COMMAND\n");
    return 0;
}

#ifdef _WIN32
static int rf_cal_send_all(SOCKET socketFd, const char* data)
{
    size_t total = 0;
    size_t length = strlen(data);

    while (total < length) {
        int sent = send(socketFd, data + total, (int)(length - total), 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return -1;
        }
        total += (size_t)sent;
    }
    return 0;
}

static int rf_cal_run_client(SOCKET clientSocket, const RfCalTcpServerConfig* config)
{
    char recvBuffer[RF_CAL_RECV_BUFFER_LEN];
    char lineBuffer[RF_CAL_COMMAND_MAX_LEN];
    char response[RF_CAL_RESPONSE_MAX_LEN];
    size_t lineLength = 0;
    int closeClient = 0;
    int shutdownServer = 0;

    if (rf_cal_send_all(clientSocket, "OK RF_CAL_SERVER 1\n") != 0) {
        return 0;
    }

    while (!closeClient && *(config->running)) {
        int received = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);
        if (received <= 0) {
            break;
        }

        for (int i = 0; i < received; i++) {
            char ch = recvBuffer[i];
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                lineBuffer[lineLength] = '\0';
                RfCalProcessCommand(lineBuffer, response, sizeof(response), config,
                                    &closeClient, &shutdownServer);
                if (rf_cal_send_all(clientSocket, response) != 0) {
                    closeClient = 1;
                }
                lineLength = 0;
                if (shutdownServer) {
                    *(config->running) = 0;
                }
                if (closeClient) {
                    break;
                }
                continue;
            }

            if (lineLength + 1 < sizeof(lineBuffer)) {
                lineBuffer[lineLength++] = ch;
            } else {
                lineLength = 0;
                if (rf_cal_send_all(clientSocket, "ERR COMMAND_TOO_LONG\n") != 0) {
                    closeClient = 1;
                }
            }
        }
    }

    return shutdownServer;
}
#endif

int RfCalTcpServerRun(const RfCalTcpServerConfig* config)
{
#ifdef _WIN32
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    const char* bindIp;
    uint16_t port;
    int reuseAddr = 1;
    int result = -1;

    if (config == NULL || config->running == NULL ||
        config->readReg == NULL || config->writeReg == NULL) {
        return -1;
    }

    bindIp = (config->bindIp != NULL && config->bindIp[0] != '\0') ?
        config->bindIp : RF_CAL_DEFAULT_BIND_IP;
    port = config->port != 0 ? config->port : RF_CAL_DEFAULT_PORT;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "RF calibration TCP: WSAStartup failed\n");
        return -1;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        fprintf(stderr, "RF calibration TCP: socket failed: %d\n", WSAGetLastError());
        goto cleanup;
    }

    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuseAddr, sizeof(reuseAddr));

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, bindIp, &serverAddr.sin_addr) != 1) {
        fprintf(stderr, "RF calibration TCP: invalid bind IP: %s\n", bindIp);
        goto cleanup;
    }

    if (bind(listenSocket, (const struct sockaddr*)&serverAddr,
             sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "RF calibration TCP: bind %s:%u failed: %d\n",
                bindIp, port, WSAGetLastError());
        goto cleanup;
    }

    if (listen(listenSocket, 1) == SOCKET_ERROR) {
        fprintf(stderr, "RF calibration TCP: listen failed: %d\n", WSAGetLastError());
        goto cleanup;
    }

    printf("RF calibration TCP server listening on %s:%u\n", bindIp, port);
    while (*(config->running)) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (*(config->running)) {
                fprintf(stderr, "RF calibration TCP: accept failed: %d\n",
                        WSAGetLastError());
            }
            break;
        }

        printf("RF calibration TCP client connected\n");
        rf_cal_run_client(clientSocket, config);
        closesocket(clientSocket);
        printf("RF calibration TCP client disconnected\n");
    }

    result = 0;

cleanup:
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
    WSACleanup();
    return result;
#else
    (void)config;
    fprintf(stderr, "RF calibration TCP server is only supported on Windows\n");
    return -1;
#endif
}
