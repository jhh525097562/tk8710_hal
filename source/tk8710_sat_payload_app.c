#include "tk8710_sat_payload_app.h"

#include <stddef.h>
#include <string.h>

#include "adc_telemetry.h"
#include "app_faults.h"
#include "data_transfer.h"
#include "hal_api.h"
#include "tk8710_tms570.h"
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_platform.h"
#include "driver/tk8710_reg_pack.h"
#include "driver/tk8710_regs.h"
#include "trm/trm_api.h"
#include "trm/trm_log.h"

#define SAT_PAYLOAD_TELEMETRY_PERIOD_MS       1000U
#define SAT_PAYLOAD_CALIBRATION_S0_INTERVAL     10U
#define SAT_PAYLOAD_MODE_C_ACM_PERIOD_MS     600000U
#define SAT_PAYLOAD_BEAM_MAX_USERS            512U
#define SAT_PAYLOAD_BEAM_TIMEOUT_MS            20000U
#define SAT_PAYLOAD_GS_BEAM_MAX                5U
#define SAT_PAYLOAD_GS_BEAM_TIMEOUT_MS         65000U
#define SAT_PAYLOAD_UPLINK_CACHE_SIZE          128U
#define SAT_PAYLOAD_CALLBACK_LOG_MAX_USERS     16U
#define SAT_PAYLOAD_CALLBACK_DATA_PREFIX       8U
#define SAT_PAYLOAD_HW_RESERVED_USER_SLOTS     1U
#define SAT_PAYLOAD_HW_RESERVED_FREQ_OFFSET    20000.0F
#define SAT_PAYLOAD_SLOT_BLOCK_BYTES            26U
#define SAT_PAYLOAD_SLOT_MODE18_BLOCK_BYTES     40U
#define SAT_PAYLOAD_SLOT_MAX_BLOCKS             16U
#define SAT_PAYLOAD_SLOT_MAX_SUPER_FRAMES       64U
#define SAT_PAYLOAD_SWEEP_START_FREQ_HZ    504000000U
#define SAT_PAYLOAD_SWEEP_END_FREQ_HZ      508000000U
#define SAT_PAYLOAD_SWEEP_RATE_MODE_BASE           5U
#define SAT_PAYLOAD_TX_FE_ANTENNA_STRIDE       0x1000U
#define SAT_PAYLOAD_CAPTURE_STORE_CHUNK_BYTES     1024U
#define SAT_PAYLOAD_STORE_MAX_RETRIES                3U
#define SAT_PAYLOAD_SWEEP_STORE_POINTS               8U

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
    uint32_t lastCalibrationS0Count;
    uint32_t lastModeCAcmRequestMs;
    uint32_t lastCaptureGeneration;
    uint32_t captureStoreGeneration;
    uint32_t captureStoreBytesPerAntenna;
    uint32_t captureStoreOffset;
    uint32_t lastSweepStoredGeneration;
    uint32_t sweepStoreGeneration;
    uint8_t captureStorePending;
    uint8_t captureStoreAntenna;
    uint8_t captureStoreRateMode;
    uint8_t captureStoreRetryCount;
    uint8_t sweepStoreRetryCount;
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

static uint8_t SatPayloadSlotBlockBytes(uint8_t rateMode)
{
    return (rateMode == 18U) ? SAT_PAYLOAD_SLOT_MODE18_BLOCK_BYTES :
                               SAT_PAYLOAD_SLOT_BLOCK_BYTES;
}

static SatPayloadResult SatPayloadSlotBlockCount(uint8_t rateMode,
                                                  uint16_t byteLen,
                                                  uint8_t* blockCount)
{
    uint32_t blockBytes;
    uint32_t blocks;

    if (blockCount == NULL) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    blockBytes = SatPayloadSlotBlockBytes(rateMode);
    blocks = ((uint32_t)byteLen + blockBytes - 1U) / blockBytes;
    if (blocks > SAT_PAYLOAD_SLOT_MAX_BLOCKS) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    *blockCount = (uint8_t)blocks;
    return SAT_PAYLOAD_OK;
}

static SatPayloadResult SatPayloadValidateParams(const SatPayloadWorkParams* params)
{
    uint32_t rate;
    uint32_t slot;

    if ((params == NULL) || (params->centerFreqHz == 0U) ||
        (params->rateCount == 0U) ||
        (params->rateCount > SAT_PAYLOAD_MAX_RATES) ||
        (params->bcnBits > 31U) ||
        (params->maxFrameCount == 0U) ||
        (params->maxFrameCount > SAT_PAYLOAD_SLOT_MAX_SUPER_FRAMES)) {
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
        for (slot = 1U; slot < SAT_PAYLOAD_SLOT_COUNT; slot++) {
            uint8_t blockCount;

            if (SatPayloadSlotBlockCount(params->rates[rate].rateMode,
                                         params->rates[rate].slots[slot].byteLen,
                                         &blockCount) != SAT_PAYLOAD_OK) {
                return SAT_PAYLOAD_ERR_PARAM;
            }
        }
    }

    if ((params->sweepMode > 3U) ||
        ((params->sweepStartFreqHz == 0U) !=
         (params->sweepEndFreqHz == 0U)) ||
        ((params->sweepStartFreqHz != 0U) &&
         (params->sweepEndFreqHz < params->sweepStartFreqHz))) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    return SAT_PAYLOAD_OK;
}

static SatPayloadResult SatPayloadBuildSlotConfig(SatPayloadWorkParams* params,
                                                  slotCfg_t* slotCfg)
{
    TRM_MultiRateSlotCalcInput calcInput;
    TRM_MultiRateSlotCalcOutput calcOutput;
    uint32_t rate;
    uint32_t antenna;

    if ((params == NULL) || (slotCfg == NULL)) {
        return SAT_PAYLOAD_ERR_PARAM;
    }

    (void)memset(slotCfg, 0, sizeof(*slotCfg));
    (void)memset(&calcInput, 0, sizeof(calcInput));
    (void)memset(&calcOutput, 0, sizeof(calcOutput));
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

    calcInput.rateCount = params->rateCount;
    calcInput.superFrameNum = (uint8_t)params->maxFrameCount;
    calcInput.calcType = TRM_SLOT_CALC_TYPE_SATELLITE;
    calcInput.minGapPos[3] = 1U;

    for (antenna = 0U; antenna < TK8710_MAX_ANTENNAS; antenna++) {
        slotCfg->bcnRotation[antenna] = (uint8_t)antenna;
    }

    for (rate = 0U; rate < params->rateCount; rate++) {
        SatPayloadResult result;

        slotCfg->rateModes[rate] = (rateMode_e)params->rates[rate].rateMode;
        calcInput.rateModes[rate] = params->rates[rate].rateMode;

        result = SatPayloadSlotBlockCount(params->rates[rate].rateMode,
                                          params->rates[rate].slots[1].byteLen,
                                          &calcInput.brdBlockNums[rate]);
        if (result != SAT_PAYLOAD_OK) {
            return result;
        }
        result = SatPayloadSlotBlockCount(params->rates[rate].rateMode,
                                          params->rates[rate].slots[2].byteLen,
                                          &calcInput.ulBlockNums[rate]);
        if (result != SAT_PAYLOAD_OK) {
            return result;
        }
        result = SatPayloadSlotBlockCount(params->rates[rate].rateMode,
                                          params->rates[rate].slots[3].byteLen,
                                          &calcInput.dlBlockNums[rate]);
        if (result != SAT_PAYLOAD_OK) {
            return result;
        }

        slotCfg->s0Cfg[rate].byteLen = params->rates[rate].slots[0].byteLen;
        slotCfg->s0Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s1Cfg[rate].byteLen = params->rates[rate].slots[1].byteLen;
        slotCfg->s1Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s2Cfg[rate].byteLen = params->rates[rate].slots[2].byteLen;
        slotCfg->s2Cfg[rate].centerFreq = params->centerFreqHz;

        slotCfg->s3Cfg[rate].byteLen = params->rates[rate].slots[3].byteLen;
        slotCfg->s3Cfg[rate].centerFreq = params->centerFreqHz;
    }

    if (trm_calc_multi_rate_slot_config(&calcInput, &calcOutput) != 0) {
        TRM_LOG_ERROR("SAT APP slot calculation failed");
        return SAT_PAYLOAD_ERR_DRIVER;
    }

    for (rate = 0U; rate < params->rateCount; rate++) {
        const TRM_RateSlotConfig* calculated = &calcOutput.rateConfigs[rate];

        slotCfg->s0Cfg[rate].da_m = calculated->bcnGap;
        slotCfg->s1Cfg[rate].da_m = calculated->brdGap;
        slotCfg->s2Cfg[rate].da_m = calculated->ulGap;
        slotCfg->s3Cfg[rate].da_m = calculated->dlGap;

        params->rates[rate].slots[0].daM = calculated->bcnGap;
        params->rates[rate].slots[1].daM = calculated->brdGap;
        params->rates[rate].slots[2].daM = calculated->ulGap;
        params->rates[rate].slots[3].daM = calculated->dlGap;

        TRM_LOG_INFO("SAT APP slot[%u]: mode=%u blocks=%u/%u/%u da_m=%lu/%lu/%lu/%lu",
                     (unsigned int)rate,
                     (unsigned int)params->rates[rate].rateMode,
                     (unsigned int)calcInput.brdBlockNums[rate],
                     (unsigned int)calcInput.ulBlockNums[rate],
                     (unsigned int)calcInput.dlBlockNums[rate],
                     (unsigned long)calculated->bcnGap,
                     (unsigned long)calculated->brdGap,
                     (unsigned long)calculated->ulGap,
                     (unsigned long)calculated->dlGap);
    }

    TRM_LOG_INFO("SAT APP slot period: raw=%lu adjusted=%lu frames=%lu addedGap=%lu",
                 (unsigned long)calcOutput.totalRawPeriod,
                 (unsigned long)calcOutput.framePeriod,
                 (unsigned long)calcOutput.frameCount,
                 (unsigned long)calcOutput.addedGap);
    return SAT_PAYLOAD_OK;
}

static void SatPayloadOnRxData(const TRM_RxDataList* rxDataList)
{
    const TRM_RxUserData* user;
    DataTransferFirstUserInfo transferInfo;
    int64_t frequencyHz;
    uint32_t logUserCount;
    uint32_t i;

    if ((rxDataList == NULL) || (rxDataList->users == NULL) ||
        (rxDataList->userCount == 0U)) {
        return;
    }

    /*
     * TRM only puts CRC/data-valid users in this callback.  Still require a
     * successfully obtained payload so a failed buffer read cannot replace
     * the last good telemetry snapshot.
     */
    user = NULL;
    for (i = 0U; i < rxDataList->userCount; i++) {
        if ((rxDataList->users[i].data != NULL) &&
            (rxDataList->users[i].dataLen != 0U)) {
            user = &rxDataList->users[i];
            break;
        }
    }
    if (user == NULL) {
        TRM_LOG_WARN("SAT APP RX: no readable CRC-valid user to publish");
        return;
    }

    frequencyHz = (int64_t)g_satPayload.activeParams.centerFreqHz +
                  ((int64_t)user->freq / 128);
    TK8710EnterCritical();
    g_satPayload.lastRx.valid = 1U;
    g_satPayload.lastRx.generation++;
    g_satPayload.lastRx.userId = user->userId;
    g_satPayload.lastRx.rateMode = user->rateMode;
    g_satPayload.lastRx.rssi = user->rssi;
    g_satPayload.lastRx.snr = user->snr;
    g_satPayload.lastRx.freqOffset = user->freq;
    g_satPayload.lastRx.frequencyHz =
        (frequencyHz > 0) ? (uint32_t)frequencyHz : 0U;
    g_satPayload.lastRx.dataLen = user->dataLen;
    g_satPayload.lastRx.frameNo = rxDataList->frameNo;
    g_satPayload.lastRx.timestampMs = TK8710GetTickMs();
    TK8710ExitCritical();

    (void)memset(&transferInfo, 0, sizeof(transferInfo));
    transferInfo.rateMode = user->rateMode;
    transferInfo.rssi = user->rssi;
    transferInfo.snr = user->snr;
    transferInfo.dataLen = user->dataLen;
    transferInfo.frameNo = rxDataList->frameNo;
    transferInfo.userId = user->userId;
    transferInfo.freqOffset = user->freq;
    transferInfo.frequencyHz = g_satPayload.lastRx.frequencyHz;
    transferInfo.pilotPower = user->beam.pilotPower;
    (void)memcpy(transferInfo.ahData, user->beam.ahData,
                 sizeof(transferInfo.ahData));
    if (DataTransfer_AppendFirstUserInfo(&transferInfo) != 0) {
        TRM_LOG_WARN("SAT APP failed to store first valid RX user");
    }

    TRM_LOG_INFO("SAT APP first valid RX: gen=%lu user=0x%08lX freq=%luHz rssi=%d snr=%u",
                 (unsigned long)g_satPayload.lastRx.generation,
                 (unsigned long)user->userId,
                 (unsigned long)g_satPayload.lastRx.frequencyHz,
                 (int)user->rssi, (unsigned int)user->snr);

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

static void SatPayloadBeginCaptureStore(const TK8710CaptureInfo* info)
{
    if ((info == NULL) || (info->bytesPerAntenna == 0U) ||
        (info->validAntennaMask != 0xFFU)) {
        TRM_LOG_WARN("SAT APP capture generation %lu not stored: bytes=%lu mask=0x%02X",
                     (unsigned long)((info != NULL) ? info->generation : 0U),
                     (unsigned long)((info != NULL) ? info->bytesPerAntenna : 0U),
                     (unsigned int)((info != NULL) ? info->validAntennaMask : 0U));
        return;
    }
    g_satPayload.captureStoreGeneration = info->generation;
    g_satPayload.captureStoreBytesPerAntenna = info->bytesPerAntenna;
    g_satPayload.captureStoreOffset = 0U;
    g_satPayload.captureStoreAntenna = 0U;
    g_satPayload.captureStoreRateMode = info->rateMode;
    g_satPayload.captureStoreRetryCount = 0U;
    g_satPayload.captureStorePending = 1U;
}

static void SatPayloadProcessCaptureStore(void)
{
    uint8_t data[SAT_PAYLOAD_CAPTURE_STORE_CHUNK_BYTES];
    DataTransferCaptureChunk chunk;
    uint32_t remaining;
    uint16_t length;
    int readResult;
    int appendResult;

    if (g_satPayload.captureStorePending == 0U) {
        return;
    }
    remaining = g_satPayload.captureStoreBytesPerAntenna -
                g_satPayload.captureStoreOffset;
    length = (uint16_t)((remaining > sizeof(data)) ? sizeof(data) : remaining);
    readResult = TK8710CaptureReadAntenna(
        g_satPayload.captureStoreGeneration,
        g_satPayload.captureStoreAntenna,
        g_satPayload.captureStoreOffset, data, length);
    if (readResult == 0) {
        (void)memset(&chunk, 0, sizeof(chunk));
        chunk.rateMode = g_satPayload.captureStoreRateMode;
        chunk.antenna = g_satPayload.captureStoreAntenna;
        chunk.generation = g_satPayload.captureStoreGeneration;
        chunk.bytesPerAntenna = g_satPayload.captureStoreBytesPerAntenna;
        chunk.offset = g_satPayload.captureStoreOffset;
        chunk.data = data;
        chunk.length = length;
        appendResult = DataTransfer_AppendCaptureRawData(&chunk);
    } else {
        appendResult = -1;
    }
    if (appendResult != 0) {
        g_satPayload.captureStoreRetryCount++;
        if (g_satPayload.captureStoreRetryCount >=
            SAT_PAYLOAD_STORE_MAX_RETRIES) {
            TRM_LOG_ERROR("SAT APP capture store failed: gen=%lu ant=%u offset=%lu read=%d",
                          (unsigned long)g_satPayload.captureStoreGeneration,
                          (unsigned int)g_satPayload.captureStoreAntenna,
                          (unsigned long)g_satPayload.captureStoreOffset,
                          readResult);
            g_satPayload.captureStorePending = 0U;
            g_satPayload.lastResult = SAT_PAYLOAD_ERR_DRIVER;
        }
        return;
    }

    g_satPayload.captureStoreRetryCount = 0U;
    g_satPayload.captureStoreOffset += length;
    if (g_satPayload.captureStoreOffset >=
        g_satPayload.captureStoreBytesPerAntenna) {
        g_satPayload.captureStoreOffset = 0U;
        g_satPayload.captureStoreAntenna++;
        if (g_satPayload.captureStoreAntenna >= 8U) {
            g_satPayload.captureStorePending = 0U;
            TRM_LOG_INFO("SAT APP capture stored: gen=%lu bytesPerAntenna=%lu",
                         (unsigned long)g_satPayload.captureStoreGeneration,
                         (unsigned long)g_satPayload.captureStoreBytesPerAntenna);
        }
    }
}

static int SatPayloadStoreSweepRound(const TRM_SweepResultInfo* info)
{
    TRM_SweepState state;
    TRM_SweepResultPoint sourcePoints[SAT_PAYLOAD_SWEEP_STORE_POINTS];
    DataTransferSweepPoint transferPoints[SAT_PAYLOAD_SWEEP_STORE_POINTS];
    DataTransferSweepChunk chunk;
    uint32_t startIndex = 0U;
    uint32_t resultCount;
    uint32_t point;
    uint32_t antenna;

    if ((info == NULL) || (info->complete == 0U) ||
        (info->completedPoints != info->totalPoints) ||
        (TRM_GetSweepState(&state) != TRM_OK)) {
        return -1;
    }
    while (startIndex < info->completedPoints) {
        if (TRM_ReadSweepResults(startIndex, sourcePoints,
                                 SAT_PAYLOAD_SWEEP_STORE_POINTS,
                                 &resultCount) != TRM_OK ||
            (resultCount == 0U)) {
            return -1;
        }
        for (point = 0U; point < resultCount; point++) {
            transferPoints[point].frequencyHz =
                sourcePoints[point].frequencyHz;
            for (antenna = 0U;
                 antenna < DATA_TRANSFER_SWEEP_ANTENNA_COUNT;
                 antenna++) {
                transferPoints[point].noiseDbmHz[antenna] =
                    sourcePoints[point].noiseDbmHz[antenna];
            }
        }
        (void)memset(&chunk, 0, sizeof(chunk));
        chunk.sweepMode = state.sweep_mode;
        chunk.rateMode = state.rate_mode;
        chunk.generation = info->generation;
        chunk.startFrequencyHz = state.start_freq;
        chunk.endFrequencyHz = state.end_freq;
        chunk.stepFrequencyHz = state.step_freq;
        chunk.totalPoints = info->totalPoints;
        chunk.startIndex = startIndex;
        chunk.points = transferPoints;
        chunk.pointCount = (uint16_t)resultCount;
        if (DataTransfer_AppendSweepBackgroundNoise(&chunk) != 0) {
            return -1;
        }
        startIndex += resultCount;
    }
    return 0;
}

static SatPayloadResult SatPayloadStopHal(void)
{
    (void)TK8710CaptureCancel();
    g_satPayload.captureStorePending = 0U;
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

static SatPayloadResult SatPayloadStartMode(SatPayloadWorkParams* params,
                                            uint8_t mode)
{
    TK8710HalInitCfg halCfg;
    slotCfg_t slotCfg;
    TRM_AcmCalibRequest acmRequest;
    TxToneConfig tone;
    int trmRet;

    if (mode == SAT_PAYLOAD_MODE_SWEEP) {
        if ((params->rates[0].rateMode < 5U) ||
            (params->rates[0].rateMode > 8U)) {
            return SAT_PAYLOAD_ERR_PARAM;
        }
        params->sweepStartFreqHz = SAT_PAYLOAD_SWEEP_START_FREQ_HZ;
        params->sweepEndFreqHz = SAT_PAYLOAD_SWEEP_END_FREQ_HZ;
        params->sweepMode = (uint8_t)(params->rates[0].rateMode -
                                      SAT_PAYLOAD_SWEEP_RATE_MODE_BASE);
        TRM_LOG_INFO("SAT APP fixed sweep: start=%lu end=%lu mode=%u rate=%u",
                     (unsigned long)params->sweepStartFreqHz,
                     (unsigned long)params->sweepEndFreqHz,
                     (unsigned int)params->sweepMode,
                     (unsigned int)params->rates[0].rateMode);
    }

    g_satRfConfig.Freq = (mode == SAT_PAYLOAD_MODE_SWEEP) ?
                         params->sweepStartFreqHz : params->centerFreqHz;
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

    {
        SatPayloadResult slotResult = SatPayloadBuildSlotConfig(params, &slotCfg);
        if (slotResult != SAT_PAYLOAD_OK) {
            return slotResult;
        }
    }
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
            g_satPayload.state = SAT_PAYLOAD_STATE_RUNNING;
            return SAT_PAYLOAD_OK;

        case SAT_PAYLOAD_MODE_C:
            g_satPayload.lastModeCAcmRequestMs = TK8710GetTickMs();
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
            TK8710GetS0PeriodStats(NULL, NULL,
                                   &g_satPayload.lastCalibrationS0Count);
            g_satPayload.state = SAT_PAYLOAD_STATE_CALIBRATING;
            return SAT_PAYLOAD_OK;

        case SAT_PAYLOAD_MODE_CAPTURE:
        {
            TK8710CaptureInfo captureInfo;
            if (TK8710CaptureGetInfo(&captureInfo) != 0) {
                return SAT_PAYLOAD_ERR_DRIVER;
            }
            g_satPayload.lastCaptureGeneration = captureInfo.generation;
            if (TK8710CaptureRequest(params->rates[0].rateMode) != 0) {
                return SAT_PAYLOAD_ERR_DRIVER;
            }
            g_satPayload.state = SAT_PAYLOAD_STATE_CAPTURING;
            return SAT_PAYLOAD_OK;
        }

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
    TRM_AcmCalibResult acmResult;
    TK8710CaptureInfo captureInfo;
    TRM_SweepResultInfo sweepInfo;
    AdcTelemetryValues adcValues;
    uint32_t now = TK8710GetTickMs();

    (void)memset(&next, 0, sizeof(next));
    (void)memset(&portStats, 0, sizeof(portStats));
    (void)memset(&trmStats, 0, sizeof(trmStats));
    (void)memset(&acmStatus, 0, sizeof(acmStatus));
    (void)memset(&acmResult, 0, sizeof(acmResult));
    (void)memset(&captureInfo, 0, sizeof(captureInfo));
    (void)memset(&sweepInfo, 0, sizeof(sweepInfo));
    (void)memset(&adcValues, 0, sizeof(adcValues));

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
    next.gpioIrqEdgeCount = portStats.irq_edge_count;
    next.gpioIrqRecoveryCount = portStats.irq_level_recovery_count;
    next.irqStatusPollCount = portStats.irq_status_poll_count;
    next.spiResetCount = portStats.spi_reset_count;
    next.resetDriveLowCount = portStats.reset_drive_low_count;
    next.resetPinLowCount = portStats.reset_pin_low_count;
    next.portInitCount = portStats.port_init_count;
    next.spiInitCount = portStats.spi_init_count;
    next.resetGioDout = portStats.reset_gio_dout;
    next.resetGioDir = portStats.reset_gio_dir;
    next.gpioDin = portStats.gio_din;
    next.gpioFlag = portStats.gio_flg;
    next.gpioEnable = portStats.gio_enaset;
    next.vimReqMask0 = portStats.vim_reqmask0;
    next.gpioIrqLevel = portStats.irq_pin_level;
    next.resetPinLevel = portStats.reset_pin_level;
    next.gpioIrqCallbackConfigured = portStats.irq_callback_configured;
    next.sdramAvailable = portStats.sdram_available;
    TK8710GetAllIrqCounters(next.irqCounters);
    next.irqStatus = TK8710GetIrqStatus();
    (void)TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        MAC_BASE + offsetof(struct mac, irq_ctrl0),
                        &next.irqMask);
    if (TK8710CaptureGetInfo(&captureInfo) == 0) {
        next.capture = captureInfo;
    }
    if (TRM_GetSweepResultInfo(&sweepInfo) == TRM_OK) {
        next.sweep = sweepInfo;
    }
    if (AdcTelemetry_Read(&adcValues) == 0) {
        next.adcBasebandTempC = adcValues.basebandTempC;
        next.adcRfTempC = adcValues.rfTempC;
        next.adcRf3v3Mv = adcValues.rf3v3Mv;
        next.adcRf1v2Mv = adcValues.rf1v2Mv;
    } else {
        AppFaults_Set(APP_FAULT_ADC1);
        AppFaults_Set(APP_FAULT_ADC2);
        AppFaults_Set(APP_FAULT_ADC3);
        AppFaults_Set(APP_FAULT_ADC4);
    }

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
        if (TRM_GetAcmCalibrationResult(&acmResult) == TRM_OK) {
            next.acmResult = acmResult;
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
    AdcTelemetry_Init();
    SatPayloadUpdateTelemetry();
}

void SatPayloadApp_Process(void)
{
    uint32_t now = TK8710GetTickMs();
    TK8710CaptureInfo captureInfo;
    TRM_SweepResultInfo sweepInfo;

    TRM_ProcessBackground();

    if (g_satPayload.state == SAT_PAYLOAD_STATE_CALIBRATING) {
        TRM_AcmCalibStatus status;
        uint32_t s0Count = 0U;
        TK8710GetS0PeriodStats(NULL, NULL, &s0Count);
        if ((TRM_GetAcmCalibrationStatus(&status) == TRM_OK) &&
            (status.pending == 0U) && (status.running == 0U) &&
            ((s0Count - g_satPayload.lastCalibrationS0Count) >=
             SAT_PAYLOAD_CALIBRATION_S0_INTERVAL)) {
            TRM_AcmCalibRequest request;
            (void)memset(&request, 0, sizeof(request));
            request.calibCount = g_satPayload.activeParams.acmCalibCount;
            request.snrThreshold = g_satPayload.activeParams.acmSnrThreshold;
            if (TRM_RequestAcmCalibration(&request) == TRM_OK) {
                g_satPayload.lastCalibrationS0Count = s0Count;
            }
        }
    }

    if ((g_satPayload.state == SAT_PAYLOAD_STATE_RUNNING) &&
        (g_satPayload.activeMode == SAT_PAYLOAD_MODE_C) &&
        ((now - g_satPayload.lastModeCAcmRequestMs) >=
         SAT_PAYLOAD_MODE_C_ACM_PERIOD_MS)) {
        TRM_AcmCalibStatus status;

        if ((TRM_GetAcmCalibrationStatus(&status) == TRM_OK) &&
            (status.pending == 0U) && (status.running == 0U)) {
            TRM_AcmCalibRequest request;

            (void)memset(&request, 0, sizeof(request));
            request.calibCount = g_satPayload.activeParams.acmCalibCount;
            request.snrThreshold = g_satPayload.activeParams.acmSnrThreshold;
            if (TRM_RequestAcmCalibration(&request) == TRM_OK) {
                g_satPayload.lastModeCAcmRequestMs = now;
                TRM_LOG_INFO("SAT APP mode C periodic ACM requested at %lu ms",
                             (unsigned long)now);
            }
        }
    }

    if ((g_satPayload.state == SAT_PAYLOAD_STATE_CAPTURING) &&
        (TK8710CaptureGetInfo(&captureInfo) == 0)) {
        if (captureInfo.generation != g_satPayload.lastCaptureGeneration) {
            g_satPayload.lastCaptureGeneration = captureInfo.generation;
            g_satPayload.lastResult = SAT_PAYLOAD_OK;
            SatPayloadBeginCaptureStore(&captureInfo);
        }
        SatPayloadProcessCaptureStore();
        if ((captureInfo.state == TK8710_CAPTURE_STATE_READY) ||
            (captureInfo.state == TK8710_CAPTURE_STATE_ERROR) ||
            (captureInfo.state == TK8710_CAPTURE_STATE_IDLE)) {
            if ((g_satPayload.captureStorePending == 0U) &&
                (TK8710CaptureRequest(
                     g_satPayload.activeParams.rates[0].rateMode) != 0)) {
                g_satPayload.lastResult = SAT_PAYLOAD_ERR_DRIVER;
            }
        }
    }

    if ((g_satPayload.state == SAT_PAYLOAD_STATE_SWEEP) &&
        (TRM_GetSweepResultInfo(&sweepInfo) == TRM_OK) &&
        (sweepInfo.complete != 0U) &&
        (sweepInfo.generation != g_satPayload.lastSweepStoredGeneration)) {
        if (g_satPayload.sweepStoreGeneration != sweepInfo.generation) {
            g_satPayload.sweepStoreGeneration = sweepInfo.generation;
            g_satPayload.sweepStoreRetryCount = 0U;
        }
        if (SatPayloadStoreSweepRound(&sweepInfo) == 0) {
            g_satPayload.lastSweepStoredGeneration = sweepInfo.generation;
            g_satPayload.sweepStoreRetryCount = 0U;
            TRM_LOG_INFO("SAT APP sweep stored: gen=%lu points=%lu",
                         (unsigned long)sweepInfo.generation,
                         (unsigned long)sweepInfo.completedPoints);
        } else {
            g_satPayload.sweepStoreRetryCount++;
            if (g_satPayload.sweepStoreRetryCount >=
                SAT_PAYLOAD_STORE_MAX_RETRIES) {
                g_satPayload.lastSweepStoredGeneration =
                    sweepInfo.generation;
                g_satPayload.lastResult = SAT_PAYLOAD_ERR_DRIVER;
                TRM_LOG_ERROR("SAT APP sweep store failed: gen=%lu points=%lu",
                              (unsigned long)sweepInfo.generation,
                              (unsigned long)sweepInfo.completedPoints);
            }
        }
    }

    if ((now - g_satPayload.lastTelemetryMs) >=
        SAT_PAYLOAD_TELEMETRY_PERIOD_MS) {
        g_satPayload.lastTelemetryMs = now;
        SatPayloadUpdateTelemetry();
    }
}

void SatPayloadApp_ProcessTelemetry(void)
{
    uint32_t now = TK8710GetTickMs();

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
        TRM_LOG_INFO("SAT APP work params accepted: freq=%lu rate=%u rfMask=0x%02X",
                     (unsigned long)params->centerFreqHz,
                     (unsigned int)params->rates[0].rateMode,
                     (unsigned int)params->rfMask);
    } else {
        TRM_LOG_WARN("SAT APP work params rejected: result=%d", result);
    }
    g_satPayload.lastResult = result;
    return result;
}

SatPayloadResult SatPayloadApp_SetWorkMode(uint8_t mode)
{
    SatPayloadResult result = SatPayloadStartModeWithRollback(mode);
    g_satPayload.lastResult = result;
    if (result == SAT_PAYLOAD_OK) {
        TRM_LOG_INFO("SAT APP mode started: mode=%u state=%u",
                     (unsigned int)mode,
                     (unsigned int)g_satPayload.state);
    } else {
        TRM_LOG_ERROR("SAT APP mode start failed: mode=%u result=%d state=%u",
                      (unsigned int)mode, result,
                      (unsigned int)g_satPayload.state);
    }
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
    if (result == SAT_PAYLOAD_OK) {
        TRM_LOG_INFO("SAT APP stopped and returned to control-ready");
    } else {
        TRM_LOG_ERROR("SAT APP stop failed: result=%d", result);
    }
    SatPayloadUpdateTelemetry();
    return result;
}

SatPayloadResult SatPayloadApp_ReadRegister(uint16_t address,
                                            uint32_t* value)
{
    SatPayloadResult result;

    if (value == NULL) {
        result = SAT_PAYLOAD_ERR_PARAM;
    } else {
        result = (TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, address, value) ==
                  TK8710_OK) ? SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
    }

    g_satPayload.lastResult = result;
    return result;
}

SatPayloadResult SatPayloadApp_WriteRegister(uint16_t address,
                                             uint32_t value)
{
    SatPayloadResult result;

    result = (TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, address, value) ==
              TK8710_OK) ? SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_DRIVER;
    g_satPayload.lastResult = result;
    return result;
}

SatPayloadResult SatPayloadApp_SetRfTxDc(uint8_t antenna,
                                        int16_t iDc,
                                        int16_t qDc)
{
    SatPayloadResult result = SAT_PAYLOAD_OK;
    uint32_t value;
    uint16_t address;

    if (antenna >= SAT_PAYLOAD_RF_ANTENNA_COUNT) {
        result = SAT_PAYLOAD_ERR_PARAM;
    } else {
        value = TK8710_S_TX_CONFIG_29_ENCODE((uint16_t)qDc,
                                             (uint16_t)iDc);
        address = (uint16_t)(TX_FE_BASE +
                  ((uint32_t)antenna * SAT_PAYLOAD_TX_FE_ANTENNA_STRIDE) +
                  offsetof(struct tx_dac_if, tx_config_29));

        if ((g_satPayload.halActive != 0U) &&
            (TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                            address, value) != TK8710_OK)) {
            result = SAT_PAYLOAD_ERR_DRIVER;
        }

        if (result == SAT_PAYLOAD_OK) {
            g_satRfConfig.txadc[antenna].i = iDc;
            g_satRfConfig.txadc[antenna].q = qDc;
            TRM_LOG_INFO("SAT APP RF TX DC set: antenna=%u I=0x%04X Q=0x%04X applied=%s",
                         (unsigned int)antenna,
                         (unsigned int)(uint16_t)iDc,
                         (unsigned int)(uint16_t)qDc,
                         (g_satPayload.halActive != 0U) ? "live" : "next-init");
        }
    }

    g_satPayload.lastResult = result;
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
            result = SatPayloadApp_ReadRegister(
                request->payload.reg.address, &value);
            break;
        case SAT_PAYLOAD_TC_WRITE_REG:
            result = SatPayloadApp_WriteRegister(
                request->payload.reg.address,
                request->payload.reg.value);
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
        case SAT_PAYLOAD_TC_SET_RF_TX_DC:
            value = TK8710_S_TX_CONFIG_29_ENCODE(
                (uint16_t)request->payload.rfTxDc.qDc,
                (uint16_t)request->payload.rfTxDc.iDc);
            result = SatPayloadApp_SetRfTxDc(
                request->payload.rfTxDc.antenna,
                request->payload.rfTxDc.iDc,
                request->payload.rfTxDc.qDc);
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

SatPayloadResult SatPayloadApp_GetAcmCalibrationResult(
    TRM_AcmCalibResult* result)
{
    return (TRM_GetAcmCalibrationResult(result) == TRM_OK) ?
           SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_STATE;
}

SatPayloadResult SatPayloadApp_GetCaptureInfo(TK8710CaptureInfo* info)
{
    return (TK8710CaptureGetInfo(info) == 0) ?
           SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_PARAM;
}

SatPayloadResult SatPayloadApp_ReadCaptureData(uint32_t generation,
                                               uint8_t antenna,
                                               uint32_t offset,
                                               void* data, uint32_t len)
{
    return (TK8710CaptureReadAntenna(generation, antenna, offset,
                                     data, len) == 0) ?
           SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_PARAM;
}

SatPayloadResult SatPayloadApp_GetSweepResultInfo(TRM_SweepResultInfo* info)
{
    return (TRM_GetSweepResultInfo(info) == TRM_OK) ?
           SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_PARAM;
}

SatPayloadResult SatPayloadApp_ReadSweepResults(uint32_t startIndex,
                                                TRM_SweepResultPoint* results,
                                                uint32_t capacity,
                                                uint32_t* resultCount)
{
    return (TRM_ReadSweepResults(startIndex, results, capacity,
                                 resultCount) == TRM_OK) ?
           SAT_PAYLOAD_OK : SAT_PAYLOAD_ERR_PARAM;
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
