/**
 * @file TestJtoolRegScan.c
 * @brief Probe a TK8710 register through jtool.dll without HAL initialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include "x64/jtool.h"
#endif

#define JTOOL_SPI_VCC_EQ_VIO 1U
#define JTOOL_SPI_VCC_OFF    2U
#define JTOOL_SPI_VIO_3V3    0U

#define TK8710_SPI_CMD_RD_REG 0x02U
#define TK8710_EXPECTED_VALUE  0xF6097E0FU

static const char* ErrorName(ErrorType err)
{
    switch (err) {
        case ErrNone:      return "ErrNone";
        case ErrDisconnect:return "ErrDisconnect";
        case ErrBusy:      return "ErrBusy";
        case ErrWaiting:   return "ErrWaiting";
        case ErrTimeOut:   return "ErrTimeOut";
        case ErrDataParse: return "ErrDataParse";
        case ErrFailACK:   return "ErrFailACK";
        default:           return "ErrUnknown";
    }
}

static uint32_t ReadBe32(const uint8_t* data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void PrintBytes(const char* name, const uint8_t* data, size_t len)
{
    size_t i;

    printf("  %s:", name);
    for (i = 0; i < len; ++i) {
        printf(" %02X", data[i]);
    }
    printf("\n");
}

static int ProbeRegister(void* handle, uint16_t addr, SPICK_TYPE mode,
                         uint8_t speed, uint8_t resetTarget, uint32_t* value)
{
    uint8_t resetTx[2] = {0x00U, 0x03U};
    uint8_t tx[8] = {0};
    uint8_t rx[8] = {0};
    ErrorType err;

    tx[0] = TK8710_SPI_CMD_RD_REG;
    tx[1] = (uint8_t)(addr >> 8);
    tx[2] = (uint8_t)(addr & 0xFFU);

    err = JSPISetSpeed(handle, speed);
    if (err != ErrNone) {
        printf("mode=%u speed=%u JSPISetSpeed failed: 0x%02X (%s)\n",
               (unsigned int)mode, (unsigned int)speed,
               (unsigned int)err, ErrorName(err));
        return -1;
    }

    if (resetTarget) {
        err = SPIWriteOnly(handle, mode, ENDIAN_MSB, sizeof(resetTx), resetTx);
        if (err != ErrNone) {
            printf("mode=%u speed=%u SPI reset failed: 0x%02X (%s)\n",
                   (unsigned int)mode, (unsigned int)speed,
                   (unsigned int)err, ErrorName(err));
            return -1;
        }
        Sleep(20);
    }

    err = SPIWriteRead(handle, mode, ENDIAN_MSB, sizeof(tx), tx, rx);
    printf("mode=%u speed=%u SPIWriteRead=0x%02X (%s)\n",
           (unsigned int)mode, (unsigned int)speed,
           (unsigned int)err, ErrorName(err));
    PrintBytes("TX", tx, sizeof(tx));
    PrintBytes("RX", rx, sizeof(rx));
    if (err != ErrNone) {
        return -1;
    }

    *value = ReadBe32(&rx[4]);
    printf("  addr=0x%04X value=0x%08X%s\n",
           (unsigned int)addr, (unsigned int)*value,
           (*value == TK8710_EXPECTED_VALUE) ? " MATCH" : "");
    return 0;
}

static int ProbeRegisterWithCommand(void* handle, uint16_t addr,
                                    SPICK_TYPE mode, uint8_t speed,
                                    FIELDLEN_TYPE dummyType, uint32_t* value)
{
    uint8_t rx[4] = {0};
    ErrorType err;

    err = JSPISetSpeed(handle, speed);
    if (err != ErrNone) {
        return -1;
    }

    err = SPIReadWithCMD(handle,
                         mode,
                         ENDIAN_MSB,
                         SINGLEALL,
                         FIELD_ONE,
                         TK8710_SPI_CMD_RD_REG,
                         FIELD_TWO,
                         addr,
                         FIELD_NONE,
                         0,
                         dummyType,
                         sizeof(rx),
                         rx);
    printf("SPIReadWithCMD mode=%u speed=%u dummy=%u: 0x%02X (%s)\n",
           (unsigned int)mode,
           (unsigned int)speed,
           (unsigned int)dummyType,
           (unsigned int)err,
           ErrorName(err));
    PrintBytes("RX", rx, sizeof(rx));
    if (err != ErrNone) {
        return -1;
    }

    *value = ReadBe32(rx);
    printf("  addr=0x%04X value=0x%08X%s\n",
           (unsigned int)addr,
           (unsigned int)*value,
           (*value == TK8710_EXPECTED_VALUE) ? " MATCH" : "");
    return 0;
}

int main(int argc, char* argv[])
{
    uint16_t addr = 0xC020U;
    int devCount = 0;
    char* devList;
    void* handle;
    ErrorType err;
    uint32_t value;
    unsigned int mode;
    unsigned int speed;
    unsigned int dummy;
    int matched = 0;
    int loop = 0;
    int argIndex;

    for (argIndex = 1; argIndex < argc; ++argIndex) {
        char* end;
        unsigned long parsed;

        if (strcmp(argv[argIndex], "--loop") == 0) {
            loop = 1;
            continue;
        }

        end = NULL;
        parsed = strtoul(argv[argIndex], &end, 0);
        if (end == argv[argIndex] || *end != '\0' || parsed > 0xFFFFUL) {
            fprintf(stderr, "Usage: %s [register_address] [--loop]\n", argv[0]);
            return 2;
        }
        addr = (uint16_t)parsed;
    }

    devList = DevicesScan(dev_spi, &devCount);
    printf("DevicesScan(dev_spi): count=%d, list=%s\n",
           devCount, devList != NULL ? devList : "(null)");
    if (devCount <= 0) {
        return 1;
    }

    handle = DevOpen(dev_spi, NULL, 0);
    printf("DevOpen(dev_spi): handle=%p\n", handle);
    if (handle == NULL) {
        return 1;
    }

    err = JSPISetVcc(handle, JTOOL_SPI_VCC_OFF);
    printf("JSPISetVcc(OFF): 0x%02X (%s)\n", (unsigned int)err, ErrorName(err));
    Sleep(100);
    err = JSPISetVio(handle, JTOOL_SPI_VIO_3V3);
    printf("JSPISetVio(3.3V): 0x%02X (%s)\n", (unsigned int)err, ErrorName(err));
    err = JSPISetVcc(handle, JTOOL_SPI_VCC_EQ_VIO);
    printf("JSPISetVcc(=VIO): 0x%02X (%s)\n", (unsigned int)err, ErrorName(err));
    Sleep(1000);

    for (speed = 0; speed <= 5 && !matched; ++speed) {
        for (mode = LOW_1EDG; mode <= HIGH_2EDG; ++mode) {
            if (ProbeRegister(handle, addr, (SPICK_TYPE)mode,
                              (uint8_t)speed, 1, &value) == 0 &&
                value == TK8710_EXPECTED_VALUE) {
                matched = 1;
                break;
            }
        }
    }

    for (speed = 0; speed <= 5 && !matched; ++speed) {
        for (mode = LOW_1EDG; mode <= HIGH_2EDG && !matched; ++mode) {
            for (dummy = FIELD_NONE; dummy <= FIELD_ONE; ++dummy) {
                if (ProbeRegisterWithCommand(handle,
                                             addr,
                                             (SPICK_TYPE)mode,
                                             (uint8_t)speed,
                                             (FIELDLEN_TYPE)dummy,
                                             &value) == 0 &&
                    value == TK8710_EXPECTED_VALUE) {
                    matched = 1;
                    break;
                }
            }
        }
    }

    if (loop) {
        printf("Continuous Mode 0/speed 0 reads; press Ctrl+C to stop.\n");
        for (;;) {
            (void)ProbeRegister(handle, addr, LOW_1EDG, 0, 0, &value);
            Sleep(200);
        }
    }

    DevClose(handle);
    printf("Result: %s\n", matched ? "PASS" : "NO MATCH");
    return matched ? 0 : 1;
}
