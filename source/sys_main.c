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
#include "sys_vim.h"
#include "tk8710_tms570.h"
#include "tk8710_hal.h"
#include "driver/tk8710_regs.h"
#include "driver/tk8710_internal.h"
#include "app_faults.h"
#include "app_selftest.h"
#include "data_transfer.h"
#include "external_watchdog.h"
#include "fpga_param_store.h"
#include "fpga_protocol.h"
#include "spi_flash.h"
#include "system.h"
#include "tk8710_sat_payload_app.h"

#define SAT_MAIN_STAGE_PORT_FAIL 1U
#define SAT_MAIN_STAGE_SDRAM_FAIL 2U
#define SAT_MAIN_STAGE_SPI_RESET_FAIL 3U
#define SAT_MAIN_STAGE_SPI_READ_FAIL 4U
#define SAT_MAIN_STAGE_CONTROL_READY 5U
#define SAT_MAIN_VERSION_REG (MAC_BASE + 0x0110U)
#define SAT_MAIN_AT_LINE_MAX 1024U
#define SAT_MAIN_AT_RX_RING_SIZE 2048U
#define SAT_MAIN_DTWRITE_PAYLOAD_MAX 495U
#define SAT_MAIN_SWEEP_RESULT_PAGE_MAX 8U
#define SAT_MAIN_DT_PRINT_MAX 512U
#define SAT_MAIN_DT_PRINT_CHUNK 32U
#define SAT_MAIN_DTFILL64K_TYPE 0x7EU
#define SAT_MAIN_DT_FLASH_TEST_ADDR DATA_TRANSFER_FLASH_DATA_START
#define SAT_MAIN_DT_FLASH_TEST_LEN DATA_TRANSFER_FLASH_PAGE_SIZE
#define SAT_MAIN_SCI_FLUSH_GUARD 1000000U

volatile uint32 g_tk8710ResetCause = 0U;
volatile uint32 g_tk8710BringupStage = 0U;
volatile uint32 g_tk8710BringupHaltLine = 0U;
volatile uint32 g_tk8710BringupReadValue = 0U;

static char g_satAtLine[SAT_MAIN_AT_LINE_MAX];
static volatile uint8 g_satAtRxRing[SAT_MAIN_AT_RX_RING_SIZE];
static volatile uint32 g_satAtRxHead = 0U;
static volatile uint32 g_satAtRxTail = 0U;
static volatile uint8 g_satAtRxOverflow = 0U;
static SatPayloadWorkParams g_satAtPendingParams;
static uint8 g_satAtPendingParamsValid = 0U;

static uint8 SatMainDtWritePayloadLengthAllowed(uint16 length)
{
    return (length <= SAT_MAIN_DTWRITE_PAYLOAD_MAX) ? 1U : 0U;
}

static void SatMainLog(const char *text);
static void SatMainLogU32(uint32 value);
static void SatMainLogS32(int32_t value);
static void SatMainLogDb(float value);
static void SatMainLogHexByte(uint8 value);
static void SatMainLogHex16(uint16 value);
static void SatMainLogHex32(uint32 value);
static uint8 SatMainIsIgnoredRxControl(uint8 ch);
static uint8 SatMainIsTrailingCommandSpace(char ch);
static void SatMainNormalizeCommandLine(char *line);
static void SatMainLogUnknownCommand(const char *line);
static void SatMainConsoleRxPush(uint8 ch);
static uint8 SatMainReadConsoleRxByte(uint8 *ch);
static void SatMainResetConsoleRx(void);
static void SatMainInitConsoleRxInterrupt(void);
static void SatMainHalt(uint32 stage, uint32 line);
static void SatMainPrintHelp(void);
static void SatMainProcessConsole(void);
static int SatMainReadLine(char *line, uint32 capacity);
static int SatMainParseU32(const char *text, uint32 *value);
static int SatMainParseList(char *text, uint32 *values, uint32 count);
static int SatMainParseHexBytes(const char *text, uint8 *data, uint32 capacity, uint16 *length);
static int SatMainHandleCommand(char *line);
static void SatMainInitDefaultFpgaStoredParams(FpgaStoredParams *params);
static void SatMainSaveFpgaParamsFromWorkParams(const SatPayloadWorkParams *params);
static void SatMainSaveFpgaMode(uint8 mode);
static void SatMainSaveRfTxDc(uint8 antenna, int16_t iDc, int16_t qDc);
static void SatMainLogPendingBootFlagComplete(void);
static void SatMainPrintTelemetry(void);
static void SatMainPrintFpgaTelemetry(void);
static int SatMainPrintSweepResults(uint32 startIndex, uint32 count);
static int SatMainSetRfTxDc(char *line);
static int SatMainDataTransferWrite(char *line);
static int SatMainDataTransferFill64K(void);
static int SatMainDataTransferClear(void);
static int SatMainDataTransferFlashTest(void);
static int SatMainDataTransferFlash2Test(void);
static int SatMainDataTransferFlashId(void);
static int SatMainDataTransferFlashStat(void);
static int SatMainDataTransferFlashDump(void);
static int SatMainDataTransferPrint(char *line);
void linHighLevelInterrupt(void);

#define SatMainHaltAt(stage) SatMainHalt((stage), (uint32)__LINE__)
#endif
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
#if defined(PLATFORM_TMS570)
    TK8710Tms570SdramDiag sdramDiag;
    TK8710Tms570EmifDiag emifDiag;

    if (TK8710Tms570Init() != 0)
    {
        SatMainHaltAt(SAT_MAIN_STAGE_PORT_FAIL);
    }
    AppFaults_Init();
    ExternalWatchdog_Init();

    SatMainLog("\r\nTK8710 TMS570 satellite payload start\r\n");
    SatMainLog("APP version=");
    SatMainLogU32(FPGA_PROTOCOL_VERSION_MAJOR);
    SatMainLog(".");
    SatMainLogU32(FPGA_PROTOCOL_VERSION_MINOR);
    SatMainLog(".");
    SatMainLogU32(FPGA_PROTOCOL_VERSION_PATCH);
    SatMainLog(" build=");
    SatMainLog(__DATE__);
    SatMainLog(" ");
    SatMainLog(__TIME__);
    SatMainLog("\r\n");
    SatMainLog("Reset cause=");
    SatMainLogHex32(g_tk8710ResetCause);
    SatMainLog("\r\n");

    if (TK8710Tms570SdramSelfTest(TK8710_TMS570_SDRAM_TEST_BASE,
                                  TK8710_TMS570_SDRAM_TEST_SIZE) != 0)
    {
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
    }
    else
    {
        SatMainLog("SDRAM self-test passed\r\n");
    }

#if !defined(FPGA_PROTOCOL_SELF_TEST)
    if (TK8710SpiReset(TK8710_RST_SM_AND_REG) != 0)
    {
        SatMainLog("TK8710 SPI reset failed\r\n");
        AppFaults_Set(APP_FAULT_TK8710_COMM);
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_RESET_FAIL);
    }
    TK8710DelayMs(20U);

    if (TK8710SpiReadReg((uint16_t)SAT_MAIN_VERSION_REG,
                         (uint32_t *)&g_tk8710BringupReadValue, 1U) != 0)
    {
        SatMainLog("TK8710 version read failed\r\n");
        AppFaults_Set(APP_FAULT_TK8710_COMM);
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_READ_FAIL);
    }
    SatMainLog("TK8710 FPGA version=");
    SatMainLogHex32(g_tk8710BringupReadValue);
    SatMainLog("\r\n");
    if ((g_tk8710BringupReadValue == 0U) ||
        (g_tk8710BringupReadValue == 0xFFFFFFFFU))
    {
        SatMainLog("TK8710 version is invalid\r\n");
        AppFaults_Set(APP_FAULT_TK8710_COMM);
        SatMainHaltAt(SAT_MAIN_STAGE_SPI_READ_FAIL);
    }
#else
    SatMainLog("FPGA protocol self-test mode: TK8710 SPI3 bring-up skipped\r\n");
#endif

    SatPayloadApp_Init();
    DataTransfer_Init();
    SatMainLogPendingBootFlagComplete();
    FpgaProtocol_Init();
    SatMainInitConsoleRxInterrupt();
    g_tk8710BringupStage = SAT_MAIN_STAGE_CONTROL_READY;
    _enable_interrupt_();

    SatMainLog("Satellite payload control ready\r\n");
    SatMainPrintHelp();
    SatMainLog("SAT> ");

    while (1)
    {
#if !defined(FPGA_PROTOCOL_SELF_TEST)
        TK8710Tms570PollIrq();
        TK8710ProcessRuntimeWatchdog();
        SatPayloadApp_Process();
#endif
        SatPayloadApp_ProcessTelemetry();
        ExternalWatchdog_Process((uint32_t)(TK8710GetTimeUs() / 1000U));
        FpgaProtocol_Process();
        DataTransfer_Process();
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
static void SatMainLog(const char *text)
{
    while ((text != NULL) && (*text != '\0'))
    {
        while (sciIsTxReady(scilinREG) == 0U)
        {
        }
        sciSendByte(scilinREG, (uint8)*text++);
    }
}

void FpgaProtocol_Log(const char *text)
{
    uint32 guard = SAT_MAIN_SCI_FLUSH_GUARD;

    SatMainLog(text);
    while (sciIsTxReady(scilinREG) == 0U)
    {
    }
    while ((sciIsIdleDetected(scilinREG) == 0U) && (guard > 0U))
    {
        guard--;
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
    if (value < 0)
    {
        SatMainLog("-");
        SatMainLogU32((uint32)(-(value + 1)) + 1U);
    }
    else
    {
        SatMainLogU32((uint32)value);
    }
}

static void SatMainLogDb(float value)
{
    int32_t scaled = (int32_t)(value * 100.0F);
    uint32 magnitude;
    uint32 fraction;

    if (scaled < 0)
    {
        SatMainLog("-");
        magnitude = (uint32)(-(scaled + 1)) + 1U;
    }
    else
    {
        magnitude = (uint32)scaled;
    }
    fraction = magnitude % 100U;
    SatMainLogU32(magnitude / 100U);
    SatMainLog(".");
    if (fraction < 10U)
    {
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

static void SatMainLogHex16(uint16 value)
{
    char text[8];
    (void)snprintf(text, sizeof(text), "0x%04X", value);
    SatMainLog(text);
}

static void SatMainLogHexByte(uint8 value)
{
    char text[4];
    (void)snprintf(text, sizeof(text), "%02X", value);
    SatMainLog(text);
}

static uint8 SatMainIsIgnoredRxControl(uint8 ch)
{
    return ((ch < 0x20U) &&
            (ch != (uint8)'\r') &&
            (ch != (uint8)'\n') &&
            (ch != 0x08U) &&
            (ch != 0x7FU)) ? 1U : 0U;
}

static uint8 SatMainIsTrailingCommandSpace(char ch)
{
    return (((uint8)ch <= (uint8)' ') || ((uint8)ch == 0x7FU)) ? 1U : 0U;
}

static void SatMainNormalizeCommandLine(char *line)
{
    char *at;
    uint32 length;

    if (line == NULL)
    {
        return;
    }
    at = strstr(line, "AT");
    if ((at != NULL) && (at != line))
    {
        (void)memmove(line, at, strlen(at) + 1U);
    }
    length = (uint32)strlen(line);
    while ((length > 0U) && SatMainIsTrailingCommandSpace(line[length - 1U]))
    {
        line[length - 1U] = '\0';
        length--;
    }
}

static void SatMainLogUnknownCommand(const char *line)
{
    uint32 index;
    uint32 length;

    if (line == NULL)
    {
        line = "";
    }
    length = (uint32)strlen(line);
    SatMainLog("ERROR unknown command line=\"");
    SatMainLog(line);
    SatMainLog("\" hex=");
    for (index = 0U; (index < length) && (index < 48U); index++)
    {
        if (index != 0U)
        {
            SatMainLog(" ");
        }
        SatMainLogHexByte((uint8)line[index]);
    }
    if (length > 48U)
    {
        SatMainLog(" ...");
    }
    SatMainLog("\r\n");
}

static void SatMainConsoleRxPush(uint8 ch)
{
    uint32 next = g_satAtRxHead + 1U;

    if (next >= SAT_MAIN_AT_RX_RING_SIZE)
    {
        next = 0U;
    }
    if (next == g_satAtRxTail)
    {
        g_satAtRxOverflow = 1U;
        return;
    }
    g_satAtRxRing[g_satAtRxHead] = ch;
    g_satAtRxHead = next;
}

static uint8 SatMainReadConsoleRxByte(uint8 *ch)
{
    uint32 tail;

    if ((ch == NULL) || (g_satAtRxTail == g_satAtRxHead))
    {
        return 0U;
    }
    tail = g_satAtRxTail;
    *ch = g_satAtRxRing[tail];
    tail++;
    if (tail >= SAT_MAIN_AT_RX_RING_SIZE)
    {
        tail = 0U;
    }
    g_satAtRxTail = tail;
    return 1U;
}

static void SatMainResetConsoleRx(void)
{
    g_satAtRxHead = 0U;
    g_satAtRxTail = 0U;
    g_satAtRxOverflow = 0U;
    while (sciIsRxReady(scilinREG) != 0U)
    {
        (void)scilinREG->RD;
    }
    scilinREG->FLR = (uint32)(SCI_FE_INT | SCI_OE_INT | SCI_PE_INT);
}

static void SatMainInitConsoleRxInterrupt(void)
{
    SatMainResetConsoleRx();
    vimChannelMap(13U, 13U, &linHighLevelInterrupt);
    vimEnableInterrupt(13U, SYS_IRQ);
    sciEnableNotification(scilinREG,
                          SCI_RX_INT | SCI_OE_INT | SCI_FE_INT | SCI_PE_INT);
}

#pragma CODE_STATE(linHighLevelInterrupt, 32)
#pragma INTERRUPT(linHighLevelInterrupt, IRQ)
void linHighLevelInterrupt(void)
{
    while (sciIsRxReady(scilinREG) != 0U)
    {
        SatMainConsoleRxPush((uint8)(scilinREG->RD & 0xFFU));
    }
    if ((scilinREG->FLR &
         (uint32)(SCI_FE_INT | SCI_OE_INT | SCI_PE_INT)) != 0U)
    {
        scilinREG->FLR = (uint32)(SCI_FE_INT | SCI_OE_INT | SCI_PE_INT);
    }
}

static void SatMainHalt(uint32 stage, uint32 line)
{
    g_tk8710BringupStage = stage;
    g_tk8710BringupHaltLine = line;

    while (1)
    {
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
    SatMainLog("  AT+RST\r\n");
    SatMainLog("  AT+STATE\r\n");
    SatMainLog("  AT+VER\r\n");
    SatMainLog("  AT+TM\r\n");
    SatMainLog("  AT+FPGATM\r\n");
    SatMainLog("  AT+SELFTEST\r\n");
    SatMainLog("  AT+DTWRITE=S,<type>,<string>\r\n");
    SatMainLog("  AT+DTWRITE=H,<type>,<hexbytes>\r\n");
    SatMainLog("  AT+DTFILL64K\r\n");
    SatMainLog("  AT+DTCLEAR\r\n");
    SatMainLog("  AT+DTFLASHTEST\r\n");
    SatMainLog("  AT+DTFLASH2TEST\r\n");
    SatMainLog("  AT+DTFLASHID\r\n");
    SatMainLog("  AT+DTFLASHSTAT\r\n");
    SatMainLog("  AT+DTFLASHDUMP\r\n");
    SatMainLog("  AT+DTPRINT[=<offset>,<length>]\r\n");
    SatMainLog("  AT+ACM\r\n");
    SatMainLog("  AT+RREG=<addr>\r\n");
    SatMainLog("  AT+WREG=<addr>,<value>\r\n");
    SatMainLog("  AT+RRF=<rfmask>,<addr>\r\n");
    SatMainLog("  AT+WRF=<rfmask>,<addr>,<value>\r\n");
    SatMainLog("  AT+SETTXDC=<antenna>,<i16>,<q16>\r\n");
}

static void SatMainProcessConsole(void)
{
    if (SatMainReadLine(g_satAtLine, (uint32)sizeof(g_satAtLine)) > 0)
    {
        SatMainNormalizeCommandLine(g_satAtLine);
        (void)SatMainHandleCommand(g_satAtLine);
        SatMainLog("SAT> ");
    }
}

static int SatMainReadLine(char *line, uint32 capacity)
{
    static uint32 index = 0U;
    static uint8 overflow = 0U;
    uint8 ch;

    if (g_satAtRxOverflow != 0U)
    {
        SatMainResetConsoleRx();
        index = 0U;
        overflow = 0U;
        SatMainLog("ERROR console RX overflow\r\n");
        return -1;
    }

    while (SatMainReadConsoleRxByte(&ch) != 0U)
    {
        if ((ch == (uint8)'\r') || (ch == (uint8)'\n'))
        {
            if ((index == 0U) && (overflow == 0U))
            {
                continue;
            }
            line[index] = '\0';
            index = 0U;
            if (overflow != 0U)
            {
                overflow = 0U;
                SatMainLog("ERROR command too long\r\n");
                return -1;
            }
            SatMainLog("\r\n");
            return 1;
        }

        if ((ch == 0x08U) || (ch == 0x7FU))
        {
            if (index > 0U)
            {
                index--;
            }
            continue;
        }

        if (SatMainIsIgnoredRxControl(ch) != 0U)
        {
            continue;
        }

        if (overflow == 0U)
        {
            if ((index + 1U) < capacity)
            {
                line[index++] = (char)ch;
            }
            else
            {
                overflow = 1U;
            }
        }
    }
    return 0;
}

static int SatMainParseU32(const char *text, uint32 *value)
{
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL) || (*text == '\0'))
    {
        return -1;
    }
    parsed = strtoul(text, &end, 0);
    while ((*end == ' ') || (*end == '\t'))
    {
        end++;
    }
    if (*end != '\0')
    {
        return -1;
    }
    *value = (uint32)parsed;
    return 0;
}

static int SatMainParseList(char *text, uint32 *values, uint32 count)
{
    uint32 index;
    char *current = text;

    if ((text == NULL) || (values == NULL))
    {
        return -1;
    }

    for (index = 0U; index < count; index++)
    {
        char *comma = strchr(current, ',');
        if (index + 1U < count)
        {
            if (comma == NULL)
            {
                return -1;
            }
            *comma = '\0';
        }
        else if (comma != NULL)
        {
            return -1;
        }

        if (SatMainParseU32(current, &values[index]) != 0)
        {
            return -1;
        }
        if (comma != NULL)
        {
            current = comma + 1;
        }
    }
    return 0;
}

static int SatMainHexNibble(char ch, uint8 *value)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        *value = (uint8)(ch - '0');
        return 0;
    }
    if ((ch >= 'A') && (ch <= 'F'))
    {
        *value = (uint8)(ch - 'A' + 10);
        return 0;
    }
    if ((ch >= 'a') && (ch <= 'f'))
    {
        *value = (uint8)(ch - 'a' + 10);
        return 0;
    }
    return -1;
}

static int SatMainParseHexBytes(const char *text, uint8 *data, uint32 capacity, uint16 *length)
{
    uint32 textLen;
    uint32 i;

    if ((text == NULL) || (data == NULL) || (length == NULL))
    {
        return -1;
    }
    textLen = (uint32)strlen(text);
    if (((textLen & 1U) != 0U) || ((textLen / 2U) > capacity))
    {
        return -1;
    }
    for (i = 0U; i < textLen; i += 2U)
    {
        uint8 high;
        uint8 low;

        if ((SatMainHexNibble(text[i], &high) != 0) ||
            (SatMainHexNibble(text[i + 1U], &low) != 0))
        {
            return -1;
        }
        data[i / 2U] = (uint8)((high << 4U) | low);
    }
    *length = (uint16)(textLen / 2U);
    return 0;
}

static void SatMainPrintResult(SatPayloadResult result)
{
    SatMainLog((result >= SAT_PAYLOAD_OK) ? "OK result=" : "ERROR result=");
    if (result < 0)
    {
        SatMainLog("-");
        SatMainLogU32((uint32)(-result));
    }
    else
    {
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
    if (tm.lastResult < 0)
    {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastResult));
    }
    else
    {
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
    for (index = 0U; index < SAT_PAYLOAD_IRQ_COUNT; index++)
    {
        SatMainLog(" ");
        SatMainLogU32(tm.irqCounters[index]);
    }
    SatMainLog(" raw=");
    SatMainLogHex32(tm.irqStatus);
    SatMainLog(" mask=");
    SatMainLogHex32(tm.irqMask);
    SatMainLog("\r\nRX valid=");
    SatMainLogU32(tm.lastRx.valid);
    SatMainLog(" user=");
    SatMainLogHex32(tm.lastRx.userId);
    SatMainLog(" rssi=");
    if (tm.lastRx.rssi < 0)
    {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastRx.rssi));
    }
    else
    {
        SatMainLogU32((uint32)tm.lastRx.rssi);
    }
    SatMainLog(" snr=");
    SatMainLogU32(tm.lastRx.snr);
    SatMainLog(" freqOffset=");
    if (tm.lastRx.freqOffset < 0)
    {
        SatMainLog("-");
        SatMainLogU32((uint32)(-tm.lastRx.freqOffset));
    }
    else
    {
        SatMainLogU32((uint32)tm.lastRx.freqOffset);
    }
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
    for (index = 0U; index < 8U; index++)
    {
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
    for (index = 0U; index < 8U; index++)
    {
        if (index != 0U)
        {
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

static void SatMainPrintFpgaTelemetry(void)
{
    FpgaProtocolSnapshot snapshot;
    DataTransferSnapshot transfer;
    char utcText[24];
    uint32 i;

    FpgaProtocol_GetSnapshot(&snapshot);
    DataTransfer_GetSnapshot(&transfer);
    FpgaProtocol_FormatUtcTime(snapshot.utcSeconds, utcText, (uint32)sizeof(utcText));
    SatMainLog("FPGA_TM rxFrames=");
    SatMainLogU32(snapshot.rxFrameCount);
    SatMainLog(" rxErrors=");
    SatMainLogU32(snapshot.rxErrorCount);
    SatMainLog(" unsupported=");
    SatMainLogU32(snapshot.unsupportedCommandCount);
    SatMainLog(" lastCmd=");
    SatMainLogHex32(snapshot.lastCommandId);
    SatMainLog(" physicalRx=");
    SatMainLogU32(snapshot.physicalRxFrameCount);
    SatMainLog(" lastPhysicalRx=");
    for (i = 0U; i < FPGA_PROTOCOL_RC_FRAME_LEN; i++)
    {
        if (i != 0U)
        {
            SatMainLog(" ");
        }
        SatMainLogHexByte(snapshot.lastPhysicalRxFrame[i]);
    }
    SatMainLog("\r\nFPGA_PARAM mode=");
    SatMainLogU32(snapshot.workMode);
    SatMainLog(" rate=");
    SatMainLogU32(snapshot.rateMode);
    SatMainLog(" slotConfig=");
    SatMainLogU32(snapshot.slotConfig);
    SatMainLog(" txPower=");
    SatMainLogU32(snapshot.txPower);
    SatMainLog(" freqHz=");
    SatMainLogU32(snapshot.centerFreqHz);
    SatMainLog(" rfMask=");
    SatMainLogU32(snapshot.rfMask);
    SatMainLog(" bootFlag=");
    SatMainLogU32(snapshot.bootFlag);
    SatMainLog(" resetCount=");
    SatMainLogU32(snapshot.resetCount);
    SatMainLog(" resetType=");
    SatMainLogU32(snapshot.resetType);
    SatMainLog(" utcTime=");
    SatMainLog(utcText);
    SatMainLog("\r\nFPGA_REG device=");
    SatMainLogHex16(snapshot.lastRegDevice);
    SatMainLog(" addr=");
    SatMainLogHex16(snapshot.lastRegAddress);
    SatMainLog(" value=");
    SatMainLogHex32(snapshot.lastRegValue);
    SatMainLog("\r\nDT head=");
    SatMainLogHex32(transfer.head);
    SatMainLog(" tail=");
    SatMainLogHex32(transfer.tail);
    SatMainLog(" ram=");
    SatMainLogU32(transfer.ramLength);
    SatMainLog(" pending=");
    SatMainLogU32(DataTransfer_GetPendingLength());
    SatMainLog(" active=");
    SatMainLogU32(transfer.transmitActive);
    SatMainLog(" starts=");
    SatMainLogU32(transfer.txStartCount);
    SatMainLog(" built=");
    SatMainLogU32(transfer.txFramesBuilt);
    SatMainLog(" sent=");
    SatMainLogU32(transfer.txFramesSent);
    SatMainLog(" sendErr=");
    SatMainLogU32(transfer.txSendErrors);
    SatMainLog(" lastBuild=");
    SatMainLogS32(transfer.lastBuildResult);
    SatMainLog(" lastSend=");
    SatMainLogS32(transfer.lastSendResult);
    SatMainLog(" lastLen=");
    SatMainLogU32(transfer.lastFrameLength);
    SatMainLog(" lastSeq=");
    SatMainLogU32(transfer.lastPacketSeq);
    SatMainLog(" slaveReady=");
    SatMainLogU32(transfer.spi2SlaveReady);
    SatMainLog(" slaveOff=");
    SatMainLogU32(transfer.spi2SlaveOffset);
    SatMainLog(" sp2Proc=");
    SatMainLogU32(transfer.spi2SlaveProcessCount);
    SatMainLog(" sp2Rx=");
    SatMainLogU32(transfer.spi2SlaveRxReadyCount);
    SatMainLog(" sp2Pc2Chg=");
    SatMainLogU32(transfer.spi2SlavePc2ChangeCount);
    SatMainLog(" sp2FLG=");
    SatMainLogHex32(transfer.spi2SlaveFlg);
    SatMainLog(" sp2PC2=");
    SatMainLogHex32(transfer.spi2SlavePc2);
    SatMainLog(" sp2PC0=");
    SatMainLogHex32(transfer.spi2SlavePc0);
    SatMainLog(" sp2PC1=");
    SatMainLogHex32(transfer.spi2SlavePc1);
    SatMainLog(" sp2GCR1=");
    SatMainLogHex32(transfer.spi2SlaveGcr1);
    SatMainLog(" dmaStat=");
    SatMainLogHex32(transfer.spi2DmaStatus);
    SatMainLog(" dmaPend=");
    SatMainLogHex32(transfer.spi2DmaPending);
    SatMainLog(" dmaEn=");
    SatMainLogHex32(transfer.spi2DmaHwEnable);
    SatMainLog(" dmaBTC=");
    SatMainLogHex32(transfer.spi2DmaBtcFlag);
    SatMainLog(" dmaRxRem=");
    SatMainLogU32(transfer.spi2DmaRxRemaining);
    SatMainLog(" dmaTxRem=");
    SatMainLogU32(transfer.spi2DmaTxRemaining);
    SatMainLog(" dmaStarted=");
    SatMainLogU32(transfer.spi2DmaStarted);
    SatMainLog(" dmaRxFirst=");
    SatMainLogHex32(transfer.spi2DmaLastRxFirst);
    SatMainLog(" dmaRxLast=");
    SatMainLogHex32(transfer.spi2DmaLastRxLast);
    SatMainLog(" dmaRxSum=");
    SatMainLogHex32(transfer.spi2DmaLastRxChecksum);
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

    if ((count == 0U) || (count > SAT_MAIN_SWEEP_RESULT_PAGE_MAX))
    {
        SatMainLog("ERROR count must be 1..8\r\n");
        return -1;
    }
    result = SatPayloadApp_GetSweepResultInfo(&info);
    if (result != SAT_PAYLOAD_OK)
    {
        SatMainPrintResult(result);
        return -1;
    }
    if (startIndex > info.completedPoints)
    {
        SatMainLog("ERROR startIndex exceeds completedPoints\r\n");
        return -1;
    }
    result = SatPayloadApp_ReadSweepResults(startIndex, results, count,
                                            &resultCount);
    if (result != SAT_PAYLOAD_OK)
    {
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

    for (index = 0U; index < resultCount; index++)
    {
        SatMainLog("POINT index=");
        SatMainLogU32(startIndex + index);
        SatMainLog(" freqHz=");
        SatMainLogU32(results[index].frequencyHz);
        SatMainLog(" noiseDbmHz=");
        for (antenna = 0U; antenna < 8U; antenna++)
        {
            if (antenna != 0U)
            {
                SatMainLog(",");
            }
            SatMainLogDb(results[index].noiseDbmHz[antenna]);
        }
        SatMainLog("\r\n");
    }
    SatMainLog("OK\r\n");
    return 0;
}

static int SatMainHandleRegisterCommand(char *line,
                                        SatPayloadTelecommandId commandId,
                                        uint32 valueCount)
{
    uint32 values[3];
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    if (SatMainParseList(line, values, valueCount) != 0)
    {
        SatMainLog("ERROR bad register arguments\r\n");
        return -1;
    }

    (void)memset(&request, 0, sizeof(request));
    request.commandId = commandId;
    if ((commandId == SAT_PAYLOAD_TC_READ_RF_REG) ||
        (commandId == SAT_PAYLOAD_TC_WRITE_RF_REG))
    {
        request.payload.reg.rfMask = (uint8_t)values[0];
        request.payload.reg.address = (uint16_t)values[1];
        if (valueCount == 3U)
        {
            request.payload.reg.value = values[2];
        }
    }
    else
    {
        request.payload.reg.address = (uint16_t)values[0];
        if (valueCount == 2U)
        {
            request.payload.reg.value = values[1];
        }
    }

    result = SatPayloadApp_HandleTelecommand(&request, &response);
    if ((result == SAT_PAYLOAD_OK) &&
        ((commandId == SAT_PAYLOAD_TC_READ_REG) ||
         (commandId == SAT_PAYLOAD_TC_READ_RF_REG)))
    {
        SatMainLog("VALUE=");
        SatMainLogHex32(response.value);
        SatMainLog("\r\n");
    }
    SatMainPrintResult(result);
    return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
}

static void SatMainInitDefaultFpgaStoredParams(FpgaStoredParams *params)
{
    (void)memset(params, 0, sizeof(*params));
    params->workMode = DEFAULT_WORK_MODE;
    params->rateMode = DEFAULT_RATE_MODE;
    params->slotConfig = DEFAULT_SLOT_CONFIG;
    params->txPower = DEFAULT_TX_POWER;
    params->centerFreqHz = DEFAULT_FREQ;
    params->rfMask = DEFAULT_RF_MASK;
}

static void SatMainSaveFpgaParamsFromWorkParams(const SatPayloadWorkParams *params)
{
    FpgaStoredParams stored;

    if (FpgaParamStore_Load(&stored) != 0)
    {
        SatMainInitDefaultFpgaStoredParams(&stored);
    }
    stored.rateMode = params->rates[0].rateMode;
    stored.centerFreqHz = params->centerFreqHz;
    stored.rfMask = params->rfMask;

    if (FpgaParamStore_Save(&stored) != 0)
    {
        SatMainLog("WARN FPGA param store save failed\r\n");
    }
}

static void SatMainSaveFpgaMode(uint8 mode)
{
    FpgaStoredParams stored;

    if (FpgaParamStore_Load(&stored) != 0)
    {
        SatMainInitDefaultFpgaStoredParams(&stored);
    }
    stored.workMode = mode;

    if (FpgaParamStore_Save(&stored) != 0)
    {
        SatMainLog("WARN FPGA mode store save failed\r\n");
    }
}

static void SatMainSaveRfTxDc(uint8 antenna, int16_t iDc, int16_t qDc)
{
    FpgaStoredParams stored;

    if (FpgaParamStore_Load(&stored) != 0)
    {
        SatMainInitDefaultFpgaStoredParams(&stored);
    }
    stored.dcValidMask = (uint8_t)(stored.dcValidMask | (uint8_t)(1U << antenna));
    stored.dcI[antenna] = iDc;
    stored.dcQ[antenna] = qDc;

    if (FpgaParamStore_Save(&stored) != 0)
    {
        SatMainLog("WARN RF TX DC store save failed\r\n");
    }
}

static void SatMainLogPendingBootFlagComplete(void)
{
    uint8_t flag = 0xFFU;

    if (FpgaParamStore_LoadBootFlag(&flag) != 0)
    {
        return;
    }
    if (flag == 1U)
    {
        SatMainLog("FPGA firmware upgrade complete\r\n");
    }
}

static int SatMainSetRfTxDc(char *line)
{
    uint32 values[3U];
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    if ((SatMainParseList(line, values, 3U) != 0) ||
        (values[0U] >= SAT_PAYLOAD_RF_ANTENNA_COUNT) ||
        (values[1U] > 0xFFFFU) || (values[2U] > 0xFFFFU))
    {
        SatMainLog("ERROR expected antenna=0..7 and 16-bit I/Q values\r\n");
        return -1;
    }

    (void)memset(&request, 0, sizeof(request));
    request.commandId = SAT_PAYLOAD_TC_SET_RF_TX_DC;
    request.payload.rfTxDc.antenna = (uint8_t)values[0U];
    request.payload.rfTxDc.iDc = (int16_t)(uint16_t)values[1U];
    request.payload.rfTxDc.qDc = (int16_t)(uint16_t)values[2U];
    result = SatPayloadApp_HandleTelecommand(&request, &response);
    if (result == SAT_PAYLOAD_OK)
    {
        SatMainSaveRfTxDc(request.payload.rfTxDc.antenna,
                          request.payload.rfTxDc.iDc,
                          request.payload.rfTxDc.qDc);
        SatMainLog("RF_TX_DC antenna=");
        SatMainLogU32(values[0U]);
        SatMainLog(" value=");
        SatMainLogHex32(response.value);
        SatMainLog("\r\n");
    }
    SatMainPrintResult(result);
    return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
}

static int SatMainDataTransferWrite(char *line)
{
    char mode;
    char *typeText;
    char *payloadText;
    char *comma;
    uint32 type;
    uint8 bytes[SAT_MAIN_AT_LINE_MAX / 2U];
    uint16 length;

    if ((line == NULL) || (line[0] == '\0') || (line[1] != ','))
    {
        SatMainLog("ERROR DTWRITE format\r\n");
        return -1;
    }
    mode = line[0];
    typeText = &line[2];
    comma = strchr(typeText, ',');
    if (comma == NULL)
    {
        SatMainLog("ERROR DTWRITE format\r\n");
        return -1;
    }
    *comma = '\0';
    payloadText = comma + 1;
    if ((SatMainParseU32(typeText, &type) != 0) || (type > 255U))
    {
        SatMainLog("ERROR DTWRITE type\r\n");
        return -1;
    }

    if (mode == 'S')
    {
        length = (uint16)strlen(payloadText);
        if (SatMainDtWritePayloadLengthAllowed(length) == 0U)
        {
            SatMainLog("ERROR DTWRITE length\r\n");
            return -1;
        }
        if (DataTransfer_AppendString((uint8)type, payloadText, length) != 0)
        {
            SatMainLog("ERROR DTWRITE append\r\n");
            return -1;
        }
    }
    else if (mode == 'H')
    {
        if (SatMainParseHexBytes(payloadText, bytes, sizeof(bytes), &length) != 0)
        {
            SatMainLog("ERROR DTWRITE hex\r\n");
            return -1;
        }
        if (SatMainDtWritePayloadLengthAllowed(length) == 0U)
        {
            SatMainLog("ERROR DTWRITE length\r\n");
            return -1;
        }
        if (DataTransfer_AppendBytes((uint8)type, bytes, length) != 0)
        {
            SatMainLog("ERROR DTWRITE append\r\n");
            return -1;
        }
    }
    else
    {
        SatMainLog("ERROR DTWRITE mode\r\n");
        return -1;
    }

    SatMainLog("OK\r\n");
    return 0;
}

static int SatMainDataTransferFill64K(void)
{
    DataTransferSnapshot transfer;

    if (DataTransfer_AppendAlphabetPattern64K(SAT_MAIN_DTFILL64K_TYPE) != 0)
    {
        DataTransfer_GetSnapshot(&transfer);
        SatMainLog("ERROR DTFILL64K append flashErr=");
        SatMainLogU32(transfer.flashLastError);
        SatMainLog(" flashOff=");
        SatMainLogU32(transfer.flashLastOffset);
        SatMainLog("\r\n");
        return -1;
    }

    SatMainLog("DTFILL64K OK bytes=65536 records=128\r\n");
    return 0;
}

static int SatMainDataTransferClear(void)
{
    if (DataTransfer_ClearPending() != 0)
    {
        SatMainLog("ERROR DTCLEAR\r\n");
        return -1;
    }
    SatMainLog("DTCLEAR OK\r\n");
    return 0;
}

static int SatMainDataTransferFlashTest(void)
{
    uint8_t tx[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint8_t rx[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint8_t erased[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint32 i;
    uint32 erasedMismatch = SAT_MAIN_DT_FLASH_TEST_LEN;
    uint32 mismatch = SAT_MAIN_DT_FLASH_TEST_LEN;

    for (i = 0U; i < SAT_MAIN_DT_FLASH_TEST_LEN; i++)
    {
        tx[i] = (uint8_t)(0xA5U ^ i);
        rx[i] = 0U;
    }

    (void)DataTransfer_ClearPending();
    if (SpiFlash_Init() != 0)
    {
        SatMainLog("DTFLASHTEST init=FAIL\r\n");
        return -1;
    }
    if (SpiFlash_EraseSector(SAT_MAIN_DT_FLASH_TEST_ADDR) != 0)
    {
        SatMainLog("DTFLASHTEST erase=FAIL\r\n");
        return -1;
    }
    if (SpiFlash_Read(SAT_MAIN_DT_FLASH_TEST_ADDR, erased, sizeof(erased)) != 0)
    {
        SatMainLog("DTFLASHTEST eraseRead=FAIL\r\n");
        return -1;
    }
    for (i = 0U; i < SAT_MAIN_DT_FLASH_TEST_LEN; i++)
    {
        if (erased[i] != 0xFFU)
        {
            erasedMismatch = i;
            break;
        }
    }
    SatMainLog("DTFLASHTEST erased first=");
    SatMainLogHexByte(erased[0]);
    SatMainLog(" last=");
    SatMainLogHexByte(erased[SAT_MAIN_DT_FLASH_TEST_LEN - 1U]);
    SatMainLog(" mismatch=");
    SatMainLogU32(erasedMismatch);
    SatMainLog("\r\n");
    if (SpiFlash_PageProgram(SAT_MAIN_DT_FLASH_TEST_ADDR, tx, sizeof(tx)) != 0)
    {
        SatMainLog("DTFLASHTEST program=FAIL\r\n");
        return -1;
    }
    if (SpiFlash_Read(SAT_MAIN_DT_FLASH_TEST_ADDR, rx, sizeof(rx)) != 0)
    {
        SatMainLog("DTFLASHTEST read=FAIL\r\n");
        return -1;
    }

    for (i = 0U; i < SAT_MAIN_DT_FLASH_TEST_LEN; i++)
    {
        if (rx[i] != tx[i])
        {
            mismatch = i;
            break;
        }
    }

    SatMainLog("DTFLASHTEST first=");
    SatMainLogHexByte(rx[0]);
    SatMainLog(" last=");
    SatMainLogHexByte(rx[SAT_MAIN_DT_FLASH_TEST_LEN - 1U]);
    SatMainLog(" mismatch=");
    SatMainLogU32(mismatch);
    SatMainLog("\r\n");

    if (mismatch != SAT_MAIN_DT_FLASH_TEST_LEN)
    {
        SatMainLog("DTFLASHTEST verify=FAIL\r\n");
        return -1;
    }
    SatMainLog("DTFLASHTEST OK\r\n");
    return 0;
}

static uint32 SatMainFindMismatch(const uint8_t *rx, const uint8_t *tx, uint32 len)
{
    uint32 i;

    for (i = 0U; i < len; i++)
    {
        if (rx[i] != tx[i])
        {
            return i;
        }
    }
    return len;
}

static void SatMainPrintFlash2Result(const char *prefix, const uint8_t *rx,
                                     uint32 mismatch)
{
    SatMainLog(prefix);
    SatMainLog(" first=");
    SatMainLogHexByte(rx[0]);
    SatMainLog(" last=");
    SatMainLogHexByte(rx[SAT_MAIN_DT_FLASH_TEST_LEN - 1U]);
    SatMainLog(" mismatch=");
    SatMainLogU32(mismatch);
    SatMainLog("\r\n");
}

static int SatMainDataTransferFlash2Test(void)
{
    uint8_t tx0[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint8_t tx1[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint8_t rx0[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint8_t rx1[SAT_MAIN_DT_FLASH_TEST_LEN];
    uint32 addr0 = DATA_TRANSFER_FLASH_DATA_START;
    uint32 addr1 = DATA_TRANSFER_FLASH_DATA_START + DATA_TRANSFER_FLASH_ERASE_SIZE;
    uint32 mismatch0;
    uint32 mismatch1;
    uint32 i;

    for (i = 0U; i < SAT_MAIN_DT_FLASH_TEST_LEN; i++)
    {
        tx0[i] = (uint8_t)(0xA5U ^ i);
        tx1[i] = (uint8_t)(0x5AU ^ i);
        rx0[i] = 0U;
        rx1[i] = 0U;
    }

    (void)DataTransfer_ClearPending();
    if (SpiFlash_Init() != 0)
    {
        SatMainLog("DTFLASH2TEST init=FAIL\r\n");
        return -1;
    }
    if ((SpiFlash_EraseSector(addr0) != 0) ||
        (SpiFlash_PageProgram(addr0, tx0, sizeof(tx0)) != 0) ||
        (SpiFlash_EraseSector(addr1) != 0) ||
        (SpiFlash_PageProgram(addr1, tx1, sizeof(tx1)) != 0) ||
        (SpiFlash_Read(addr0, rx0, sizeof(rx0)) != 0) ||
        (SpiFlash_Read(addr1, rx1, sizeof(rx1)) != 0))
    {
        SatMainLog("DTFLASH2TEST io=FAIL\r\n");
        return -1;
    }

    mismatch0 = SatMainFindMismatch(rx0, tx0, SAT_MAIN_DT_FLASH_TEST_LEN);
    mismatch1 = SatMainFindMismatch(rx1, tx1, SAT_MAIN_DT_FLASH_TEST_LEN);
    SatMainPrintFlash2Result("DTFLASH2TEST s0", rx0, mismatch0);
    SatMainPrintFlash2Result("DTFLASH2TEST s1", rx1, mismatch1);

    if ((mismatch0 != SAT_MAIN_DT_FLASH_TEST_LEN) ||
        (mismatch1 != SAT_MAIN_DT_FLASH_TEST_LEN))
    {
        SatMainLog("DTFLASH2TEST verify=FAIL\r\n");
        return -1;
    }
    SatMainLog("DTFLASH2TEST OK\r\n");
    return 0;
}

static int SatMainDataTransferFlashId(void)
{
    uint8_t id[3];

    if ((SpiFlash_Init() != 0) || (SpiFlash_ReadJedecId(id) != 0))
    {
        SatMainLog("DTFLASHID read=FAIL\r\n");
        return -1;
    }
    SatMainLog("DTFLASHID JEDEC=");
    SatMainLogHexByte(id[0]);
    SatMainLogHexByte(id[1]);
    SatMainLogHexByte(id[2]);
    SatMainLog("\r\n");
    return 0;
}

static int SatMainDataTransferFlashStat(void)
{
    uint8_t sr1 = 0U;
    uint8_t sr2 = 0U;

    if ((SpiFlash_Init() != 0) ||
        (SpiFlash_ReadStatusRegisters(&sr1, &sr2) != 0))
    {
        SatMainLog("DTFLASHSTAT read=FAIL\r\n");
        return -1;
    }
    SatMainLog("DTFLASHSTAT SR1=");
    SatMainLogHexByte(sr1);
    SatMainLog(" SR2=");
    SatMainLogHexByte(sr2);
    SatMainLog("\r\n");
    return 0;
}

static void SatMainDataTransferDumpAddr(uint32 address)
{
    uint8_t data[16];
    uint32 i;

    SatMainLog("addr=");
    SatMainLogHex32(address);
    if (SpiFlash_Read(address, data, sizeof(data)) != 0)
    {
        SatMainLog(" read=FAIL\r\n");
        return;
    }
    SatMainLog(" data=");
    for (i = 0U; i < sizeof(data); i++)
    {
        SatMainLogHexByte(data[i]);
    }
    SatMainLog("\r\n");
}

static int SatMainDataTransferFlashDump(void)
{
    if (SpiFlash_Init() != 0)
    {
        SatMainLog("DTFLASHDUMP init=FAIL\r\n");
        return -1;
    }
    SatMainDataTransferDumpAddr(DATA_TRANSFER_FLASH_METADATA_START);
    SatMainDataTransferDumpAddr(DATA_TRANSFER_FLASH_DATA_START);
    SatMainDataTransferDumpAddr(DATA_TRANSFER_FLASH_DATA_START + DATA_TRANSFER_FLASH_ERASE_SIZE);
    return 0;
}

static int SatMainDataTransferPrint(char *line)
{
    uint32 values[2];
    uint32 total;
    uint32 count;
    uint32 printed = 0U;
    uint8 bytes[SAT_MAIN_DT_PRINT_CHUNK];

    if ((line == NULL) || (*line == '\0'))
    {
        values[0] = 0U;
        count = DataTransfer_GetPendingLength();
    }
    else if (SatMainParseList(line, values, 2U) != 0)
    {
        SatMainLog("ERROR DTPRINT needs offset,length\r\n");
        return -1;
    }
    else
    {
        count = values[1];
        if (count > SAT_MAIN_DT_PRINT_MAX)
        {
            count = SAT_MAIN_DT_PRINT_MAX;
        }
    }
    total = DataTransfer_GetPendingLength();
    if (values[0] >= total)
    {
        count = 0U;
    }
    else if (count > (total - values[0]))
    {
        count = total - values[0];
    }

    SatMainLog("DT len=");
    SatMainLogU32(total);
    SatMainLog(" offset=");
    SatMainLogU32(values[0]);
    SatMainLog(" count=");
    SatMainLogU32(count);
    SatMainLog("\r\n");

    while (printed < count)
    {
        uint32 chunk = count - printed;
        uint32 actual;
        uint32 i;

        if (chunk > sizeof(bytes))
        {
            chunk = sizeof(bytes);
        }
        actual = DataTransfer_ReadPending(values[0] + printed, bytes, chunk);
        if (actual != chunk)
        {
            SatMainLog("\r\nERROR DTPRINT read\r\n");
            return -1;
        }
        for (i = 0U; i < actual; i++)
        {
            if ((printed != 0U) || (i != 0U))
            {
                SatMainLog(" ");
            }
            SatMainLogHexByte(bytes[i]);
        }
        printed += actual;
    }
    if (count != 0U)
    {
        SatMainLog("\r\n");
    }
    SatMainLog("OK\r\n");
    return 0;
}

static int SatMainHandleCommand(char *line)
{
    uint32 values[14];
    uint32 value;
    uint32 stepFreq;
    SatPayloadWorkParams params;
    SatPayloadTelecommand request;
    SatPayloadTelecommandResponse response;
    SatPayloadResult result;

    while ((*line == ' ') || (*line == '\t'))
    {
        line++;
    }
    if (strcmp(line, "AT") == 0)
    {
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+HELP") == 0)
    {
        SatMainPrintHelp();
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strncmp(line, "AT+SETPARAM=", 12) == 0)
    {
        if (SatMainParseList(line + 12, values, 14U) != 0)
        {
            SatMainLog("ERROR SETPARAM needs 14 values\r\n");
            return -1;
        }
        (void)memset(&params, 0, sizeof(params));
        params.rateCount = DEFAULT_RATE_COUNT;
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
        params.bcnBits = DEFAULT_BCN_BITS;
        params.txBcnAntennaMask = params.antennaMask;
        params.mdAgc = DEFAULT_MD_AGC;
        params.maxFrameCount = DEFAULT_MAX_FRAME_COUNT;
        params.sweepStartFreqHz = params.centerFreqHz;
        params.sweepEndFreqHz = params.centerFreqHz;
        params.sweepMode = DEFAULT_SWEEP_MODE;
        params.toneFreq = DEFAULT_TONE_FREQ;
        params.toneGain = DEFAULT_TONE_GAIN;
        params.acmCalibCount = DEFAULT_ACM_CALIB_COUNT;
        params.acmSnrThreshold = DEFAULT_ACM_SNR_THRESHOLD;
        result = SatPayloadApp_SetWorkParams(&params);
        if (result == SAT_PAYLOAD_OK)
        {
            g_satAtPendingParams = params;
            g_satAtPendingParamsValid = 1U;
            SatMainSaveFpgaParamsFromWorkParams(&params);
        }
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+SETSWEEP=", 12) == 0)
    {
        if (SatMainParseList(line + 12, values, 3U) != 0)
        {
            SatMainLog("ERROR SETSWEEP needs 3 values\r\n");
            return -1;
        }
        if (g_satAtPendingParamsValid == 0U)
        {
            SatMainLog("ERROR run AT+SETPARAM first\r\n");
            return -1;
        }
        if ((values[0] == 0U) || (values[1] < values[0]) ||
            (values[2] > 3U))
        {
            SatMainLog("ERROR invalid sweep range or mode\r\n");
            return -1;
        }
        switch (values[2])
        {
        case 0U:
            stepFreq = 62500U;
            break;
        case 1U:
            stepFreq = 125000U;
            break;
        case 2U:
            stepFreq = 250000U;
            break;
        default:
            stepFreq = 500000U;
            break;
        }
        if ((((values[1] - values[0]) / stepFreq) + 1U) >
            TRM_SWEEP_MAX_RESULT_POINTS)
        {
            SatMainLog("ERROR sweep exceeds 1024 points\r\n");
            return -1;
        }
        params = g_satAtPendingParams;
        params.sweepStartFreqHz = values[0];
        params.sweepEndFreqHz = values[1];
        params.sweepMode = (uint8_t)values[2];
        result = SatPayloadApp_SetWorkParams(&params);
        if (result == SAT_PAYLOAD_OK)
        {
            g_satAtPendingParams = params;
        }
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+SWEEPRESULT=", 15) == 0)
    {
        if (SatMainParseList(line + 15, values, 2U) != 0)
        {
            SatMainLog("ERROR SWEEPRESULT needs 2 values\r\n");
            return -1;
        }
        return SatMainPrintSweepResults(values[0], values[1]);
    }
    if (strncmp(line, "AT+SETMODE=", 11) == 0)
    {
        if (SatMainParseU32(line + 11, &value) != 0)
        {
            SatMainLog("ERROR bad mode\r\n");
            return -1;
        }
        result = SatPayloadApp_SetWorkMode((uint8_t)value);
        if (result == SAT_PAYLOAD_OK)
        {
            SatMainSaveFpgaMode((uint8)value);
        }
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strcmp(line, "AT+STOP") == 0)
    {
        result = SatPayloadApp_Stop();
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strcmp(line, "AT+RST") == 0)
    {
        SatMainLog("OK\r\n");
        systemREG1->SYSECR = (uint32)(0x10U << 14U);
        return 0;
    }
    if (strcmp(line, "AT+STATE") == 0)
    {
        SatMainLog("STATE=");
        SatMainLog(SatPayloadApp_StateName(SatPayloadApp_GetState()));
        SatMainLog(" MODE=");
        SatMainLogU32(SatPayloadApp_GetActiveMode());
        SatMainLog("\r\nOK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+VER") == 0)
    {
        SatMainLog("VER=");
        SatMainLogU32(FPGA_PROTOCOL_VERSION_MAJOR);
        SatMainLog(".");
        SatMainLogU32(FPGA_PROTOCOL_VERSION_MINOR);
        SatMainLog(".");
        SatMainLogU32(FPGA_PROTOCOL_VERSION_PATCH);
        SatMainLog("\r\nOK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+FPGATM") == 0)
    {
        SatMainPrintFpgaTelemetry();
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+SELFTEST") == 0)
    {
        SatMainLog("SELFTEST faults=");
        SatMainLogHex32(AppSelfTest_Run());
        SatMainLog("\r\nOK\r\n");
        return 0;
    }
    if (strncmp(line, "AT+DTWRITE=", 11) == 0)
    {
        return SatMainDataTransferWrite(line + 11);
    }
    if (strcmp(line, "AT+DTFILL64K") == 0)
    {
        return SatMainDataTransferFill64K();
    }
    if (strcmp(line, "AT+DTCLEAR") == 0)
    {
        return SatMainDataTransferClear();
    }
    if (strcmp(line, "AT+DTFLASHTEST") == 0)
    {
        return SatMainDataTransferFlashTest();
    }
    if (strcmp(line, "AT+DTFLASH2TEST") == 0)
    {
        return SatMainDataTransferFlash2Test();
    }
    if (strcmp(line, "AT+DTFLASHID") == 0)
    {
        return SatMainDataTransferFlashId();
    }
    if (strcmp(line, "AT+DTFLASHSTAT") == 0)
    {
        return SatMainDataTransferFlashStat();
    }
    if (strcmp(line, "AT+DTFLASHDUMP") == 0)
    {
        return SatMainDataTransferFlashDump();
    }
    if (strncmp(line, "AT+DTPRINT=", 11) == 0)
    {
        return SatMainDataTransferPrint(line + 11);
    }
    if (strcmp(line, "AT+DTPRINT") == 0)
    {
        return SatMainDataTransferPrint("");
    }
    if (strcmp(line, "AT+TM") == 0)
    {
        SatMainPrintTelemetry();
        SatMainLog("OK\r\n");
        return 0;
    }
    if (strcmp(line, "AT+ACM") == 0)
    {
        (void)memset(&request, 0, sizeof(request));
        request.commandId = SAT_PAYLOAD_TC_REQUEST_ACM;
        result = SatPayloadApp_HandleTelecommand(&request, &response);
        SatMainPrintResult(result);
        return (result >= SAT_PAYLOAD_OK) ? 0 : -1;
    }
    if (strncmp(line, "AT+RREG=", 8) == 0)
    {
        return SatMainHandleRegisterCommand(line + 8,
                                            SAT_PAYLOAD_TC_READ_REG, 1U);
    }
    if (strncmp(line, "AT+WREG=", 8) == 0)
    {
        return SatMainHandleRegisterCommand(line + 8,
                                            SAT_PAYLOAD_TC_WRITE_REG, 2U);
    }
    if (strncmp(line, "AT+RRF=", 7) == 0)
    {
        return SatMainHandleRegisterCommand(line + 7,
                                            SAT_PAYLOAD_TC_READ_RF_REG, 2U);
    }
    if (strncmp(line, "AT+WRF=", 7) == 0)
    {
        return SatMainHandleRegisterCommand(line + 7,
                                            SAT_PAYLOAD_TC_WRITE_RF_REG, 3U);
    }
    if (strncmp(line, "AT+SETTXDC=", 11) == 0)
    {
        return SatMainSetRfTxDc(line + 11);
    }

    SatMainLogUnknownCommand(line);
    return -1;
}
#endif
/* USER CODE END */
