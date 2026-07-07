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
    printf("TestJtoolPowerScan is only available on Windows JTOOL builds.\n");
    return 1;
}
#else

#define JTOOL_SPI_VCC_5V     0
#define JTOOL_SPI_VCC_EQ_VIO 1
#define JTOOL_SPI_VCC_OFF    2
#define JTOOL_SPI_VIO_3V3    0
#define JTOOL_SPI_VIO_1V8    1

typedef struct {
    const char* name;
    uint8_t vcc;
    uint8_t vio;
    const char* description;
} PowerMode;

static const PowerMode g_powerModes[] = {
    { "3v3", JTOOL_SPI_VCC_EQ_VIO, JTOOL_SPI_VIO_3V3, "VCC=IO level, IO=3.3V" },
    { "1v8", JTOOL_SPI_VCC_EQ_VIO, JTOOL_SPI_VIO_1V8, "VCC=IO level, IO=1.8V" },
    { "5v",  JTOOL_SPI_VCC_5V,     JTOOL_SPI_VIO_3V3, "VCC=5V, IO=3.3V" },
    { "off", JTOOL_SPI_VCC_OFF,    JTOOL_SPI_VIO_3V3, "VCC=off, IO=3.3V" },
};

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
    printf("%-18s -> 0x%02X (%s)\n", op, (unsigned int)err, JtoolErrorName(err));
}

static const PowerMode* FindMode(const char* name)
{
    size_t i;

    for (i = 0; i < sizeof(g_powerModes) / sizeof(g_powerModes[0]); i++) {
        if (strcmp(name, g_powerModes[i].name) == 0) {
            return &g_powerModes[i];
        }
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    const PowerMode* mode = &g_powerModes[0];
    void* handle;
    int count = 0;
    char* list;

    if (argc >= 2) {
        mode = FindMode(argv[1]);
    }

    if (mode == NULL) {
        printf("Usage: %s [3v3|1v8|5v|off]\n", argv[0]);
        printf("Examples:\n");
        printf("  %s off\n", argv[0]);
        printf("  %s 1v8\n", argv[0]);
        printf("  %s 3v3\n", argv[0]);
        printf("  %s 5v\n", argv[0]);
        return 1;
    }

    list = DevicesScan(dev_spi, &count);
    printf("DevicesScan(dev_spi): count=%d", count);
    if (list != NULL && list[0] != '\0') {
        printf(", list=%s", list);
    }
    printf("\n");

    handle = DevOpen(dev_spi, NULL, 0);
    printf("DevOpen(dev_spi): handle=%p\n", handle);
    if (handle == NULL) {
        return 1;
    }

    printf("Set JTOOL SPI power mode: %s (%s)\n", mode->name, mode->description);
    PrintError("JSPISetVio", JSPISetVio(handle, mode->vio));
    PrintError("JSPISetVcc", JSPISetVcc(handle, mode->vcc));

    printf("Measure the physical pins now, then press Enter to close JTOOL...\n");
    (void)getchar();

    DevClose(handle);
    return 0;
}
#endif
