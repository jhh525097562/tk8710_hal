#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include "x64/jtool.h"
#endif

#ifndef _WIN32
int main(void)
{
    printf("TestJtoolIoScan is only available on Windows JTOOL builds.\n");
    return 1;
}
#else

#define JTOOL_SPI_VCC_EQ_VIO 1
#define JTOOL_SPI_VIO_3V3    0
#define JTOOL_IO_VCC_3V3     1
#define JTOOL_IO_VIO_3V3     0

typedef struct {
    int type;
    const char* name;
} JtoolDeviceType;

static const char* JtoolErrorName(ErrorType err)
{
    switch (err) {
        case ErrNone:       return "ErrNone";
        case ErrParam:      return "ErrParam";
        case ErrDisconnect: return "ErrDisconnect";
        case ErrBusy:       return "ErrBusy";
        case ErrWaiting:    return "ErrWaiting";
        case ErrTimeOut:    return "ErrTimeOut";
        case ErrDataParse:  return "ErrDataParse";
        case ErrFailACK:    return "ErrFailACK";
        default:            return "ErrUnknown";
    }
}

static void PrintError(const char* op, ErrorType err)
{
    printf("  %-22s -> 0x%02X (%s)\n", op, (unsigned int)err, JtoolErrorName(err));
}

static void PrintScanResult(int devType, const char* name)
{
    int count = 0;
    char* list = DevicesScan(devType, &count);

    printf("DevicesScan(%s): count=%d", name, count);
    if (list != NULL && list[0] != '\0') {
        printf(", list=%s", list);
    }
    printf("\n");
}

static void SetupVoltage(void* handle, int devType)
{
    if (handle == NULL) {
        return;
    }

    if (devType == dev_spi) {
        PrintError("JSPISetVio(3.3V)", JSPISetVio(handle, JTOOL_SPI_VIO_3V3));
        PrintError("JSPISetVcc(=VIO)", JSPISetVcc(handle, JTOOL_SPI_VCC_EQ_VIO));
    } else if (devType == dev_io) {
        PrintError("JIOSetVio(3.3V)", JIOSetVio(handle, JTOOL_IO_VIO_3V3));
        PrintError("JIOSetVcc(3.3V)", JIOSetVcc(handle, JTOOL_IO_VCC_3V3));
    }
}

static void TestPin(void* handle, const char* label, uint32_t pin)
{
    ErrorType err;

    if (handle == NULL) {
        printf("[%s] handle is NULL, skip IO%u\n", label, (unsigned int)pin);
        return;
    }

    printf("[%s] test IO%u\n", label, (unsigned int)pin);

    err = IOSetOutWithVal(handle, pin, 1, 0);
    PrintError("IOSetOutWithVal low", err);
    Sleep(200);

    err = IOSetOutWithVal(handle, pin, 1, 1);
    PrintError("IOSetOutWithVal high", err);

    if (err != ErrNone) {
        PrintError("IOSetOut", IOSetOut(handle, pin, 1));
        PrintError("IOSetVal high", IOSetVal(handle, pin, 1));
    }

    printf("[%s] IO%u final request is high\n", label, (unsigned int)pin);
}

static int ParsePin(const char* text, uint32_t* pin)
{
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 0);

    if (end == text || *end != '\0' || value > 63UL) {
        return -1;
    }

    *pin = (uint32_t)value;
    return 0;
}

int main(int argc, char* argv[])
{
    static const JtoolDeviceType deviceTypes[] = {
        { dev_io,  "dev_io" },
        { dev_spi, "dev_spi" },
        { dev_all, "dev_all" },
    };
    uint32_t pin = 2;
    int i;

    if (argc >= 2 && ParsePin(argv[1], &pin) != 0) {
        printf("Usage: %s [io_number]\n", argv[0]);
        printf("Example: %s 2\n", argv[0]);
        printf("Example: %s 3\n", argv[0]);
        return 1;
    }

    printf("JTOOL IO scan, target IO%u\n", (unsigned int)pin);
    PrintScanResult(dev_all, "dev_all");
    PrintScanResult(dev_spi, "dev_spi");
    PrintScanResult(dev_io, "dev_io");

    for (i = 0; i < (int)(sizeof(deviceTypes) / sizeof(deviceTypes[0])); i++) {
        const JtoolDeviceType* dev = &deviceTypes[i];
        void* handle;

        printf("\nOpen %s...\n", dev->name);
        handle = DevOpen(dev->type, NULL, 0);
        printf("  handle=%p\n", handle);

        if (handle == NULL) {
            continue;
        }

        SetupVoltage(handle, dev->type);
        TestPin(handle, dev->name, pin);
        DevClose(handle);
    }

    return 0;
}
#endif
