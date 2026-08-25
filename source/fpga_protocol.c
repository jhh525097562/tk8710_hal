#include "fpga_protocol.h"

#include <stdio.h>
#include <string.h>

#include "data_transfer.h"
#include "fpga_param_store.h"
#include "tk8710_sat_payload_app.h"

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "mibspi.h"
#include "tk8710_hal.h"
#include "system.h"
#endif

#define FPGA_RC_SYNC0 0x76U
#define FPGA_RC_SYNC1 0x25U
#define FPGA_TM_SYNC0 0xEBU
#define FPGA_TM_SYNC1 0x90U
#define FPGA_SPI_GROUP 0U
#define FPGA_UNSUPPORTED_RESULT ((int32_t)SAT_PAYLOAD_ERR_UNSUPPORTED)
#define FPGA_PARAM_STORE_RESULT_FAIL (-10)
#define FPGA_BOOT_FLAG_STORE_RESULT_FAIL (-11)
#define FPGA_BOOT_FLAG_FIRMWARE_UPGRADE 1U
#define FPGA_BOOT_FLAG_ROLLBACK 2U
#define FPGA_BOOT_FLAG_CLEAR 0U
#define FPGA_WORK_MODE_MAX 6U
#define FPGA_PROTOCOL_RATE_MAX 2U
#define FPGA_TK_RATE_BASE 6U
#define FPGA_DISPATCH_RANGE_ERROR (-2)
#define FPGA_RESET_TYPE_NORMAL 0U
#define FPGA_RESET_TYPE_WATCHDOG 1U
#define FPGA_RESET_TYPE_OTHER 2U
#define FPGA_RESET_CAUSE_WATCHDOG 0x2000U
#define FPGA_RESET_CAUSE_NORMAL_MASK 0x8030U
#define FPGA_RC_LOG_TEXT_LEN 192U
#define FPGA_RC_FRAME_HEX_TEXT_LEN 32U

#if !defined(FPGA_PROTOCOL_HOST_TEST)
/* FPGA RC/TM is physically wired to MIBSPI1. */
#define FPGA_MIBSPI_REG mibspiREG1
#define FPGA_MIBSPI_RAM mibspiRAM1
#endif

typedef enum
{
    FPGA_CMD_WORK_MODE = 0x01U,
    FPGA_CMD_RATE = 0x02U,
    FPGA_CMD_SLOT = 0x03U,
    FPGA_CMD_TX_POWER = 0x04U,
    FPGA_CMD_FREQ = 0x05U,
    FPGA_CMD_RF_CHANNEL = 0x06U,
    FPGA_CMD_RESET = 0x07U,
    FPGA_CMD_FIRMWARE_UPGRADE = 0x08U,
    FPGA_CMD_DATA_TRANSFER = 0x09U,
    FPGA_CMD_WRITE_REG = 0x0AU,
    FPGA_CMD_READ_REG = 0x0BU,
    FPGA_CMD_ROLLBACK = 0x0CU,
    FPGA_CMD_UTC_TIME = 0x0DU,
    FPGA_CMD_DC_PARAMS = 0x0EU
} FpgaCommand;

typedef struct
{
    uint32_t rxFrameCount;
    uint32_t physicalRxFrameCount;
    uint32_t rxErrorCount;
    uint32_t unsupportedCount;
    uint8_t lastCommandId;
    uint8_t lastPhysicalRxFrame[FPGA_PROTOCOL_RC_FRAME_LEN];
    uint32_t lastTelemetryMs;
    int32_t lastResult;
    uint8_t activeMode;
    uint8_t slotConfig;
    uint8_t txPower;
    uint16_t lastRegDevice;
    uint16_t lastRegAddress;
    uint32_t lastRegValue;
    uint32_t utcSeconds;
    uint32_t resetCount;
    uint8_t resetType;
    uint8_t initialized;
    uint8_t pendingParamsValid;
    uint8_t txPageIndex;
    uint8_t txBurstActive;
    uint8_t txBurstPending;
    uint8_t pendingRcLogValid;
    uint8_t restoringDcParams;
    uint8_t dcValidMask;
    int16_t dcI[8U];
    int16_t dcQ[8U];
    SatPayloadWorkParams pendingParams;
    char pendingRcLog[FPGA_RC_LOG_TEXT_LEN];
    uint8_t txFrame[FPGA_PROTOCOL_TM_FRAME_LEN];
} FpgaProtocolContext;

static FpgaProtocolContext g_fpga;

#if defined(FPGA_PROTOCOL_HOST_TEST)
volatile uint32_t g_tk8710ResetCause;
static uint8_t g_hostLastMode;
static SatPayloadTelemetry g_hostTelemetry;
static uint8_t g_hostAppliedRateMode;
static uint32_t g_hostDcApplyCount;
static uint8_t g_hostDcAntenna[8U];
static int16_t g_hostDcI[8U];
static int16_t g_hostDcQ[8U];
static char g_hostLastRcLog[FPGA_RC_LOG_TEXT_LEN];
static uint32_t g_hostRcLogCount;
#else
extern volatile uint32 g_tk8710ResetCause;
#endif

#if defined(FPGA_PROTOCOL_HOST_TEST)
void FpgaProtocol_Log(const char *text)
{
    if (text == NULL)
    {
        g_hostLastRcLog[0] = '\0';
        return;
    }
    (void)snprintf(g_hostLastRcLog, sizeof(g_hostLastRcLog), "%s", text);
    g_hostRcLogCount++;
}
#else
#if defined(__TI_COMPILER_VERSION__)
#pragma WEAK(FpgaProtocol_Log)
void FpgaProtocol_Log(const char *text)
#elif defined(__GNUC__)
__attribute__((weak)) void FpgaProtocol_Log(const char *text)
#else
void FpgaProtocol_Log(const char *text)
#endif
{
    (void)text;
}
#endif

static uint8_t FpgaChecksum(const uint8_t *data, uint32_t start, uint32_t end)
{
    uint32_t i;
    uint8_t sum = 0U;

    for (i = start; i <= end; i++)
    {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static uint16_t FpgaReadBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static uint32_t FpgaReadBe32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           data[3];
}

static int FpgaIsUniformFrame(const uint8_t *frame, uint8_t value)
{
    uint32_t i;

    if (frame == NULL)
    {
        return 0;
    }
    for (i = 0U; i < FPGA_PROTOCOL_RC_FRAME_LEN; i++)
    {
        if (frame[i] != value)
        {
            return 0;
        }
    }
    return 1;
}

static int FpgaShouldSuppressRcLog(const uint8_t *frame)
{
    return ((frame == NULL) ||
            (FpgaIsUniformFrame(frame, 0x00U) != 0) ||
            (FpgaIsUniformFrame(frame, 0xFFU) != 0)) ? 1 : 0;
}

static void FpgaFormatRcFrameHex(const uint8_t *frame, char *text, uint32_t textSize)
{
    uint32_t i;
    uint32_t used = 0U;

    if ((text == NULL) || (textSize == 0U))
    {
        return;
    }
    text[0] = '\0';
    if (frame == NULL)
    {
        return;
    }
    for (i = 0U; i < FPGA_PROTOCOL_RC_FRAME_LEN; i++)
    {
        int written = snprintf(&text[used], (size_t)(textSize - used),
                               "%s%02X", (i == 0U) ? "" : " ", frame[i]);
        if ((written < 0) || ((uint32_t)written >= (textSize - used)))
        {
            text[textSize - 1U] = '\0';
            return;
        }
        used += (uint32_t)written;
    }
}

static void FpgaQueueRcLogText(const char *logText)
{
    if (logText == NULL)
    {
        g_fpga.pendingRcLog[0] = '\0';
        g_fpga.pendingRcLogValid = 0U;
        return;
    }
    (void)snprintf(g_fpga.pendingRcLog, sizeof(g_fpga.pendingRcLog), "%s", logText);
    g_fpga.pendingRcLogValid = 1U;
}

static void FpgaFlushPendingRcLog(void)
{
    if (g_fpga.pendingRcLogValid == 0U)
    {
        return;
    }
    FpgaProtocol_Log(g_fpga.pendingRcLog);
    g_fpga.pendingRcLog[0] = '\0';
    g_fpga.pendingRcLogValid = 0U;
}

static int FpgaIsKnownCommand(uint8_t command)
{
    switch ((FpgaCommand)command)
    {
    case FPGA_CMD_WORK_MODE:
    case FPGA_CMD_RATE:
    case FPGA_CMD_SLOT:
    case FPGA_CMD_TX_POWER:
    case FPGA_CMD_FREQ:
    case FPGA_CMD_RF_CHANNEL:
    case FPGA_CMD_RESET:
    case FPGA_CMD_FIRMWARE_UPGRADE:
    case FPGA_CMD_DATA_TRANSFER:
    case FPGA_CMD_WRITE_REG:
    case FPGA_CMD_READ_REG:
    case FPGA_CMD_ROLLBACK:
    case FPGA_CMD_UTC_TIME:
    case FPGA_CMD_DC_PARAMS:
        return 1;
    default:
        return 0;
    }
}

static int FpgaCommandLogsBeforeReset(uint8_t command)
{
    return ((command == FPGA_CMD_RESET) ||
            (command == FPGA_CMD_FIRMWARE_UPGRADE) ||
            (command == FPGA_CMD_ROLLBACK)) ? 1 : 0;
}

static uint8_t FpgaMapProtocolRateToTkRate(uint8_t rate)
{
    return (uint8_t)(rate + FPGA_TK_RATE_BASE);
}

static uint8_t FpgaMapTkRateToProtocolRate(uint8_t rate)
{
    if ((rate >= FPGA_TK_RATE_BASE) &&
        (rate <= (uint8_t)(FPGA_TK_RATE_BASE + FPGA_PROTOCOL_RATE_MAX)))
    {
        return (uint8_t)(rate - FPGA_TK_RATE_BASE);
    }
    return rate;
}

static void FpgaLogRcHeaderError(const uint8_t *frame)
{
    char frameText[FPGA_RC_FRAME_HEX_TEXT_LEN];
    char logText[FPGA_RC_LOG_TEXT_LEN];

    FpgaFormatRcFrameHex(frame, frameText, (uint32_t)sizeof(frameText));
    (void)snprintf(logText, sizeof(logText), "RC ERR header frame=%s\r\n", frameText);
    FpgaQueueRcLogText(logText);
}

static void FpgaLogRcChecksumError(const uint8_t *frame, uint8_t expected)
{
    char frameText[FPGA_RC_FRAME_HEX_TEXT_LEN];
    char logText[FPGA_RC_LOG_TEXT_LEN];

    FpgaFormatRcFrameHex(frame, frameText, (uint32_t)sizeof(frameText));
    (void)snprintf(logText, sizeof(logText),
                   "RC ERR checksum cmd=0x%02X expected=0x%02X actual=0x%02X frame=%s\r\n",
                   frame[2], expected, frame[9], frameText);
    FpgaQueueRcLogText(logText);
}

static void FpgaLogRcUnsupported(const uint8_t *frame)
{
    char frameText[FPGA_RC_FRAME_HEX_TEXT_LEN];
    char logText[FPGA_RC_LOG_TEXT_LEN];

    FpgaFormatRcFrameHex(frame, frameText, (uint32_t)sizeof(frameText));
    (void)snprintf(logText, sizeof(logText),
                   "RC ERR unsupported cmd=0x%02X frame=%s\r\n",
                   frame[2], frameText);
    FpgaQueueRcLogText(logText);
}

static void FpgaLogRcExecuteError(const uint8_t *frame)
{
    char frameText[FPGA_RC_FRAME_HEX_TEXT_LEN];
    char logText[FPGA_RC_LOG_TEXT_LEN];

    FpgaFormatRcFrameHex(frame, frameText, (uint32_t)sizeof(frameText));
    (void)snprintf(logText, sizeof(logText),
                   "RC ERR execute cmd=0x%02X result=%ld frame=%s\r\n",
                   frame[2], (long)g_fpga.lastResult, frameText);
    FpgaQueueRcLogText(logText);
}

static void FpgaLogRcRangeError(const uint8_t *frame, const char *field,
                                uint8_t value, uint8_t minValue, uint8_t maxValue)
{
    char frameText[FPGA_RC_FRAME_HEX_TEXT_LEN];
    char logText[FPGA_RC_LOG_TEXT_LEN];

    FpgaFormatRcFrameHex(frame, frameText, (uint32_t)sizeof(frameText));
    (void)snprintf(logText, sizeof(logText),
                   "RC ERR range cmd=0x%02X field=%s value=%u allowed=%u..%u frame=%s\r\n",
                   frame[2], field, value, minValue, maxValue, frameText);
    FpgaQueueRcLogText(logText);
}

static void FpgaLogRcOk(const uint8_t *frame, uint8_t immediate)
{
    char logText[FPGA_RC_LOG_TEXT_LEN];

    switch ((FpgaCommand)frame[2])
    {
    case FPGA_CMD_WORK_MODE:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_01 workMode=%u\r\n", frame[3]);
        break;
    case FPGA_CMD_RATE:
        (void)snprintf(logText, sizeof(logText),
                       "RC OK CMD_02 rate=%u mappedRate=%u\r\n",
                       frame[3], FpgaMapProtocolRateToTkRate(frame[3]));
        break;
    case FPGA_CMD_SLOT:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_03 slotConfig=%u\r\n", frame[3]);
        break;
    case FPGA_CMD_TX_POWER:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_04 txPower=%u\r\n", frame[3]);
        break;
    case FPGA_CMD_FREQ:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_05 freqHz=%lu\r\n",
                       (unsigned long)FpgaReadBe32(&frame[3]));
        break;
    case FPGA_CMD_RF_CHANNEL:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_06 rfMask=0x%02X\r\n", frame[3]);
        break;
    case FPGA_CMD_RESET:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_07 reset request\r\n");
        break;
    case FPGA_CMD_FIRMWARE_UPGRADE:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_08 firmwareUpgrade request\r\n");
        break;
    case FPGA_CMD_DATA_TRANSFER:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_09 dataTransfer start\r\n");
        break;
    case FPGA_CMD_WRITE_REG:
        (void)snprintf(logText, sizeof(logText),
                       "RC OK CMD_0A writeReg addr=0x%04X value=0x%08lX\r\n",
                       FpgaReadBe16(&frame[3]), (unsigned long)FpgaReadBe32(&frame[5]));
        break;
    case FPGA_CMD_READ_REG:
        (void)snprintf(logText, sizeof(logText),
                       "RC OK CMD_0B readReg device=0x%04X addr=0x%04X value=0x%08lX\r\n",
                       FpgaReadBe16(&frame[3]), FpgaReadBe16(&frame[5]),
                       (unsigned long)g_fpga.lastRegValue);
        break;
    case FPGA_CMD_ROLLBACK:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_0C rollback request\r\n");
        break;
    case FPGA_CMD_UTC_TIME:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_0D utcSeconds=%lu\r\n",
                       (unsigned long)FpgaReadBe32(&frame[3]));
        break;
    case FPGA_CMD_DC_PARAMS:
        (void)snprintf(logText, sizeof(logText),
                       "RC OK CMD_0E dc antenna=%u iDc=%d qDc=%d\r\n",
                       frame[3], (int)((int16_t)FpgaReadBe16(&frame[4])),
                       (int)((int16_t)FpgaReadBe16(&frame[6])));
        break;
    default:
        (void)snprintf(logText, sizeof(logText), "RC OK CMD_%02X\r\n", frame[2]);
        break;
    }
    if (immediate != 0U)
    {
        FpgaProtocol_Log(logText);
    }
    else
    {
        FpgaQueueRcLogText(logText);
    }
}

static void FpgaLogRcDispatchResult(const uint8_t *frame, int result)
{
    if (result == FPGA_DISPATCH_RANGE_ERROR)
    {
        return;
    }
    if (result == 0)
    {
        if (FpgaCommandLogsBeforeReset(frame[2]) != 0)
        {
            return;
        }
        FpgaLogRcOk(frame, 0U);
    }
    else if (FpgaIsKnownCommand(frame[2]) == 0)
    {
        FpgaLogRcUnsupported(frame);
    }
    else
    {
        FpgaLogRcExecuteError(frame);
    }
}

static uint8_t FpgaIsLeapYear(uint32_t year)
{
    return (uint8_t)((((year % 4U) == 0U) && ((year % 100U) != 0U)) ||
                     ((year % 400U) == 0U));
}

static uint32_t FpgaDaysInMonth(uint32_t year, uint32_t month)
{
    static const uint8_t daysByMonth[12] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if ((month == 2U) && (FpgaIsLeapYear(year) != 0U))
    {
        return 29U;
    }
    return daysByMonth[month - 1U];
}

void FpgaProtocol_FormatUtcTime(uint32_t seconds, char *text, uint32_t textSize)
{
    uint32_t days = seconds / 86400U;
    uint32_t rem = seconds % 86400U;
    uint32_t year = 2009U;
    uint32_t month = 1U;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t yearDays;
    uint32_t monthDays;

    if ((text == NULL) || (textSize == 0U))
    {
        return;
    }

    for (;;)
    {
        yearDays = (FpgaIsLeapYear(year) != 0U) ? 366U : 365U;
        if (days < yearDays)
        {
            break;
        }
        days -= yearDays;
        year++;
    }

    for (;;)
    {
        monthDays = FpgaDaysInMonth(year, month);
        if (days < monthDays)
        {
            break;
        }
        days -= monthDays;
        month++;
    }

    day = days + 1U;
    hour = rem / 3600U;
    rem %= 3600U;
    minute = rem / 60U;
    rem %= 60U;

    (void)snprintf(text, (size_t)textSize, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu",
                   (unsigned long)year,
                   (unsigned long)month,
                   (unsigned long)day,
                   (unsigned long)hour,
                   (unsigned long)minute,
                   (unsigned long)rem);
}

static void FpgaTriggerTms570Reset(void)
{
#if !defined(FPGA_PROTOCOL_HOST_TEST)
    systemREG1->SYSECR = (uint32_t)(0x10U << 14U);
    for (;;)
    {
    }
#endif
}

static void FpgaWriteBe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static void FpgaWriteBe32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static void FpgaBuildDcCommandFrame(uint8_t *frame, uint8_t antenna,
                                    int16_t iDc, int16_t qDc)
{
    (void)memset(frame, 0, FPGA_PROTOCOL_RC_FRAME_LEN);
    frame[0] = FPGA_RC_SYNC0;
    frame[1] = FPGA_RC_SYNC1;
    frame[2] = FPGA_CMD_DC_PARAMS;
    frame[3] = antenna;
    FpgaWriteBe16(&frame[4], (uint16_t)iDc);
    FpgaWriteBe16(&frame[6], (uint16_t)qDc);
    frame[9] = FpgaChecksum(frame, 2U, 8U);
}

static int16_t FpgaNoiseToTelemetry(float noiseDbmHz)
{
    float clamped = noiseDbmHz;
    int32_t value;

    if (clamped < -255.0F)
    {
        clamped = -255.0F;
    }
    else if (clamped > 0.0F)
    {
        clamped = 0.0F;
    }
    value = (int32_t)(clamped * 128.0F);
    return (int16_t)value;
}

static uint8_t FpgaClassifyResetType(uint32_t resetCause)
{
    if ((resetCause & FPGA_RESET_CAUSE_WATCHDOG) != 0U)
    {
        return FPGA_RESET_TYPE_WATCHDOG;
    }
    if ((resetCause == 0U) || ((resetCause & ~FPGA_RESET_CAUSE_NORMAL_MASK) == 0U))
    {
        return FPGA_RESET_TYPE_NORMAL;
    }
    return FPGA_RESET_TYPE_OTHER;
}

static void FpgaLoadAndIncrementResetCount(void)
{
    uint32_t count = 0U;

    (void)FpgaParamStore_LoadResetCount(&count);
    if (count != 0xFFFFFFFFUL)
    {
        count++;
    }
    g_fpga.resetCount = count;
    (void)FpgaParamStore_SaveResetCount(count);
}

static void FpgaClearCompletedBootFlag(void)
{
    uint8_t flag = 0xFFU;

    if (FpgaParamStore_LoadBootFlag(&flag) != 0)
    {
        return;
    }
    if ((flag != FPGA_BOOT_FLAG_FIRMWARE_UPGRADE) &&
        (flag != FPGA_BOOT_FLAG_ROLLBACK))
    {
        return;
    }

    if (FpgaParamStore_SaveBootFlag(FPGA_BOOT_FLAG_CLEAR) != 0)
    {
        g_fpga.lastResult = FPGA_BOOT_FLAG_STORE_RESULT_FAIL;
    }
}

static void FpgaInitDefaultParams(SatPayloadWorkParams *params)
{
    (void)memset(params, 0, sizeof(*params));
    params->centerFreqHz = DEFAULT_FREQ;
    params->rateCount = DEFAULT_RATE_COUNT;
    params->rates[0].rateMode = DEFAULT_RATE_MODE;
    params->rates[0].slots[0].byteLen = DEFAULT_SLOT0_LENGTH;
    params->rates[0].slots[0].daM = DEFAULT_SLOT0_DAM;
    params->rates[0].slots[1].byteLen = DEFAULT_SLOT1_LENGTH;
    params->rates[0].slots[1].daM = DEFAULT_SLOT1_DAM;
    params->rates[0].slots[2].byteLen = DEFAULT_SLOT2_LENGTH;
    params->rates[0].slots[2].daM = DEFAULT_SLOT2_DAM;
    params->rates[0].slots[3].byteLen = DEFAULT_SLOT3_LENGTH;
    params->rates[0].slots[3].daM = DEFAULT_SLOT3_DAM;
    params->rxGain = DEFAULT_RX_GAIN;
    params->txGain = DEFAULT_TX_GAIN;
    params->antennaMask = DEFAULT_ANTENNA_MASK;
    params->rfMask = DEFAULT_RF_MASK;
    params->bcnBits = DEFAULT_BCN_BITS;
    params->txBcnAntennaMask = params->antennaMask;
    params->mdAgc = DEFAULT_MD_AGC;
    params->maxFrameCount = DEFAULT_MAX_FRAME_COUNT;
    params->sweepStartFreqHz = 0U;
    params->sweepEndFreqHz = 0U;
    params->sweepMode = DEFAULT_SWEEP_MODE;
    params->toneFreq = DEFAULT_TONE_FREQ;
    params->toneGain = DEFAULT_TONE_GAIN;
    params->acmCalibCount = DEFAULT_ACM_CALIB_COUNT;
    params->acmSnrThreshold = DEFAULT_ACM_SNR_THRESHOLD;
}

static void FpgaMakeTkWorkParams(const SatPayloadWorkParams *protocolParams,
                                 SatPayloadWorkParams *tkParams)
{
    *tkParams = *protocolParams;
    if (tkParams->rateCount != 0U)
    {
        tkParams->rates[0].rateMode = FpgaMapProtocolRateToTkRate(protocolParams->rates[0].rateMode);
    }
}

static void FpgaMakeStoredParams(FpgaStoredParams *stored)
{
    stored->workMode = g_fpga.activeMode;
    stored->rateMode = g_fpga.pendingParams.rates[0].rateMode;
    stored->slotConfig = g_fpga.slotConfig;
    stored->txPower = g_fpga.txPower;
    stored->centerFreqHz = g_fpga.pendingParams.centerFreqHz;
    stored->rfMask = g_fpga.pendingParams.rfMask;
    stored->dcValidMask = g_fpga.dcValidMask;
    (void)memcpy(stored->dcI, g_fpga.dcI, sizeof(stored->dcI));
    (void)memcpy(stored->dcQ, g_fpga.dcQ, sizeof(stored->dcQ));
}

static void FpgaApplyStoredParamsToContext(const FpgaStoredParams *stored)
{
    FpgaInitDefaultParams(&g_fpga.pendingParams);
    g_fpga.activeMode = stored->workMode;
    g_fpga.slotConfig = stored->slotConfig;
    g_fpga.txPower = stored->txPower;
    g_fpga.pendingParams.rates[0].rateMode = FpgaMapTkRateToProtocolRate(stored->rateMode);
    g_fpga.pendingParams.centerFreqHz = stored->centerFreqHz;
    g_fpga.pendingParams.sweepStartFreqHz = 0U;
    g_fpga.pendingParams.sweepEndFreqHz = 0U;
    g_fpga.pendingParams.rfMask = stored->rfMask;
    g_fpga.pendingParamsValid = 1U;
    g_fpga.dcValidMask = stored->dcValidMask;
    (void)memcpy(g_fpga.dcI, stored->dcI, sizeof(g_fpga.dcI));
    (void)memcpy(g_fpga.dcQ, stored->dcQ, sizeof(g_fpga.dcQ));
}

static int FpgaSaveCurrentParams(void)
{
    FpgaStoredParams stored;

    FpgaMakeStoredParams(&stored);
    if (FpgaParamStore_Save(&stored) != 0)
    {
        g_fpga.lastResult = FPGA_PARAM_STORE_RESULT_FAIL;
        return -1;
    }
    return 0;
}

static int FpgaSaveDcParams(uint8_t antenna, int16_t iDc, int16_t qDc)
{
    if (antenna >= 8U)
    {
        g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
        return -1;
    }

    g_fpga.dcI[antenna] = iDc;
    g_fpga.dcQ[antenna] = qDc;
    g_fpga.dcValidMask = (uint8_t)(g_fpga.dcValidMask | (uint8_t)(1U << antenna));
    return FpgaSaveCurrentParams();
}

static int FpgaSaveBootFlagAndReset(uint8_t flag, const uint8_t *frame)
{
    if (FpgaParamStore_SaveBootFlag(flag) != 0)
    {
        g_fpga.lastResult = FPGA_BOOT_FLAG_STORE_RESULT_FAIL;
        return -1;
    }
    if (frame != NULL)
    {
        FpgaLogRcOk(frame, 1U);
    }
    FpgaTriggerTms570Reset();
    return 0;
}

static void FpgaEnsurePendingParams(void)
{
    SatPayloadTelemetry telemetry;

    if (g_fpga.pendingParamsValid != 0U)
    {
        return;
    }

    (void)memset(&telemetry, 0, sizeof(telemetry));
    SatPayloadApp_GetTelemetry(&telemetry);
    if ((telemetry.activeParams.centerFreqHz != 0U) &&
        (telemetry.activeParams.rateCount != 0U))
    {
        g_fpga.pendingParams = telemetry.activeParams;
        g_fpga.pendingParams.rates[0].rateMode =
            FpgaMapTkRateToProtocolRate(telemetry.activeParams.rates[0].rateMode);
    }
    else
    {
        FpgaInitDefaultParams(&g_fpga.pendingParams);
    }
    g_fpga.pendingParamsValid = 1U;
}

static SatPayloadResult FpgaApplyPendingParams(void)
{
    SatPayloadWorkParams tkParams;
    SatPayloadResult result;

    FpgaEnsurePendingParams();
    g_fpga.pendingParams.sweepStartFreqHz = 0U;
    g_fpga.pendingParams.sweepEndFreqHz = 0U;
    FpgaMakeTkWorkParams(&g_fpga.pendingParams, &tkParams);
    result = SatPayloadApp_SetWorkParams(&tkParams);
    g_fpga.lastResult = result;
    return result;
}

static SatPayloadResult FpgaHandleTelecommand(SatPayloadTelecommand *request,
                                              SatPayloadTelecommandResponse *response)
{
    SatPayloadResult result;

    request->requestId = g_fpga.rxFrameCount;
    result = SatPayloadApp_HandleTelecommand(request, response);
    g_fpga.lastResult = result;
    return result;
}

static int FpgaHandleWriteRegisterCommand(const uint8_t *frame)
{
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;

    (void)memset(&request, 0, sizeof(request));
    (void)memset(&response, 0, sizeof(response));
    request.commandId = SAT_PAYLOAD_TC_WRITE_REG;
    request.payload.reg.address = FpgaReadBe16(&frame[3]);
    request.payload.reg.value = FpgaReadBe32(&frame[5]);
    return (FpgaHandleTelecommand(&request, &response) >= SAT_PAYLOAD_OK) ? 0 : -1;
}

static int FpgaHandleReadRegisterCommand(const uint8_t *frame)
{
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;

    (void)memset(&request, 0, sizeof(request));
    (void)memset(&response, 0, sizeof(response));
    request.commandId = SAT_PAYLOAD_TC_READ_REG;
    g_fpga.lastRegDevice = FpgaReadBe16(&frame[3]);
    request.payload.reg.address = FpgaReadBe16(&frame[5]);
    g_fpga.lastRegAddress = request.payload.reg.address;
    if (FpgaHandleTelecommand(&request, &response) < SAT_PAYLOAD_OK)
    {
        return -1;
    }
    g_fpga.lastRegValue = response.value;
    return 0;
}

static int FpgaHandleUtcTimeCommand(const uint8_t *frame)
{
    g_fpga.utcSeconds = FpgaReadBe32(&frame[3]);
    DataTransfer_SetUtcSeconds(g_fpga.utcSeconds);
    g_fpga.lastResult = SAT_PAYLOAD_OK;
    return 0;
}

static int FpgaHandleDcParamsCommand(const uint8_t *frame)
{
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    uint8_t antenna = frame[3];
    int16_t iDc = (int16_t)FpgaReadBe16(&frame[4]);
    int16_t qDc = (int16_t)FpgaReadBe16(&frame[6]);

    (void)memset(&request, 0, sizeof(request));
    (void)memset(&response, 0, sizeof(response));
    request.commandId = SAT_PAYLOAD_TC_SET_RF_TX_DC;
    request.payload.rfTxDc.antenna = antenna;
    request.payload.rfTxDc.iDc = iDc;
    request.payload.rfTxDc.qDc = qDc;
    if (FpgaHandleTelecommand(&request, &response) < SAT_PAYLOAD_OK)
    {
        return -1;
    }
    if (g_fpga.restoringDcParams != 0U)
    {
        return 0;
    }
    return FpgaSaveDcParams(antenna, iDc, qDc);
}

static void FpgaRestoreStoredDcParams(void)
{
    uint8_t frame[FPGA_PROTOCOL_RC_FRAME_LEN];
    uint8_t antenna;

#if defined(FPGA_PROTOCOL_HOST_TEST)
    g_hostDcApplyCount = 0U;
#endif
    g_fpga.restoringDcParams = 1U;
    for (antenna = 0U; antenna < 8U; antenna++)
    {
        if ((g_fpga.dcValidMask & (uint8_t)(1U << antenna)) != 0U)
        {
            FpgaBuildDcCommandFrame(frame, antenna,
                                    g_fpga.dcI[antenna],
                                    g_fpga.dcQ[antenna]);
            (void)FpgaHandleDcParamsCommand(frame);
        }
    }
    g_fpga.restoringDcParams = 0U;
}

static int FpgaDispatchCommand(const uint8_t *frame)
{
#if !defined(FPGA_PROTOCOL_SELF_TEST)
    SatPayloadResult result;
#endif
    uint8_t command = frame[2];

#if defined(FPGA_PROTOCOL_SELF_TEST)
    switch (command)
    {
    case FPGA_CMD_WORK_MODE:
        if (frame[3] > FPGA_WORK_MODE_MAX)
        {
            g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
            FpgaLogRcRangeError(frame, "workMode", frame[3], 0U, FPGA_WORK_MODE_MAX);
            return FPGA_DISPATCH_RANGE_ERROR;
        }
        g_fpga.activeMode = frame[3];
        (void)SatPayloadApp_SetWorkMode(g_fpga.activeMode);
        g_fpga.lastResult = SAT_PAYLOAD_OK;
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RATE:
        if (frame[3] > FPGA_PROTOCOL_RATE_MAX)
        {
            g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
            FpgaLogRcRangeError(frame, "rate", frame[3], 0U, FPGA_PROTOCOL_RATE_MAX);
            return FPGA_DISPATCH_RANGE_ERROR;
        }
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.rateCount = 1U;
        g_fpga.pendingParams.rates[0].rateMode = frame[3];
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_SLOT:
        g_fpga.slotConfig = frame[3];
        g_fpga.lastResult = SAT_PAYLOAD_OK;
        return FpgaSaveCurrentParams();

    case FPGA_CMD_TX_POWER:
        g_fpga.txPower = frame[3];
        g_fpga.lastResult = SAT_PAYLOAD_OK;
        return FpgaSaveCurrentParams();

    case FPGA_CMD_FREQ:
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.centerFreqHz = FpgaReadBe32(&frame[3]);
        g_fpga.pendingParams.sweepStartFreqHz = 0U;
        g_fpga.pendingParams.sweepEndFreqHz = 0U;
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RF_CHANNEL:
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.rfMask = frame[3];
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RESET:
        FpgaLogRcOk(frame, 1U);
        FpgaTriggerTms570Reset();
        return 0;

    case FPGA_CMD_FIRMWARE_UPGRADE:
        return FpgaSaveBootFlagAndReset(FPGA_BOOT_FLAG_FIRMWARE_UPGRADE, frame);

    case FPGA_CMD_DATA_TRANSFER:
        g_fpga.lastResult = (DataTransfer_StartTransmit() == 0) ? SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
        return (g_fpga.lastResult >= SAT_PAYLOAD_OK) ? 0 : -1;

    case FPGA_CMD_WRITE_REG:
        return FpgaHandleWriteRegisterCommand(frame);

    case FPGA_CMD_READ_REG:
        return FpgaHandleReadRegisterCommand(frame);

    case FPGA_CMD_ROLLBACK:
        return FpgaSaveBootFlagAndReset(FPGA_BOOT_FLAG_ROLLBACK, frame);

    case FPGA_CMD_DC_PARAMS:
        return FpgaHandleDcParamsCommand(frame);

    case FPGA_CMD_UTC_TIME:
        return FpgaHandleUtcTimeCommand(frame);

    default:
        g_fpga.unsupportedCount++;
        g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
        return -1;
    }
#else
    switch (command)
    {
    case FPGA_CMD_WORK_MODE:
        if (frame[3] > FPGA_WORK_MODE_MAX)
        {
            g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
            FpgaLogRcRangeError(frame, "workMode", frame[3], 0U, FPGA_WORK_MODE_MAX);
            return FPGA_DISPATCH_RANGE_ERROR;
        }
        result = SatPayloadApp_SetWorkMode(frame[3]);
        g_fpga.lastResult = result;
        if (result < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        g_fpga.activeMode = frame[3];
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RATE:
        if (frame[3] > FPGA_PROTOCOL_RATE_MAX)
        {
            g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
            FpgaLogRcRangeError(frame, "rate", frame[3], 0U, FPGA_PROTOCOL_RATE_MAX);
            return FPGA_DISPATCH_RANGE_ERROR;
        }
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.rateCount = 1U;
        g_fpga.pendingParams.rates[0].rateMode = frame[3];
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_SLOT:
        g_fpga.slotConfig = frame[3];
        g_fpga.lastResult = SAT_PAYLOAD_OK;
        return FpgaSaveCurrentParams();

    case FPGA_CMD_TX_POWER:
        g_fpga.txPower = frame[3];
        g_fpga.lastResult = SAT_PAYLOAD_OK;
        return FpgaSaveCurrentParams();

    case FPGA_CMD_FREQ:
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.centerFreqHz = FpgaReadBe32(&frame[3]);
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RF_CHANNEL:
        FpgaEnsurePendingParams();
        g_fpga.pendingParams.rfMask = frame[3];
        if (FpgaApplyPendingParams() < SAT_PAYLOAD_OK)
        {
            return -1;
        }
        return FpgaSaveCurrentParams();

    case FPGA_CMD_RESET:
        FpgaLogRcOk(frame, 1U);
        FpgaTriggerTms570Reset();
        return 0;

    case FPGA_CMD_WRITE_REG:
        return FpgaHandleWriteRegisterCommand(frame);

    case FPGA_CMD_READ_REG:
        return FpgaHandleReadRegisterCommand(frame);

    case FPGA_CMD_FIRMWARE_UPGRADE:
        return FpgaSaveBootFlagAndReset(FPGA_BOOT_FLAG_FIRMWARE_UPGRADE, frame);

    case FPGA_CMD_DATA_TRANSFER:
        g_fpga.lastResult = (DataTransfer_StartTransmit() == 0) ? SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
        return (g_fpga.lastResult >= SAT_PAYLOAD_OK) ? 0 : -1;

    case FPGA_CMD_DC_PARAMS:
        return FpgaHandleDcParamsCommand(frame);

    case FPGA_CMD_UTC_TIME:
        return FpgaHandleUtcTimeCommand(frame);

    case FPGA_CMD_ROLLBACK:
        return FpgaSaveBootFlagAndReset(FPGA_BOOT_FLAG_ROLLBACK, frame);

    default:
        g_fpga.unsupportedCount++;
        g_fpga.lastResult = SAT_PAYLOAD_ERR_PARAM;
        return -1;
    }
#endif
}

static int FpgaHandleRxFrame(const uint8_t *frame)
{
    uint8_t expectedChecksum;
    int result;

    if (frame != NULL)
    {
        g_fpga.physicalRxFrameCount++;
        (void)memcpy(g_fpga.lastPhysicalRxFrame, frame, FPGA_PROTOCOL_RC_FRAME_LEN);
    }

    if ((frame == NULL) ||
        (frame[0] != FPGA_RC_SYNC0) ||
        (frame[1] != FPGA_RC_SYNC1))
    {
        if (FpgaShouldSuppressRcLog(frame) == 0)
        {
            FpgaLogRcHeaderError(frame);
        }
        return 0;
    }

    expectedChecksum = FpgaChecksum(frame, 2U, 8U);
    if (expectedChecksum != frame[9])
    {
        FpgaLogRcChecksumError(frame, expectedChecksum);
        return -1;
    }

    g_fpga.rxFrameCount++;
    g_fpga.lastCommandId = frame[2];
    result = FpgaDispatchCommand(frame);
    FpgaLogRcDispatchResult(frame, result);
    return result;
}

void FpgaProtocol_GetSnapshot(FpgaProtocolSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    (void)memset(snapshot, 0, sizeof(*snapshot));
    snapshot->rxFrameCount = g_fpga.rxFrameCount;
    snapshot->physicalRxFrameCount = g_fpga.physicalRxFrameCount;
    snapshot->rxErrorCount = g_fpga.rxErrorCount;
    snapshot->unsupportedCommandCount = g_fpga.unsupportedCount;
    snapshot->lastCommandId = g_fpga.lastCommandId;
    (void)memcpy(snapshot->lastPhysicalRxFrame, g_fpga.lastPhysicalRxFrame,
                 FPGA_PROTOCOL_RC_FRAME_LEN);
    snapshot->workMode = g_fpga.activeMode;
    snapshot->rateMode = g_fpga.pendingParams.rates[0].rateMode;
    snapshot->slotConfig = g_fpga.slotConfig;
    snapshot->txPower = g_fpga.txPower;
    snapshot->centerFreqHz = g_fpga.pendingParams.centerFreqHz;
    snapshot->rfMask = g_fpga.pendingParams.rfMask;
    snapshot->lastRegDevice = g_fpga.lastRegDevice;
    snapshot->lastRegAddress = g_fpga.lastRegAddress;
    snapshot->lastRegValue = g_fpga.lastRegValue;
    snapshot->utcSeconds = g_fpga.utcSeconds;
    (void)FpgaParamStore_LoadBootFlag(&snapshot->bootFlag);
    snapshot->resetCount = g_fpga.resetCount;
    snapshot->resetType = g_fpga.resetType;
}

static void FpgaBuildTelemetry(uint8_t *frame)
{
    SatPayloadTelemetry telemetry;
    uint32_t heapPercent;
    uint32_t antenna;

    (void)memset(&telemetry, 0, sizeof(telemetry));
    SatPayloadApp_GetTelemetry(&telemetry);
#if defined(FPGA_PROTOCOL_SELF_TEST)
    telemetry.activeMode = g_fpga.activeMode;
    telemetry.activeParams = g_fpga.pendingParams;
#endif
    (void)memset(frame, 0, FPGA_PROTOCOL_TM_FRAME_LEN);

    frame[0] = FPGA_TM_SYNC0;
    frame[1] = FPGA_TM_SYNC1;
    for (antenna = 0U; antenna < 8U; antenna++)
    {
        FpgaWriteBe16(&frame[2U + (antenna * 2U)],
                      (uint16_t)FpgaNoiseToTelemetry(telemetry.capture.noiseDbmHz[antenna]));
    }
    frame[20] = telemetry.activeMode;
    frame[21] = FpgaMapTkRateToProtocolRate(telemetry.activeParams.rates[0].rateMode);
    frame[22] = g_fpga.slotConfig;
    frame[23] = g_fpga.txPower;
    FpgaWriteBe32(&frame[24], telemetry.activeParams.centerFreqHz);
    frame[28] = telemetry.activeParams.rfMask;
    frame[29] = FPGA_PROTOCOL_VERSION_MAJOR;
    frame[30] = FPGA_PROTOCOL_VERSION_MINOR;
    frame[31] = FPGA_PROTOCOL_VERSION_PATCH;
    FpgaWriteBe32(&frame[32], telemetry.uptimeMs / 1000U);
    FpgaWriteBe32(&frame[36], telemetry.rxCount);
    FpgaWriteBe32(&frame[40], telemetry.txCount);
    frame[44] = (g_fpga.resetCount > 255U) ? 255U : (uint8_t)g_fpga.resetCount;
    frame[45] = g_fpga.resetType;

    heapPercent = 0U;
    if (telemetry.heapPeak != 0U)
    {
        heapPercent = (telemetry.heapUsed * 100U) / telemetry.heapPeak;
        if (heapPercent > 100U)
        {
            heapPercent = 100U;
        }
    }
    frame[46] = (uint8_t)heapPercent;
    frame[47] = 0U;
    frame[48] = 0U;

    FpgaWriteBe16(&frame[61], (uint16_t)telemetry.gpioIrqCount);
    FpgaWriteBe16(&frame[63], g_fpga.lastRegDevice);
    FpgaWriteBe16(&frame[65], g_fpga.lastRegAddress);
    FpgaWriteBe32(&frame[67], g_fpga.lastRegValue);

    for (antenna = 0U; antenna < 8U; antenna++)
    {
        FpgaWriteBe32(&frame[71U + (antenna * 8U)],
                      telemetry.acmResult.iFactor[antenna]);
        FpgaWriteBe32(&frame[75U + (antenna * 8U)],
                      telemetry.acmResult.qFactor[antenna]);
    }

    frame[143] = FpgaChecksum(frame, 2U, 142U);
}

static void FpgaBuildTelemetryPage(const uint8_t *frame, uint8_t *page, uint8_t pageIndex)
{
    uint32_t offset = (uint32_t)pageIndex * FPGA_PROTOCOL_SPI_PAGE_LEN;
    uint32_t copyLen = 0U;

    (void)memset(page, 0, FPGA_PROTOCOL_SPI_PAGE_LEN);
    if (offset < FPGA_PROTOCOL_TM_FRAME_LEN)
    {
        copyLen = FPGA_PROTOCOL_TM_FRAME_LEN - offset;
        if (copyLen > FPGA_PROTOCOL_SPI_PAGE_LEN)
        {
            copyLen = FPGA_PROTOCOL_SPI_PAGE_LEN;
        }
        (void)memcpy(page, &frame[offset], copyLen);
    }
}

static void FpgaQueueTelemetryBurst(void)
{
    FpgaBuildTelemetry(g_fpga.txFrame);
    g_fpga.txPageIndex = 0U;
    g_fpga.txBurstPending = 1U;
}

static void FpgaBuildCurrentTxPage(uint8_t *page)
{
    (void)memset(page, 0, FPGA_PROTOCOL_SPI_PAGE_LEN);
    if (g_fpga.txBurstActive == 0U)
    {
        if (g_fpga.txBurstPending == 0U)
        {
            return;
        }
        g_fpga.txBurstPending = 0U;
        g_fpga.txBurstActive = 1U;
        g_fpga.txPageIndex = 0U;
    }

    FpgaBuildTelemetryPage(g_fpga.txFrame, page, g_fpga.txPageIndex);
    if (g_fpga.txPageIndex == 0U)
    {
        g_fpga.txPageIndex = 1U;
    }
    else
    {
        g_fpga.txPageIndex = 0U;
        g_fpga.txBurstActive = 0U;
    }
}

#if !defined(FPGA_PROTOCOL_HOST_TEST)
static void FpgaWriteSpiTxRam(void)
{
    uint8_t page[FPGA_PROTOCOL_SPI_PAGE_LEN];
    uint32_t i;

    FpgaBuildCurrentTxPage(page);
    for (i = 0U; i < FPGA_PROTOCOL_SPI_PAGE_LEN; i++)
    {
        FPGA_MIBSPI_RAM->tx[i].data = page[i];
    }
}

static void FpgaArmTransfer(void)
{
    FPGA_MIBSPI_REG->TGINTFLG = (FPGA_MIBSPI_REG->TGINTFLG & 0x0000FFFFU) |
                                ((uint32_t)1U << (16U + FPGA_SPI_GROUP));
    mibspiTransfer(FPGA_MIBSPI_REG, FPGA_SPI_GROUP);
}

static void FpgaRefreshTelemetry(void)
{
    FpgaQueueTelemetryBurst();
}

static void FpgaConfigMibspi3Slave(void)
{
    uint32_t i;

    FPGA_MIBSPI_REG->GCR1 &= 0xFEFFFFFFU;
    FPGA_MIBSPI_REG->GCR0 = 0U;
    FPGA_MIBSPI_REG->GCR0 = 1U;
    FPGA_MIBSPI_REG->MIBSPIE = 1U;
    FPGA_MIBSPI_REG->GCR1 = (FPGA_MIBSPI_REG->GCR1 & 0xFFFFFFFCU);
    FPGA_MIBSPI_REG->INT0 = 0U;
    FPGA_MIBSPI_REG->DELAY = 0U;
    FPGA_MIBSPI_REG->FMT0 = (uint32_t)((uint32_t)0U << 24U) |
                            (uint32_t)((uint32_t)0U << 23U) |
                            (uint32_t)((uint32_t)0U << 22U) |
                            (uint32_t)((uint32_t)0U << 21U) |
                            (uint32_t)((uint32_t)0U << 20U) |
                            (uint32_t)((uint32_t)0U << 17U) |
                            (uint32_t)((uint32_t)1U << 16U) |
                            (uint32_t)((uint32_t)79U << 8U) |
                            (uint32_t)8U;
    FPGA_MIBSPI_REG->FMT1 = FPGA_MIBSPI_REG->FMT0;
    FPGA_MIBSPI_REG->FMT2 = FPGA_MIBSPI_REG->FMT0;
    FPGA_MIBSPI_REG->FMT3 = FPGA_MIBSPI_REG->FMT0;
    FPGA_MIBSPI_REG->DEF = CS_NONE;

    while ((FPGA_MIBSPI_REG->FLG & 0x01000000U) != 0U)
    {
    }
    FPGA_MIBSPI_REG->UERRCTRL = (FPGA_MIBSPI_REG->UERRCTRL & 0xFFFFFFF0U) | 0x00000005U;

    FPGA_MIBSPI_REG->TGCTRL[0U] = (uint32_t)((uint32_t)1U << 30U) |
                                  (uint32_t)((uint32_t)0U << 29U) |
                                  (uint32_t)((uint32_t)TRG_ALWAYS << 20U) |
                                  (uint32_t)((uint32_t)TRG_DISABLED << 16U) |
                                  (uint32_t)0U;
    FPGA_MIBSPI_REG->TGCTRL[1U] = (uint32_t)FPGA_PROTOCOL_SPI_PAGE_LEN << 8U;
    for (i = 2U; i < 16U; i++)
    {
        FPGA_MIBSPI_REG->TGCTRL[i] = (uint32_t)FPGA_PROTOCOL_SPI_PAGE_LEN << 8U;
    }
    FPGA_MIBSPI_REG->LTGPEND = (FPGA_MIBSPI_REG->LTGPEND & 0xFFFF00FFU) |
                               (uint32_t)((FPGA_PROTOCOL_SPI_PAGE_LEN - 1U) << 8U);

    for (i = 0U; i < FPGA_PROTOCOL_SPI_PAGE_LEN; i++)
    {
        FPGA_MIBSPI_RAM->tx[i].control = (uint16)((uint16)4U << 13U) |
                                         (uint16)(((i < (FPGA_PROTOCOL_SPI_PAGE_LEN - 1U)) ? 1U : 0U) << 12U) |
                                         (uint16)((uint16)0U << 10U) |
                                         (uint16)((uint16)0U << 11U) |
                                         (uint16)((uint16)0U << 8U) |
                                         ((uint16)(~((uint16)0xFFU ^ (uint16)CS_0)) &
                                          (uint16)0x00FFU);
        FPGA_MIBSPI_RAM->rx[i].flags = 0U;
        FPGA_MIBSPI_RAM->rx[i].data = 0U;
    }

    FPGA_MIBSPI_REG->LVL = 0U;
    FPGA_MIBSPI_REG->FLG |= 0xFFFFU;
    FPGA_MIBSPI_REG->INT0 = 0U;

    FPGA_MIBSPI_REG->PC3 = (uint32_t)((uint32_t)1U << 0U) |
                           (uint32_t)((uint32_t)1U << 1U) |
                           (uint32_t)((uint32_t)1U << 2U) |
                           (uint32_t)((uint32_t)1U << 3U) |
                           (uint32_t)((uint32_t)1U << 4U) |
                           (uint32_t)((uint32_t)1U << 5U) |
                           (uint32_t)((uint32_t)0U << 8U) |
                           (uint32_t)((uint32_t)0U << 9U) |
                           (uint32_t)((uint32_t)0U << 10U) |
                           (uint32_t)((uint32_t)0U << 11U);
    FPGA_MIBSPI_REG->PC1 = (uint32_t)((uint32_t)0U << 0U) |
                           (uint32_t)((uint32_t)0U << 8U) |
                           (uint32_t)((uint32_t)0U << 9U) |
                           (uint32_t)((uint32_t)0U << 10U) |
                           (uint32_t)((uint32_t)1U << 11U);
    FPGA_MIBSPI_REG->PC6 = 0U;
    FPGA_MIBSPI_REG->PC8 = (uint32_t)((uint32_t)1U << 0U) |
                           (uint32_t)((uint32_t)1U << 8U) |
                           (uint32_t)((uint32_t)1U << 9U) |
                           (uint32_t)((uint32_t)1U << 10U) |
                           (uint32_t)((uint32_t)1U << 11U);
    FPGA_MIBSPI_REG->PC7 = 0U;
    FPGA_MIBSPI_REG->PC0 = (uint32_t)((uint32_t)1U << 0U) |
                           (uint32_t)((uint32_t)1U << 9U) |
                           (uint32_t)((uint32_t)1U << 10U) |
                           (uint32_t)((uint32_t)1U << 11U);
    FPGA_MIBSPI_REG->GCR1 = (FPGA_MIBSPI_REG->GCR1 & 0xFEFFFFFFU) | 0x01000000U;
}
#endif

void FpgaProtocol_Init(void)
{
    FpgaStoredParams stored;
    uint8_t loadedStoredParams = 0U;

    (void)memset(&g_fpga, 0, sizeof(g_fpga));
    g_fpga.slotConfig = DEFAULT_SLOT_CONFIG;
    g_fpga.txPower = DEFAULT_TX_POWER;
    if (FpgaParamStore_Load(&stored) == 0)
    {
        FpgaApplyStoredParamsToContext(&stored);
        loadedStoredParams = 1U;
    }
    else
    {
        g_fpga.activeMode = DEFAULT_WORK_MODE;
        FpgaInitDefaultParams(&g_fpga.pendingParams);
        g_fpga.pendingParamsValid = 1U;
    }
    g_fpga.lastResult = SAT_PAYLOAD_OK;
    FpgaClearCompletedBootFlag();
    g_fpga.resetType = FpgaClassifyResetType((uint32_t)g_tk8710ResetCause);
    FpgaLoadAndIncrementResetCount();
    (void)loadedStoredParams;
#if !defined(FPGA_PROTOCOL_HOST_TEST)
    if (FpgaApplyPendingParams() >= SAT_PAYLOAD_OK)
    {
        (void)SatPayloadApp_SetWorkMode(g_fpga.activeMode);
    }
#endif
    FpgaRestoreStoredDcParams();
    FpgaBuildTelemetry(g_fpga.txFrame);
    g_fpga.txPageIndex = 0U;

#if !defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaConfigMibspi3Slave();
    FpgaWriteSpiTxRam();
    FpgaArmTransfer();
    g_fpga.lastTelemetryMs = (uint32_t)(TK8710GetTimeUs() / 1000U);
#endif

    g_fpga.initialized = 1U;
}

void FpgaProtocol_Process(void)
{
#if !defined(FPGA_PROTOCOL_HOST_TEST)
    uint16_t rxWords[FPGA_PROTOCOL_SPI_PAGE_LEN];
    uint8_t rxFrame[FPGA_PROTOCOL_RC_FRAME_LEN];
    uint32_t nowMs;
    uint32_t i;

    if (g_fpga.initialized == 0U)
    {
        return;
    }

    if (mibspiIsTransferComplete(FPGA_MIBSPI_REG, FPGA_SPI_GROUP) != FALSE)
    {
        (void)mibspiGetData(FPGA_MIBSPI_REG, FPGA_SPI_GROUP, rxWords);
        for (i = 0U; i < FPGA_PROTOCOL_RC_FRAME_LEN; i++)
        {
            rxFrame[i] = (uint8_t)rxWords[i];
        }
        (void)FpgaHandleRxFrame(rxFrame);
        FpgaWriteSpiTxRam();
        FpgaArmTransfer();
        FpgaFlushPendingRcLog();
    }

    nowMs = (uint32_t)(TK8710GetTimeUs() / 1000U);
    if ((nowMs - g_fpga.lastTelemetryMs) >= 1000U)
    {
        g_fpga.lastTelemetryMs = nowMs;
        FpgaRefreshTelemetry();
    }
#endif
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
SatPayloadResult SatPayloadApp_SetWorkParams(const SatPayloadWorkParams *params)
{
    if (params == 0)
    {
        (void)printf("CALL SatPayloadApp_SetWorkParams NULL\n");
        return SAT_PAYLOAD_ERR_PARAM;
    }
    (void)printf("CALL SatPayloadApp_SetWorkParams rate=%u freqHz=%lu rfMask=%u\n",
                 (unsigned)params->rates[0].rateMode,
                 (unsigned long)params->centerFreqHz,
                 (unsigned)params->rfMask);
    g_hostAppliedRateMode = params->rates[0].rateMode;
    g_hostTelemetry.activeParams = *params;
    return SAT_PAYLOAD_OK;
}

uint8_t FpgaProtocol_TestGetHostAppliedRateMode(void)
{
    return g_hostAppliedRateMode;
}

SatPayloadResult SatPayloadApp_SetWorkMode(uint8_t mode)
{
    (void)printf("CALL SatPayloadApp_SetWorkMode mode=%u\n", (unsigned)mode);
    g_hostLastMode = mode;
    g_hostTelemetry.activeMode = mode;
    return SAT_PAYLOAD_OK;
}

SatPayloadResult SatPayloadApp_Stop(void)
{
    return SAT_PAYLOAD_OK;
}

SatPayloadResult SatPayloadApp_HandleTelecommand(
    const SatPayloadTelecommand *request,
    SatPayloadTelecommandResponse *response)
{
    if ((request == 0) || (response == 0))
    {
        return SAT_PAYLOAD_ERR_PARAM;
    }
    response->requestId = request->requestId;
    response->commandId = request->commandId;
    response->result = SAT_PAYLOAD_OK;
    response->value = 0x12345678U;
    if ((request->commandId == SAT_PAYLOAD_TC_SET_RF_TX_DC) &&
        (g_hostDcApplyCount < 8U))
    {
        g_hostDcAntenna[g_hostDcApplyCount] = request->payload.rfTxDc.antenna;
        g_hostDcI[g_hostDcApplyCount] = request->payload.rfTxDc.iDc;
        g_hostDcQ[g_hostDcApplyCount] = request->payload.rfTxDc.qDc;
        g_hostDcApplyCount++;
    }
    return SAT_PAYLOAD_OK;
}

void SatPayloadApp_GetTelemetry(SatPayloadTelemetry *telemetry)
{
    uint32_t i;

    if (telemetry == 0)
    {
        return;
    }
    if (g_hostTelemetry.activeParams.rateCount == 0U)
    {
        FpgaInitDefaultParams(&g_hostTelemetry.activeParams);
    }
    g_hostTelemetry.uptimeMs = 123000U;
    g_hostTelemetry.rxCount = 7U;
    g_hostTelemetry.txCount = 9U;
    g_hostTelemetry.heapUsed = 25U;
    g_hostTelemetry.heapPeak = 100U;
    g_hostTelemetry.capture.noiseDbmHz[0] = -10.75F;
    g_hostTelemetry.capture.noiseDbmHz[1] = -300.0F;
    g_hostTelemetry.capture.noiseDbmHz[2] = 3.25F;
    g_hostTelemetry.capture.noiseDbmHz[3] = 0.0F;
    g_hostTelemetry.capture.noiseDbmHz[4] = -1.2F;
    g_hostTelemetry.capture.noiseDbmHz[5] = -2.3F;
    g_hostTelemetry.capture.noiseDbmHz[6] = -3.4F;
    g_hostTelemetry.capture.noiseDbmHz[7] = -4.5F;
    for (i = 0U; i < 8U; i++)
    {
        g_hostTelemetry.acmResult.iFactor[i] = 0x01020304UL + i;
        g_hostTelemetry.acmResult.qFactor[i] = 0x11223344UL + i;
    }
    *telemetry = g_hostTelemetry;
}

SatPayloadResult SatPayloadApp_GetAcmCalibrationResult(TRM_AcmCalibResult *result)
{
    if (result == 0)
    {
        return SAT_PAYLOAD_ERR_PARAM;
    }
    (void)memset(result, 0, sizeof(*result));
    return SAT_PAYLOAD_OK;
}

SatPayloadResult SatPayloadApp_GetCaptureInfo(TK8710CaptureInfo *info)
{
    (void)info;
    return SAT_PAYLOAD_ERR_UNSUPPORTED;
}

SatPayloadResult SatPayloadApp_ReadCaptureData(uint32_t generation,
                                               uint8_t antenna,
                                               uint32_t offset,
                                               void *data, uint32_t len)
{
    (void)generation;
    (void)antenna;
    (void)offset;
    (void)data;
    (void)len;
    return SAT_PAYLOAD_ERR_UNSUPPORTED;
}

SatPayloadResult SatPayloadApp_GetSweepResultInfo(TRM_SweepResultInfo *info)
{
    (void)info;
    return SAT_PAYLOAD_ERR_UNSUPPORTED;
}

SatPayloadResult SatPayloadApp_ReadSweepResults(uint32_t startIndex,
                                                TRM_SweepResultPoint *results,
                                                uint32_t capacity,
                                                uint32_t *resultCount)
{
    (void)startIndex;
    (void)results;
    (void)capacity;
    (void)resultCount;
    return SAT_PAYLOAD_ERR_UNSUPPORTED;
}

SatPayloadState SatPayloadApp_GetState(void)
{
    return SAT_PAYLOAD_STATE_CONTROL_READY;
}

uint8_t SatPayloadApp_GetActiveMode(void)
{
    return g_hostLastMode;
}

const char *SatPayloadApp_StateName(SatPayloadState state)
{
    (void)state;
    return "HOST";
}

void SatPayloadApp_Init(void)
{
}

void SatPayloadApp_Process(void)
{
}

void FpgaProtocol_TestReset(void)
{
    (void)memset(&g_fpga, 0, sizeof(g_fpga));
    (void)memset(&g_hostTelemetry, 0, sizeof(g_hostTelemetry));
    g_hostAppliedRateMode = 0U;
    (void)memset(g_hostLastRcLog, 0, sizeof(g_hostLastRcLog));
    g_hostRcLogCount = 0U;
    g_hostDcApplyCount = 0U;
    (void)memset(g_hostDcAntenna, 0, sizeof(g_hostDcAntenna));
    (void)memset(g_hostDcI, 0, sizeof(g_hostDcI));
    (void)memset(g_hostDcQ, 0, sizeof(g_hostDcQ));
    FpgaParamStore_TestErase();
    FpgaInitDefaultParams(&g_fpga.pendingParams);
    g_fpga.pendingParamsValid = 1U;
}

int FpgaProtocol_TestHandleRxFrameNoReset(const uint8_t *frame, uint32_t *errorCount)
{
    int result;

    result = FpgaHandleRxFrame(frame);
    FpgaFlushPendingRcLog();
    if (errorCount != 0)
    {
        *errorCount = g_fpga.rxErrorCount + g_fpga.unsupportedCount;
    }
    return result;
}

int FpgaProtocol_TestHandleRxFrame(const uint8_t *frame, uint32_t *errorCount)
{
    FpgaProtocol_TestReset();
    return FpgaProtocol_TestHandleRxFrameNoReset(frame, errorCount);
}

const char *FpgaProtocol_TestGetLastLog(void)
{
    return g_hostLastRcLog;
}

uint32_t FpgaProtocol_TestGetLogCount(void)
{
    return g_hostRcLogCount;
}

void FpgaProtocol_TestClearLastLog(void)
{
    g_hostLastRcLog[0] = '\0';
    g_hostRcLogCount = 0U;
    g_fpga.pendingRcLog[0] = '\0';
    g_fpga.pendingRcLogValid = 0U;
}

void FpgaProtocol_TestBuildTelemetry(uint8_t *frame)
{
    FpgaBuildTelemetry(frame);
}

void FpgaProtocol_TestBuildTelemetryPage(uint8_t *page, uint8_t pageIndex)
{
    uint8_t frame[FPGA_PROTOCOL_TM_FRAME_LEN];

    FpgaBuildTelemetry(frame);
    FpgaBuildTelemetryPage(frame, page, pageIndex);
}

void FpgaProtocol_TestQueueTelemetryBurst(void)
{
    FpgaQueueTelemetryBurst();
}

void FpgaProtocol_TestCompleteTransferAndBuildNextPage(uint8_t *page)
{
    FpgaBuildCurrentTxPage(page);
}

void FpgaProtocol_TestNextTelemetryTxPage(uint8_t *page)
{
    FpgaBuildCurrentTxPage(page);
}

uint8_t FpgaProtocol_TestGetSavedDcMask(void)
{
    return g_fpga.dcValidMask;
}

int FpgaProtocol_TestGetSavedDc(uint8_t antenna, int16_t *iDc, int16_t *qDc)
{
    if ((antenna >= 8U) || (iDc == 0) || (qDc == 0) ||
        ((g_fpga.dcValidMask & (uint8_t)(1U << antenna)) == 0U))
    {
        return -1;
    }
    *iDc = g_fpga.dcI[antenna];
    *qDc = g_fpga.dcQ[antenna];
    return 0;
}

uint32_t FpgaProtocol_TestGetDcApplyCount(void)
{
    return g_hostDcApplyCount;
}

int FpgaProtocol_TestGetAppliedDc(uint32_t index, uint8_t *antenna,
                                  int16_t *iDc, int16_t *qDc)
{
    if ((index >= g_hostDcApplyCount) || (antenna == 0) ||
        (iDc == 0) || (qDc == 0))
    {
        return -1;
    }
    *antenna = g_hostDcAntenna[index];
    *iDc = g_hostDcI[index];
    *qDc = g_hostDcQ[index];
    return 0;
}
#endif
