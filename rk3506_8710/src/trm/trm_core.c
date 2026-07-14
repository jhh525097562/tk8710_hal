/**
 * @file trm_core.c
 * @brief TRM核心功能实现
 */

#include "../inc/trm/trm_api.h"
#include "../inc/trm/trm_internal.h"
#include "../inc/trm/trm_log.h"
#include "../inc/trm/trm_beam.h"
#include "../inc/trm/trm_data.h"
#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_rf_regs.h"
#include "../inc/tk8710_noise_api.h"
#include "../port/tk8710_hal.h"
#include "driver/tk8710_log.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* IPC通信头文件 - 仅在RK3506平台需要 */
#ifdef PLATFORM_RK3506
#include "../inc/tk8710_ipc_comm.h"
#include "../inc/tk8710_scan_ipc_server.h"
#endif

/* usleep定义 - Linux平台使用usleep，Windows使用Sleep */
#if defined(PLATFORM_TMS570)
#define usleep(us) TK8710DelayUs((uint32_t)(us))
#elif defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#define usleep(us) Sleep((us) / 1000U)
#endif

/* 外部函数声明 - 来自trm_beam.c和trm_data.c */
extern void TRM_BeamInit(uint32_t maxUsers, uint32_t timeoutMs);
extern void TRM_BeamDeinit(void);
extern void TRM_DataInit(void);
extern void TRM_DataDeinit(void);
extern int TRM_ProcessTxSlot(uint8_t slotIndex, uint8_t maxUserCount, TK8710IrqResult* irqResult);

/*==============================================================================
 * 私有定义
 *============================================================================*/

/* TRM上下文 */
static TrmContext g_trmCtx;

/* 全局帧号管理变量 */
uint32_t g_trmCurrentFrame = 0;
uint32_t g_trmMaxFrameCount = 100;

/* 扫频状态控制变量 */
static volatile TRM_SweepState g_sweepState = {0};
static volatile uint8_t g_sweepCapturePending = 0;
static volatile uint8_t g_sweepCaptureWaitCount = 0;
static volatile uint8_t g_sweepCaptureDone = 0;
static volatile uint32_t g_sweepCaptureFreq = 0;

#define TRM_ACM_DEFAULT_CALIB_COUNT       5
#define TRM_ACM_DEFAULT_SNR_THRESHOLD     32
#define TRM_ACM_DEFAULT_RESTART_ADVANCE_US 90
#define TRM_ACM_DEFAULT_GUARD_US          1000
#define TRM_ACM_BUSY_WAIT_US              2000
#define TRM_ACM_S0_PERIOD_WARN_US         5000

typedef struct {
    volatile uint8_t pending;
    volatile uint8_t running;
    TRM_AcmCalibRequest request;
    int lastResult;
    uint32_t lastElapsedUs;
    uint32_t lastWaitUs;
} TRM_AcmCalibState;

static volatile TRM_AcmCalibState g_acmCalibState = {
    .pending = 0,
    .running = 0,
    .lastResult = TRM_OK,
    .lastElapsedUs = 0,
    .lastWaitUs = 0
};

static volatile uint8_t g_acmS0MonitorRemaining = 0;
static volatile uint32_t g_acmS0MonitorSeq = 0;
static volatile uint32_t g_acmS0PeriodBeforeUs = 0;
static volatile uint32_t g_acmS0CountBefore = 0;
static volatile uint64_t g_acmFastStartEndUs = 0;

/* 内部函数声明 */
TrmContext* TRM_GetContext(void);

/*==============================================================================
 * Driver回调函数声明
 *============================================================================*/

static void TRM_OnDriverSlotEnd(uint8_t slotType, uint8_t slotIndex, uint32_t frameNo);
static void TRM_OnDriverTxSlot(uint8_t slotIndex, uint8_t maxUserCount, TK8710IrqResult* irqResult);
static void TRM_OnDriverSlotRx(TK8710IrqResult* irqResult);
static void TRM_OnDriverError(TK8710IrqResult* irqResult);

/* 多回调适配函数 */
static void TRM_OnDriverSlotEndAdapter(TK8710IrqResult* irqResult);
static void TRM_OnDriverTxSlotAdapter(TK8710IrqResult* irqResult);
static void TRM_OnDriverSlotRxAdapter(TK8710IrqResult* irqResult);
static void TRM_OnDriverErrorAdapter(TK8710IrqResult* irqResult);
static int TRM_ConfigSweepCapture(void);
static void TRM_ProcessSweepCaptureInRx(void);
static void TRM_UpdateSweepFrequencyAfterCapture(void);
static int TRM_TryRunAcmCalibrationAtS2(TK8710IrqResult* irqResult);
static uint32_t TRM_GetAcmSlot3WindowUs(const slotCfg_t* slotCfg, const TK8710IrqResult* irqResult);
static uint32_t TRM_ReadAcmSlot3LenUs(void);
static int TRM_RefreshAcmSlotConfig(const slotCfg_t* slotCfg);
static void TRM_CompleteVirtualS3(void);
static uint32_t TRM_WaitUntilUs(uint64_t targetUs);

/*==============================================================================
 * 公共接口实现
 *============================================================================*/

int TRM_Init(const TRM_InitConfig* config)
{
    int ret;
    
    /* 初始化默认TRM日志系统（如果尚未初始化） */
    TRM_LogInit(TRM_LOG_INFO);
    
    if (config == NULL) {
        TRM_LOG_ERROR("TRM init failed: config is NULL");
        return TRM_ERR_PARAM;
    }
    
    if (g_trmCtx.state != TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM already initialized, state: %d", g_trmCtx.state);
        return TRM_ERR_STATE;
    }
    
    /* 初始化TRM日志系统 */
    /* 注意：不重复初始化日志系统，使用全局设置 */
    TRM_LOG_INFO("Start TRM system initialization");
    
    /* 清零上下文 */
    memset(&g_trmCtx, 0, sizeof(g_trmCtx));
    
    /* 保存配置 */
    memcpy(&g_trmCtx.config, config, sizeof(TRM_InitConfig));
    TRM_LOG_DEBUG("Save TRM config: beamMaxUsers=%u, beamTimeoutMs=%u, maxFrameCount=%u", 
                  config->beamMaxUsers, config->beamTimeoutMs, config->maxFrameCount);
    
    /* 设置默认值 */
    if (g_trmCtx.config.beamMaxUsers == 0) {
        g_trmCtx.config.beamMaxUsers = TRM_BEAM_MAX_USERS_DEFAULT;
        TRM_LOG_DEBUG("Use default beam max users: %u", g_trmCtx.config.beamMaxUsers);
    }
    if (g_trmCtx.config.beamTimeoutMs == 0) {
        g_trmCtx.config.beamTimeoutMs = TRM_BEAM_TIMEOUT_DEFAULT;
        TRM_LOG_DEBUG("Use default beam timeout: %u ms", g_trmCtx.config.beamTimeoutMs);
    }
    if (g_trmCtx.config.maxFrameCount == 0) {
        g_trmCtx.config.maxFrameCount = 254;  /* 默认最大帧数 */
        TRM_LOG_DEBUG("Use default max frame count: %u", g_trmCtx.config.maxFrameCount);
    }
    
    /* 设置全局帧管理参数 */
    g_trmMaxFrameCount = g_trmCtx.config.maxFrameCount;
    TRM_LOG_DEBUG("Set global max frame count: %u", g_trmMaxFrameCount);
    
    /* 初始化波束管理 */
    TRM_BeamInit(g_trmCtx.config.beamMaxUsers, g_trmCtx.config.beamTimeoutMs);
    TRM_LOG_INFO("Beam management initialized");
    
    /* 初始化发送队列 */
    TRM_DataInit();
    TRM_LOG_INFO("TX queue initialized");
    
    g_trmCtx.state = TRM_STATE_INIT;
    TRM_LOG_INFO("TRM system initialized, state: INIT");
    
    /* 注册TRM到Driver的回调函数 */
    ret = TRM_RegisterDriverCallbacks();
    if (ret != TRM_OK) {
        TRM_LOG_ERROR("TRM Driver callback registration failed: code=%d", ret);
        return ret;
    }
    TRM_LOG_INFO("TRM Driver callbacks registered");

    return TRM_OK;
}

int TRM_Deinit(void)
{
    if (g_trmCtx.state == TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM is not initialized, no cleanup needed");
        return TRM_OK;
    }
    
    TRM_LOG_INFO("Start TRM system cleanup");
    
    /* TRM不直接控制Driver停止，由DriverManager控制 */
    
    /* 清理波束管理 */
    TRM_BeamDeinit();
    TRM_LOG_INFO("Beam management cleaned");
    
    /* 清理发送队列 */
    TRM_DataDeinit();
    TRM_LOG_INFO("TX queue cleaned");
    
    g_trmCtx.state = TRM_STATE_UNINIT;
    TRM_LOG_INFO("TRM system cleaned, state: UNINIT");
    
    return TRM_OK;
}

int TRM_Reset(void)
{
    /* TRM不直接控制Driver复位，由DriverManager控制 */
    
    /* 清理波束 */
    TRM_ClearBeamInfo(0xFFFFFFFF);
    
    /* 清理发送队列 */
    TRM_ClearTxData(0xFFFFFFFF);
    
    return TRM_OK;
}

int TRM_GetStats(TRM_Stats* stats)
{
    if (stats == NULL) {
        return TRM_ERR_PARAM;
    }
    
    /* 复制统计信息 */
    memcpy(stats, &g_trmCtx.stats, sizeof(TRM_Stats));
    
    /* 设置当前状态 */
    stats->state = g_trmCtx.state;
    
    /* 更新剩余发送队列数量 */
    extern uint32_t TRM_GetTxQueueCount(void);  /* 获取发送队列当前数量 */
    extern uint32_t TRM_GetTxQueueCapacity(void); /* 获取发送队列最大容量 */
    
    uint32_t currentCount = TRM_GetTxQueueCount();
    uint32_t maxCapacity = TRM_GetTxQueueCapacity();
    stats->txQueueRemaining = (maxCapacity > currentCount) ? (maxCapacity - currentCount) : 0;
    
    return TRM_OK;
}

void TRM_SetCurrentFrame(uint32_t frameNo)
{
    g_trmCurrentFrame = frameNo;
}

uint32_t TRM_GetCurrentFrame(void)
{
    return g_trmCurrentFrame;
}

int TRM_RequestAcmCalibration(const TRM_AcmCalibRequest* request)
{
    TRM_AcmCalibRequest normalized;

    if (g_trmCtx.state == TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM: ACM calibration request rejected, TRM not initialized");
        return TRM_ERR_STATE;
    }

    if (g_acmCalibState.pending || g_acmCalibState.running) {
        TRM_LOG_WARN("TRM: ACM calibration request rejected, pending=%d running=%d",
                     g_acmCalibState.pending, g_acmCalibState.running);
        return TRM_ERR_STATE;
    }

    if (request != NULL) {
        normalized = *request;
    } else {
        memset(&normalized, 0, sizeof(normalized));
    }

    if (normalized.calibCount == 0) {
        normalized.calibCount = TRM_ACM_DEFAULT_CALIB_COUNT;
    }
    if (normalized.snrThreshold == 0) {
        normalized.snrThreshold = TRM_ACM_DEFAULT_SNR_THRESHOLD;
    }
    if (normalized.restartAdvanceUs == 0) {
        normalized.restartAdvanceUs = TRM_ACM_DEFAULT_RESTART_ADVANCE_US;
    }
    if (normalized.guardUs == 0) {
        normalized.guardUs = TRM_ACM_DEFAULT_GUARD_US;
    }

    g_acmCalibState.request = normalized;
    g_acmCalibState.lastResult = TRM_OK;
    g_acmCalibState.lastElapsedUs = 0;
    g_acmCalibState.lastWaitUs = 0;
    g_acmCalibState.pending = 1;

    TRM_LOG_INFO("TRM: ACM calibration requested: count=%u, snr=%u, restartAdvance=%u us, guard=%u us",
                 normalized.calibCount, normalized.snrThreshold,
                 normalized.restartAdvanceUs, normalized.guardUs);

    return TRM_OK;
}



void TRM_SetMaxFrameCount(uint32_t maxCount)
{
    g_trmMaxFrameCount = maxCount;
}

uint32_t TRM_GetSuperFramePosition(void)
{
    return (g_trmCurrentFrame % g_trmMaxFrameCount) + 1;
}

int TRM_RegisterDriverCallbacks(void)
{
    if (g_trmCtx.state == TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM is not initialized, cannot register Driver callbacks");
        return TRM_ERR_STATE;
    }
    
    /* 设置Driver回调结构体 */
    TK8710DriverCallbacks callbacks = {
        .onRxData = TRM_OnDriverSlotRxAdapter,
        .onTxSlot = TRM_OnDriverTxSlotAdapter,
        .onSlotEnd = TRM_OnDriverSlotEndAdapter,
        .onError = TRM_OnDriverErrorAdapter
    };
    
    /* 注册到Driver */
    TK8710RegisterCallbacks(&callbacks);
    
    TRM_LOG_INFO("TRM Driver callbacks registered");
    return TRM_OK;
}

/* 内部函数实现 */
TrmContext* TRM_GetContext(void)
{
    return &g_trmCtx;
}

static int TRM_ConfigSweepCapture(void)
{
    s_ram_rd0 ramRd0;
    ramRd0.data = 0;
    ramRd0.b.cap_en = 1;

    int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0),
                             ramRd0.data);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: Configure sweep capture register failed: %d", ret);
        return ret;
    }

    g_sweepCapturePending = 1;
    g_sweepCaptureWaitCount = 0;
    g_sweepCaptureDone = 0;
    g_sweepCaptureFreq = g_sweepState.current_freq;
    return TK8710_OK;
}

static void TRM_ProcessSweepCaptureInRx(void)
{
    if (!g_sweepCapturePending) {
        return;
    }

    if (g_sweepCaptureWaitCount < 2) {
        g_sweepCaptureWaitCount++;
        TRM_LOG_DEBUG("TRM: Wait sweep capture RX frame count=%u", g_sweepCaptureWaitCount);
        return;
    }

    TRM_LOG_DEBUG("TRM: Performing sweep capture at RX");
    int captureRet = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, TK8710_DBG_OPT_GET, NULL, NULL);
    if (captureRet == TK8710_OK) {
        TRM_LOG_DEBUG("Capture data operation succeeded\n");
        uint8_t append_result = (g_sweepCaptureFreq != g_sweepState.start_freq);
        tk8710_sweep_noise_process("8710CaptureData", g_sweepState.rate_mode,
                                  g_sweepCaptureFreq, append_result);
        g_sweepCaptureDone = 1;
    } else {
        TRM_LOG_DEBUG("Capture data operation failed: ret=%d\n", captureRet);
    }

    g_sweepCapturePending = 0;
    g_sweepCaptureWaitCount = 0;
}

static void TRM_UpdateSweepFrequencyAfterCapture(void)
{
    g_sweepState.current_freq += g_sweepState.step_freq;
    if (g_sweepState.current_freq > g_sweepState.end_freq) {
        g_sweepState.sweep_active = 0;
        g_sweepCapturePending = 0;
        g_sweepCaptureWaitCount = 0;
        g_sweepCaptureDone = 0;
        TRM_LOG_INFO("TRM: Frequency sweep completed");
#ifdef PLATFORM_RK3506
        TK8710ScanIpcNotifySweepDone();
        IpcCommClearConfigReceived();

        int request_count = 0;
        while (request_count < 3) {
            TRM_LOG_INFO("Send config request attempt %d...\n", request_count + 1);
            if (IpcCommSendConfigRequest(&g_ipc_ctx) != 0) {
                TRM_LOG_INFO("Config request send failed\n");
            }

            for (int i = 0; i < 100 && !IpcCommIsConfigReceived(); i++) {
                usleep(100000);
            }

            request_count++;
        }
#endif
        return;
    }

    int ret = TK8710_OK;
    double freq_step;
    uint32_t freq_reg;

    if (g_sweepState.rftype == TK8710_RF_TYPE_1257_32M) {
        freq_step = RF_SX1257_FREQ_STEP;
    } else {
        freq_step = RF_SX1255_FREQ_STEP;
    }
    freq_reg = (uint32_t)((double)g_sweepState.current_freq / freq_step);

    TRM_LOG_INFO("TRM: Switching to frequency %u Hz (step=%.2f, reg=0x%06X)",
                 g_sweepState.current_freq, freq_step, freq_reg);

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_RX_MSB >> 8, (freq_reg >> 16) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: RX frequency MSB write failed: %d", ret);
        return;
    }

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_RX_MID >> 8, (freq_reg >> 8) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: RX frequency MID write failed: %d", ret);
        return;
    }

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_RX_LSB >> 8, (freq_reg >> 0) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: RX frequency LSB write failed: %d", ret);
        return;
    }
    TRM_LOG_DEBUG("TRM: RX frequency configuration completed");

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_RX_GAIN >> 8, g_sweepState.rxgain);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: RX gain configuration failed: %d", ret);
        return;
    }
    TRM_LOG_DEBUG("TRM: RX gain set to: 0x%02X", g_sweepState.rxgain);

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_TX_MSB >> 8, (freq_reg >> 16) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: TX frequency MSB write failed: %d", ret);
        return;
    }

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_TX_MID >> 8, (freq_reg >> 8) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: TX frequency MID write failed: %d", ret);
        return;
    }

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_FRF_TX_LSB >> 8, (freq_reg >> 0) & 0xFF);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: TX frequency LSB write failed: %d", ret);
        return;
    }
    TRM_LOG_DEBUG("TRM: TX frequency configuration completed");

    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_TX_GAIN >> 8, g_sweepState.txgain);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: TX gain configuration failed: %d", ret);
        return;
    }
    TRM_LOG_DEBUG("TRM: TX gain set to: 0x%02X", g_sweepState.txgain);
    TRM_LOG_INFO("TRM: Frequency switch to %u Hz completed", g_sweepState.current_freq);
}

/* 多回调适配函数实现 */
static void TRM_OnDriverSlotRxAdapter(TK8710IrqResult* irqResult)
{
    /* 更新统计信息 */
    g_trmCtx.stats.rxCount++;

    /* 调试：记录中断类型 */
    TRM_LOG_DEBUG("TRM: Received RX interrupt type=%d", irqResult->irq_type);

    if (g_sweepState.sweep_active) {
        TRM_ProcessSweepCaptureInRx();
    }

    TRM_OnDriverSlotRx(irqResult);
}

static void TRM_OnDriverTxSlotAdapter(TK8710IrqResult* irqResult)
{
    /* 调试：记录中断类型 */
    TRM_LOG_DEBUG("TRM: Received TX interrupt type=%d", irqResult->irq_type);
    
    /* S1时隙，最大128用户 */
    TRM_OnDriverTxSlot(1, 128, irqResult);
}

static void TRM_OnDriverSlotEndAdapter(TK8710IrqResult* irqResult)
{
    /* 调试：记录中断类型 */
    TRM_LOG_DEBUG("TRM: Received SlotEnd interrupt type=%d", irqResult->irq_type);
    
    /* 根据中断类型确定时隙信息 */
    uint8_t slotType = 0;
    uint8_t slotIndex = 0;
    
    switch (irqResult->irq_type) {
        case TK8710_IRQ_S0:
            slotType = 0; slotIndex = 0;  /* BCN时隙 */
            break;
        case TK8710_IRQ_S2:
            slotType = 2; slotIndex = 2;  /* S2时隙 */
            if (TRM_TryRunAcmCalibrationAtS2(irqResult) == TRM_OK) {
                return;
            }
            break;
        case TK8710_IRQ_S3:
            /* 检查是否处于扫频状态 */
            if (g_sweepState.sweep_active) {
                TRM_LOG_DEBUG("TRM: Sweep active, current frame=%u", g_trmCurrentFrame);

                if (g_sweepCaptureDone) {
                    TRM_UpdateSweepFrequencyAfterCapture();
                    g_sweepCaptureDone = 0;
                }

                if (!g_sweepState.sweep_active) {
                    slotType = 3; slotIndex = 3;  /* S3时隙 */
                    break;
                }

                if (g_sweepCapturePending) {
                    TRM_LOG_DEBUG("TRM: Sweep capture pending, wait RX capture complete");
                } else if ((g_trmCurrentFrame % 4) != 0) {
                    TRM_LOG_DEBUG("TRM: Skip sweep processing at frame %u", g_trmCurrentFrame);
                } else {
#ifdef PLATFORM_RK3506
                    TK8710ScanIpcNotifySweepRunning();
#endif
                    TRM_LOG_DEBUG("TRM: Configure sweep capture at frequency %u", g_sweepState.current_freq);
                    int ret = TRM_ConfigSweepCapture();
                    if (ret != TK8710_OK) {
                        return;
                    }
                }
            }
            slotType = 3; slotIndex = 3;  /* S3时隙 */
            break;
        default:
            TRM_LOG_WARN("TRM: Unexpected slot end interrupt type: %d", irqResult->irq_type);
            return;
    }
    
    TRM_OnDriverSlotEnd(slotType, slotIndex, g_trmCurrentFrame);
}

static void TRM_OnDriverErrorAdapter(TK8710IrqResult* irqResult)
{
    /* 调试：记录中断类型 */
    TRM_LOG_DEBUG("TRM: Received Error interrupt type=%d", irqResult->irq_type);
    
    TRM_OnDriverError(irqResult);
}

static void TRM_OnDriverError(TK8710IrqResult* irqResult)
{
    int errorCode = irqResult->irq_type;
    TRM_LOG_WARN("TRM: Driver error callback, error code: %d", errorCode);
    
    /* 可以根据错误类型进行不同的处理 */
    switch (errorCode) {
        case TK8710_IRQ_RX_BCN:
            TRM_LOG_DEBUG("TRM: BCN receive error");
            break;
        case TK8710_IRQ_BRD_UD:
            TRM_LOG_DEBUG("TRM: Broadcast UD error");
            break;
        case TK8710_IRQ_BRD_DATA:
            TRM_LOG_DEBUG("TRM: Broadcast DATA error");
            break;
        case TK8710_IRQ_MD_UD:
            TRM_LOG_DEBUG("TRM: MD UD error");
            break;
        case TK8710_IRQ_ACM:
            TRM_LOG_DEBUG("TRM: ACM calibration error");
            break;
        default:
            TRM_LOG_WARN("TRM: Unknown error type: %d", errorCode);
            break;
    }
}

/*==============================================================================
 * 内部函数实现
 *============================================================================*/

static void TRM_OnDriverSlotRx(TK8710IrqResult* irqResult)
{
    TRM_LOG_DEBUG("TRM: Processing MD_DATA interrupt");
    if (irqResult->mdDataValid) {
        TRM_LOG_DEBUG("TRM: RxData valid_users=%d, crc_errors=%d", 
               irqResult->crcValidCount, irqResult->crcErrorCount);
        
        /* 收集所有CRC正确的用户索引 */
        uint8_t validUserIndices[128];
        uint8_t validUserCount = 0;
        
        for (uint8_t i = 0; i < 128; i++) {
            if (irqResult->crcResults[i].userIndex < 128 && 
                irqResult->crcResults[i].dataValid) {
                validUserIndices[validUserCount++] = i;
            }
        }
        
        /* 批量处理所有CRC正确的用户 */
        if (validUserCount > 0) {
            TRM_ProcessRxUserDataBatch(validUserIndices, validUserCount, irqResult->crcResults, irqResult);
        }
    } else {
        TRM_LOG_DEBUG("TRM: MD_DATA interrupt but mdDataValid=0, skipping");
    }
}

/*==============================================================================
 * Driver回调函数实现
 *============================================================================*/

static void TRM_OnDriverSlotEnd(uint8_t slotType, uint8_t slotIndex, uint32_t frameNo)
{
    TRM_LOG_DEBUG("TRM: Slot end: type=%d, index=%d, frame=%u", slotType, slotIndex, frameNo);
    
    /* 根据时隙类型处理 */
    switch (slotType) {
        case 0: /* TK8710_IRQ_S0 */
            /* Slot0时隙结束 */
            if (g_acmS0MonitorRemaining > 0) {
                uint64_t s0TimeUs = 0;
                uint32_t s0PeriodUs = 0;
                uint32_t s0Count = 0;
                uint32_t expectedPeriodUs = g_acmS0PeriodBeforeUs;
                int32_t diffUs = 0;

                TK8710GetS0PeriodStats(&s0TimeUs, &s0PeriodUs, &s0Count);
                if (expectedPeriodUs != 0) {
                    diffUs = (int32_t)s0PeriodUs - (int32_t)expectedPeriodUs;
                }

                g_acmS0MonitorSeq++;
                g_acmS0MonitorRemaining--;

                if (diffUs > (int32_t)TRM_ACM_S0_PERIOD_WARN_US ||
                    diffUs < -(int32_t)TRM_ACM_S0_PERIOD_WARN_US) {
                    TRM_LOG_WARN("TRM: ACM post S0 period mismatch[%u]: period=%u us "
                                 "before=%u us diff=%d us s0Count=%u beforeCount=%u "
                                 "fromFastStart=%u us",
                                 g_acmS0MonitorSeq, s0PeriodUs, expectedPeriodUs, diffUs,
                                 s0Count, g_acmS0CountBefore,
                                 (uint32_t)(s0TimeUs - g_acmFastStartEndUs));
                }
            }
            break;
            
        case 1: /* TK8710_IRQ_S1 */
            /* Slot1时隙结束 */
            break;
            
        case 2: /* TK8710_IRQ_S2 */
            /* Slot2时隙结束 */
            break;
            
        case 3: /* TK8710_IRQ_S3 */
            /* Slot3时隙结束 - 系统帧号加1，并更新当前系统帧号 */
            frameNo++;
            TRM_SetCurrentFrame(frameNo);
            TRM_LOG_DEBUG("TRM: Frame updated to %u after S3 slot", frameNo);
            break;
            
        default:
            break;
    }
}

static void TRM_OnDriverTxSlot(uint8_t slotIndex, uint8_t maxUserCount, TK8710IrqResult* irqResult)
{
    TRM_LOG_DEBUG("TRM: TxSlot: slot=%d, maxUsers=%d\n", slotIndex, maxUserCount);
    
    /* 处理广播发送管理 */
    TRM_ManageBroadcast();
    
    /* 从发送队列取数据，查询波束，调用Driver发送 */
    int sentCount = TRM_ProcessTxSlot(slotIndex, maxUserCount, irqResult);
    
    if (sentCount > 0) {
        g_trmCtx.stats.txCount += sentCount;
        TRM_LOG_DEBUG("TRM: TxSlot: sent %d users\n", sentCount);
    }
}

static uint32_t TRM_GetAcmSlot3WindowUs(const slotCfg_t* slotCfg, const TK8710IrqResult* irqResult)
{
    uint8_t rateIndex = 0;
    uint32_t slot3Us;
    s_obv_4 obv4;

    if (slotCfg == NULL) {
        return 0;
    }

    if (TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, obv_4),
                      &obv4.data) == TK8710_OK && obv4.b.s3_len > 0) {
        return obv4.b.s3_len;
    }

    if (irqResult != NULL && irqResult->currentRateIndex < slotCfg->rateCount) {
        rateIndex = irqResult->currentRateIndex;
    }

    slot3Us = slotCfg->s3Cfg[rateIndex].timeLen;
    if (slot3Us == 0) {
        slot3Us = slotCfg->s3Cfg[rateIndex].da_m;
    }

    return slot3Us;
}

static uint32_t TRM_ReadAcmSlot3LenUs(void)
{
    s_obv_4 obv4;

    obv4.data = 0;

    if (TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                      MAC_BASE + offsetof(struct mac, obv_4), &obv4.data) != TK8710_OK) {
        return 0;
    }

    return obv4.b.s3_len;
}

static int TRM_RefreshAcmSlotConfig(const slotCfg_t* slotCfg)
{
    slotCfg_t refreshCfg;

    if (slotCfg == NULL) {
        return TRM_ERR_PARAM;
    }

    memcpy(&refreshCfg, slotCfg, sizeof(refreshCfg));

    return TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &refreshCfg);
}

static void TRM_CompleteVirtualS3(void)
{
    uint32_t frameNo = g_trmCurrentFrame + 1;

    TRM_SetCurrentFrame(frameNo);
    TRM_LOG_DEBUG("TRM: Virtual S3 completed, frame updated to %u", frameNo);
}

static uint32_t TRM_WaitUntilUs(uint64_t targetUs)
{
    uint64_t nowUs;
    uint32_t coarseWaitUs;

    while (1) {
        nowUs = TK8710GetTimeUs();
        if (nowUs >= targetUs) {
            return (uint32_t)(nowUs - targetUs);
        }

        if (targetUs - nowUs <= TRM_ACM_BUSY_WAIT_US) {
            break;
        }

        coarseWaitUs = (uint32_t)(targetUs - nowUs - TRM_ACM_BUSY_WAIT_US);
        TK8710DelayUs(coarseWaitUs);
    }

    do {
        nowUs = TK8710GetTimeUs();
    } while (nowUs < targetUs);

    return (uint32_t)(nowUs - targetUs);
}

static int TRM_TryRunAcmCalibrationAtS2(TK8710IrqResult* irqResult)
{
    const slotCfg_t* slotCfg;
    slotCfg_t slotCfgBeforeAcm;
    TRM_AcmCalibRequest request;
    acmParam_t acmParam;
    uint64_t startUs;
    uint64_t endUs;
    uint64_t slot3EndUs;
    uint64_t restartTargetUs;
    uint64_t fastStartBeginUs;
    uint64_t fastStartEndUs;
    uint64_t s0BeforeTimeUs = 0;
    uint64_t s0AfterStartTimeUs = 0;
    uint32_t s0BeforePeriodUs = 0;
    uint32_t s0AfterStartPeriodUs = 0;
    uint32_t s0BeforeCount = 0;
    uint32_t s0AfterStartCount = 0;
    uint32_t elapsedUs;
    uint32_t slot3Us;
    uint32_t slot3AfterCalibUs;
    uint32_t slot3AfterRefreshUs;
    uint32_t waitUs = 0;
    uint32_t triggerLateUs = 0;
    uint32_t irqAfterCalib = 0;
    uint32_t irqAfterStart = 0;
    int calibRet;
    int ret;

    if (!g_acmCalibState.pending || g_acmCalibState.running) {
        return TRM_ERR_STATE;
    }

    if (g_trmMaxFrameCount == 0 || TRM_GetSuperFramePosition() != g_trmMaxFrameCount) {
        return TRM_ERR_STATE;
    }

    if (g_sweepState.sweep_active) {
        TRM_LOG_WARN("TRM: ACM calibration waits because sweep is active");
        return TRM_ERR_STATE;
    }

    slotCfg = TK8710GetSlotConfig();
    if (slotCfg == NULL || slotCfg->msMode != TK8710_MODE_MASTER) {
        TRM_LOG_WARN("TRM: ACM calibration rejected, invalid slot config or mode");
        return TRM_ERR_STATE;
    }
    memcpy(&slotCfgBeforeAcm, slotCfg, sizeof(slotCfgBeforeAcm));

    slot3Us = TRM_GetAcmSlot3WindowUs(slotCfg, irqResult);
    if (slot3Us == 0) {
        TRM_LOG_WARN("TRM: ACM calibration rejected, slot3 window is unknown");
        return TRM_ERR_STATE;
    }

    request = g_acmCalibState.request;
    g_acmCalibState.pending = 0;
    g_acmCalibState.running = 1;

    acmParam.calibCount = request.calibCount;
    acmParam.snrThreshold = request.snrThreshold;
    acmParam.snrPassCount = 0;

    TK8710GetS0PeriodStats(&s0BeforeTimeUs, &s0BeforePeriodUs, &s0BeforeCount);

    startUs = TK8710GetTimeUs();
    TRM_LOG_INFO("TRM: ACM calibration starts at last-frame S2 end, slot3 window=%u us, "
                 "s0PeriodBefore=%u us, s0CountBefore=%u",
                 slot3Us, s0BeforePeriodUs, s0BeforeCount);

    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);
    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);
    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);

    calibRet = TK8710Ctrl(TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY, &acmParam);
    endUs = TK8710GetTimeUs();
    irqAfterCalib = TK8710GetIrqStatus();
    slot3AfterCalibUs = TRM_ReadAcmSlot3LenUs();
    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);
    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);
    TK8710ClearIrqStatus(1 << TK8710_IRQ_ACM);
    elapsedUs = (uint32_t)(endUs - startUs);
    g_acmCalibState.lastElapsedUs = elapsedUs;
    g_acmCalibState.lastResult = calibRet;
    slot3EndUs = startUs + slot3Us;

    if (calibRet < 0) {
        TRM_LOG_ERROR("TRM: ACM calibration failed: ret=%d elapsed=%u us", calibRet, elapsedUs);
        g_acmCalibState.running = 0;
        return TRM_ERR_DRIVER;
    }

    ret = TRM_RefreshAcmSlotConfig(&slotCfgBeforeAcm);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: Failed to refresh slot config after ACM: %d", ret);
        g_acmCalibState.running = 0;
        return TRM_ERR_DRIVER;
    }
    slot3AfterRefreshUs = TRM_ReadAcmSlot3LenUs();
    if (slot3AfterRefreshUs != slot3Us) {
        TRM_LOG_WARN("TRM: ACM slot refresh mismatch: before=%u us afterCalib=%u us afterRefresh=%u us",
                     slot3Us, slot3AfterCalibUs, slot3AfterRefreshUs);
    } else {
        TRM_LOG_INFO("TRM: ACM slot refreshed: before=%u us afterCalib=%u us afterRefresh=%u us",
                     slot3Us, slot3AfterCalibUs, slot3AfterRefreshUs);
    }

    if (slot3Us > elapsedUs + request.restartAdvanceUs + request.guardUs) {
        restartTargetUs = slot3EndUs - request.restartAdvanceUs;
        {
            uint64_t nowUs = TK8710GetTimeUs();
            waitUs = (restartTargetUs > nowUs) ? (uint32_t)(restartTargetUs - nowUs) : 0;
        }
    } else {
        TRM_LOG_WARN("TRM: ACM elapsed %u us exceeds slot3 window %u us, restart immediately",
                     elapsedUs, slot3Us);
        restartTargetUs = TK8710GetTimeUs();
    }

    g_acmCalibState.lastWaitUs = waitUs;

    ret = TK8710FastStartPrepare(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: Failed to prepare fast restart after ACM: %d", ret);
        g_acmCalibState.running = 0;
        return TRM_ERR_DRIVER;
    }

    triggerLateUs = TRM_WaitUntilUs(restartTargetUs);
    fastStartBeginUs = TK8710GetTimeUs();
    ret = TK8710FastStartTrigger(TK8710_MODE_MASTER);
    fastStartEndUs = TK8710GetTimeUs();
    irqAfterStart = TK8710GetIrqStatus();
    TK8710GetS0PeriodStats(&s0AfterStartTimeUs, &s0AfterStartPeriodUs, &s0AfterStartCount);
    g_acmFastStartEndUs = fastStartEndUs;
    g_acmS0PeriodBeforeUs = s0BeforePeriodUs;
    g_acmS0CountBefore = s0BeforeCount;
    g_acmS0MonitorSeq = 0;
    g_acmS0MonitorRemaining = 4;
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: Failed to trigger fast restart after ACM: %d", ret);
        g_acmCalibState.running = 0;
        return TRM_ERR_DRIVER;
    }

    ret = TK8710AdvanceRateAfterS3();
    if (ret != TK8710_OK) {
        TRM_LOG_ERROR("TRM: Failed to advance rate after ACM: %d", ret);
        g_acmCalibState.running = 0;
        return TRM_ERR_DRIVER;
    }

    TRM_CompleteVirtualS3();

    TRM_LOG_INFO("TRM: ACM hidden in slot3: valid=%d elapsed=%u us wait=%u us restartAdvance=%u us "
                 "targetOffset=%u us triggerLate=%u us triggerCost=%u us "
                 "irqAfterCalib=0x%08X irqAfterStart=0x%08X "
                 "s0PeriodBefore=%u us s0PeriodAfterStart=%u us "
                 "s0CountBefore=%u s0CountAfterStart=%u s0DeltaToStart=%u us",
                 calibRet, elapsedUs, waitUs, request.restartAdvanceUs,
                 (uint32_t)(restartTargetUs - startUs), triggerLateUs,
                 (uint32_t)(fastStartEndUs - fastStartBeginUs),
                 irqAfterCalib, irqAfterStart,
                 s0BeforePeriodUs, s0AfterStartPeriodUs,
                 s0BeforeCount, s0AfterStartCount,
                 (uint32_t)(fastStartEndUs - s0BeforeTimeUs));
    g_acmCalibState.running = 0;

    return TRM_OK;
}

/*==============================================================================
 * 扫频控制API实现
 *============================================================================*/

int TRM_StartFrequencySweep(uint32_t start_freq, uint32_t end_freq, uint8_t sweep_mode, uint8_t rate_mode)
{
    if (start_freq == 0 || end_freq == 0 || start_freq > end_freq) {
        TRM_LOG_ERROR("TRM: Invalid sweep parameters: start=%u, end=%u", start_freq, end_freq);
        return TRM_ERR_PARAM;
    }

    if (sweep_mode > 3) {
        TRM_LOG_ERROR("TRM: Invalid sweep mode: %d (should be 0-3)", sweep_mode);
        return TRM_ERR_PARAM;
    }

    /* 计算扫频间隔 */
    uint32_t step_freq = 0;
    switch (sweep_mode) {
        case 0: step_freq = 62500; break;   /* 62.5kHz */
        case 1: step_freq = 125000; break;  /* 125kHz */
        case 2: step_freq = 250000; break;  /* 250kHz */
        case 3: step_freq = 500000; break;  /* 500kHz */
        default:
            TRM_LOG_ERROR("TRM: Unknown sweep mode: %d", sweep_mode);
            return TRM_ERR_PARAM;
    }

    /* 从当前时隙配置获取 RF 选择器 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    if (slotCfg == NULL) {
        TRM_LOG_ERROR("TRM: Failed to get slot config for RF initialization");
        return TRM_ERR_STATE;
    }

    /* 初始化扫频状态 - RF参数从当前配置获取，增益使用默认值 */
    g_sweepState.sweep_active = 1;
    g_sweepState.sweep_mode = sweep_mode;
    g_sweepState.rate_mode = rate_mode;
    g_sweepState.start_freq = start_freq;
    g_sweepState.end_freq = end_freq;
    g_sweepState.current_freq = start_freq;
    g_sweepState.step_freq = step_freq;
    g_sweepState.rfSel = slotCfg->rfSel;
    g_sweepState.rftype = 0;
    /* 增益使用默认值 - 假设初始配置已设置好增益 */
    g_sweepState.rxgain = 0x7E;  /* 默认RX增益 */
    g_sweepState.txgain = 0x2A;  /* 默认TX增益 */
    g_sweepCapturePending = 0;
    g_sweepCaptureWaitCount = 0;
    g_sweepCaptureDone = 0;
    g_sweepCaptureFreq = start_freq;

    TRM_LOG_INFO("TRM: Frequency sweep started: start=%u Hz, end=%u Hz, step=%u Hz, mode=%d, rate=%d, rfSel=0x%02X",
                 start_freq, end_freq, step_freq, sweep_mode, rate_mode, g_sweepState.rfSel);

    return TRM_OK;
}

int TRM_StopFrequencySweep(void)
{
    g_sweepState.sweep_active = 0;
    g_sweepCapturePending = 0;
    g_sweepCaptureWaitCount = 0;
    g_sweepCaptureDone = 0;
    TRM_LOG_INFO("TRM: Frequency sweep stopped");
    return TRM_OK;
}

int TRM_GetSweepState(TRM_SweepState* sweep_state)
{
    if (sweep_state == NULL) {
        TRM_LOG_ERROR("TRM: NULL sweep_state pointer");
        return TRM_ERR_PARAM;
    }
    
    /* 复制当前扫频状态 */
    *sweep_state = g_sweepState;
    
    return TRM_OK;
}
