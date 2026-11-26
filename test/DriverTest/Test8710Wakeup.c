/**
 * @file Test8710Wakeup.c
 * @brief 独立唤醒信号发送测试，初始化流程参考 Test8710RFTest.c。
 * @note 参数: mode frequency_hz duration_ms reg0814 reg0824
 *       初始化后覆盖两个全局寄存器，再发送一次；u 回车重发，q 回车退出。
 */

#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAKEUP_REG_0814 0x0814u
#define WAKEUP_REG_0824 0x0824u

typedef struct {
    uint32_t mode;
    uint32_t frequency_hz;
    uint32_t duration_ms;
    uint32_t reg0814;
    uint32_t reg0824;
} WakeupTestConfig;

static void PrintUsage(const char* program)
{
    printf("Usage: %s mode frequency_hz duration_ms reg0814 reg0824\n", program);
    printf("  mode:         1~3\n");
    printf("  frequency_hz: RF frequency in Hz, non-zero\n");
    printf("  duration_ms:  0~16777215 ms (use non-zero for initial testing)\n");
    printf("  reg0814:      full 32-bit value for global register 0x0814\n");
    printf("  reg0824:      full 32-bit value for global register 0x0824\n");
    printf("All five parameters are required; decimal or 0x-prefixed hex accepted.\n");
    printf("After initialization: write/read registers, then send once.\n");
    printf("Console: u + Enter = resend; q + Enter = exit.\n");
}

static int ParseU32(const char* text, uint32_t* value)
{
    char* end;
    unsigned long long parsed;
    int base = 10;

    /* Reject signs/whitespace; avoid base-0 interpreting leading zeroes as octal. */
    if (text == NULL || text[0] < '0' || text[0] > '9') {
        return -1;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    }
    errno = 0;
    parsed = strtoull(text, &end, base);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int WriteAndReadRegister(uint16_t address, uint32_t value)
{
    uint32_t readback;
    int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, address, value);

    if (ret != TK8710_OK) {
        fprintf(stderr, "Write 0x%04X failed: %d\n", (unsigned int)address, ret);
        return ret;
    }
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, address, &readback);
    if (ret != TK8710_OK) {
        fprintf(stderr, "Read 0x%04X failed: %d\n", (unsigned int)address, ret);
        return ret;
    }
    printf("Register 0x%04X: write=0x%08" PRIX32 " readback=0x%08" PRIX32 "\n",
           (unsigned int)address, value, readback);
    if (readback != value) {
        fprintf(stderr, "Register readback mismatch; wakeup not triggered.\n");
        return TK8710_ERR;
    }
    return TK8710_OK;
}

static int SendWakeup(const WakeupTestConfig* config)
{
    wakeUpParam_t params = {
        .wakeUpMode = (wakeUpMode_e)config->mode,
        .wakeUpId = 0,
        .wakeUpLen = config->duration_ms
    };
    int ret;

    /* Init writes 0x0814, so overrides must be applied AFTER TK8710Init.
     * Reapply them before each send. Addresses are global, not RF registers.
     */
    ret = WriteAndReadRegister(WAKEUP_REG_0814, config->reg0814);
    if (ret != TK8710_OK) {
        return ret;
    }
    ret = WriteAndReadRegister(WAKEUP_REG_0824, config->reg0824);
    if (ret != TK8710_OK) {
        return ret;
    }
    ret = TK8710Ctrl(TK8710_CTRL_TYPE_SEND_WAKEUP, &params);
    if (ret != TK8710_OK) {
        fprintf(stderr, "Wakeup configuration failed: %d\n", ret);
        return ret;
    }
    printf("Wakeup configured: mode=%" PRIu32 " frequency=%" PRIu32
           "Hz duration_ms=%" PRIu32 "\n",
           config->mode, config->frequency_hz, config->duration_ms);
    return TK8710_OK;
}

int main(int argc, char* argv[])
{
    WakeupTestConfig config;
    int input;
    int ret;
    ChiprfConfig rf_config = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .rxgain = 0x7e,
        .txgain = 0x2a
    };
    ChipConfig chip_config = {
        .bcn_agc = 32,
        .interval = 32,
        .conti_mode = 1,
        .ant_en = 0xff,
        .rf_sel = 0xff,
        .tx_bcn_en = 1,
        .rf_model = 1,
        .irq_ctrl0 = 0x7ff,
        .rfConfig = &rf_config
    };
    TK8710LogConfig_t log_config = {
        .level = TK8710_LOG_WARN,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
        .enable_file_logging = 1
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        PrintUsage(argv[0]);
        return 0;
    }
    if (argc != 6 || ParseU32(argv[1], &config.mode) != 0 ||
        ParseU32(argv[2], &config.frequency_hz) != 0 ||
        ParseU32(argv[3], &config.duration_ms) != 0 ||
        ParseU32(argv[4], &config.reg0814) != 0 ||
        ParseU32(argv[5], &config.reg0824) != 0) {
        fprintf(stderr, "Invalid arguments.\n");
        PrintUsage(argv[0]);
        return 1;
    }
    if (config.mode < 1 || config.mode > 3 || config.frequency_hz == 0 ||
        config.duration_ms > 0xffffffu) {
        fprintf(stderr, "Require mode=1~3, non-zero frequency, duration=0~16777215.\n");
        return 1;
    }

    rf_config.Freq = config.frequency_hz;
    ret = TK8710LogInit(&log_config);
    if (ret != TK8710_OK) {
        fprintf(stderr, "Log initialization failed: %d\n", ret);
        return 1;
    }
    printf("Initializing TK8710 at %" PRIu32 "Hz (TX gain=0x2A, RX gain=0x7E)...\n",
           config.frequency_hz);
    ret = TK8710Init(&chip_config);
    if (ret != TK8710_OK) {
        fprintf(stderr, "TK8710 initialization failed: %d\n", ret);
        return 1;
    }
    ret = SendWakeup(&config);
    if (ret != TK8710_OK) {
        return 1;
    }

    printf("Commands: u + Enter = resend same configuration; q + Enter = exit.\n");
    /* Keep the process alive, as in Test8710RFTest; duration is timed by chip. */
    while ((input = getchar()) != EOF) {
        if (input == 'q' || input == 'Q') {
            break;
        }
        if (input == 'u' || input == 'U') {
            ret = SendWakeup(&config);
            if (ret != TK8710_OK) {
                return 1;
            }
        }
    }
    return 0;
}
