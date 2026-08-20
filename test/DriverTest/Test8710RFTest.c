#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal_api.h"
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"
#include "driver_test_platform.h"
#include "support/rf_cal_tcp_server.h"

#define RF_TEST_DEFAULT_RATE_MODE   6
#define RF_TEST_DEFAULT_FREQ_HZ     509100000u
#define RF_TEST_DEFAULT_TX_GAIN     0x2au
#define RF_TEST_DEFAULT_RX_GAIN     0x7eu
#define RF_TEST_DEFAULT_SELECT      0u
#define RF_TEST_TONE_FREQ           335544u
#define RF_TEST_TX_POWER_TONE_GAIN  0x40u
#define RF_TEST_LOG_LINE_LEN        256u

typedef enum {
    RF_TEST_SELECT_DC_REMOVAL = 0,
    RF_TEST_SELECT_TX_POWER = 1,
    RF_TEST_SELECT_RX_SENSITIVITY = 2
} RfTestSelect;

static volatile int g_running = 1;
static volatile uint8_t g_testSelect = RF_TEST_DEFAULT_SELECT;
static volatile uint32_t g_packetCount = 0;
static volatile uint32_t g_packetLostCount = 0;
static volatile uint32_t g_totalPacketCount = 0;
static volatile uint32_t g_totalPacketLostCount = 0;
static volatile uint8_t g_hasLastSignal = 0;
static volatile uint8_t g_lastValidUserIndex = 0;
static volatile int16_t g_lastRssi = 0;
static volatile uint8_t g_lastSnr = 0;
static volatile int32_t g_lastFreqHz = 0;
static volatile uint32_t g_logSequence = 0;
static char g_lastLogLine[RF_TEST_LOG_LINE_LEN] = "no_rx_log";

#ifndef _WIN32
static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
    printf("\nReceived signal, exiting...\n");
}
#endif

static int RfTestReadRegister(uint16_t addr, uint32_t* value, void* userData)
{
    (void)userData;
    return TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr, value);
}

static int RfTestWriteRegister(uint16_t addr, uint32_t value, void* userData)
{
    (void)userData;
    return TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr, value);
}

static int RfTestFormatStats(char* response, size_t responseSize, void* userData)
{
    uint32_t periodCount;
    uint32_t periodLost;
    uint32_t totalCount;
    uint32_t totalLost;
    uint8_t hasLastSignal;
    uint8_t lastUser;
    int16_t lastRssi;
    uint8_t lastSnr;
    int32_t lastFreqHz;
    float totalLoss;
    int written;

    (void)userData;

    periodCount = g_packetCount;
    periodLost = g_packetLostCount;
    totalCount = g_totalPacketCount;
    totalLost = g_totalPacketLostCount;
    hasLastSignal = g_hasLastSignal;
    lastUser = g_lastValidUserIndex;
    lastRssi = g_lastRssi;
    lastSnr = g_lastSnr;
    lastFreqHz = g_lastFreqHz;
    totalLoss = totalCount == 0 ? 0.0f : (float)totalLost * 100.0f / (float)totalCount;

    written = snprintf(response, responseSize,
                       "OK STATS mode=%u total=%u lost=%u period=%u period_lost=%u "
                       "loss=%.2f last_valid=%u last_user=%u last_rssi=%d "
                       "last_snr=%u last_freq=%ld\n",
                       (unsigned int)g_testSelect,
                       (unsigned int)totalCount,
                       (unsigned int)totalLost,
                       (unsigned int)periodCount,
                       (unsigned int)periodLost,
                       totalLoss,
                       (unsigned int)hasLastSignal,
                       (unsigned int)lastUser,
                       (int)lastRssi,
                       (unsigned int)lastSnr,
                       (long)lastFreqHz);
    if (written < 0 || (size_t)written >= responseSize) {
        return -1;
    }
    return 0;
}

static int RfTestFormatLog(char* response, size_t responseSize, void* userData)
{
    char line[RF_TEST_LOG_LINE_LEN];
    uint32_t sequence;
    int written;

    (void)userData;

    sequence = g_logSequence;
    snprintf(line, sizeof(line), "%s", g_lastLogLine);
    written = snprintf(response, responseSize, "OK LOG seq=%u %s\n",
                       (unsigned int)sequence, line);
    if (written < 0 || (size_t)written >= responseSize) {
        return -1;
    }
    return 0;
}

static void RfTestEmitLog(const char* format, ...)
{
    char line[RF_TEST_LOG_LINE_LEN];
    va_list args;

    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    line[sizeof(line) - 1u] = '\0';
    snprintf(g_lastLogLine, sizeof(g_lastLogLine), "%s", line);
    g_logSequence++;
    printf("%s\n", line);
}

static int ParseU32Argument(const char* text, uint32_t* value)
{
    char* end;
    unsigned long long parsed;

    if (text == NULL || value == NULL || text[0] == '\0' || text[0] == '-') {
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

static int IsSupportedRateMode(int rateMode)
{
    return ((rateMode >= 5 && rateMode <= 11) || rateMode == 18);
}

static const char* GetTestSelectName(uint8_t testSelect)
{
    switch (testSelect) {
        case RF_TEST_SELECT_DC_REMOVAL:
            return "DC removal";
        case RF_TEST_SELECT_TX_POWER:
            return "TX power";
        case RF_TEST_SELECT_RX_SENSITIVITY:
            return "RX sensitivity";
        default:
            return "unknown";
    }
}

static void PrintUsage(const char* program)
{
    printf("Usage: %s [rate_mode frequency tx_gain rx_gain test_select] "
           "[--tcp] [--bind IP] [--port PORT]\n", program);
    printf("  rate_mode:   5, 6, 7, 8, 9, 10, 11 or 18 (default: 6)\n");
    printf("  frequency:   RF frequency in Hz (default: 509100000)\n");
    printf("  tx_gain:     TX gain, decimal or hex (default: 42 / 0x2A)\n");
    printf("  rx_gain:     RX gain, decimal or hex (default: 126 / 0x7E)\n");
    printf("  test_select: 0=DC removal, 1=TX power, 2=RX sensitivity\n");
    printf("  --tcp:       Run the Windows TCP server\n");
    printf("  --bind:      Listen IPv4 address (default: %s)\n", RF_CAL_DEFAULT_BIND_IP);
    printf("  --port:      Listen TCP port (default: %u)\n", RF_CAL_DEFAULT_PORT);
    printf("\nExamples:\n");
    printf("  %s 6 509100000 0x2a 0x7e 0\n", program);
    printf("  %s 6 509100000 0x2a 0x7e 1 --tcp\n", program);
    printf("  %s 6 509100000 0x2a 0x7e 2 --tcp --port 12879\n", program);
}

static void PrintStats(void)
{
    char stats[512];

    if (RfTestFormatStats(stats, sizeof(stats), NULL) == 0) {
        printf("%s", stats);
    } else {
        printf("ERR STATS_FORMAT_FAILED\n");
    }
}

static void OnDriverRxData(TK8710IrqResult* irqResult)
{
    uint8_t validUserCount = 0;

    if (irqResult == NULL || irqResult->irq_type != TK8710_IRQ_MD_DATA) {
        return;
    }

    g_packetCount++;
    g_totalPacketCount++;

    if (irqResult->crcValidCount > 0) {
        for (uint8_t userIndex = 0; userIndex < TK8710_MAX_DATA_USERS; userIndex++) {
            if (irqResult->crcResults[userIndex].crcValid) {
                uint32_t rssi;
                uint32_t freqSignal;
                uint8_t snr;

                if (TK8710GetRxUserSignalQuality(userIndex, &rssi, &snr,
                                                 &freqSignal) == TK8710_OK) {
                    uint32_t freq26 = freqSignal & 0x03FFFFFFu;
                    int32_t freqValue = freq26 > (1u << 25) ?
                        (int32_t)(freq26 - (1u << 26)) : (int32_t)freq26;
                    int16_t rssiValue = (int16_t)(rssi - 2048u) / 4;
                    uint8_t snrValue = snr / 4u;

                    g_hasLastSignal = 1;
                    g_lastValidUserIndex = userIndex;
                    g_lastRssi = rssiValue;
                    g_lastSnr = snrValue;
                    g_lastFreqHz = freqValue / 128;
                    validUserCount++;

                    RfTestEmitLog("RX user=%u freq=%ldHz rssi=%d snr=%u total=%u lost=%u",
                                  (unsigned int)userIndex,
                                  (long)g_lastFreqHz,
                                  (int)rssiValue,
                                  (unsigned int)snrValue,
                                  (unsigned int)g_totalPacketCount,
                                  (unsigned int)g_totalPacketLostCount);
                }
            }
        }
    }

    if (validUserCount == 0) {
        g_packetLostCount++;
        g_totalPacketLostCount++;
        RfTestEmitLog("RX lost: crc_ok=%u crc_err=%u total=%u lost=%u",
                      (unsigned int)irqResult->crcValidCount,
                      (unsigned int)irqResult->crcErrorCount,
                      (unsigned int)g_totalPacketCount,
                      (unsigned int)g_totalPacketLostCount);
    }

    if (g_packetCount >= 100u) {
        PrintStats();
        g_packetCount = 0;
        g_packetLostCount = 0;
    }
}

static void OnDriverTxSlot(TK8710IrqResult* irqResult)
{
    (void)irqResult;
}

static void OnDriverSlotEnd(TK8710IrqResult* irqResult)
{
    (void)irqResult;
}

static void OnDriverError(TK8710IrqResult* irqResult)
{
    if (irqResult != NULL) {
        printf("Driver error irq_type=%d\n", irqResult->irq_type);
    } else {
        printf("Driver error\n");
    }
}

static int ConfigureTone(uint8_t testSelect)
{
    int ret;
    TxToneConfig txToneConfig = {
        .freq = RF_TEST_TONE_FREQ,
        .gain = testSelect == RF_TEST_SELECT_TX_POWER ?
            RF_TEST_TX_POWER_TONE_GAIN : 0u
    };

    ret = TK8710DebugCtrl(TK8710_DBG_TYPE_TX_TONE, TK8710_DBG_OPT_SET,
                          &txToneConfig, NULL);
    if (ret == TK8710_OK) {
        printf("Tone configured: freq=%uHz gain=0x%02X\n",
               (unsigned int)txToneConfig.freq,
               (unsigned int)txToneConfig.gain);
    } else {
        printf("Tone configuration failed: %d\n", ret);
    }
    return ret;
}

static int FillRxDaM(slotCfg_t* slotCfg, int rateMode)
{
    if (slotCfg == NULL) {
        return -1;
    }

    switch (rateMode) {
        case 5:
            slotCfg->s0Cfg[0].da_m = 40u * 256u;
            slotCfg->s3Cfg[0].da_m = 135072u;
            break;
        case 6:
            slotCfg->s0Cfg[0].da_m = 46u * 256u;
            slotCfg->s3Cfg[0].da_m = 69536u;
            break;
        case 7:
            slotCfg->s0Cfg[0].da_m = 113u * 256u;
            slotCfg->s3Cfg[0].da_m = 36768u;
            break;
        case 8:
            slotCfg->s0Cfg[0].da_m = 146u * 256u;
            slotCfg->s3Cfg[0].da_m = 20384u;
            break;
        case 9:
            slotCfg->s0Cfg[0].da_m = 64u * 256u;
            slotCfg->s3Cfg[0].da_m = 12192u;
            break;
        case 10:
            slotCfg->s0Cfg[0].da_m = 19u * 256u;
            slotCfg->s3Cfg[0].da_m = 8096u;
            break;
        case 11:
        case 18:
            slotCfg->s0Cfg[0].da_m = 256u;
            slotCfg->s3Cfg[0].da_m = 6048u;
            break;
        default:
            return -1;
    }

    slotCfg->s1Cfg[0].da_m = 0;
    slotCfg->s2Cfg[0].da_m = 0;
    return 0;
}

static int ConfigureRxSensitivity(int rateMode, uint32_t frequency)
{
    int ret;
    int dataLen = rateMode == 18 ? 36 : 22;
    slotCfg_t slotCfg;

    memset(&slotCfg, 0, sizeof(slotCfg));
    slotCfg.msMode = TK8710_MODE_SLAVE;
    slotCfg.plCrcEn = 1;
    slotCfg.brdUserNum = 0;
    slotCfg.antEn = 0x1;
    slotCfg.rfSel = 0x1;
    slotCfg.txBeamCtrlMode = 0;
    slotCfg.txBcnAntEn = 0x1;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0f;
    slotCfg.frameTimeLen = 0;
    slotCfg.rateCount = 1;
    slotCfg.rateModes[0] = (rateMode_e)rateMode;

    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = (uint8_t)i;
    }

    if (FillRxDaM(&slotCfg, rateMode) != 0) {
        printf("Unsupported RX sensitivity rate mode: %d\n", rateMode);
        return -1;
    }

    slotCfg.s0Cfg[0].byteLen = 0;
    slotCfg.s0Cfg[0].centerFreq = frequency;
    slotCfg.s1Cfg[0].byteLen = 0;
    slotCfg.s1Cfg[0].centerFreq = frequency;
    slotCfg.s2Cfg[0].byteLen = 0;
    slotCfg.s2Cfg[0].centerFreq = frequency;
    slotCfg.s3Cfg[0].byteLen = (uint16_t)dataLen;
    slotCfg.s3Cfg[0].centerFreq = frequency;

    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slotCfg);
    if (ret != TK8710_OK) {
        printf("RX slot configuration failed: %d\n", ret);
        return ret;
    }
    printf("RX slot configured: rate_mode=%d freq=%uHz data_len=%d\n",
           rateMode, (unsigned int)frequency, dataLen);

    /* 配置rx_fe_regs->ddc寄存器 */
    {
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x0000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x1000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x2000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x3000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x4000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x5000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x6000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x7000, 0x1b33333);
        if (ret != TK8710_OK) return ret;
    }

    ret = TK8710Start(TK8710_MODE_SLAVE, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        printf("RX start failed: %d\n", ret);
        return TK8710_HAL_ERROR_START;
    }
    printf("RX sensitivity test started\n");
    return TK8710_OK;
}

static int ParseCommandLine(int argc, char* argv[], int* rateMode, uint32_t* frequency,
                            uint32_t* txGain, uint32_t* rxGain,
                            uint32_t* testSelect, int* tcpMode,
                            const char** tcpBindIp, uint16_t* tcpPort)
{
    uint32_t positionalValues[5] = {0};
    uint8_t positionalCount = 0;

    for (int argIndex = 1; argIndex < argc; argIndex++) {
        uint32_t parsedPort;

        if (strcmp(argv[argIndex], "--help") == 0 ||
            strcmp(argv[argIndex], "-h") == 0) {
            PrintUsage(argv[0]);
            return 1;
        }
        if (strcmp(argv[argIndex], "--tcp") == 0) {
            *tcpMode = 1;
            continue;
        }
        if (strcmp(argv[argIndex], "--bind") == 0) {
            if (++argIndex >= argc) {
                printf("Error: --bind requires an IP address\n");
                return -1;
            }
            *tcpBindIp = argv[argIndex];
            continue;
        }
        if (strcmp(argv[argIndex], "--port") == 0) {
            if (++argIndex >= argc ||
                ParseU32Argument(argv[argIndex], &parsedPort) != 0 ||
                parsedPort == 0 || parsedPort > UINT16_MAX) {
                printf("Error: invalid --port value\n");
                return -1;
            }
            *tcpPort = (uint16_t)parsedPort;
            continue;
        }
        if (argv[argIndex][0] == '-') {
            printf("Error: unknown option: %s\n", argv[argIndex]);
            return -1;
        }
        if (positionalCount >= 5 ||
            ParseU32Argument(argv[argIndex], &positionalValues[positionalCount]) != 0) {
            printf("Error: invalid positional argument: %s\n", argv[argIndex]);
            return -1;
        }
        positionalCount++;
    }

    if (positionalCount != 0 && positionalCount != 3 && positionalCount != 5) {
        printf("Error: provide either 3 or 5 positional arguments\n");
        PrintUsage(argv[0]);
        return -1;
    }

    if (positionalCount == 3 || positionalCount == 5) {
        *rateMode = (int)positionalValues[0];
        *frequency = positionalValues[1];
        *txGain = positionalValues[2];
    }
    if (positionalCount == 5) {
        *rxGain = positionalValues[3];
        *testSelect = positionalValues[4];
    }

    return 0;
}

static int ValidateParameters(int rateMode, uint32_t frequency, uint32_t txGain,
                              uint32_t rxGain, uint32_t testSelect)
{
    if (!IsSupportedRateMode(rateMode)) {
        printf("Error: invalid rate mode %d. Supported modes: 5,6,7,8,9,10,11,18\n",
               rateMode);
        return -1;
    }
    if (frequency == 0) {
        printf("Error: frequency must be non-zero\n");
        return -1;
    }
    if (txGain > UINT8_MAX || rxGain > UINT8_MAX) {
        printf("Error: tx_gain and rx_gain must be <= 255\n");
        return -1;
    }
    if (testSelect > RF_TEST_SELECT_RX_SENSITIVITY) {
        printf("Error: test_select must be 0, 1 or 2\n");
        return -1;
    }
    return 0;
}

static void RunConsoleLoop(void)
{
    int input;

    printf("Commands: s=stats, q=quit\n");
    while (g_running) {
        printf("TK8710-RF> ");
#ifdef _WIN32
        input = _getch();
        if (input != '\r') {
            printf("%c\n", input);
        } else {
            printf("\n");
            continue;
        }
#else
        input = getchar();
        if (input == '\n' || input == '\r') {
            continue;
        }
#endif

        switch (input) {
            case 's':
            case 'S':
                PrintStats();
                break;
            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
                break;
            default:
                printf("Unknown command. Commands: s=stats, q=quit\n");
                break;
        }
    }
}

int main(int argc, char* argv[])
{
    int ret;
    int rateMode = RF_TEST_DEFAULT_RATE_MODE;
    uint32_t frequency = RF_TEST_DEFAULT_FREQ_HZ;
    uint32_t txGain = RF_TEST_DEFAULT_TX_GAIN;
    uint32_t rxGain = RF_TEST_DEFAULT_RX_GAIN;
    uint32_t testSelect = RF_TEST_DEFAULT_SELECT;
    int tcpMode = 0;
    const char* tcpBindIp = RF_CAL_DEFAULT_BIND_IP;
    uint16_t tcpPort = RF_CAL_DEFAULT_PORT;
    ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .Freq = RF_TEST_DEFAULT_FREQ_HZ,
        .rxgain = RF_TEST_DEFAULT_RX_GAIN,
        .txgain = RF_TEST_DEFAULT_TX_GAIN
    };
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 1,
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = 0,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig
    };
    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_WARN,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
        .enable_file_logging = 1,
        .log_file_dir = NULL
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = OnDriverSlotEnd,
        .onError = OnDriverError
    };

    ret = ParseCommandLine(argc, argv, &rateMode, &frequency, &txGain, &rxGain,
                           &testSelect, &tcpMode, &tcpBindIp, &tcpPort);
    if (ret > 0) {
        return 0;
    }
    if (ret != 0 ||
        ValidateParameters(rateMode, frequency, txGain, rxGain, testSelect) != 0) {
        return 1;
    }

#ifndef _WIN32
    if (tcpMode) {
        printf("Error: TCP mode is only supported on Windows\n");
        return 1;
    }
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#else
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, ".UTF8");
#endif

    g_testSelect = (uint8_t)testSelect;
    rfConfig.Freq = frequency;
    rfConfig.txgain = (uint8_t)txGain;
    rfConfig.rxgain = (uint8_t)rxGain;

    printf("TK8710 RF Test Program\n");
    printf("rate_mode=%d frequency=%uHz tx_gain=0x%02X rx_gain=0x%02X "
           "test_select=%u (%s)\n",
           rateMode,
           (unsigned int)frequency,
           (unsigned int)txGain,
           (unsigned int)rxGain,
           (unsigned int)testSelect,
           GetTestSelectName((uint8_t)testSelect));

    TK8710RegisterCallbacks(&driverCallbacks);
    TK8710LogInit(&defaultLogConfig);

    printf("Initializing TK8710...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("TK8710 initialization failed: %d\n", ret);
        return 1;
    }
    printf("TK8710 initialization completed\n");

    if (testSelect == RF_TEST_SELECT_RX_SENSITIVITY) {
        ret = ConfigureRxSensitivity(rateMode, frequency);
    } else {
        ret = ConfigureTone((uint8_t)testSelect);
    }
    if (ret != TK8710_OK) {
        return 1;
    }

#ifdef _WIN32
    if (tcpMode) {
        RfCalTcpServerConfig tcpConfig = {
            .bindIp = tcpBindIp,
            .port = tcpPort,
            .readReg = RfTestReadRegister,
            .writeReg = RfTestWriteRegister,
            .getStats = RfTestFormatStats,
            .getLog = RfTestFormatLog,
            .userData = NULL,
            .running = &g_running
        };

        ret = RfCalTcpServerRun(&tcpConfig);
        if (ret != 0) {
            printf("RF test TCP server failed: %d\n", ret);
        }
    } else
#endif
    {
        RunConsoleLoop();
    }

    printf("Final stats:\n");
    PrintStats();
    TK8710PrintIrqTimeStats();
    return ret == 0 ? 0 : 1;
}
