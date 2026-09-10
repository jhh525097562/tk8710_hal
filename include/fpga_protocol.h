#ifndef FPGA_PROTOCOL_H
#define FPGA_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FPGA_PROTOCOL_RC_FRAME_LEN 10U
#define FPGA_PROTOCOL_TM_FRAME_LEN 144U
#define FPGA_PROTOCOL_SPI_PAGE_LEN 128U

#define FPGA_PROTOCOL_VERSION_MAJOR 1U
#define FPGA_PROTOCOL_VERSION_MINOR 0U
#define FPGA_PROTOCOL_VERSION_PATCH 1U

#define DEFAULT_WORK_MODE 3U
#define DEFAULT_FREQ 506500000U
#define DEFAULT_RATE_COUNT 1U
#define DEFAULT_RATE_MODE 0U
#define DEFAULT_SLOT0_LENGTH 0U
#define DEFAULT_SLOT0_DAM 0U
#define DEFAULT_SLOT1_LENGTH 0U
#define DEFAULT_SLOT1_DAM 0U
#define DEFAULT_SLOT2_LENGTH 52U
#define DEFAULT_SLOT2_DAM 0U
#define DEFAULT_SLOT3_LENGTH 52U
#define DEFAULT_SLOT3_DAM 0U
#define DEFAULT_RX_GAIN 126U
#define DEFAULT_TX_GAIN 46U
#define DEFAULT_ANTENNA_MASK 0xFFU
#define DEFAULT_RF_MASK 0xFFU
#define DEFAULT_BCN_BITS 0U
#define DEFAULT_MD_AGC 1024U
#define DEFAULT_MAX_FRAME_COUNT 1U
#define DEFAULT_SWEEP_MODE 0U
#define DEFAULT_TONE_FREQ 335544U
#define DEFAULT_TONE_GAIN 0x40U
#define DEFAULT_ACM_CALIB_COUNT 1U
#define DEFAULT_ACM_SNR_THRESHOLD 28U

#define DEFAULT_SLOT_CONFIG 0U
#define DEFAULT_TX_POWER 26U

    typedef struct
    {
        uint32_t rxFrameCount;
        uint32_t physicalRxFrameCount;
        uint32_t rxErrorCount;
        uint32_t unsupportedCommandCount;
        uint8_t lastCommandId;
        uint8_t lastPhysicalRxFrame[FPGA_PROTOCOL_RC_FRAME_LEN];
        uint8_t workMode;
        uint8_t rateMode;
        uint8_t slotConfig;
        uint8_t txPower;
        uint32_t centerFreqHz;
        uint8_t rfMask;
        uint8_t bootFlag;
        uint16_t lastRegDevice;
        uint16_t lastRegAddress;
        uint32_t lastRegValue;
        uint32_t utcSeconds;
        uint32_t resetCount;
        uint8_t resetType;
    } FpgaProtocolSnapshot;

    void FpgaProtocol_Init(void);
    void FpgaProtocol_Process(void);
    void FpgaProtocol_GetSnapshot(FpgaProtocolSnapshot *snapshot);
    void FpgaProtocol_FormatUtcTime(uint32_t seconds, char *text, uint32_t textSize);
    void FpgaProtocol_Log(const char *text);

#if defined(FPGA_PROTOCOL_HOST_TEST)
    void FpgaProtocol_TestReset(void);
    int FpgaProtocol_TestHandleRxFrame(const uint8_t *frame, uint32_t *errorCount);
    int FpgaProtocol_TestHandleRxFrameNoReset(const uint8_t *frame, uint32_t *errorCount);
    void FpgaProtocol_TestBuildTelemetry(uint8_t *frame);
    void FpgaProtocol_TestBuildTelemetryPage(uint8_t *page, uint8_t pageIndex);
    void FpgaProtocol_TestQueueTelemetryBurst(void);
    void FpgaProtocol_TestCompleteTransferAndBuildNextPage(uint8_t *page);
    void FpgaProtocol_TestNextTelemetryTxPage(uint8_t *page);
    const char *FpgaProtocol_TestGetLastLog(void);
    uint32_t FpgaProtocol_TestGetLogCount(void);
    void FpgaProtocol_TestClearLastLog(void);
    uint8_t FpgaProtocol_TestGetSavedDcMask(void);
    int FpgaProtocol_TestGetSavedDc(uint8_t antenna, int16_t *iDc, int16_t *qDc);
    uint32_t FpgaProtocol_TestGetDcApplyCount(void);
    int FpgaProtocol_TestGetAppliedDc(uint32_t index, uint8_t *antenna, int16_t *iDc, int16_t *qDc);
    uint8_t FpgaProtocol_TestGetHostAppliedRateMode(void);
    uint32_t FpgaProtocol_TestGetResetRequestCount(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FPGA_PROTOCOL_H */
