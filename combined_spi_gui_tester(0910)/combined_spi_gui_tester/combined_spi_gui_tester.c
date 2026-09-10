#define WIN32_LEAN_AND_MEAN

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#endif

#include "../fpga_jtool_tester/jtool.h"

#define APP_TITLE "TMS570 SPI1/SPI2 Combined Tester"
#define REPORT_TOOL_VERSION "combined_spi_gui_tester v0.2"
#define REPORT_TEST_VERSION "载荷基带软件测试报告_v0.1自动报告"
#define DEFAULT_UART_BAUD 1000000U
#define RC_LEN 10U
#define SPI1_PAGE_LEN 128U
#define TM_LEN 144U
#define DT_FRAME_SYNC 0x1ACFFC1DUL
#define DT_FRAME_LEN 512U
#define DT_FRAME_DATA_LEN 502U
#define DT_RECORD_HEADER_LEN 8U
#define DT_FORMAT_STRING 0x01U
#define DT_FORMAT_BINARY 0x02U
#define MAX_SPI_DEVICES 16
#define LOG_MAX_CHARS 60000
#define LOG_EDIT_TEXT_LIMIT (LOG_MAX_CHARS + 8192)
#define FIXED_SPI1_MODE ((SPICK_TYPE)0)
#define FIXED_SPI2_MODE ((SPICK_TYPE)2)
#define FIXED_SPI2_DELAY_MS 1
#define SPI2_IDLE_DELAY_MS 20
#define SPI2_CAPTURE_TIMEOUT_MS 3000U
#define SPI2_CAPTURE_IDLE_TIMEOUT_MS 2000U
#define SPI2_CAPTURE_MAX_FRAMES 140000U
#define SPI2_CAPTURE_ARM_DELAY_MS 100U
#define SPI2_KEEP_DEVICE_OPEN 1
#define SPI1_CMD_AFTER_TM_PAUSE_MS 120U
#define REPORT_UART_WAIT_TIMEOUT_MS 3000U
#define REPORT_TELEMETRY_VERIFY_TIMEOUT_MS 2000U
#define REPORT_DETAIL_AT_SUMMARY_CHARS 96U
#define CMD_PARAMS_WIDTH 192
#define CMD_SEND_X 970
#define CMD_HINT_MAX_VISIBLE_CHARS 78U
#define CMD_HINT_Y 66
#define CMD_HINT_H 18
#define LOG_HEADER_Y 92
#define LOG_BUTTON_Y 88
#define LOG_EDIT_TOP 116

#define IDC_SCAN 1001
#define IDC_SPI1_DEVICE 1002
#define IDC_SPI2_DEVICE 1003
#define IDC_SPI1_TOGGLE 1007
#define IDC_SPI2_TOGGLE 1009
#define IDC_TM_RX_TOGGLE 1010
#define IDC_CMD_SELECT 1011
#define IDC_CMD_PARAMS 1012
#define IDC_CMD_SEND 1013
#define IDC_SPI1_LOG 1015
#define IDC_SPI2_LOG 1016
#define IDC_SPI1_CLEAR 1017
#define IDC_SPI2_CLEAR 1018
#define IDC_STATUS 1019
#define IDC_UART_PORT 1020
#define IDC_UART_BAUD 1021
#define IDC_REPORT_RUN 1022
#define IDC_LONG_RUN 1023
#define IDC_REPORT_PROGRESS 1024

#define IDT_DEVICE_SCAN 2001

#define WM_APP_LOG_SPI1 (WM_APP + 1)
#define WM_APP_LOG_SPI2 (WM_APP + 2)
#define WM_APP_STATUS   (WM_APP + 3)
#define WM_APP_REFRESH_TOGGLES (WM_APP + 4)

#define DEVICE_ENTRY_MANUAL 0
#define DEVICE_ENTRY_SN 1
#define GUI_COMMAND_MANUAL_RAW 0xFFU

typedef char* (*DevicesScanFn)(int DevType, int* OutCnt);
typedef void* (*DevOpenFn)(int DevType, char* Sn, int Id);
typedef BOOL (*DevCloseFn)(void* DevHandle);
typedef ErrorType (*SPIWriteReadFn)(void* DevHandle, SPICK_TYPE ck,
                                    SPIFIRSTBIT_TYPE firstbit, uint32_t len,
                                    uint8_t* dataw, uint8_t* bufr);
typedef ErrorType (*JSPISetVioFn)(void* DevHandle, uint8_t val);
typedef ErrorType (*JSPISetSpeedFn)(void* DevHandle, uint8_t val);

typedef struct
{
    uint8_t* data;
    uint32_t length;
    uint32_t capacity;
} ByteBuffer;

typedef struct
{
    uint32_t* data;
    uint32_t length;
    uint32_t capacity;
} FrameIndexBuffer;

typedef struct
{
    ByteBuffer pending;
    FrameIndexBuffer pendingFrames;
    uint32_t nextIndex;
    uint32_t lastRecordStartFrame;
    uint32_t lastRecordEndFrame;
    uint32_t lastRecordBytes;
} RecordStream;

typedef struct
{
    int spi1Index;
    int spi2Index;
} DeviceAssignment;

typedef struct
{
    int type;
    int id;
    char sn[64];
    char label[96];
} DeviceEntry;

typedef struct
{
    uint8_t pending;
    uint8_t repeatRemaining;
    uint8_t tx[TM_LEN];
    uint32_t txLength;
    uint8_t isManual;
    uint8_t requestSpi2Capture;
} Spi1CommandQueue;

typedef struct
{
    int spi1X;
    int spi1Y;
    int spi1W;
    int spi1H;
    int spi2X;
    int spi2Y;
    int spi2W;
    int spi2H;
    int statusX;
    int statusY;
    int statusW;
    int statusH;
} MainLayout;

typedef struct
{
    char id[16];
    char item[96];
    char result[32];
    char detail[256];
} ReportRow;

typedef struct
{
    ReportRow rows[96];
    uint32_t count;
    uint32_t pass;
    uint32_t failed;
    uint32_t skip;
} ReportTable;

typedef struct
{
    char uartPort[32];
    uint32_t uartBaud;
    int spi1Index;
    int spi2Index;
    char startTime[32];
    char endTime[32];
} ReportRunConfig;

typedef struct
{
    const char* id;
    const char* item;
} ReportItemDef;

typedef struct
{
    const char* title;
    const ReportItemDef* items;
    uint32_t count;
} ReportSectionDef;

typedef struct
{
    HWND hwnd;
    HWND spi1Log;
    HWND spi2Log;
    HWND status;
    HWND spi1Device;
    HWND spi2Device;
    HWND spi1Toggle;
    HWND spi2Toggle;
    HWND tmRxToggle;
    HWND cmdSelect;
    HWND cmdParams;
    HWND cmdHint;
    HWND uartPort;
    HWND uartBaud;
    HWND reportRun;
    HWND longRun;
    HWND reportProgress;
    DeviceEntry devices[MAX_SPI_DEVICES];
    Spi1CommandQueue spi1Queue;
    CRITICAL_SECTION spi1QueueLock;
    CRITICAL_SECTION spiIoLock;
    int spi1QueueLockReady;
    int spiIoLockReady;
    char deviceSignature[512];
    char uartSignature[512];
    volatile LONG spi1Run;
    volatile LONG spi1TelemetryEnabled;
    volatile LONG spi2Run;
    volatile LONG spi2CaptureRequest;
    volatile LONG spi2ContinuousCapture;
    volatile LONG spi1SuspendForSpi2;
    volatile LONG reportRunActive;
    volatile LONG reportStop;
    volatile LONG longRunActive;
    volatile LONG longRunStop;
    volatile LONG longRunIterations;
    volatile LONG longRunFailures;
    HANDLE spi1Thread;
    HANDLE spi2Thread;
    HANDLE reportThread;
    HANDLE longRunThread;
    int deviceCount;
} AppState;

static DevicesScanFn g_DevicesScan;
static DevOpenFn g_DevOpen;
static DevCloseFn g_DevClose;
static SPIWriteReadFn g_SPIWriteRead;
static JSPISetVioFn g_JSPISetVio;
static JSPISetSpeedFn g_JSPISetSpeed;
static AppState g_app;

static const ReportItemDef REPORT_RC_ERROR_ITEMS[] = {
    {"RC-ERR-01", "帧头错误"}, {"RC-ERR-02", "长度错误-短帧"},
    {"RC-ERR-03", "长度错误-长帧"}, {"RC-ERR-04", "校验错误"},
    {"RC-ERR-05", "参数异常"}
};

static const ReportItemDef REPORT_TC_ITEMS[] = {
    {"TC-01", "工作模式"}, {"TC-02", "设置速率"},
    {"TC-03", "设置时隙"}, {"TC-04", "发射功率"},
    {"TC-05", "设置频率"}, {"TC-06", "设置射频通道"},
    {"TC-07", "单机复位"}, {"TC-08", "固件升级"},
    {"TC-09", "数据传送"}, {"TC-10", "写入寄存器"},
    {"TC-11", "读取寄存器"}, {"TC-12", "版本回滚"},
    {"TC-13", "UTC时间"}, {"TC-14", "配置直流参数"}
};

static const ReportItemDef REPORT_TM_ITEMS[] = {{"TM-01", "遥测周期读取"}};
static const ReportItemDef REPORT_DT_ITEMS[] = {
    {"DT-01", "字符格式不满512字节包"}, {"DT-02", "hex格式不满512字节包"},
    {"DT-03", "字符格式512字节包"}, {"DT-04", "hex格式512字节包"}
};

static const ReportItemDef REPORT_ER_ITEMS[] = {
    {"ER-01", "软件复位恢复"}, {"ER-02", "看门狗复位恢复"},
    {"ER-03", "FPGA复位模拟"}, {"ER-04", "SPI1时钟异常"},
    {"ER-05", "SPI2数传异常"}, {"ER-06", "快速重复命令"},
    {"ER-07", "长时间运行"}
};

static const ReportItemDef REPORT_RF_ITEMS[] = {
    {"RF-01", "参数配置"}, {"RF-02", "底噪检测"},
    {"RF-03", "单tone模式"}, {"RF-04", "ACM校准"},
    {"RF-05", "信号采集"}, {"RF-06", "模式A/B/C"},
    {"RF-07", "单用户/速率0双向收发"}, {"RF-08", "单用户/速率1双向收发"},
    {"RF-09", "单用户/速率2双向收发"}, {"RF-10", "16用户/速率0双向收发"},
    {"RF-11", "16用户/速率1双向收发"}, {"RF-12", "16用户/速率2双向收发"},
    {"RF-13", "稳定性测试"}
};

static const ReportItemDef REPORT_FL_ITEMS[] = {
    {"FL-01", "正常业务"}, {"FL-02", "配置覆盖+遥测确认"},
    {"FL-03", "错误帧夹杂+状态保持"}, {"FL-04", "遥控遥测并发+配置更新"},
    {"FL-05", "数传启动+遥测并行"}, {"FL-06", "数传启动+中止+重启"},
    {"FL-07", "UTC连续下发+缓存+数传"}, {"FL-08", "寄存器写入+读取+遥测返回"},
    {"FL-09", "复位恢复+遥测确认"}, {"FL-10", "看门狗复位+启动路径"},
    {"FL-11", "高频遥控压力+遥测"}, {"FL-12", "长时间运行闭环"}
};

static const ReportSectionDef REPORT_SECTIONS[] = {
    {"### 3.1 遥控错误帧类型", REPORT_RC_ERROR_ITEMS, (uint32_t)(sizeof(REPORT_RC_ERROR_ITEMS) / sizeof(REPORT_RC_ERROR_ITEMS[0]))},
    {"### 3.2 单条遥控指令测试", REPORT_TC_ITEMS, (uint32_t)(sizeof(REPORT_TC_ITEMS) / sizeof(REPORT_TC_ITEMS[0]))},
    {"### 3.3 遥测测试", REPORT_TM_ITEMS, (uint32_t)(sizeof(REPORT_TM_ITEMS) / sizeof(REPORT_TM_ITEMS[0]))},
    {"### 3.4 数传测试", REPORT_DT_ITEMS, (uint32_t)(sizeof(REPORT_DT_ITEMS) / sizeof(REPORT_DT_ITEMS[0]))},
    {"### 3.5 复位、异常和稳定性测试", REPORT_ER_ITEMS, (uint32_t)(sizeof(REPORT_ER_ITEMS) / sizeof(REPORT_ER_ITEMS[0]))},
    {"### 3.6 基带/射频软件功能测试", REPORT_RF_ITEMS, (uint32_t)(sizeof(REPORT_RF_ITEMS) / sizeof(REPORT_RF_ITEMS[0]))},
    {"### 3.7 完整流程和组合流程测试", REPORT_FL_ITEMS, (uint32_t)(sizeof(REPORT_FL_ITEMS) / sizeof(REPORT_FL_ITEMS[0]))}
};

static uint32_t ReportTotalItemCount(void)
{
    uint32_t total = 0U;
    uint32_t i;

    for (i = 0U; i < (sizeof(REPORT_SECTIONS) / sizeof(REPORT_SECTIONS[0])); i++) {
        total += REPORT_SECTIONS[i].count;
    }
    return total;
}

static uint32_t ReportProgressPercent(uint32_t done, uint32_t total)
{
    if (total == 0U) {
        return 0U;
    }
    if (done >= total) {
        return 100U;
    }
    return (uint32_t)(((uint64_t)done * 100ULL) / (uint64_t)total);
}

static int ReportStopRequested(void)
{
    return (InterlockedCompareExchange(&g_app.reportStop, 0, 0) != 0) ? 1 : 0;
}

static UINT ReportLogTargetForId(const char* id)
{
    if ((id != NULL) &&
        ((strncmp(id, "DT-", 3U) == 0) || (strcmp(id, "TC-09") == 0) ||
         (strcmp(id, "TC-13") == 0) || (strcmp(id, "ER-05") == 0) ||
         (strcmp(id, "FL-05") == 0) || (strcmp(id, "FL-06") == 0) ||
         (strcmp(id, "FL-07") == 0) || (strcmp(id, "FL-12") == 0))) {
        return WM_APP_LOG_SPI2;
    }
    return WM_APP_LOG_SPI1;
}

static void ReportUpdateProgress(const ReportTable* table)
{
    uint32_t percent;
    char status[96];
    uint32_t total = ReportTotalItemCount();
    uint32_t done = (table == NULL) ? 0U : table->count;

    percent = ReportProgressPercent(done, total);
    if (g_app.reportProgress != NULL) {
        SendMessageA(g_app.reportProgress, PBM_SETPOS, (WPARAM)percent, 0);
    }
    if (InterlockedCompareExchange(&g_app.reportRunActive, 1, 1) != 0) {
        (void)snprintf(status, sizeof(status), "report progress %lu/%lu (%lu%%)",
                       (unsigned long)done, (unsigned long)total, (unsigned long)percent);
        SetWindowTextA(g_app.status, status);
    }
}

static int BuildGuiCommandFromText(uint8_t command, const char* text, uint8_t* tx,
                                   uint32_t* txLength, uint8_t* requestSpi2Capture,
                                   char* error, size_t errorSize);
static void* OpenSpiDevice(int index, UINT logMsg);
static int SelectedComboIndex(HWND combo);
static void PostLog(UINT msg, const char* fmt, ...);
static int LoadJtoolApi(void);
static int ValidateTelemetry(const uint8_t* rx);
static int ParseHexBytes(const char* text, uint8_t* out, uint32_t maxLen, uint32_t* outLen);
static int ValidateDtFrame(const uint8_t* frame, uint16_t* seq, uint16_t* length,
                           char* error, size_t errorSize);
static int AppendFramePayload(ByteBuffer* stream, const uint8_t* frame,
                              char* error, size_t errorSize);
static int RecordStreamAppendAndMatch(RecordStream* stream, const uint8_t* data,
                                      uint32_t length, uint32_t minTimestamp,
                                      uint32_t maxTimestamp,
                                      uint8_t expectedFormat, uint8_t expectedType,
                                      const uint8_t* expectedPayload,
                                      uint16_t expectedLength);

static ErrorType SpiWriteReadLocked(void* dev, SPICK_TYPE mode,
                                    SPIFIRSTBIT_TYPE firstbit, uint32_t len,
                                    uint8_t* tx, uint8_t* rx)
{
    ErrorType result;

    if (g_app.spiIoLockReady != 0) {
        EnterCriticalSection(&g_app.spiIoLock);
    }
    result = g_SPIWriteRead(dev, mode, firstbit, len, tx, rx);
    if (g_app.spiIoLockReady != 0) {
        LeaveCriticalSection(&g_app.spiIoLock);
    }
    return result;
}

static uint8_t Checksum8(const uint8_t* data, uint32_t start, uint32_t end)
{
    uint8_t sum = 0U;
    uint32_t i;

    for (i = start; i <= end; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static uint16_t ReadBe16(const uint8_t* data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static uint32_t ReadBe32(const uint8_t* data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           data[3];
}

static int16_t ReadS16Be(const uint8_t* data)
{
    return (int16_t)ReadBe16(data);
}

static void WriteBe16(uint8_t* data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static void WriteBe32(uint8_t* data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static uint16_t DtFrameChecksum(const uint8_t* frame)
{
    uint32_t i;
    uint32_t sum = 0U;

    for (i = 0U; i < (DT_FRAME_LEN - 2U); i += 2U) {
        sum += ((uint16_t)frame[i] << 8U) | frame[i + 1U];
    }
    return (uint16_t)sum;
}

static uint32_t TickMs(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    return 0U;
#endif
}

static int IsLeapYear(uint32_t year)
{
    return (((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U))) ? 1 : 0;
}

static void FormatDtTimestamp(uint32_t seconds, char* out, size_t outSize)
{
    static const uint8_t monthDays[12] = {31U, 28U, 31U, 30U, 31U, 30U,
                                          31U, 31U, 30U, 31U, 30U, 31U};
    uint32_t days = seconds / 86400UL;
    uint32_t daySeconds = seconds % 86400UL;
    uint32_t year = 2009UL;
    uint32_t month = 0U;
    uint32_t hour = daySeconds / 3600UL;
    uint32_t minute = (daySeconds % 3600UL) / 60UL;
    uint32_t second = daySeconds % 60UL;

    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    while (days >= (uint32_t)(IsLeapYear(year) ? 366U : 365U)) {
        days -= (uint32_t)(IsLeapYear(year) ? 366U : 365U);
        year++;
    }
    while (month < 12U) {
        uint32_t dim = monthDays[month];
        if ((month == 1U) && (IsLeapYear(year) != 0)) {
            dim++;
        }
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }
    (void)snprintf(out, outSize, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu",
                   (unsigned long)year, (unsigned long)(month + 1U),
                   (unsigned long)(days + 1U), (unsigned long)hour,
                   (unsigned long)minute, (unsigned long)second);
}

static int Spi2ReadDelayMs(uint32_t frameOffset)
{
    return (frameOffset == 0U) ? SPI2_IDLE_DELAY_MS : FIXED_SPI2_DELAY_MS;
}

static const char* Spi1ToggleButtonText(int running)
{
    return (running != 0) ? "Stop SPI1 TM" : "Start SPI1 TM";
}

static const char* Spi2ToggleButtonText(int running)
{
    return (running != 0) ? "Stop SPI2 DT" : "Start SPI2 DT";
}

static const char* TelemetryRxToggleButtonText(int enabled)
{
    return (enabled != 0) ? "Stop TM RX" : "Start TM RX";
}

static const char* LongRunButtonText(int running)
{
    return (running != 0) ? "Stop ER-07/FL-12" : "ER-07/FL-12 Loop";
}

static const char* ReportRunButtonText(int running)
{
    return (running != 0) ? "Stop Report Tests" : "Run Report Tests";
}

static void ReadReportRunConfigFromUi(ReportRunConfig* config)
{
    char baudText[32];
    uint32_t baud;

    if (config == NULL) {
        return;
    }
    (void)memset(config, 0, sizeof(*config));
    GetWindowTextA(g_app.uartPort, config->uartPort, sizeof(config->uartPort));
    GetWindowTextA(g_app.uartBaud, baudText, sizeof(baudText));
    baud = (uint32_t)strtoul(baudText, NULL, 10);
    config->uartBaud = (baud == 0U) ? DEFAULT_UART_BAUD : baud;
    config->spi1Index = SelectedComboIndex(g_app.spi1Device);
    config->spi2Index = SelectedComboIndex(g_app.spi2Device);
}

static void UpdateSpiToggleButtons(void)
{
#ifdef _WIN32
    if (g_app.spi1Toggle != NULL) {
        SetWindowTextA(g_app.spi1Toggle,
                       Spi1ToggleButtonText(
                           InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0));
    }
    if (g_app.spi2Toggle != NULL) {
        SetWindowTextA(g_app.spi2Toggle,
                       Spi2ToggleButtonText(
                           InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0));
    }
    if (g_app.tmRxToggle != NULL) {
        SetWindowTextA(g_app.tmRxToggle,
                       TelemetryRxToggleButtonText(
                           InterlockedCompareExchange(&g_app.spi1TelemetryEnabled, 1, 1) != 0));
    }
    if (g_app.longRun != NULL) {
        SetWindowTextA(g_app.longRun,
                       LongRunButtonText(
                           InterlockedCompareExchange(&g_app.longRunActive, 1, 1) != 0));
    }
    if (g_app.reportRun != NULL) {
        SetWindowTextA(g_app.reportRun,
                       ReportRunButtonText(
                           InterlockedCompareExchange(&g_app.reportRunActive, 1, 1) != 0));
    }
#endif
}

static void RequestSpiToggleRefresh(void)
{
#ifdef _WIN32
    if (g_app.hwnd != NULL) {
        (void)PostMessageA(g_app.hwnd, WM_APP_REFRESH_TOGGLES, 0, 0);
    }
#endif
}

static void ClearSpi2MonitorState(void)
{
    InterlockedExchange(&g_app.spi2Run, 0);
    InterlockedExchange(&g_app.spi2CaptureRequest, 0);
    InterlockedExchange(&g_app.spi2ContinuousCapture, 0);
    InterlockedExchange(&g_app.spi1SuspendForSpi2, 0);
}

static int Spi2CaptureShouldContinue(uint32_t framesThisCapture, uint32_t idleMs,
                                     int continuousCapture)
{
    if (framesThisCapture >= SPI2_CAPTURE_MAX_FRAMES) {
        return 0;
    }
    if (continuousCapture != 0) {
        return 1;
    }
    if (idleMs >= SPI2_CAPTURE_IDLE_TIMEOUT_MS) {
        return 0;
    }
    return 1;
}

static int Spi1TelemetryShouldSuspendForSpi2(int hasQueuedCommand)
{
    return ((hasQueuedCommand == 0) &&
            (InterlockedCompareExchange(&g_app.spi1SuspendForSpi2, 1, 1) != 0))
               ? 1
               : 0;
}

static int Spi2CaptureAcceptByte(uint8_t* frame, uint32_t* offset, uint8_t rxByte)
{
    if ((*offset == 0U) && (rxByte == 0xCFU)) {
        frame[0] = 0x1AU;
        frame[1] = 0xCFU;
        *offset = 2U;
        return 0;
    }
    if (*offset == 0U) {
        if (rxByte == 0x1AU) {
            frame[(*offset)++] = rxByte;
        }
        return 0;
    }
    frame[(*offset)++] = rxByte;
    if ((*offset == 2U) && (frame[1] != 0xCFU)) {
        *offset = (rxByte == 0x1AU) ? 1U : 0U;
        frame[0] = (*offset == 1U) ? rxByte : 0U;
        return 0;
    }
    if ((*offset == 3U) && (frame[2] != 0xFCU)) {
        *offset = (rxByte == 0x1AU) ? 1U : 0U;
        frame[0] = (*offset == 1U) ? rxByte : 0U;
        return 0;
    }
    if ((*offset == 4U) && (frame[3] != 0x1DU)) {
        *offset = (rxByte == 0x1AU) ? 1U : 0U;
        frame[0] = (*offset == 1U) ? rxByte : 0U;
        return 0;
    }
    return (*offset >= DT_FRAME_LEN) ? 1 : 0;
}

static void BuildCommand(uint8_t* tx, uint8_t command)
{
    (void)memset(tx, 0, TM_LEN);
    tx[0] = 0x76U;
    tx[1] = 0x25U;
    tx[2] = command;
}

static void FinishCommand(uint8_t* tx)
{
    tx[9] = Checksum8(tx, 2U, 8U);
}

static void BuildDataTransferCommand(uint8_t* tx, uint8_t enable)
{
    BuildCommand(tx, 0x09U);
    tx[3] = enable;
    FinishCommand(tx);
}

static int IsDataTransferCommandFrame(const uint8_t* tx, uint32_t txLength)
{
    uint8_t expected[TM_LEN];

    if ((tx == NULL) || (txLength != RC_LEN)) {
        return 0;
    }
    BuildDataTransferCommand(expected, 1U);
    return (memcmp(tx, expected, RC_LEN) == 0) ? 1 : 0;
}

static int GetDataTransferCommandSwitch(const uint8_t* tx, uint32_t txLength,
                                        uint8_t* enable)
{
    uint8_t expected[TM_LEN];

    if ((tx == NULL) || (txLength != RC_LEN) || (enable == NULL)) {
        return 0;
    }
    BuildDataTransferCommand(expected, 1U);
    if (memcmp(tx, expected, RC_LEN) == 0) {
        *enable = 1U;
        return 1;
    }
    BuildDataTransferCommand(expected, 0U);
    if (memcmp(tx, expected, RC_LEN) == 0) {
        *enable = 0U;
        return 1;
    }
    return 0;
}

static void ApplySpi2CaptureStateForCommand(const uint8_t* tx, uint32_t txLength,
                                            uint8_t requestSpi2Capture)
{
    uint8_t dataTransferSwitch = 0U;

    if (GetDataTransferCommandSwitch(tx, txLength, &dataTransferSwitch) != 0) {
        if (dataTransferSwitch != 0U) {
            InterlockedExchange(&g_app.spi2ContinuousCapture, 1);
            if (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) {
                InterlockedExchange(&g_app.spi1SuspendForSpi2, 1);
                InterlockedExchange(&g_app.spi2CaptureRequest, 1);
            }
        } else {
            InterlockedExchange(&g_app.spi2ContinuousCapture, 0);
            InterlockedExchange(&g_app.spi2CaptureRequest, 0);
            InterlockedExchange(&g_app.spi1SuspendForSpi2, 0);
        }
    } else if ((requestSpi2Capture != 0U) &&
               (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0)) {
        InterlockedExchange(&g_app.spi1SuspendForSpi2, 1);
        InterlockedExchange(&g_app.spi2CaptureRequest, 1);
    }
}

static int BufferAppend(ByteBuffer* buffer, const uint8_t* data, uint32_t length)
{
    uint8_t* resized;
    uint32_t capacity;

    if ((buffer == NULL) || ((data == NULL) && (length != 0U))) {
        return -1;
    }
    if (length == 0U) {
        return 0;
    }
    if ((buffer->length + length) > buffer->capacity) {
        capacity = (buffer->capacity == 0U) ? 1024U : buffer->capacity;
        while ((buffer->length + length) > capacity) {
            capacity *= 2U;
        }
        resized = (uint8_t*)realloc(buffer->data, capacity);
        if (resized == NULL) {
            return -1;
        }
        buffer->data = resized;
        buffer->capacity = capacity;
    }
    (void)memcpy(&buffer->data[buffer->length], data, length);
    buffer->length += length;
    return 0;
}

static int FrameIndexAppend(FrameIndexBuffer* buffer, uint32_t frameIndex, uint32_t count)
{
    uint32_t* resized;
    uint32_t capacity;
    uint32_t i;

    if (buffer == NULL) {
        return -1;
    }
    if (count == 0U) {
        return 0;
    }
    if ((buffer->length + count) > buffer->capacity) {
        capacity = (buffer->capacity == 0U) ? 1024U : buffer->capacity;
        while ((buffer->length + count) > capacity) {
            capacity *= 2U;
        }
        resized = (uint32_t*)realloc(buffer->data, (size_t)capacity * sizeof(buffer->data[0]));
        if (resized == NULL) {
            return -1;
        }
        buffer->data = resized;
        buffer->capacity = capacity;
    }
    for (i = 0U; i < count; i++) {
        buffer->data[buffer->length + i] = frameIndex;
    }
    buffer->length += count;
    return 0;
}

static void ResetRecordStream(RecordStream* stream)
{
    if (stream != NULL) {
        stream->pending.length = 0U;
        stream->pendingFrames.length = 0U;
        stream->nextIndex = 0U;
        stream->lastRecordStartFrame = 0U;
        stream->lastRecordEndFrame = 0U;
        stream->lastRecordBytes = 0U;
    }
}

static void FormatHex(char* out, size_t outSize, const uint8_t* data, uint32_t len)
{
    uint32_t i;
    size_t used = 0U;

    if (outSize == 0U) {
        return;
    }
    out[0] = '\0';
    for (i = 0U; i < len; i++) {
        int written = snprintf(&out[used], outSize - used, "%s%02X", (i == 0U) ? "" : " ", data[i]);
        if ((written < 0) || ((size_t)written >= (outSize - used))) {
            out[outSize - 1U] = '\0';
            return;
        }
        used += (size_t)written;
    }
}

static void FormatRawDtFrameLog(char* out, size_t outSize, uint32_t frameIndex,
                                uint16_t seq, const uint8_t* frame)
{
    int used;

    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    out[0] = '\0';
    if (frame == NULL) {
        (void)snprintf(out, outSize, "RAW frame=%lu seq=%u len=0 hex=",
                       (unsigned long)frameIndex, seq);
        return;
    }
    used = snprintf(out, outSize, "RAW frame=%lu seq=%u len=%u hex=",
                    (unsigned long)frameIndex, seq, DT_FRAME_LEN);
    if ((used < 0) || ((size_t)used >= outSize)) {
        out[outSize - 1U] = '\0';
        return;
    }
    FormatHex(&out[used], outSize - (size_t)used, frame, DT_FRAME_LEN);
}

static void BuildDailyLogPath(char* out, size_t outSize, const char* prefix,
                              int year, int month, int day)
{
    (void)snprintf(out, outSize, "logs\\%s_%04d%02d%02d.log",
                   prefix, year, month, day);
}

static int FormatTimestampedLog(char* out, size_t outSize, const char* timestamp,
                                const char* text)
{
    const char* p = text;
    size_t used = 0U;

    if ((out == NULL) || (outSize == 0U) || (timestamp == NULL) || (text == NULL)) {
        return -1;
    }
    out[0] = '\0';
    while (*p != '\0') {
        const char* lineEnd = strpbrk(p, "\r\n");
        size_t lineLen = (lineEnd == NULL) ? strlen(p) : (size_t)(lineEnd - p);
        int written;

        if (lineLen == 0U) {
            while ((*p == '\r') || (*p == '\n')) {
                p++;
            }
            continue;
        }
        written = snprintf(&out[used], outSize - used, "%s %.*s\r\n",
                           timestamp, (int)lineLen, p);
        if ((written < 0) || ((size_t)written >= (outSize - used))) {
            out[outSize - 1U] = '\0';
            return -1;
        }
        used += (size_t)written;
        p += lineLen;
        while ((*p == '\r') || (*p == '\n')) {
            p++;
        }
    }
    return 0;
}

static void GetCurrentTimestamp(char* timestamp, size_t timestampSize,
                                int* year, int* month, int* day)
{
    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    if (local == NULL) {
        (void)snprintf(timestamp, timestampSize, "0000-00-00 00:00:00");
        *year = 0;
        *month = 0;
        *day = 0;
        return;
    }
    *year = local->tm_year + 1900;
    *month = local->tm_mon + 1;
    *day = local->tm_mday;
    (void)snprintf(timestamp, timestampSize, "%04d-%02d-%02d %02d:%02d:%02d",
                   *year, *month, *day,
                   local->tm_hour, local->tm_min, local->tm_sec);
}

static void WriteDailyLogFile(const char* prefix, const char* text)
{
#ifdef _WIN32
    char timestamp[32];
    char path[160];
    int year;
    int month;
    int day;
    FILE* file;

    GetCurrentTimestamp(timestamp, sizeof(timestamp), &year, &month, &day);
    BuildDailyLogPath(path, sizeof(path), prefix, year, month, day);
    (void)CreateDirectoryA("logs", NULL);
    file = fopen(path, "ab");
    if (file == NULL) {
        return;
    }
    (void)fwrite(text, 1U, strlen(text), file);
    (void)fclose(file);
#else
    (void)prefix;
    (void)text;
#endif
}

static void ReportTableAdd(ReportTable* table, const char* id, const char* item,
                           const char* result, const char* detail)
{
    ReportRow* row;

    if ((table == NULL) || (table->count >= (sizeof(table->rows) / sizeof(table->rows[0])))) {
        return;
    }
    row = &table->rows[table->count++];
    (void)snprintf(row->id, sizeof(row->id), "%s", id);
    (void)snprintf(row->item, sizeof(row->item), "%s", item);
    (void)snprintf(row->result, sizeof(row->result), "%s", result);
    (void)snprintf(row->detail, sizeof(row->detail), "%s", detail);
    if (strcmp(result, "PASS") == 0) {
        table->pass++;
    } else if (strcmp(result, "FAILED") == 0) {
        table->failed++;
    } else if (strncmp(result, "SKIP", 4U) == 0) {
        table->skip++;
    }
    if ((g_app.hwnd != NULL) && (InterlockedCompareExchange(&g_app.reportRunActive, 1, 1) != 0)) {
        PostLog(ReportLogTargetForId(id), "REPORT %s %s: %s - %s", id, item, result, detail);
        ReportUpdateProgress(table);
    }
}

static uint32_t ReportTableCountId(const ReportTable* table, const char* id)
{
    uint32_t count = 0U;
    uint32_t i;

    if ((table == NULL) || (id == NULL)) {
        return 0U;
    }
    for (i = 0U; i < table->count; i++) {
        if (strcmp(table->rows[i].id, id) == 0) {
            count++;
        }
    }
    return count;
}

static const ReportRow* ReportTableFindRow(const ReportTable* table, const char* id)
{
    uint32_t i;

    if ((table == NULL) || (id == NULL)) {
        return NULL;
    }
    for (i = 0U; i < table->count; i++) {
        if (strcmp(table->rows[i].id, id) == 0) {
            return &table->rows[i];
        }
    }
    return NULL;
}

static void ReportTableAddIfMissing(ReportTable* table, const char* id,
                                    const char* item, const char* result,
                                    const char* detail)
{
    if (ReportTableCountId(table, id) == 0U) {
        ReportTableAdd(table, id, item, result, detail);
    }
}

static void ReportEscapeCell(char* out, size_t outSize, const char* text)
{
    size_t used = 0U;
    const char* p = (text == NULL) ? "" : text;

    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    out[0] = '\0';
    while ((*p != '\0') && ((used + 2U) < outSize)) {
        char ch = *p++;
        if (ch == '|') {
            out[used++] = '/';
        } else if ((ch == '\r') || (ch == '\n')) {
            if ((used == 0U) || (out[used - 1U] != ' ')) {
                out[used++] = ' ';
            }
        } else {
            out[used++] = ch;
        }
    }
    out[used] = '\0';
}

static int ReportFormatMarkdownRow(char* out, size_t outSize, const ReportRow* row)
{
    char id[32];
    char item[128];
    char result[32];
    char detail[320];

    if ((out == NULL) || (outSize == 0U) || (row == NULL)) {
        return -1;
    }
    ReportEscapeCell(id, sizeof(id), row->id);
    ReportEscapeCell(item, sizeof(item), row->item);
    ReportEscapeCell(result, sizeof(result), row->result);
    ReportEscapeCell(detail, sizeof(detail), row->detail);
    return (snprintf(out, outSize, "| %s | %s | %s | %s |\r\n",
                     id, item, result, detail) > 0) ? 0 : -1;
}

static int ReportWriteMarkdown(const char* path, const ReportTable* table,
                               const ReportRunConfig* config)
{
    FILE* file;
    uint32_t sectionIndex;
    uint32_t itemIndex;
    char row[560];
    ReportRow missing;
    const ReportRow* reportRow;

    if ((path == NULL) || (table == NULL) || (config == NULL)) {
        return -1;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    (void)fprintf(file, "# 载荷基带软件测试报告\r\n\r\n");
    (void)fprintf(file, "## 一、测试概况\r\n\r\n");
    (void)fprintf(file, "| 项目 | 内容 |\r\n| --- | --- |\r\n");
    (void)fprintf(file, "| 被测对象 | TMS570LS3137 载荷基带软件 |\r\n");
    (void)fprintf(file, "| 测试版本号 | %s |\r\n", REPORT_TEST_VERSION);
    (void)fprintf(file, "| 测试工具版本号 | %s |\r\n", REPORT_TOOL_VERSION);
    (void)fprintf(file, "| 测试开始时间 | %s |\r\n", config->startTime[0] != '\0' ? config->startTime : "未记录");
    (void)fprintf(file, "| 测试结束时间 | %s |\r\n", config->endTime[0] != '\0' ? config->endTime : "未记录");
    (void)fprintf(file, "| 测试环境 | PC运行combined_spi_gui_tester，通过JTOOL SPI1/SPI2和UART连接DUT |\r\n");
    (void)fprintf(file, "| 测试环境与真实环境差异 | 当前PC数传测试使用临时固件时TMS570 SPI2为slave、PC/JTOOL为master；真实环境为TMS570 SPI2 master、FPGA slave，需整机复测确认 |\r\n");
    (void)fprintf(file, "| 测试依据 | `载荷基带软件测试方案_v0.5.md`、`TMS570L与FPGA接口和协议定义_v0.5.md`、`AT指令.md` |\r\n");
    (void)fprintf(file, "| UART | %s @ %lu |\r\n",
                  config->uartPort, (unsigned long)config->uartBaud);
    (void)fprintf(file, "| SPI选择 | SPI1=%d, SPI2=%d |\r\n",
                  config->spi1Index, config->spi2Index);
    (void)fprintf(file, "| 汇总 | PASS=%lu FAILED=%lu SKIP=%lu TOTAL=%lu |\r\n\r\n",
                  (unsigned long)table->pass, (unsigned long)table->failed,
                  (unsigned long)table->skip, (unsigned long)table->count);

    (void)fprintf(file, "## 二、通用记录要求\r\n\r\n");
    (void)fprintf(file, "| 项目 | 要求 |\r\n| --- | --- |\r\n");
    (void)fprintf(file, "| 真实结果 | 记录工具日志、串口日志、遥测字段、数传数据和外部仪器结果等客观现象。 |\r\n");
    (void)fprintf(file, "| 判定 | 自动执行项仅在真实UART/SPI通信和必要遥测回读满足预期时填写PASS；未执行或需外部设备项填写SKIP/人工测试。 |\r\n");
    (void)fprintf(file, "| 失败记录 | FAILED项需结合raw日志记录失败时间、操作步骤、复现次数和初步原因。 |\r\n\r\n");

    (void)fprintf(file, "## 三、单元测试记录\r\n\r\n");
    for (sectionIndex = 0U; sectionIndex < (sizeof(REPORT_SECTIONS) / sizeof(REPORT_SECTIONS[0])); sectionIndex++) {
        (void)fprintf(file, "%s\r\n\r\n", REPORT_SECTIONS[sectionIndex].title);
        (void)fprintf(file, "| 编号 | 测试项 | 结果 | 详情 |\r\n| --- | --- | --- | --- |\r\n");
        for (itemIndex = 0U; itemIndex < REPORT_SECTIONS[sectionIndex].count; itemIndex++) {
            reportRow = ReportTableFindRow(table, REPORT_SECTIONS[sectionIndex].items[itemIndex].id);
            if (reportRow == NULL) {
                (void)memset(&missing, 0, sizeof(missing));
                (void)snprintf(missing.id, sizeof(missing.id), "%s", REPORT_SECTIONS[sectionIndex].items[itemIndex].id);
                (void)snprintf(missing.item, sizeof(missing.item), "%s", REPORT_SECTIONS[sectionIndex].items[itemIndex].item);
                (void)snprintf(missing.result, sizeof(missing.result), "SKIP");
                (void)snprintf(missing.detail, sizeof(missing.detail), "报告生成时未产生该项自动结果");
                reportRow = &missing;
            }
            if (ReportFormatMarkdownRow(row, sizeof(row), reportRow) == 0) {
                (void)fwrite(row, 1U, strlen(row), file);
            }
        }
        (void)fprintf(file, "\r\n");
    }
    (void)fclose(file);
    return 0;
}

static const char* ReportMissingAutomatedResourceResult(int selectedButOpenFailed)
{
    return (selectedButOpenFailed != 0) ? "FAILED" : "SKIP";
}

static int EnsureDirectory(const char* path)
{
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) == 0) {
        return (GetLastError() == ERROR_ALREADY_EXISTS) ? 0 : -1;
    }
    return 0;
#else
    (void)path;
    return -1;
#endif
}

static int BuildReportPath(char* out, size_t outSize, const char* prefix,
                           const char* ext)
{
    time_t now;
    struct tm* local;
#ifdef _WIN32
    char reportDir[MAX_PATH];
    char* slash;
#endif

    if ((out == NULL) || (outSize == 0U)) {
        return -1;
    }
    now = time(NULL);
    local = localtime(&now);
    if (local == NULL) {
        return -1;
    }
#ifdef _WIN32
    reportDir[0] = '\0';
    (void)GetModuleFileNameA(NULL, reportDir, sizeof(reportDir));
    slash = strrchr(reportDir, '\\');
    if (slash == NULL) {
        (void)snprintf(reportDir, sizeof(reportDir), "reports");
    } else {
        slash[1] = '\0';
        (void)strncat(reportDir, "reports", sizeof(reportDir) - strlen(reportDir) - 1U);
    }
    if (EnsureDirectory(reportDir) != 0) {
        return -1;
    }
    (void)snprintf(out, outSize, "%s\\%s_%04d%02d%02d_%02lu%02lu%02lu.%s",
                   reportDir,
#else
    if (EnsureDirectory("reports") != 0) {
        return -1;
    }
    (void)snprintf(out, outSize, "reports\\%s_%04d%02d%02d_%02lu%02lu%02lu.%s",
#endif
                   prefix, local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                   (unsigned long)local->tm_hour,
                   (unsigned long)local->tm_min,
                   (unsigned long)local->tm_sec, ext);
    return 0;
}

static HANDLE SerialOpen(const char* portName, uint32_t baud)
{
#ifdef _WIN32
    char path[64];
    HANDLE handle;
    DCB dcb;
    COMMTIMEOUTS timeouts;

    if ((portName == NULL) || (portName[0] == '\0')) {
        return INVALID_HANDLE_VALUE;
    }
    (void)snprintf(path, sizeof(path), "\\\\.\\%s", portName);
    handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }
    (void)memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if ((GetCommState(handle, &dcb) == 0) ||
        (SetupComm(handle, 4096, 4096) == 0)) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8U;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (SetCommState(handle, &dcb) == 0) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    timeouts.ReadIntervalTimeout = 20U;
    timeouts.ReadTotalTimeoutMultiplier = 1U;
    timeouts.ReadTotalTimeoutConstant = 200U;
    timeouts.WriteTotalTimeoutMultiplier = 1U;
    timeouts.WriteTotalTimeoutConstant = 200U;
    (void)SetCommTimeouts(handle, &timeouts);
    (void)PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return handle;
#else
    (void)portName;
    (void)baud;
    return NULL;
#endif
}

static int SerialSendLine(HANDLE handle, const char* line)
{
#ifdef _WIN32
    DWORD written = 0;
    char text[1200];

    if ((handle == INVALID_HANDLE_VALUE) || (line == NULL)) {
        return -1;
    }
    (void)snprintf(text, sizeof(text), "%s\r\n", line);
    return (WriteFile(handle, text, (DWORD)strlen(text), &written, NULL) != 0) ? 0 : -1;
#else
    (void)handle;
    (void)line;
    return -1;
#endif
}

static int SerialReadAvailable(HANDLE handle, char* out, uint32_t outSize,
                               uint32_t timeoutMs)
{
#ifdef _WIN32
    uint32_t start = TickMs();
    uint32_t used = 0U;

    if ((handle == INVALID_HANDLE_VALUE) || (out == NULL) || (outSize == 0U)) {
        return -1;
    }
    out[0] = '\0';
    while ((TickMs() - start) < timeoutMs) {
        DWORD got = 0;
        if (ReadFile(handle, &out[used], (DWORD)(outSize - used - 1U), &got, NULL) == 0) {
            return -1;
        }
        if (got != 0U) {
            used += got;
            out[used] = '\0';
            if (used >= (outSize - 1U)) {
                break;
            }
        } else {
            Sleep(20);
        }
    }
    return (int)used;
#else
    (void)handle;
    (void)out;
    (void)outSize;
    (void)timeoutMs;
    return -1;
#endif
}

static void SerialDrain(HANDLE handle)
{
#ifdef _WIN32
    if (handle != INVALID_HANDLE_VALUE) {
        (void)PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
#else
    (void)handle;
#endif
}

static void FormatAtCommandSummary(const char* atCommand, char* out, size_t outSize)
{
    size_t length;

    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    out[0] = '\0';
    if (atCommand == NULL) {
        return;
    }
    length = strlen(atCommand);
    if (length <= REPORT_DETAIL_AT_SUMMARY_CHARS) {
        (void)snprintf(out, outSize, "%s", atCommand);
    } else {
        (void)snprintf(out, outSize, "%.*s... len=%lu",
                       (int)REPORT_DETAIL_AT_SUMMARY_CHARS,
                       atCommand, (unsigned long)length);
    }
}

static int ReportSendAtAndWaitOk(HANDLE serial, const char* atCommand,
                                 char* detail, size_t detailSize)
{
    uint32_t start = TickMs();
    char uartText[2048];
    char summary[160];
    char seen[256];
    size_t seenUsed = 0U;
    uint32_t timeoutMs = REPORT_UART_WAIT_TIMEOUT_MS;

    FormatAtCommandSummary(atCommand, summary, sizeof(summary));
    SerialDrain(serial);
    if (SerialSendLine(serial, atCommand) != 0) {
        (void)snprintf(detail, detailSize, "AT发送失败：%s", summary);
        return -1;
    }
    seen[0] = '\0';
    while ((TickMs() - start) < timeoutMs) {
        int got = SerialReadAvailable(serial, uartText, sizeof(uartText), 120U);
        if (got < 0) {
            (void)snprintf(detail, detailSize, "AT读取失败：%s", summary);
            return -1;
        }
        if (got > 0) {
            size_t copy = (size_t)got;
            if (copy > (sizeof(seen) - seenUsed - 1U)) {
                copy = sizeof(seen) - seenUsed - 1U;
            }
            if (copy > 0U) {
                (void)memcpy(&seen[seenUsed], uartText, copy);
                seenUsed += copy;
                seen[seenUsed] = '\0';
            }
            if (strstr(uartText, "OK") != NULL) {
                (void)snprintf(detail, detailSize, "AT OK：%s", summary);
                return 0;
            }
            if (strstr(uartText, "ERROR") != NULL) {
                (void)snprintf(detail, detailSize, "AT返回ERROR：%s，实际：%.120s", summary, uartText);
                return -1;
            }
        }
    }
    (void)snprintf(detail, detailSize, "AT等待OK超时：%s，实际：%.120s", summary, seen);
    return -1;
}

static int ReportSendSpi1WithHandle(void* dev, uint8_t command, const char* paramsText,
                                     char* detail, size_t detailSize)
{
    uint8_t tx[TM_LEN];
    uint8_t rx[SPI1_PAGE_LEN];
    uint32_t txLength = 0U;
    uint8_t requestCapture = 0U;
    char error[160];

    if (BuildGuiCommandFromText(command, paramsText, tx, &txLength,
                                &requestCapture, error, sizeof(error)) != 0) {
        (void)snprintf(detail, detailSize, "build CMD_%02X failed: %s", command, error);
        return -1;
    }
    if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, txLength, tx, rx) != ErrNone) {
        (void)snprintf(detail, detailSize, "SPIWriteRead CMD_%02X failed", command);
        return -1;
    }
    (void)snprintf(detail, detailSize, "sent CMD_%02X params=%s", command, paramsText);
    return 0;
}

static int ReportSendManualSpi1WithHandle(void* dev, const char* hexText,
                                           char* detail, size_t detailSize)
{
    uint8_t tx[TM_LEN];
    uint8_t rx[SPI1_PAGE_LEN];
    uint32_t txLength = 0U;
    uint8_t requestCapture = 0U;
    char error[160];

    if (BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, hexText, tx, &txLength,
                                &requestCapture, error, sizeof(error)) != 0) {
        (void)snprintf(detail, detailSize, "build manual frame failed: %s", error);
        return -1;
    }
    if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, txLength, tx, rx) != ErrNone) {
        (void)snprintf(detail, detailSize, "SPIWriteRead manual frame failed");
        return -1;
    }
    (void)snprintf(detail, detailSize, "manual SPI1 frame sent len=%lu hex=%s",
                   (unsigned long)txLength, hexText);
    return 0;
}

static int ReportSendPaddedManualSpi1WithHandle(void* dev, const char* hexText,
                                                char* detail, size_t detailSize)
{
    uint8_t tx[TM_LEN];
    uint8_t rx[SPI1_PAGE_LEN];
    uint32_t rawLength = 0U;

    (void)memset(tx, 0, sizeof(tx));
    if ((ParseHexBytes(hexText, tx, SPI1_PAGE_LEN, &rawLength) != 0) || (rawLength == 0U)) {
        (void)snprintf(detail, detailSize, "build padded manual frame failed: %.120s", hexText);
        return -1;
    }
    if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, SPI1_PAGE_LEN, tx, rx) != ErrNone) {
        (void)snprintf(detail, detailSize, "SPIWriteRead padded manual frame failed");
        return -1;
    }
    (void)snprintf(detail, detailSize, "manual SPI1 frame sent len=%lu rawLen=%lu hex=%s",
                   (unsigned long)SPI1_PAGE_LEN, (unsigned long)rawLength, hexText);
    return 0;
}

static int ReportReadUartUntilContains(HANDLE serial, const char* expected,
                                       char* detail, size_t detailSize)
{
    char uartText[2048];
    char seen[256];
    size_t seenUsed = 0U;
    uint32_t start = TickMs();
    uint32_t timeoutMs = REPORT_UART_WAIT_TIMEOUT_MS;

    seen[0] = '\0';
    while ((TickMs() - start) < timeoutMs) {
        int got = SerialReadAvailable(serial, uartText, sizeof(uartText), 120U);
        if (got < 0) {
            (void)snprintf(detail, detailSize, "UART读取失败，期望包含：%s", expected);
            return -1;
        }
        if (got > 0) {
            size_t copy = (size_t)got;
            if (copy > (sizeof(seen) - seenUsed - 1U)) {
                copy = sizeof(seen) - seenUsed - 1U;
            }
            if (copy > 0U) {
                (void)memcpy(&seen[seenUsed], uartText, copy);
                seenUsed += copy;
                seen[seenUsed] = '\0';
            }
            if ((strstr(uartText, expected) != NULL) || (strstr(seen, expected) != NULL)) {
                (void)snprintf(detail, detailSize, "UART日志匹配：%s", expected);
                return 0;
            }
        }
    }
    if (seenUsed == 0U) {
        (void)snprintf(detail, detailSize, "UART未读到日志，期望包含：%s", expected);
    } else {
        (void)snprintf(detail, detailSize, "UART日志不匹配，期望包含：%s，实际：%.160s",
                       expected, seen);
    }
    return -1;
}

static int ReportReadTelemetryWithHandle(void* dev, uint8_t* tm, char* detail, size_t detailSize)
{
    uint8_t tx[SPI1_PAGE_LEN];
    uint8_t page0[SPI1_PAGE_LEN];
    uint8_t page1[SPI1_PAGE_LEN];
    uint32_t tries;

    (void)memset(tx, 0, sizeof(tx));
    for (tries = 0U; tries < 50U; tries++) {
        if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, SPI1_PAGE_LEN, tx, page0) != ErrNone) {
            (void)snprintf(detail, detailSize, "SPI1读取遥测page0失败");
            return -1;
        }
        Sleep(40);
        if ((page0[0] != 0xEBU) || (page0[1] != 0x90U)) {
            continue;
        }
        if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, SPI1_PAGE_LEN, tx, page1) != ErrNone) {
            (void)snprintf(detail, detailSize, "SPI1读取遥测page1失败");
            return -1;
        }
        (void)memcpy(tm, page0, SPI1_PAGE_LEN);
        (void)memcpy(&tm[SPI1_PAGE_LEN], page1, TM_LEN - SPI1_PAGE_LEN);
        if (ValidateTelemetry(tm) == 0) {
            return 0;
        }
    }
    (void)snprintf(detail, detailSize, "未在重试窗口内读到有效144字节遥测帧");
    return -1;
}

static int ReportSendSpi1AndVerifyTelemetry(void* dev, uint8_t command,
                                            const char* paramsText,
                                            uint32_t telemetryOffset,
                                            uint8_t expectedValue,
                                            char* detail, size_t detailSize)
{
    uint8_t tm[TM_LEN];
    uint8_t lastActual = 0U;
    uint32_t start;
    int sawTelemetry = 0;

    if (ReportSendSpi1WithHandle(dev, command, paramsText, detail, detailSize) != 0) {
        return -1;
    }
    start = TickMs();
    while ((TickMs() - start) < REPORT_TELEMETRY_VERIFY_TIMEOUT_MS) {
        Sleep(120);
        if (ReportReadTelemetryWithHandle(dev, tm, detail, detailSize) == 0) {
            sawTelemetry = 1;
            lastActual = tm[telemetryOffset];
            if (lastActual == expectedValue) {
                (void)snprintf(detail, detailSize,
                               "SPI1遥控CMD_%02X参数=%s已发送，SPI1遥测DATA%lu回读=0x%02X",
                               command, paramsText, (unsigned long)telemetryOffset, expectedValue);
                return 0;
            }
        }
    }
    if (sawTelemetry == 0) {
        return -1;
    }
    (void)snprintf(detail, detailSize,
                   "SPI1遥控CMD_%02X参数=%s已发送，但%lums内遥测DATA%lu最后为0x%02X，期望0x%02X",
                   command, paramsText, (unsigned long)REPORT_TELEMETRY_VERIFY_TIMEOUT_MS,
                   (unsigned long)telemetryOffset, lastActual, expectedValue);
    return -1;
}

static void ReportRunU8RangeWithTelemetry(ReportTable* table, void* dev,
                                          const char* id, const char* item,
                                          uint8_t command, uint8_t first,
                                          uint8_t last, uint32_t telemetryOffset)
{
    uint32_t passed = 0U;
    uint8_t value;
    char valueText[16];
    char stepDetail[256];
    char detail[256];

    for (value = first; value <= last; value++) {
        (void)snprintf(valueText, sizeof(valueText), "%u", value);
        if (ReportSendSpi1AndVerifyTelemetry(dev, command, valueText,
                                             telemetryOffset, value,
                                             stepDetail, sizeof(stepDetail)) != 0) {
            (void)snprintf(detail, sizeof(detail),
                           "遍历%u..%u，已通过%lu项，失败值=%u：%s",
                           first, last, (unsigned long)passed, value, stepDetail);
            ReportTableAdd(table, id, item, "FAILED", detail);
            return;
        }
        passed++;
    }
    (void)snprintf(detail, sizeof(detail),
                   "遍历%u..%u全部通过，共%lu项；每项均完成SPI1遥控下发和DATA%lu遥测回读匹配",
                   first, last, (unsigned long)passed, (unsigned long)telemetryOffset);
    ReportTableAdd(table, id, item, "PASS", detail);
}

static void ReportRunByteValueSetWithTelemetry(ReportTable* table, void* dev,
                                               const char* id, const char* item,
                                               uint8_t command,
                                               const uint8_t* values,
                                               uint32_t valueCount,
                                               uint32_t telemetryOffset)
{
    uint32_t i;
    char valueText[16];
    char stepDetail[256];
    char detail[256];

    for (i = 0U; i < valueCount; i++) {
        (void)snprintf(valueText, sizeof(valueText), "0x%02X", values[i]);
        if (ReportSendSpi1AndVerifyTelemetry(dev, command, valueText,
                                             telemetryOffset, values[i],
                                             stepDetail, sizeof(stepDetail)) != 0) {
            (void)snprintf(detail, sizeof(detail),
                           "遍历代表值集合失败，已通过%lu/%lu项，失败值=0x%02X：%s",
                           (unsigned long)i, (unsigned long)valueCount,
                           values[i], stepDetail);
            ReportTableAdd(table, id, item, "FAILED", detail);
            return;
        }
    }
    (void)snprintf(detail, sizeof(detail),
                   "遍历代表值0x00/0x01/0x02/0x55/0xAA/0xFF全部通过，共%lu项；每项均完成SPI1遥控下发和DATA%lu遥测回读匹配",
                   (unsigned long)valueCount, (unsigned long)telemetryOffset);
    ReportTableAdd(table, id, item, "PASS", detail);
}

static int ReportSendSpi1AndVerifyTelemetryU32(void* dev, uint8_t command,
                                               const char* paramsText,
                                               uint32_t telemetryOffset,
                                               uint32_t expectedValue,
                                               char* detail, size_t detailSize)
{
    uint8_t tm[TM_LEN];
    uint32_t actual = 0U;
    uint32_t start;
    int sawTelemetry = 0;

    if (ReportSendSpi1WithHandle(dev, command, paramsText, detail, detailSize) != 0) {
        return -1;
    }
    start = TickMs();
    while ((TickMs() - start) < REPORT_TELEMETRY_VERIFY_TIMEOUT_MS) {
        Sleep(120);
        if (ReportReadTelemetryWithHandle(dev, tm, detail, detailSize) == 0) {
            sawTelemetry = 1;
            actual = ReadBe32(&tm[telemetryOffset]);
            if (actual == expectedValue) {
                (void)snprintf(detail, detailSize,
                               "SPI1遥控CMD_%02X参数=%s已发送，SPI1遥测DATA%lu..DATA%lu回读=0x%08lX",
                               command, paramsText, (unsigned long)telemetryOffset,
                               (unsigned long)(telemetryOffset + 3U), (unsigned long)expectedValue);
                return 0;
            }
        }
    }
    if (sawTelemetry == 0) {
        return -1;
    }
    (void)snprintf(detail, detailSize,
                   "SPI1遥控CMD_%02X参数=%s已发送，但%lums内遥测DATA%lu..DATA%lu最后为0x%08lX，期望0x%08lX",
                   command, paramsText, (unsigned long)REPORT_TELEMETRY_VERIFY_TIMEOUT_MS,
                   (unsigned long)telemetryOffset, (unsigned long)(telemetryOffset + 3U),
                   (unsigned long)actual, (unsigned long)expectedValue);
    return -1;
}

static void ReportRunRcErrorRows(ReportTable* table, void* dev, HANDLE serial,
                                 int spi1OpenFailed, int uartOpenFailed)
{
    static const struct {
        const char* id;
        const char* item;
        const char* hexText;
        const char* expectedLog;
    } cases[] = {
        {"RC-ERR-01", "遥控帧头错误", "00 01 02 03 04 05 06 07 08 23", "RC ERR header frame=00 01 02 03 04 05 06 07 08 23"},
        {"RC-ERR-02", "遥控长度错误-短帧", "00 01 02 03 04 09", "RC ERR header frame=00 01 02 03 04 09 00 00 00 00"},
        {"RC-ERR-03", "遥控长度错误-长帧", "00 01 02 03 04 05 06 07 08 09 0A 0B 41", "RC ERR header frame=00 01 02 03 04 05 06 07 08 09"},
        {"RC-ERR-04", "遥控校验错误", "76 25 01 02 00 00 00 00 00 09", "RC ERR checksum cmd=0x01"},
        {"RC-ERR-05", "遥控参数异常", "76 25 02 03 00 00 00 00 00 05", "RC ERR range cmd=0x02 field=rate value=3 allowed=0..2"}
    };
    uint32_t i;
    char detail[256];

    if ((dev == NULL) || (serial == INVALID_HANDLE_VALUE)) {
        const char* result = ReportMissingAutomatedResourceResult(spi1OpenFailed || uartOpenFailed);
        const char* reason = (spi1OpenFailed != 0) ? "打开SPI1设备失败" :
                             ((uartOpenFailed != 0) ? "打开UART失败" :
                              "需要SPI1和UART同时可用以发送原始错误帧并读取RC ERR日志");
        for (i = 0U; i < (uint32_t)(sizeof(cases) / sizeof(cases[0])); i++) {
            ReportTableAdd(table, cases[i].id, cases[i].item, result, reason);
        }
        return;
    }
    for (i = 0U; i < (uint32_t)(sizeof(cases) / sizeof(cases[0])); i++) {
        SerialDrain(serial);
        if ((ReportSendPaddedManualSpi1WithHandle(dev, cases[i].hexText, detail, sizeof(detail)) == 0) &&
            (ReportReadUartUntilContains(serial, cases[i].expectedLog, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, cases[i].id, cases[i].item, "PASS", detail);
        } else {
            ReportTableAdd(table, cases[i].id, cases[i].item, "FAILED", detail);
        }
    }
}

static int ReportCaptureExpectedDtRecord(void* spi1Dev, int spi2Index, uint32_t minTimestamp,
                                         uint32_t maxTimestamp,
                                         uint8_t expectedFormat, uint8_t expectedType,
                                         const uint8_t* expectedPayload,
                                         uint16_t expectedLength,
                                         char* detail, size_t detailSize)
{
    uint8_t txByte = 0U;
    uint8_t rxByte = 0U;
    uint8_t frame[DT_FRAME_LEN];
    ByteBuffer payload = {0};
    RecordStream records = {0};
    uint32_t offset = 0U;
    uint32_t frames = 0U;
    uint32_t start = TickMs();
    char error[160];
    void* dev = OpenSpiDevice(spi2Index, WM_APP_LOG_SPI2);

    if (dev == NULL) {
        (void)snprintf(detail, detailSize, "打开SPI2设备失败");
        return -1;
    }
    if (ReportSendSpi1WithHandle(spi1Dev, 0x09U, "1", detail, detailSize) != 0) {
        (void)g_DevClose(dev);
        return -1;
    }
    (void)memset(frame, 0, sizeof(frame));
    while ((TickMs() - start) < SPI2_CAPTURE_TIMEOUT_MS) {
        if (SpiWriteReadLocked(dev, FIXED_SPI2_MODE, ENDIAN_MSB, 1U, &txByte, &rxByte) != ErrNone) {
            (void)g_DevClose(dev);
            free(payload.data);
            free(records.pending.data);
            (void)snprintf(detail, detailSize, "SPI2读取失败");
            return -1;
        }
        Sleep((DWORD)Spi2ReadDelayMs(offset));
        if (Spi2CaptureAcceptByte(frame, &offset, rxByte) == 0) {
            continue;
        }
        offset = 0U;
        if (AppendFramePayload(&payload, frame, error, sizeof(error)) == 0) {
            int matched;
            frames++;
            matched = RecordStreamAppendAndMatch(&records, payload.data, payload.length,
                                                 minTimestamp, maxTimestamp, expectedFormat,
                                                 expectedType, expectedPayload,
                                                 expectedLength);
            payload.length = 0U;
            if (matched == 1) {
                (void)g_DevClose(dev);
                free(payload.data);
                free(records.pending.data);
                (void)snprintf(detail, detailSize,
                               "SPI2收到匹配record frameCount=%lu rawTime=0x%08lX format=0x%02X type=0x%02X length=%u",
                               (unsigned long)frames, (unsigned long)minTimestamp,
                               expectedFormat, expectedType, expectedLength);
                return 0;
            }
            if (matched < 0) {
                (void)g_DevClose(dev);
                free(payload.data);
                free(records.pending.data);
                (void)snprintf(detail, detailSize, "SPI2 record缓存分配失败");
                return -1;
            }
        }
    }
    (void)g_DevClose(dev);
    free(payload.data);
    free(records.pending.data);
    (void)snprintf(detail, detailSize, "SPI2超时未收到匹配record frames=%lu", (unsigned long)frames);
    return -1;
}

static int ReportRunDtScenario(const ReportRunConfig* config, void* spi1Dev, HANDLE serial,
                               int spi1OpenFailed, int uartOpenFailed,
                               const char* atCommand, uint32_t minTimestamp,
                               uint32_t maxTimestamp,
                               uint8_t expectedFormat, uint8_t expectedType,
                               const uint8_t* expectedPayload, uint16_t expectedLength,
                               char* detail, size_t detailSize)
{
    if ((serial == INVALID_HANDLE_VALUE) || (spi1Dev == NULL) || (config->spi2Index < 0)) {
        if (spi1OpenFailed != 0) {
            (void)snprintf(detail, detailSize, "打开SPI1设备失败");
            return -1;
        }
        if (uartOpenFailed != 0) {
            (void)snprintf(detail, detailSize, "打开UART失败");
            return -1;
        }
        (void)snprintf(detail, detailSize, "需要UART、SPI1、SPI2同时可用");
        return 1;
    }
    if (LoadJtoolApi() != 0) {
        (void)snprintf(detail, detailSize, "jtool.dll SPI接口不可用");
        return 1;
    }
    if (ReportSendAtAndWaitOk(serial, "AT+DTCLEAR", detail, detailSize) != 0) {
        return -1;
    }
    if (ReportSendAtAndWaitOk(serial, atCommand, detail, detailSize) != 0) {
        return -1;
    }
    return ReportCaptureExpectedDtRecord(spi1Dev, config->spi2Index,
                                         minTimestamp, maxTimestamp,
                                         expectedFormat, expectedType,
                                         expectedPayload, expectedLength,
                                         detail, detailSize);
}

static void FillPatternPayload(uint8_t* payload, uint16_t length)
{
    uint16_t i;

    for (i = 0U; i < length; i++) {
        payload[i] = (uint8_t)('A' + (i % 26U));
    }
}

static void FormatHexPayloadText(char* out, size_t outSize, const uint8_t* payload, uint16_t length)
{
    uint16_t i;
    size_t used = 0U;

    if (outSize == 0U) {
        return;
    }
    out[0] = '\0';
    for (i = 0U; i < length; i++) {
        int written = snprintf(&out[used], outSize - used, "%02X", payload[i]);
        if ((written < 0) || ((size_t)written >= (outSize - used))) {
            out[outSize - 1U] = '\0';
            return;
        }
        used += (size_t)written;
    }
}

static void ReportRunDtRows(ReportTable* table, const ReportRunConfig* config,
                            void* spi1Dev, HANDLE serial,
                            int spi1OpenFailed, int uartOpenFailed)
{
    static const uint8_t dt01Payload[] = {'h', 'e', 'l', 'l', 'o'};
    static const uint8_t dt02Payload[] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    uint8_t payload494[494];
    char atCommand[1100];
    char hexPayload[1000];
    char detail[256];
    int result;

    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 "AT+DTWRITE=S,3,hello", 0U, 0U,
                                 DT_FORMAT_STRING, 3U, dt01Payload,
                                 (uint16_t)sizeof(dt01Payload), detail, sizeof(detail));
    ReportTableAdd(table, "DT-01", "字符格式不满512字节包",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);

    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 "AT+DTWRITE=H,2,010203040506070809", 0U, 0U,
                                 DT_FORMAT_BINARY, 2U, dt02Payload,
                                 (uint16_t)sizeof(dt02Payload), detail, sizeof(detail));
    ReportTableAdd(table, "DT-02", "hex格式不满512字节包",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);

    FillPatternPayload(payload494, (uint16_t)sizeof(payload494));
    (void)snprintf(atCommand, sizeof(atCommand), "AT+DTWRITE=S,3,%.*s",
                   (int)sizeof(payload494), (const char*)payload494);
    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 atCommand, 0U, 0U,
                                 DT_FORMAT_STRING, 3U, payload494,
                                 (uint16_t)sizeof(payload494), detail, sizeof(detail));
    ReportTableAdd(table, "DT-03", "字符格式512字节包",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);

    FormatHexPayloadText(hexPayload, sizeof(hexPayload), payload494, (uint16_t)sizeof(payload494));
    (void)snprintf(atCommand, sizeof(atCommand), "AT+DTWRITE=H,2,%s", hexPayload);
    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 atCommand, 0U, 0U,
                                 DT_FORMAT_BINARY, 2U, payload494,
                                 (uint16_t)sizeof(payload494), detail, sizeof(detail));
    ReportTableAdd(table, "DT-04", "hex格式512字节包",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);
}

static void ReportRunTc12Tc13Rows(ReportTable* table, const ReportRunConfig* config,
                                   void* spi1Dev, HANDLE serial,
                                   int spi1OpenFailed, int uartOpenFailed)
{
    static const uint8_t tc13Payload[] = {1U, 2U, 3U, 4U, 5U};
    const char* tc12Expected = "RC OK CMD_0C rollback flagWrite=2 flagRead=2 noReset";
    char detail[256];
    int result;

    if ((spi1Dev == NULL) || (serial == INVALID_HANDLE_VALUE)) {
        const char* resultText = ReportMissingAutomatedResourceResult(spi1OpenFailed || uartOpenFailed);
        const char* reason = (spi1OpenFailed != 0) ? "打开SPI1设备失败" :
                             ((uartOpenFailed != 0) ? "打开UART失败" :
                              "需要SPI1和UART读取CMD_0C回滚日志");
        ReportTableAdd(table, "TC-12", "版本回滚", resultText, reason);
    } else {
        SerialDrain(serial);
        if ((ReportSendSpi1WithHandle(spi1Dev, 0x0CU, "00 00 00 00 00 00", detail, sizeof(detail)) == 0) &&
               (ReportReadUartUntilContains(serial, tc12Expected, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "TC-12", "版本回滚", "PASS", detail);
        } else {
            ReportTableAdd(table, "TC-12", "版本回滚", "FAILED", detail);
        }
    }

    if ((spi1Dev == NULL) || (serial == INVALID_HANDLE_VALUE) || (config->spi2Index < 0)) {
        const char* resultText = ReportMissingAutomatedResourceResult(spi1OpenFailed || uartOpenFailed);
        const char* reason = (spi1OpenFailed != 0) ? "打开SPI1设备失败" :
                             ((uartOpenFailed != 0) ? "打开UART失败" :
                              "需要SPI1、SPI2和UART完成UTC+DTWRITE+数传时间戳验证");
        ReportTableAdd(table, "TC-13", "UTC时间", resultText, reason);
        return;
    }
    SerialDrain(serial);
    if ((ReportSendSpi1WithHandle(spi1Dev, 0x0DU, "556369297", detail, sizeof(detail)) != 0) ||
        (ReportReadUartUntilContains(serial, "RC OK CMD_0D utcSeconds=556369297", detail, sizeof(detail)) != 0)) {
        ReportTableAdd(table, "TC-13", "UTC时间", "FAILED", detail);
        return;
    }
    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 "AT+DTWRITE=H,2,0102030405",
                                 556369297UL, 556369420UL, DT_FORMAT_BINARY, 2U,
                                 tc13Payload, (uint16_t)sizeof(tc13Payload),
                                 detail, sizeof(detail));
    ReportTableAdd(table, "TC-13", "UTC时间",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);
}

static void ReportRunErAndFlowRows(ReportTable* table, const ReportRunConfig* config,
                                   void* spi1Dev, HANDLE serial,
                                   int spi1OpenFailed, int uartOpenFailed)
{
    static const uint8_t fl01Payload[] = {'h', 'e', 'l', 'l', 'o'};
    static const uint8_t fl07Payload[] = {1U, 2U, 3U, 4U, 5U};
    char detail[256];
    uint8_t tm[TM_LEN];
    uint32_t i;
    int result;

    if ((spi1Dev == NULL) || (serial == INVALID_HANDLE_VALUE) || (config->spi2Index < 0)) {
        if (spi1OpenFailed != 0) {
            (void)snprintf(detail, sizeof(detail), "未测试OTA和复位；打开SPI1设备失败，未完成遥控、遥测、数传组合验证");
        } else if (uartOpenFailed != 0) {
            (void)snprintf(detail, sizeof(detail), "未测试OTA和复位；打开UART失败，未完成遥控、遥测、数传组合验证");
        } else {
            (void)snprintf(detail, sizeof(detail), "未测试OTA和复位；需要SPI1、SPI2、UART完成遥控、遥测、数传组合验证");
        }
        ReportTableAdd(table, "FL-01", "正常业务", "FAILED", detail);
    } else if ((ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "1", 21U, 1U, detail, sizeof(detail)) == 0) &&
               (ReportReadTelemetryWithHandle(spi1Dev, tm, detail, sizeof(detail)) == 0)) {
        result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                     "AT+DTWRITE=S,3,hello", 0U, 0U,
                                     DT_FORMAT_STRING, 3U, fl01Payload,
                                     (uint16_t)sizeof(fl01Payload), detail, sizeof(detail));
        if (result == 0) {
            ReportTableAdd(table, "FL-01", "正常业务", "PASS",
                           "遥控速率=1并遥测确认，SPI1遥测读取成功，SPI2数传收到hello记录；未测试OTA和复位");
        } else {
            (void)snprintf(detail, sizeof(detail), "未测试OTA和复位；数传验证失败");
            ReportTableAdd(table, "FL-01", "正常业务", "FAILED", detail);
        }
    } else {
        (void)snprintf(detail, sizeof(detail), "未测试OTA和复位；遥控或遥测验证失败");
        ReportTableAdd(table, "FL-01", "正常业务", "FAILED", detail);
    }

    if (spi1Dev == NULL) {
        const char* resultText = ReportMissingAutomatedResourceResult(spi1OpenFailed);
        const char* reason = (spi1OpenFailed != 0) ? "打开SPI1设备失败" : "需要SPI1可用";
        ReportTableAdd(table, "ER-04", "SPI1时钟异常", resultText, reason);
        ReportTableAdd(table, "ER-06", "快速重复命令", resultText, reason);
        ReportTableAdd(table, "FL-02", "配置覆盖+遥测确认", resultText, reason);
        ReportTableAdd(table, "FL-03", "错误帧夹杂+状态保持", resultText, reason);
        ReportTableAdd(table, "FL-04", "遥控遥测并发+配置更新", resultText, reason);
        ReportTableAdd(table, "FL-08", "寄存器写入+读取+遥测返回", resultText, reason);
        ReportTableAdd(table, "FL-11", "高频遥控压力+遥测", resultText, reason);
    } else {
        if (ReportReadTelemetryWithHandle(spi1Dev, tm, detail, sizeof(detail)) == 0) {
            Sleep(2000);
            if (ReportReadTelemetryWithHandle(spi1Dev, tm, detail, sizeof(detail)) == 0) {
                ReportTableAdd(table, "ER-04", "SPI1时钟异常", "PASS", "SPI1遥测读取成功，等待2秒后再次读取遥测成功");
            } else {
                ReportTableAdd(table, "ER-04", "SPI1时钟异常", "FAILED", detail);
            }
        } else {
            ReportTableAdd(table, "ER-04", "SPI1时钟异常", "FAILED", detail);
        }
        for (i = 0U; i < 100U; i++) {
            if (ReportSendSpi1WithHandle(spi1Dev, 0x02U, "1", detail, sizeof(detail)) != 0) {
                ReportTableAdd(table, "ER-06", "快速重复命令", "FAILED", detail);
                ReportTableAdd(table, "FL-11", "高频遥控压力+遥测", "FAILED", detail);
                break;
            }
        }
        if (i == 100U) {
            if (ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "1", 21U, 1U, detail, sizeof(detail)) == 0) {
                ReportTableAdd(table, "ER-06", "快速重复命令", "PASS", "速率=1遥控连续下发100次后，遥测DATA21仍回读为1");
                ReportTableAdd(table, "FL-11", "高频遥控压力+遥测", "PASS", "复用ER-06压力场景：100次速率=1遥控后遥测稳定");
            } else {
                ReportTableAdd(table, "ER-06", "快速重复命令", "FAILED", detail);
                ReportTableAdd(table, "FL-11", "高频遥控压力+遥测", "FAILED", detail);
            }
        }
        if ((ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x01U, "5", 20U, 5U, detail, sizeof(detail)) == 0) &&
            (ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "2", 21U, 2U, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "FL-02", "配置覆盖+遥测确认", "PASS", "工作模式配置为5后遥测确认；速率配置为2后遥测确认，后配置生效");
        } else {
            ReportTableAdd(table, "FL-02", "配置覆盖+遥测确认", "FAILED", detail);
        }
        if ((ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "0", 21U, 0U, detail, sizeof(detail)) == 0) &&
            (ReportSendManualSpi1WithHandle(spi1Dev, "01 02 03 04", detail, sizeof(detail)) == 0) &&
            (ReportReadTelemetryWithHandle(spi1Dev, tm, detail, sizeof(detail)) == 0) &&
            (tm[21] == 0U) &&
            (ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "1", 21U, 1U, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "FL-03", "错误帧夹杂+状态保持", "PASS", "速率0确认后插入错误帧，遥测速率保持0；后续速率1正确帧可执行");
        } else {
            ReportTableAdd(table, "FL-03", "错误帧夹杂+状态保持", "FAILED", detail);
        }
        if ((ReportReadTelemetryWithHandle(spi1Dev, tm, detail, sizeof(detail)) == 0) &&
            (ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "2", 21U, 2U, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "FL-04", "遥控遥测并发+配置更新", "PASS", "遥测读取窗口内下发速率2配置，后续遥测DATA21同步为2");
        } else {
            ReportTableAdd(table, "FL-04", "遥控遥测并发+配置更新", "FAILED", detail);
        }
        if ((ReportSendSpi1WithHandle(spi1Dev, 0x0AU, "0xC020 0xAABBCCDD", detail, sizeof(detail)) == 0) &&
            (ReportSendSpi1AndVerifyTelemetryU32(spi1Dev, 0x0BU, "0xC020", 67U, 0xAABBCCDDUL, detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "FL-08", "寄存器写入+读取+遥测返回", "PASS", "寄存器0xC020写入0xAABBCCDD后读取，遥测DATA67..70回读一致");
        } else {
            ReportTableAdd(table, "FL-08", "寄存器写入+读取+遥测返回", "FAILED", detail);
        }
    }

    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 "AT+DTWRITE=H,2,0102030405", 0U, 0U,
                                 DT_FORMAT_BINARY, 2U, fl07Payload,
                                 (uint16_t)sizeof(fl07Payload), detail, sizeof(detail));
    ReportTableAdd(table, "ER-05", "SPI2数传异常",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);
    ReportTableAdd(table, "FL-05", "数传启动+遥测并行", "SKIP",
                   "当前PC临时测试模式中TMS570 SPI2为slave，不证明SPI2数传与SPI1遥测真实并行");
    result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                 "AT+DTWRITE=H,2,0102030405", 0U, 0U,
                                 DT_FORMAT_BINARY, 2U, fl07Payload,
                                 (uint16_t)sizeof(fl07Payload), detail, sizeof(detail));
    if ((result == 0) && (spi1Dev != NULL) &&
        (ReportSendSpi1WithHandle(spi1Dev, 0x09U, "0", detail, sizeof(detail)) == 0)) {
        result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                     "AT+DTWRITE=H,2,0102030405", 0U, 0U,
                                     DT_FORMAT_BINARY, 2U, fl07Payload,
                                     (uint16_t)sizeof(fl07Payload), detail, sizeof(detail));
    }
    ReportTableAdd(table, "FL-06", "数传启动+中止+重启",
                   (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);
    if ((spi1Dev != NULL) && (serial != INVALID_HANDLE_VALUE) &&
        (ReportSendSpi1WithHandle(spi1Dev, 0x0DU, "556369297", detail, sizeof(detail)) == 0) &&
        (ReportReadUartUntilContains(serial, "RC OK CMD_0D utcSeconds=556369297", detail, sizeof(detail)) == 0)) {
        result = ReportRunDtScenario(config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed,
                                     "AT+DTWRITE=H,2,0102030405",
                                     556369297UL, 556369420UL, DT_FORMAT_BINARY, 2U,
                                     fl07Payload, (uint16_t)sizeof(fl07Payload),
                                     detail, sizeof(detail));
        ReportTableAdd(table, "FL-07", "UTC连续下发+缓存+数传",
                       (result == 0) ? "PASS" : ((result > 0) ? "SKIP" : "FAILED"), detail);
    } else {
        if (spi1OpenFailed != 0) {
            (void)snprintf(detail, sizeof(detail), "打开SPI1设备失败");
        } else if (uartOpenFailed != 0) {
            (void)snprintf(detail, sizeof(detail), "打开UART失败");
        } else if (spi1Dev == NULL) {
            (void)snprintf(detail, sizeof(detail), "未选择SPI1，无法自动执行UTC缓存数传流程");
        } else if (serial == INVALID_HANDLE_VALUE) {
            (void)snprintf(detail, sizeof(detail), "未配置UART，无法自动执行UTC缓存数传流程");
        } else if (config->spi2Index < 0) {
            (void)snprintf(detail, sizeof(detail), "未选择SPI2，无法自动执行UTC缓存数传流程");
        }
        ReportTableAdd(table, "FL-07", "UTC连续下发+缓存+数传", "FAILED", detail);
    }
    if (InterlockedCompareExchange(&g_app.longRunIterations, 0, 0) > 0) {
        LONG iterations = InterlockedCompareExchange(&g_app.longRunIterations, 0, 0);
        LONG failures = InterlockedCompareExchange(&g_app.longRunFailures, 0, 0);
        (void)snprintf(detail, sizeof(detail), "ER-07/FL-12 Loop按钮累计运行%ld轮，失败%ld轮；每轮执行遥控、遥测、UTC、数传",
                       iterations, failures);
        ReportTableAdd(table, "ER-07", "长时间运行", (failures == 0) ? "PASS" : "FAILED", detail);
        ReportTableAdd(table, "FL-12", "长时间运行闭环", (failures == 0) ? "PASS" : "FAILED", detail);
    } else {
        ReportTableAdd(table, "ER-07", "长时间运行", "SKIP", "未运行ER-07/FL-12 Loop按钮；该按钮循环执行遥控、遥测、UTC、数传");
        ReportTableAdd(table, "FL-12", "长时间运行闭环", "SKIP", "未运行ER-07/FL-12 Loop按钮；该按钮循环执行遥控、遥测、UTC、数传");
    }
}

static void ReportAddRcErrorRows(ReportTable* table)
{
    ReportTableAddIfMissing(table, "RC-ERR-01", "遥控帧头错误", "SKIP/人工测试",
                            "需通过Manual原始帧下发错误帧并确认DUT不执行命令、状态不变化");
    ReportTableAddIfMissing(table, "RC-ERR-02", "遥控长度错误-短帧", "SKIP/人工测试",
                            "需通过Manual原始帧下发短帧并确认DUT不执行命令、状态不变化");
    ReportTableAddIfMissing(table, "RC-ERR-03", "遥控长度错误-长帧", "SKIP/人工测试",
                            "需通过Manual原始帧下发长帧并确认DUT不执行命令、状态不变化");
    ReportTableAddIfMissing(table, "RC-ERR-04", "遥控校验错误", "SKIP/人工测试",
                            "需通过Manual原始帧破坏Byte9校验并确认DUT不执行命令、状态不变化");
    ReportTableAddIfMissing(table, "RC-ERR-05", "遥控参数异常", "SKIP/人工测试",
                            "需下发越界参数并确认DUT拒绝执行且遥测状态保持原值");
}

static void ReportAddMissingV01Rows(ReportTable* table)
{
    ReportAddRcErrorRows(table);

    ReportTableAddIfMissing(table, "TC-07", "单机复位", "SKIP/人工测试",
                            "复位会中断自动报告流程，需人工记录DUT复位和恢复后的遥测");
    ReportTableAddIfMissing(table, "TC-08", "固件升级", "SKIP/人工测试",
                            "升级命令会触发复位进入bootloader，需结合bootloader/CAN链路人工确认");
    ReportTableAddIfMissing(table, "TC-12", "版本回滚", "SKIP/人工测试",
                            "回滚需外部电源控制者或FPGA流程重启TMS570后确认");
    ReportTableAddIfMissing(table, "TC-13", "UTC时间", "SKIP/人工测试",
                            "需下发UTC后写入数传数据并通过SPI2记录时间戳确认");
    ReportTableAddIfMissing(table, "TC-14", "直流参数配置", "SKIP/人工测试",
                            "协议参数范围为天线1..8；当前144字节SPI1遥测未定义DC回读字段，需串口/TK8710寄存器或新增遥测字段确认遍历结果");

    ReportTableAddIfMissing(table, "DT-02", "hex格式不满512字节包", "SKIP/人工测试",
                            "需通过AT+DTWRITE注入hex短包，并用SPI2捕获解析record内容");
    ReportTableAddIfMissing(table, "DT-03", "字符格式512字节包", "SKIP/人工测试",
                            "需注入494字节字符串payload，并用SPI2确认512字节帧、填充和record内容");
    ReportTableAddIfMissing(table, "DT-04", "hex格式512字节包", "SKIP/人工测试",
                            "需注入494字节hex payload，并用SPI2确认512字节帧、填充和record内容");

    ReportTableAddIfMissing(table, "ER-01", "软件复位恢复", "SKIP/人工测试",
                            "需触发复位并人工确认启动日志、复位字段和遥测恢复");
    ReportTableAddIfMissing(table, "ER-02", "看门狗复位恢复", "SKIP/人工测试",
                            "需硬件看门狗或故障注入环境确认");
    ReportTableAddIfMissing(table, "ER-03", "FPGA复位模拟", "SKIP/人工测试",
                            "需外部RESET控制并记录复位时序和恢复遥测");
    ReportTableAddIfMissing(table, "ER-04", "SPI1时钟异常", "SKIP/人工测试",
                            "需制造SPI1时钟不足、中断或片选异常并观察后续恢复");
    ReportTableAddIfMissing(table, "ER-05", "SPI2数传异常", "SKIP/人工测试",
                            "需可控暂停SPI2接收或注入链路异常，确认pending保留和恢复");
    ReportTableAddIfMissing(table, "ER-06", "快速重复命令", "SKIP/人工测试",
                            "需连续下发100条以上遥控并确认最终配置和遥测稳定");
    ReportTableAddIfMissing(table, "ER-07", "长时间运行", "SKIP",
                            "未运行ER-07/FL-12 Loop按钮；该按钮循环执行遥控、遥测、UTC、数传");

    ReportTableAddIfMissing(table, "RF-01", "参数配置", "SKIP/人工测试",
                            "需结合TK8710状态、射频链路或外部观测确认");
    ReportTableAddIfMissing(table, "RF-02", "底噪检测", "SKIP/人工测试",
                            "需射频环境和数传结果确认");
    ReportTableAddIfMissing(table, "RF-03", "单tone模式", "SKIP/人工测试",
                            "需频谱仪确认tone频率和功率");
    ReportTableAddIfMissing(table, "RF-04", "ACM校准", "SKIP/人工测试",
                            "需射频链路或TK8710日志确认");
    ReportTableAddIfMissing(table, "RF-05", "信号采集", "SKIP/人工测试",
                            "需外部信号源和数传结果确认");
    ReportTableAddIfMissing(table, "RF-06", "模式A/B/C", "SKIP/人工测试",
                            "需射频链路和终端环境确认");
    ReportTableAddIfMissing(table, "RF-07", "单用户/速率0双向收发", "SKIP/人工测试",
                            "需终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-08", "单用户/速率1双向收发", "SKIP/人工测试",
                            "需终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-09", "单用户/速率2双向收发", "SKIP/人工测试",
                            "需终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-10", "16用户/速率0双向收发", "SKIP/人工测试",
                            "需16个终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-11", "16用户/速率1双向收发", "SKIP/人工测试",
                            "需16个终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-12", "16用户/速率2双向收发", "SKIP/人工测试",
                            "需16个终端和NS环境确认");
    ReportTableAddIfMissing(table, "RF-13", "稳定性测试", "SKIP/人工测试",
                            "需24小时整机运行确认");

    ReportTableAddIfMissing(table, "FL-01", "正常业务", "SKIP/人工测试",
                            "需完成上电、遥控、遥测、数传、OTA命令和复位闭环");
    ReportTableAddIfMissing(table, "FL-02", "配置覆盖+遥测确认", "SKIP/人工测试",
                            "需连续配置工作模式和速率，并确认后一次配置覆盖前一次配置");
    ReportTableAddIfMissing(table, "FL-03", "错误帧夹杂+状态保持", "SKIP/人工测试",
                            "需正确帧之间插入错误帧并确认状态保持和后续恢复");
    ReportTableAddIfMissing(table, "FL-04", "遥控遥测并发+配置更新", "SKIP/人工测试",
                            "需遥测期间下发配置并确认后续遥测同步更新");
    ReportTableAddIfMissing(table, "FL-05", "数传启动+遥测并行", "SKIP/人工测试",
                            "需SPI2数传过程中确认SPI1遥测仍正常");
    ReportTableAddIfMissing(table, "FL-06", "数传启动+中止+重启", "SKIP/人工测试",
                            "需数传启动、停止、再次启动并确认包序号和pending策略");
    ReportTableAddIfMissing(table, "FL-07", "UTC连续下发+缓存+数传", "SKIP/人工测试",
                            "需连续UTC、写缓存、数传导出并确认时间戳");
    ReportTableAddIfMissing(table, "FL-08", "寄存器写入+读取+遥测返回", "SKIP/人工测试",
                            "需写寄存器、读寄存器并确认遥测寄存器字段一致");
    ReportTableAddIfMissing(table, "FL-09", "复位恢复+遥测确认", "SKIP/人工测试",
                            "需遥控复位后确认复位次数、来源、启动状态和遥测恢复");
    ReportTableAddIfMissing(table, "FL-10", "看门狗复位+启动路径", "SKIP/人工测试",
                            "需看门狗复位硬件环境确认");
    ReportTableAddIfMissing(table, "FL-11", "高频遥控压力+遥测", "SKIP/人工测试",
                            "需高频连续下发遥控并确认最终状态和遥测稳定");
    ReportTableAddIfMissing(table, "FL-12", "长时间运行闭环", "SKIP",
                            "未运行ER-07/FL-12 Loop按钮；该按钮循环执行遥控、遥测、UTC、数传");

}

static void ReportRunSpi1CommandSweep(ReportTable* table, const ReportRunConfig* config)
{
    static const uint8_t rfMasks[] = {0x00U, 0x01U, 0x02U, 0x55U, 0xAAU, 0xFFU};
    void* dev;
    char detail[256];

    if (config->spi1Index < 0) {
        ReportTableAdd(table, "TC-01", "工作模式遥控下发和遥测回读", "SKIP", "未选择SPI1设备，未遍历0..6");
        ReportTableAdd(table, "TC-02", "速率遥控下发和遥测回读", "SKIP", "未选择SPI1设备，未遍历0..2");
        ReportTableAdd(table, "TC-03", "时隙配置遥控下发和遥测回读", "SKIP", "未选择SPI1设备");
        ReportTableAdd(table, "TC-04", "发射功率遥控下发和遥测回读", "SKIP", "未选择SPI1设备");
        ReportTableAdd(table, "TC-05", "频率遥控下发和遥测回读", "SKIP", "未选择SPI1设备");
        ReportTableAdd(table, "TC-06", "射频通道遥控下发和遥测回读", "SKIP", "未选择SPI1设备，未遍历代表值0x00/0x01/0x02/0x55/0xAA/0xFF");
        ReportTableAdd(table, "TC-10", "寄存器写入遥控和读回验证", "SKIP", "未选择SPI1设备");
        ReportTableAdd(table, "TC-11", "寄存器读取遥控和遥测返回", "SKIP", "未选择SPI1设备");
        return;
    }
    if (LoadJtoolApi() != 0) {
        ReportTableAdd(table, "TC-01", "工作模式遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用，未遍历0..6");
        ReportTableAdd(table, "TC-02", "速率遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用，未遍历0..2");
        ReportTableAdd(table, "TC-03", "时隙配置遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用");
        ReportTableAdd(table, "TC-04", "发射功率遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用");
        ReportTableAdd(table, "TC-05", "频率遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用");
        ReportTableAdd(table, "TC-06", "射频通道遥控下发和遥测回读", "SKIP", "jtool.dll SPI接口不可用，未遍历代表值0x00/0x01/0x02/0x55/0xAA/0xFF");
        ReportTableAdd(table, "TC-10", "寄存器写入遥控和读回验证", "SKIP", "jtool.dll SPI接口不可用");
        ReportTableAdd(table, "TC-11", "寄存器读取遥控和遥测返回", "SKIP", "jtool.dll SPI接口不可用");
        return;
    }
    dev = OpenSpiDevice(config->spi1Index, WM_APP_LOG_SPI1);
    if (dev == NULL) {
        ReportTableAdd(table, "TC-01", "工作模式遥控下发和遥测回读", "FAILED", "打开SPI1设备失败，未遍历0..6");
        ReportTableAdd(table, "TC-02", "速率遥控下发和遥测回读", "FAILED", "打开SPI1设备失败，未遍历0..2");
        ReportTableAdd(table, "TC-03", "时隙配置遥控下发和遥测回读", "FAILED", "打开SPI1设备失败");
        ReportTableAdd(table, "TC-04", "发射功率遥控下发和遥测回读", "FAILED", "打开SPI1设备失败");
        ReportTableAdd(table, "TC-05", "频率遥控下发和遥测回读", "FAILED", "打开SPI1设备失败");
        ReportTableAdd(table, "TC-06", "射频通道遥控下发和遥测回读", "FAILED", "打开SPI1设备失败，未遍历代表值0x00/0x01/0x02/0x55/0xAA/0xFF");
        ReportTableAdd(table, "TC-10", "寄存器写入遥控和读回验证", "FAILED", "打开SPI1设备失败");
        ReportTableAdd(table, "TC-11", "寄存器读取遥控和遥测返回", "FAILED", "打开SPI1设备失败");
        return;
    }
    ReportRunU8RangeWithTelemetry(table, dev, "TC-01", "工作模式遥控下发和遥测回读", 0x01U, 0U, 6U, 20U);
    ReportRunU8RangeWithTelemetry(table, dev, "TC-02", "速率遥控下发和遥测回读", 0x02U, 0U, 2U, 21U);
    ReportTableAdd(table, "TC-03", "时隙配置遥控下发和遥测回读",
                   (ReportSendSpi1AndVerifyTelemetry(dev, 0x03U, "3", 22U, 3U, detail, sizeof(detail)) == 0) ? "PASS" : "FAILED",
                   detail);
    ReportTableAdd(table, "TC-04", "发射功率遥控下发和遥测回读",
                   (ReportSendSpi1AndVerifyTelemetry(dev, 0x04U, "4", 23U, 4U, detail, sizeof(detail)) == 0) ? "PASS" : "FAILED",
                   detail);
    ReportTableAdd(table, "TC-05", "频率遥控下发和遥测回读",
                   (ReportSendSpi1AndVerifyTelemetryU32(dev, 0x05U, "492800000", 24U, 492800000UL, detail, sizeof(detail)) == 0) ? "PASS" : "FAILED",
                   detail);
    ReportRunByteValueSetWithTelemetry(table, dev, "TC-06", "射频通道遥控下发和遥测回读", 0x06U,
                                       rfMasks, (uint32_t)(sizeof(rfMasks) / sizeof(rfMasks[0])), 28U);
    ReportTableAdd(table, "TC-10", "寄存器写入遥控和读回验证",
                   (ReportSendSpi1WithHandle(dev, 0x0AU, "0xC020 0xAABBCCDD", detail, sizeof(detail)) == 0) ? "PASS" : "FAILED",
                   detail);
    ReportTableAdd(table, "TC-11", "寄存器读取遥控和遥测返回",
                   (ReportSendSpi1AndVerifyTelemetryU32(dev, 0x0BU, "0xC020", 67U, 0xAABBCCDDUL, detail, sizeof(detail)) == 0) ? "PASS" : "FAILED",
                   detail);
    (void)g_DevClose(dev);
}

static void ReportRunTelemetryRead(ReportTable* table, const ReportRunConfig* config)
{
    void* dev;
    uint8_t tx[SPI1_PAGE_LEN];
    uint8_t page0[SPI1_PAGE_LEN];
    uint8_t page1[SPI1_PAGE_LEN];
    uint8_t tm[TM_LEN];
    char detail[256];
    uint32_t tries;

    if ((config->spi1Index < 0) || (LoadJtoolApi() != 0)) {
        ReportTableAdd(table, "TM-01", "SPI1遥测读取", "SKIP", "SPI1或JTOOL不可用");
        return;
    }
    dev = OpenSpiDevice(config->spi1Index, WM_APP_LOG_SPI1);
    if (dev == NULL) {
        ReportTableAdd(table, "TM-01", "SPI1遥测读取", "FAILED", "打开SPI1设备失败");
        return;
    }
    (void)memset(tx, 0, sizeof(tx));
    for (tries = 0U; tries < 50U; tries++) {
        if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, SPI1_PAGE_LEN, tx, page0) != ErrNone) {
            (void)g_DevClose(dev);
            ReportTableAdd(table, "TM-01", "SPI1遥测读取", "FAILED", "SPI1读取遥测page0失败");
            return;
        }
        Sleep(40);
        if ((page0[0] != 0xEBU) || (page0[1] != 0x90U)) {
            continue;
        }
        if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, SPI1_PAGE_LEN, tx, page1) != ErrNone) {
            (void)g_DevClose(dev);
            ReportTableAdd(table, "TM-01", "SPI1遥测读取", "FAILED", "SPI1读取遥测page1失败");
            return;
        }
        (void)memcpy(tm, page0, SPI1_PAGE_LEN);
        (void)memcpy(&tm[SPI1_PAGE_LEN], page1, TM_LEN - SPI1_PAGE_LEN);
        if (ValidateTelemetry(tm) == 0) {
            (void)snprintf(detail, sizeof(detail), "工作模式=%u 速率=%u 异常=0x%08lX",
                           tm[20], tm[21], (unsigned long)ReadBe32(&tm[49]));
            (void)g_DevClose(dev);
            ReportTableAdd(table, "TM-01", "SPI1遥测读取", "PASS", detail);
            return;
        }
    }
    (void)g_DevClose(dev);
    ReportTableAdd(table, "TM-01", "SPI1遥测读取", "FAILED", "未在重试窗口内读到有效144字节遥测帧");
}

static void ReportRunHardwareChecks(ReportTable* table, const ReportRunConfig* config)
{
    char detail[256];
    HANDLE serial = INVALID_HANDLE_VALUE;
    void* spi1Dev = NULL;
    int uartConfigured = ((config->uartPort[0] != '\0') && (strcmp(config->uartPort, "COM?") != 0)) ? 1 : 0;
    int uartOpenFailed = 0;
    int jtoolAvailable = (LoadJtoolApi() == 0) ? 1 : 0;
    int spi1OpenFailed = 0;

    if (uartConfigured != 0) {
        serial = SerialOpen(config->uartPort, config->uartBaud);
        uartOpenFailed = (serial == INVALID_HANDLE_VALUE) ? 1 : 0;
    }
    if ((config->spi1Index >= 0) && (jtoolAvailable != 0)) {
        spi1Dev = OpenSpiDevice(config->spi1Index, WM_APP_LOG_SPI1);
        spi1OpenFailed = (spi1Dev == NULL) ? 1 : 0;
    }

    ReportRunRcErrorRows(table, spi1Dev, serial, spi1OpenFailed, uartOpenFailed);
    if (spi1Dev != NULL) {
        (void)g_DevClose(spi1Dev);
        spi1Dev = NULL;
    }
    if (ReportStopRequested() != 0) {
        goto done;
    }
    ReportRunSpi1CommandSweep(table, config);
    if (ReportStopRequested() != 0) {
        goto done;
    }
    ReportRunTelemetryRead(table, config);
    if (ReportStopRequested() != 0) {
        goto done;
    }
    if ((config->spi1Index >= 0) && (jtoolAvailable != 0)) {
        spi1Dev = OpenSpiDevice(config->spi1Index, WM_APP_LOG_SPI1);
        if (spi1Dev == NULL) {
            spi1OpenFailed = 1;
        }
    }
    ReportRunTc12Tc13Rows(table, config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed);
    if (ReportStopRequested() != 0) {
        goto done;
    }

    if (spi1Dev != NULL) {
        if ((ReportSendSpi1WithHandle(spi1Dev, 0x09U, "1", detail, sizeof(detail)) == 0) &&
            (ReportSendSpi1WithHandle(spi1Dev, 0x09U, "0", detail, sizeof(detail)) == 0)) {
            ReportTableAdd(table, "TC-09", "数传打开/关闭遥控", "PASS",
                           "SPI1已下发CMD_09参数1和0，完成数传开关遥控；实际数据内容由DT-01..DT-04判定");
        } else {
            ReportTableAdd(table, "TC-09", "数传打开/关闭遥控", "FAILED", detail);
        }
    } else {
        ReportTableAdd(table, "TC-09", "数传打开/关闭遥控",
                       ReportMissingAutomatedResourceResult(spi1OpenFailed),
                        (spi1OpenFailed != 0) ? "打开SPI1设备失败" : "SPI1或JTOOL不可用");
    }
    if (ReportStopRequested() != 0) {
        goto done;
    }

    ReportRunDtRows(table, config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed);
    if (ReportStopRequested() != 0) {
        goto done;
    }
    ReportRunErAndFlowRows(table, config, spi1Dev, serial, spi1OpenFailed, uartOpenFailed);

done:
    if (spi1Dev != NULL) {
        (void)g_DevClose(spi1Dev);
    }
    if (serial != INVALID_HANDLE_VALUE) {
        CloseHandle(serial);
    }
}

static DWORD WINAPI ReportRunThread(LPVOID param)
{
    ReportRunConfig* config = (ReportRunConfig*)param;
    ReportTable table;
    char reportPath[160];
    char rawPath[160];
    FILE* raw;
    uint32_t i;
    int year;
    int month;
    int day;

    (void)memset(&table, 0, sizeof(table));
    GetCurrentTimestamp(config->startTime, sizeof(config->startTime), &year, &month, &day);
    PostLog(WM_APP_LOG_SPI1, "Report tests started UART=%s baud=%lu SPI1=%d SPI2=%d",
            config->uartPort, (unsigned long)config->uartBaud,
            config->spi1Index, config->spi2Index);
    ReportUpdateProgress(&table);
    ReportRunHardwareChecks(&table, config);
    if (ReportStopRequested() == 0) {
        ReportAddMissingV01Rows(&table);
        ReportUpdateProgress(&table);
    } else {
        PostLog(WM_APP_LOG_SPI1, "Report tests force-stopped after %lu item(s)",
                (unsigned long)table.count);
    }
    GetCurrentTimestamp(config->endTime, sizeof(config->endTime), &year, &month, &day);

    if ((BuildReportPath(reportPath, sizeof(reportPath), "report", "md") == 0) &&
        (ReportWriteMarkdown(reportPath, &table, config) == 0)) {
        PostLog(WM_APP_LOG_SPI1, "Report tests wrote %s", reportPath);
    } else {
        PostLog(WM_APP_LOG_SPI1, "ERROR report tests failed to write markdown report");
    }
    if (BuildReportPath(rawPath, sizeof(rawPath), "raw", "log") == 0) {
        raw = fopen(rawPath, "wb");
        if (raw != NULL) {
            for (i = 0U; i < table.count; i++) {
                (void)fprintf(raw, "%s\t%s\t%s\t%s\r\n",
                              table.rows[i].id, table.rows[i].item,
                              table.rows[i].result, table.rows[i].detail);
            }
            (void)fclose(raw);
            PostLog(WM_APP_LOG_SPI1, "Report tests wrote %s", rawPath);
        }
    }
    PostLog(WM_APP_LOG_SPI1, "Report tests complete PASS=%lu FAILED=%lu SKIP=%lu TOTAL=%lu",
            (unsigned long)table.pass, (unsigned long)table.failed,
            (unsigned long)table.skip, (unsigned long)table.count);
    InterlockedExchange(&g_app.reportRunActive, 0);
    InterlockedExchange(&g_app.reportStop, 0);
    UpdateSpiToggleButtons();
    free(config);
    return 0;
}

static DWORD WINAPI LongRunThread(LPVOID param)
{
    ReportRunConfig* config = (ReportRunConfig*)param;
    HANDLE serial = INVALID_HANDLE_VALUE;
    void* spi1Dev = NULL;
    uint8_t payload[] = {'l', 'o', 'o', 'p'};
    char detail[256];
    char utcText[32];
    uint32_t iteration = 0U;

    PostLog(WM_APP_LOG_SPI1, "ER-07/FL-12 loop starting UART=%s baud=%lu SPI1=%d SPI2=%d",
            config->uartPort, (unsigned long)config->uartBaud,
            config->spi1Index, config->spi2Index);
    if ((config->uartPort[0] == '\0') || (strcmp(config->uartPort, "COM?") == 0)) {
        PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop requires UART port");
        goto done;
    }
    if ((config->spi1Index < 0) || (config->spi2Index < 0)) {
        PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop requires SPI1 and SPI2 selections");
        goto done;
    }
    serial = SerialOpen(config->uartPort, config->uartBaud);
    if (serial == INVALID_HANDLE_VALUE) {
        PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop open UART failed");
        goto done;
    }
    if (LoadJtoolApi() != 0) {
        PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop jtool.dll unavailable");
        goto done;
    }
    spi1Dev = OpenSpiDevice(config->spi1Index, WM_APP_LOG_SPI1);
    if (spi1Dev == NULL) {
        PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop open SPI1 failed");
        goto done;
    }
    while (InterlockedCompareExchange(&g_app.longRunStop, 0, 0) == 0) {
        uint32_t utcSeconds = (uint32_t)time(NULL);
        int stepOk = 1;

        iteration++;
        PostLog(WM_APP_LOG_SPI1, "ER-07/FL-12 loop iteration=%lu start", (unsigned long)iteration);
        if (ReportSendSpi1AndVerifyTelemetry(spi1Dev, 0x02U, "1", 21U, 1U, detail, sizeof(detail)) != 0) {
            PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop RC/TM failed: %s", detail);
            stepOk = 0;
        }
        (void)snprintf(utcText, sizeof(utcText), "%lu", (unsigned long)utcSeconds);
        if ((stepOk != 0) && (ReportSendSpi1WithHandle(spi1Dev, 0x0DU, utcText, detail, sizeof(detail)) != 0)) {
            PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop UTC failed: %s", detail);
            stepOk = 0;
        }
        if ((stepOk != 0) &&
            (ReportRunDtScenario(config, spi1Dev, serial, 0, 0, "AT+DTWRITE=S,3,loop",
                                 utcSeconds, utcSeconds + 120U, DT_FORMAT_STRING, 3U,
                                 payload, (uint16_t)sizeof(payload), detail, sizeof(detail)) != 0)) {
            PostLog(WM_APP_LOG_SPI1, "ERROR ER-07/FL-12 loop DT failed: %s", detail);
            stepOk = 0;
        }
        InterlockedIncrement(&g_app.longRunIterations);
        if (stepOk == 0) {
            InterlockedIncrement(&g_app.longRunFailures);
        }
        PostLog(WM_APP_LOG_SPI1, "ER-07/FL-12 loop iteration=%lu %s",
                (unsigned long)iteration, (stepOk != 0) ? "PASS" : "FAILED");
        Sleep(1000);
    }

done:
    if (spi1Dev != NULL) {
        (void)g_DevClose(spi1Dev);
    }
    if (serial != INVALID_HANDLE_VALUE) {
        CloseHandle(serial);
    }
    PostLog(WM_APP_LOG_SPI1, "ER-07/FL-12 loop stopped iterations=%ld failures=%ld",
            InterlockedCompareExchange(&g_app.longRunIterations, 0, 0),
            InterlockedCompareExchange(&g_app.longRunFailures, 0, 0));
    InterlockedExchange(&g_app.longRunActive, 0);
    InterlockedExchange(&g_app.longRunStop, 0);
    UpdateSpiToggleButtons();
    free(config);
    return 0;
}

static void StartReportTests(void)
{
    ReportRunConfig* config;

    if (InterlockedCompareExchange(&g_app.reportRunActive, 1, 0) != 0) {
        InterlockedExchange(&g_app.reportStop, 1);
        SetWindowTextA(g_app.reportRun, "Stopping...");
        PostLog(WM_APP_LOG_SPI1, "Report tests stop requested");
        return;
    }
    if ((InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0) ||
        (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) ||
        (InterlockedCompareExchange(&g_app.longRunActive, 1, 1) != 0)) {
        InterlockedExchange(&g_app.reportRunActive, 0);
        PostLog(WM_APP_LOG_SPI1, "ERROR stop SPI1/SPI2 monitors and ER-07/FL-12 loop before running report tests");
        return;
    }
    config = (ReportRunConfig*)calloc(1U, sizeof(*config));
    if (config == NULL) {
        InterlockedExchange(&g_app.reportRunActive, 0);
        return;
    }
    ReadReportRunConfigFromUi(config);
    InterlockedExchange(&g_app.reportStop, 0);
    if (g_app.reportProgress != NULL) {
        SendMessageA(g_app.reportProgress, PBM_SETPOS, 0, 0);
    }
    UpdateSpiToggleButtons();
    g_app.reportThread = CreateThread(NULL, 0, ReportRunThread, config, 0, NULL);
    if (g_app.reportThread == NULL) {
        UpdateSpiToggleButtons();
        InterlockedExchange(&g_app.reportRunActive, 0);
        free(config);
        PostLog(WM_APP_LOG_SPI1, "ERROR create report test thread failed");
    }
}

static void ToggleLongRun(void)
{
    ReportRunConfig* config;

    if (InterlockedCompareExchange(&g_app.longRunActive, 1, 1) != 0) {
        InterlockedExchange(&g_app.longRunStop, 1);
        SetWindowTextA(g_app.longRun, "Stopping...");
        return;
    }
    if ((InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0) ||
        (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) ||
        (InterlockedCompareExchange(&g_app.reportRunActive, 1, 1) != 0)) {
        PostLog(WM_APP_LOG_SPI1, "ERROR stop SPI1/SPI2 monitors and report tests before ER-07/FL-12 loop");
        return;
    }
    config = (ReportRunConfig*)calloc(1U, sizeof(*config));
    if (config == NULL) {
        PostLog(WM_APP_LOG_SPI1, "ERROR allocate ER-07/FL-12 loop config failed");
        return;
    }
    if (g_app.longRunThread != NULL) {
        CloseHandle(g_app.longRunThread);
        g_app.longRunThread = NULL;
    }
    ReadReportRunConfigFromUi(config);
    InterlockedExchange(&g_app.longRunIterations, 0);
    InterlockedExchange(&g_app.longRunFailures, 0);
    InterlockedExchange(&g_app.longRunStop, 0);
    InterlockedExchange(&g_app.longRunActive, 1);
    UpdateSpiToggleButtons();
    g_app.longRunThread = CreateThread(NULL, 0, LongRunThread, config, 0, NULL);
    if (g_app.longRunThread == NULL) {
        InterlockedExchange(&g_app.longRunActive, 0);
        free(config);
        UpdateSpiToggleButtons();
        PostLog(WM_APP_LOG_SPI1, "ERROR create ER-07/FL-12 loop thread failed");
    }
}

static void PostLog(UINT msg, const char* fmt, ...)
{
#ifdef _WIN32
    char stack[2048];
    char timestamp[32];
    char* raw;
    char* text;
    va_list ap;
    int len;
    int year;
    int month;
    int day;
    size_t rawLen;
    size_t lineCount = 1U;
    size_t i;

    va_start(ap, fmt);
    len = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (len < 0) {
        return;
    }
    raw = (char*)malloc((size_t)len + 3U);
    if (raw == NULL) {
        return;
    }
    va_start(ap, fmt);
    (void)vsnprintf(raw, (size_t)len + 1U, fmt, ap);
    va_end(ap);
    (void)strcat(raw, "\r\n");
    rawLen = strlen(raw);
    for (i = 0U; i < rawLen; i++) {
        if (raw[i] == '\n') {
            lineCount++;
        }
    }
    text = (char*)malloc(rawLen + (lineCount * 24U) + 1U);
    if (text == NULL) {
        free(raw);
        return;
    }
    GetCurrentTimestamp(timestamp, sizeof(timestamp), &year, &month, &day);
    (void)year;
    (void)month;
    (void)day;
    if (FormatTimestampedLog(text, rawLen + (lineCount * 24U) + 1U, timestamp, raw) != 0) {
        free(raw);
        free(text);
        return;
    }
    free(raw);
    (void)PostMessageA(g_app.hwnd, msg, 0, (LPARAM)text);
#else
    (void)msg;
    (void)fmt;
#endif
}

static int TrimStartForAppend(int currentLength, int appendLength, int maxLength);

static wchar_t* Utf8ToWideAlloc(const char* text)
{
    int needed;
    wchar_t* wide;

    if (text == NULL) {
        text = "";
    }
    needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (needed <= 0) {
        needed = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    }
    if (needed <= 0) {
        return NULL;
    }
    wide = (wchar_t*)calloc((size_t)needed, sizeof(*wide));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, needed) <= 0) {
        if (MultiByteToWideChar(CP_ACP, 0, text, -1, wide, needed) <= 0) {
            free(wide);
            return NULL;
        }
    }
    return wide;
}

static void AppendLogText(HWND edit, const char* text)
{
#ifdef _WIN32
    wchar_t* wide = Utf8ToWideAlloc(text);
    int appendLength = (wide == NULL) ? (int)strlen(text) : (int)wcslen(wide);
    int length = GetWindowTextLengthW(edit);
    int trim = TrimStartForAppend(length, appendLength, LOG_MAX_CHARS);

    if (trim > 0) {
        SendMessageW(edit, EM_SETSEL, 0, (LPARAM)trim);
        SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"");
        length = GetWindowTextLengthW(edit);
    }
    SendMessageW(edit, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    if (wide != NULL) {
        SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)wide);
        free(wide);
    } else {
        SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    }
#else
    (void)edit;
    (void)text;
#endif
}

static int TrimStartForAppend(int currentLength, int appendLength, int maxLength)
{
    int overflow = currentLength + appendLength - maxLength;

    if (overflow <= 0) {
        return 0;
    }
    overflow += maxLength / 4;
    if (overflow > currentLength) {
        return currentLength;
    }
    return overflow;
}

static const char* GetCommandHint(uint8_t command)
{
    switch (command) {
        case 0x01U: return "mode: 0..6 decimal; invalid values allowed for DUT negative tests";
        case 0x02U: return "rate: 0..2 decimal; maps to TK8710 6/7/8; invalid tests allowed";
        case 0x03U: return "slotConfig: 1 byte decimal/0x, e.g. 64 or 0x40";
        case 0x04U: return "txPower: 1 byte decimal, e.g. 26";
        case 0x05U: return "frequency: decimal Hz or 0x-prefixed u32, e.g. 473200000";
        case 0x06U: return "rfMask: 1 byte hex/decimal, e.g. 0xFF";
        case 0x07U: return "reset: no params";
        case 0x08U: return "upgrade: 6 raw hex bytes, e.g. 01 02 03 04 05 06";
        case 0x09U: return "data transfer switch: 0=off, 1=on; other params ignored";
        case 0x0AU: return "write reg: regAddr regValue, e.g. 0x1234 0xAABBCCDD";
        case 0x0BU: return "read reg: regAddr, e.g. 0x1234";
        case 0x0CU: return "rollback: 6 raw hex bytes, e.g. 01 02 03 04 05 06";
        case 0x0DU: return "UTC seconds: decimal/0x u32, e.g. 1723246576";
        case 0x0EU: return "DC: antenna iDc qDc, e.g. 1 0x0010 0xFFF0";
        case GUI_COMMAND_MANUAL_RAW: return "manual raw SPI1 frame: 1..128 hex bytes, sent exactly as entered";
        default: return "";
    }
}

static int ParseHexBytes(const char* text, uint8_t* out, uint32_t maxLen, uint32_t* outLen)
{
    uint32_t count = 0U;
    const char* p = text;

    while ((p != NULL) && (*p != '\0')) {
        const char* end;
        char token[16];
        uint32_t tokenLen = 0U;
        int base = 16;
        unsigned long value;

        while ((*p == ' ') || (*p == '\t') || (*p == ',') || (*p == ';')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != ',') && (*p != ';')) {
            if (tokenLen >= (sizeof(token) - 1U)) {
                return -1;
            }
            token[tokenLen++] = *p++;
        }
        token[tokenLen] = '\0';
        if ((tokenLen > 2U) && (token[0] == '0') && ((token[1] == 'x') || (token[1] == 'X'))) {
            base = 0;
        }
        value = strtoul(token, (char**)&end, base);
        if ((end == token) || (*end != '\0')) {
            return -1;
        }
        if ((value > 0xFFUL) || (count >= maxLen)) {
            return -1;
        }
        out[count++] = (uint8_t)value;
    }
    *outLen = count;
    return 0;
}

static int BuildGuiCommand(uint8_t command, const uint8_t* params, uint32_t paramLen, uint8_t* tx)
{
    if ((command == 0U) || (command > 0x0EU) || (paramLen > 6U)) {
        return -1;
    }
    BuildCommand(tx, command);
    if (paramLen != 0U) {
        (void)memcpy(&tx[3], params, paramLen);
    }
    FinishCommand(tx);
    return 0;
}

static uint8_t CommandRepeatCount(uint8_t command)
{
    (void)command;
    return 1U;
}

static uint8_t CommandFromComboIndex(int commandIndex)
{
    return (commandIndex == 14) ? GUI_COMMAND_MANUAL_RAW : (uint8_t)(commandIndex + 1);
}

static void FormatCommandLogLabel(uint8_t selectedCommand, const uint8_t* tx,
                                  char* out, size_t outSize)
{
    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    if (selectedCommand == GUI_COMMAND_MANUAL_RAW) {
        (void)snprintf(out, outSize, "manual SPI1 frame");
        return;
    }
    (void)snprintf(out, outSize, "CMD_%02X", (tx != NULL) ? tx[2] : selectedCommand);
}

static int ParseU32Token(const char* text, uint32_t* value)
{
    char* end;
    unsigned long parsed;
    int base = 10;

    if ((text == NULL) || (value == NULL) || (*text == '\0')) {
        return -1;
    }
    if ((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X'))) {
        base = 16;
    }
    parsed = strtoul(text, &end, base);
    if (*end != '\0') {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int NextToken(const char** cursor, char* token, size_t tokenSize)
{
    const char* p = *cursor;
    size_t len = 0U;

    while ((*p == ' ') || (*p == '\t') || (*p == ',') || (*p == ';')) {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return 0;
    }
    while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != ',') && (*p != ';')) {
        if (len >= (tokenSize - 1U)) {
            return -1;
        }
        token[len++] = *p++;
    }
    token[len] = '\0';
    *cursor = p;
    return 1;
}

static int ParseCommandValues(const char* text, uint32_t* values, uint32_t maxValues,
                              uint32_t* valueCount)
{
    const char* cursor = text;
    uint32_t count = 0U;

    while (count < maxValues) {
        char token[32];
        int tokenResult = NextToken(&cursor, token, sizeof(token));

        if (tokenResult < 0) {
            return -1;
        }
        if (tokenResult == 0) {
            *valueCount = count;
            return 0;
        }
        if (ParseU32Token(token, &values[count]) != 0) {
            return -1;
        }
        count++;
    }
    {
        char token[32];
        if (NextToken(&cursor, token, sizeof(token)) != 0) {
            return -1;
        }
    }
    *valueCount = count;
    return 0;
}

static int BuildGuiCommandFromText(uint8_t command, const char* text, uint8_t* tx,
                                   uint32_t* txLength, uint8_t* requestSpi2Capture,
                                   char* error, size_t errorSize)
{
    uint8_t params[6];
    uint32_t paramLen = 0U;
    uint32_t values[3];
    uint32_t valueCount = 0U;

    if ((tx == NULL) || (txLength == NULL) || (requestSpi2Capture == NULL)) {
        (void)snprintf(error, errorSize, "internal command build error");
        return -1;
    }
    *txLength = SPI1_PAGE_LEN;
    *requestSpi2Capture = 0U;
    (void)memset(params, 0, sizeof(params));
    if (command == GUI_COMMAND_MANUAL_RAW) {
        (void)memset(tx, 0, TM_LEN);
        if ((ParseHexBytes(text, tx, SPI1_PAGE_LEN, &paramLen) != 0) || (paramLen == 0U)) {
            (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
            return -1;
        }
        *txLength = paramLen;
        *requestSpi2Capture = (uint8_t)IsDataTransferCommandFrame(tx, paramLen);
        return 0;
    }
    switch (command) {
        case 0x01U:
        case 0x02U:
        case 0x03U:
        case 0x04U:
        case 0x06U:
            if ((ParseCommandValues(text, values, 1U, &valueCount) != 0) ||
                (valueCount != 1U) || (values[0] > 0xFFU)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            params[0] = (uint8_t)values[0];
            paramLen = 1U;
            break;
        case 0x05U:
        case 0x0DU:
            if ((ParseCommandValues(text, values, 1U, &valueCount) != 0) || (valueCount != 1U)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            WriteBe32(params, values[0]);
            paramLen = 4U;
            break;
        case 0x07U:
            if ((text != NULL) && (strspn(text, " \t,;") != strlen(text))) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            paramLen = 0U;
            break;
        case 0x09U:
            if ((ParseCommandValues(text, values, 1U, &valueCount) != 0) ||
                (valueCount != 1U) || (values[0] > 1U)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            params[0] = (uint8_t)values[0];
            paramLen = 1U;
            break;
        case 0x08U:
        case 0x0CU:
            if ((ParseHexBytes(text, params, sizeof(params), &paramLen) != 0) || (paramLen != 6U)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            break;
        case 0x0AU:
            if ((ParseCommandValues(text, values, 2U, &valueCount) != 0) ||
                (valueCount != 2U) || (values[0] > 0xFFFFU)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            WriteBe16(&params[0], (uint16_t)values[0]);
            WriteBe32(&params[2], values[1]);
            paramLen = 6U;
            break;
        case 0x0BU:
            if ((ParseCommandValues(text, values, 1U, &valueCount) != 0) ||
                (valueCount != 1U) || (values[0] > 0xFFFFU)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            WriteBe16(&params[0], 0U);
            WriteBe16(&params[2], (uint16_t)values[0]);
            paramLen = 4U;
            break;
        case 0x0EU:
            if ((ParseCommandValues(text, values, 3U, &valueCount) != 0) ||
                (valueCount != 3U) || (values[0] > 0xFFU) ||
                (values[1] > 0xFFFFU) || (values[2] > 0xFFFFU)) {
                (void)snprintf(error, errorSize, "%s", GetCommandHint(command));
                return -1;
            }
            params[0] = (uint8_t)values[0];
            WriteBe16(&params[1], (uint16_t)values[1]);
            WriteBe16(&params[3], (uint16_t)values[2]);
            paramLen = 5U;
            break;
        default:
            (void)snprintf(error, errorSize, "unknown command");
            return -1;
    }
    if (BuildGuiCommand(command, params, paramLen, tx) != 0) {
        (void)snprintf(error, errorSize, "command build failed");
        return -1;
    }
    *requestSpi2Capture = ((command == 0x09U) && (tx[3] == 1U)) ? 1U : 0U;
    return 0;
}

static int QueueSpi1Command(Spi1CommandQueue* queue, const uint8_t* tx,
                            uint32_t txLength, uint8_t isManual,
                            uint8_t requestSpi2Capture)
{
    if ((queue == NULL) || (tx == NULL) || (txLength == 0U) || (txLength > SPI1_PAGE_LEN)) {
        return -1;
    }
    (void)memset(queue->tx, 0, sizeof(queue->tx));
    (void)memcpy(queue->tx, tx, txLength);
    queue->txLength = txLength;
    queue->isManual = (isManual != 0U) ? 1U : 0U;
    queue->requestSpi2Capture = (requestSpi2Capture != 0U) ? 1U : 0U;
    queue->repeatRemaining = CommandRepeatCount(((isManual == 0U) && (txLength > 2U)) ? tx[2] : 0U);
    queue->pending = 1U;
    return 0;
}

static int TakeSpi1Command(Spi1CommandQueue* queue, uint8_t* tx,
                           uint32_t* txLength, uint8_t* isManual,
                           uint8_t* requestSpi2Capture)
{
    if ((queue == NULL) || (tx == NULL)) {
        return 0;
    }
    if (txLength != NULL) {
        *txLength = SPI1_PAGE_LEN;
    }
    if (isManual != NULL) {
        *isManual = 0U;
    }
    if (requestSpi2Capture != NULL) {
        *requestSpi2Capture = 0U;
    }
    if (queue->pending == 0U) {
        (void)memset(tx, 0, TM_LEN);
        return 0;
    }
    (void)memcpy(tx, queue->tx, TM_LEN);
    if (txLength != NULL) {
        *txLength = queue->txLength;
    }
    if (isManual != NULL) {
        *isManual = queue->isManual;
    }
    if (requestSpi2Capture != NULL) {
        *requestSpi2Capture = queue->requestSpi2Capture;
    }
    if (queue->repeatRemaining > 0U) {
        queue->repeatRemaining--;
    }
    if (queue->repeatRemaining == 0U) {
        queue->pending = 0U;
    }
    return 1;
}

static int ValidateTelemetry(const uint8_t* rx)
{
    return ((rx[0] == 0xEBU) && (rx[1] == 0x90U) &&
            (Checksum8(rx, 2U, 142U) == rx[143])) ? 0 : -1;
}

static void FormatTelemetry(const uint8_t* rx, char* out, size_t outSize)
{
    char hex[480];

    FormatHex(hex, sizeof(hex), rx, TM_LEN);
    (void)snprintf(out, outSize,
                   "checksum=OK noise=[%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f]\r\n"
                   "param mode=%u rate=%u slotConfig=%u txPower=%u freqHz=%lu rfMask=%u\r\n"
                   "ver=%u.%u.%u uptime=%lu rxCount=%lu txCount=%lu resetCount=%u resetType=%u\r\n"
                   "reg device=0x%04X addr=0x%04X value=0x%08lX\r\nTM: %s",
                   ((double)ReadS16Be(&rx[2])) / 128.0,
                   ((double)ReadS16Be(&rx[4])) / 128.0,
                   ((double)ReadS16Be(&rx[6])) / 128.0,
                   ((double)ReadS16Be(&rx[8])) / 128.0,
                   ((double)ReadS16Be(&rx[10])) / 128.0,
                   ((double)ReadS16Be(&rx[12])) / 128.0,
                   ((double)ReadS16Be(&rx[14])) / 128.0,
                   ((double)ReadS16Be(&rx[16])) / 128.0,
                   rx[20], rx[21], rx[22], rx[23],
                   (unsigned long)ReadBe32(&rx[24]), rx[28],
                   rx[29], rx[30], rx[31],
                   (unsigned long)ReadBe32(&rx[32]),
                   (unsigned long)ReadBe32(&rx[36]),
                   (unsigned long)ReadBe32(&rx[40]),
                   rx[44], rx[45], ReadBe16(&rx[63]), ReadBe16(&rx[65]),
                   (unsigned long)ReadBe32(&rx[67]), hex);
}

static int ValidateDtFrame(const uint8_t* frame, uint16_t* seq, uint16_t* length,
                           char* error, size_t errorSize)
{
    uint32_t sync = ReadBe32(frame);
    uint16_t frameLength = ReadBe16(&frame[4]);
    uint16_t frameSeq = ReadBe16(&frame[6]);
    uint16_t expected = DtFrameChecksum(frame);
    uint16_t actual = ReadBe16(&frame[DT_FRAME_LEN - 2U]);

    if (sync != DT_FRAME_SYNC) {
        (void)snprintf(error, errorSize, "BAD_SYNC seq=%u sync=0x%08lX", frameSeq, (unsigned long)sync);
        return -1;
    }
    if (frameLength > DT_FRAME_DATA_LEN) {
        (void)snprintf(error, errorSize, "BAD_LENGTH seq=%u length=%u", frameSeq, frameLength);
        return -1;
    }
    if (expected != actual) {
        (void)snprintf(error, errorSize, "BAD_CHECKSUM seq=%u expected=0x%04X actual=0x%04X",
                       frameSeq, expected, actual);
        return -1;
    }
    if (seq != NULL) {
        *seq = frameSeq;
    }
    if (length != NULL) {
        *length = frameLength;
    }
    return 0;
}

static int AppendFramePayload(ByteBuffer* stream, const uint8_t* frame, char* error, size_t errorSize)
{
    uint16_t length = 0U;

    if (ValidateDtFrame(frame, NULL, &length, error, errorSize) != 0) {
        return -1;
    }
    return BufferAppend(stream, &frame[8], length);
}

static void FormatRecordContent(const uint8_t* data, uint16_t length, uint8_t format,
                                char* out, size_t outSize)
{
    uint16_t i;
    size_t used = 0U;

    if (outSize == 0U) {
        return;
    }
    out[0] = '\0';
    if (format != DT_FORMAT_STRING) {
        FormatHex(out, outSize, data, length);
        return;
    }
    used += (size_t)snprintf(out, outSize, "\"");
    for (i = 0U; i < length; i++) {
        int written;
        if ((data[i] == '\\') || (data[i] == '"')) {
            written = snprintf(&out[used], outSize - used, "\\%c", data[i]);
        } else if ((data[i] >= 0x20U) && (data[i] <= 0x7EU)) {
            written = snprintf(&out[used], outSize - used, "%c", data[i]);
        } else {
            written = snprintf(&out[used], outSize - used, "\\x%02X", data[i]);
        }
        if ((written < 0) || ((size_t)written >= (outSize - used))) {
            out[outSize - 1U] = '\0';
            return;
        }
        used += (size_t)written;
    }
    if (used < (outSize - 1U)) {
        (void)snprintf(&out[used], outSize - used, "\"");
    }
}

static int DecodeRecordStream(RecordStream* stream, const uint8_t* data, uint32_t length,
                               UINT logMsg, uint32_t sourceFrame)
{
    uint32_t offset = 0U;

    if (BufferAppend(&stream->pending, data, length) != 0) {
        return -1;
    }
    if (FrameIndexAppend(&stream->pendingFrames, sourceFrame, length) != 0) {
        return -1;
    }
    while (offset < stream->pending.length) {
        uint32_t available = stream->pending.length - offset;
        uint32_t recordBytes;
        uint32_t startFrame;
        uint32_t endFrame;
        uint32_t timestamp;
        uint16_t recordLength;
        uint8_t format;
        uint8_t type;
        const char* formatName;
        char content[1024];
        char timestampText[24];

        if (available < DT_RECORD_HEADER_LEN) {
            break;
        }
        recordLength = ReadBe16(&stream->pending.data[offset + 6U]);
        if ((available - DT_RECORD_HEADER_LEN) < recordLength) {
            break;
        }
        recordBytes = DT_RECORD_HEADER_LEN + (uint32_t)recordLength;
        startFrame = stream->pendingFrames.data[offset];
        endFrame = stream->pendingFrames.data[offset + recordBytes - 1U];
        timestamp = ReadBe32(&stream->pending.data[offset]);
        format = stream->pending.data[offset + 4U];
        type = stream->pending.data[offset + 5U];
        formatName = (format == DT_FORMAT_STRING) ? "string" :
                     ((format == DT_FORMAT_BINARY) ? "hex" : "unknown");
        FormatDtTimestamp(timestamp, timestampText, sizeof(timestampText));
        FormatRecordContent(&stream->pending.data[offset + DT_RECORD_HEADER_LEN],
                            recordLength, format, content, sizeof(content));
        if (logMsg != 0U) {
            PostLog(logMsg, "RECORD index=%lu frameSpan=%lu..%lu recordBytes=%lu time=%s rawTime=0x%08lX format=%s type=0x%02X length=%u content=%s",
                    (unsigned long)stream->nextIndex,
                    (unsigned long)startFrame, (unsigned long)endFrame,
                    (unsigned long)recordBytes, timestampText, (unsigned long)timestamp,
                    formatName, type, recordLength, content);
        }
        stream->lastRecordStartFrame = startFrame;
        stream->lastRecordEndFrame = endFrame;
        stream->lastRecordBytes = recordBytes;
        offset += recordBytes;
        stream->nextIndex++;
    }
    if (offset != 0U) {
        stream->pending.length -= offset;
        stream->pendingFrames.length -= offset;
        if (stream->pending.length != 0U) {
            (void)memmove(stream->pending.data, &stream->pending.data[offset], stream->pending.length);
            (void)memmove(stream->pendingFrames.data, &stream->pendingFrames.data[offset],
                          (size_t)stream->pendingFrames.length * sizeof(stream->pendingFrames.data[0]));
        }
    }
    return 0;
}

static int RecordStreamAppendAndMatch(RecordStream* stream, const uint8_t* data,
                                      uint32_t length, uint32_t minTimestamp,
                                      uint32_t maxTimestamp,
                                      uint8_t expectedFormat, uint8_t expectedType,
                                      const uint8_t* expectedPayload,
                                      uint16_t expectedLength)
{
    uint32_t offset = 0U;

    if (BufferAppend(&stream->pending, data, length) != 0) {
        return -1;
    }
    while (offset < stream->pending.length) {
        uint32_t available = stream->pending.length - offset;
        uint32_t timestamp;
        uint16_t recordLength;
        uint8_t format;
        uint8_t type;

        if (available < DT_RECORD_HEADER_LEN) {
            break;
        }
        recordLength = ReadBe16(&stream->pending.data[offset + 6U]);
        if ((available - DT_RECORD_HEADER_LEN) < recordLength) {
            break;
        }
        timestamp = ReadBe32(&stream->pending.data[offset]);
        format = stream->pending.data[offset + 4U];
        type = stream->pending.data[offset + 5U];
        if (((minTimestamp == 0U) ||
             ((timestamp >= minTimestamp) && (timestamp <= maxTimestamp))) &&
            (format == expectedFormat) && (type == expectedType) &&
            (recordLength == expectedLength) &&
            (memcmp(&stream->pending.data[offset + DT_RECORD_HEADER_LEN],
                    expectedPayload, expectedLength) == 0)) {
            stream->lastRecordBytes = DT_RECORD_HEADER_LEN + (uint32_t)recordLength;
            return 1;
        }
        offset += DT_RECORD_HEADER_LEN + (uint32_t)recordLength;
    }
    if (offset != 0U) {
        stream->pending.length -= offset;
        if (stream->pending.length != 0U) {
            (void)memmove(stream->pending.data, &stream->pending.data[offset], stream->pending.length);
        }
    }
    return 0;
}

static void BuildDtRecord(ByteBuffer* stream, uint32_t timestamp, uint8_t format,
                          uint8_t type, const uint8_t* payload, uint16_t length)
{
    uint8_t header[DT_RECORD_HEADER_LEN];

    WriteBe32(&header[0], timestamp);
    header[4] = format;
    header[5] = type;
    WriteBe16(&header[6], length);
    (void)BufferAppend(stream, header, sizeof(header));
    (void)BufferAppend(stream, payload, length);
}

static void BuildDtFrame(uint8_t* frame, uint16_t seq, const uint8_t* data, uint16_t length)
{
    (void)memset(frame, 0x5A, DT_FRAME_LEN);
    WriteBe32(&frame[0], DT_FRAME_SYNC);
    WriteBe16(&frame[4], length);
    WriteBe16(&frame[6], seq);
    (void)memcpy(&frame[8], data, length);
    WriteBe16(&frame[DT_FRAME_LEN - 2U], DtFrameChecksum(frame));
}

static DeviceAssignment DefaultAssignment(int deviceCount)
{
    DeviceAssignment assignment;

    assignment.spi1Index = (deviceCount > 0) ? 0 : -1;
    assignment.spi2Index = (deviceCount > 1) ? 1 : assignment.spi1Index;
    return assignment;
}

static int ParseScannedDeviceEntries(const char* scanText, int scannedCount,
                                     DeviceEntry* entries, int maxEntries)
{
    const char* cursor = scanText;
    int count = 0;

    if ((entries == NULL) || (maxEntries <= 0)) {
        return 0;
    }
    if (scanText != NULL) {
        while ((count < maxEntries) && ((cursor = strstr(cursor, "SN:")) != NULL)) {
            const char* start = cursor + 3;
            const char* end = strchr(start, ')');
            size_t snLen;

            if (end == NULL) {
                break;
            }
            snLen = (size_t)(end - start);
            if ((snLen > 0U) && (snLen < sizeof(entries[count].sn))) {
                (void)memset(&entries[count], 0, sizeof(entries[count]));
                entries[count].type = DEVICE_ENTRY_SN;
                entries[count].id = count;
                (void)memcpy(entries[count].sn, start, snLen);
                entries[count].sn[snLen] = '\0';
                (void)snprintf(entries[count].label, sizeof(entries[count].label),
                               "SN%d %s", count + 1, entries[count].sn);
                count++;
            }
            cursor = end + 1;
        }
    }
    while ((count < scannedCount) && (count < maxEntries)) {
        (void)memset(&entries[count], 0, sizeof(entries[count]));
        entries[count].type = DEVICE_ENTRY_MANUAL;
        entries[count].id = count;
        (void)snprintf(entries[count].label, sizeof(entries[count].label),
                       "Scanned SPI USB #%d", count);
        count++;
    }
    return count;
}

static int EffectiveDeviceListCount(int scannedCount)
{
    if (scannedCount < 0) {
        scannedCount = 0;
    }
    if (scannedCount < 2) {
        return 4;
    }
    return scannedCount;
}

static MainLayout ComputeMainLayout(int width, int height)
{
    MainLayout layout;
    int gap = 15;
    int margin = 10;
    int top = LOG_EDIT_TOP;
    int statusHeight = 24;
    int minWidth = 700;
    int minHeight = 360;

    if (width < minWidth) {
        width = minWidth;
    }
    if (height < minHeight) {
        height = minHeight;
    }

    layout.statusX = margin;
    layout.statusY = height - statusHeight - margin;
    layout.statusW = width - (margin * 2);
    layout.statusH = statusHeight;
    layout.spi1X = margin;
    layout.spi1Y = top;
    layout.spi1W = (width - (margin * 2) - gap) / 2;
    layout.spi1H = layout.statusY - top - margin;
    layout.spi2X = layout.spi1X + layout.spi1W + gap;
    layout.spi2Y = top;
    layout.spi2W = width - layout.spi2X - margin;
    layout.spi2H = layout.spi1H;
    return layout;
}

static void ApplyMainLayout(HWND hwnd, int width, int height)
{
    MainLayout layout = ComputeMainLayout(width, height);

    (void)MoveWindow(g_app.spi1Log, layout.spi1X, layout.spi1Y,
                     layout.spi1W, layout.spi1H, TRUE);
    (void)MoveWindow(g_app.spi2Log, layout.spi2X, layout.spi2Y,
                     layout.spi2W, layout.spi2H, TRUE);
    (void)MoveWindow(g_app.status, layout.statusX, layout.statusY,
                     layout.statusW, layout.statusH, TRUE);
    (void)hwnd;
}

static int LoadJtoolApi(void)
{
#ifdef _WIN32
    char exePath[MAX_PATH];
    char* slash;
    HMODULE dll;

    if (g_DevicesScan != NULL) {
        return 0;
    }
    exePath[0] = '\0';
    (void)GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    slash = strrchr(exePath, '\\');
    if (slash != NULL) {
        slash[1] = '\0';
        (void)strncat(exePath, "jtool.dll", sizeof(exePath) - strlen(exePath) - 1U);
        dll = LoadLibraryA(exePath);
    } else {
        dll = LoadLibraryA("jtool.dll");
    }
    if (dll == NULL) {
        exePath[0] = '\0';
        (void)GetModuleFileNameA(NULL, exePath, sizeof(exePath));
        slash = strrchr(exePath, '\\');
        if (slash != NULL) {
            slash[1] = '\0';
            (void)strncat(exePath, "..\\fpga_jtool_tester\\jtool.dll",
                          sizeof(exePath) - strlen(exePath) - 1U);
            dll = LoadLibraryA(exePath);
        }
    }
    if (dll == NULL) {
        return -1;
    }
    g_DevicesScan = (DevicesScanFn)GetProcAddress(dll, "DevicesScan");
    g_DevOpen = (DevOpenFn)GetProcAddress(dll, "DevOpen");
    g_DevClose = (DevCloseFn)GetProcAddress(dll, "DevClose");
    g_SPIWriteRead = (SPIWriteReadFn)GetProcAddress(dll, "SPIWriteRead");
    g_JSPISetVio = (JSPISetVioFn)GetProcAddress(dll, "JSPISetVio");
    g_JSPISetSpeed = (JSPISetSpeedFn)GetProcAddress(dll, "JSPISetSpeed");
    return ((g_DevicesScan != NULL) && (g_DevOpen != NULL) &&
            (g_DevClose != NULL) && (g_SPIWriteRead != NULL) &&
            (g_JSPISetVio != NULL) && (g_JSPISetSpeed != NULL)) ? 0 : -1;
#else
    return -1;
#endif
}

static void* OpenSpiDevice(int index, UINT logMsg)
{
    void* dev;

    if ((g_DevOpen == NULL) || (g_JSPISetVio == NULL) || (g_JSPISetSpeed == NULL)) {
        PostLog(logMsg, "ERROR jtool.dll SPI API not loaded");
        return NULL;
    }
    if ((index >= 0) && (index < g_app.deviceCount) &&
        (g_app.devices[index].type == DEVICE_ENTRY_SN)) {
        dev = g_DevOpen(dev_spi, g_app.devices[index].sn, 0);
    } else {
        dev = g_DevOpen(dev_spi, NULL, index);
    }
    if (dev == NULL) {
        PostLog(logMsg, "ERROR open SPI device selection=%d failed", index);
        return NULL;
    }
    (void)g_JSPISetVio(dev, 33U);
    (void)g_JSPISetSpeed(dev, 0U);
    return dev;
}

static DWORD WINAPI Spi1MonitorThread(LPVOID param)
{
    int deviceIndex = (int)(intptr_t)param;
    SPICK_TYPE mode = FIXED_SPI1_MODE;
    uint8_t tx[TM_LEN];
    uint8_t page[SPI1_PAGE_LEN];
    uint8_t page0[SPI1_PAGE_LEN];
    uint8_t rx[TM_LEN];
    char formatted[1400];
    void* dev;
    uint32_t frameCount = 0U;
    uint32_t lastLogMs = 0U;
    int havePage0 = 0;

    dev = OpenSpiDevice(deviceIndex, WM_APP_LOG_SPI1);
    if (dev == NULL) {
        InterlockedExchange(&g_app.spi1Run, 0);
        RequestSpiToggleRefresh();
        return 1;
    }
    (void)memset(tx, 0, sizeof(tx));
    PostLog(WM_APP_LOG_SPI1, "SPI1 telemetry monitor started selection=%d %s mode=%d",
            deviceIndex,
            ((deviceIndex >= 0) && (deviceIndex < g_app.deviceCount)) ? g_app.devices[deviceIndex].label : "",
            (int)mode);
    while (InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0) {
        int hasCommand;
        uint32_t txLength = SPI1_PAGE_LEN;
        uint8_t isManualCommand = 0U;
        uint8_t requestSpi2Capture = 0U;

        EnterCriticalSection(&g_app.spi1QueueLock);
        hasCommand = TakeSpi1Command(&g_app.spi1Queue, tx, &txLength,
                                     &isManualCommand, &requestSpi2Capture);
        LeaveCriticalSection(&g_app.spi1QueueLock);
        if (Spi1TelemetryShouldSuspendForSpi2(hasCommand)) {
            Sleep(20);
            continue;
        }
        if ((hasCommand == 0) &&
            (InterlockedCompareExchange(&g_app.spi1TelemetryEnabled, 1, 1) == 0)) {
            havePage0 = 0;
            Sleep(50);
            continue;
        }
        if (SpiWriteReadLocked(dev, mode, ENDIAN_MSB, txLength, tx, page) != ErrNone) {
            PostLog(WM_APP_LOG_SPI1, "ERROR SPI1 SPIWriteRead failed");
            Sleep(100);
            continue;
        }
        if (hasCommand != 0) {
            char hex[(SPI1_PAGE_LEN * 3U) + 1U];
            char rxHeadHex[(16U * 3U) + 1U];
            uint32_t logLength = (isManualCommand != 0U) ? txLength : RC_LEN;

            FormatHex(hex, sizeof(hex), tx, logLength);
            FormatHex(rxHeadHex, sizeof(rxHeadHex), page, 16U);
            if (isManualCommand != 0U) {
                PostLog(WM_APP_LOG_SPI1, "queued manual SPI1 frame sent by SPI1 monitor len=%lu: %s",
                        (unsigned long)txLength, hex);
            } else {
                PostLog(WM_APP_LOG_SPI1, "queued command sent by SPI1 monitor: %s", hex);
            }
            PostLog(WM_APP_LOG_SPI1,
                    "SPI1 command diagnostic txLen=%lu requestSpi2=%u rxHead=%s",
                    (unsigned long)txLength, requestSpi2Capture, rxHeadHex);
            ApplySpi2CaptureStateForCommand(tx, logLength, requestSpi2Capture);
            (void)memset(tx, 0, sizeof(tx));
            havePage0 = 0;
            Sleep(20);
            continue;
        }
        if (havePage0 == 0) {
            if ((page[0] == 0xEBU) && (page[1] == 0x90U)) {
                (void)memcpy(page0, page, SPI1_PAGE_LEN);
                havePage0 = 1;
            }
            continue;
        }
        (void)memcpy(rx, page0, SPI1_PAGE_LEN);
        (void)memcpy(&rx[SPI1_PAGE_LEN], page, TM_LEN - SPI1_PAGE_LEN);
        havePage0 = 0;
        if (ValidateTelemetry(rx) != 0) {
            if ((page[0] == 0xEBU) && (page[1] == 0x90U)) {
                (void)memcpy(page0, page, SPI1_PAGE_LEN);
                havePage0 = 1;
            }
            continue;
        }
        frameCount++;
        if ((frameCount == 1U) || ((TickMs() - lastLogMs) >= 1000U)) {
            FormatTelemetry(rx, formatted, sizeof(formatted));
            PostLog(WM_APP_LOG_SPI1, "TM frame=%lu\r\n%s", (unsigned long)frameCount, formatted);
            lastLogMs = TickMs();
        }
        Sleep(50);
    }
    (void)g_DevClose(dev);
    InterlockedExchange(&g_app.spi1Run, 0);
    PostLog(WM_APP_LOG_SPI1, "SPI1 telemetry monitor stopped");
    RequestSpiToggleRefresh();
    return 0;
}

static DWORD WINAPI Spi2MonitorThread(LPVOID param)
{
    int deviceIndex = (int)(intptr_t)param;
    SPICK_TYPE mode = FIXED_SPI2_MODE;
    uint8_t frame[DT_FRAME_LEN];
    ByteBuffer payload = {0};
    RecordStream records = {0};
    uint32_t frameCount = 0U;
    uint32_t byteCount = 0U;
    uint32_t offset = 0U;
    char error[160];
    void* dev;

    (void)memset(frame, 0, sizeof(frame));
    dev = OpenSpiDevice(deviceIndex, WM_APP_LOG_SPI2);
    if (dev == NULL) {
        ClearSpi2MonitorState();
        RequestSpiToggleRefresh();
        return 1;
    }
    PostLog(WM_APP_LOG_SPI2, "SPI2 monitor armed selection=%d %s mode=%d transferMode=%s delayMs=%d",
            deviceIndex,
            ((deviceIndex >= 0) && (deviceIndex < g_app.deviceCount)) ? g_app.devices[deviceIndex].label : "",
            (int)mode, "byte-paced slave-test", FIXED_SPI2_DELAY_MS);
    while (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) {
        uint32_t idleStart;
        uint32_t captureBytes = 0U;
        uint32_t framesThisCapture = 0U;
        int continuousCapture;
        uint8_t firstBytes[32];
        uint32_t firstByteCount = 0U;
        int sawValidFrame = 0;

        if (InterlockedExchange(&g_app.spi2CaptureRequest, 0) == 0) {
            Sleep((DWORD)SPI2_IDLE_DELAY_MS);
            continue;
        }
        continuousCapture =
            (InterlockedCompareExchange(&g_app.spi2ContinuousCapture, 1, 1) != 0) ? 1 : 0;
        Sleep((DWORD)SPI2_CAPTURE_ARM_DELAY_MS);
        idleStart = TickMs();
        offset = 0U;
        (void)memset(firstBytes, 0, sizeof(firstBytes));
        (void)memset(frame, 0, sizeof(frame));
        PostLog(WM_APP_LOG_SPI2, "SPI2 capture started armDelayMs=%lu idleTimeoutMs=%lu maxFrames=%lu",
                (unsigned long)SPI2_CAPTURE_ARM_DELAY_MS,
                (unsigned long)SPI2_CAPTURE_IDLE_TIMEOUT_MS,
                (unsigned long)SPI2_CAPTURE_MAX_FRAMES);
        payload.length = 0U;
        ResetRecordStream(&records);
        while ((InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) &&
               (Spi2CaptureShouldContinue(framesThisCapture, TickMs() - idleStart,
                                          continuousCapture) != 0)) {
            int frameDone = 0;
            continuousCapture =
                (InterlockedCompareExchange(&g_app.spi2ContinuousCapture, 1, 1) != 0) ? 1 : 0;
        {
            uint8_t txByte = 0U;
            uint8_t rxByte = 0U;
            if (SpiWriteReadLocked(dev, mode, ENDIAN_MSB, 1U, &txByte, &rxByte) != ErrNone) {
                PostLog(WM_APP_LOG_SPI2, "ERROR SPI2 byte read failed byte=%lu", (unsigned long)byteCount);
                Sleep(100);
                continue;
            }
            Sleep((DWORD)Spi2ReadDelayMs(offset));
            byteCount++;
            captureBytes++;
            if (firstByteCount < (uint32_t)sizeof(firstBytes)) {
                firstBytes[firstByteCount++] = rxByte;
            }
            if (Spi2CaptureAcceptByte(frame, &offset, rxByte) == 0) {
                continue;
            }
            offset = 0U;
            if (AppendFramePayload(&payload, frame, error, sizeof(error)) == 0) {
                uint16_t seq = 0U;
                uint16_t frameLength = 0U;
                char firstHex[96];

                (void)ValidateDtFrame(frame, &seq, &frameLength, error, sizeof(error));
                frameCount++;
                framesThisCapture++;
                sawValidFrame = 1;
                idleStart = TickMs();
                FormatHex(firstHex, sizeof(firstHex), &frame[8],
                          (frameLength < 16U) ? frameLength : 16U);
                PostLog(WM_APP_LOG_SPI2,
                        "DT frame=%lu captureFrame=%lu seq=%u wire=%u data=%u bytes=%lu first=%s",
                        (unsigned long)frameCount,
                        (unsigned long)framesThisCapture,
                        seq, DT_FRAME_LEN, frameLength, (unsigned long)byteCount, firstHex);
                {
                    char rawLine[(DT_FRAME_LEN * 3U) + 96U];
                    FormatRawDtFrameLog(rawLine, sizeof(rawLine), framesThisCapture, seq, frame);
                    PostLog(WM_APP_LOG_SPI2, "%s", rawLine);
                }
                (void)DecodeRecordStream(&records, payload.data, payload.length,
                                         WM_APP_LOG_SPI2, framesThisCapture);
                payload.length = 0U;
                if ((continuousCapture == 0) && (frameLength < DT_FRAME_DATA_LEN)) {
                    frameDone = 1;
                }
            } else {
                PostLog(WM_APP_LOG_SPI2, "%s", error);
                if (sawValidFrame != 0) {
                    frameDone = 1;
                }
            }
        }
            if (frameDone != 0) {
                break;
            }
        }
        if (sawValidFrame == 0) {
            char firstHex[128];
            FormatHex(firstHex, sizeof(firstHex), firstBytes, firstByteCount);
            PostLog(WM_APP_LOG_SPI2,
                    "SPI2 capture timeout offset=%lu captureBytes=%lu totalBytes=%lu first=%s",
                    (unsigned long)offset,
                    (unsigned long)captureBytes,
                    (unsigned long)byteCount,
                    firstHex);
        } else {
            PostLog(WM_APP_LOG_SPI2,
                    "SPI2 capture finished frames=%lu captureBytes=%lu totalBytes=%lu pendingPartial=%lu",
                    (unsigned long)framesThisCapture,
                    (unsigned long)captureBytes,
                    (unsigned long)byteCount,
                    (unsigned long)records.pending.length);
        }
        offset = 0U;
        if (continuousCapture == 0) {
            InterlockedExchange(&g_app.spi1SuspendForSpi2, 0);
        }
    }
    (void)g_DevClose(dev);
    free(payload.data);
    free(records.pending.data);
    free(records.pendingFrames.data);
    ClearSpi2MonitorState();
    PostLog(WM_APP_LOG_SPI2, "SPI2 monitor stopped");
    RequestSpiToggleRefresh();
    return 0;
}

static void FillCombo(HWND combo, const char* const* items, int count, int selected)
{
    int i;

    SendMessageA(combo, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < count; i++) {
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)items[i]);
    }
    SendMessageA(combo, CB_SETCURSEL, (WPARAM)selected, 0);
}

static int SelectedComboIndex(HWND combo);

static void FormatComPortName(int number, char* out, size_t outSize)
{
    if ((out == NULL) || (outSize == 0U)) {
        return;
    }
    (void)snprintf(out, outSize, "COM%d", number);
}

static int IsComPortPresent(int number)
{
    char name[16];
    char target[256];

    FormatComPortName(number, name, sizeof(name));
    return (QueryDosDeviceA(name, target, (DWORD)sizeof(target)) != 0) ? 1 : 0;
}

static void PopulateUartPorts(void)
{
    char oldText[32];
    char signature[sizeof(g_app.uartSignature)];
    char ports[64][16];
    int portCount = 0;
    int selected = 0;
    int i;
    size_t used = 0U;

    if (g_app.uartPort == NULL) {
        return;
    }
    GetWindowTextA(g_app.uartPort, oldText, sizeof(oldText));
    (void)snprintf(signature, sizeof(signature), "COM?|");
    for (i = 1; i <= 256; i++) {
        if ((IsComPortPresent(i) != 0) && (portCount < 64)) {
            FormatComPortName(i, ports[portCount], sizeof(ports[portCount]));
            portCount++;
        }
    }
    used = strlen(signature);
    for (i = 0; i < portCount; i++) {
        int written = snprintf(&signature[used], sizeof(signature) - used, "%s|", ports[i]);
        if ((written < 0) || ((size_t)written >= (sizeof(signature) - used))) {
            signature[sizeof(signature) - 1U] = '\0';
            break;
        }
        used += (size_t)written;
    }
    if (strcmp(signature, g_app.uartSignature) == 0) {
        return;
    }
    SendMessageA(g_app.uartPort, CB_RESETCONTENT, 0, 0);
    SendMessageA(g_app.uartPort, CB_ADDSTRING, 0, (LPARAM)"COM?");
    for (i = 0; i < portCount; i++) {
        SendMessageA(g_app.uartPort, CB_ADDSTRING, 0, (LPARAM)ports[i]);
        if (strcmp(oldText, ports[i]) == 0) {
            selected = i + 1;
        }
    }
    if ((selected == 0) && (portCount > 0) &&
        ((oldText[0] == '\0') || (strcmp(oldText, "COM?") == 0))) {
        selected = 1;
    }
    SendMessageA(g_app.uartPort, CB_SETCURSEL, (WPARAM)selected, 0);
    if ((selected == 0) && (oldText[0] != '\0') && (strcmp(oldText, "COM?") != 0)) {
        SetWindowTextA(g_app.uartPort, oldText);
    }
    (void)strncpy(g_app.uartSignature, signature, sizeof(g_app.uartSignature) - 1U);
    g_app.uartSignature[sizeof(g_app.uartSignature) - 1U] = '\0';
}

static void PopulateDevices(void)
{
    int count = 0;
    int parsedCount;
    int listCount;
    int i;
    DeviceAssignment assignment;
    DeviceEntry devices[MAX_SPI_DEVICES];
    char* scanText;
    char* allScanText = NULL;
    int allCount = 0;
    int oldSpi1 = SelectedComboIndex(g_app.spi1Device);
    int oldSpi2 = SelectedComboIndex(g_app.spi2Device);
    char signature[sizeof(g_app.deviceSignature)];
    size_t used = 0U;

    if (LoadJtoolApi() != 0) {
        SetWindowTextA(g_app.status, "jtool.dll load failed");
        return;
    }
    scanText = g_DevicesScan(dev_spi, &count);
    if (count > MAX_SPI_DEVICES) {
        count = MAX_SPI_DEVICES;
    }
    listCount = EffectiveDeviceListCount(count);
    if (listCount > MAX_SPI_DEVICES) {
        listCount = MAX_SPI_DEVICES;
    }
    (void)memset(devices, 0, sizeof(devices));
    parsedCount = ParseScannedDeviceEntries(scanText, count, devices, listCount);
    if (parsedCount < 2) {
        DeviceEntry allDevices[MAX_SPI_DEVICES];
        int allParsed;

        allScanText = g_DevicesScan(dev_all, &allCount);
        (void)memset(allDevices, 0, sizeof(allDevices));
        allParsed = ParseScannedDeviceEntries(allScanText, 0, allDevices, listCount);
        if (allParsed > parsedCount) {
            (void)memcpy(devices, allDevices, sizeof(devices));
            parsedCount = allParsed;
        }
    }
    for (i = parsedCount; i < listCount; i++) {
        (void)memset(&devices[i], 0, sizeof(devices[i]));
        devices[i].type = DEVICE_ENTRY_MANUAL;
        devices[i].id = i;
        (void)snprintf(devices[i].label, sizeof(devices[i].label),
                       "Manual ID #%d", i);
    }

    signature[0] = '\0';
    for (i = 0; i < listCount; i++) {
        int written = snprintf(&signature[used], sizeof(signature) - used,
                               "%s|", devices[i].label);
        if ((written < 0) || ((size_t)written >= (sizeof(signature) - used))) {
            signature[sizeof(signature) - 1U] = '\0';
            break;
        }
        used += (size_t)written;
    }
    if (strcmp(signature, g_app.deviceSignature) == 0) {
        return;
    }

    SendMessageA(g_app.spi1Device, CB_RESETCONTENT, 0, 0);
    SendMessageA(g_app.spi2Device, CB_RESETCONTENT, 0, 0);
    (void)memcpy(g_app.devices, devices, sizeof(g_app.devices));
    g_app.deviceCount = listCount;
    for (i = 0; i < listCount; i++) {
        SendMessageA(g_app.spi1Device, CB_ADDSTRING, 0, (LPARAM)g_app.devices[i].label);
        SendMessageA(g_app.spi2Device, CB_ADDSTRING, 0, (LPARAM)g_app.devices[i].label);
    }
    assignment = DefaultAssignment(listCount);
    if (assignment.spi1Index >= 0) {
        int spi1Select = ((oldSpi1 >= 0) && (oldSpi1 < listCount)) ? oldSpi1 : assignment.spi1Index;
        int spi2Select = ((oldSpi2 >= 0) && (oldSpi2 < listCount)) ? oldSpi2 : assignment.spi2Index;
        SendMessageA(g_app.spi1Device, CB_SETCURSEL, (WPARAM)spi1Select, 0);
        SendMessageA(g_app.spi2Device, CB_SETCURSEL, (WPARAM)spi2Select, 0);
    }
    SetWindowTextA(g_app.status, (count > 0) ? "devices scanned; manual IDs are available" :
                   "no scanned SPI USB device; manual IDs are available");
    (void)strncpy(g_app.deviceSignature, signature, sizeof(g_app.deviceSignature) - 1U);
    g_app.deviceSignature[sizeof(g_app.deviceSignature) - 1U] = '\0';
    PostLog(WM_APP_LOG_SPI1,
            "Scan found %d SPI device(s), parsed %d SN entries, showing %d selectable entry(s)",
            count, parsedCount, listCount);
    if ((scanText != NULL) && (scanText[0] != '\0')) {
        PostLog(WM_APP_LOG_SPI1, "Raw DevicesScan: %s", scanText);
    }
    if ((allScanText != NULL) && (allScanText[0] != '\0')) {
        PostLog(WM_APP_LOG_SPI1, "Raw DevicesScan(all): %s", allScanText);
    }
}

static int SelectedComboIndex(HWND combo)
{
    LRESULT value = SendMessageA(combo, CB_GETCURSEL, 0, 0);
    return (value == CB_ERR) ? -1 : (int)value;
}

static void PauseSpi1TelemetryRxForCommand(uint8_t selectedCommand,
                                           uint8_t requestSpi2Capture)
{
    if ((selectedCommand != 0x09U) || (requestSpi2Capture == 0U) ||
        (InterlockedCompareExchange(&g_app.spi1TelemetryEnabled, 1, 1) == 0)) {
        return;
    }

    InterlockedExchange(&g_app.spi1TelemetryEnabled, 0);
    UpdateSpiToggleButtons();
    PostLog(WM_APP_LOG_SPI1, "auto-paused SPI1 TM RX before CMD_09 send");
    Sleep((DWORD)SPI1_CMD_AFTER_TM_PAUSE_MS);
}

static void SendSpi1Command(void)
{
    int deviceIndex = SelectedComboIndex(g_app.spi1Device);
    int commandIndex = SelectedComboIndex(g_app.cmdSelect);
    char paramsText[768];
    uint8_t tx[TM_LEN];
    uint8_t rx[SPI1_PAGE_LEN];
    char hex[(SPI1_PAGE_LEN * 3U) + 1U];
    char commandLabel[32];
    char error[160];
    uint8_t selectedCommand;
    uint8_t isManualCommand;
    uint8_t requestSpi2Capture;
    uint32_t txLength;
    uint32_t logLength;
    uint8_t repeat;
    uint8_t repeatIndex;
    void* dev;

    if (deviceIndex < 0) {
        PostLog(WM_APP_LOG_SPI1, "ERROR select SPI1 device first");
        return;
    }
    if (commandIndex < 0) {
        PostLog(WM_APP_LOG_SPI1, "ERROR select command first");
        return;
    }
    GetWindowTextA(g_app.cmdParams, paramsText, sizeof(paramsText));
    selectedCommand = CommandFromComboIndex(commandIndex);
    isManualCommand = (selectedCommand == GUI_COMMAND_MANUAL_RAW) ? 1U : 0U;
    if (BuildGuiCommandFromText(selectedCommand, paramsText, tx, &txLength,
                                &requestSpi2Capture, error, sizeof(error)) != 0) {
        PostLog(WM_APP_LOG_SPI1, "ERROR params: %s", error);
        return;
    }
    logLength = (isManualCommand != 0U) ? txLength : RC_LEN;
    FormatHex(hex, sizeof(hex), tx, logLength);
    FormatCommandLogLabel(selectedCommand, tx, commandLabel, sizeof(commandLabel));
    repeat = CommandRepeatCount((isManualCommand == 0U) ? tx[2] : 0U);
    PauseSpi1TelemetryRxForCommand(selectedCommand, requestSpi2Capture);
    if (InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0) {
        EnterCriticalSection(&g_app.spi1QueueLock);
        (void)QueueSpi1Command(&g_app.spi1Queue, tx, txLength,
                               isManualCommand, requestSpi2Capture);
        LeaveCriticalSection(&g_app.spi1QueueLock);
        if (isManualCommand != 0U) {
            PostLog(WM_APP_LOG_SPI1, "queued %s len=%lu for SPI1 monitor repeat=%u: %s",
                    commandLabel, (unsigned long)txLength, repeat, hex);
        } else {
            PostLog(WM_APP_LOG_SPI1, "queued %s for SPI1 monitor repeat=%u: %s",
                    commandLabel, repeat, hex);
        }
        return;
    }
    dev = OpenSpiDevice(deviceIndex, WM_APP_LOG_SPI1);
    if (dev == NULL) {
        return;
    }
    for (repeatIndex = 0U; repeatIndex < repeat; repeatIndex++) {
        if (isManualCommand != 0U) {
            PostLog(WM_APP_LOG_SPI1, "TX %s len=%lu repeat=%u/%u: %s",
                    commandLabel, (unsigned long)txLength, repeatIndex + 1U, repeat, hex);
        } else {
            PostLog(WM_APP_LOG_SPI1, "TX %s repeat=%u/%u: %s",
                    commandLabel, repeatIndex + 1U, repeat, hex);
        }
        if (SpiWriteReadLocked(dev, FIXED_SPI1_MODE, ENDIAN_MSB, txLength, tx, rx) != ErrNone) {
            PostLog(WM_APP_LOG_SPI1, "ERROR command SPIWriteRead failed repeat=%u/%u",
                    repeatIndex + 1U, repeat);
        } else {
            char rxHeadHex[(16U * 3U) + 1U];
            FormatHex(rxHeadHex, sizeof(rxHeadHex), rx, 16U);
            PostLog(WM_APP_LOG_SPI1, "command sent repeat=%u/%u", repeatIndex + 1U, repeat);
            PostLog(WM_APP_LOG_SPI1,
                    "SPI1 direct command diagnostic txLen=%lu requestSpi2=%u rxHead=%s",
                    (unsigned long)txLength, requestSpi2Capture, rxHeadHex);
        }
        Sleep(20);
    }
    (void)g_DevClose(dev);
    ApplySpi2CaptureStateForCommand(tx, logLength, requestSpi2Capture);
}

static void StartSpi1Monitor(void)
{
    int deviceIndex = SelectedComboIndex(g_app.spi1Device);

    if (deviceIndex < 0) {
        PostLog(WM_APP_LOG_SPI1, "ERROR select SPI1 device first");
        return;
    }
    if (InterlockedCompareExchange(&g_app.spi1Run, 1, 0) != 0) {
        return;
    }
    g_app.spi1Thread = CreateThread(NULL, 0, Spi1MonitorThread, (LPVOID)(intptr_t)deviceIndex, 0, NULL);
    if (g_app.spi1Thread == NULL) {
        InterlockedExchange(&g_app.spi1Run, 0);
        PostLog(WM_APP_LOG_SPI1, "ERROR create SPI1 monitor thread failed");
    }
    UpdateSpiToggleButtons();
}

static void StartSpi2Monitor(void)
{
    int deviceIndex = SelectedComboIndex(g_app.spi2Device);

    if (deviceIndex < 0) {
        PostLog(WM_APP_LOG_SPI2, "ERROR select SPI2 device first");
        return;
    }
    if (InterlockedCompareExchange(&g_app.spi2Run, 1, 0) != 0) {
        return;
    }
    g_app.spi2Thread = CreateThread(NULL, 0, Spi2MonitorThread, (LPVOID)(intptr_t)deviceIndex, 0, NULL);
    if (g_app.spi2Thread == NULL) {
        InterlockedExchange(&g_app.spi2Run, 0);
        PostLog(WM_APP_LOG_SPI2, "ERROR create SPI2 monitor thread failed");
    }
    UpdateSpiToggleButtons();
}

static void ToggleSpi1Monitor(void)
{
    if (InterlockedCompareExchange(&g_app.spi1Run, 1, 1) != 0) {
        InterlockedExchange(&g_app.spi1Run, 0);
        UpdateSpiToggleButtons();
    } else {
        StartSpi1Monitor();
    }
}

static void ToggleSpi2Monitor(void)
{
    if (InterlockedCompareExchange(&g_app.spi2Run, 1, 1) != 0) {
        InterlockedExchange(&g_app.spi2Run, 0);
        InterlockedExchange(&g_app.spi2CaptureRequest, 0);
        InterlockedExchange(&g_app.spi2ContinuousCapture, 0);
        InterlockedExchange(&g_app.spi1SuspendForSpi2, 0);
        UpdateSpiToggleButtons();
    } else {
        StartSpi2Monitor();
    }
}

static void ToggleTelemetryRx(void)
{
    LONG enabled = InterlockedCompareExchange(&g_app.spi1TelemetryEnabled, 1, 1);
    InterlockedExchange(&g_app.spi1TelemetryEnabled, (enabled != 0) ? 0 : 1);
    UpdateSpiToggleButtons();
}

static void UpdateCommandHint(void)
{
    int commandIndex = SelectedComboIndex(g_app.cmdSelect);

    if ((commandIndex >= 0) && (g_app.cmdHint != NULL)) {
        SetWindowTextA(g_app.cmdHint, GetCommandHint(CommandFromComboIndex(commandIndex)));
    }
}

static HWND AddControl(HWND parent, const char* cls, const char* text, DWORD style,
                       int x, int y, int w, int h, int id)
{
    return CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(intptr_t)id,
                           GetModuleHandleA(NULL), NULL);
}

static void CreateMainControls(HWND hwnd)
{
    static const char* const cmds[] = {
        "CMD_01 work mode", "CMD_02 rate", "CMD_03 slot config", "CMD_04 TX power",
        "CMD_05 frequency Hz bytes", "CMD_06 RF mask", "CMD_07 reset",
        "CMD_08 upgrade", "CMD_09 data transfer", "CMD_0A write register",
        "CMD_0B read register", "CMD_0C rollback", "CMD_0D UTC seconds", "CMD_0E DC params",
        "Manual raw SPI1 frame"};
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HWND child;

    AddControl(hwnd, "STATIC", "SPI1 device", 0, 10, 10, 80, 20, 0);
    g_app.spi1Device = AddControl(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 90, 8, 120, 120, IDC_SPI1_DEVICE);
    AddControl(hwnd, "STATIC", "SPI2 device", 0, 220, 10, 80, 20, 0);
    g_app.spi2Device = AddControl(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 300, 8, 120, 120, IDC_SPI2_DEVICE);
    child = AddControl(hwnd, "BUTTON", "Scan/Auto", BS_PUSHBUTTON, 430, 7, 90, 24, IDC_SCAN);
    SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    AddControl(hwnd, "STATIC", "UART", 0, 535, 10, 45, 20, 0);
    g_app.uartPort = AddControl(hwnd, "COMBOBOX", "", CBS_DROPDOWN | WS_VSCROLL,
                                 580, 8, 70, 160, IDC_UART_PORT);
    AddControl(hwnd, "STATIC", "baud", 0, 660, 10, 45, 20, 0);
    g_app.uartBaud = AddControl(hwnd, "EDIT", "1000000", WS_BORDER | ES_AUTOHSCROLL,
                                705, 8, 80, 24, IDC_UART_BAUD);
    g_app.reportRun = AddControl(hwnd, "BUTTON", ReportRunButtonText(0),
                                  BS_PUSHBUTTON, 800, 7, 135, 24, IDC_REPORT_RUN);
    SendMessageA(g_app.reportRun, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.longRun = AddControl(hwnd, "BUTTON", LongRunButtonText(0),
                                 BS_PUSHBUTTON, 945, 7, 125, 24, IDC_LONG_RUN);
    SendMessageA(g_app.longRun, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.reportProgress = CreateWindowExA(0, PROGRESS_CLASSA, "",
                                           WS_CHILD | WS_VISIBLE,
                                           1080, 8, 120, 24, hwnd,
                                           (HMENU)(intptr_t)IDC_REPORT_PROGRESS,
                                           GetModuleHandleA(NULL), NULL);
    SendMessageA(g_app.reportProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(g_app.reportProgress, PBM_SETPOS, 0, 0);
    g_app.spi1Toggle = AddControl(hwnd, "BUTTON", Spi1ToggleButtonText(0),
                                  BS_PUSHBUTTON, 10, 40, 105, 26, IDC_SPI1_TOGGLE);
    SendMessageA(g_app.spi1Toggle, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.tmRxToggle = AddControl(hwnd, "BUTTON", TelemetryRxToggleButtonText(1),
                                  BS_PUSHBUTTON, 120, 40, 105, 26, IDC_TM_RX_TOGGLE);
    SendMessageA(g_app.tmRxToggle, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.spi2Toggle = AddControl(hwnd, "BUTTON", Spi2ToggleButtonText(0),
                                  BS_PUSHBUTTON, 235, 40, 105, 26, IDC_SPI2_TOGGLE);
    SendMessageA(g_app.spi2Toggle, WM_SETFONT, (WPARAM)font, TRUE);

    AddControl(hwnd, "STATIC", "SPI1 command", 0, 430, 44, 90, 20, 0);
    g_app.cmdSelect = AddControl(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 520, 40, 180, 180, IDC_CMD_SELECT);
    AddControl(hwnd, "STATIC", "Params", 0, 710, 44, 50, 20, 0);
    g_app.cmdParams = AddControl(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL,
                                 760, 40, CMD_PARAMS_WIDTH, 24, IDC_CMD_PARAMS);
    child = AddControl(hwnd, "BUTTON", "Send", BS_PUSHBUTTON, CMD_SEND_X, 39, 60, 26, IDC_CMD_SEND);
    SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.cmdHint = AddControl(hwnd, "STATIC", "", 0, 430, 66, 630, 18, 0);

    AddControl(hwnd, "STATIC", "SPI1 telemetry / command log", 0, 10, LOG_HEADER_Y, 240, 20, 0);
    child = AddControl(hwnd, "BUTTON", "Clear SPI1", BS_PUSHBUTTON, 255, LOG_BUTTON_Y, 80, 24, IDC_SPI1_CLEAR);
    SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    AddControl(hwnd, "STATIC", "SPI2 data-transfer log", 0, 545, LOG_HEADER_Y, 220, 20, 0);
    child = AddControl(hwnd, "BUTTON", "Clear SPI2", BS_PUSHBUTTON, 770, LOG_BUTTON_Y, 80, 24, IDC_SPI2_CLEAR);
    SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    g_app.spi1Log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                    ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                    10, LOG_EDIT_TOP, 520, 500, hwnd, (HMENU)(intptr_t)IDC_SPI1_LOG,
                                    GetModuleHandleA(NULL), NULL);
    g_app.spi2Log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                    ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                    545, LOG_EDIT_TOP, 520, 500, hwnd, (HMENU)(intptr_t)IDC_SPI2_LOG,
                                    GetModuleHandleA(NULL), NULL);
    g_app.status = AddControl(hwnd, "STATIC", "ready", WS_BORDER, 10, 610, 1055, 24, IDC_STATUS);
    SendMessageA(g_app.spi1Log, EM_SETLIMITTEXT, (WPARAM)LOG_EDIT_TEXT_LIMIT, 0);
    SendMessageA(g_app.spi2Log, EM_SETLIMITTEXT, (WPARAM)LOG_EDIT_TEXT_LIMIT, 0);

    FillCombo(g_app.cmdSelect, cmds, 15, 8);
    SendMessageA(g_app.spi1Device, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.spi2Device, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.uartPort, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.uartBaud, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.cmdSelect, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.cmdParams, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.cmdHint, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.spi1Log, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.spi2Log, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)font, TRUE);
    PopulateUartPorts();
    PopulateDevices();
    UpdateCommandHint();
    UpdateSpiToggleButtons();
    ApplyMainLayout(hwnd, 1220, 650);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE:
            g_app.hwnd = hwnd;
            InterlockedExchange(&g_app.spi1TelemetryEnabled, 1);
            if (g_app.spi1QueueLockReady == 0) {
                InitializeCriticalSection(&g_app.spi1QueueLock);
                g_app.spi1QueueLockReady = 1;
            }
            if (g_app.spiIoLockReady == 0) {
                InitializeCriticalSection(&g_app.spiIoLock);
                g_app.spiIoLockReady = 1;
            }
            CreateMainControls(hwnd);
            SetTimer(hwnd, IDT_DEVICE_SCAN, 2000U, NULL);
            return 0;
        case WM_SIZE:
            if (g_app.spi1Log != NULL) {
                ApplyMainLayout(hwnd, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_COMMAND:
            if ((LOWORD(wParam) == IDC_CMD_SELECT) && (HIWORD(wParam) == CBN_SELCHANGE)) {
                UpdateCommandHint();
                return 0;
            }
            switch (LOWORD(wParam)) {
                case IDC_SCAN:
                    PopulateDevices();
                    return 0;
                case IDC_SPI1_TOGGLE:
                    ToggleSpi1Monitor();
                    return 0;
                case IDC_TM_RX_TOGGLE:
                    ToggleTelemetryRx();
                    return 0;
                case IDC_SPI2_TOGGLE:
                    ToggleSpi2Monitor();
                    return 0;
                case IDC_CMD_SEND:
                    SendSpi1Command();
                    return 0;
                case IDC_REPORT_RUN:
                    StartReportTests();
                    return 0;
                case IDC_LONG_RUN:
                    ToggleLongRun();
                    return 0;
                case IDC_SPI1_CLEAR:
                    SetWindowTextA(g_app.spi1Log, "");
                    return 0;
                case IDC_SPI2_CLEAR:
                    SetWindowTextA(g_app.spi2Log, "");
                    return 0;
                default:
                    break;
            }
            break;
        case WM_TIMER:
            if (wParam == IDT_DEVICE_SCAN) {
                PopulateUartPorts();
                PopulateDevices();
                return 0;
            }
            break;
        case WM_DEVICECHANGE:
            PopulateUartPorts();
            PopulateDevices();
            return 0;
        case WM_APP_LOG_SPI1:
            WriteDailyLogFile("spi1", (const char*)lParam);
            AppendLogText(g_app.spi1Log, (const char*)lParam);
            free((void*)lParam);
            return 0;
        case WM_APP_LOG_SPI2:
            WriteDailyLogFile("spi2", (const char*)lParam);
            AppendLogText(g_app.spi2Log, (const char*)lParam);
            free((void*)lParam);
            return 0;
        case WM_APP_STATUS:
            SetWindowTextA(g_app.status, (const char*)lParam);
            free((void*)lParam);
            return 0;
        case WM_APP_REFRESH_TOGGLES:
            UpdateSpiToggleButtons();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, IDT_DEVICE_SCAN);
            InterlockedExchange(&g_app.spi1Run, 0);
            InterlockedExchange(&g_app.spi2Run, 0);
            InterlockedExchange(&g_app.spi2ContinuousCapture, 0);
            InterlockedExchange(&g_app.spi1SuspendForSpi2, 0);
            InterlockedExchange(&g_app.longRunStop, 1);
            if (g_app.spi1Thread != NULL) {
                (void)WaitForSingleObject(g_app.spi1Thread, 1000U);
                CloseHandle(g_app.spi1Thread);
                g_app.spi1Thread = NULL;
            }
            if (g_app.spi2Thread != NULL) {
                (void)WaitForSingleObject(g_app.spi2Thread, 1000U);
                CloseHandle(g_app.spi2Thread);
                g_app.spi2Thread = NULL;
            }
            if (g_app.reportThread != NULL) {
                (void)WaitForSingleObject(g_app.reportThread, 1000U);
                CloseHandle(g_app.reportThread);
                g_app.reportThread = NULL;
            }
            if (g_app.longRunThread != NULL) {
                (void)WaitForSingleObject(g_app.longRunThread, 1000U);
                CloseHandle(g_app.longRunThread);
                g_app.longRunThread = NULL;
            }
            if (g_app.spi1QueueLockReady != 0) {
                DeleteCriticalSection(&g_app.spi1QueueLock);
                g_app.spi1QueueLockReady = 0;
            }
            if (g_app.spiIoLockReady != 0) {
                DeleteCriticalSection(&g_app.spiIoLock);
                g_app.spiIoLockReady = 0;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int RunGui(void)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    InitCommonControls();
    (void)memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "CombinedSpiGuiTesterWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (RegisterClassA(&wc) == 0) {
        MessageBoxA(NULL, "RegisterClass failed", APP_TITLE, MB_ICONERROR);
        return 1;
    }
    hwnd = CreateWindowExA(0, wc.lpszClassName, APP_TITLE,
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1220, 690,
                           NULL, NULL, wc.hInstance, NULL);
    if (hwnd == NULL) {
        MessageBoxA(NULL, "CreateWindow failed", APP_TITLE, MB_ICONERROR);
        return 1;
    }
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

static int RunSelftest(void)
{
    static const uint8_t expectedCmd09[RC_LEN] = {
        0x76U, 0x25U, 0x09U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0AU};
    static const uint8_t payload[] = {0x1AU, 0x2BU, 0x3CU};
    uint8_t tx[TM_LEN];
    uint8_t params[2] = {0x12U, 0x34U};
    uint8_t parsed[6];
    uint32_t parsedLen = 0U;
    char errorText[128];
    char logPath[128];
    char timestamped[256];
    uint8_t frame[DT_FRAME_LEN];
    ByteBuffer records = {0};
    ByteBuffer framed = {0};
    ByteBuffer splitRecords = {0};
    ByteBuffer splitFramed = {0};
    RecordStream splitStream = {0};
    DeviceEntry entries[4];
    Spi1CommandQueue queue = {0};
    uint8_t queuedTx[TM_LEN];
    uint32_t queuedLength = 0U;
    uint8_t queuedIsManual = 0U;
    uint8_t queuedRequestCapture = 0U;
    uint32_t txLength = 0U;
    uint8_t requestCapture = 0U;
    uint32_t spi2Offset;
    uint8_t largePayload[512];
    uint32_t payloadIndex;
    char manual128Text[(SPI1_PAGE_LEN * 3U) + 1U];
    char manual129Text[((SPI1_PAGE_LEN + 1U) * 3U) + 1U];
    size_t manualTextUsed;
    uint32_t manualIndex;
    DeviceAssignment one;
    DeviceAssignment two;
    MainLayout largeLayout;
    char error[128];
    char formatted[256];
    char rawLine[(DT_FRAME_LEN * 3U) + 96U];
    uint8_t tm[TM_LEN];
    ReportTable reportTable;
    ReportRow reportRow;
    ReportRunConfig reportConfig;
    const ReportRow* foundRow;
    char markdownRow[560];
    char longAt[180];
    const char* tc12Expected = "RC OK CMD_0C rollback flagWrite=2 flagRead=2 noReset";
    uint32_t reportIndex;
    FILE* selfReport;
    long selfReportSize;
    char* selfReportText;
    wchar_t* wideText;
    static const char* const expectedReportIds[] = {
        "RC-ERR-01", "RC-ERR-02", "RC-ERR-03", "RC-ERR-04", "RC-ERR-05",
        "TC-01", "TC-02", "TC-03", "TC-04", "TC-05", "TC-06", "TC-07",
        "TC-08", "TC-09", "TC-10", "TC-11", "TC-12", "TC-13", "TC-14",
        "TM-01",
        "DT-01", "DT-02", "DT-03", "DT-04",
        "ER-01", "ER-02", "ER-03", "ER-04", "ER-05", "ER-06", "ER-07",
        "RF-01", "RF-02", "RF-03", "RF-04", "RF-05", "RF-06", "RF-07",
        "RF-08", "RF-09", "RF-10", "RF-11", "RF-12", "RF-13",
        "FL-01", "FL-02", "FL-03", "FL-04", "FL-05", "FL-06", "FL-07",
        "FL-08", "FL-09", "FL-10", "FL-11", "FL-12"
    };
    static const char* const autoReportIds[] = {
        "RC-ERR-01", "RC-ERR-02", "RC-ERR-03", "RC-ERR-04", "RC-ERR-05",
        "TC-12", "TC-13", "DT-01", "DT-02", "DT-03", "DT-04",
        "ER-04", "ER-05", "ER-06", "FL-01", "FL-02", "FL-03",
        "ER-07", "FL-04", "FL-05", "FL-06", "FL-07", "FL-08", "FL-11", "FL-12"
    };

    if (DEFAULT_UART_BAUD != 1000000U) {
        printf("SELFTEST FAIL default UART baud\n");
        return 1;
    }
    if ((strcmp(ReportMissingAutomatedResourceResult(0), "SKIP") != 0) ||
        (strcmp(ReportMissingAutomatedResourceResult(1), "FAILED") != 0)) {
        printf("SELFTEST FAIL automated resource missing result policy\n");
        return 1;
    }
    if ((strcmp(LongRunButtonText(0), "ER-07/FL-12 Loop") != 0) ||
        (strcmp(LongRunButtonText(1), "Stop ER-07/FL-12") != 0)) {
        printf("SELFTEST FAIL long-run button text\n");
        return 1;
    }
    FormatComPortName(9, formatted, sizeof(formatted));
    if (strcmp(formatted, "COM9") != 0) {
        printf("SELFTEST FAIL COM port format\n");
        return 1;
    }
    FormatComPortName(128, formatted, sizeof(formatted));
    if (strcmp(formatted, "COM128") != 0) {
        printf("SELFTEST FAIL high COM port format\n");
        return 1;
    }
    if ((ReportProgressPercent(1U, 50U) != 2U) ||
        (ReportProgressPercent(49U, 50U) != 98U) ||
        (ReportProgressPercent(50U, 50U) != 100U)) {
        printf("SELFTEST FAIL report progress percent\n");
        return 1;
    }
    if ((ReportLogTargetForId("TC-01") != WM_APP_LOG_SPI1) ||
        (ReportLogTargetForId("TM-01") != WM_APP_LOG_SPI1) ||
        (ReportLogTargetForId("DT-01") != WM_APP_LOG_SPI2) ||
        (ReportLogTargetForId("TC-09") != WM_APP_LOG_SPI2)) {
        printf("SELFTEST FAIL report log routing\n");
        return 1;
    }
    if (strcmp(tc12Expected, "RC OK CMD_0C rollback flagWrite=2 flagRead=2 noReset") != 0) {
        printf("SELFTEST FAIL TC-12 rollback expected log\n");
        return 1;
    }
    (void)memset(longAt, 'A', sizeof(longAt));
    longAt[0] = 'A';
    longAt[1] = 'T';
    longAt[2] = '+';
    longAt[sizeof(longAt) - 1U] = '\0';
    FormatAtCommandSummary(longAt, formatted, sizeof(formatted));
    if ((strlen(formatted) >= strlen(longAt)) || (strstr(formatted, "len=") == NULL)) {
        printf("SELFTEST FAIL long AT summary\n");
        return 1;
    }
    wideText = Utf8ToWideAlloc("\xE9\x81\xA5\xE6\x8E\xA7");
    if ((wideText == NULL) || (wcscmp(wideText, L"遥控") != 0)) {
        free(wideText);
        printf("SELFTEST FAIL UTF-8 GUI text conversion\n");
        return 1;
    }
    free(wideText);

    BuildDataTransferCommand(tx, 1U);
    if (memcmp(tx, expectedCmd09, sizeof(expectedCmd09)) != 0) {
        printf("SELFTEST FAIL CMD09 bytes\n");
        return 1;
    }
    if (BuildGuiCommand(0x02U, params, 1U, tx) != 0 || tx[3] != 0x12U || tx[9] != 0x14U) {
        printf("SELFTEST FAIL generic command\n");
        return 1;
    }
    if ((QueueSpi1Command(&queue, tx, SPI1_PAGE_LEN, 0U, 0U) != 0) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 1) ||
        (queuedLength != SPI1_PAGE_LEN) || (queuedIsManual != 0U) ||
        (queuedRequestCapture != 0U) || (memcmp(tx, queuedTx, TM_LEN) != 0) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 0)) {
        printf("SELFTEST FAIL SPI1 command queue\n");
        return 1;
    }
    (void)memset(queuedTx, 0xA5, sizeof(queuedTx));
    if ((TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 0) ||
        (queuedLength != SPI1_PAGE_LEN) || (queuedIsManual != 0U) ||
        (queuedRequestCapture != 0U) || (queuedTx[0] != 0U) ||
        (queuedTx[1] != 0U) || (queuedTx[2] != 0U)) {
        printf("SELFTEST FAIL SPI1 idle tx clear\n");
        return 1;
    }
    BuildDataTransferCommand(tx, 1U);
    (void)memset(&queue, 0, sizeof(queue));
    if ((CommandRepeatCount(0x09U) != 1U) ||
        (QueueSpi1Command(&queue, tx, SPI1_PAGE_LEN, 0U, 1U) != 0) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 1) ||
        (queuedLength != SPI1_PAGE_LEN) || (queuedIsManual != 0U) ||
        (queuedRequestCapture != 1U) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 0)) {
        printf("SELFTEST FAIL CMD09 repeat queue\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x05U, "473200000", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != SPI1_PAGE_LEN) || (requestCapture != 0U) ||
        (tx[3] != 0x1CU) || (tx[4] != 0x34U) || (tx[5] != 0x75U) || (tx[6] != 0x80U)) {
        printf("SELFTEST FAIL CMD05 decimal packing\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x02U, "08", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != SPI1_PAGE_LEN) || (requestCapture != 0U) ||
        (tx[3] != 0x08U)) {
        printf("SELFTEST FAIL leading-zero decimal packing\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x09U, "1", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != SPI1_PAGE_LEN) || (requestCapture != 1U) ||
        (memcmp(tx, expectedCmd09, sizeof(expectedCmd09)) != 0)) {
        printf("SELFTEST FAIL CMD09 on packing\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x09U, "0", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != SPI1_PAGE_LEN) || (requestCapture != 0U) ||
        (tx[3] != 0U) || (tx[9] != 0x09U)) {
        printf("SELFTEST FAIL CMD09 off packing\n");
        return 1;
    }
    if ((GetDataTransferCommandSwitch(tx, RC_LEN, &requestCapture) == 0) ||
        (requestCapture != 0U)) {
        printf("SELFTEST FAIL CMD09 off switch detection\n");
        return 1;
    }
    BuildDataTransferCommand(tx, 1U);
    if ((GetDataTransferCommandSwitch(tx, RC_LEN, &requestCapture) == 0) ||
        (requestCapture != 1U)) {
        printf("SELFTEST FAIL CMD09 on switch detection\n");
        return 1;
    }
    ClearSpi2MonitorState();
    InterlockedExchange(&g_app.spi2Run, 1);
    ApplySpi2CaptureStateForCommand(tx, RC_LEN, 1U);
    if ((InterlockedCompareExchange(&g_app.spi2ContinuousCapture, 0, 0) != 1) ||
        (InterlockedCompareExchange(&g_app.spi2CaptureRequest, 0, 0) != 1) ||
        (InterlockedCompareExchange(&g_app.spi1SuspendForSpi2, 0, 0) != 1) ||
        (Spi1TelemetryShouldSuspendForSpi2(0) == 0) ||
        (Spi1TelemetryShouldSuspendForSpi2(1) != 0)) {
        printf("SELFTEST FAIL CMD09 on continuous capture state\n");
        return 1;
    }
    BuildDataTransferCommand(tx, 0U);
    ApplySpi2CaptureStateForCommand(tx, RC_LEN, 0U);
    if ((InterlockedCompareExchange(&g_app.spi2ContinuousCapture, 0, 0) != 0) ||
        (InterlockedCompareExchange(&g_app.spi2CaptureRequest, 0, 0) != 0) ||
        (InterlockedCompareExchange(&g_app.spi1SuspendForSpi2, 0, 0) != 0)) {
        printf("SELFTEST FAIL CMD09 off continuous capture state\n");
        return 1;
    }
    ClearSpi2MonitorState();
    if (BuildGuiCommandFromText(0x09U, "2", tx, &txLength, &requestCapture,
                                 errorText, sizeof(errorText)) == 0) {
        printf("SELFTEST FAIL CMD09 invalid switch accepted\n");
        return 1;
    }
    if ((strstr(GetCommandHint(0x01U), "0..6") == NULL) ||
        (strstr(GetCommandHint(0x02U), "0..2") == NULL) ||
        (strstr(GetCommandHint(0x02U), "6/7/8") == NULL)) {
        printf("SELFTEST FAIL CMD01/CMD02 range hints\n");
        return 1;
    }
    if ((strlen(GetCommandHint(0x01U)) > CMD_HINT_MAX_VISIBLE_CHARS) ||
        (strlen(GetCommandHint(0x02U)) > CMD_HINT_MAX_VISIBLE_CHARS) ||
        (strlen(GetCommandHint(GUI_COMMAND_MANUAL_RAW)) > CMD_HINT_MAX_VISIBLE_CHARS)) {
        printf("SELFTEST FAIL command hint visible length\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x01U, "7", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (tx[3] != 7U)) {
        printf("SELFTEST FAIL CMD01 out-of-range GUI passthrough\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(0x02U, "3", tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (tx[3] != 3U)) {
        printf("SELFTEST FAIL CMD02 out-of-range GUI passthrough\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, "76 25 09",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != 3U) || (requestCapture != 0U) ||
        (tx[0] != 0x76U) || (tx[1] != 0x25U) || (tx[2] != 0x09U)) {
        printf("SELFTEST FAIL manual short raw frame packing\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, "AA",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != 1U) || (requestCapture != 0U) || (tx[0] != 0xAAU)) {
        printf("SELFTEST FAIL manual one-byte raw frame packing\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW,
                                  "76 25 09 01 00 00 00 00 00 0A",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != RC_LEN) || (requestCapture != 1U) ||
        (memcmp(tx, expectedCmd09, sizeof(expectedCmd09)) != 0)) {
        printf("SELFTEST FAIL manual 10-byte command packing\n");
        return 1;
    }
    (void)memset(&queue, 0, sizeof(queue));
    if ((QueueSpi1Command(&queue, tx, txLength, 1U, requestCapture) != 0) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 1) ||
        (queuedLength != RC_LEN) || (queuedIsManual != 1U) ||
        (queuedRequestCapture != 1U) ||
        (memcmp(queuedTx, expectedCmd09, RC_LEN) != 0)) {
        printf("SELFTEST FAIL manual CMD09 queue metadata\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW,
                                  "00 01 09 00 00 00 00 00 00 09",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != RC_LEN) || (requestCapture != 0U)) {
        printf("SELFTEST FAIL manual malformed CMD09 capture guard\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW,
                                  "76 25 09 00 00 00 00 00 00 00",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != RC_LEN) || (requestCapture != 0U)) {
        printf("SELFTEST FAIL manual bad-checksum CMD09 capture guard\n");
        return 1;
    }
    FormatCommandLogLabel(GUI_COMMAND_MANUAL_RAW, tx, formatted, sizeof(formatted));
    if (strcmp(formatted, "manual SPI1 frame") != 0) {
        printf("SELFTEST FAIL manual command log label\n");
        return 1;
    }
    FormatCommandLogLabel(0x09U, tx, formatted, sizeof(formatted));
    if (strcmp(formatted, "CMD_09") != 0) {
        printf("SELFTEST FAIL normal command log label\n");
        return 1;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW,
                                  "76 25 09 01 00 00 00 00 00 0A AA BB",
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != 12U) || (requestCapture != 0U) ||
        (tx[10] != 0xAAU) || (tx[11] != 0xBBU)) {
        printf("SELFTEST FAIL manual long raw frame packing\n");
        return 1;
    }
    (void)memset(&queue, 0, sizeof(queue));
    if ((QueueSpi1Command(&queue, tx, txLength, 1U, requestCapture) != 0) ||
        (TakeSpi1Command(&queue, queuedTx, &queuedLength,
                         &queuedIsManual, &queuedRequestCapture) != 1) ||
        (queuedLength != 12U) || (queuedIsManual != 1U) ||
        (queuedRequestCapture != 0U) || (memcmp(queuedTx, tx, 12U) != 0)) {
        printf("SELFTEST FAIL manual long raw frame queue metadata\n");
        return 1;
    }
    if (BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, "",
                                tx, &txLength, &requestCapture,
                                errorText, sizeof(errorText)) == 0) {
        printf("SELFTEST FAIL manual empty raw frame validation\n");
        return 1;
    }
    manualTextUsed = 0U;
    for (manualIndex = 0U; manualIndex < SPI1_PAGE_LEN; manualIndex++) {
        int written = snprintf(&manual128Text[manualTextUsed], sizeof(manual128Text) - manualTextUsed,
                               "%s%02lX", (manualIndex == 0U) ? "" : " ",
                               (unsigned long)(manualIndex & 0xFFU));
        if ((written < 0) || ((size_t)written >= (sizeof(manual128Text) - manualTextUsed))) {
            printf("SELFTEST FAIL manual 128-byte text build\n");
            return 1;
        }
        manualTextUsed += (size_t)written;
    }
    if ((BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, manual128Text,
                                  tx, &txLength, &requestCapture,
                                  errorText, sizeof(errorText)) != 0) ||
        (txLength != SPI1_PAGE_LEN) || (requestCapture != 0U) ||
        (tx[0] != 0x00U) || (tx[SPI1_PAGE_LEN - 1U] != 0x7FU)) {
        printf("SELFTEST FAIL manual 128-byte raw frame packing\n");
        return 1;
    }
    manualTextUsed = 0U;
    for (manualIndex = 0U; manualIndex <= SPI1_PAGE_LEN; manualIndex++) {
        int written = snprintf(&manual129Text[manualTextUsed], sizeof(manual129Text) - manualTextUsed,
                               "%s%02lX", (manualIndex == 0U) ? "" : " ",
                               (unsigned long)(manualIndex & 0xFFU));
        if ((written < 0) || ((size_t)written >= (sizeof(manual129Text) - manualTextUsed))) {
            printf("SELFTEST FAIL manual 129-byte text build\n");
            return 1;
        }
        manualTextUsed += (size_t)written;
    }
    if (BuildGuiCommandFromText(GUI_COMMAND_MANUAL_RAW, manual129Text,
                                tx, &txLength, &requestCapture,
                                errorText, sizeof(errorText)) == 0) {
        printf("SELFTEST FAIL manual 129-byte raw frame validation\n");
        return 1;
    }
    InterlockedExchange(&g_app.spi2Run, 1);
    InterlockedExchange(&g_app.spi2CaptureRequest, 1);
    InterlockedExchange(&g_app.spi1SuspendForSpi2, 1);
    ClearSpi2MonitorState();
    if ((InterlockedCompareExchange(&g_app.spi2Run, 0, 0) != 0) ||
        (InterlockedCompareExchange(&g_app.spi2CaptureRequest, 0, 0) != 0) ||
        (InterlockedCompareExchange(&g_app.spi1SuspendForSpi2, 0, 0) != 0)) {
        printf("SELFTEST FAIL SPI2 monitor state clear\n");
        return 1;
    }
    if (strstr(GetCommandHint(0x05U), "decimal Hz") == NULL) {
        printf("SELFTEST FAIL command hint\n");
        return 1;
    }
    if ((strcmp(Spi1ToggleButtonText(0), "Start SPI1 TM") != 0) ||
        (strcmp(Spi1ToggleButtonText(1), "Stop SPI1 TM") != 0) ||
        (strcmp(Spi2ToggleButtonText(0), "Start SPI2 DT") != 0) ||
        (strcmp(Spi2ToggleButtonText(1), "Stop SPI2 DT") != 0)) {
        printf("SELFTEST FAIL SPI toggle labels\n");
        return 1;
    }
    if ((strcmp(TelemetryRxToggleButtonText(0), "Start TM RX") != 0) ||
        (strcmp(TelemetryRxToggleButtonText(1), "Stop TM RX") != 0)) {
        printf("SELFTEST FAIL telemetry RX toggle labels\n");
        return 1;
    }
    if ((CMD_PARAMS_WIDTH != 192) || (CMD_SEND_X != 970)) {
        printf("SELFTEST FAIL command params layout\n");
        return 1;
    }
    if ((LOG_HEADER_Y < (CMD_HINT_Y + CMD_HINT_H + 4)) ||
        (LOG_BUTTON_Y < (CMD_HINT_Y + CMD_HINT_H + 4)) ||
        (LOG_EDIT_TOP < (LOG_HEADER_Y + 24))) {
        printf("SELFTEST FAIL command hint/log vertical layout\n");
        return 1;
    }
    if (TrimStartForAppend(59000, 5000, LOG_MAX_CHARS) <= 0) {
        printf("SELFTEST FAIL log trim calculation\n");
        return 1;
    }
#ifndef LOG_EDIT_TEXT_LIMIT
    printf("SELFTEST FAIL log edit text limit missing\n");
    return 1;
#else
    if (LOG_EDIT_TEXT_LIMIT < LOG_MAX_CHARS) {
        printf("SELFTEST FAIL log edit text limit too small\n");
        return 1;
    }
#endif
    FormatDtTimestamp(0U, formatted, sizeof(formatted));
    if (strcmp(formatted, "2009-01-01 00:00:00") != 0) {
        printf("SELFTEST FAIL DT timestamp epoch\n");
        return 1;
    }
    FormatDtTimestamp(3661U, formatted, sizeof(formatted));
    if (strcmp(formatted, "2009-01-01 01:01:01") != 0) {
        printf("SELFTEST FAIL DT timestamp offset\n");
        return 1;
    }
    if ((Spi2ReadDelayMs(0U) != SPI2_IDLE_DELAY_MS) ||
        (Spi2ReadDelayMs(1U) != FIXED_SPI2_DELAY_MS)) {
        printf("SELFTEST FAIL SPI2 read delay policy\n");
        return 1;
    }
#ifndef SPI2_CAPTURE_ARM_DELAY_MS
    printf("SELFTEST FAIL SPI2 capture arm delay missing\n");
    return 1;
#else
    if (SPI2_CAPTURE_ARM_DELAY_MS < 50U) {
        printf("SELFTEST FAIL SPI2 capture arm delay too small\n");
        return 1;
    }
#endif
#ifndef SPI2_KEEP_DEVICE_OPEN
    printf("SELFTEST FAIL SPI2 persistent device handle missing\n");
    return 1;
#endif
#ifndef SPI2_CAPTURE_MAX_FRAMES
    printf("SELFTEST FAIL SPI2 capture max frame guard missing\n");
    return 1;
#else
    if (SPI2_CAPTURE_MAX_FRAMES < 134000U) {
        printf("SELFTEST FAIL SPI2 capture max frame guard too small\n");
        return 1;
    }
#endif
#ifndef SPI2_CAPTURE_IDLE_TIMEOUT_MS
    printf("SELFTEST FAIL SPI2 capture idle timeout missing\n");
    return 1;
#else
    if ((Spi2CaptureShouldContinue(1U, 0U, 0) == 0) ||
        (Spi2CaptureShouldContinue(SPI2_CAPTURE_MAX_FRAMES, 0U, 1) != 0) ||
        (Spi2CaptureShouldContinue(1U, SPI2_CAPTURE_IDLE_TIMEOUT_MS, 0) != 0) ||
        (Spi2CaptureShouldContinue(1U, SPI2_CAPTURE_IDLE_TIMEOUT_MS, 1) == 0)) {
        printf("SELFTEST FAIL SPI2 capture continuation policy\n");
        return 1;
    }
#endif
    spi2Offset = 0U;
    (void)memset(frame, 0, sizeof(frame));
    if ((Spi2CaptureAcceptByte(frame, &spi2Offset, 0xCFU) != 0) ||
        (spi2Offset != 2U) || (frame[0] != 0x1AU) || (frame[1] != 0xCFU) ||
        (Spi2CaptureAcceptByte(frame, &spi2Offset, 0xFCU) != 0) ||
        (spi2Offset != 3U) || (Spi2CaptureAcceptByte(frame, &spi2Offset, 0x1DU) != 0) ||
        (spi2Offset != 4U)) {
        printf("SELFTEST FAIL SPI2 lost sync recovery\n");
        return 1;
    }
    BuildDailyLogPath(logPath, sizeof(logPath), "spi1", 2026, 8, 11);
    if (strcmp(logPath, "logs\\spi1_20260811.log") != 0) {
        printf("SELFTEST FAIL log path\n");
        return 1;
    }
    if ((FormatTimestampedLog(timestamped, sizeof(timestamped), "2026-08-11 12:34:56", "A\r\nB\r\n") != 0) ||
        (strstr(timestamped, "2026-08-11 12:34:56 A") == NULL) ||
        (strstr(timestamped, "\r\n2026-08-11 12:34:56 B") == NULL)) {
        printf("SELFTEST FAIL timestamped log\n");
        return 1;
    }
    (void)memset(&reportTable, 0, sizeof(reportTable));
    ReportTableAdd(&reportTable, "TC-X", "item", "PASS", "detail with | pipe");
    ReportTableAdd(&reportTable, "RF-X", "manual", "SKIP/人工测试", "manual only");
    if ((reportTable.count != 2U) || (reportTable.pass != 1U) ||
        (reportTable.skip != 1U) || (reportTable.failed != 0U)) {
        printf("SELFTEST FAIL report aggregation\n");
        return 1;
    }
    (void)memset(&reportTable, 0, sizeof(reportTable));
    (void)memset(&reportConfig, 0, sizeof(reportConfig));
    (void)snprintf(reportConfig.uartPort, sizeof(reportConfig.uartPort), "COM?");
    reportConfig.uartBaud = DEFAULT_UART_BAUD;
    reportConfig.spi1Index = -1;
    reportConfig.spi2Index = -1;
    ReportRunHardwareChecks(&reportTable, &reportConfig);
    ReportAddMissingV01Rows(&reportTable);
    if (reportTable.pass != 0U) {
        printf("SELFTEST FAIL disconnected report must not contain PASS rows\n");
        return 1;
    }
    for (reportIndex = 0U; reportIndex < reportTable.count; reportIndex++) {
        if ((strcmp(reportTable.rows[reportIndex].result, "INFO") == 0) ||
            (strstr(reportTable.rows[reportIndex].item, "Local") != NULL) ||
            (strstr(reportTable.rows[reportIndex].item, "GUI command packing") != NULL)) {
            printf("SELFTEST FAIL report contains local-only row: %s %s %s\n",
                   reportTable.rows[reportIndex].id,
                   reportTable.rows[reportIndex].item,
                   reportTable.rows[reportIndex].result);
            return 1;
        }
    }
    for (reportIndex = 0U; reportIndex < (sizeof(expectedReportIds) / sizeof(expectedReportIds[0])); reportIndex++) {
        if (ReportTableCountId(&reportTable, expectedReportIds[reportIndex]) != 1U) {
            printf("SELFTEST FAIL report ID coverage %s count=%lu\n",
                   expectedReportIds[reportIndex],
                   (unsigned long)ReportTableCountId(&reportTable, expectedReportIds[reportIndex]));
            return 1;
        }
    }
    if (reportTable.count != (sizeof(expectedReportIds) / sizeof(expectedReportIds[0]))) {
        printf("SELFTEST FAIL report row count expected=%lu got=%lu\n",
               (unsigned long)(sizeof(expectedReportIds) / sizeof(expectedReportIds[0])),
               (unsigned long)reportTable.count);
        return 1;
    }
    for (reportIndex = 0U; reportIndex < (sizeof(autoReportIds) / sizeof(autoReportIds[0])); reportIndex++) {
        foundRow = ReportTableFindRow(&reportTable, autoReportIds[reportIndex]);
        if ((foundRow == NULL) || (strcmp(foundRow->result, "SKIP/人工测试") == 0)) {
            printf("SELFTEST FAIL report row should be automated when resources exist: %s\n",
                   autoReportIds[reportIndex]);
            return 1;
        }
    }
    foundRow = ReportTableFindRow(&reportTable, "FL-07");
    if ((foundRow == NULL) || (strcmp(foundRow->result, "FAILED") != 0)) {
        printf("SELFTEST FAIL FL-07 missing board must fail\n");
        return 1;
    }
    foundRow = ReportTableFindRow(&reportTable, "FL-01");
    if ((foundRow == NULL) || (strcmp(foundRow->result, "SKIP/人工测试") == 0) ||
        (strstr(foundRow->detail, "未测试OTA和复位") == NULL)) {
        printf("SELFTEST FAIL FL-01 automation note\n");
        return 1;
    }
    (void)snprintf(reportConfig.startTime, sizeof(reportConfig.startTime), "2026-09-03 10:00:00");
    (void)snprintf(reportConfig.endTime, sizeof(reportConfig.endTime), "2026-09-03 10:05:00");
    if ((EnsureDirectory("reports") != 0) ||
        (ReportWriteMarkdown("reports\\selftest_report.md", &reportTable, &reportConfig) != 0)) {
        printf("SELFTEST FAIL report markdown write\n");
        return 1;
    }
    selfReport = fopen("reports\\selftest_report.md", "rb");
    if (selfReport == NULL) {
        printf("SELFTEST FAIL report markdown open\n");
        return 1;
    }
    (void)fseek(selfReport, 0, SEEK_END);
    selfReportSize = ftell(selfReport);
    (void)fseek(selfReport, 0, SEEK_SET);
    selfReportText = (char*)malloc((size_t)selfReportSize + 1U);
    if (selfReportText == NULL) {
        (void)fclose(selfReport);
        printf("SELFTEST FAIL report markdown allocation\n");
        return 1;
    }
    if (fread(selfReportText, 1U, (size_t)selfReportSize, selfReport) != (size_t)selfReportSize) {
        free(selfReportText);
        (void)fclose(selfReport);
        printf("SELFTEST FAIL report markdown read\n");
        return 1;
    }
    selfReportText[selfReportSize] = '\0';
    (void)fclose(selfReport);
    if ((strstr(selfReportText, "## 一、测试概况") == NULL) ||
        (strstr(selfReportText, "| 测试版本号 |") == NULL) ||
        (strstr(selfReportText, "| 测试工具版本号 |") == NULL) ||
        (strstr(selfReportText, "| 测试开始时间 | 2026-09-03 10:00:00 |") == NULL) ||
        (strstr(selfReportText, "| 测试结束时间 | 2026-09-03 10:05:00 |") == NULL) ||
        (strstr(selfReportText, "| 测试环境与真实环境差异 |") == NULL) ||
        (strstr(selfReportText, "### 3.1 遥控错误帧类型") == NULL) ||
        (strstr(selfReportText, "### 3.2 单条遥控指令测试") == NULL) ||
        (strstr(selfReportText, "### 3.7 完整流程和组合流程测试") == NULL)) {
        free(selfReportText);
        printf("SELFTEST FAIL report markdown metadata/sections\n");
        return 1;
    }
    if ((strstr(selfReportText, "## 四、整机OTA联调记录") != NULL) ||
        (strstr(selfReportText, "| OT-06 |") != NULL)) {
        free(selfReportText);
        printf("SELFTEST FAIL report markdown OTA section removed\n");
        return 1;
    }
    if (strstr(selfReportText, "| TC-07 |") < strstr(selfReportText, "| TC-06 |")) {
        free(selfReportText);
        printf("SELFTEST FAIL report markdown TC order\n");
        return 1;
    }
    if (strstr(selfReportText, "| TC-10 |") < strstr(selfReportText, "| TC-09 |")) {
        free(selfReportText);
        printf("SELFTEST FAIL report markdown TC gap order\n");
        return 1;
    }
    free(selfReportText);
    (void)memset(&reportRow, 0, sizeof(reportRow));
    (void)snprintf(reportRow.id, sizeof(reportRow.id), "TC-X");
    (void)snprintf(reportRow.item, sizeof(reportRow.item), "item");
    (void)snprintf(reportRow.result, sizeof(reportRow.result), "PASS");
    (void)snprintf(reportRow.detail, sizeof(reportRow.detail), "A|B\r\nC");
    if ((ReportFormatMarkdownRow(markdownRow, sizeof(markdownRow), &reportRow) != 0) ||
        (strstr(markdownRow, "A/B C") == NULL)) {
        printf("SELFTEST FAIL report markdown row\n");
        return 1;
    }
    if ((ParseHexBytes("12 34", parsed, sizeof(parsed), &parsedLen) != 0) ||
        (parsedLen != 2U) || (parsed[0] != 0x12U) || (parsed[1] != 0x34U)) {
        printf("SELFTEST FAIL hex byte parse\n");
        return 1;
    }
    BuildDtRecord(&records, 0x66B6A7F0UL, DT_FORMAT_BINARY, 0x01U, payload, sizeof(payload));
    BuildDtFrame(frame, 3U, records.data, (uint16_t)records.length);
    if (AppendFramePayload(&framed, frame, error, sizeof(error)) != 0) {
        printf("SELFTEST FAIL frame parse: %s\n", error);
        return 1;
    }
    if ((framed.length != records.length) || (memcmp(framed.data, records.data, records.length) != 0)) {
        printf("SELFTEST FAIL frame payload mismatch\n");
        return 1;
    }
    FormatRawDtFrameLog(rawLine, sizeof(rawLine), 3U, 9U, frame);
    if ((strstr(rawLine, "RAW frame=3 seq=9 len=512 hex=1A CF FC 1D") == NULL) ||
        (strstr(rawLine, "5A 5A") == NULL) ||
        (strstr(rawLine, "truncated") != NULL)) {
        printf("SELFTEST FAIL raw SPI2 frame log format\n");
        return 1;
    }
    for (payloadIndex = 0U; payloadIndex < sizeof(largePayload); payloadIndex++) {
        largePayload[payloadIndex] = (uint8_t)('A' + (payloadIndex % 26U));
    }
    BuildDtRecord(&splitRecords, 0x66B6A7F1UL, DT_FORMAT_BINARY, 0x7EU,
                  largePayload, sizeof(largePayload));
    if (splitRecords.length != (DT_RECORD_HEADER_LEN + sizeof(largePayload))) {
        printf("SELFTEST FAIL split record length\n");
        return 1;
    }
    BuildDtFrame(frame, 0U, splitRecords.data, DT_FRAME_DATA_LEN);
    if ((AppendFramePayload(&splitFramed, frame, error, sizeof(error)) != 0) ||
        (DecodeRecordStream(&splitStream, splitFramed.data, splitFramed.length, 0U, 1U) != 0) ||
        (splitStream.nextIndex != 0U) || (splitStream.pending.length != DT_FRAME_DATA_LEN)) {
        printf("SELFTEST FAIL split record first frame handling\n");
        return 1;
    }
    splitFramed.length = 0U;
    BuildDtFrame(frame, 1U, &splitRecords.data[DT_FRAME_DATA_LEN],
                 (uint16_t)(splitRecords.length - DT_FRAME_DATA_LEN));
    if ((AppendFramePayload(&splitFramed, frame, error, sizeof(error)) != 0) ||
        (DecodeRecordStream(&splitStream, splitFramed.data, splitFramed.length, 0U, 2U) != 0) ||
        (splitStream.nextIndex != 1U) || (splitStream.pending.length != 0U)) {
        printf("SELFTEST FAIL split record second frame handling\n");
        return 1;
    }
    if ((splitStream.lastRecordStartFrame != 1U) ||
        (splitStream.lastRecordEndFrame != 2U) ||
        (splitStream.lastRecordBytes != (DT_RECORD_HEADER_LEN + sizeof(largePayload)))) {
        printf("SELFTEST FAIL split record frame span\n");
        return 1;
    }
    splitStream.pending.length = DT_FRAME_DATA_LEN;
    splitStream.nextIndex = 42U;
    ResetRecordStream(&splitStream);
    if ((splitStream.pending.length != 0U) || (splitStream.nextIndex != 0U)) {
        printf("SELFTEST FAIL SPI2 record stream reset\n");
        return 1;
    }
    one = DefaultAssignment(1);
    two = DefaultAssignment(2);
    if ((one.spi1Index != 0) || (one.spi2Index != 0) || (two.spi1Index != 0) || (two.spi2Index != 1)) {
        printf("SELFTEST FAIL default assignment\n");
        return 1;
    }
    if ((ParseScannedDeviceEntries("JTool-SPI (SN:SPI1A001) (ID:0)\nJTool-SPI (SN:SPI2B002) (ID:1)",
                                   2, entries, 4) != 2) ||
        (entries[0].type != DEVICE_ENTRY_SN) || (strcmp(entries[0].sn, "SPI1A001") != 0) ||
        (strstr(entries[0].label, "SN1") == NULL) ||
        (entries[1].type != DEVICE_ENTRY_SN) || (strcmp(entries[1].sn, "SPI2B002") != 0) ||
        (strstr(entries[1].label, "SN2") == NULL)) {
        printf("SELFTEST FAIL scan SN parse\n");
        return 1;
    }
    if (EffectiveDeviceListCount(1) < 2) {
        printf("SELFTEST FAIL fallback device list count\n");
        return 1;
    }
    largeLayout = ComputeMainLayout(1600, 900);
    if ((largeLayout.spi1W <= 520) || (largeLayout.spi2W <= 520) ||
        ((largeLayout.spi1H + largeLayout.spi1Y) < 850) ||
        ((largeLayout.spi2H + largeLayout.spi2Y) < 850)) {
        printf("SELFTEST FAIL resize layout\n");
        return 1;
    }
    (void)memset(tm, 0, sizeof(tm));
    tm[0] = 0xEBU;
    tm[1] = 0x90U;
    tm[20] = 3U;
    tm[21] = 2U;
    tm[143] = Checksum8(tm, 2U, 142U);
    if (ValidateTelemetry(tm) != 0) {
        printf("SELFTEST FAIL telemetry checksum\n");
        return 1;
    }
    FormatTelemetry(tm, formatted, sizeof(formatted));
    if (strstr(formatted, "mode=3") == NULL || strstr(formatted, "rate=2") == NULL) {
        printf("SELFTEST FAIL telemetry format\n");
        return 1;
    }
    free(records.data);
    free(framed.data);
    free(splitRecords.data);
    free(splitFramed.data);
    free(splitStream.pending.data);
    free(splitStream.pendingFrames.data);
    printf("SELFTEST PASS\n");
    return 0;
}

int main(int argc, char** argv)
{
    if ((argc >= 2) && (strcmp(argv[1], "selftest") == 0)) {
        return RunSelftest();
    }
#ifdef _WIN32
    {
        int result;
        (void)timeBeginPeriod(1U);
        result = RunGui();
        (void)timeEndPeriod(1U);
        return result;
    }
#else
    printf("Windows is required for the GUI.\n");
    return 1;
#endif
}
