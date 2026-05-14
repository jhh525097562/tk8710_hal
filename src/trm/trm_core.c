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
#include "../inc/driver/tk8710_rf_regs.h"
#include "../inc/tk8710_noise_api.h"
#include "../port/tk8710_hal.h"
#include "driver/tk8710_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* IPC通信头文件 - 仅在RK3506平台需要 */
#ifdef PLATFORM_RK3506
#include "../inc/tk8710_ipc_comm.h"
#include "../inc/tk8710_scan_ipc_server.h"
#endif

/* usleep定义 - Linux平台使用usleep，Windows使用Sleep */
#ifdef __linux__
#include <unistd.h>
#define usleep(ms) usleep(ms)
#elif defined(_WIN32)
#include <windows.h>
#define usleep(ms) Sleep((ms))
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

/*==============================================================================
 * 公共接口实现
 *============================================================================*/

int TRM_Init(const TRM_InitConfig* config)
{
    int ret;
    
    /* 初始化默认TRM日志系统（如果尚未初始化） */
    TRM_LogInit(TRM_LOG_INFO);
    
    if (config == NULL) {
        TRM_LOG_ERROR("TRM初始化失败: 配置参数为空");
        return TRM_ERR_PARAM;
    }
    
    if (g_trmCtx.state != TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM已经初始化，当前状态: %d", g_trmCtx.state);
        return TRM_ERR_STATE;
    }
    
    /* 初始化TRM日志系统 */
    /* 注意：不重复初始化日志系统，使用全局设置 */
    TRM_LOG_INFO("开始初始化TRM系统");
    
    /* 清零上下文 */
    memset(&g_trmCtx, 0, sizeof(g_trmCtx));
    
    /* 保存配置 */
    memcpy(&g_trmCtx.config, config, sizeof(TRM_InitConfig));
    TRM_LOG_DEBUG("保存TRM配置: beamMaxUsers=%u, beamTimeoutMs=%u, maxFrameCount=%u", 
                  config->beamMaxUsers, config->beamTimeoutMs, config->maxFrameCount);
    
    /* 设置默认值 */
    if (g_trmCtx.config.beamMaxUsers == 0) {
        g_trmCtx.config.beamMaxUsers = TRM_BEAM_MAX_USERS_DEFAULT;
        TRM_LOG_DEBUG("使用默认波束最大用户数: %u", g_trmCtx.config.beamMaxUsers);
    }
    if (g_trmCtx.config.beamTimeoutMs == 0) {
        g_trmCtx.config.beamTimeoutMs = TRM_BEAM_TIMEOUT_DEFAULT;
        TRM_LOG_DEBUG("使用默认波束超时时间: %u ms", g_trmCtx.config.beamTimeoutMs);
    }
    if (g_trmCtx.config.maxFrameCount == 0) {
        g_trmCtx.config.maxFrameCount = 254;  /* 默认最大帧数 */
        TRM_LOG_DEBUG("使用默认最大帧数: %u", g_trmCtx.config.maxFrameCount);
    }
    
    /* 设置全局帧管理参数 */
    g_trmMaxFrameCount = g_trmCtx.config.maxFrameCount;
    TRM_LOG_DEBUG("设置全局最大帧数: %u", g_trmMaxFrameCount);
    
    /* 初始化波束管理 */
    TRM_BeamInit(g_trmCtx.config.beamMaxUsers, g_trmCtx.config.beamTimeoutMs);
    TRM_LOG_INFO("波束管理初始化完成");
    
    /* 初始化发送队列 */
    TRM_DataInit();
    TRM_LOG_INFO("发送队列初始化完成");
    
    g_trmCtx.state = TRM_STATE_INIT;
    TRM_LOG_INFO("TRM系统初始化完成，状态: INIT");
    
    /* 注册TRM到Driver的回调函数 */
    ret = TRM_RegisterDriverCallbacks();
    if (ret != TRM_OK) {
        TRM_LOG_ERROR("TRM Driver回调注册失败: 错误码=%d", ret);
        return ret;
    }
    TRM_LOG_INFO("TRM Driver回调注册完成");

    return TRM_OK;
}

int TRM_Deinit(void)
{
    if (g_trmCtx.state == TRM_STATE_UNINIT) {
        TRM_LOG_WARN("TRM未初始化，无需清理");
        return TRM_OK;
    }
    
    TRM_LOG_INFO("开始清理TRM系统");
    
    /* TRM不直接控制Driver停止，由DriverManager控制 */
    
    /* 清理波束管理 */
    TRM_BeamDeinit();
    TRM_LOG_INFO("波束管理清理完成");
    
    /* 清理发送队列 */
    TRM_DataDeinit();
    TRM_LOG_INFO("发送队列清理完成");
    
    g_trmCtx.state = TRM_STATE_UNINIT;
    TRM_LOG_INFO("TRM系统清理完成，状态: UNINIT");
    
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
        TRM_LOG_WARN("TRM未初始化，无法注册Driver回调");
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
    
    TRM_LOG_INFO("TRM Driver回调注册完成");
    return TRM_OK;
}

/* 内部函数实现 */
TrmContext* TRM_GetContext(void)
{
    return &g_trmCtx;
}

/* 多回调适配函数实现 */
static void TRM_OnDriverSlotRxAdapter(TK8710IrqResult* irqResult)
{
    /* 更新统计信息 */
    g_trmCtx.stats.rxCount++;

    /* 调试：记录中断类型 */
    TRM_LOG_DEBUG("TRM: Received RX interrupt type=%d", irqResult->irq_type);

    /* 扫频采数逻辑 - 仅在 S3 时隙且 g_trmCurrentFrame % 3 == 0 时采数 */
    if (g_sweepState.sweep_active) {
        if ((g_trmCurrentFrame % 3) != 0) {
            TRM_LOG_DEBUG("TRM: Skip sweep capture at frame %u", g_trmCurrentFrame);
        } else {
            TRM_LOG_DEBUG("TRM: Performing sweep capture at RX");
            /* 采数计算噪底 */
            int captureRet = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, TK8710_DBG_OPT_GET, NULL, NULL);
            if (captureRet == TK8710_OK) {
                TRM_LOG_DEBUG("采集数据功能执行成功\n");
                /* 采集数据成功后计算噪底能量并保存扫频结果 */
                uint8_t append_result = (g_sweepState.current_freq != g_sweepState.start_freq);
                tk8710_sweep_noise_process("8710CaptureData", g_sweepState.rate_mode,
                                          g_sweepState.current_freq, append_result);
            } else {
                TRM_LOG_DEBUG("采集数据功能执行失败: ret=%d\n", captureRet);
            }
        }
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
            break;
        case TK8710_IRQ_S3:
            /* 检查是否处于扫频状态 */
            if (g_sweepState.sweep_active) {
                TRM_LOG_DEBUG("TRM: Sweep active, current frame=%u", g_trmCurrentFrame);

                if ((g_trmCurrentFrame % 3) != 0) {
                    TRM_LOG_DEBUG("TRM: Skip sweep processing at frame %u", g_trmCurrentFrame);
                } else {
                    TK8710ScanIpcNotifySweepRunning();
                    TRM_LOG_DEBUG("TRM: Performing frequency configuration");
                }

                /* 检测结束状态并切换下一个频点 */
                g_sweepState.current_freq += g_sweepState.step_freq;
                if (g_sweepState.current_freq > g_sweepState.end_freq) {
                    /* 扫频完成，停止扫频 */
                    g_sweepState.sweep_active = 0;
                    TRM_LOG_INFO("TRM: Frequency sweep completed");
                    TK8710ScanIpcNotifySweepDone();
                    IpcCommClearConfigReceived();
                    /* IPC通信 - 仅在RK3506平台需要 */
                    int request_count = 0;
                    while (request_count < 3) {
                        TRM_LOG_INFO("发送第%d次配置请求...\n", request_count + 1);
                        if (IpcCommSendConfigRequest(&g_ipc_ctx) != 0) {
                            TRM_LOG_INFO("配置请求发送失败\n");
                        }

                        for (int i = 0; i < 100 && !IpcCommIsConfigReceived(); i++) {
                            usleep(100000);
                        }

                        request_count++;
                    }
                } else {
                    /* 切换到下一个频点 - 使用 g_sweepState 中保存的 RF 配置 */
                    /* 6. RX频率配置 (24bit: MSB/MID/LSB) */
                    int ret = TK8710_OK;
                    double freq_step;
                    uint32_t freq_reg;

                    /* 根据射频类型选择频率步进 */
                    if (g_sweepState.rftype == TK8710_RF_TYPE_1257_32M) {
                        freq_step = RF_SX1257_FREQ_STEP;
                    } else {
                        freq_step = RF_SX1255_FREQ_STEP;
                    }
                    freq_reg = (uint32_t)((double)g_sweepState.current_freq / freq_step);

                    TRM_LOG_INFO("TRM: Switching to frequency %u Hz (step=%.2f, reg=0x%06X)",
                                 g_sweepState.current_freq, freq_step, freq_reg);

                    /* RX频率 */
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

                    /* 7. RX增益配置 */
                    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_RX_GAIN >> 8, g_sweepState.rxgain);
                    if (ret != TK8710_OK) {
                        TRM_LOG_ERROR("TRM: RX gain configuration failed: %d", ret);
                        return;
                    }
                    TRM_LOG_DEBUG("TRM: RX gain set to: 0x%02X", g_sweepState.rxgain);

                    /* 9. TX频率 */
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

                    /* 10. TX增益配置 */
                    ret = tk8710_rf_write(g_sweepState.rfSel, RF_CMD_TX_GAIN >> 8, g_sweepState.txgain);
                    if (ret != TK8710_OK) {
                        TRM_LOG_ERROR("TRM: TX gain configuration failed: %d", ret);
                        return;
                    }
                    TRM_LOG_DEBUG("TRM: TX gain set to: 0x%02X", g_sweepState.txgain);
                    TRM_LOG_INFO("TRM: Frequency switch to %u Hz completed", g_sweepState.current_freq);
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

    TRM_LOG_INFO("TRM: Frequency sweep started: start=%u Hz, end=%u Hz, step=%u Hz, mode=%d, rate=%d, rfSel=0x%02X",
                 start_freq, end_freq, step_freq, sweep_mode, rate_mode, g_sweepState.rfSel);

    return TRM_OK;
}

int TRM_StopFrequencySweep(void)
{
    g_sweepState.sweep_active = 0;
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


