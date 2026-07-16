#include "tk8710_sat_payload_app.h"

#include <string.h>

#include "hal_api.h"
#include "tk8710_tms570.h"
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_platform.h"
#include "driver/tk8710_regs.h"
#include "trm/trm_api.h"
#include "trm/trm_log.h"

#define SAT_PAYLOAD_TELEMETRY_PERIOD_MS       1000U
#define SAT_PAYLOAD_CALIBRATION_PERIOD_MS     1000U
#define SAT_PAYLOAD_CAPTURE_PERIOD_MS         1000U
#define SAT_PAYLOAD_BEAM_MAX_USERS            512U
#define SAT_PAYLOAD_BEAM_TIMEOUT_MS            20000U
#define SAT_PAYLOAD_GS_BEAM_MAX                5U
#define SAT_PAYLOAD_GS_BEAM_TIMEOUT_MS         65000U
#define SAT_PAYLOAD_UPLINK_CACHE_SIZE          128U
#define SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS     16U
#define SAT_PAYLOAD_CALLBACK_DATA_PREFIX       8U
#define SAT_PAYLOAD_HW_RESERVED_USER_SLOTS     1U
#define SAT_PAYLOAD_HW_RESERVED_FREQ_OFFSET    20000.0F

typedef struct {
    SatPayloadState state;
    uint8_t activeMode;
    uint8_t halActive;
    uint8_t pendingValid;
    uint8_t activeValid;
    SatPayloadWorkParams pendingParams;
    SatPayloadWorkParams activeParams;
    uint32_t configVersion;
    uint32_t startMs;
    uint32_t lastTelemetryMs;
    uint32_t lastCalibrationRequestMs;
    uint32_t lastCaptureMs;
    int32_t lastResult;
    SatPayloadLastRx lastRx;
    SatPayloadTelemetry telemetry;
} SatPayloadContext;

static SatPayloadContext g_satPayload;

static SpiConfig g_satSpiConfig = {
    .speed = TK8710_TMS570_SPI_SPEED_HZ,
    .mode = TK8710_TMS570_SPI_MODE,
    .bits = TK8710_TMS570_SPI_BITS,
    .lsb_first = 0U,
    .cs_pin = 0U
};

static ChiprfConfig g_satRfConfig = {
    .rftype = TK8710_RF_TYPE_1255_1M,
    .Freq = 503100000UL,
    .rxgain = 0x7EU,
    .txgain = 0x2AU,
    .txadc = {
        {0x0400, 0x0350}, {0x0400, 0x0450},
        {0x0250, 0x0230}, {0x0400, 0x0290},
        {0x0390, 0x0500}, {0x0390, 0x0400},
        {0x0300, 0x0350}, {0x0490, 0x04A0}
    }
};

static ChipConfig g_satChipConfig = {
    .bcn_agc = 32U,
    .interval = 32U,
    .tx_dly = 0U,
    .tx_fix_info = 0U,
    .offset_adj = 0,
    .tx_pre = 0,
    .conti_mode = 1U,
    .bcn_scan = 0U,
    .ant_en = 0xFFU,
    .rf_sel = 0xFFU,
    .tx_bcn_en = 0xFFU,
    .ts_sync = 0U,
    .rf_model = 1U,
    .bcnbits = 0U,
    .anoiseThe1 = 0U,
    .power2rssi = 0U,
    .irq_ctrl0 = 0x7FFU,
    .irq_ctrl1 = 0U,
    .spiConfig = &g_satSpiConfig,
    .rfConfig = &g_satRfConfig
};

static const char* SatPayloadTxResultName(TRM_TxResult result)
{
    switch (result) {
        case TRM_TX_OK:
            return "OK";
        case TRM_TX_NO_BEAM:
            return "NO_BEAM";
        case TRM_TX_TIMEOUT:
            return "TIMEOUT";
        case TRM_TX_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static void SatPayloadFormatDataPrefix(const uint8_t* data, uint16_t dataLen,
                                       char* text, uint32_t textSize)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32_t byteCount;
    uint32_t i;
    uint32_t pos = 0U;

    if ((text == NULL) || (textSize == 0U)) {
        return;
    }
    text[0] = '\0';
    if ((data == NULL) || (dataLen == 0U)) {
        return;
    }

    byteCount = dataLen;
    if (byteCount > SAT_PAYLOAD_CALLBACK_DATA_PREFIX) {
        byteCount = SAT_PAYLOAD_CALLBACK_DATA_PREFIX;
    }
    for (i = 0U; i < byteCount; i++) {
        if ((pos + 3U) >= textSize) {
            break;
        }
        if (i != 0U) {
            text[pos++] = ' ';
        }
        text[pos++] = hex[(data[i] >> 4U) & 0x0FU];
        text[pos++] = hex[data[i] & 0x0FU];
    }
    text[pos] = '\0';
}

static uint8_t SatPayloadRateModeValid(uint8_t mode)
{
    return (uint8_t)((mode == 5U) || (mode == 6U) || (mode == 7U) ||
                     (mode == 8U) || (mode == 9U) || (mode == 10U) ||
                     (mode == 11U) || (mode == 18U));
}

static SatPayloadResult SatPayloadValidateParams(const SatPayloadWorkParams* params)
{
    uint32_t rate;
    uint32_t slot;

    if ((params == NULL) || (params->centerFreqHz == 0U) ||
        (params->rateCount == 0U) ||
        (params->rateCount > SAT_PAYLOAD_MAX_RATES) ||
        (params->bcnBits > 31U) ||
        (params->maxFrameCount == 0U)) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    for (rate = 0U; rate < params->rateCount; rate++) {
        if (SatPayloadRateModeValid(params->rates[rate].rateMode) == 0U) {
            return SAT_PAYLOAD_ERR_PARAM;
        }
        for (slot = 0U; slot < SAT_PAYLOAD_SLOT_COUNT; slot++) {
            if (params->rates[rate].slots[slot].byteLen > 1023U) {
                return SAT_PAYLOAD_ERR_PARAM;
            }
        }
    }

    if ((params->sweepMode > 3U) ||
        ((params->sweepStartFreqHz != 0U) &&
         (params->sweepEndFreqHz < params->sweepStartFreqHz))) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    return SAT_PAYLOAD_OK;
}

static void SatPayloadBuildSlotConfig(const SatPayloadWorkParams* params,
                                      slotCfg_t* slotCfg)
{
    uint32_t rate;
    uint32_t antenna;

    (void)memset(slotCfg, 0, sizeof(*slotCfg));
    slotCfg->msMode = TK8710_MODE_MASTER;
    slotCfg->plCrcEn = params->plCrcEnable;
    slotCfg->rateCount = params->rateCount;
    /* Keep hardware user index 0 reserved so dedicated users start at index 1. */
    slotCfg->brdUserNum = SAT_PAYLOAD_HW_RESERVED_USER_SLOTS;
    slotCfg->brdFreq[0] = SAT_PAYLOAD_HW_RESERVED_FREQ_OFFSET;
    slotCfg->antEn = params->antennaMask;
    slotCfg->rfSel = params->rfMask;
    slotCfg->txBeamCtrlMode = 1U;
    slotCfg->txBcnAntEn = params->txBcnAntennaMask;
    slotCfg->rx_delay = params->rxDelay;
    slotCfg->md_agc = params->mdAgc;
    slotCfg->local_sync = params->localSync;

    for (antenna = 0U; antenna < TK8710_MAX_ANTENNAS; antenna++) {
        slotCfg->bcnRotation[antenna] = (uint8_t)antenna;
    }

    for (rate = 0U; rate < params->rateCount; rate++) {
        slotCfg->rateModes[rate] = (rateMode_e)params->rates[rate].rateMode;

        slotCfg->s0Cfg[rate].byteLen = params->rates[rate].slots[0].byteLen;
        slotCfg->s0Cfg[rate].da_m = params->rates[rate].slots[0].daM;
        slotCfg->s0Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s1Cfg[rate].byteLen = params->rates[rate].slots[1].byteLen;
        slotCfg->s1Cfg[rate].da_m = params->rates[rate].slots[1].daM;
        slotCfg->s1Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s2Cfg[rate].byteLen = params->rates[rate].slots[2].byteLen;
        slotCfg->s2Cfg[rate].da_m = params->rates[rate].slots[2].daM;
        slotCfg->s2Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s3Cfg[rate].byteLen = params->rates[rate].slots[3].byteLen;
        slotCfg->s3Cfg[rate].da_m = params->rates[rate].slots[3].daM;
        slotCfg->s3Cfg[rate].centerFreq = params->centerFreqHz;
    }
}

static void SatPayloadOnRxData(const TRM_RxDataList* rxDataList)
{
    const TRM_RxUserData* user;
    uint32_t logUserCount;
    uint32_t i;

    if ((rxDataList == NULL) || (rxDataList->users == NULL) ||
        (rxDataList->userCount == 0U)) {
        return;
    }

    user = &rxDataList->users[0];
    TK8710EnterCritical();
    g_satPayload.lastRx.valid = user->valid;
    g_satPayload.lastRx.userId = user->userId;
    g_satPayload.lastRx.rateMode = user->rateMode;
    g_satPayload.lastRx.rssi = user->rssi;
    g_satPayload.lastRx.snr = user->snr;
    g_satPayload.lastRx.freqOffset = user->freq;
    g_satPayload.lastRx.dataLen = user->dataLen;
    g_satPayload.lastRx.frameNo = rxDataList->frameNo;
    g_satPayload.lastRx.timestampMs = TK8710GetTickMs();
    TK8710ExitCritical();

    logUserCount = rxDataList->userCount;
    if (logUserCount > SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS) {
        logUserCount = SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS;
    }

    TRM_LOG_INFO("SAT APP RX callback: superFrame=%u systemFrame=%u users=%u",
                 rxDataList->frameNo, TRM_GetCurrentFrame(), rxDataList->userCount);
    for (i = 0U; i < logUserCount; i++) {
        char dataPrefix[(SAT_PAYLOAD_CALLBACK_DATA_PREFIX * 3U) + 1U];

        user = &rxDataList->users[i];
        SatPayloadFormatDataPrefix(user->data, user->dataLen,
                                   dataPrefix, sizeof(dataPrefix));
        TRM_LOG_INFO("SAT APP RX[%u]: id=0x%08lX rate=%u len=%u valid=%u rssi=%d snr=%u freq=%ldHz data=%s%s",
                     i, (unsigned long)user->userId, user->rateMode,
                     user->dataLen, user->valid, (int)user->rssi, user->snr,
                     (long)(user->freq / 128), dataPrefix,
                     (user->dataLen > SAT_PAYLOAD_CALLBACK_DATA_PREFIX) ? " ..." : "");
    }
    if (logUserCount < rxDataList->userCount) {
        TRM_LOG_INFO("SAT APP RX callback: %u additional users omitted",
                     (unsigned int)(rxDataList->userCount - logUserCount));
    }
}

static void SatPayloadOnTxComplete(const TRM_TxCompleteResult* txResult)
{
    uint32_t logUserCount;
    uint32_t i;

    if (txResult == NULL) {
        TRM_LOG_WARN("SAT APP TX callback: null result");
        return;
    }

    TRM_LOG_INFO("SAT APP TX callback: superFrame=%u systemFrame=%u total=%lu results=%lu remaining=%lu",
                 txResult->superFrameNo, TRM_GetCurrentFrame(),
                 (unsigned long)txResult->totalUsers,
                 (unsigned long)txResult->userCount,
                 (unsigned long)txResult->remainingQueue);

    if ((txResult->users == NULL) || (txResult->userCount == 0U)) {
        return;
    }

    logUserCount = txResult->userCount;
    if (logUserCount > SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS) {
        logUserCount = SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS;
    }
    for (i = 0U; i < logUserCount; i++) {
        const TRM_TxUserResult* userResult = &txResult->users[i];

        TRM_LOG_INFO("SAT APP TX[%lu]: id=0x%08lX result=%s(%u)",
                     (unsigned long)i, (unsigned long)userResult->userId,
                     SatPayloadTxResultName(userResult->result),
                     (unsigned int)userResult->result);
    }
    if (logUserCount < txResult->userCount) {
        TRM_LOG_INFO("SAT APP TX callback: %lu additional results omitted",
                     (unsigned long)(txResult->userCount - logUserCount));
    }
}

static SatPayloadResult SatPayloadStopHal(void)
{
    if (g_satPayload.halActive == 0U) {
        return SAT_PAYLOAD_OK;
    }

    (void)TRM_StopFrequencySweep();
    if (TK8710HalReset() != TK8710_HAL_OK) {
        return SAT_PAYLOAD_ERR_DRIVER;
    }
    g_satPayload.halActive = 0U;
    return SAT_PAYLOAD_OK;
}

static SatPayloadResult SatPayloadStartMode(const SatPayloadWorkParams* params,
                                            uint8_t mode)
{
    TK8710HalInitCfg halCfg;
    slotCfg_t slotCfg;
    TRM_AcmCalibRequest acmRequest;
    TxToneConfig tone;
    int trmRet;

    g_satRfConfig.Freq = params->centerFreqHz;
    g_satRfConfig.rxgain = params->rxGain;
    g_satRfConfig.txgain = params->txGain;
    g_satChipConfig.ant_en = params->antennaMask;
    g_satChipConfig.rf_sel = params->rfMask;
    g_satChipConfig.tx_bcn_en = params->txBcnAntennaMask;
    g_satChipConfig.bcnbits = params->bcnBits;

    (void)memset(&halCfg, 0, sizeof(halCfg));
    halCfg.chipInitCfg = &g_satChipConfig;
    halCfg.trmCfg.beamMaxUsers = SAT_PAYLOAD_BEAM_MAX_USERS;
    halCfg.trmCfg.beamTimeoutMs = SAT_PAYLOAD_BEAM_TIMEOUT_MS;
    halCfg.trmCfg.maxFrameCount = params->maxFrameCount;
    halCfg.trmCfg.nodeRole = TRM_NODE_ROLE_SAT_PAYLOAD;
    halCfg.trmCfg.groundStationBeamMax = SAT_PAYLOAD_GS_BEAM_MAX;
    halCfg.trmCfg.groundStationBeamTimeoutMs = SAT_PAYLOAD_GS_BEAM_TIMEOUT_MS;
    halCfg.trmCfg.satelliteUplinkCacheSize = SAT_PAYLOAD_UPLINK_CACHE_SIZE;
    halCfg.trmCfg.onRxData = SatPayloadOnRxData;
    halCfg.trmCfg.onTxComplete = SatPayloadOnTxComplete;

    if (TK8710HalInit(&halCfg) != TK8710_HAL_OK) {
        (void)TK8710HalReset();
        return SAT_PAYLOAD_ERR_DRIVER;
    }
    g_satPayload.halActive = 1U;

    SatPayloadBuildSlotConfig(params, &slotCfg);
    if (TK8710HalCfg(&slotCfg) != TK8710_HAL_OK) {
        return SAT_PAYLOAD_ERR_DRIVER;
    }

    if (mode == SAT_PAYLOAD_MODE_TONE) {
        tone.freq = params->toneFreq;
        tone.gain = params->toneGain;
        if (TK8710DebugCtrl(TK8710_DBG_TYPE_TX_TONE,
                            TK8710_DBG_OPT_SET, &tone, NULL) != TK8710_OK) {
            return SAT_PAYLOAD_ERR_DRIVER;
        }
        g_satPayload.state = SAT_PAYLOAD_STATE_TONE;
        return SAT_PAYLOAD_OK;
    }

    if (TK8710HalStart() != TK8710_HAL_OK) {
        return SAT_PAYLOAD_ERR_DRIVER;
    }

    switch (mode) {
        case SAT_PAYLOAD_MODE_A:
        case SAT_PAYLOAD_MODE_B:
        case SAT_PAYLOAD_MODE_C:
            g_satPayload.state = SAT_PAYLOAD_STATE_RUNNING;
            return SAT_PAYLOAD_OK;

        case SAT_PAYLOAD_MODE_SWEEP:
            trmRet = TRM_StartFrequencySweep(params->sweepStartFreqHz,
                                             params->sweepEndFreqHz,
                                             params->sweepMode,
                                             params->rates[0].rateMode);
            if (trmRet != TRM_OK) {
                return SAT_PAYLOAD_ERR_DRIVER;
            }
            g_satPayload.state = SAT_PAYLOAD_STATE_SWEEP;
            return SAT_PAYLOAD_OK;

        case SAT_PAYLOAD_MODE_ANT_CAL:
            (void)memset(&acmRequest, 0, sizeof(acmRequest));
            acmRequest.calibCount = params->acmCalibCount;
            acmRequest.snrThreshold = params->acmSnrThreshold;
            if (TRM_RequestAcmCalibration(&acmRequest) != TRM_OK) {
                return SAT_PAYLOAD_ERR_DRIVER;
            }
            g_satPayload.lastCalibrationRequestMs = TK8710GetTickMs();
            g_satPayload.state = SAT_PAYLOAD_STATE_CALIBRATING;
            return SAT_PAYLOAD_OK;

        case SAT_PAYLOAD_MODE_CAPTURE:
            if (TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA,
                                TK8710_DBG_OPT_GET, NULL, NULL) != TK8710_OK) {
                return SAT_PAYLOAD_ERR_DRIVER;
            }
            g_satPayload.lastCaptureMs = TK8710GetTickMs();
            g_satPayload.state = SAT_PAYLOAD_STATE_CAPTURING;
            return SAT_PAYLOAD_OK;

        default:
            return SAT_PAYLOAD_ERR_PARAM;
    }
}

static SatPayloadResult SatPayloadStartModeWithRollback(uint8_t mode)
{
    SatPayloadWorkParams oldParams;
    uint8_t oldMode = g_satPayload.activeMode;
    uint8_t oldValid = g_satPayload.activeValid;
    SatPayloadResult result;

    if ((mode > SAT_PAYLOAD_MODE_CAPTURE) || (g_satPayload.pendingValid == 0U)) {
        return SAT_PAYLOAD_ERR_STATE;
    }

    if (((mode == SAT_PAYLOAD_MODE_SWEEP) ||
         (mode == SAT_PAYLOAD_MODE_CAPTURE)) &&
        (TK8710PortCaptureWrite("capture", NULL, 0U) != 0)) {
        return SAT_PAYLOAD_ERR_UNSUPPORTED;
    }

    oldParams = g_satPayload.activeParams;
    g_satPayload.state = SAT_PAYLOAD_STATE_CONFIGURING;

    result = SatPayloadStopHal();
    if (result == SAT_PAYLOAD_OK) {
        result = SatPayloadStartMode(&g_satPayload.pendingParams, mode);
    }

    if (result == SAT_PAYLOAD_OK) {
        g_satPayload.activeParams = g_satPayload.pendingParams;
        g_satPayload.activeMode = mode;
        g_satPayload.activeValid = 1U;
        g_satPayload.configVersion++;
        return SAT_PAYLOAD_OK;
    }

    (void)SatPayloadStopHal();
    if (oldValid != 0U) {
        g_satPayload.state = SAT_PAYLOAD_STATE_RECOVERING;
        if (SatPayloadStartMode(&oldParams, oldMode) == SAT_PAYLOAD_OK) {
            g_satPayload.activeParams = oldParams;
            g_satPayload.activeMode = oldMode;
            g_satPayload.activeValid = 1U;
            return result;
        }
        g_satPayload.state = SAT_PAYLOAD_STATE_FAULT;
        g_satPayload.activeValid = 0U;
        g_satPayload.activeMode = SAT_PAYLOAD_MODE_NONE;
        return SAT_PAYLOAD_ERR_ROLLBACK;
    }

    g_satPayload.state = SAT_PAYLOAD_STATE_CONTROL_READY;
    g_satPayload.activeMode = SAT_PAYLOAD_MODE_NONE;
    g_satPayload.activeValid = 0U;
    return result;
}

static void SatPayloadUpdateTelemetry(void)
{
    SatPayloadTelemetry next;
    TK8710Tms570Stats portStats;
    TRM_Stats trmStats;
    TRM_AcmCalibStatus acmStatus;
    uint32_t now = TK8710GetTickMs();

    (void)memset(&next, 0, sizeof(next));
    (void)memset(&portStats, 0, sizeof(portStats));
    (void)memset(&trmStats, 0, sizeof(trmStats));
    (void)memset(&acmStatus, 0, sizeof(acmStatus));

    TK8710Tms570GetStats(&portStats);
    next.sequence = g_satPayload.telemetry.sequence + 1U;
    next.uptimeMs = now - g_satPayload.startMs;
    next.state = g_satPayload.state;
    next.activeMode = g_satPayload.activeMode;
    next.pendingParamsValid = g_satPayload.pendingValid;
    next.lastResult = g_satPayload.lastResult;
    next.configVersion = g_satPayload.configVersion;
    next.activeParams = g_satPayload.activeParams;
    next.heapUsed = portStats.heap_used;
    next.heapPeak = portStats.heap_peak;
    next.heapFailCount = portStats.heap_fail_count;
    next.spiErrorCount = portStats.spi_error_count;
    next.gpioIrqCount = portStats.irq_count;
    TK8710GetAllIrqCounters(next.irqCounters);
    next.irqStatus = TK8710GetIrqStatus();

    if ((g_satPayload.halActive != 0U) &&
        (TK8710HalGetStatus(&trmStats) == TK8710_HAL_OK)) {
        next.txCount = trmStats.txCount;
        next.txSuccessCount = trmStats.txSuccessCount;
        next.rxCount = trmStats.rxCount;
        next.beamCount = trmStats.beamCount;
        next.txQueueRemaining = trmStats.txQueueRemaining;
        next.satelliteCacheCount = trmStats.satelliteCacheCount;
        next.satelliteGroundStationBeamCount =
            trmStats.satelliteGroundStationBeamCount;
        next.satelliteRouteCount = trmStats.satelliteRouteCount;
        next.satelliteBeamMissCount = trmStats.satelliteBeamMissCount;
        if (TRM_GetAcmCalibrationStatus(&acmStatus) == TRM_OK) {
            next.acmPending = acmStatus.pending;
            next.acmRunning = acmStatus.running;
            next.acmCompletedCount = acmStatus.completedCount;
            next.acmLastResult = acmStatus.lastResult;
        }
    }

    TK8710EnterCritical();
    next.lastRx = g_satPayload.lastRx;
    g_satPayload.telemetry = next;
    TK8710ExitCritical();
}

void SatPayloadApp_Init(void)
{
    (void)memset(&g_satPayload, 0, sizeof(g_satPayload));
    g_satPayload.state = SAT_PAYLOAD_STATE_CONTROL_READY;
    g_satPayload.activeMode = SAT_PAYLOAD_MODE_NONE;
    g_satPayload.startMs = TK8710GetTickMs();
    g_satPayload.lastTelemetryMs = g_satPayload.startMs;
    g_satPayload.lastResult = SAT_PAYLOAD_OK;
    SatPayloadUpdateTelemetry();
}

void SatPayloadApp_Process(void)
{
    uint32_t now = TK8710GetTickMs();

    if (g_satPayload.state == SAT_PAYLOAD_STATE_CALIBRATING) {
        TRM_AcmCalibStatus status;
        if ((TRM_GetAcmCalibrationStatus(&status) == TRM_OK) &&
            (status.pending == 0U) && (status.running == 0U) &&
            ((now - g_satPayload.lastCalibrationRequestMs) >=
             SAT_PAYLOAD_CALIBRATION_PERIOD_MS)) {
            TRM_AcmCalibRequest request;
            (void)memset(&request, 0, sizeof(request));
            request.calibCount = g_satPayload.activeParams.acmCalibCount;
            request.snrThreshold = g_satPayload.activeParams.acmSnrThreshold;
            if (TRM_RequestAcmCalibration(&request) == TRM_OK) {
                g_satPayload.lastCalibrationRequestMs = now;
            }
        }
    }

    if ((g_satPayload.state == SAT_PAYLOAD_STATE_CAPTURING) &&
        ((now - g_satPayload.lastCaptureMs) >= SAT_PAYLOAD_CAPTURE_PERIOD_MS)) {
        if (TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA,
                            TK8710_DBG_OPT_GET, NULL, NULL) == TK8710_OK) {
            g_satPayload.lastCaptureMs = now;
        } else {
            g_satPayload.lastResult = SAT_PAYLOAD_ERR_DRIVER;
        }
    }

    if ((now - g_satPayload.lastTelemetryMs) >=
        SAT_PAYLOAD_TELEMETRY_PERIOD_MS) {
        g_satPayload.lastTelemetryMs = now;
        SatPayloadUpdateTelemetry();
    }
}

SatPayloadResult SatPayloadApp_SetWorkParams(const SatPayloadWorkParams* params)
{
    SatPayloadResult result = SatPayloadValidateParams(params);
    if (result == SAT_PAYLOAD_OK) {
        g_satPayload.pendingParams = *params;
        g_satPayload.pendingValid = 1U;
    }
    g_satPayload.lastResult = result;
    return result;
}

SatPayloadResult SatPayloadApp_SetWorkMode(uint8_t mode)
{
    SatPayloadResult result = SatPayloadStartModeWithRollback(mode);
    g_satPayload.lastResult = result;
    SatPayloadUpdateTelemetry();
    return result;
}

SatPayloadResult SatPayloadApp_Stop(void)
{
    SatPayloadResult result = SatPayloadStopHal();
    if (result == SAT_PAYLOAD_OK) {
        g_satPayload.state = SAT_PAYLOAD_STATE_CONTROL_READY;
        g_satPayload.activeMode = SAT_PAYLOAD_MODE_NONE;
        g_satPayload.activeValid = 0U;
    } else {
        g_satPayload.state = SAT_PAYLOAD_STATE_FAULT;
    }
    g_satPayload.lastResult = result;
    SatPayloadUpdateTelemetry();
    return result;
}

SatPayloadResult SatPayloadApp_HandleTelecommand(
    const SatPayloadTelecommand* request,
    SatPayloadTelecommandResponse* response)
{
    SatPayloadResult result;
    uint32_t value = 0U;
    SatPayloadState before;
    int driverResult;

    if ((request == NULL) || (response == NULL)) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    before = g_satPayload.state;
    switch (request->commandId) {
        case SAT_PAYLOAD_TC_SET_WORK_PARAMS:
            result = SatPayloadApp_SetWorkParams(&request->payload.workParams);
            break;
        case SAT_PAYLOAD_TC_SET_WORK_MODE:
            result = SatPayloadApp_SetWorkMode(request->payload.workMode);
            break;
        case SAT_PAYLOAD_TC_STOP:
            result = SatPayloadApp_Stop();
            break;
        case SAT_PAYLOAD_TC_RESET_8710:
            result = SatPayloadApp_Stop();
            if ((result == SAT_PAYLOAD_OK) &&
                (TK8710SpiReset(TK8710_RST_SM_AND_REG) != 0)) {
                result = SAT_PAYLOAD_ERR_DRIVER;
            }
            break;
        case SAT_PAYLOAD_TC_REQUEST_ACM:
            result = (g_satPayload.halActive != 0U) &&
                     (TRM_RequestAcmCalibration(NULL) == TRM_OK) ?
                     SAT_PAYLOAD_ACCEPTED : SAT_PAYLOAD_ERR_STATE;
            break;
        case SAT_PAYLOAD_TC_READ_REG:
            driverResult = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                                         request->payload.reg.address, &value);
            result = (driverResult == TK8710_OK) ?
                     SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
            break;
        case SAT_PAYLOAD_TC_WRITE_REG:
            driverResult = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                                          request->payload.reg.address,
                                          request->payload.reg.value);
            result = (driverResult == TK8710_OK) ?
                     SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
            break;
        case SAT_PAYLOAD_TC_READ_RF_REG:
            driverResult = tk8710_rf_read(request->payload.reg.rfMask,
                                          request->payload.reg.address, &value);
            result = (driverResult == TK8710_OK) ?
                     SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
            break;
        case SAT_PAYLOAD_TC_WRITE_RF_REG:
            driverResult = tk8710_rf_write(request->payload.reg.rfMask,
                                           request->payload.reg.address,
                                           request->payload.reg.value);
            result = (driverResult == TK8710_OK) ?
                     SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
            break;
        case SAT_PAYLOAD_TC_SELECT_PAYLOAD:
        case SAT_PAYLOAD_TC_SYSTEM_RESET:
        case SAT_PAYLOAD_TC_FIRMWARE_UPGRADE:
            result = SAT_PAYLOAD_ERR_UNSUPPORTED;
            break;
        default:
            result = SAT_PAYLOAD_ERR_PARAM;
            break;
    }

    g_satPayload.lastResult = result;
    response->requestId = request->requestId;
    response->commandId = request->commandId;
    response->result = result;
    response->stateBefore = before;
    response->stateAfter = g_satPayload.state;
    response->value = value;
    return result;
}

void SatPayloadApp_GetTelemetry(SatPayloadTelemetry* telemetry)
{
    if (telemetry == NULL) {
        return;
    }
    TK8710EnterCritical();
    *telemetry = g_satPayload.telemetry;
    TK8710ExitCritical();
}

SatPayloadState SatPayloadApp_GetState(void)
{
    return g_satPayload.state;
}

uint8_t SatPayloadApp_GetActiveMode(void)
{
    return g_satPayload.activeMode;
}

const char* SatPayloadApp_StateName(SatPayloadState state)
{
    static const char* const names[] = {
        "BOOT", "SELF_TEST", "CONTROL_READY", "CONFIGURING", "RUNNING",
        "SWEEP", "TONE", "CALIBRATING", "CAPTURING", "RECOVERING", "FAULT"
    };
    if ((uint32_t)state >= (uint32_t)(sizeof(names) / sizeof(names[0]))) {
        return "UNKNOWN";
    }
    return names[(uint32_t)state];
}
