/** @file sys_main.c
 *  @brief TMS570LS3137 satellite payload application entry.
 */

#include "sys_common.h"

/* USER CODE BEGIN (1) */
#if defined(PLATFORM_TMS570)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sci.h"
#include "sys_core.h"
#include "tk8710_tms570.h"
#include "tk8710_hal.h"
#include "driver/tk8710_regs.h"
#include "driver/tk8710_internal.h"
#include "tk8710_sat_payload_app.h"

#define SAT_MAIN_STAGE_PORT_FAIL       1U
#define SAT_MAIN_STAGE_SDRAM_FAIL      2U
#define SAT_MAIN_STAGE_SPI_RESET_FAIL  3U
#define SAT_MAIN_STAGE_SPI_READ_FAIL   4U
#define SAT_MAIN_STAGE_CONTROL_READY   5U
#define SAT_MAIN_VERSION_REG           (MAC_BASE + 0x0110U)
#define SAT_MAIN_AT_LINE_MAX           256U
#define SAT_MAIN_SWEEP_RESULT_PAGE_MAX 8U

volatile uint32 g_tk8710ResetCause = 0U;
volatile uint32 g_tk8710BringupStage = 0U;
volatile uint32 g_tk8710BringupHaltLine = 0U;
volatile uint32 g_tk8710BringupReadValue = 0U;

static char g_satAtLine[SAT_MAIN_AT_LINE_MAX];
static SatPayloadWorkParams g_satAtPendingParams;
static uint8 g_satAtPendingParamsValid = 0U;

static void SatMainLog(const char* text);
static void SatMainLogU32(uint32 value);
static void SatMainLogS32(int32_t value);
static void SatMainLogDb(float value);
static void SatMainLogHex32(uint32 value);
static void SatMainHalt(uint32 stage, uint32 line);
static void SatMainPrintHelp(void);
static void SatMainProcessConsole(void);
static int SatMainReadLine(char* line, uint32 capacity);
static int SatMainParseU32(const char* text, uint32* value);
static int SatMainParseList(char* text, uint32* values, uint32 count);
static int SatMainHandleCommand(char* line);
static void SatMainPrintTelemetry(void);
static int SatMainPrintSweepResults(uint32 startIndex, uint32 count);
static int SatMainSetRfTxDc(char* line);

#define SatMainHaltAt(stage) SatMainHalt((stage), (uint32)__LINE__)
#endif
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
#if defined(PLATFORM_TMS570)
    TK8710Tms570SdramDiag sdramDiag;
    TK8710Tms570EmifDiag emifDiag;

    if (TK8710Tms570Init() != 0) {
        SatMainHaltAt(SAT_MAIN_STAGE_PORT_FAIL);
    }

    SatMainLog("\r\nTK8710 TMS570 satellite payload start\r\n");
    SatMainLog("Reset cause=");
    SatMainLogHex32(g_tk8710ResetCause);
    SatMainLog("\r\n");

    if (TK8710Tms570SdramSelfTest(TK8710_TMS570_SDRAM_TEST_BASE,
                                  TK8710_TMS570_SDRAM_TEST_SIZE) != 0) {
        TK8710Tms570GetSdramDiag(&sdramDiag);
        TK8710Tms570GetEmifDiag(&emifDiag);
        SatMainLog("SDRAM self-test failed phase=");
        SatMainLogU32(sdramDiag.phase);
        SatMainLog(" address=");
        SatMainLogHex32(sdramDiag.address);
        SatMainLog(" index=");
        SatMainLogU32(sdramDiag.index);
        SatMainLog(" expected=");
        SatMainLogHex32(sdramDiag.expected);
        SatMainLog(" actual=");
        SatMainLogHex32(sdramDiag.actual);
        SatMainLog("\r\n");
        SatMainLog("EMIF GPREG1=");
        SatMainLogHex32(emifDiag.gpreg1);
        SatMainLog(" SDCR=");
        SatMainLogHex32(emifDiag.sdcr);
        SatMainLog(" SDRCR=");
        SatMainLogHex32(emifDiag.sdrcr);
        SatMainLog("\r\nEMIF SDTIMR=");
        SatMainLogHex32(emifDiag.sdtimr);
        SatMainLog(" SDSRETR=");
        SatMainLogHex32(emifDiag.sdsretr);
        SatMainLog(" PINMMR29=");
        SatMainLogHex32(emifDiag.pinmmr29);
        SatMainLog("\r\nEMIF CLK2CNTL=");
        SatMainLogHex32(emifDiag.clk2cntl);
        SatMainLog(" VCLKACON1=");
        SatMainLogHex32(emifDiag.vclkacon1);
        SatMainLog("\r\nSDRAM unavailable; capture and sweep disabled\r\n");
    } else {
        SatMainLog("SDRAM self-test passed\r\n");
    }

    if (TK8710SpiReset(TK8710_RST_SM_AND_REG) != 0) {
        SatMainLog("TK8710 SPI reset failed\r\n");
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_RESET_FAIL);
    }
    TK8710DelayMs(20U);

    if (TK8710SpiReadReg((uint16_t)SAT_MAIN_VERSION_REG,
                         (uint32_t*)&g_tk8710BringupReadValue, 1U) != 0) {
        SatMainLog("TK8710 version read failed\r\n");
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_READ_FAIL);
    }
    SatMainLog("TK8710 FPGA version=");
    SatMainLogHex32(g_tk8710BringupReadValue);
    SatMainLog("\r\n");
    if ((g_tk8710BringupReadValue == 0U) ||
        (g_tk8710BringupReadValue == 0xFFFFFFFFU)) {
        SatMainLog("TK8710 version is invalid\r\n");
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_READ_FAIL);
    }

    SatPayloadApp_Init();
    g_tk8710BringupStage = SAT_MAIN_STAGE_CONTROL_READY;
    _enable_interrupt_();

    SatMainLog("Satellite payload control ready\r\n");
    SatMainPrintHelp();
    SatMainLog("SAT> ");

    while (1) {
        TK8710Tms570PollIrq();
        TK8710ProcessRuntimeWatchdog();
        SatPayloadApp_Process();
        SatMainProcessConsole();
    }
#endif

#if defined(__TI_COMPILER_VERSION__)
#pragma diag_push
#pragma diag_suppress 112
#endif
    return 0;
#if defined(__TI_COMPILER_VERSION__)
#pragma diag_pop
#endif
/* USER CODE END */
}

/* USER CODE BEGIN (4) */
#if defined(PLATFORM_TMS570)
static void SatMainLog(const char* text)
{
    while ((text != NULL) && (*text != '\0')) {
        while (sciIsTxReady(scilinREG) == 0U) {
        }
        sciSendByte(scilinREG, (uint8)*text++);
    }
}

static void SatMainLogU32(uint32 value)
{
    char text[16];
    (void)snprintf(text, sizeof(text), "%lu", (unsigned long)value);
    SatMainLog(text);
}

static void SatMainLogS32(int32_t value)
{
    if (value < 0) {
        SatMainLog("-");
        SatMainLogU32((uint32)(-(value + 1)) + 1U);
    } else {
        SatMainLogU32((uint32)value);
    }
}

static void SatMainLogDb(float value)
{
    int32_t scaled = (int32_t)(value * 100.0F);
    uint32 magnitude;
    uint32 fraction;

    if (scaled < 0) {
        SatMainLog("-");
        magnitude = (uint32)(-(scaled + 1)) + 1U;
    } else {
        magnitude = (uint32)scaled;
    }
    fraction = magnitude % 100U;
    SatMainLogU32(magnitude / 100U);
    SatMainLog(".");
    if (fraction < 10U) {
        SatMainLog("0");
    }
    SatMainLogU32(fraction);
}

static void SatMainLogHex32(uint32 value)
{
    char text[16];
    (void)snprintf(text, sizeof(text), "0x%08lX", (unsigned long)value);
    SatMainLog(text);
}

static void SatMainHalt(uint32 stage, uint32 line)
{
    g_tk8710BringupStage = stage;
    g_tk8710BringupHaltLine = line;

    while (1) {
    }
}

static void SatMainPrintHelp(void)
{
    SatMainLog("AT commands:\r\n");
    SatMainLog("  AT\r\n");
    SatMainLog("  AT+HELP\r\n");
    SatMainLog("  AT+SETPARAM=<rate>,<s0len>,<s0gap>,<s1len>,<s1gap>,<s2len>,<s2gap>,<s3len>,<s3gap>,<freq>,<rxgain>,<txgain>,<antmask>,<rfmask>\r\n");
    SatMainLog("  AT+SETSWEEP=<startHz>,<endHz>,<sweepMode>\r\n");
    SatMainLog("  AT+SWEEPRESULT=<startIndex>,<count>\r\n");
    SatMainLog("  AT+SETMODE=<0..6>\r\n");
    SatMainLog("  AT+STOP\r\n");
    SatMainLog("  AT+STATE\r\n");
    SatMainLog("  AT+TM\r\n");
    SatMainLog("  AT+ACM\r\n");
    SatMainLog("  AT+RREG=<addr>\r\n");
    SatMainLog("  AT+WREG=<addr>,<value>\r\n");
    SatMainLog("  AT+RRF=<rfmask>,<addr>\r\n");
    SatMainLog("  AT+WRF=<rfmask>,<addr>,<value>\r\n");
    SatMainLog("  AT+SETTXDC=<antenna>,<i16>,<q16>\r\n");
}

static void SatMainProcessConsole(void)
{
    if (SatMainReadLine(g_satAtLine, (uint32)sizeof(g_satAtLine)) > 0) {
        (void)SatMainHandleCommand(g_satAtLine);
        SatMainLog("SAT> ");
    }
}

static int SatMainReadLine(char* line, uint32 capacity)
{
    static uint32 index = 0U;
    static uint8 overflow = 0U;

    while (sciIsRxReady(scilinREG) != 0U) {
        uint8 ch = sciReceiveByte(scilinREG);
        if ((ch == (uint8)'\r') || (ch == (uint8)'\n')) {
            if ((index == 0U) && (overflow == 0U)) {
                continue;
            }
            line[index] = '\0';
            index = 0U;
            if (overflow != 0U) {
                overflow = 0U;
                SatMainLog("ERROR command too long\r\n");
                return -1;
            }
            SatMainLog("\r\n");
            return 1;
        }

        if ((ch == 0x08U) || (ch == 0x7FU)) {
            if (index > 0U) {
                index--;
            }
            continue;
        }

        if (overflow == 0U) {
            if ((index + 1U) < capacity) {
                line[index++] = (char)ch;
            } else {
                overflow = 1U;
            }
        }
    }
    return 0;
}

static int SatMainParseU32(const char* text, uint32* value)
{
    char* end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL) || (*text == '\0')) {
        return -1;
    }
    parsed = strtoul(text, &end, 0);
    while ((*end == ' ') || (*end == '\t')) {
        end++;
    }
    if (*end != '\0') {
        return -1;
    }
    *value = (uint32)parsed;
    return 0;
}

static int SatMainParseList(char* text, uint32* values, uint32 count)
{
    uint32 index;
    char* current = text;

    if ((text == NULL) || (values == NULL)) {
        return -1;
    }

    for (index = 0U; index < count; index++) {
        char* comma = strchr(current, ',');
        if (index + 1U < count) {
            if (comma == NULL) {
                return -1;
            }
            *comma = '\0';
        } else if (comma != NULL) {
            return -1;
        }

        if (SatMainParseU32(current, &values[index]) != 0) {
            return -1;
        }
        if (comma != NULL) {
            current = comma + 1;
        }
    }
    return 0;
}

static void SatMainPrintResult(SatPayloadResult result)
{
    SatMainLog((result >= SAT_PAYLOAD_OK) ? "OK result=" : "ERROR result=");
    if (result < 0) {
        SatMainLog("-");
        SatMainLogU32((uint32)(-result));
    } else {
        SatMainLogU32((uint32)result);
    }
    SatMainLog("\r\n");
}

static void SatMainPrintTelemetry(void)
{
    SatPayloadTelemetry tm;
    uint32 index;

    SatPayloadApp_GetTelemetry(&tm);
    SatMainLog("TM seq=");
    SatMainLogU32(tm.sequence);
    SatMainLog(" uptimeMs=");
    SatMainLogU32(tm.uptimeMs);
    SatMainLog(" state=");
    SatMainLog(SatPayloadApp_StateName(tm.state));
    SatMainLog(" mode=");
    SatMainLogU32(tm.activeMode);
    SatMainLog(" configVersion=");
    SatMainLogU32(tm.configVersion);
    SatMainLog(" lastResult=");
    if (tm.lastResult < 0) {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastResult));
    } else {
        SatMainLogU32((uint32)tm.lastResult);
    }
    SatMainLog("\r\nTRM tx=");
    SatMainLogU32(tm.txCount);
    SatMainLog(" txOk=");
    SatMainLogU32(tm.txSuccessCount);
    SatMainLog(" rx=");
    SatMainLogU32(tm.rxCount);
    SatMainLog(" queueFree=");
    SatMainLogU32(tm.txQueueRemaining);
    SatMainLog(" cache=");
    SatMainLogU32(tm.satelliteCacheCount);
    SatMainLog(" routes=");
    SatMainLogU32(tm.satelliteRouteCount);
    SatMainLog(" gsBeams=");
    SatMainLogU32(tm.satelliteGroundStationBeamCount);
    SatMainLog(" beamMiss=");
    SatMainLogU32(tm.satelliteBeamMissCount);
    SatMainLog("\r\nPORT heap=");
    SatMainLogU32(tm.heapUsed);
    SatMainLog(" peak=");
    SatMainLogU32(tm.heapPeak);
    SatMainLog(" heapFail=");
    SatMainLogU32(tm.heapFailCount);
    SatMainLog(" spiErr=");
    SatMainLogU32(tm.spiErrorCount);
    SatMainLog(" gpioIrq=");
    SatMainLogU32(tm.gpioIrqCount);
    SatMainLog(" edge=");
    SatMainLogU32(tm.gpioIrqEdgeCount);
    SatMainLog(" recovery=");
    SatMainLogU32(tm.gpioIrqRecoveryCount);
    SatMainLog(" statusPoll=");
    SatMainLogU32(tm.irqStatusPollCount);
    SatMainLog(" sdram=");
    SatMainLogU32(tm.sdramAvailable);
    SatMainLog("\r\nGIO pin=");
    SatMainLogU32(tm.gpioIrqLevel);
    SatMainLog(" callback=");
    SatMainLogU32(tm.gpioIrqCallbackConfigured);
    SatMainLog(" DIN=");
    SatMainLogHex32(tm.gpioDin);
    SatMainLog(" FLG=");
    SatMainLogHex32(tm.gpioFlag);
    SatMainLog(" ENA=");
    SatMainLogHex32(tm.gpioEnable);
    SatMainLog(" VIMMASK0=");
    SatMainLogHex32(tm.vimReqMask0);
    SatMainLog("\r\nRST pin=");
    SatMainLogU32(tm.resetPinLevel);
    SatMainLog(" lowEdge=");
    SatMainLogU32(tm.resetPinLowCount);
    SatMainLog(" driveLow=");
    SatMainLogU32(tm.resetDriveLowCount);
    SatMainLog(" spiReset=");
    SatMainLogU32(tm.spiResetCount);
    SatMainLog(" portInit=");
    SatMainLogU32(tm.portInitCount);
    SatMainLog(" spiInit=");
    SatMainLogU32(tm.spiInitCount);
    SatMainLog(" DOUT=");
    SatMainLogHex32(tm.resetGioDout);
    SatMainLog(" DIR=");
    SatMainLogHex32(tm.resetGioDir);
    SatMainLog("\r\nIRQ");
    for (index = 0U; index < SAT_PAYLOAD_IRQ_COUNT; index++) {
        SatMainLog(" ");
        SatMainLogU32(tm.irqCounters[index]);
    }
    SatMainLog(" raw=");
    SatMainLogHex32(tm.irqStatus);
    SatMainLog(" mask=");
    SatMainLogHex32(tm.irqMask);
    SatMainLog("\r\nRX valid=");
    SatMainLogU32(tm.lastRx.valid);
    SatMainLog(" gen=");
    SatMainLogU32(tm.lastRx.generation);
    SatMainLog(" user=");
    SatMainLogHex32(tm.lastRx.userId);
    SatMainLog(" rssi=");
    if (tm.lastRx.rssi < 0) {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastRx.rssi));
    } else {
        SatMainLogU32((uint32)tm.lastRx.rssi);
    }
    SatMainLog(" snr=");
    SatMainLogU32(tm.lastRx.snr);
    SatMainLog(" freqOffset=");
    if (tm.lastRx.freqOffset < 0) {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastRx.freqOffset));
    } else {
        SatMainLogU32((uint32)tm.lastRx.freqOffset);
    }
    SatMainLog(" freqHz=");
    SatMainLogU32(tm.lastRx.frequencyHz);
    SatMainLog("\r\nACM pending=");
    SatMainLogU32(tm.acmPending);
    SatMainLog(" running=");
    SatMainLogU32(tm.acmRunning);
    SatMainLog(" completed=");
    SatMainLogU32(tm.acmCompletedCount);
    SatMainLog(" last=");
    SatMainLogS32(tm.acmLastResult);
    SatMainLog("\r\nACM_RESULT valid=");
    SatMainLogU32(tm.acmResult.valid);
    SatMainLog(" gen=");
    SatMainLogU32(tm.acmResult.generation);
    SatMainLog(" timestampMs=");
    SatMainLogU32(tm.acmResult.timestampMs);
    SatMainLog(" validCount=");
    SatMainLogU32(tm.acmResult.validCalibCount);
    SatMainLog(" antMask=");
    SatMainLogHex32(tm.acmResult.validAntennaMask);
    SatMainLog(" last=");
    SatMainLogS32(tm.acmResult.lastResult);
    for (index = 0U; index < 8U; index++) {
        SatMainLog("\r\nACM_FACTOR ant=");
        SatMainLogU32(index + 1U);
        SatMainLog(" I=");
        SatMainLogHex32(tm.acmResult.iFactor[index]);
        SatMainLog(" Q=");
        SatMainLogHex32(tm.acmResult.qFactor[index]);
    }
    SatMainLog("\r\nCAP state=");
    SatMainLogU32(tm.capture.state);
    SatMainLog(" gen=");
    SatMainLogU32(tm.capture.generation);
    SatMainLog(" rate=");
    SatMainLogU32(tm.capture.rateMode);
    SatMainLog(" valid=");
    SatMainLogHex32(tm.capture.validAntennaMask);
    SatMainLog(" bytes=");
    SatMainLogU32(tm.capture.bytesPerAntenna);
    SatMainLog(" errors=");
    SatMainLogU32(tm.capture.errorCount);
    SatMainLog(" spiUs=");
    SatMainLogU32(tm.capture.lastSpiUs);
    SatMainLog("/");
    SatMainLogU32(tm.capture.maxSpiUs);
    SatMainLog(" fftUs=");
    SatMainLogU32(tm.capture.lastFftUs);
    SatMainLog("/");
    SatMainLogU32(tm.capture.maxFftUs);
    SatMainLog(" last=");
    SatMainLogS32(tm.capture.lastError);
    SatMainLog("\r\nCAP noiseDbmHz=");
    for (index = 0U; index < 8U; index++) {
        if (index != 0U) {
            SatMainLog(",");
        }
        SatMainLogDb(tm.capture.noiseDbmHz[index]);
    }
    SatMainLog("\r\nSWEEP gen=");
    SatMainLogU32(tm.sweep.generation);
    SatMainLog(" active=");
    SatMainLogU32(tm.sweep.active);
    SatMainLog(" complete=");
    SatMainLogU32(tm.sweep.complete);
    SatMainLog(" points=");
    SatMainLogU32(tm.sweep.completedPoints);
    SatMainLog("/");
    SatMainLogU32(tm.sweep.totalPoints);
    SatMainLog(" last=");
    SatMainLogS32(tm.sweep.lastError);
    SatMainLog("\r\n");
}

static int SatMainPrintSweepResults(uint32 startIndex, uint32 count)
{
    TRM_SweepResultInfo info;
    TRM_SweepResultPoint results[SAT_MAIN_SWEEP_RESULT_PAGE_MAX];
    uint32 resultCount = 0U;
    uint32 index;
    uint32 antenna;
    SatPayloadResult result;

    if ((count == 0U) || (count > SAT_MAIN_SWEEP_RESULT_PAGE_MAX)) {
        SatMainLog("ERROR count must be 1..8\r\n");
        return -1;
    }
    result = SatPayloadApp_GetSweepResultInfo(&info);
    if (result != SAT_PAYLOAD_OK) {
        SatMainPrintResult(result);
        return -1;
    }
    if (startIndex > info.completedPoints) {
        SatMainLog("ERROR startIndex exceeds completedPoints\r\n");
        return -1;
    }
    result = SatPayloadApp_ReadSweepResults(startIndex, results, count,
                                            &resultCount);
    if (result != SAT_PAYLOAD_OK) {
        SatMainPrintResult(result);
        return -1;
    }

    SatMainLog("SWEEPRESULT gen=");
    SatMainLogU32(info.generation);
    SatMainLog(" start=");
    SatMainLogU32(startIndex);
    SatMainLog(" count=");
    SatMainLogU32(resultCount);
    SatMainLog(" completed=");
    SatMainLogU32(info.completedPoints);
    SatMainLog("/");
    SatMainLogU32(info.totalPoints);
    SatMainLog(" active=");
    SatMainLogU32(info.active);
    SatMainLog(" complete=");
    SatMainLogU32(info.complete);
    SatMainLog(" last=");
    SatMainLogS32(info.lastError);
    SatMainLog("\r\n");

    for (index = 0U; index < resultCount; index++) {
        SatMainLog("POINT index=");
        SatMainLogU32(startIndex + index);
        SatMainLog(" freqHz=");
        SatMainLogU32(results[index].frequencyHz);
        SatMainLog(" noiseDbmHz=");
        for (antenna = 0U; antenna < 8U; antenna++) {
            if (antenna != 0U) {
                SatMainLog(",");
            }
            SatMainLogDb(results[index].noiseDbmHz[antenna]);
        }
        SatMainLog("\r\n");
    }
    SatMainLog("OK\r\n");
    return 0;
}

static int SatMainHandleRegisterCommand(char* line,
                                        SatPayloadTelecommandId commandId,
                                        uint32 valueCount)
{
    uint32 values[3];
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    if (SatMainParseList(line, values, valueCount) != 0) {
        SatMainLog("ERROR bad register arguments\r\n");
        return -1;
    }

    (void)memset(&request, 0, sizeof(request));
    request.commandId = commandId;
    if ((commandId == SAT_PAYLOAD_TC_READ_RF_REG) ||
        (commandId == SAT_PAYLOAD_TC_WRITE_RF_REG)) {
        request.payload.reg.rfMask = (uint8_t)values[0];
        request.payload.reg.address = (uint16_t)values[1];
        if (valueCount == 3U) {
            request.payload.reg.value = values[2];
        }
    } else {
        request.payload.reg.address = (uint16_t)values[0];
        if (valueCount == 2U) {
            request.payload.reg.value = values[1];
        }
    }

    result = SatPayloadApp_HandleTelecommand(&request, &response);
    if ((result == SAT_PAYLOAD_OK) &&
        ((commandId == SAT_PAYLOAD_TC_READ_REG) ||
         (commandId == SAT_PAYLOAD_TC_READ_RF_REG))) {
        SatMainLog("VALUE=");
        SatMainLogHex32(response.value);
        SatMainLog("\r\n");
    }
    SatMainPrintResult(result);
    return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
}

static int SatMainSetRfTxDc(char* line)
{
    uint32 values[3U];
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    if ((SatMainParseList(line, values, 3U) != 0) ||
        (values[0U] >= SAT_PAYLOAD_RF_ANTENNA_COUNT) ||
        (values[1U] > 0xFFFFU) || (values[2U] > 0xFFFFU)) {
        SatMainLog("ERROR expected antenna=0..7 and 16-bit I/Q values\r\n");
        return -1;
    }

    (void)memset(&request, 0, sizeof(request));
    request.commandId = SAT_PAYLOAD_TC_SET_RF_TX_DC;
    request.payload.rfTxDc.antenna = (uint8_t)values[0U];
    request.payload.rfTxDc.iDc = (int16_t)(uint16_t)values[1U];
    request.payload.rfTxDc.qDc = (int16_t)(uint16_t)values[2U];
    result = SatPayloadApp_HandleTelecommand(&request, &response);
    if (result == SAT_PAYLOAD_OK) {
        SatMainLog("RF_TX_DC antenna=");
        SatMainLogU32(values[0U]);
        SatMainLog(" value=");
        SatMainLogHex32(response.value);
        SatMainLog("\r\n");
    }
    SatMainPrintResult(result);
    return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
}

static int SatMainHandleCommand(char* line)
{
    uint32 values[14];
    uint32 value;
    uint32 stepFreq;
    SatPayloadWorkParams params;
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    while ((*line == ' ') || (*line == '\t')) {
        line++;
    }
    if (strcmp(line, "AT") == 0) {
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+HELP") == 0) {
        SatMainPrintHelp();
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strncmp(line, "AT+SETPARAM=", 12) == 0) {
        if (SatMainParseList(line + 12, values, 14U) != 0) {
            SatMainLog("ERROR SETPARAM needs 14 values\r\n");
            return -1;
        }
        (void)memset(&params, 0, sizeof(params));
        params.rateCount = 1U;
        params.rates[0].rateMode = (uint8_t)values[0];
        params.rates[0].slots[0].byteLen = (uint16_t)values[1];
        params.rates[0].slots[0].daM = values[2];
        params.rates[0].slots[1].byteLen = (uint16_t)values[3];
        params.rates[0].slots[1].daM = values[4];
        params.rates[0].slots[2].byteLen = (uint16_t)values[5];
        params.rates[0].slots[2].daM = values[6];
        params.rates[0].slots[3].byteLen = (uint16_t)values[7];
        params.rates[0].slots[3].daM = values[8];
        params.centerFreqHz = values[9];
        params.rxGain = (uint8_t)values[10];
        params.txGain = (uint8_t)values[11];
        params.antennaMask = (uint8_t)values[12];
        params.rfMask = (uint8_t)values[13];
        params.bcnBits = 0U;
        params.txBcnAntennaMask = params.antennaMask;
        params.mdAgc = 1024U;
        params.maxFrameCount = 10U;
        params.sweepStartFreqHz = 0U;
        params.sweepEndFreqHz = 0U;
        params.sweepMode = 1U;
        params.toneFreq = 335544U;
        params.toneGain = 0x40U;
        params.acmCalibCount = 1U;
        params.acmSnrThreshold = 28U;
        result = SatPayloadApp_SetWorkParams(&params);
        if (result == SAT_PAYLOAD_OK) {
            g_satAtPendingParams = params;
            g_satAtPendingParamsValid = 1U;
        }
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+SETSWEEP=", 12) == 0) {
        if (SatMainParseList(line + 12, values, 3U) != 0) {
            SatMainLog("ERROR SETSWEEP needs 3 values\r\n");
            return -1;
        }
        if (g_satAtPendingParamsValid == 0U) {
            SatMainLog("ERROR run AT+SETPARAM first\r\n");
            return -1;
        }
        if ((values[0] == 0U) || (values[1] < values[0]) ||
            (values[2] > 3U)) {
            SatMainLog("ERROR invalid sweep range or mode\r\n");
            return -1;
        }
        switch (values[2]) {
            case 0U: stepFreq = 62500U; break;
            case 1U: stepFreq = 125000U; break;
            case 2U: stepFreq = 250000U; break;
            default: stepFreq = 500000U; break;
        }
        if ((((values[1] - values[0]) / stepFreq) + 1U) >
            TRM_SWEEP_MAX_RESULT_POINTS) {
            SatMainLog("ERROR sweep exceeds 1024 points\r\n");
            return -1;
        }
        params = g_satAtPendingParams;
        params.sweepStartFreqHz = values[0];
        params.sweepEndFreqHz = values[1];
        params.sweepMode = (uint8_t)values[2];
        result = SatPayloadApp_SetWorkParams(&params);
        if (result == SAT_PAYLOAD_OK) {
            g_satAtPendingParams = params;
        }
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+SWEEPRESULT=", 15) == 0) {
        if (SatMainParseList(line + 15, values, 2U) != 0) {
            SatMainLog("ERROR SWEEPRESULT needs 2 values\r\n");
            return -1;
        }
        return SatMainPrintSweepResults(values[0], values[1]);
    }
    if (strncmp(line, "AT+SETMODE=", 11) == 0) {
        if (SatMainParseU32(line + 11, &value) != 0) {
            SatMainLog("ERROR bad mode\r\n");
            return -1;
        }
        result = SatPayloadApp_SetWorkMode((uint8_t)value);
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strcmp(line, "AT+STOP") == 0) {
        result = SatPayloadApp_Stop();
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strcmp(line, "AT+STATE") == 0) {
        SatMainLog("STATE=");
        SatMainLog(SatPayloadApp_StateName(SatPayloadApp_GetState()));
        SatMainLog(" MODE=");
        SatMainLogU32(SatPayloadApp_GetActiveMode());
        SatMainLog("\r\nOK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+TM") == 0) {
        SatMainPrintTelemetry();
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+ACM") == 0) {
        (void)memset(&request, 0, sizeof(request));
        request.commandId = SAT_PAYLOAD_TC_REQUEST_ACM;
        result = SatPayloadApp_HandleTelecommand(&request, &response);
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+RREG=", 8) == 0) {
        return SatMainHandleRegisterCommand(line + 8,
            SAT_PAYLOAD_TC_READ_REG, 1U);
    }
    if (strncmp(line, "AT+WREG=", 8) == 0) {
        return SatMainHandleRegisterCommand(line + 8,
            SAT_PAYLOAD_TC_WRITE_REG, 2U);
    }
    if (strncmp(line, "AT+RRF=", 7) == 0) {
        return SatMainHandleRegisterCommand(line + 7,
            SAT_PAYLOAD_TC_READ_RF_REG, 2U);
    }
    if (strncmp(line, "AT+WRF=", 7) == 0) {
        return SatMainHandleRegisterCommand(line + 7,
            SAT_PAYLOAD_TC_WRITE_RF_REG, 3U);
    }
    if (strncmp(line, "AT+SETTXDC=", 11) == 0) {
        return SatMainSetRfTxDc(line + 11);
    }

    SatMainLog("ERROR unknown command\r\n");
    return -1;
}
#endif
/* USER CODE END */
