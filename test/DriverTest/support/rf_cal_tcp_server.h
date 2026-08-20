#ifndef RF_CAL_TCP_SERVER_H
#define RF_CAL_TCP_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RF_CAL_DEFAULT_BIND_IP "127.0.0.1"
#define RF_CAL_DEFAULT_PORT    12879u

typedef int (*RfCalRegReadFn)(uint16_t addr, uint32_t* value, void* userData);
typedef int (*RfCalRegWriteFn)(uint16_t addr, uint32_t value, void* userData);
typedef int (*RfCalStatsFn)(char* response, size_t responseSize, void* userData);
typedef int (*RfCalLogFn)(char* response, size_t responseSize, void* userData);

typedef struct {
    const char* bindIp;
    uint16_t port;
    RfCalRegReadFn readReg;
    RfCalRegWriteFn writeReg;
    RfCalStatsFn getStats;
    RfCalLogFn getLog;
    void* userData;
    volatile int* running;
} RfCalTcpServerConfig;

int RfCalProcessCommand(const char* command, char* response, size_t responseSize,
                        const RfCalTcpServerConfig* config,
                        int* closeClient, int* shutdownServer);

int RfCalTcpServerRun(const RfCalTcpServerConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* RF_CAL_TCP_SERVER_H */
