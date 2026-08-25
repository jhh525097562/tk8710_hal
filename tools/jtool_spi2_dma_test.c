#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "jtool.h"

#define SPI_PAGE_LEN 128U
#define DATA_FRAME_LEN 512U

typedef char *(*DevicesScanFn)(int, int *);
typedef void *(*DevOpenFn)(int, char *, int);
typedef BOOL (*DevCloseFn)(void *);
typedef ErrorType (*SPIWriteReadFn)(void *, SPICK_TYPE, SPIFIRSTBIT_TYPE,
                                    uint32_t, uint8_t *, uint8_t *);
typedef ErrorType (*JSPISetVioFn)(void *, uint8_t);
typedef ErrorType (*JSPISetSpeedFn)(void *, uint8_t);

static DevicesScanFn pDevicesScan;
static DevOpenFn pDevOpen;
static DevCloseFn pDevClose;
static SPIWriteReadFn pSPIWriteRead;
static JSPISetVioFn pJSPISetVio;
static JSPISetSpeedFn pJSPISetSpeed;

static int LoadApi(void)
{
    HMODULE dll = LoadLibraryA("jtool.dll");

    if (dll == NULL) {
        fprintf(stderr, "ERROR: LoadLibrary(jtool.dll) failed: %lu\n",
                (unsigned long)GetLastError());
        return -1;
    }
    pDevicesScan = (DevicesScanFn)GetProcAddress(dll, "DevicesScan");
    pDevOpen = (DevOpenFn)GetProcAddress(dll, "DevOpen");
    pDevClose = (DevCloseFn)GetProcAddress(dll, "DevClose");
    pSPIWriteRead = (SPIWriteReadFn)GetProcAddress(dll, "SPIWriteRead");
    pJSPISetVio = (JSPISetVioFn)GetProcAddress(dll, "JSPISetVio");
    pJSPISetSpeed = (JSPISetSpeedFn)GetProcAddress(dll, "JSPISetSpeed");
    if ((pDevicesScan == NULL) || (pDevOpen == NULL) || (pDevClose == NULL) ||
        (pSPIWriteRead == NULL) || (pJSPISetVio == NULL) ||
        (pJSPISetSpeed == NULL)) {
        fprintf(stderr, "ERROR: required JTool symbol missing\n");
        return -1;
    }
    return 0;
}

static void PrintBytes(const char *name, const uint8_t *data, uint32_t length)
{
    uint32_t i;

    printf("%s", name);
    for (i = 0U; i < length; i++) {
        if ((i % 16U) == 0U) {
            printf("\n  %04lX:", (unsigned long)i);
        }
        printf(" %02X", data[i]);
    }
    printf("\n");
}

static int SaveBytes(const char *path, const uint8_t *data, uint32_t length)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        return -1;
    }
    if (fwrite(data, 1U, length, file) != length) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

static void *OpenDevice(const char *serial)
{
    void *device = pDevOpen(dev_spi, (char *)serial, 0);

    if (device == NULL) {
        fprintf(stderr, "ERROR: DevOpen(%s) failed\n", serial);
        return NULL;
    }
    if (pJSPISetVio(device, 33U) != ErrNone) {
        fprintf(stderr, "ERROR: JSPISetVio(%s, 3.3V) failed\n", serial);
        pDevClose(device);
        return NULL;
    }
    if (pJSPISetSpeed(device, 0U) != ErrNone) {
        fprintf(stderr, "ERROR: JSPISetSpeed(%s, 1MHz) failed\n", serial);
        pDevClose(device);
        return NULL;
    }
    return device;
}

static uint8_t Sum8(const uint8_t *data, uint32_t first, uint32_t last)
{
    uint32_t i;
    uint8_t sum = 0U;

    for (i = first; i <= last; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static int ProbeTelemetry(const char *serial, SPICK_TYPE mode)
{
    void *device = OpenDevice(serial);
    uint8_t tx[SPI_PAGE_LEN];
    uint8_t page[SPI_PAGE_LEN];
    uint8_t page0[SPI_PAGE_LEN];
    uint8_t telemetry[144U];
    unsigned int attempt;
    int havePage0 = 0;
    int sawHeader = 0;
    int found = 0;

    if (device == NULL) {
        return -1;
    }
    memset(tx, 0, sizeof(tx));
    printf("PROBE serial=%s mode=%d\n", serial, (int)mode);
    for (attempt = 0U; attempt < 32U; attempt++) {
        ErrorType error = pSPIWriteRead(device, mode, ENDIAN_MSB,
                                        SPI_PAGE_LEN, tx, page);
        if (error != ErrNone) {
            printf("  attempt=%u error=0x%X\n", attempt, (unsigned int)error);
            continue;
        }
        if (havePage0 == 0) {
            if ((page[0] == 0xEBU) && (page[1] == 0x90U)) {
                sawHeader = 1;
                memcpy(page0, page, sizeof(page0));
                havePage0 = 1;
            }
        } else {
            memcpy(telemetry, page0, sizeof(page0));
            memcpy(&telemetry[128], page, 16U);
            havePage0 = 0;
            if (Sum8(telemetry, 2U, 142U) == telemetry[143]) {
                printf("  telemetry=PASS attempt=%u uptime=%lu\n", attempt,
                       (unsigned long)(((uint32_t)telemetry[32] << 24U) |
                                       ((uint32_t)telemetry[33] << 16U) |
                                       ((uint32_t)telemetry[34] << 8U) |
                                       telemetry[35]));
                found = 1;
                break;
            }
        }
    }
    if (found == 0) {
        printf("  telemetry=%s last_rx_first16:",
               sawHeader ? "HEADER_SEEN" : "NOT_FOUND");
        for (attempt = 0U; attempt < 16U; attempt++) {
            printf(" %02X", page[attempt]);
        }
        printf("\n");
    }
    pDevClose(device);
    return (found || sawHeader) ? 0 : 1;
}

static int SendDataTransferCommand(const char *serial, SPICK_TYPE mode)
{
    void *device = OpenDevice(serial);
    uint8_t tx[SPI_PAGE_LEN];
    uint8_t rx[SPI_PAGE_LEN];
    ErrorType error;

    if (device == NULL) {
        return -1;
    }
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    tx[0] = 0x76U;
    tx[1] = 0x25U;
    tx[2] = 0x09U;
    tx[9] = 0x09U;
    error = pSPIWriteRead(device, mode, ENDIAN_MSB, SPI_PAGE_LEN, tx, rx);
    printf("COMMAND09 serial=%s mode=%d result=0x%X rx_first16:",
           serial, (int)mode, (unsigned int)error);
    {
        unsigned int i;
        for (i = 0U; i < 16U; i++) {
            printf(" %02X", rx[i]);
        }
    }
    printf("\n");
    pDevClose(device);
    return (error == ErrNone) ? 0 : (int)error;
}

static int SendWorkModeCommand(const char *serial, SPICK_TYPE mode,
                               uint8_t workMode)
{
    void *device = OpenDevice(serial);
    uint8_t tx[SPI_PAGE_LEN];
    uint8_t rx[SPI_PAGE_LEN];
    ErrorType error;

    if (device == NULL) {
        return -1;
    }
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    tx[0] = 0x76U;
    tx[1] = 0x25U;
    tx[2] = 0x01U;
    tx[3] = workMode;
    tx[9] = (uint8_t)(0x01U + workMode);
    error = pSPIWriteRead(device, mode, ENDIAN_MSB, SPI_PAGE_LEN, tx, rx);
    printf("COMMAND01 serial=%s mode=%d workMode=%u result=0x%X\n",
           serial, (int)mode, workMode, (unsigned int)error);
    pDevClose(device);
    return (error == ErrNone) ? 0 : (int)error;
}

static int Exchange512(const char *serial, SPICK_TYPE mode, const char *prefix)
{
    void *device = OpenDevice(serial);
    uint8_t tx[DATA_FRAME_LEN];
    uint8_t rx[DATA_FRAME_LEN];
    char path[MAX_PATH];
    uint32_t i;
    ErrorType error;

    if (device == NULL) {
        return -1;
    }
    for (i = 0U; i < DATA_FRAME_LEN; i++) {
        tx[i] = (uint8_t)i;
        rx[i] = 0U;
    }
    error = pSPIWriteRead(device, mode, ENDIAN_MSB, DATA_FRAME_LEN, tx, rx);
    printf("EXCHANGE serial=%s mode=%d length=512 result=0x%X\n",
           serial, (int)mode, (unsigned int)error);
    PrintBytes("RX", rx, DATA_FRAME_LEN);
    snprintf(path, sizeof(path), "%s_tx.bin", prefix);
    (void)SaveBytes(path, tx, sizeof(tx));
    snprintf(path, sizeof(path), "%s_rx.bin", prefix);
    (void)SaveBytes(path, rx, sizeof(rx));
    pDevClose(device);
    return (error == ErrNone) ? 0 : (int)error;
}

static void Usage(const char *program)
{
    printf("Usage:\n");
    printf("  %s scan\n", program);
    printf("  %s probe <serial> [mode]\n", program);
    printf("  %s command09 <serial> [mode]\n", program);
    printf("  %s command01 <serial> [mode] [work-mode]\n", program);
    printf("  %s exchange <serial> [mode=1] [output-prefix]\n", program);
}

int main(int argc, char **argv)
{
    int count = 0;
    char *devices;
    int mode = 0;

    if (LoadApi() != 0) {
        return 1;
    }
    devices = pDevicesScan(dev_spi, &count);
    printf("SCAN count=%d\n%s\n", count, devices != NULL ? devices : "(null)");
    if ((argc < 2) || (strcmp(argv[1], "scan") == 0)) {
        return (count > 0) ? 0 : 1;
    }
    if (argc >= 4) {
        mode = atoi(argv[3]);
    }
    if ((mode < 0) || (mode > 3)) {
        fprintf(stderr, "ERROR: mode must be 0..3\n");
        return 1;
    }
    if ((strcmp(argv[1], "probe") == 0) && (argc >= 3)) {
        return ProbeTelemetry(argv[2], (SPICK_TYPE)mode);
    }
    if ((strcmp(argv[1], "command09") == 0) && (argc >= 3)) {
        return SendDataTransferCommand(argv[2], (SPICK_TYPE)mode);
    }
    if ((strcmp(argv[1], "command01") == 0) && (argc >= 3)) {
        int workMode = (argc >= 5) ? atoi(argv[4]) : 3;
        if ((workMode < 0) || (workMode > 255)) {
            fprintf(stderr, "ERROR: work mode must be 0..255\n");
            return 1;
        }
        return SendWorkModeCommand(argv[2], (SPICK_TYPE)mode,
                                   (uint8_t)workMode);
    }
    if ((strcmp(argv[1], "exchange") == 0) && (argc >= 3)) {
        if (argc < 4) {
            mode = LOW_2EDG;
        }
        const char *prefix = (argc >= 5) ? argv[4] : "spi2_dma_512";
        return Exchange512(argv[2], (SPICK_TYPE)mode, prefix);
    }
    Usage(argv[0]);
    return 1;
}
