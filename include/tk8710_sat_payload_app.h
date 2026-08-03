#ifndef TK8710_SAT_PAYLOAD_APP_H
#define TK8710_SAT_PAYLOAD_APP_H

#include <stdint.h>
#include "driver/tk8710_platform.h"
#include "trm/trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_PAYLOAD_MAX_RATES       4U
#define SAT_PAYLOAD_SLOT_COUNT      4U
#define SAT_PAYLOAD_IRQ_COUNT       10U
#define SAT_PAYLOAD_MAX_DATA_LEN    64U
#define SAT_PAYLOAD_RF_ANTENNA_COUNT 8U
#define SAT_PAYLOAD_MODE_NONE       0xFFU

typedef enum {
    SAT_PAYLOAD_STATE_BOOT = 0,
    SAT_PAYLOAD_STATE_SELF_TEST,
    SAT_PAYLOAD_STATE_CONTROL_READY,
    SAT_PAYLOAD_STATE_CONFIGURING,
    SAT_PAYLOAD_STATE_RUNNING,
    SAT_PAYLOAD_STATE_SWEEP,
    SAT_PAYLOAD_STATE_TONE,
    SAT_PAYLOAD_STATE_CALIBRATING,
    SAT_PAYLOAD_STATE_CAPTURING,
    SAT_PAYLOAD_STATE_RECOVERING,
    SAT_PAYLOAD_STATE_FAULT
} SatPayloadState;

typedef enum {
    SAT_PAYLOAD_MODE_SWEEP = 0,
    SAT_PAYLOAD_MODE_A = 1,
    SAT_PAYLOAD_MODE_B = 2,
    SAT_PAYLOAD_MODE_C = 3,
    SAT_PAYLOAD_MODE_TONE = 4,
    SAT_PAYLOAD_MODE_ANT_CAL = 5,
    SAT_PAYLOAD_MODE_CAPTURE = 6
} SatPayloadWorkMode;

typedef enum {
    SAT_PAYLOAD_OK = 0,
    SAT_PAYLOAD_ACCEPTED = 1,
    SAT_PAYLOAD_ERR_PARAM = -1,
    SAT_PAYLOAD_ERR_STATE = -2,
    SAT_PAYLOAD_ERR_DRIVER = -3,
    SAT_PAYLOAD_ERR_UNSUPPORTED = -4,
    SAT_PAYLOAD_ERR_ROLLBACK = -5
} SatPayloadResult;

typedef struct {
    uint16_t byteLen;
    uint32_t daM;
} SatPayloadSlotParams;

typedef struct {
    uint8_t rateMode;
    SatPayloadSlotParams slots[SAT_PAYLOAD_SLOT_COUNT];
} SatPayloadRateParams;

/*
 * This is an application-level configuration, not an on-wire packet.
 * A future spacecraft communication adapter must serialize each field
 * explicitly and must not memcpy this structure to or from the wire.
 */
typedef struct {
    uint32_t centerFreqHz;
    uint8_t rateCount;
    SatPayloadRateParams rates[SAT_PAYLOAD_MAX_RATES];
    uint8_t rxGain;
    uint8_t txGain;
    uint8_t antennaMask;
    uint8_t rfMask;
    uint8_t bcnBits;
    uint8_t txBcnAntennaMask;
    uint8_t plCrcEnable;
    uint8_t localSync;
    uint32_t rxDelay;
    uint32_t mdAgc;
    uint32_t maxFrameCount;
    uint32_t sweepStartFreqHz;
    uint32_t sweepEndFreqHz;
    uint8_t sweepMode;
    uint32_t toneFreq;
    uint8_t toneGain;
    uint8_t acmCalibCount;
    uint8_t acmSnrThreshold;
} SatPayloadWorkParams;

typedef struct {
    uint8_t valid;
    uint32_t generation;
    uint32_t userId;
    uint8_t rateMode;
    int16_t rssi;
    uint8_t snr;
    int32_t freqOffset;
    uint32_t frequencyHz;
    uint16_t dataLen;
    uint32_t frameNo;
    uint32_t timestampMs;
} SatPayloadLastRx;

typedef struct {
    uint32_t sequence;
    uint32_t uptimeMs;
    SatPayloadState state;
    uint8_t activeMode;
    uint8_t pendingParamsValid;
    int32_t lastResult;
    uint32_t configVersion;
    SatPayloadWorkParams activeParams;
    uint32_t txCount;
    uint32_t txSuccessCount;
    uint32_t rxCount;
    uint32_t beamCount;
    uint32_t txQueueRemaining;
    uint32_t satelliteCacheCount;
    uint32_t satelliteGroundStationBeamCount;
    uint32_t satelliteRouteCount;
    uint32_t satelliteBeamMissCount;
    uint32_t irqCounters[SAT_PAYLOAD_IRQ_COUNT];
    uint32_t irqStatus;
    uint32_t irqMask;
    uint32_t heapUsed;
    uint32_t heapPeak;
    uint32_t heapFailCount;
    uint32_t spiErrorCount;
    uint32_t gpioIrqCount;
    uint32_t gpioIrqEdgeCount;
    uint32_t gpioIrqRecoveryCount;
    uint32_t irqStatusPollCount;
    uint32_t spiResetCount;
    uint32_t resetDriveLowCount;
    uint32_t resetPinLowCount;
    uint32_t portInitCount;
    uint32_t spiInitCount;
    uint32_t resetGioDout;
    uint32_t resetGioDir;
    uint32_t gpioDin;
    uint32_t gpioFlag;
    uint32_t gpioEnable;
    uint32_t vimReqMask0;
    uint8_t gpioIrqLevel;
    uint8_t resetPinLevel;
    uint8_t gpioIrqCallbackConfigured;
    uint8_t sdramAvailable;
    uint8_t acmPending;
    uint8_t acmRunning;
    uint32_t acmCompletedCount;
    int32_t acmLastResult;
    TRM_AcmCalibResult acmResult;
    SatPayloadLastRx lastRx;
    TK8710CaptureInfo capture;
    TRM_SweepResultInfo sweep;
} SatPayloadTelemetry;

typedef enum {
    SAT_PAYLOAD_TC_SET_WORK_PARAMS = 1,
    SAT_PAYLOAD_TC_SET_WORK_MODE,
    SAT_PAYLOAD_TC_STOP,
    SAT_PAYLOAD_TC_RESET_8710,
    SAT_PAYLOAD_TC_REQUEST_ACM,
    SAT_PAYLOAD_TC_READ_REG,
    SAT_PAYLOAD_TC_WRITE_REG,
    SAT_PAYLOAD_TC_READ_RF_REG,
    SAT_PAYLOAD_TC_WRITE_RF_REG,
    SAT_PAYLOAD_TC_SELECT_PAYLOAD,
    SAT_PAYLOAD_TC_SYSTEM_RESET,
    SAT_PAYLOAD_TC_FIRMWARE_UPGRADE,
    SAT_PAYLOAD_TC_SET_RF_TX_DC
} SatPayloadTelecommandId;

typedef struct {
    uint16_t address;
    uint32_t value;
    uint8_t rfMask;
} SatPayloadRegisterCommand;

typedef struct {
    uint8_t antenna;
    int16_t iDc;
    int16_t qDc;
} SatPayloadRfTxDcCommand;

typedef union {
    SatPayloadWorkParams workParams;
    uint8_t workMode;
    SatPayloadRegisterCommand reg;
    SatPayloadRfTxDcCommand rfTxDc;
} SatPayloadTelecommandPayload;

typedef struct {
    uint32_t requestId;
    SatPayloadTelecommandId commandId;
    SatPayloadTelecommandPayload payload;
} SatPayloadTelecommand;

typedef struct {
    uint32_t requestId;
    SatPayloadTelecommandId commandId;
    SatPayloadResult result;
    SatPayloadState stateBefore;
    SatPayloadState stateAfter;
    uint32_t value;
} SatPayloadTelecommandResponse;

void SatPayloadApp_Init(void);
void SatPayloadApp_Process(void);
SatPayloadResult SatPayloadApp_SetWorkParams(const SatPayloadWorkParams* params);
SatPayloadResult SatPayloadApp_SetWorkMode(uint8_t mode);
SatPayloadResult SatPayloadApp_Stop(void);
/* Direct TK8710 global-register access; valid in every payload work state. */
SatPayloadResult SatPayloadApp_ReadRegister(uint16_t address,
                                            uint32_t* value);
SatPayloadResult SatPayloadApp_WriteRegister(uint16_t address,
                                             uint32_t value);
/* Set one antenna's TX I/Q DC compensation; applies live when HAL is active. */
SatPayloadResult SatPayloadApp_SetRfTxDc(uint8_t antenna,
                                        int16_t iDc,
                                        int16_t qDc);
SatPayloadResult SatPayloadApp_HandleTelecommand(
    const SatPayloadTelecommand* request,
    SatPayloadTelecommandResponse* response);
void SatPayloadApp_GetTelemetry(SatPayloadTelemetry* telemetry);
SatPayloadResult SatPayloadApp_GetAcmCalibrationResult(
    TRM_AcmCalibResult* result);
SatPayloadResult SatPayloadApp_GetCaptureInfo(TK8710CaptureInfo* info);
SatPayloadResult SatPayloadApp_ReadCaptureData(uint32_t generation,
                                               uint8_t antenna,
                                               uint32_t offset,
                                               void* data, uint32_t len);
SatPayloadResult SatPayloadApp_GetSweepResultInfo(TRM_SweepResultInfo* info);
SatPayloadResult SatPayloadApp_ReadSweepResults(uint32_t startIndex,
                                                TRM_SweepResultPoint* results,
                                                uint32_t capacity,
                                                uint32_t* resultCount);
SatPayloadState SatPayloadApp_GetState(void);
uint8_t SatPayloadApp_GetActiveMode(void);
const char* SatPayloadApp_StateName(SatPayloadState state);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_SAT_PAYLOAD_APP_H */
