/**
 * @file tk8710_irq.c
 * @brief TK8710 中断处理实现
 * 
 * 中断类型说明:
 * - RX_BCN_IRQ (0): BCN检测成功产生的中断
 * - BRD_UD_IRQ (1): FDL时隙的UD中断
 * - BRD_DATA_IRQ (2): FDL时隙的data中断
 * - MD_UD_IRQ (3): ADL/UL时隙UD中断
 * - MD_DATA_IRQ (4): ADL/UL时隙data中断
 * - S0_IRQ (5): Slot0结束中断
 * - S1_IRQ (6): Slot1结束中断
 * - S2_IRQ (7): Slot2结束中断
 * - S3_IRQ (8): Slot3结束中断
 * - ACM_IRQ (9): ACM校准结束中断
 */

#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_regs.h"
#include "driver/tk8710_log.h"
#include "../port/tk8710_hal.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/* 全局变量 */
static TK8710DriverCallbacks g_driverCallbacks = {0};
static uint8_t g_useMultiCallbacks = 1;  /* 默认使用多回调模式 */
static TK8710IrqResult g_irqResult = {0};
static volatile uint8_t g_irqInProgress = 0;  /* 中断处理进行标志 */

/* 测试控制变量 */
static uint8_t g_forceProcessAllUsers = 0;  /* 是否强制处理所有用户数据（用于测试） */
static uint8_t g_forceMaxUsersTx = 0;      /* 是否强制按最大用户数发送（用于测试） */
static uint8_t g_simulationDataLoaded = 0; /* 是否已加载仿真数据（用于测试） */

/* 中断计数器 */
static uint32_t g_irqCounters[10] = {0};  /* 对应10种中断类型 */

/* 中断处理时间统计 (单位: 微秒) */
static uint32_t g_irqTotalTime[10] = {0};   /* 每种中断总处理时间 */
static uint32_t g_irqMaxTime[10] = {0};     /* 每种中断最大处理时间 */
static uint32_t g_irqMinTime[10] = {0};     /* 每种中断最小处理时间 */
static uint8_t g_irqTimeInitialized[10] = {0}; /* 最小时间初始化标志 */

/* 当前BCN发送天线 */
static uint8_t g_currentBcnAntenna = 0;

/* Buffer管理变量 */
static TK8710RxBuffer g_rxBuffers[128] = {0};      /* 接收数据Buffer */
static TK8710TxBuffer g_txBuffers[128] = {0};      /* 发送数据Buffer */
static TK8710BrdBuffer g_brdBuffers[16] = {0};     /* 广播数据Buffer */
static TK8710SignalInfo g_signalInfo[128] = {0};   /* 接收用户信号质量信息buffer */

/* 用户信息Buffer (用于指定信息发送模式) */
typedef struct {
    uint32_t freq;        /* 频率 */
    uint32_t ahData[16];  /* AH数据 (8天线 × 2 = 16个32位) */
    uint64_t pilotPower;  /* Pilot功率 */
    uint8_t  valid;       /* 数据有效性 */
} TK8710UserInfoBuffer;

static TK8710UserInfoBuffer g_userInfoRxBuffers[128] = {0}; /* 接收用户波束信息Buffer */
static TK8710UserInfoBuffer g_userInfoTxBuffers[128] = {0}; /* 发送用户波束信息Buffer */

/* ANoise获取计数器 */
static uint32_t g_aNoiseGetCount = 0;

/* 中断处理函数声明 */
static void tk8710_handle_rx_bcn(void);
static void tk8710_handle_brd_ud(void);
static void tk8710_handle_brd_data(void);
static void tk8710_handle_md_ud(void);
static void tk8710_handle_md_data(void);
static void tk8710_handle_slot0(void);
static void tk8710_handle_slot1(void);
static void tk8710_handle_slot2(void);
static void tk8710_handle_slot3(void);
static void tk8710_handle_acm(void);
static void tk8710_md_ud_get_user_info(void);
static void tk8710_md_data_process(void);
static void tk8710_md_data_read_signal_info(void);
static void tk8710_s1_auto_tx_process(void);
static void tk8710_s1_manual_tx_process(void);
static void tk8710_s0_bcn_rotation_process(void);
static void tk8710_s1_broadcast_tx_process(void);

/* 中断处理函数表 */
typedef void (*IrqHandler)(void);
static const IrqHandler g_irqHandlers[] = {
    [TK8710_IRQ_RX_BCN]   = tk8710_handle_rx_bcn,
    [TK8710_IRQ_BRD_UD]   = tk8710_handle_brd_ud,
    [TK8710_IRQ_BRD_DATA] = tk8710_handle_brd_data,
    [TK8710_IRQ_MD_UD]    = tk8710_handle_md_ud,
    [TK8710_IRQ_MD_DATA]  = tk8710_handle_md_data,
    [TK8710_IRQ_S0]       = tk8710_handle_slot0,
    [TK8710_IRQ_S1]       = tk8710_handle_slot1,
    [TK8710_IRQ_S2]       = tk8710_handle_slot2,
    [TK8710_IRQ_S3]       = tk8710_handle_slot3,
    [TK8710_IRQ_ACM]      = tk8710_handle_acm,
};

/**
 * @brief 注册Driver回调函数
 * @param callbacks 回调函数结构体指针
 */
void TK8710RegisterCallbacks(const TK8710DriverCallbacks* callbacks)
{
    if (callbacks) {
        memcpy(&g_driverCallbacks, callbacks, sizeof(TK8710DriverCallbacks));
        g_useMultiCallbacks = 1;
        TK8710_LOG_IRQ_INFO("Driver callbacks registered successfully");
    } else {
        memset(&g_driverCallbacks, 0, sizeof(TK8710DriverCallbacks));
        g_useMultiCallbacks = 0;
        TK8710_LOG_IRQ_INFO("Driver callbacks cleared");
    }
    
    /* 重置中断结果 */
    memset(&g_irqResult, 0, sizeof(TK8710IrqResult));
}

/**
 * @brief 中断服务函数 (由HAL层GPIO中断调用)
 * @note 芯片PAD_IRQ管脚拉高时触发，需读取中断状态寄存器判断类型并清理中断
 */
void TK8710_IRQHandler(void)
{
    uint32_t irqStatus;
    int ret;
    
    /* 检查重入保护 */
    if (g_irqInProgress) {
        /* 读取中断状态 (复用外层变量) */
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                                MAC_BASE + offsetof(struct mac, irq_res), 
                                &irqStatus);
        if (ret != TK8710_OK) {
            return; /* 读取失败，直接返回 */
        }
        
        /* 清除中断状态，避免中断丢失 */
        if (irqStatus != 0) {
            TK8710ClearIrqStatus(irqStatus);
            TK8710ClearIrqStatus(irqStatus);
            TK8710ClearIrqStatus(irqStatus);
            TK8710_LOG_IRQ_DEBUG("IRQ in progress, cleared status: 0x%08X", irqStatus);
        }
        return;
    }
    
    /* 设置中断处理进行标志 */
    g_irqInProgress = 1;
    
    /* 1. 读取中断状态寄存器 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                        MAC_BASE + offsetof(struct mac, irq_res), 
                        &irqStatus);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to read interrupt status: %d", ret);
        g_irqInProgress = 0;
        return; /* 读取失败，直接返回 */
    }
    
    TK8710_LOG_IRQ_DEBUG("IRQ status: 0x%08X", irqStatus);
    
    /* 如果没有中断状态，直接返回 */
    if (irqStatus == 0) {
        g_irqInProgress = 0;
        return;
    }
    
    /* 2. 如果是ACM校准中断，直接退出不做处理 */
    if (irqStatus & (1 << TK8710_IRQ_ACM)) {
        TK8710_LOG_IRQ_DEBUG("ACM calibration interrupt detected, skipping processing");
        g_irqInProgress = 0;
        return;
    }
    
    /* 3. 清除中断状态 - 先清除再处理，避免中断丢失 */
    TK8710ClearIrqStatus(irqStatus);
    TK8710ClearIrqStatus(irqStatus);
    TK8710ClearIrqStatus(irqStatus);
    /* 4. 处理每个中断 */
    for (size_t i = 0; i < sizeof(g_irqHandlers) / sizeof(g_irqHandlers[0]); i++) {
        if (irqStatus & (1 << i) && g_irqHandlers[i]) {
            /* 增加中断计数器 */
            g_irqCounters[i]++;
            
            TK8710_LOG_IRQ_TRACE("Handling IRQ %d (count: %u, rateIndex: %u)", i, g_irqCounters[i], g_irqResult.currentRateIndex);
            
            /* 设置中断类型 */
            g_irqResult.irq_type = i;
            
            /* 开始计时 */
            uint64_t startTime = TK8710GetTimeUs();
            
            /* 调用中断处理函数 */
            g_irqHandlers[i]();
            
            /* 结束计时并计算处理时间 */
            uint64_t endTime = TK8710GetTimeUs();
            uint32_t processTime = (uint32_t)(endTime - startTime);
            
            /* 更新时间统计 */
            g_irqTotalTime[i] += processTime;
            
            /* 更新最大时间 */
            if (processTime > g_irqMaxTime[i]) {
                g_irqMaxTime[i] = processTime;
            }
            
            /* 更新最小时间 */
            if (!g_irqTimeInitialized[i]) {
                g_irqMinTime[i] = processTime;
                g_irqTimeInitialized[i] = 1;
            } else if (processTime < g_irqMinTime[i]) {
                g_irqMinTime[i] = processTime;
            }
            
            /* 打印处理时间 (可选，用于调试) */
            TK8710_LOG_IRQ_DEBUG("IRQ %d processed in %u us", i, processTime);
            
            /* 为每个中断调用用户回调 (如果注册了) */
            if (g_useMultiCallbacks) {
                /* 使用多回调模式 */
                switch (i) {
                    case TK8710_IRQ_MD_DATA:
                        if (g_driverCallbacks.onRxData) {
                            g_driverCallbacks.onRxData(&g_irqResult);
                        }
                        break;
                    case TK8710_IRQ_S1:
                        if (g_driverCallbacks.onTxSlot) {
                            g_driverCallbacks.onTxSlot(&g_irqResult);
                        }
                        break;
                    case TK8710_IRQ_S0:
                    case TK8710_IRQ_S2:
                    case TK8710_IRQ_S3:
                        if (g_driverCallbacks.onSlotEnd) {
                            g_driverCallbacks.onSlotEnd(&g_irqResult);
                        }
                        break;
                    case TK8710_IRQ_MD_UD:
                        break;
                    case TK8710_IRQ_RX_BCN:
                        break;
                    case TK8710_IRQ_BRD_UD://slave时，TRM需要考虑
                        break;
                    case TK8710_IRQ_BRD_DATA://slave，TRM需要考虑
                        break;
                    default:
                        /* 其他中断类型，调用错误回调 */
                        if (g_driverCallbacks.onError) {
                            g_driverCallbacks.onError(&g_irqResult);
                        }
                        break;
                }
            } else {
                /* 未注册回调，记录警告 */
                TK8710_LOG_IRQ_WARN("No callback registered for interrupt %d", i);
            }
        }
    }
    
    /* 清除中断处理进行标志 */
    g_irqInProgress = 0;
}

/**
 * @brief 获取当前中断状态
 * @return 中断状态位图
 */
uint32_t TK8710GetIrqStatus(void)
{
    uint32_t irqStatus;
    int ret;
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                        MAC_BASE + offsetof(struct mac, irq_res), 
                        &irqStatus);
    
    return (ret == TK8710_OK) ? irqStatus : 0;
}

/**
 * @brief 清除中断状态
 * @param mask 要清除的中断位
 */
void TK8710ClearIrqStatus(uint32_t mask)
{
    s_irq_ctrl1 irqCtrl1;
    int ret;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, irq_ctrl1),
                    &irqCtrl1.data);
    if (ret != TK8710_OK) {
        return;
    }
    
    /* 根据mask设置对应的清除位 */
    if (mask & (1 << TK8710_IRQ_RX_BCN))   irqCtrl1.b.rxbcn_intr_clr = 1;
    if (mask & (1 << TK8710_IRQ_BRD_UD))   irqCtrl1.b.brd_ud_intr_clr = 1;
    if (mask & (1 << TK8710_IRQ_BRD_DATA)) irqCtrl1.b.brd_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_MD_UD))    irqCtrl1.b.data_ud_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_MD_DATA))  irqCtrl1.b.data_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_S0))       irqCtrl1.b.s0_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_S1))       irqCtrl1.b.s1_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_S2))       irqCtrl1.b.s2_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_S3))       irqCtrl1.b.s3_irq_clr = 1;
    if (mask & (1 << TK8710_IRQ_ACM))      irqCtrl1.b.acm_irq_clr = 1;
    
    irqCtrl1.data = 0x1ff;
    TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                   MAC_BASE + offsetof(struct mac, irq_ctrl1), 
                   irqCtrl1.data);
}

/**
 * @brief 使能指定中断
 * @param mask 要使能的中断位
 */
void TK8710EnableIrq(uint32_t mask)
{
    s_irq_ctrl0 irqCtrl0;
    int ret;
    
    /* 读取当前中断配置 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                        MAC_BASE + offsetof(struct mac, irq_ctrl0), 
                        &irqCtrl0.data);
    if (ret != TK8710_OK) return;
    
    /* 清除对应的mask位 (0=使能, 1=禁用) */
    if (mask & (1 << TK8710_IRQ_RX_BCN))   irqCtrl0.b.rxbcn_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_BRD_UD))   irqCtrl0.b.brd_ud_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_BRD_DATA)) irqCtrl0.b.brd_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_MD_UD))    irqCtrl0.b.md_ud_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_MD_DATA))  irqCtrl0.b.md_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_S0))       irqCtrl0.b.s0_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_S1))       irqCtrl0.b.s1_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_S2))       irqCtrl0.b.s2_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_S3))       irqCtrl0.b.s3_irq_mask = 0;
    if (mask & (1 << TK8710_IRQ_ACM))      irqCtrl0.b.acm_irq_mask = 0;
    
    TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                   MAC_BASE + offsetof(struct mac, irq_ctrl0), 
                   irqCtrl0.data);
}

/**
 * @brief 禁用指定中断
 * @param mask 要禁用的中断位
 */
void TK8710DisableIrq(uint32_t mask)
{
    s_irq_ctrl0 irqCtrl0;
    int ret;
    
    /* 读取当前中断配置 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                        MAC_BASE + offsetof(struct mac, irq_ctrl0), 
                        &irqCtrl0.data);
    if (ret != TK8710_OK) return;
    
    /* 设置对应的mask位 (0=使能, 1=禁用) */
    if (mask & (1 << TK8710_IRQ_RX_BCN))   irqCtrl0.b.rxbcn_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_BRD_UD))   irqCtrl0.b.brd_ud_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_BRD_DATA)) irqCtrl0.b.brd_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_MD_UD))    irqCtrl0.b.md_ud_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_MD_DATA))  irqCtrl0.b.md_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_S0))       irqCtrl0.b.s0_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_S1))       irqCtrl0.b.s1_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_S2))       irqCtrl0.b.s2_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_S3))       irqCtrl0.b.s3_irq_mask = 1;
    if (mask & (1 << TK8710_IRQ_ACM))      irqCtrl0.b.acm_irq_mask = 1;
    
    TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                   MAC_BASE + offsetof(struct mac, irq_ctrl0), 
                   irqCtrl0.data);
}

/**
 * @brief 获取中断处理时间统计
 * @param irqType 中断类型 (0-9)
 * @param totalTime 总处理时间 (输出参数)
 * @param maxTime 最大处理时间 (输出参数)
 * @param minTime 最小处理时间 (输出参数)
 * @param count 中断次数 (输出参数)
 * @return TK8710_OK 成功，TK8710_ERROR 参数错误
 */
int TK8710GetIrqTimeStats(uint8_t irqType, uint32_t* totalTime, 
                          uint32_t* maxTime, uint32_t* minTime, uint32_t* count)
{
    if (irqType >= sizeof(g_irqCounters)/sizeof(g_irqCounters[0]) ||
        !totalTime || !maxTime || !minTime || !count) {
        return TK8710_ERR;
    }
    
    *totalTime = g_irqTotalTime[irqType];
    *maxTime = g_irqMaxTime[irqType];
    *minTime = g_irqTimeInitialized[irqType] ? g_irqMinTime[irqType] : 0;
    *count = g_irqCounters[irqType];
    
    return TK8710_OK;
}

/**
 * @brief 重置中断处理时间统计
 * @param irqType 中断类型 (0-9)，255表示重置所有
 * @return TK8710_OK 成功，TK8710_ERROR 参数错误
 */
int TK8710ResetIrqTimeStats(uint8_t irqType)
{
    if (irqType == 255) {
        /* 重置所有中断的时间统计 */
        memset(g_irqTotalTime, 0, sizeof(g_irqTotalTime));
        memset(g_irqMaxTime, 0, sizeof(g_irqMaxTime));
        memset(g_irqMinTime, 0, sizeof(g_irqMinTime));
        memset(g_irqTimeInitialized, 0, sizeof(g_irqTimeInitialized));
    } else if (irqType < sizeof(g_irqCounters)/sizeof(g_irqCounters[0])) {
        /* 重置指定中断的时间统计 */
        g_irqTotalTime[irqType] = 0;
        g_irqMaxTime[irqType] = 0;
        g_irqMinTime[irqType] = 0;
        g_irqTimeInitialized[irqType] = 0;
    } else {
        return TK8710_ERR;
    }
    
    return TK8710_OK;
}

/**
 * @brief 打印中断处理时间统计报告
 */
void TK8710PrintIrqTimeStats(void)
{
    const char* irqNames[] = {
        "RX_BCN", "BRD_UD", "BRD_DATA", "MD_UD", "MD_DATA",
        "S0", "S1", "S2", "S3", "ACM"
    };
    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_ALL,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1
    };
    TK8710LogInit(&defaultLogConfig);
    TK8710_LOG_IRQ_INFO("=== 中断处理时间统计报告 ===");
    TK8710_LOG_IRQ_INFO("%-8s %-8s %-8s %-8s %-8s %-12s", 
                        "类型", "次数", "总时间", "平均时间", "最大时间", "最小时间");
    TK8710_LOG_IRQ_INFO("------------------------------------------------");
    
    for (size_t i = 0; i < sizeof(g_irqCounters) / sizeof(g_irqCounters[0]); i++) {
        if (g_irqCounters[i] > 0) {
            uint32_t avgTime = g_irqTotalTime[i] / g_irqCounters[i];
            uint32_t minTime = g_irqTimeInitialized[i] ? g_irqMinTime[i] : 0;
            
            TK8710_LOG_IRQ_INFO("%-8s %-8u %-8u %-8u %-8u %-12u", 
                                irqNames[i], g_irqCounters[i], 
                                g_irqTotalTime[i], avgTime, 
                                g_irqMaxTime[i], minTime);
        }
    }
    
    TK8710_LOG_IRQ_INFO("================================================");
}

/**
 * @brief 重置中断计数器
 */
void TK8710ResetIrqCounters(void)
{
    memset(g_irqCounters, 0, sizeof(g_irqCounters));
    /* 同时重置时间统计 */
    TK8710ResetIrqTimeStats(255);
    TK8710_LOG_IRQ_INFO("IRQ counters reset");
    memset(&g_irqResult, 0, sizeof(TK8710IrqResult));
    
    /* TODO: 如果有硬件计数器，需要重置硬件寄存器 */
}

/**
 * @brief 获取中断结果
 * @param result 输出结果指针，包含:
 *        - irq_type: 中断类型
 *        - bcn_freq_offset: BCN rx offset
 *        - rx_bcnbits: 接收BCN bit数
 *        - rxbcn_status: 接收BCN状态
 *        - mdUserDataValid: MD_UD用户波束信息有效性
 *        - brdUserDataValid: BRD_UD用户波束信息有效性
 *        - mdDataValid: MD_DATA数据有效性
 *        - crcValidCount: CRC正确的用户数量
 *        - crcErrorCount: CRC错误的用户数量
 *        - maxUsers: 当前速率模式最大用户数
 *        - crcResults[128]: CRC结果数组 (最多128个用户)
 *        - brdDataValid: BRD_DATA数据有效性
 *        - brdCrcValidCount: CRC正确的用户数量
 *        - brdCrcErrorCount: CRC错误的用户数量
 *        - brdMaxUsers: 当前速率模式最大用户数
 *        - brdCrcResults[16]: CRC结果数组 (最多16个用户)
 *        - autoTxValid: 自动发送数据有效性
 *        - autoTxCount: 自动发送用户数量
 *        - brdTxValid: 广播发送数据有效性
 *        - brdTxCount: 广播发送用户数量
 *        - signalInfoValid: 信号信息有效性
 *        - currentRateIndex: 当前速率序号 (0-based)
 */
int TK8710GetIrqResult(TK8710IrqResult* result)
{
    if (result == NULL) return TK8710_ERR;
    
    /* 复制全局中断结果 */
    *result = g_irqResult;
    return TK8710_OK;
}

/**
 * @brief 设置是否强制处理所有用户数据（测试接口）
 * @param enable 1-强制处理所有用户数据，0-按CRC结果正常处理
 * @note 此函数仅用于测试中断处理能力
 */
void TK8710SetForceProcessAllUsers(uint8_t enable)
{
    g_forceProcessAllUsers = enable ? 1 : 0;
    TK8710_LOG_IRQ_INFO("Force process all users: %s", enable ? "ENABLED" : "DISABLED");
}

/**
 * @brief 获取当前是否强制处理所有用户数据
 * @return 1-强制处理，0-正常处理
 */
uint8_t TK8710GetForceProcessAllUsers(void)
{
    return g_forceProcessAllUsers;
}

/**
 * @brief 设置是否强制按最大用户数发送（测试接口）
 * @param enable 1-强制按最大用户数发送，0-按实际输入用户数发送
 * @note 此函数仅用于测试中断处理能力
 */
void TK8710SetForceMaxUsersTx(uint8_t enable)
{
    g_forceMaxUsersTx = enable ? 1 : 0;
    TK8710_LOG_IRQ_INFO("Force max users TX: %s", enable ? "ENABLED" : "DISABLED");
}

/**
 * @brief 获取当前是否强制按最大用户数发送
 * @return 1-强制发送，0-正常发送
 */
uint8_t TK8710GetForceMaxUsersTx(void)
{
    return g_forceMaxUsersTx;
}

/**
 * @brief 获取中断计数器
 * @param irqType 中断类型 (0-9)
 * @return 中断发生次数
 */
uint32_t TK8710GetIrqCounter(uint8_t irqType)
{
    if (irqType >= sizeof(g_irqCounters)/sizeof(g_irqCounters[0])) {
        return 0;
    }
    return g_irqCounters[irqType];
}

/**
 * @brief 获取所有中断计数器
 * @param counters 输出数组指针，至少10个元素
 */
void TK8710GetAllIrqCounters(uint32_t* counters)
{
    if (counters == NULL) return;
    
    memcpy(counters, g_irqCounters, sizeof(g_irqCounters));
}

/* ============================================================================
 * 多速率配置函数
 * ============================================================================
 */

/**
 * @brief 根据速率索引配置多速率参数
 * @param rateIndex 当前速率索引 (0-based)
 * @return TK8710_OK 成功，其他值失败
 */
static int tk8710_configure_multi_rate(uint8_t rateIndex)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    s_mac_config_0 macConfig0;
    s_mac_config_1 macConfig1;
    s_init_2 init2;
    s_init_3 init3;
    s_init_4 init4;
    s_init_9 init9;
    int ret;
    
    if(rateIndex % slotCfg->rateCount==0){
        init9.data = 0;
        init9.b.ant_en = slotCfg->antEn;
        init9.b.rf_sel = slotCfg->rfSel;
        init9.b.tx_bcn_ant_en = slotCfg->txBcnAntEn;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
    }else{
        init9.data = 0;
        init9.b.ant_en = slotCfg->antEn;
        init9.b.rf_sel = slotCfg->rfSel;
        init9.b.tx_bcn_ant_en = 0;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
    }
    
    if (slotCfg == NULL || rateIndex >= slotCfg->rateCount) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters for multi-rate config");
        return TK8710_ERR;
    }
    
    TK8710_LOG_IRQ_INFO("Configuring multi-rate index %d (rate mode: %d)", 
                        rateIndex, slotCfg->rateModes[rateIndex]);
    
    /* 配置mac_config_0 (0x00): s0_cfg, s1_cfg, mode */
    macConfig0.data = 0;
    macConfig0.b.s0_cfg = 0;  /* BCN时隙 */
    macConfig0.b.s1_cfg = slotCfg->s1Cfg[rateIndex].byteLen & 0x3FF;
    macConfig0.b.mode = slotCfg->rateModes[rateIndex] & 0xFF;  /* 使用当前速率索引的模式 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, mac_config_0), macConfig0.data);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to configure mac_config_0 for rate %d", rateIndex);
        return ret;
    }
    
    /* 配置mac_config_1 (0x04): s2_cfg, s3_cfg, pl_crc_en */
    macConfig1.data = 0;
    macConfig1.b.s2_cfg = slotCfg->s2Cfg[rateIndex].byteLen & 0x3FF;
    macConfig1.b.s3_cfg = slotCfg->s3Cfg[rateIndex].byteLen & 0x3FF;
    macConfig1.b.pl_crc_en = slotCfg->plCrcEn & 0x01;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, mac_config_1), macConfig1.data);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to configure mac_config_1 for rate %d", rateIndex);
        return ret;
    }
    
    /* 配置 init_2: da1_m */
    init2.data = 0;
    init2.b.da1_m = slotCfg->s1Cfg[rateIndex].da_m & 0xFFFFFF;  /* da1_m from S1[rateIndex] */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, init_2), init2.data);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to configure init_2 for rate %d", rateIndex);
        return ret;
    }
    
    /* 配置 init_3: da2_m, tx_fix_freq */
    init3.data = 0;
    init3.b.da2_m = slotCfg->s2Cfg[rateIndex].da_m & 0xFFFFFF;  /* da2_m from S2[rateIndex] */
    init3.b.tx_fix_freq = slotCfg->txBeamCtrlMode & 0x01;     /* tx_fix_freq */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, init_3), init3.data);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to configure init_3 for rate %d", rateIndex);
        return ret;
    }
    
    /* 配置 init_4: da3_m */
    init4.data = 0;
    init4.b.da3_m = slotCfg->s3Cfg[rateIndex].da_m & 0xFFFFFF;  /* da3_m from S3[rateIndex] */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, init_4), init4.data);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to configure init_4 for rate %d", rateIndex);
        return ret;
    }
    
    /* 配置 init_18: da0_m */
    {
        s_init_18 init18;
        init18.data = 0;
        init18.b.da0_m = slotCfg->s0Cfg[rateIndex].da_m & 0xFFFFFF;  /* da0_m from S0[rateIndex] */
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
            MAC_BASE + offsetof(struct mac, init_18), init18.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to configure init_18 for rate %d", rateIndex);
            return ret;
        }
    }
    
    TK8710_LOG_IRQ_DEBUG("Multi-rate configuration completed for index %d", rateIndex);
    return TK8710_OK;
}

/* ============================================================================
 * 中断处理函数实现
 * ============================================================================
 */

/**
 * @brief 处理RX BCN中断
 */
static void tk8710_handle_rx_bcn(void)
{
    TK8710_LOG_IRQ_DEBUG("RX BCN interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_RX_BCN]);
    
    /* TODO: 实现BCN接收处理 */
    g_irqResult.irq_type = TK8710_IRQ_RX_BCN;
    
    /* 读取BCN频偏和BCN bits */
    {
        s_bcn_obv1 bcnObv1;
        s_bcn_obv2 bcnObv2;
        
        /* 读取BCN状态信息 */
        int ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                                RX_BCN_BASE + offsetof(struct rx_bcn, bcn_obv1), 
                                &bcnObv1.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to read bcn_obv1 register");
            return;
        }
        
        /* 读取BCN频偏和bits信息 */
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                           RX_BCN_BASE + offsetof(struct rx_bcn, bcn_obv2), 
                           &bcnObv2.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to read bcn_obv2 register");
            return;
        }
        
        /* 设置中断结果中的BCN信息 */
        g_irqResult.bcn_freq_offset = (int16_t)bcnObv2.b.freq_offset;  /* 转换为有符号数 */
        /* 如果offset大于32768，则减去65536（处理16位有符号数的负数表示） */
        if (g_irqResult.bcn_freq_offset > 32768) {
            g_irqResult.bcn_freq_offset -= 65536;
        }
        g_irqResult.rx_bcnbits = bcnObv2.b.bcn_bits_out;
        g_irqResult.rxbcn_status = bcnObv1.b.sync_on;  /* 同步状态 */
        
        /* 打印读取到的BCN信息 */
        TK8710_LOG_IRQ_INFO("BCN Info: bits=%u, freq_offset=%d, q=%u, sync=%u", 
                           bcnObv2.b.bcn_bits_out, (int16_t)bcnObv2.b.freq_offset,
                           bcnObv1.b.bcn_q, bcnObv1.b.sync_on);
    }
    
}

/**
 * @brief 处理BRD UD中断
 */
static void tk8710_handle_brd_ud(void)
{
    TK8710_LOG_IRQ_DEBUG("BRD UD interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_BRD_UD]);
    
    /* TODO: 实现广播UD处理 */
    g_irqResult.irq_type = TK8710_IRQ_BRD_UD;
}

/**
 * @brief 处理BRD DATA中断
 */
static void tk8710_handle_brd_data(void)
{
    TK8710_LOG_IRQ_DEBUG("BRD DATA interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_BRD_DATA]);
    
    /* TODO: 实现广播数据处理 */
    g_irqResult.irq_type = TK8710_IRQ_BRD_DATA;
    
    /* 读取CRC状态 */
    /* TODO: 从相应寄存器读取crc_status */
    
    /* 读取接收数据成功总数 */
    /* TODO: 从obv_0读取counters_rx_user_total */
}

/**
 * @brief 处理MD UD中断
 */
static void tk8710_handle_md_ud(void)
{
    TK8710_LOG_IRQ_DEBUG("MD UD interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_MD_UD]);
    
    /* 设置中断类型 */
    g_irqResult.irq_type = TK8710_IRQ_MD_UD;
    g_irqResult.mdUserDataValid = 0;  /* 默认无效 */
    
    /* 检查是否为指定信息发送模式 */
    if (TK8710GetTxBeamCtrlMode() == 1) {
        // /* 指定信息发送模式：在中断中获取用户信息（Master和Loopback模式） */
        // if (TK8710GetWorkType() == TK8710_MODE_MASTER || TK8710GetWorkType() == TK8710_MODE_LOOPBACK) {
        //     tk8710_md_ud_get_user_info();
        // } else {
        //     TK8710_LOG_IRQ_DEBUG("Slave mode, skip user info retrieval");
        // }
        tk8710_md_ud_get_user_info();
    } else {
        /* 自动发送模式：不需要获取用户信息 */
        TK8710_LOG_IRQ_DEBUG("Auto TX mode, skip user info retrieval");
    }
}

/**
 * @brief MD_UD中断中获取用户信息 (指定信息发送模式)
 */
static void tk8710_md_ud_get_user_info(void)
{
    int ret;
    uint8_t rxBuf[5120]; /* 缓冲区大小: 128用户 × 40字节(AH最大) = 5120字节 */
    uint8_t maxUsers;
    uint8_t dataUserNum;
    uint8_t i;
    
    /* 获取当前配置 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    RateModeParams rateParams;
    
    /* 根据速率模式获取最大用户数 */
    if (TK8710GetRateModeParams(slotCfg->rateModes[g_irqResult.currentRateIndex], &rateParams) == TK8710_OK) {
        dataUserNum = rateParams.maxUsers;
        TK8710_LOG_IRQ_DEBUG("Rate mode %d: max users = %d", 
                            slotCfg->rateModes[g_irqResult.currentRateIndex], dataUserNum);
    } else {
        dataUserNum = 16;  /* 默认值，获取失败时使用 */
        TK8710_LOG_IRQ_WARN("Failed to get rate mode params, use default data users = 16");
    }
    
    /* MD_UD中断只处理数据用户，不包含广播用户 */
    maxUsers = dataUserNum;
    
    TK8710_LOG_IRQ_DEBUG("Getting user info for %d data users", maxUsers);
    
    /* 1. 获取用户频率 (GetInfo type=0) */
    ret = TK8710SpiGetInfo(TK8710_GET_INFO_FREQ, rxBuf, maxUsers * 4);
    if (ret == 0) {
        for (i = 0; i < maxUsers; i++) {
            g_userInfoRxBuffers[i].freq = *((uint32_t*)&rxBuf[i*4]);
            g_userInfoRxBuffers[i].valid = 1;
        }
        TK8710_LOG_IRQ_DEBUG("User freq retrieved successfully");
    } else {
        TK8710_LOG_IRQ_ERROR("Failed to get user freq: %d", ret);
        return;
    }
    
    /* 2. 获取用户信道 (GetInfo type=1) */
    /* 每个用户AH为40bit × 8天线 = 320bit = 40字节 */
    ret = TK8710SpiGetInfo(TK8710_GET_INFO_AH, rxBuf, maxUsers * 40);
    if (ret == 0) {
        for (i = 0; i < maxUsers; i++) {
            /* 获取所有8天线的AH数据 */
            for (int ant = 0; ant < 8; ant++) {
                /* 直接按字节顺序获取AH数据，不做调整 */
                uint64_t ah40 = *((uint64_t*)&rxBuf[i*40 + ant*5]);
                
                /* 保存每个天线的AH数据到userInfoBuffer */
                g_userInfoRxBuffers[i].ahData[ant*2] = (uint32_t)((ah40 >> 20) & 0xFFFFF);   /* I: 20bit */
                g_userInfoRxBuffers[i].ahData[ant*2 + 1] = (uint32_t)(ah40 & 0xFFFFF);      /* Q: 20bit */
            }
        }
        TK8710_LOG_IRQ_DEBUG("User channel retrieved successfully");
    } else {
        TK8710_LOG_IRQ_ERROR("Failed to get user channel: %d", ret);
        return;
    }
    
    /* 3. 获取Pilot功率 (GetInfo type=3) */
    ret = TK8710SpiGetInfo(TK8710_GET_INFO_PILOT_POW, rxBuf, maxUsers * 5);
    if (ret == 0) {
        for (i = 0; i < maxUsers; i++) {
            uint64_t pilotPower = *((uint64_t*)&rxBuf[i*5]) & 0x0FFFFFFFFFF;  /* 40位功率, 取低40位 */
            g_userInfoRxBuffers[i].pilotPower = pilotPower;
        }
        TK8710_LOG_IRQ_DEBUG("User pilot power retrieved successfully");
    } else {
        TK8710_LOG_IRQ_ERROR("Failed to get user pilot power: %d", ret);
        return;
    }
    
    /* 获取8根天线的ANoise信息 */
    uint8_t aNoiseData[32];  /* 8根天线，每根4字节（32位） */
    ret = TK8710SpiGetInfo(TK8710_GET_INFO_ANOISE, aNoiseData, sizeof(aNoiseData));
    if (ret == TK8710_OK) {
        /* 将ANoise数据转换为uint32_t数组 */
        for (int i = 0; i < 8; i++) {
            g_irqResult.ANoiseInfo[i] = (aNoiseData[i*4] << 24) | (aNoiseData[i*4 + 1] << 16) | 
                                     (aNoiseData[i*4 + 2] << 8) | aNoiseData[i*4 + 3];
        }
        
        /* 增加计数器 */
        g_aNoiseGetCount++;
        
        /* 每10次打印一次ANoise值 */
        if (g_aNoiseGetCount % 10 == 0) {
            TK8710_LOG_IRQ_INFO("ANoise values (count=%lu): ANT0=%lu, ANT1=%lu, ANT2=%lu, ANT3=%lu, ANT4=%lu, ANT5=%lu, ANT6=%lu, ANT7=%lu", 
                               g_aNoiseGetCount,
                               g_irqResult.ANoiseInfo[0], g_irqResult.ANoiseInfo[1], 
                               g_irqResult.ANoiseInfo[2], g_irqResult.ANoiseInfo[3],
                               g_irqResult.ANoiseInfo[4], g_irqResult.ANoiseInfo[5], 
                               g_irqResult.ANoiseInfo[6], g_irqResult.ANoiseInfo[7]);
        }
        
        TK8710_LOG_IRQ_DEBUG("ANoise info retrieved successfully for 8 antennas");
    } else {
        TK8710_LOG_IRQ_ERROR("Failed to get ANoise info: %d", ret);
        /* 获取失败时清零ANoise数据 */
        memset(g_irqResult.ANoiseInfo, 0, sizeof(g_irqResult.ANoiseInfo));
    }

    /* 标记数据有效 */
    g_irqResult.mdUserDataValid = 1;
    TK8710_LOG_IRQ_INFO("MD UD user info completed for %d users", maxUsers);
}

/**
 * @brief 处理MD DATA中断
 */
static void tk8710_handle_md_data(void)
{
    TK8710_LOG_IRQ_DEBUG("MD DATA interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_MD_DATA]);
    
    /* 设置中断类型 */
    g_irqResult.irq_type = TK8710_IRQ_MD_DATA;
    g_irqResult.mdDataValid = 0;  /* 默认无效 */
    
    /* 释放之前分配的接收数据内存，防止内存泄漏 */
    for (int i = 0; i < 128; i++) {
        if (g_rxBuffers[i].valid && g_rxBuffers[i].data != NULL) {
            free(g_rxBuffers[i].data);
            g_rxBuffers[i].data = NULL;
        }
        g_rxBuffers[i].dataLen = 0;
        g_rxBuffers[i].valid = 0;
        g_irqResult.crcResults[i].dataValid = 0;
    }
    
    /* 处理MD_DATA中断 */
    tk8710_md_data_process();
    
    /* 读取CRC正确用户的SNR、RSSI、AH、freq数据 */
    tk8710_md_data_read_signal_info();
}

/**
 * @brief MD_DATA中断处理：读取CRC结果和用户数据
 */
static void tk8710_md_data_process(void)
{
    int ret;
    uint32_t crcRegs[4];
    uint8_t maxUsers;
    uint8_t i;
    uint16_t dataLen;
    RateModeParams rateParams;
    
    /* 获取当前速率模式的最大用户数 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    if (TK8710GetRateModeParams(slotCfg->rateModes[g_irqResult.currentRateIndex], &rateParams) == TK8710_OK) {
        maxUsers = rateParams.maxUsers;
        /* 限制最大用户数不超过数组大小 */
        if (maxUsers > 128) {
            maxUsers = 128;
            TK8710_LOG_IRQ_WARN("Rate mode max users %d exceeds array size, limiting to 128", rateParams.maxUsers);
        }
    } else {
        maxUsers = 16;  /* 默认值 */
        TK8710_LOG_IRQ_WARN("Failed to get rate mode params, use default max users = 16");
    }
    
    /* 根据主从模式获取实际数据长度 */
    if (slotCfg->msMode == TK8710_MODE_MASTER) {
        /* Master模式：使用S2时隙配置 */
        dataLen = slotCfg->s2Cfg[g_irqResult.currentRateIndex].byteLen;
        TK8710_LOG_IRQ_DEBUG("Master mode: using S2 config, dataLen=%d", dataLen);
    } else if (slotCfg->msMode == TK8710_MODE_LOOPBACK) {
        /* Loopback模式：使用S3时隙配置 */
        dataLen = slotCfg->s3Cfg[g_irqResult.currentRateIndex].byteLen;
        TK8710_LOG_IRQ_DEBUG("Loopback mode: using S1 config, dataLen=%d", dataLen);
    } else {
        /* Slave模式：使用S3时隙配置 */
        dataLen = slotCfg->s3Cfg[g_irqResult.currentRateIndex].byteLen;
        TK8710_LOG_IRQ_DEBUG("Slave mode: using S3 config, dataLen=%d", dataLen);
    }
    
    g_irqResult.maxUsers = maxUsers;
    TK8710_LOG_IRQ_DEBUG("MD DATA processing for %d users, payload len=%d", maxUsers, dataLen);
    
    /* 读取CRC结果寄存器 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, rx_crc_res0), &crcRegs[0]);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to read rx_crc_res0");
        return;
    }
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, rx_crc_res1), &crcRegs[1]);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to read rx_crc_res1");
        return;
    }
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, rx_crc_res2), &crcRegs[2]);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to read rx_crc_res2");
        return;
    }
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, rx_crc_res3), &crcRegs[3]);
    if (ret != TK8710_OK) {
        TK8710_LOG_IRQ_ERROR("Failed to read rx_crc_res3");
        return;
    }
    
    /* 解析CRC结果 */
    g_irqResult.crcValidCount = 0;
    g_irqResult.crcErrorCount = 0;
    
    for (i = 0; i < maxUsers; i++) {
        uint32_t crcBit = 0;
        uint8_t regIndex = i / 32;
        uint8_t bitIndex = i % 32;
        
        if (regIndex < 4) {
            crcBit = (crcRegs[regIndex] >> bitIndex) & 0x1;
        }
        
        /* 设置CRC结果 */
        g_irqResult.crcResults[i].userIndex = i;
        g_irqResult.crcResults[i].crcValid = crcBit;
        g_irqResult.crcResults[i].dataValid = 0;  /* 初始未读取 */
        g_irqResult.crcResults[i].reserved = 0;
        
        /* 测试模式下强制设置crcValid为有效 */
        if (g_forceProcessAllUsers && !crcBit) {
            g_irqResult.crcResults[i].crcValid = 1;  /* 测试时强制有效 */
        }
        
        /* 读取接收数据并存储到Buffer系统 */
        if (crcBit || g_forceProcessAllUsers) {
            /* 分配数据缓冲区 */
            uint8_t* dataBuffer = malloc(dataLen);
            if (dataBuffer != NULL) {
                /* 读取用户数据 */
                int ret = TK8710ReadBuffer(i, dataBuffer, dataLen);
                if (ret == TK8710_OK) {
                    /* 存储数据到接收Buffer */
                    g_rxBuffers[i].data = dataBuffer;
                    g_rxBuffers[i].dataLen = dataLen;
                    g_rxBuffers[i].valid = 1;
                    g_rxBuffers[i].userIndex = i;
                    
                    g_irqResult.crcResults[i].dataValid = 1;
                    
                    TK8710_LOG_IRQ_DEBUG("User[%d] data read and stored, len=%d, crc=%s%s", 
                                       i, dataLen, crcBit ? "valid" : "invalid",
                                       g_forceProcessAllUsers && !crcBit ? " (forced)" : "");
                } else {
                    TK8710_LOG_IRQ_ERROR("Failed to read user[%d] data", i);
                    free(dataBuffer);
                    g_rxBuffers[i].data = NULL;
                    g_rxBuffers[i].dataLen = 0;
                    g_rxBuffers[i].valid = 0;
                    g_irqResult.crcResults[i].dataValid = 0;
                }
            } else {
                TK8710_LOG_IRQ_ERROR("Failed to allocate buffer for user[%d] data", i);
                g_rxBuffers[i].data = NULL;
                g_rxBuffers[i].dataLen = 0;
                g_rxBuffers[i].valid = 0;
                g_irqResult.crcResults[i].dataValid = 0;
            }
            g_irqResult.crcValidCount++;
        } else {
            g_rxBuffers[i].data = NULL;
            g_rxBuffers[i].dataLen = 0;
            g_rxBuffers[i].valid = 0;
            g_irqResult.crcResults[i].dataValid = 0;
            g_irqResult.crcErrorCount++;
        }
    }
    
    TK8710_LOG_IRQ_DEBUG("CRC results: valid=%d, error=%d", 
                       g_irqResult.crcValidCount, g_irqResult.crcErrorCount);
    
    /* 标记数据有效 */
    g_irqResult.mdDataValid = 1;
    TK8710_LOG_IRQ_INFO("MD DATA processing completed: %d valid users", 
                       g_irqResult.crcValidCount);
}

/**
 * @brief 处理S0时隙结束中断
 */
static void tk8710_handle_slot0(void)
{
    TK8710_LOG_IRQ_DEBUG("S0 slot interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_S0]);
    
    /* 设置中断类型 */
    g_irqResult.irq_type = TK8710_IRQ_S0;
    
    // /* 处理BCN轮流发送 - 仅在Master模式下运行 */
    // if (TK8710GetWorkType() == TK8710_MODE_MASTER) {
    //     tk8710_s0_bcn_rotation_process();
    // }
}

/**
 * @brief S0时隙BCN轮流发送处理
 */
static void tk8710_s0_bcn_rotation_process(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    int ret;
    uint8_t currentAntenna;
    
    /* 检查是否启用BCN轮流发送 (txBcnEn == 0xFF) */
    // if (slotCfg->txBcnAntEn != 0xFF) {
    //     TK8710_LOG_IRQ_DEBUG("BCN rotation disabled (txBcnAntEn=0x%02X)", slotCfg->txBcnAntEn);
        
    //     /* 不是轮流发送时，设置g_currentBcnAntenna为BCN选择的天线 */
    //     g_currentBcnAntenna = 0;  /* 使用天线0 */
    //     TK8710_LOG_IRQ_DEBUG("Set current BCN antenna to RF selection: %d", g_currentBcnAntenna);
    //     return;
    // }
    
    /* 计算当前应该使用的天线 (使用中断计数器循环) */
    uint8_t rotationIndex = g_irqCounters[TK8710_IRQ_S0] % TK8710_MAX_ANTENNAS;
    currentAntenna = slotCfg->bcnRotation[rotationIndex];
    
    /* 更新全局变量，供广播发送函数使用 */
    g_currentBcnAntenna = currentAntenna;
    
    TK8710_LOG_IRQ_DEBUG("BCN rotation: index %u -> antenna %d (count: %u)", 
                         rotationIndex, currentAntenna, g_irqCounters[TK8710_IRQ_S0]);
    
    /* 配置所有天线的BCN发送功率 */
    for (int ant = 0; ant < 8; ant++) {
        uint32_t regAddr = TX_FE_BASE + 0x78 + 0x1000 * ant;  /* tx_config_28 offset = 0x78 */
        
        /* 配置BCN和DA参数 */
        s_tx_config_28 config28;
        config28.data = 0;  /* 直接初始化为0 */
        config28.b.sc_da = 0x40;  /* 设置DA参数 */
        
        if (ant == currentAntenna) {
            /* 当前天线：启用BCN发送 */
            config28.b.sc_bcn = 0x60;
            TK8710_LOG_IRQ_DEBUG("Enabling BCN on antenna %d", ant);
        } else {
            /* 其他天线：禁用BCN发送 */
            config28.b.sc_bcn = 0x00;
        }
        
        /* 写入配置 */
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, regAddr, config28.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to write tx_config_28 for antenna %d", ant);
        } else {
            TK8710_LOG_IRQ_DEBUG("Configured antenna %d: sc_bcn=0x%02X, sc_da=0x%02X", 
                               ant, config28.b.sc_bcn, config28.b.sc_da);
        }
    }
    
    TK8710_LOG_IRQ_INFO("BCN rotation completed: antenna %d active", currentAntenna);
}

/**
 * @brief 清除自动发送用户数据
 * @param userIndex 用户索引 (0-127), 255表示清除所有
 * @return 0-成功, 1-失败
 */
int TK8710ClearTxUserData(uint8_t userIndex)
{
    if (userIndex == 255) {
        /* 清除所有用户数据 */
        for (int i = 0; i < 128; i++) {
            if (g_txBuffers[i].valid && 
                g_txBuffers[i].data != NULL) {
                free(g_txBuffers[i].data);
            }
            memset(&g_txBuffers[i], 0, sizeof(TK8710TxBuffer));
        }
        TK8710_LOG_IRQ_DEBUG("All TX user data cleared");
        return TK8710_OK;
    } else if (userIndex < 128) {
        /* 清除指定用户数据 */
        if (g_txBuffers[userIndex].valid && 
            g_txBuffers[userIndex].data != NULL) {
            free(g_txBuffers[userIndex].data);
        }
        memset(&g_txBuffers[userIndex], 0, sizeof(TK8710TxBuffer));
        TK8710_LOG_IRQ_DEBUG("TX user data cleared: user[%d]", userIndex);
        return TK8710_OK;
    } else {
        TK8710_LOG_IRQ_ERROR("Invalid user index: %d", userIndex);
        return TK8710_ERR;
    }
}

/**
 * @brief 设置下行发送数据和功率
 * @param downlinkType 下行类型: 0=下行1(广播数据), 1=下行2(专用数据)
 * @param index 索引: 下行1时范围(0-15), 下行2时范围(0-127)
 * @param data 数据指针
 * @param dataLen 数据长度
 * @param txPower 发送功率
 * @param beamType 波束类型: 0=广播数据, 1=专用数据
 * @return 0-成功, 1-失败
 */
int TK8710SetTxData(TK8710DownlinkType downlinkType, uint8_t index, const uint8_t* data, uint16_t dataLen, uint8_t txPower, uint8_t beamType)
{
    if (downlinkType == TK8710_DOWNLINK_A) {
        /* 下行1 (广播数据) */
        if (index >= 16 || data == NULL || dataLen == 0) {
            TK8710_LOG_IRQ_ERROR("Invalid parameters for downlink1: index=%d, data=%p, dataLen=%d", 
                                index, data, dataLen);
            return TK8710_ERR;
        }
        
        /* 检查是否已存在数据，先释放 */
        if (g_brdBuffers[index].valid && 
            g_brdBuffers[index].data != NULL) {
            free(g_brdBuffers[index].data);
        }
        
        /* 分配内存并复制数据 */
        uint8_t* newData = malloc(dataLen);
        if (newData == NULL) {
            TK8710_LOG_IRQ_ERROR("Failed to allocate memory for downlink1[%d] data", index);
            return TK8710_ERR;
        }
        
        memcpy(newData, data, dataLen);
        
        /* 设置下行1数据Buffer */
        g_brdBuffers[index].data = newData;
        g_brdBuffers[index].dataLen = dataLen;
        g_brdBuffers[index].valid = 1;
        g_brdBuffers[index].brdIndex = index;
        g_brdBuffers[index].txPower = txPower;
        g_brdBuffers[index].beamType = beamType;
        
        TK8710_LOG_IRQ_DEBUG("Downlink1 TX data set: downlink1[%d], len=%d, power=%d, beamType=%d", 
                            index, dataLen, txPower, beamType);
    } else if (downlinkType == TK8710_DOWNLINK_B) {
        /* 下行2 (专用数据) */
        if (index >= 128 || data == NULL || dataLen == 0) {
            TK8710_LOG_IRQ_ERROR("Invalid parameters for downlink2: index=%d, data=%p, dataLen=%d", 
                                index, data, dataLen);
            return TK8710_ERR;
        }
        
        /* 检查是否已存在数据，先释放 */
        if (g_txBuffers[index].valid && 
            g_txBuffers[index].data != NULL) {
            free(g_txBuffers[index].data);
        }
        
        /* 分配内存并复制数据 */
        uint8_t* newData = malloc(dataLen);
        if (newData == NULL) {
            TK8710_LOG_IRQ_ERROR("Failed to allocate memory for downlink2[%d] data", index);
            return TK8710_ERR;
        }
        
        memcpy(newData, data, dataLen);
        
        /* 设置下行2数据Buffer */
        g_txBuffers[index].data = newData;
        g_txBuffers[index].dataLen = dataLen;
        g_txBuffers[index].valid = 1;
        g_txBuffers[index].userIndex = index;
        g_txBuffers[index].txPower = txPower;
        g_txBuffers[index].beamType = beamType;
        
        TK8710_LOG_IRQ_DEBUG("Downlink2 TX data set: downlink2[%d], len=%d, power=%d, beamType=%d", 
                            index, dataLen, txPower, beamType);
    } else {
        TK8710_LOG_IRQ_ERROR("Invalid downlink type: %d", downlinkType);
        return TK8710_ERR;
    }
    
    return TK8710_OK;
}

/**
 * @brief 设置发送用用户信息 (指定信息发送模式)
 * @param userBufferIdx 用户索引 (0-127)
 * @param freqInfo 频率
 * @param ahInfo AH数据数组 (16个32位数据)
 * @param pilotPowerInfo Pilot功率
 * @return 0-成功, 1-失败
 */
int TK8710SetTxUserInfo(uint8_t userBufferIdx, uint32_t freqInfo, const uint32_t* ahInfo, uint64_t pilotPowerInfo)
{
    if (userBufferIdx >= 128 || ahInfo == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userBufferIdx=%d, ahInfo=%p", userBufferIdx, ahInfo);
        return TK8710_ERR;
    }
    
    /* 设置发送用用户信息 */
    g_userInfoTxBuffers[userBufferIdx].freq = freqInfo;
    g_userInfoTxBuffers[userBufferIdx].pilotPower = pilotPowerInfo;
    g_userInfoTxBuffers[userBufferIdx].valid = 1;
    
    /* 复制AH数据 */
    for (int i = 0; i < 16; i++) {
        g_userInfoTxBuffers[userBufferIdx].ahData[i] = ahInfo[i];
    }
    
    TK8710_LOG_IRQ_DEBUG("TX user info set: user[%d], freq=%u, pilotPower=%llu", 
                        userBufferIdx, freqInfo, pilotPowerInfo);
    return TK8710_OK;
}

/**
 * @brief 获取发送用用户信息
 * @param userIndex 用户索引 (0-127)
 * @param freq 输出频率指针
 * @param ahData 输出AH数据数组 (16个32位数据)
 * @param pilotPower 输出Pilot功率指针
 * @return 0-成功, 1-失败
 */
int TK8710GetTxUserInfo(uint8_t userIndex, uint32_t* freq, uint32_t* ahData, uint64_t* pilotPower)
{
    if (userIndex >= 128 || freq == NULL || ahData == NULL || pilotPower == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userIndex=%d, freq=%p, ahData=%p, pilotPower=%p", 
                            userIndex, freq, ahData, pilotPower);
        return TK8710_ERR;
    }
    
    if (!g_userInfoTxBuffers[userIndex].valid) {
        TK8710_LOG_IRQ_ERROR("TX user info not valid for user[%d]", userIndex);
        return TK8710_ERR;
    }
    
    /* 获取发送用用户信息 */
    *freq = g_userInfoTxBuffers[userIndex].freq;
    *pilotPower = g_userInfoTxBuffers[userIndex].pilotPower;
    
    /* 复制AH数据 */
    for (int i = 0; i < 16; i++) {
        ahData[i] = g_userInfoTxBuffers[userIndex].ahData[i];
    }
    
    TK8710_LOG_IRQ_DEBUG("TX user info retrieved: user[%d], freq=%u, pilotPower=%llu", 
                        userIndex, *freq, *pilotPower);
    return TK8710_OK;
}

/**
 * @brief 清除发送用用户信息
 * @param userIndex 用户索引 (0-127), 255表示清除所有
 * @return 0-成功, 1-失败
 */
int TK8710ClearTxUserInfo(uint8_t userIndex)
{
    if (userIndex == 255) {
        /* 清除所有用户的发送信息 */
        for (int i = 0; i < 128; i++) {
            g_userInfoTxBuffers[i].valid = 0;
        }
        TK8710_LOG_IRQ_DEBUG("All TX user info cleared");
    } else if (userIndex < 128) {
        /* 清除指定用户的发送信息 */
        g_userInfoTxBuffers[userIndex].valid = 0;
        TK8710_LOG_IRQ_DEBUG("TX user info cleared: user[%d]", userIndex);
    } else {
        TK8710_LOG_IRQ_ERROR("Invalid user index: %d", userIndex);
        return TK8710_ERR;
    }
    
    return TK8710_OK;
}

/**
 * @brief 获取接收用户信息 (从MD_UD中断获取的数据)
 * @param userBufferIdx 用户索引 (0-127)
 * @param freqInfo 输出频率指针
 * @param ahInfo 输出AH数据数组 (16个32位数据)
 * @param pilotPowerInfo 输出Pilot功率指针
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserInfo(uint8_t userBufferIdx, uint32_t* freqInfo, uint32_t* ahInfo, uint64_t* pilotPowerInfo)
{
    if (userBufferIdx >= 128 || freqInfo == NULL || ahInfo == NULL || pilotPowerInfo == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userBufferIdx=%d, freqInfo=%p, ahInfo=%p, pilotPowerInfo=%p", 
                            userBufferIdx, freqInfo, ahInfo, pilotPowerInfo);
        return TK8710_ERR;
    }
    
    if (!g_userInfoRxBuffers[userBufferIdx].valid) {
        TK8710_LOG_IRQ_ERROR("RX user info not valid for user[%d]", userBufferIdx);
        return TK8710_ERR;
    }
    
    /* 获取接收用户信息 */
    *freqInfo = g_userInfoRxBuffers[userBufferIdx].freq;
    *pilotPowerInfo = g_userInfoRxBuffers[userBufferIdx].pilotPower;
    
    /* 复制AH数据 */
    for (int i = 0; i < 16; i++) {
        ahInfo[i] = g_userInfoRxBuffers[userBufferIdx].ahData[i];
    }
    
    /* 频率转换：26-bit格式转换为实际频率Hz */
    // uint32_t freq26 = *freqInfo & 0x03FFFFFF;  /* 取26位 */
    // int32_t freqValue = freq26 > (1<<25) ? (int)(freq26 - (1<<26)) : freq26;
    
    // TK8710_LOG_IRQ_DEBUG("RX user info retrieved: user[%d], freq=%u (raw), freqHz=%d, pilotPower=%llu", 
    //                     userBufferIdx, *freqInfo, freqValue/128, *pilotPowerInfo);
    return TK8710_OK;
}


/**
 * @brief 处理S1时隙结束中断
 */
static void tk8710_handle_slot1(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    
    TK8710_LOG_IRQ_DEBUG("S1 slot interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_S1]);
    
    /* 设置中断类型 */
    g_irqResult.irq_type = TK8710_IRQ_S1;
    g_irqResult.autoTxValid = 0;  /* 默认无效 */
    
    // /* 处理BCN轮流发送 - 仅在Master模式下运行 */
    // if (TK8710GetWorkType() == TK8710_MODE_MASTER) {
    //     tk8710_s0_bcn_rotation_process();
    // }
    
    /* 处理S1时隙自动发送 */
    tk8710_s1_auto_tx_process();
    
    /* 处理S1时隙指定信息发送 */
    if (slotCfg->txBeamCtrlMode == 1) {
        /* 如果已加载仿真数据，跳过手动发送处理 */
        if (g_simulationDataLoaded) {
                int ret;
                uint32_t user_val_regs[4] = {0xffffffff,0xffffffff,0xffffffff,0xffffffff}; /* user_val0, user_val1, user_val2, user_val3 */
                /* 写入MAC寄存器 */
                for (int reg = 0; reg < 4; reg++) {
                    uint32_t reg_offset = MAC_BASE + 0x3c + reg * 4;
                    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, reg_offset, user_val_regs[reg]);
                    if (ret == TK8710_OK) {
                        TK8710_LOG_IRQ_DEBUG("Set MAC user_val%d = 0x%08X", reg, user_val_regs[reg]);
                    } else {
                        TK8710_LOG_IRQ_ERROR("Failed to set MAC user_val%d: %d", reg, ret);
                    }
                }
                s_init_17 brdUserVal;
                brdUserVal.data = 0;
                brdUserVal.b.brd_user_val = 0xffff;

                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_17), brdUserVal.data);
                TK8710_LOG_IRQ_DEBUG("Simulation data loaded, skipping manual TX process");
            } else {
                tk8710_s1_manual_tx_process();
            }
    } else {
        /* 处理S1时隙广播发送 */
        tk8710_s1_broadcast_tx_process();
    }
}

/**
 * @brief S1时隙自动发送处理
 */
static void tk8710_s1_auto_tx_process(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint16_t expectedLen;
    uint8_t i;
    
    /* 检查是否启用自动发送模式 */
    if (slotCfg->txBeamCtrlMode != 0) {
        TK8710_LOG_IRQ_DEBUG("Auto TX mode disabled");
        return;
    }
    
    /* 根据主从模式确定期望的数据长度 */
    if (slotCfg->msMode == TK8710_MODE_MASTER) {
        /* Master模式：应该等于S3配置长度 */
        expectedLen = slotCfg->s3Cfg[g_irqResult.currentRateIndex].byteLen;
        TK8710_LOG_IRQ_DEBUG("Master mode: expected data length = %d (S3 config)", expectedLen);
    } else {
        /* Slave模式：应该等于S2配置长度 */
        expectedLen = slotCfg->s2Cfg[g_irqResult.currentRateIndex].byteLen;
        TK8710_LOG_IRQ_DEBUG("Slave mode: expected data length = %d (S2 config)", expectedLen);
    }
    
    /* 统计有效用户数据 */
    g_irqResult.autoTxCount = 0;
    uint8_t maxUsers = g_irqResult.maxUsers;
    for (i = 0; i < maxUsers; i++) {
        if (g_txBuffers[i].valid) {
            g_irqResult.autoTxCount++;
        }
    }
    
    if (g_irqResult.autoTxCount == 0) {
        TK8710_LOG_IRQ_DEBUG("No valid TX user data");
        return;
    }
    
    TK8710_LOG_IRQ_DEBUG("Processing %d TX users", g_irqResult.autoTxCount);
    
    /* 配置所有用户的MAC寄存器有效位 */
    uint32_t user_val_regs[4] = {0}; /* user_val0, user_val1, user_val2, user_val3 */
    
    for (i = 0; i < maxUsers; i++) {
        if (g_txBuffers[i].valid) {
            uint8_t userIndex = g_txBuffers[i].userIndex;
            uint32_t regIndex = userIndex / 32;
            uint32_t bitIndex = userIndex % 32;
            user_val_regs[regIndex] |= (1 << bitIndex);
        }
    }
    
    /* 写入MAC寄存器 - 每次自动发送都更新所有4个寄存器 */
    for (int reg = 0; reg < 4; reg++) {
        uint32_t reg_offset = MAC_BASE + 0x3c + reg * 4;
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, reg_offset, user_val_regs[reg]);
        if (ret == TK8710_OK) {
            TK8710_LOG_IRQ_DEBUG("Set MAC user_val%d = 0x%08X", reg, user_val_regs[reg]);
        } else {
            TK8710_LOG_IRQ_ERROR("Failed to set MAC user_val%d: %d", reg, ret);
        }
    }
    
    /* 发送所有有效用户数据 */
    uint8_t successCount = 0;
    uint8_t errorCount = 0;
    
    for (i = 0; i < maxUsers; i++) {
        if (g_txBuffers[i].valid) {
            uint8_t userIndex = g_txBuffers[i].userIndex;
            uint8_t* userData = g_txBuffers[i].data;
            uint16_t dataLen = g_txBuffers[i].dataLen;
            uint8_t txPower = g_txBuffers[i].txPower;
            
            /* 检查数据长度 */
            if (dataLen != expectedLen) {
                dataLen = expectedLen;
                // TK8710_LOG_IRQ_ERROR("User[%d] data length mismatch: expected=%d, actual=%d", 
                //                    userIndex, expectedLen, dataLen);
                // errorCount++;
                // continue;
            }
            
            /* 发送用户数据 */
            int ret = TK8710WriteBuffer(userIndex, userData, dataLen);
            if (ret == TK8710_OK) {
                /* 设置发送功率 */
                s_tx_pow_ctrl tx_pow_ctrl;
                tx_pow_ctrl.data = 0;
                tx_pow_ctrl.b.UserIndex = userIndex;
                tx_pow_ctrl.b.power = txPower;
                
                int powerRet = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);
                if (powerRet != TK8710_OK) {
                    TK8710_LOG_IRQ_ERROR("Failed to set TX power for user[%d]: %d", userIndex, powerRet);
                } else {
                    TK8710_LOG_IRQ_DEBUG("TX power set for user[%d]: %d", userIndex, txPower);
                }
                
                successCount++;
                
                /* 释放对应的RAM资源 */
                TK8710ClearTxUserData(userIndex);
                
                TK8710_LOG_IRQ_DEBUG("User[%d] TX buffer cleared and RAM freed to prevent repeat transmission", userIndex);
            } else {
                TK8710_LOG_IRQ_ERROR("Failed to send user[%d] data: %d", userIndex, ret);
                errorCount++;
            }
        }
    }
    
    /* 输出汇总统计信息 */
    TK8710_LOG_IRQ_DEBUG("S1 TX summary: success=%d, error=%d", successCount, errorCount);
    
    /* 标记自动发送完成 */
    g_irqResult.autoTxValid = 1;
    TK8710_LOG_IRQ_INFO("S1 auto TX completed: %d users processed", g_irqResult.autoTxCount);
}

/**
 * @brief 处理S2时隙结束中断
 */
static void tk8710_handle_slot2(void)
{
    TK8710_LOG_IRQ_DEBUG("S2 slot interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_S2]);
    
    /* 设置中断类型 */
    g_irqResult.irq_type = TK8710_IRQ_S2;
}

/**
 * @brief 处理S3时隙结束中断
 */
static void tk8710_handle_slot3(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    int ret;
    
    TK8710_LOG_IRQ_DEBUG("S3 slot interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_S3]);
    
    /* 根据rateCount个数循环配置速率 */
    if (slotCfg->rateCount > 1) {
        /* 多速率模式：循环配置不同速率 */
        static uint8_t currentRateIndex = 0;
        
        /* 更新中断结果中的当前速率序号（下一帧将使用的速率） */
        g_irqResult.currentRateIndex = (currentRateIndex + 1) % slotCfg->rateCount;
        
        /* 更新速率索引，循环切换 */
        currentRateIndex = (currentRateIndex + 1) % slotCfg->rateCount;
        
        TK8710_LOG_IRQ_DEBUG("Multi-rate mode: switched to rate index %d/%d", 
                            currentRateIndex, slotCfg->rateCount);

        /* 调用速率配置函数 */
        ret = tk8710_configure_multi_rate(currentRateIndex);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to configure multi-rate %d: %d", currentRateIndex, ret);
        }

    } else {
        /* 单速率模式：保持速率序号为0 */
        g_irqResult.currentRateIndex = 0;
        TK8710_LOG_IRQ_DEBUG("Single rate mode: no rate switching needed");
    }
    
    /* TODO: 实现S3时隙结束处理 */
    g_irqResult.irq_type = TK8710_IRQ_S3;
}

/**
 * @brief 处理ACM校准结束中断
 */
static void tk8710_handle_acm(void)
{
    TK8710_LOG_IRQ_DEBUG("ACM interrupt handled (count: %u)", g_irqCounters[TK8710_IRQ_ACM]);
    
    /* TODO: 实现ACM校准结束处理 */
    g_irqResult.irq_type = TK8710_IRQ_ACM;
}

/**
 * @brief S1时隙指定信息发送处理
 * 
 * 指定信息发送模式：从中断中获取用户的freq、ah、pilotPower等参数进行配置
 */
static void tk8710_s1_manual_tx_process(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    int ret;
    
    TK8710_LOG_IRQ_DEBUG("S1 manual TX process started");
    
    /* 1. 首先处理广播发送作为第一个用户 */
    uint8_t hasBroadcast = 0;
    if (slotCfg->brdUserNum > 0) {
        hasBroadcast = 1;
        TK8710_LOG_IRQ_DEBUG("Processing broadcast as first user in manual TX");
        
        /* 配置广播用户valid位 (MAC init_17寄存器) */
        {
            s_init_17 brdUserVal;
            
            /* 广播用户valid位: bit[brdUserNum-1:0] = 1 */
            brdUserVal.data = 0;
            brdUserVal.b.brd_user_val = (1 << slotCfg->brdUserNum) - 1;
            
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_17), brdUserVal.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_IRQ_ERROR("Failed to configure broadcast user valid bits (init_17)");
                return;
            }
            
        }
        
        TK8710_LOG_IRQ_INFO("Manual TX broadcast configured as first user");
    }
    
    /* 检查是否有有效的用户信息 */
    uint8_t hasUserData = g_irqResult.mdUserDataValid;
    if (!hasUserData) {
        TK8710_LOG_IRQ_DEBUG("No valid user info for manual TX (only broadcast will be processed)");
    }
    
    /* 获取当前速率模式的最大用户数 */
    uint8_t maxUsers = g_irqResult.maxUsers;
    uint8_t actualMaxUsers = maxUsers;
    
    /* 指定信息发送模式：广播占用一个用户位置，所以实际用户数减1 */
    if (hasBroadcast && maxUsers > 0) {
        actualMaxUsers = maxUsers - 1;
    }
    
    /* 统计有效用户数量 */
    uint8_t validUserCount = 0;
    uint8_t validUsers[128] = {0};  /* 有效用户索引数组 */
    if (hasUserData) {
        for (uint8_t i = 0; i < actualMaxUsers; i++) {
            if (g_txBuffers[i].valid) {
                validUsers[validUserCount] = i;
                validUserCount++;
            }
        }
        
        /* 测试模式：强制按最大用户数发送 */
        if (g_forceMaxUsersTx && hasUserData) {
            /* 重新填充validUsers数组为所有用户索引 */
            for (uint8_t i = validUserCount; i < actualMaxUsers; i++) {
                validUsers[i] = i;
            }
            validUserCount = actualMaxUsers;
            TK8710_LOG_IRQ_DEBUG("Force max users TX: using %d users (test mode)", validUserCount);
        }
    }
    
    if (validUserCount == 0 && !hasBroadcast) {
        /* 写入MAC寄存器 */
        for (int reg = 0; reg < 4; reg++) {
            uint32_t reg_offset = MAC_BASE + 0x3c + reg * 4;
            int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, reg_offset, 0);
            if (ret == TK8710_OK) {
                TK8710_LOG_IRQ_DEBUG("Set MAC user_val%d = 0x%08X", reg, 0);
            } else {
                TK8710_LOG_IRQ_ERROR("Failed to set MAC user_val%d: %d", reg, ret);
            }
        }
        TK8710_LOG_IRQ_DEBUG("No valid users found for manual TX");
        return;
    }
    
    TK8710_LOG_IRQ_INFO("Processing manual TX for broadcast: %s (%d users), users: %d/%d (max: %d)", 
                        hasBroadcast ? "yes" : "no", 
                        hasBroadcast ? slotCfg->brdUserNum : 0,
                        validUserCount, actualMaxUsers, maxUsers);
    
    /* 复用最大的缓冲区(5160字节)来处理所有SPI传输 */
    uint8_t* spiBuffer = (uint8_t*)malloc(5160);
    if (spiBuffer == NULL) {
        TK8710_LOG_IRQ_ERROR("Failed to allocate SPI buffer");
        return;
    }
    
    /* 1. 配置频率 - 广播和用户数据合并传输 */
    {
        uint16_t writeLen = 0;
        
        /* 首先添加广播频率 */
        if (hasBroadcast) {
            /* 从slotCfg->brdFreq获取频率数据并填充到spiBuffer */
            for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
                /* TK8710频率格式: 26-bit unsigned (1 sign + 18 integer + 7 decimal) */
                float freqFloat = slotCfg->brdFreq[i];
                uint32_t freq;
                
                /* 将float频率转换为TK8710 26位格式 */
                /* 格式: [sign:1][integer:18][decimal:7] */
                if (freqFloat >= 0) {
                    /* 正数：sign=0 */
                    uint32_t integerPart = (uint32_t)freqFloat;
                    uint32_t decimalPart = (uint32_t)((freqFloat - integerPart) * 128); /* 2^7 = 128 */
                    freq = (integerPart << 7) | decimalPart;
                } else {
                    /* 负数：sign=1 (使用补码) */
                    float absFreq = -freqFloat;
                    uint32_t integerPart = (uint32_t)absFreq;
                    uint32_t decimalPart = (uint32_t)((absFreq - integerPart) * 128);
                    uint32_t magnitude = (integerPart << 7) | decimalPart;
                    freq = ((~magnitude) + 1) & 0x03FFFFFF; /* 26位补码 */
                }
                
                /* 确保频率在26位范围内 */
                freq &= 0x03FFFFFF; /* 26位掩码 */
                
                /* SPI传输按字节顺序发送 */
                spiBuffer[writeLen*4]   = (uint8_t)((freq >> 24) & 0xFF);
                spiBuffer[writeLen*4+1] = (uint8_t)((freq >> 16) & 0xFF);
                spiBuffer[writeLen*4+2] = (uint8_t)((freq >> 8) & 0xFF);
                spiBuffer[writeLen*4+3] = (uint8_t)(freq & 0xFF);
                
                TK8710_LOG_IRQ_DEBUG("Manual TX broadcast[%d] freqFloat=%.2f, freq=0x%08X", i, freqFloat, freq);
                writeLen++;
            }
        }
        
        /* 然后添加所有有效用户的频率信息 */
        for (uint8_t i = 0; i < validUserCount; i++) {
            uint8_t userIndex = validUsers[i];
            
            if (g_forceMaxUsersTx && !g_txBuffers[userIndex].valid) {
                /* 测试模式：使用用户信息Buffer中的频率数据 */
                uint32_t testFreq = g_userInfoRxBuffers[userIndex].freq;
                memcpy(&spiBuffer[writeLen*4], &testFreq, 4);
                TK8710_LOG_IRQ_DEBUG("Manual TX test user[%d]: freq=0x%08X (from userInfo)", userIndex, testFreq);
            } else {
                /* 正常模式：从发送用用户信息Buffer获取频率 */
                uint32_t freq = g_userInfoTxBuffers[userIndex].freq;
                memcpy(&spiBuffer[writeLen*4], &freq, 4);
                TK8710_LOG_IRQ_DEBUG("Manual TX user[%d]: freq=0x%08X", userIndex, freq);
            }
            writeLen++;
        }
        
        if (writeLen > 0) {
            ret = TK8710SpiSetInfo(TK8710_GET_INFO_FREQ, spiBuffer, writeLen * 4);
            if (ret != 0) {
                TK8710_LOG_IRQ_ERROR("SPI SetInfo frequency failed: %d", ret);
                free(spiBuffer);
                return;
            }
            TK8710_LOG_IRQ_DEBUG("Manual TX frequency configured successfully (broadcast + %d users)", 
                                writeLen - (hasBroadcast ? 1 : 0));
        }
    }
    
    /* 2. 配置AH数据 - 广播和用户数据合并传输 */
    {
        uint16_t writeLen = 0;
        
        /* 首先添加广播AH数据 */
        if (hasBroadcast) {
            /* 根据当前BCN天线配置AH数据 */
            for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
                for (uint8_t ant = 0; ant < 8; ant++) {
                    uint64_t ah40 = 0;
                    
                    // if (ant == 0) {  /* 广播使用天线0作为主天线 */
                    //     /* 当前广播天线：I路=8192U, Q路=0 */
                    //     ah40 = ((uint64_t)8192U << 20) | 0;  /* I=8192U (20bit), Q=0 (20bit) */
                    // } else {
                    //     /* 其他天线：I路=0, Q路=0 */
                    //     ah40 = 0;
                    // }
                    if (ant == g_currentBcnAntenna) {
                        /* 当前广播天线：I路=8192U, Q路=0 */
                        ah40 = ((uint64_t)8192U << 20) | 0;  /* I=8192U (20bit), Q=0 (20bit) */
                    } else {
                        /* 其他天线：I路=0, Q路=0 */
                        ah40 = 0;
                    }
                    /* 填充AH数据到缓冲区 (每个天线5字节) */
                    spiBuffer[writeLen*40 + ant*5]     = (uint8_t)(ah40 >> 32);
                    spiBuffer[writeLen*40 + ant*5 + 1] = (uint8_t)(ah40 >> 24);
                    spiBuffer[writeLen*40 + ant*5 + 2] = (uint8_t)(ah40 >> 16);
                    spiBuffer[writeLen*40 + ant*5 + 3] = (uint8_t)(ah40 >> 8);
                    spiBuffer[writeLen*40 + ant*5 + 4] = (uint8_t)(ah40);
                }
                writeLen++;
                TK8710_LOG_IRQ_DEBUG("Manual TX broadcast[%d] AH configured (antenna %d: I=8192, Q=0)", i, g_currentBcnAntenna);
            }
        }
        
        /* 然后添加所有有效用户的AH信息 */
        for (uint8_t i = 0; i < validUserCount; i++) {
            uint8_t userIndex = validUsers[i];
            
            if (g_forceMaxUsersTx && !g_txBuffers[userIndex].valid) {
                /* 测试模式：使用用户信息Buffer中的AH数据 */
                for (int ant = 0; ant < 8; ant++) {
                    /* 从用户信息Buffer获取AH数据 */
                    uint32_t iData = g_userInfoRxBuffers[userIndex].ahData[ant*2];     /* I路数据 */
                    uint32_t qData = g_userInfoRxBuffers[userIndex].ahData[ant*2 + 1]; /* Q路数据 */
                    
                    /* 组合成40bit数据 */
                    uint64_t ah40 = ((uint64_t)iData << 20) | qData;
                    memcpy(&spiBuffer[writeLen*40 + ant*5], &ah40, 5);
                }
                TK8710_LOG_IRQ_DEBUG("Manual TX test user[%d] AH configured (from userInfo)", userIndex);
            } else {
                /* 正常模式：从发送用用户信息Buffer获取AH数据 */
                for (int ant = 0; ant < 8; ant++) {
                    /* 每个天线20bit I + 20bit Q = 40bit = 5字节 */
                    uint32_t iData = g_userInfoTxBuffers[userIndex].ahData[ant*2];     /* I路数据 */
                    uint32_t qData = g_userInfoTxBuffers[userIndex].ahData[ant*2 + 1]; /* Q路数据 */
                    
                    /* 组合成40bit数据 */
                    uint64_t ah40 = ((uint64_t)iData << 20) | qData;
                    
                    /* 直接按字节顺序发送AH数据，不做调整 */
                    memcpy(&spiBuffer[writeLen*40 + ant*5], &ah40, 5);  /* 直接复制5字节 */
                }
                TK8710_LOG_IRQ_DEBUG("Manual TX user[%d] AH configured (antenna 0: I=%u, Q=%u)", 
                                   userIndex, g_userInfoTxBuffers[userIndex].ahData[0], 
                                   g_userInfoTxBuffers[userIndex].ahData[1]);
            }
            writeLen++;
        }
        
        if (writeLen > 0) {
            ret = TK8710SpiSetInfo(TK8710_GET_INFO_AH, spiBuffer, writeLen * 40);
            if (ret != 0) {
                TK8710_LOG_IRQ_ERROR("SPI SetInfo AH failed: %d", ret);
                free(spiBuffer);
                return;
            }
            TK8710_LOG_IRQ_DEBUG("Manual TX AH configured successfully (broadcast + %d users)", 
                                writeLen - (hasBroadcast ? 1 : 0));
        }
    }
    
    /* 3. 配置Pilot功率 - 广播和用户数据合并传输 */
    {
        uint16_t writeLen = 0;
        
        /* 首先添加广播Pilot功率 */
        if (hasBroadcast) {
            /* 为每个广播用户配置Pilot功率默认值261720U */
            for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
                uint64_t pilotPower = 261720U;  /* 默认Pilot功率值 */
                
                /* 填充Pilot功率数据到缓冲区 (每个用户5字节) */
                spiBuffer[writeLen*5]     = (uint8_t)(pilotPower >> 32);
                spiBuffer[writeLen*5 + 1] = (uint8_t)(pilotPower >> 24);
                spiBuffer[writeLen*5 + 2] = (uint8_t)(pilotPower >> 16);
                spiBuffer[writeLen*5 + 3] = (uint8_t)(pilotPower >> 8);
                spiBuffer[writeLen*5 + 4] = (uint8_t)(pilotPower);
                
                TK8710_LOG_IRQ_DEBUG("Manual TX broadcast[%d] pilot power=%u", i, (uint32_t)pilotPower);
                writeLen++;
            }
        }
        
        /* 然后添加所有有效用户的Pilot功率信息 */
        for (uint8_t i = 0; i < validUserCount; i++) {
            /* 从发送用用户信息Buffer获取Pilot功率 */
            uint64_t pilotPower = g_userInfoTxBuffers[i].pilotPower;
            
            /* 直接按字节顺序发送Pilot功率数据，不做调整 */
            memcpy(&spiBuffer[writeLen*5], &pilotPower, 5);  /* 直接复制5字节 */
            
            TK8710_LOG_IRQ_DEBUG("Manual TX user[%d]: pilot power=%u", i, (uint32_t)pilotPower);
            writeLen++;
        }
        
        if (writeLen > 0) {
            ret = TK8710SpiSetInfo(TK8710_GET_INFO_PILOT_POW, spiBuffer, writeLen * 5);
            if (ret != 0) {
                TK8710_LOG_IRQ_ERROR("SPI SetInfo pilot power failed: %d", ret);
                free(spiBuffer);
                return;
            }
            TK8710_LOG_IRQ_DEBUG("Manual TX pilot power configured successfully (broadcast + %d users)", 
                                writeLen - (hasBroadcast ? 1 : 0));
        }
    }
    
    /* 4. 配置Anoise - 使用固定的默认值（只配置一次） */
    {
        /* 使用8根天线的默认Anoise数据 */
        static uint16_t fixed_anoises[8] = {
            40U, 32U, 41U, 34U, 40U, 37U, 39U, 34U
        };
        
        for (uint8_t i = 0; i < 8; i++) {
            uint16_t anoise = fixed_anoises[i];
            spiBuffer[i*2]   = (uint8_t)(anoise >> 8);
            spiBuffer[i*2+1] = (uint8_t)(anoise);
        }
        
        ret = TK8710SpiSetInfo(TK8710_GET_INFO_ANOISE, spiBuffer, 16);
        if (ret != 0) {
            TK8710_LOG_IRQ_ERROR("SPI SetInfo anoise failed: %d", ret);
            free(spiBuffer);
            return;
        }
        TK8710_LOG_IRQ_DEBUG("Manual TX anoise configured successfully");
    }
    
    /* 5. 配置发送功率 - 为广播和每个有效用户设置功率 */
    {
        /* 首先设置广播发送功率 */
        if (hasBroadcast) {
            /* 为每个广播用户设置功率 */
            for (uint8_t i = 0; i < 16; i++) {
                if(g_brdBuffers[i].valid){
                    s_tx_pow_ctrl tx_pow_ctrl;
                    tx_pow_ctrl.data = 0;
                    tx_pow_ctrl.b.UserIndex = 128 + i;  /* 广播用户索引从128开始 */
                    /* 使用g_brdBuffers中配置的功率，如果无效则使用默认值0 */
                    tx_pow_ctrl.b.power = g_brdBuffers[i].txPower;
                    
                    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                        MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);
                    if (ret != TK8710_OK) {
                        TK8710_LOG_IRQ_ERROR("Failed to set manual TX broadcast[%d] power: %d", i, ret);
                        return;
                    }
                    TK8710_LOG_IRQ_DEBUG("Manual TX broadcast[%d] power set to %d (index: %d)", 
                                        i, tx_pow_ctrl.b.power, 128 + i);
                }
            }
        }
        
        /* 然后为每个有效用户设置功率 */
        for (uint8_t i = 0; i < validUserCount; i++) {
            s_tx_pow_ctrl tx_pow_ctrl;
            uint8_t userIndex = validUsers[i];
            /* 使用g_txBuffers中配置的功率 */
            uint8_t power = g_txBuffers[userIndex].txPower;
            
            tx_pow_ctrl.data = 0;
            tx_pow_ctrl.b.UserIndex = i + 1;  /* 发送时用户索引从1开始 */
            tx_pow_ctrl.b.power = power;
            
            int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_IRQ_ERROR("Failed to set manual TX power for user %d: %d", userIndex, ret);
                free(spiBuffer);
                return;
            }
            
            TK8710_LOG_IRQ_DEBUG("Manual TX user[%d]: TX power = %d", userIndex, power);
        }
        
        TK8710_LOG_IRQ_DEBUG("Manual TX power configured successfully (broadcast + %d users)", validUserCount);
    }
    
    /* 6. 发送广播数据 */
    /* 统计slot1中与Slot3共用波束信息的广播个数 */
    uint8_t slot3SharedBrdCount = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if (g_brdBuffers[i].valid && g_brdBuffers[i].beamType == TK8710_DATA_TYPE_DED) {
            slot3SharedBrdCount++;
        }
    }
    TK8710_LOG_IRQ_DEBUG("Slot3 shared broadcast count: %d", slot3SharedBrdCount);
    
    if (hasBroadcast) {
        uint8_t brdSuccessCount = 0;
        uint8_t brdErrorCount = 0;
        
        /* 发送所有有效的广播数据 */
        for (uint8_t i = 0; i < 16; i++) {
            if (g_brdBuffers[i].valid) {
                uint8_t brdIndex = g_brdBuffers[i].brdIndex;
                uint8_t* brdData = g_brdBuffers[i].data;
                uint16_t dataLen = g_brdBuffers[i].dataLen;
                uint8_t userIndex = 128 + brdIndex;  /* 广播用户索引从128开始 */
                
                /* 发送广播数据 */
                int ret = TK8710WriteBuffer(userIndex, brdData, dataLen);
                if (ret == TK8710_OK) {
                    brdSuccessCount++;
                    TK8710_LOG_IRQ_DEBUG("Manual TX broadcast[%d] data sent successfully (index: %d, length=%d)", 
                                        brdIndex, userIndex, dataLen);
                    
                    /* 发送成功后清除valid状态，避免重复发送 */
                    g_brdBuffers[i].valid = 0;
                    
                    /* 释放对应的RAM资源 */
                    if (g_brdBuffers[i].data != NULL) {
                        free(g_brdBuffers[i].data);
                        g_brdBuffers[i].data = NULL;
                    }
                    
                    TK8710_LOG_IRQ_DEBUG("Broadcast[%d] TX buffer cleared and RAM freed to prevent repeat transmission", brdIndex);
                } else {
                    brdErrorCount++;
                    TK8710_LOG_IRQ_ERROR("Failed to send broadcast[%d] data: %d", brdIndex, ret);
                }
            }
        }
        
        TK8710_LOG_IRQ_DEBUG("Manual TX broadcast summary: success=%d, error=%d", brdSuccessCount, brdErrorCount);
    }
    
    /* 7. 发送用户数据 */
    if (hasUserData && validUserCount > 0) {
        /* 计算期望的数据长度 */
        /* 统计与Slot1共用波束信息的用户个数 */
        uint8_t slot1SharedUserCount = 0;
        for (uint8_t i = 0; i < actualMaxUsers; i++) {
            if (g_txBuffers[i].valid && g_txBuffers[i].beamType == TK8710_DATA_TYPE_BRD) {
                slot1SharedUserCount++;
            }
        }
        
        /* 验证slot1SharedUserCount不超过totalBrdUsers */
        uint8_t totalBrdUsers = slotCfg->brdUserNum;
        if (slot1SharedUserCount > totalBrdUsers) {
            TK8710_LOG_IRQ_ERROR("Slot1 shared user count (%d) exceeds total broadcast users (%d)", 
                                slot1SharedUserCount, totalBrdUsers);
            slot1SharedUserCount = totalBrdUsers;  /* 限制为最大值 */
        }
        TK8710_LOG_IRQ_DEBUG("Slot1 shared user count: %d (max: %d)", slot1SharedUserCount, totalBrdUsers);
    }
    
    /* 配置广播用户valid位 (MAC init_17寄存器) */
    {
        s_init_17 brdUserVal;
        
        /* 广播用户valid位: bit[brdUserNum + slot3SharedBrdCount - 1:0] = 1 */
        /* 正常广播数 + 与Slot3共用波束的广播数 */
        uint8_t totalBrdUsers = slotCfg->brdUserNum + slot3SharedBrdCount;
        brdUserVal.data = 0;
        brdUserVal.b.brd_user_val = (1 << totalBrdUsers) - 1;
        
        TK8710_LOG_IRQ_DEBUG("Broadcast user valid: brdUserNum=%d, slot3SharedBrdCount=%d, total=%d, val=0x%04X",
                            slotCfg->brdUserNum, slot3SharedBrdCount, totalBrdUsers, brdUserVal.b.brd_user_val);
        
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_17), brdUserVal.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to configure broadcast user valid bits (init_17)");
            return;
        }
        
    }
    
    /* 配置所有用户的MAC寄存器有效位 */
    {
        uint32_t user_val_regs[4] = {0}; /* user_val0, user_val1, user_val2, user_val3 */
        uint8_t slot1SharedIdx = 0;  /* 与Slot1共用波束的用户计数器 */
        uint8_t totalBrdUsersForUser = slotCfg->brdUserNum;  /* 重新计算totalBrdUsers */
        /* 发送所有有效用户数据 */
        uint8_t successCount = 0;
        uint8_t errorCount = 0;
        uint16_t expectedLen = 0;
        
        /* 根据主从模式确定期望的数据长度 */
        if (slotCfg->msMode == TK8710_MODE_MASTER) {
            /* Master模式：应该等于S3配置长度 */
            expectedLen = slotCfg->s3Cfg[g_irqResult.currentRateIndex].byteLen;
            TK8710_LOG_IRQ_DEBUG("Master mode: expected data length = %d (S3 config)", expectedLen);
        } else {
            /* Slave模式：应该等于S2配置长度 */
            expectedLen = slotCfg->s2Cfg[g_irqResult.currentRateIndex].byteLen;
            TK8710_LOG_IRQ_DEBUG("Slave mode: expected data length = %d (S2 config)", expectedLen);
        }
        
        /* 设置有效用户valid位 */
        for (int i = 0; i < validUserCount; i++) {
            uint8_t origUserIndex = validUsers[i];
            uint8_t userIndex;
            
            /* 检查是否与Slot1共用波束信息 */
            if (g_txBuffers[origUserIndex].beamType == TK8710_DATA_TYPE_BRD && 
                slot1SharedIdx < totalBrdUsersForUser) {
                /* 与Slot1共用波束的用户，使用广播用户的位置 */
                userIndex = slot1SharedIdx;
                slot1SharedIdx++;
                TK8710_LOG_IRQ_DEBUG("User[%d] shares Slot1 beam, mapped to index %d", origUserIndex, userIndex);
            } else {
                /* 正常用户，使用广播用户数之后的位置 */
                userIndex = origUserIndex + slotCfg->brdUserNum;
            }
            if (g_txBuffers[origUserIndex].valid) {
                uint8_t* userData = g_txBuffers[origUserIndex].data;
                uint16_t dataLen = g_txBuffers[origUserIndex].dataLen;
                
                /* 检查数据长度 */
                if (dataLen != expectedLen) {
                    dataLen = expectedLen;
                    // TK8710_LOG_IRQ_ERROR("User[%d] data length mismatch: expected=%d, actual=%d", 
                    //                    i+1, expectedLen, dataLen);
                    // errorCount++;
                    // continue;
                }
                
                /* 发送用户数据 - 按顺序写入buffer */
                int ret = TK8710WriteBuffer(userIndex, userData, dataLen);
                if (ret == TK8710_OK) {
                    successCount++;
                    TK8710_LOG_IRQ_DEBUG("Manual TX user[%d] data sent successfully (length=%d)", 
                                        i+1, dataLen);
                } else {
                    TK8710_LOG_IRQ_ERROR("Failed to send user[%d] data: %d", i+1, ret);
                    errorCount++;
                }
            }
            uint32_t regIndex = userIndex / 32;
            uint32_t bitIndex = userIndex % 32;
            user_val_regs[regIndex] |= (1 << bitIndex);
            /* 释放对应的RAM资源 */
            TK8710ClearTxUserData(origUserIndex);
            
            TK8710_LOG_IRQ_DEBUG("User[%d] TX buffer cleared and RAM freed to prevent repeat transmission", origUserIndex);
        }
        TK8710_LOG_IRQ_INFO("Manual TX user data transmission completed: success=%d, error=%d", 
                           successCount, errorCount);
        /* 写入MAC寄存器 */
        for (int reg = 0; reg < 4; reg++) {
            uint32_t reg_offset = MAC_BASE + 0x3c + reg * 4;
            int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, reg_offset, user_val_regs[reg]);
            if (ret == TK8710_OK) {
                TK8710_LOG_IRQ_DEBUG("Set MAC user_val%d = 0x%08X", reg, user_val_regs[reg]);
            } else {
                TK8710_LOG_IRQ_ERROR("Failed to set MAC user_val%d: %d", reg, ret);
            }
        }
    }
    
    /* 释放SPI缓冲区 */
    free(spiBuffer);
    
    TK8710_LOG_IRQ_INFO("S1 manual TX configuration completed successfully");
}

/**
 * @brief S1时隙广播发送处理
 * 
 * 当广播发送用户不为0时，配置广播的频率、AH、pilot等参数
 */
static void tk8710_s1_broadcast_tx_process(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    /* 检查是否有广播用户需要发送 */
    if (slotCfg->brdUserNum == 0) {
        TK8710_LOG_IRQ_DEBUG("No broadcast users to send (brdUserNum=0)");
        return;
    }
    

    /* 0. 配置广播用户valid位 (MAC init_17寄存器) */
    {
        s_init_17 brdUserVal;
        
        /* 广播用户valid位: bit[brdUserNum-1:0] = 1 */
        brdUserVal.data = 0;
        brdUserVal.b.brd_user_val = (1 << slotCfg->brdUserNum) - 1;
        
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_17), brdUserVal.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_IRQ_ERROR("Failed to configure broadcast user valid bits (init_17)");
            return;
        }
        
    }
    
    /* 1. 配置广播频率 */
    {
        int ret;
        uint8_t freqBuf[128];  /* 频率缓冲区 */
        uint16_t writeLen = slotCfg->brdUserNum * 4;  /* 每个用户4字节频率 */
        
        
        /* 从slotCfg->brdFreq获取频率数据并填充到freqBuf */
        for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
            /* TK8710频率格式: 26-bit unsigned (1 sign + 18 integer + 7 decimal) */
            float freqFloat = slotCfg->brdFreq[i];
            uint32_t freq;
            
            /* 将float频率转换为TK8710 26位格式 */
            /* 格式: [sign:1][integer:18][decimal:7] */
            if (freqFloat >= 0) {
                /* 正数：sign=0 */
                uint32_t integerPart = (uint32_t)freqFloat;
                uint32_t decimalPart = (uint32_t)((freqFloat - integerPart) * 128); /* 2^7 = 128 */
                freq = (integerPart << 7) | decimalPart;
            } else {
                /* 负数：sign=1 (使用补码) */
                float absFreq = -freqFloat;
                uint32_t integerPart = (uint32_t)absFreq;
                uint32_t decimalPart = (uint32_t)((absFreq - integerPart) * 128);
                uint32_t magnitude = (integerPart << 7) | decimalPart;
                freq = ((~magnitude) + 1) & 0x03FFFFFF; /* 26位补码 */
            }
            
            /* 确保频率在26位范围内 */
            freq &= 0x03FFFFFF; /* 26位掩码 */
            
            /* SPI传输按字节顺序发送，需要调整字节位置 */
            /* freq = 0x3d8f57e -> SPI发送: 03、d8、f5、7e */
            freqBuf[i*4]   = (uint8_t)((freq >> 24) & 0xFF);  /* 高字节在前 */
            freqBuf[i*4+1] = (uint8_t)((freq >> 16) & 0xFF);
            freqBuf[i*4+2] = (uint8_t)((freq >> 8) & 0xFF);
            freqBuf[i*4+3] = (uint8_t)(freq & 0xFF);        /* 低字节在后 */
            
        }
        
        ret = TK8710SpiSetInfo(TK8710_GET_INFO_FREQ, freqBuf, writeLen);
        if (ret != 0) {
            TK8710_LOG_IRQ_ERROR("SPI SetInfo frequency failed: %d", ret);
            return;
        }
    }
    
    /* 2. 配置AH数据 */
    {
        int ret;
        uint8_t ahBuf[640];  /* AH缓冲区: 预留足够空间 */
        uint16_t writeLen = slotCfg->brdUserNum * 40;  /* 每个用户40字节AH数据 */
        /* 根据当前BCN天线配置AH数据 */
        for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
            for (uint8_t ant = 0; ant < 8; ant++) {
                uint64_t ah40 = 0;
                
                if (ant == g_currentBcnAntenna) {
                    /* 当前广播天线：I路=8192U, Q路=0 */
                    ah40 = ((uint64_t)8192U << 20) | 0;  /* I=8192U (20bit), Q=0 (20bit) */
                } else {
                    /* 其他天线：I路=0, Q路=0 */
                    ah40 = 0;
                }
                /* 填充AH数据到缓冲区 (每个天线5字节) */
                ahBuf[i*40 + ant*5]     = (uint8_t)(ah40 >> 32);
                ahBuf[i*40 + ant*5 + 1] = (uint8_t)(ah40 >> 24);
                ahBuf[i*40 + ant*5 + 2] = (uint8_t)(ah40 >> 16);
                ahBuf[i*40 + ant*5 + 3] = (uint8_t)(ah40 >> 8);
                ahBuf[i*40 + ant*5 + 4] = (uint8_t)(ah40);
            }
        }
        
        ret = TK8710SpiSetInfo(TK8710_GET_INFO_AH, ahBuf, writeLen);
        if (ret != 0) {
            TK8710_LOG_IRQ_ERROR("SPI SetInfo AH failed: %d", ret);
            return;
        }
    }
    
    /* 3. 配置Pilot功率 */
    {
        int ret;
        uint8_t powBuf[640] = {0};  /* Pilot功率缓冲区 */
        uint16_t writeLen = slotCfg->brdUserNum * 5;  /* 每个用户5字节Pilot功率 */
        /* 为每个广播用户配置Pilot功率默认值261720U */
        for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
            uint64_t pow40 = 261720U;  /* 默认Pilot功率值 */
            
            /* 填充Pilot功率数据到缓冲区 (每个用户5字节) */
            powBuf[i*5]     = (uint8_t)(pow40 >> 32);
            powBuf[i*5 + 1] = (uint8_t)(pow40 >> 24);
            powBuf[i*5 + 2] = (uint8_t)(pow40 >> 16);
            powBuf[i*5 + 3] = (uint8_t)(pow40 >> 8);
            powBuf[i*5 + 4] = (uint8_t)(pow40);
            
        }
        
        ret = TK8710SpiSetInfo(TK8710_GET_INFO_PILOT_POW, powBuf, writeLen);
        // ret = TK8710SpiSetInfo(TK8710_GET_INFO_PILOT_POW, powBuf, 640);      
        if (ret != 0) {
            TK8710_LOG_IRQ_ERROR("SPI SetInfo pilot power failed: %d", ret);
            return;
        }
    }
    
    /* 4. 配置Anoise */
    {
        int ret;
        uint8_t anoiseBuf[16];  /* Anoise缓冲区: 8个数据点 * 2字节 = 16字节 */
        uint16_t writeLen = 16;  /* 8个数据点 * 2字节 */
        /* 使用8根天线的默认Anoise数据 */
        static uint16_t fixed_anoises[8] = {
            40U, 32U, 41U, 34U, 40U, 37U, 39U, 34U
        };
        
        for (uint8_t i = 0; i < 8; i++) {
            uint16_t anoise = fixed_anoises[i];
            anoiseBuf[i*2]   = (uint8_t)(anoise >> 8);
            anoiseBuf[i*2+1] = (uint8_t)(anoise);
        }
        
        ret = TK8710SpiSetInfo(TK8710_GET_INFO_ANOISE, anoiseBuf, writeLen);
        if (ret != 0) {
            TK8710_LOG_IRQ_ERROR("SPI SetInfo anoise failed: %d", ret);
            return;
        }
    }
    
    /* 配置广播发送功率 */
    {
        
        for (uint8_t i = 0; i < slotCfg->brdUserNum; i++) {
            s_tx_pow_ctrl tx_pow_ctrl;
            uint8_t userIndex = 128 + i;  /* 广播用户index从128开始 */
            uint8_t power = 0;          /* 默认广播发送功率值 */
            
            tx_pow_ctrl.data = 0;
            tx_pow_ctrl.b.UserIndex = userIndex;
            tx_pow_ctrl.b.power = power;
            
            int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_IRQ_ERROR("Failed to set broadcast TX power for user %d: %d", userIndex, ret);
                return;
            }
            
        }
        
    }
    
    /* 5. 发送广播数据 */
    {
        uint8_t brdSuccessCount = 0;
        uint8_t brdErrorCount = 0;
        
        /* 发送所有有效的广播数据 */
        for (uint8_t i = 0; i < 16; i++) {
            if (g_brdBuffers[i].valid) {
                uint8_t brdIndex = g_brdBuffers[i].brdIndex;
                uint8_t* brdData = g_brdBuffers[i].data;
                uint16_t dataLen = g_brdBuffers[i].dataLen;
                uint8_t userIndex = 128 + brdIndex;  /* 广播用户索引从128开始 */
                
                /* 发送广播数据 */
                int ret = TK8710WriteBuffer(userIndex, brdData, dataLen);
                if (ret == TK8710_OK) {
                    brdSuccessCount++;
                    TK8710_LOG_IRQ_DEBUG("Broadcast TX broadcast[%d] data sent successfully (index: %d, length=%d)", 
                                        brdIndex, userIndex, dataLen);
                    
                    /* 发送成功后清除valid状态，避免重复发送 */
                    g_brdBuffers[i].valid = 0;
                    
                    /* 释放对应的RAM资源 */
                    if (g_brdBuffers[i].data != NULL) {
                        free(g_brdBuffers[i].data);
                        g_brdBuffers[i].data = NULL;
                    }
                    
                    TK8710_LOG_IRQ_DEBUG("Broadcast[%d] TX buffer cleared and RAM freed to prevent repeat transmission", brdIndex);
                } else {
                    brdErrorCount++;
                    TK8710_LOG_IRQ_ERROR("Failed to send broadcast[%d] data: %d", brdIndex, ret);
                }
            }
        }
        
        TK8710_LOG_IRQ_DEBUG("Broadcast TX data transmission completed: success=%d, error=%d", 
                           brdSuccessCount, brdErrorCount);
    }
    
}

/**
 * @brief 读取CRC正确用户的SNR、RSSI、AH、freq数据
 */
static void tk8710_md_data_read_signal_info(void)
{
    int ret;
    uint8_t maxUsers = g_irqResult.maxUsers;
    uint16_t readLen;
    uint8_t rxBuf[5120];  /* 最大128*5*8=5120 bytes (AH数据) */
    int i;
    uint8_t validUserCount = 0;
    
    /* 首先检查是否有CRC正确的用户 */
    for (i = 0; i < maxUsers; i++) {
        if (g_irqResult.crcResults[i].crcValid) {
            validUserCount++;
            break;  /* 找到至少一个有效用户就足够了 */
        }
    }
    
    /* 如果没有CRC正确的用户，直接返回 */
    if (validUserCount == 0) {
        TK8710_LOG_IRQ_DEBUG("No CRC valid users found, skipping signal info reading");
        return;
    }
    
    TK8710_LOG_IRQ_DEBUG("Reading signal info for CRC valid users");
    
    /* 1. 读取SNR和RSSI数据 (GetInfo type=20) */
    readLen = maxUsers * 4;  /* 只读取数据用户，每个4字节 */
    ret = TK8710SpiGetInfo(TK8710_GET_INFO_SNR_RSSI, rxBuf, readLen);
    if (ret == 0) {
        for (i = 0; i < maxUsers; i++) {
            if (g_irqResult.crcResults[i].crcValid) {
                /* SNR和RSSI数据格式: 每个用户4字节 [reserve:7][SNR:9][reserve:5][RSSI:11] */
                /* SPI传输是字节顺序直接组合，不是Little Endian */
                uint32_t snrRssi = ((uint32_t)rxBuf[i*4] << 24) |
                                  ((uint32_t)rxBuf[i*4+1] << 16) |
                                  ((uint32_t)rxBuf[i*4+2] << 8) |
                                  ((uint32_t)rxBuf[i*4+3]);
                
                /* 保存原始SNR和RSSI数据到Buffer系统 */
                g_signalInfo[i].snr = (uint8_t)((snrRssi >> 16) & 0x1FF);     /* SNR原始值 (9bit) */
                g_signalInfo[i].rssi = (uint32_t)(snrRssi & 0x7FF);            /* RSSI原始值 (11bit) */
                g_signalInfo[i].valid = 1;  
            }
        }
    } else {
        TK8710_LOG_IRQ_ERROR("Failed to read SNR/RSSI data: %d", ret);
    }
    
    /* 2. 读取用户频点 (GetInfo type=0) - 只读取CRC正确的用户 */
    /* 在指定信息发送模式下，直接使用已获取的频率数据，避免重复SPI读取 */
    if (TK8710GetTxBeamCtrlMode() == 1) {
        /* 指定信息发送模式：直接使用g_userInfoRxBuffers中的频率数据 */
        for (i = 0; i < maxUsers; i++) {
            if (g_irqResult.crcResults[i].crcValid && g_userInfoRxBuffers[i].valid) {
                /* 从g_userInfoRxBuffers获取原始字节数据并按字节顺序组合频率 */
                /* 参考SPI读取时的字节顺序组合方式 */
                uint32_t* freqBytes = (uint32_t*)&g_userInfoRxBuffers[i].freq;
                uint32_t freqRaw = ((uint32_t)((uint8_t*)freqBytes)[0] << 24) |
                                   ((uint32_t)((uint8_t*)freqBytes)[1] << 16) |
                                   ((uint32_t)((uint8_t*)freqBytes)[2] << 8) |
                                   ((uint32_t)((uint8_t*)freqBytes)[3]);
                
                /* 保存组合后的频率数据到Buffer系统 */
                g_signalInfo[i].freq = freqRaw;  /* 保存26-bit原始频率值 */
                g_signalInfo[i].valid = 1;
                TK8710_LOG_IRQ_DEBUG("Using cached freq for user[%d]: 0x%08X", i, g_signalInfo[i].freq);
            }
        }
    } else {
        /* 非指定信息发送模式：通过SPI获取频率数据 */
        readLen = maxUsers * 4;
        ret = TK8710SpiGetInfo(TK8710_GET_INFO_FREQ, rxBuf, readLen);
        if (ret == 0) {
            for (i = 0; i < maxUsers; i++) {
                if (g_irqResult.crcResults[i].crcValid) {
                    /* 按字节顺序直接组合频率数据 */
                    uint32_t freqRaw = ((uint32_t)rxBuf[i*4] << 24) |
                                       ((uint32_t)rxBuf[i*4+1] << 16) |
                                       ((uint32_t)rxBuf[i*4+2] << 8) |
                                       ((uint32_t)rxBuf[i*4+3]);
                    
                    /* 保存原始频率数据到Buffer系统 */
                    g_signalInfo[i].freq = freqRaw;  /* 保存26-bit原始频率值 */
                    g_signalInfo[i].valid = 1;
                }
            }
        } else {
            TK8710_LOG_IRQ_ERROR("Failed to read user frequency data: %d", ret);
        }
    }
    
    TK8710_LOG_IRQ_DEBUG("Signal info reading completed");
}

/* ============================================================================
 * Buffer管理API实现
 * ============================================================================
 */

/**
 * @brief 获取接收数据
 * @param userIndex 用户索引 (0-127)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen)
{
    if (userIndex >= 128 || data == NULL || dataLen == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userIndex=%d, data=%p, dataLen=%p", 
                            userIndex, data, dataLen);
        return TK8710_ERR;
    }
    
    if (g_rxBuffers[userIndex].valid) {
        *data = g_rxBuffers[userIndex].data;
        *dataLen = g_rxBuffers[userIndex].dataLen;
        TK8710_LOG_IRQ_DEBUG("RX data retrieved: user[%d], len=%d", userIndex, *dataLen);
        return TK8710_OK;
    } else {
        *data = NULL;
        *dataLen = 0;
        TK8710_LOG_IRQ_DEBUG("No RX data available for user[%d]", userIndex);
        return TK8710_ERR;
    }
}

/**
 * @brief 释放接收数据
 * @param userIndex 用户索引 (0-127)
 * @return 0-成功, 1-失败
 */
int TK8710ReleaseRxData(uint8_t userIndex)
{
    if (userIndex >= 128) {
        TK8710_LOG_IRQ_ERROR("Invalid user index: %d", userIndex);
        return TK8710_ERR;
    }
    
    if (g_rxBuffers[userIndex].valid && g_rxBuffers[userIndex].data != NULL) {
        free(g_rxBuffers[userIndex].data);
        memset(&g_rxBuffers[userIndex], 0, sizeof(TK8710RxBuffer));
        TK8710_LOG_IRQ_DEBUG("RX data released: user[%d]", userIndex);
    }
    
    return TK8710_OK;
}

/**
 * @brief 获取发送数据
 * @param userIndex 用户索引 (0-127)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @param txPower 发送功率输出
 * @return 0-成功, 1-失败
 */
int TK8710GetTxData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen, uint8_t* txPower)
{
    if (userIndex >= 128 || data == NULL || dataLen == NULL || txPower == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userIndex=%d, data=%p, dataLen=%p, txPower=%p", 
                            userIndex, data, dataLen, txPower);
        return TK8710_ERR;
    }
    
    if (g_txBuffers[userIndex].valid) {
        *data = g_txBuffers[userIndex].data;
        *dataLen = g_txBuffers[userIndex].dataLen;
        *txPower = g_txBuffers[userIndex].txPower;
        TK8710_LOG_IRQ_DEBUG("TX data retrieved: user[%d], len=%d, power=%d", 
                            userIndex, *dataLen, *txPower);
        return TK8710_OK;
    } else {
        *data = NULL;
        *dataLen = 0;
        *txPower = 0;
        TK8710_LOG_IRQ_DEBUG("No TX data available for user[%d]", userIndex);
        return TK8710_ERR;
    }
}

/**
 * @brief 获取广播数据
 * @param brdIndex 广播用户索引 (0-15)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @param txPower 发送功率输出
 * @return 0-成功, 1-失败
 */
int TK8710GetBrdData(uint8_t brdIndex, uint8_t** data, uint16_t* dataLen, uint8_t* txPower)
{
    if (brdIndex >= 16 || data == NULL || dataLen == NULL || txPower == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: brdIndex=%d, data=%p, dataLen=%p, txPower=%p", 
                            brdIndex, data, dataLen, txPower);
        return TK8710_ERR;
    }
    
    if (g_brdBuffers[brdIndex].valid) {
        *data = g_brdBuffers[brdIndex].data;
        *dataLen = g_brdBuffers[brdIndex].dataLen;
        *txPower = g_brdBuffers[brdIndex].txPower;
        TK8710_LOG_IRQ_DEBUG("Broadcast data retrieved: brd[%d], len=%d, power=%d", 
                            brdIndex, *dataLen, *txPower);
        return TK8710_OK;
    } else {
        *data = NULL;
        *dataLen = 0;
        *txPower = 0;
        TK8710_LOG_IRQ_DEBUG("No broadcast data available for brd[%d]", brdIndex);
        return TK8710_ERR;
    }
}

/**
 * @brief 获取信号质量信息
 * @param userIndex 用户索引 (0-127)
 * @param rssi RSSI值输出
 * @param snr SNR值输出
 * @param freq 频率值输出
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserSignalQuality(uint8_t userIndex, uint32_t* rssi, uint8_t* snr, uint32_t* freq)
{
    if (userIndex >= 128 || rssi == NULL || snr == NULL || freq == NULL) {
        TK8710_LOG_IRQ_ERROR("Invalid parameters: userIndex=%d, rssi=%p, snr=%p, freq=%p", 
                            userIndex, rssi, snr, freq);
        return TK8710_ERR;
    }
    
    if (g_signalInfo[userIndex].valid) {
        *rssi = g_signalInfo[userIndex].rssi;
        *snr = g_signalInfo[userIndex].snr;
        *freq = g_signalInfo[userIndex].freq;
        return TK8710_OK;
    } else {
        *rssi = 0;
        *snr = 0;
        *freq = 0;
        TK8710_LOG_IRQ_DEBUG("No signal info available for user[%d]", userIndex);
        return TK8710_ERR;
    }
}

/**
 * @brief 设置仿真数据加载状态
 * @param loaded 是否已加载仿真数据 (1-已加载, 0-未加载)
 * @note 用于测试时控制中断处理行为
 */
void TK8710SetSimulationDataLoaded(uint8_t loaded)
{
    g_simulationDataLoaded = loaded ? 1 : 0;
    TK8710_LOG_IRQ_INFO("Simulation data loaded status set to: %s", 
                       g_simulationDataLoaded ? "YES" : "NO");
}

/**
 * @brief 获取仿真数据加载状态
 * @return 1-已加载仿真数据, 0-未加载仿真数据
 */
uint8_t TK8710GetSimulationDataLoaded(void)
{
    return g_simulationDataLoaded;
}
