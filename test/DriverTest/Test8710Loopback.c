/**
 * @file Test8710Loopback.c
 * @brief 8710Loopback 主测试程序
 * @note 完整的初始化、配置、工作和中断处理流程,运行完Test8710Loopback.c后需要硬件复位一下才行
 * 
 * RK3506 编译方法:
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port test8710main.c \
 *       ../port/tk8710_rk3506.c ../src/tk8710_irq.c ../src/tk8710_core.c \
 *       ../src/tk8710_config.c ../src/tk8710_log.c \
 *       -o test8710main -lgpiod -lpthread
 */

#define _GNU_SOURCE  /* 必须在所有头文件之前定义，用于CPU_ZERO/CPU_SET等宏 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "hal_api.h"   /* HAL API接口 */
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"  /* Driver API接口 */
#include "driver/tk8710_internal.h"     /* Driver内部函数 */
#include "driver/tk8710_regs.h"

#include "driver_test_platform.h"

/*============================================================================
 * 全局变量和配置
 *============================================================================*/

/* 运行标志 */
static volatile int g_running = 1;

#ifndef _WIN32
/**
 * @brief Linux信号处理函数
 */
static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
    printf("\nReceived signal, exiting...\n");
}
#endif

#ifdef _WIN32
static int set_cpu_affinity(int cpu_core)
{
    (void)cpu_core;
    return 0;
}
#else
int set_cpu_affinity(int cpu_core)
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_core, &cpu_set);
    
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) < 0) {
        perror("Failed to set CPU affinity");
        return -1;
    }
    
    printf("Process bound to CPU core %d\n", cpu_core);
    return 0;
}
#endif

/* 下行发送状态跟踪 */
static volatile bool g_hasValidUsers = false;     /* 是否有有效用户 */
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

/* 全局变量 */
static volatile uint32_t g_irqCount = 0;                  /* 中断计数 */
static volatile uint32_t g_trmRxCount = 0;                /* TRM接收计数 */
static volatile uint32_t g_trmRxValidCount = 0;           /* TRM接收有效计数 */
static volatile uint32_t g_trmSendCount = 0;               /* TRM发送计数 */
static volatile uint32_t g_trmEventCount = 0;            /* TRM事件计数 */
static volatile uint32_t g_trmValidUserCount = 0;         /* TRM有效用户计数 */
static volatile uint32_t g_trmTestStartTime = 0;          /* 测试开始时间 */
static volatile uint32_t g_trmTestDuration = 0;           /* 测试持续时间 */
static volatile uint32_t g_trmMaxValidUsers = 0;          /* 最大有效用户数 */
static volatile uint8_t g_trmTestRunning = 0;             /* 测试运行标志 */
static volatile uint8_t g_trmTestCompleted = 0;           /* 测试完成标志 */
static volatile int g_mdDataCount = 0;                     /* MD_DATA中断计数 */

/* 遍历测试相关结构体 */
typedef struct {
    int mode;
    int classNum;
    int caseNum;
    int s1ByteLen;
    int s3ByteLen;
    uint32_t threshold;  /* 测试通过标准 */
    uint32_t maxValidUsers;  /* 测试结果最大有效用户数 */
    uint8_t passed;     /* 是否通过测试 */
    uint32_t testDuration; /* 测试持续时间(ms) */
} TestCase_t;

/* 测试配置表 */
static const TestCase_t testConfigs[] = {
    {5, 3, 1, 0, 0, 120, 0, 0, 0},
    {6, 3, 17, 0, 0, 120, 0, 0, 0},
    {7, 3, 33, 0, 0, 120, 0, 0, 0},
    {8, 3, 49, 0, 0, 120, 0, 0, 0},
    {9, 3, 65, 0, 0, 60, 0, 0, 0},
    {10, 3, 81, 0, 0, 28, 0, 0, 0},
    {11, 3, 97, 0, 0, 14, 0, 0, 0},
    {18, 3, 113, 0, 0, 14, 0, 0, 0}
};

static TestCase_t* g_currentTest = NULL;
static FILE* g_resultFile = NULL;

/* 丢包统计变量 */
static uint32_t g_packetCount = 0;               /* 当前统计周期内的包计数 */
static uint32_t g_packetLostCount = 0;           /* 当前统计周期内的丢包计数 */
static uint32_t g_totalPacketCount = 0;         /* 总包计数 */
static uint32_t g_totalPacketLostCount = 0;     /* 总丢包计数 */
static uint32_t g_lastValidUserIndex = 0;       /* 上次有效用户索引 */

/* TRM回调函数声明 */
static void __attribute__((unused)) OnTrmRxData(const TRM_RxDataList* rxDataList);
static void __attribute__((unused)) OnTrmTxComplete(const TRM_TxCompleteResult* txResult);

/* Driver回调函数声明 */
static void OnDriverRxData(TK8710IrqResult* irqResult);
static void OnDriverTxSlot(TK8710IrqResult* irqResult);
static void OnDriverSlotEnd(TK8710IrqResult* irqResult);
static void OnDriverError(TK8710IrqResult* irqResult);

/* 辅助函数声明 */
static int load_and_send_simulation_data(int classNum, int caseNum);

/*============================================================================
 * Driver回调函数实现
 *============================================================================*/

/**
 * @brief Driver接收数据回调
 * @param irqResult 中断结果
 */
static void OnDriverRxData(TK8710IrqResult* irqResult)
{
    if (!irqResult) return;
    
    printf("=== Driver RX Data Callback ===\n");
    printf("中断类型: %d\n", irqResult->irq_type);
    
    switch (irqResult->irq_type) {
        case TK8710_IRQ_RX_BCN:
            printf("BCN接收: bits=%u, freq_offset=%d, status=%u\n", 
                   irqResult->rx_bcnbits, irqResult->bcn_freq_offset, irqResult->rxbcn_status);
            break;
            
        case TK8710_IRQ_BRD_DATA:
            printf("BRD数据接收: 有效=%d, CRC正确=%d, CRC错误=%d\n", 
                   irqResult->brdDataValid, irqResult->crcValidCount, irqResult->crcErrorCount);
            break;
            
        case TK8710_IRQ_MD_DATA:
            printf("MD数据接收: 有效=%d, CRC正确=%d, CRC错误=%d\n", 
                   irqResult->mdDataValid, irqResult->crcValidCount, irqResult->crcErrorCount);
            
            /* 如果正在进行遍历测试，统计CRC正确的用户数 */
            if (g_trmTestRunning && g_currentTest) {
                /* 更新最大有效用户数 */
                if (irqResult->crcValidCount > g_trmMaxValidUsers) {
                    g_trmMaxValidUsers = irqResult->crcValidCount;
                }
                
                g_trmValidUserCount += irqResult->crcValidCount;
                
                /* 计算测试持续时间 */
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint32_t currentTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                g_trmTestDuration = currentTime - g_trmTestStartTime;
                
                /* MD_DATA中断计数（直接使用全局变量） */
                g_mdDataCount++;
                
                /* 如果MD_DATA中断达到10次，认为测试完成 */
                if (g_mdDataCount >= 10) {
                    g_trmTestRunning = 0;
                    g_trmTestCompleted = 1;
                    g_currentTest->maxValidUsers = g_trmMaxValidUsers;
                    g_currentTest->testDuration = g_trmTestDuration;
                    g_currentTest->passed = (g_trmMaxValidUsers >= g_currentTest->threshold) ? 1 : 0;
                    printf("MD_DATA中断达到10次，测试完成\n");
                }
            }
            
            /* 丢包统计逻辑 */
            g_packetCount++;
            g_totalPacketCount++;
            
            /* 如果有CRC正确的用户，获取用户信号质量信息 */
            if (irqResult->crcValidCount > 0) {
                printf("=== 用户信号质量信息 ===\n");
                uint8_t validUserCount = 0;
                
                for (uint8_t userIndex = 0; userIndex < TK8710_MAX_DATA_USERS; userIndex++) {
                    /* 检查该用户的CRC结果 */
                    if (userIndex < 128 && irqResult->crcResults[userIndex].crcValid) {
                        uint32_t rssi, freqSignal;
                        uint8_t snr;
                        
                        /* 获取用户信号质量 */
                        if (TK8710GetRxUserSignalQuality(userIndex, &rssi, &snr, &freqSignal) == TK8710_OK) {
                            /* SNR转换：uint8_t最大255，直接除以4 */
                            uint8_t snrValue = snr / 4;
                            
                            /* RSSI转换：11位有符号数，需要转换为有符号值 */
                            uint32_t rssiRaw = rssi;
                            int16_t rssiValue = (int16_t)(rssiRaw - 2048) / 4;
                            
                            /* 频率转换：26-bit格式转换为实际频率Hz */
                            uint32_t freq26 = freqSignal & 0x03FFFFFF;  /* 取26位 */
                            int32_t freqValue = freq26 > (1<<25) ? (int32_t)(freq26 - (1<<26)) : (int32_t)freq26;
                            
                            /* 只打印前10个用户的信息 */
                            if (userIndex < 10) {
                                printf("用户%u: freq=%dHz (raw=0x%08X), rssi=%d, snr=%u, 丢包=%u/%u\n", 
                                       userIndex, freqValue/128, freq26, rssiValue, snrValue, g_totalPacketLostCount, g_totalPacketCount);
                            }
                            
                            validUserCount++;
                            g_lastValidUserIndex = userIndex;
                        } else {
                            printf("用户%u: 获取信号质量失败\n", userIndex);
                        }
                        // TK8710ReleaseRxData(userIndex);
                    }
                }
                
                /* 如果没有收到预期的用户数据，计为丢包 */
                if (validUserCount == 0) {
                    g_packetLostCount++;
                    g_totalPacketLostCount++;
                    printf("丢包: 未收到有效用户数据\n");
                }
                
                printf("========================\n");
            } else {
                /* 没有CRC正确的用户，计为丢包 */
                g_packetLostCount++;
                g_totalPacketLostCount++;
                printf("丢包: 无CRC正确用户\n");
            }
            
            /* 每100包重新开始统计 */
            if (g_packetCount >= 100) {
                printf("\n=== 丢包统计 (100包) ===\n");
                printf("接收包数: %u\n", g_packetCount);
                printf("丢包数: %u\n", g_packetLostCount);
                printf("丢包率: %.2f%%\n", (float)g_packetLostCount * 100.0f / g_packetCount);
                printf("总包数: %u\n", g_totalPacketCount);
                printf("总丢包数: %u\n", g_totalPacketLostCount);
                printf("总丢包率: %.2f%%\n", (float)g_totalPacketLostCount * 100.0f / g_totalPacketCount);
                printf("========================\n\n");
                
                /* 重置当前统计周期 */
                g_packetCount = 0;
                g_packetLostCount = 0;
            }
            break;
            
        default:
            printf("其他中断类型: %d\n", irqResult->irq_type);
            break;
    }
    
    printf("============================\n");
}

/**
 * @brief Driver发送时隙回调
 * @param irqResult 中断结果
 */
static void OnDriverTxSlot(TK8710IrqResult* irqResult)
{
    if (!irqResult) return;
    
    printf("=== Driver TX Slot Callback ===\n");
    printf("中断类型: %d\n", irqResult->irq_type);
    printf("============================\n");
}

/**
 * @brief Driver时隙结束回调
 * @param irqResult 中断结果
 */
static void OnDriverSlotEnd(TK8710IrqResult* irqResult)
{
    if (!irqResult) return;
    
    printf("=== Driver Slot End Callback ===\n");
    printf("中断类型: %d\n", irqResult->irq_type);
    printf("============================\n");
}

/**
 * @brief Driver错误回调
 * @param irqResult 中断结果
 */
static void OnDriverError(TK8710IrqResult* irqResult)
{
    if (!irqResult) return;
    
    printf("=== Driver Error Callback ===\n");
    printf("中断类型: %d\n", irqResult->irq_type);
    printf("============================\n");
}

/*============================================================================
 * TRM回调函数实现
 *============================================================================*/

/**
 * @brief TRM接收数据回调
 * @param rxDataList 接收数据列表
 */
static void OnTrmRxData(const TRM_RxDataList* rxDataList)
{
    g_trmEventCount++;
    
    printf("=== TRM接收数据事件 (帧号=%u) ===\n", rxDataList->frameNo);
    printf("时隙: 用户数=%d\n", 
           rxDataList->userCount);
    
    /* 打印第一个用户的速率模式信息（如果有用户数据） */
    if (rxDataList->userCount > 0 && rxDataList->users) {
        TRM_RxUserData* firstUser = &rxDataList->users[0];
        printf("第一个用户信息: ID=0x%08X, 速率模式=%d, 数据长度=%u\n", 
               firstUser->userId, firstUser->rateMode, firstUser->dataLen);
    }
    
    g_trmRxCount += rxDataList->userCount;
    
    // for (uint8_t i = 0; i < rxDataList->userCount; i++) {
    //     TRM_RxUserData* user = &rxDataList->users[i];
    //     printf("  用户[%d]: ID=0x%08X, 长度=%d, RSSI=%d, SNR=%d, Freq=%d Hz\n", 
    //            i, user->userId, user->dataLen, user->rssi, user->snr, user->freq/128);
        
    //     /* 显示数据内容 */
    //     if (user->data != NULL && user->dataLen > 0) {
    //         printf("    数据: ");
    //         for (int k = 0; k < user->dataLen && k < 8; k++) {
    //             printf("%02X ", user->data[k]);
    //         }
    //         if (user->dataLen > 8) printf("...");
    //         printf("\n");
    //     }
    // }
    
    // /* 调用发送验证器 */
    // int ret = TRM_TxValidatorOnRxData(rxDataList);
    // if (ret != TRM_OK) {
    //     printf("  验证器处理失败: 错误码=%d\n", ret);
    // }
    // /* 显示验证统计信息 */
    // TRM_TxValidatorStats stats;
    // if (TRM_TxValidatorGetStats(&stats) == TRM_OK) {
    //     printf("  发送统计: 总触发=%u, 成功=%u, 失败=%u, 接收总次数=%u\n", 
    //            stats.totalTriggerCount, stats.successSendCount, stats.failedSendCount, g_trmRxCount);
    // }
    
    printf("==================\n");
}


/**
 * @brief TRM发送完成回调
 * @param txResult 发送完成结果
 */
static void OnTrmTxComplete(const TRM_TxCompleteResult* txResult)
{
    if (!txResult) return;
    
    printf("=== TRM发送完成事件 ===\n");
    printf("发送用户总数: %u, 剩余队列: %u\n", txResult->totalUsers, txResult->remainingQueue);
    g_trmSendCount += txResult->userCount;
    /* 打印每个用户的发送结果 */
    // const char* resultStr[] = {"OK", "NO_BEAM", "TIMEOUT", "ERROR"};
    // for (uint32_t i = 0; i < txResult->userCount; i++) {
    //     const TRM_TxUserResult* userResult = &txResult->users[i];
    //     printf("  用户ID: 0x%08X, 结果: %s\n", userResult->userId, resultStr[userResult->result]);
    // }
    printf("==================\n");
}


/**
 * @brief 读取寄存器
 */
void read_register(void)
{
    uint32_t addr, value;
    int ret;
    
    printf("\n=== 读取寄存器 ===\n");
    printf("输入寄存器地址 (十六进制，如 0xc030): ");
    
    if (scanf("%x", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr, &value);
    if (ret == TK8710_OK) {
        printf("寄存器 0x%08X = 0x%08X (%u)\n", addr, value, value);
    } else {
        printf("读取失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}

/**
 * @brief 写入寄存器
 */
void write_register(void)
{
    uint32_t addr, value;
    int ret;
    
    printf("\n=== 写入寄存器 ===\n");
    printf("输入寄存器地址 (十六进制，如 0xc030): ");
    
    if (scanf("%x", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    printf("输入写入值 (十六进制，如 0x8): ");
    
    if (scanf("%x", &value) != 1) {
        printf("无效的值格式\n");
        return;
    }
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr, value);
    if (ret == TK8710_OK) {
        printf("写入成功: 0x%08X = 0x%08X (%u)\n", addr, value, value);
    } else {
        printf("写入失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}


/**
 * @brief 显示TRM统计信息
 */
void show_trm_statistics(void)
{
    printf("\n=== TRM统计信息 ===\n");
    printf("发送计数: %u\n", g_trmSendCount);
    printf("接收计数: %u\n", g_trmRxCount);
    
    /* 获取TRM内部统计 */
    TRM_Stats stats;
    if (TRM_GetStats(&stats) == TRM_OK) {
        printf("TRM内部统计:\n");
        printf("  发送次数: %u\n", stats.txCount);
        printf("  发送成功: %u\n", stats.txSuccessCount);
        printf("  接收次数: %u\n", stats.rxCount);
        printf("  波束数量: %u\n", stats.beamCount);
        printf("当前帧号: %u\n", TRM_GetCurrentFrame());
    }
    
    printf("==================\n\n");
}

/**
 * @brief 重置测试统计变量
 */
static void reset_test_statistics(void)
{
    g_trmEventCount = 0;
    g_trmValidUserCount = 0;
    g_trmMaxValidUsers = 0;
    g_trmTestDuration = 0;
    g_trmTestRunning = 0;
    g_trmTestCompleted = 0;
    g_mdDataCount = 0;  /* 重置MD_DATA中断计数 */
}

/**
 * @brief 开始单个测试
 * @param testCase 测试用例
 * @param s1ByteLen slot1字节长度
 * @param s3ByteLen slot3字节长度
 * @return 0-成功, 非0-失败
 */
static int start_single_test(TestCase_t* testCase, int s1ByteLen, int s3ByteLen)
{
    int ret;
    struct timeval tv;
    
    printf("\n=== 开始测试 ===\n");
    printf("模式: %d, Class: %d, Case: %d\n", testCase->mode, testCase->classNum, testCase->caseNum);
    printf("Slot1 ByteLen: %d, Slot3 ByteLen: %d\n", s1ByteLen, s3ByteLen);
    printf("通过标准: %u 用户\n", testCase->threshold);
    
    /* 重置统计变量 */
    reset_test_statistics();
    g_currentTest = testCase;
    
    /* 记录开始时间 */
    gettimeofday(&tv, NULL);
    g_trmTestStartTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    /* 开始测试 */
    g_trmTestRunning = 1;
    
    /* ========== 每个测试都执行完整的初始化流程 ========== */
    
    /* 0. 注册Driver回调函数 */
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = OnDriverSlotEnd,
        .onError = OnDriverError
    };
    TK8710RegisterCallbacks(&driverCallbacks);
    printf("Driver callbacks registered\n");
    
    /* 1. 重新初始化TK8710 */
    printf("重新初始化TK8710...\n");
    
    /* 停止当前运行 */
    // TK8710Stop();  /* 函数可能不存在，暂时注释 */
    
    /* 重新准备芯片配置 */
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 1,
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = 0,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        .rfConfig    = NULL
    };
    
    /* 2. 重新初始化芯片 */
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("TK8710重新初始化失败: %d\n", ret);
        return -1;
    }
    
    /* 3. 配置时隙参数 */
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    
    /* 配置基本参数 */
    slotCfg.msMode = TK8710_MODE_LOOPBACK;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 0;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
    slotCfg.txBcnAntEn = 0x7f;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;
    
    /* 配置BCN轮流发送 */
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;
    }
    
    /* 单速率配置 */
    slotCfg.rateCount = 1;
    slotCfg.rateModes[0] = testCase->mode;
    
    /* 根据模式设置不同的da_m值 */
    switch (testCase->mode) {
        case 5:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 65000;
            break;
        case 6:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 65536;
            break;
        case 7:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 32768;
            break;
        case 8:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 8000;
            break;
        case 9:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 8192;
            break;
        case 10:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 4096;
            break;
        case 11:
        case 18:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 0;
            break;
        default:
            printf("Error: Unsupported mode %d\n", testCase->mode);
            return -1;
    }
    
    slotCfg.s0Cfg[0].byteLen = 0;
    slotCfg.s0Cfg[0].centerFreq = 509100000;
    slotCfg.s1Cfg[0].byteLen = s1ByteLen;
    slotCfg.s1Cfg[0].centerFreq = 509100000;
    slotCfg.s2Cfg[0].byteLen = 0;
    slotCfg.s2Cfg[0].centerFreq = 509100000;
    slotCfg.s3Cfg[0].byteLen = s3ByteLen;
    slotCfg.s3Cfg[0].centerFreq = 509100000;
    
    /* 4. 配置时隙 */
    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slotCfg);
    if (ret != TK8710_OK) {
        printf("时隙配置失败: %d\n", ret);
        return -1;
    }
    
    /* 5. 重新加载仿真数据 */
    if (load_and_send_simulation_data(testCase->classNum, testCase->caseNum) != 0) {
        printf("仿真数据加载失败，程序将继续运行\n");
    }
    
    /* 6. 重新启动TK8710 */
    ret = TK8710Start(TK8710_MODE_LOOPBACK, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        printf("TK8710重新启动失败: %d\n", ret);
        return -1;
    }
    
    printf("测试配置完成，等待测试结果...\n");
    return 0;
}

/**
 * @brief 执行完整的初始化流程
 * @return 0-成功, 非0-失败
 */
static int __attribute__((unused)) perform_full_initialization(void)
{
    int ret;
    
    printf("\n");
    printf("+======================================+\n");
    printf("|   TK8710 Main Test Program           |\n");
    printf("|   Version: 1.0 (RK3506)             |\n");
    printf("|   Complete Init, Config, Workflow   |\n");
    printf("+======================================+\n");
    
#ifndef _WIN32
    /* 注册Linux信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    
    /* 注册Driver回调函数 */
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = OnDriverSlotEnd,
        .onError = OnDriverError
    };
    TK8710RegisterCallbacks(&driverCallbacks);
    printf("Driver callbacks registered\n");
    
    /* ========== 使用 HAL API 进行初始化 ========== */
    /* 1. 准备RF配置 */
    static ChiprfConfig __attribute__((unused)) rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .Freq = 509100000,
        .rxgain = 0x7e,
        .txgain = 0x2a,
        .txadc = {//2号板
            {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
            {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        }
    };
    
    /* 2. 准备芯片配置 (与原 init_tk8710_chip 配置一致) */
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 1,
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = 0,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        .rfConfig    = NULL  /* RF配置在TK8710Init中自动调用 */
    };
    
    /* 3. 初始化默认日志系统（如果尚未初始化） */
    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_WARN,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
        .enable_file_logging = 1,
        .log_file_dir = NULL
    };
    TK8710LogInit(&defaultLogConfig);

    /* 5. 调用 8710 init 完成芯片、RF初始化 */
    printf("Initializing 8710 (chip + RF)...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("8710 initialization failed: %d\n", ret);
        return -1;
    }

    printf("HAL initialization completed (including RF)\n");
    
    /* 7. 启动TK8710 */
    // 启动TK8710芯片，使用Loopback模式和连续工作模式
    ret = TK8710Start(TK8710_MODE_LOOPBACK, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        return TK8710_HAL_ERROR_START;
    }    

    printf("\nSystem initialization completed, ready for automatic testing\n");
    return 0;
}

/**
 * @brief 执行遍历测试
 */
static void run_automatic_test(void)
{
    int totalTests = 0;
    int passedTests = 0;
    
    printf("\n=== 开始自动遍历测试 ===\n");
    
    /* 打开结果文件 */
    g_resultFile = fopen("8710LoopbackTestResult.txt", "w");
    if (g_resultFile == NULL) {
        printf("无法创建结果文件: 8710LoopbackTestResult.txt\n");
        return;
    }
    
    /* 写入文件头 */
    fprintf(g_resultFile, "TK8710 Loopback 自动测试结果\n");
    fprintf(g_resultFile, "=====================================\n\n");
    fprintf(g_resultFile, "模式\tSlot1\tSlot3\tClass\tCase\t标准\t最大用户\t持续时间\t结果\n");
    fprintf(g_resultFile, "----\t-----\t-----\t-----\t----\t----\t--------\t--------\t----\n");
    
    /* 遍历所有测试配置 */
    for (size_t configIndex = 0; configIndex < sizeof(testConfigs) / sizeof(testConfigs[0]); configIndex++) {
        TestCase_t testCase = testConfigs[configIndex];
        
        /* 确定遍历范围 */
        int maxS1Len = (testCase.mode >= 9) ? 16 : 10;
        int maxS3Len = (testCase.mode >= 9) ? 16 : 10;
        // int maxS1Len = 1;
        // int maxS3Len = 1;

        
        /* 遍历slot1字节长度 */
        for (int s1Len = 1; s1Len <= maxS1Len; s1Len++) {
            /* 遍历slot3字节长度 */
            for (int s3Len = 1; s3Len <= maxS3Len; s3Len++) {
                totalTests++;
                
                /* 开始单个测试 */
                if (start_single_test(&testCase, s1Len*26, s3Len*26) != 0) {
                    printf("测试启动失败\n");
                    continue;
                }
                
                /* 等待测试完成 */
                while (!g_trmTestCompleted) {
                    usleep(100000); /* 等待100ms */
                    
                    /* 在等待期间也计算测试持续时间 */
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    uint32_t currentTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                    g_trmTestDuration = currentTime - g_trmTestStartTime;
                    
                    /* 如果时间超过1分钟（60秒），强制结束测试 */
                    if (g_trmTestDuration > 60000) {
                        g_trmTestRunning = 0;
                        g_trmTestCompleted = 1;
                        g_currentTest->maxValidUsers = g_trmMaxValidUsers;
                        g_currentTest->testDuration = g_trmTestDuration;
                        g_currentTest->passed = (g_trmMaxValidUsers >= g_currentTest->threshold) ? 1 : 0;
                        printf("测试超时（1分钟），强制结束\n");
                        break;
                    }
                }
                
                /* 停止测试 */
                g_trmTestRunning = 0;
                
                /* 测试结束后重置TK8710 */
                TK8710Reset(TK8710_RST_STATE_MACHINE);
                TK8710Reset(TK8710_RST_ALL);
                
                /* 记录结果 */
                fprintf(g_resultFile, "%d\t%d\t%d\t%d\t%d\t%u\t%u\t%ums\t%s\n",
                    testCase.mode, s1Len, s3Len, testCase.classNum, testCase.caseNum,
                    testCase.threshold, testCase.maxValidUsers, testCase.testDuration,
                    testCase.passed ? "通过" : "失败");
                
                printf("测试结果: %s (最大用户数: %u, 标准: %u)\n",
                    testCase.passed ? "通过" : "失败",
                    testCase.maxValidUsers, testCase.threshold);
                
                if (testCase.passed) {
                    passedTests++;
                }
                
                /* 测试间隔 */
                usleep(500000); /* 等待500ms */
            }
        }
    }
    
    /* 写入总结 */
    fprintf(g_resultFile, "\n=====================================\n");
    fprintf(g_resultFile, "总测试数: %d\n", totalTests);
    fprintf(g_resultFile, "通过测试: %d\n", passedTests);
    fprintf(g_resultFile, "失败测试: %d\n", totalTests - passedTests);
    fprintf(g_resultFile, "通过率: %.2f%%\n", (float)passedTests * 100.0f / totalTests);
    
    fclose(g_resultFile);
    g_resultFile = NULL;
    
    printf("\n=== 自动测试完成 ===\n");
    printf("总测试数: %d\n", totalTests);
    printf("通过测试: %d\n", passedTests);
    printf("失败测试: %d\n", totalTests - passedTests);
    printf("通过率: %.2f%%\n", (float)passedTests * 100.0f / totalTests);
    printf("结果已保存到: 8710LoopbackTestResult.txt\n");
}

/*============================================================================
 * 主程序
 *============================================================================*/

/**
 * @brief 从文件读取数据并通过SPI传输
 * @param classNum class序号 (如: 1, 3, 等)
 * @param caseNum case序号 (如: 11, 17, 等)
 * @return 0-成功, 非0-失败
 */
int load_and_send_simulation_data(int classNum, int caseNum)
{
    char filePath[256];
    FILE *fp;
    char lineBuffer[16384];  /* 增加缓冲区大小以容纳2048个20bit值 (2048×8=16384字符) */
    uint8_t spiDataBuffer[5120];  /* 增加缓冲区大小以容纳128用户×16AH×20bit=5120字节 */
    int dataCount;
    
    printf("\n=== 开始加载目录：/class%d/case%d下仿真数据并传输 ===\n", classNum, caseNum);
    
    /* 1. 读取 ANoise 数据 */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/ANoise.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filePath);
        return -1;
    }
    
    if (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL) {
        /* 解析 ANoise 数据 (8个值) */
        uint16_t anoiseData[8];
        dataCount = sscanf(lineBuffer, "%hu %hu %hu %hu %hu %hu %hu %hu",
                          &anoiseData[0], &anoiseData[1], &anoiseData[2], &anoiseData[3],
                          &anoiseData[4], &anoiseData[5], &anoiseData[6], &anoiseData[7]);
        
        if (dataCount == 8) {
            /* 转换为字节数组并发送 */
            for (int i = 0; i < 8; i++) {
                spiDataBuffer[i*2] = (uint8_t)(anoiseData[i] >> 8);
                spiDataBuffer[i*2+1] = (uint8_t)(anoiseData[i] & 0xFF);
            }
            
            /* 打印前16个输入数据 (十六进制格式) */
            printf("ANoise SPI输入数据 (前16字节): ");
            for (int i = 0; i < 16; i++) {
                printf("0x%02X ", spiDataBuffer[i]);
            }
            printf("\n");
            
            int ret = TK8710SpiSetInfo(TK8710_SET_INFO_ANOISE, spiDataBuffer, 16);
            if (ret == 0) {
                printf("ANoise 数据传输成功: 8个值\n");
            } else {
                printf("ANoise 数据传输失败: %d\n", ret);
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);
    
    /* 2. 读取 GWRXAH 数据 (按用户格式处理: 128个用户，每个用户16个20bit AH) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/GWRXAH.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filePath);
        return -1;
    }
    
    uint32_t gwrxahData[2048];  // 128用户 * 16AH = 2048个20bit值
    dataCount = 0;
    
    while (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL && dataCount < 2048) {
        char *ptr = lineBuffer;
        while (*ptr != '\0' && dataCount < 2048) {
            uint32_t value;
            if (sscanf(ptr, "%u", &value) == 1) {
                gwrxahData[dataCount++] = value;
                /* 跳过当前数字 */
                while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ptr++;
                /* 跳过空白字符 */
                while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
            } else {
                break;
            }
        }
    }
    fclose(fp);
    
    printf("GWRXAH 读取了 %d 个20bit数据值\n", dataCount);
    
    if (dataCount > 0) {
        /* 将20bit数据按用户格式打包到字节缓冲区
         * 每个用户16个20bit AH = 320bit = 40字节
         * 128个用户总共需要 128 * 40 = 5120字节
         */
        int userCount = dataCount / 16;  // 计算实际用户数
        if (userCount > 128) userCount = 128;
        
        printf("处理 %d 个用户的GWRXAH数据\n", userCount);
        
        /* 按用户顺序打包20bit数据到字节数组
         * 使用位操作实现紧凑的20bit数据打包
         */
        int byteIndex = 0;
        uint32_t bitBuffer = 0;
        int bitsInBuffer = 0;
        
        for (int user = 0; user < userCount; user++) {
            for (int ah = 0; ah < 16; ah++) {
                uint32_t ahValue = gwrxahData[user * 16 + ah] & 0xFFFFF;  // 确保只取20bit
                
                /* 将20bit数据添加到位缓冲区 */
                bitBuffer = (bitBuffer << 20) | ahValue;
                bitsInBuffer += 20;
                
                /* 当缓冲区有8位或更多时，提取字节 */
                while (bitsInBuffer >= 8 && (size_t)byteIndex < sizeof(spiDataBuffer) - 1U) {
                    spiDataBuffer[byteIndex++] = (uint8_t)((bitBuffer >> (bitsInBuffer - 8)) & 0xFF);
                    bitsInBuffer -= 8;
                }
            }
        }
        
        /* 处理剩余的位 */
        if (bitsInBuffer > 0 && (size_t)byteIndex < sizeof(spiDataBuffer)) {
            spiDataBuffer[byteIndex++] = (uint8_t)((bitBuffer << (8 - bitsInBuffer)) & 0xFF);
        }
        
        /* 打印前16个输入数据 (十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("GWRXAH SPI输入数据 (前%d字节): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("GWRXAH 数据打包完成: %d用户, 每用户16个AH, 总字节数: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_AH, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("GWRXAH 数据传输成功: %d个用户, 每用户16个AH\n", userCount);
        } else {
            printf("GWRXAH 数据传输失败: %d\n", ret);
            return -1;
        }
    }
    
    /* 3. 读取 GWRxPilotPower 数据 (按用户格式处理: 128个用户，每个用户40bit PilotPower) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/GWRxPilotPower.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filePath);
        return -1;
    }
    
    uint64_t pilotPowerData[128];  // 128个用户，每个用户40bit PilotPower
    dataCount = 0;
    
    while (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL && dataCount < 128) {
        char *ptr = lineBuffer;
        while (*ptr != '\0' && dataCount < 128) {
            uint64_t value;
            if (sscanf(ptr, "%llu", &value) == 1) {
                pilotPowerData[dataCount++] = value;
                /* 跳过当前数字 */
                while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ptr++;
                /* 跳过空白字符 */
                while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
            } else {
                break;
            }
        }
    }
    fclose(fp);
    
    printf("GWRxPilotPower 读取了 %d 个40bit数据值\n", dataCount);
    
    if (dataCount > 0) {
        /* 将40bit数据按用户格式打包到字节缓冲区
         * 每个用户40bit PilotPower = 5字节
         * 128个用户总共需要 128 * 5 = 640字节
         */
        int userCount = dataCount;  // 每个用户一个40bit值
        if (userCount > 128) userCount = 128;
        
        printf("处理 %d 个用户的GWRxPilotPower数据\n", userCount);
        
        /* 按用户顺序打包40bit数据到字节数组
         * 40bit数据可以直接拆分为5个字节
         */
        int byteIndex = 0;
        
        for (int user = 0; user < userCount; user++) {
            uint64_t pilotValue = pilotPowerData[user] & 0xFFFFFFFFFFULL;  // 确保只取40bit
            
            /* 将40bit数据拆分为5个字节 (大端序) */
            if ((size_t)(byteIndex + 4) < sizeof(spiDataBuffer)) {
                spiDataBuffer[byteIndex] = (uint8_t)((pilotValue >> 32) & 0xFF);    // 字节0 (最高8位)
                spiDataBuffer[byteIndex + 1] = (uint8_t)((pilotValue >> 24) & 0xFF); // 字节1
                spiDataBuffer[byteIndex + 2] = (uint8_t)((pilotValue >> 16) & 0xFF); // 字节2
                spiDataBuffer[byteIndex + 3] = (uint8_t)((pilotValue >> 8) & 0xFF);  // 字节3
                spiDataBuffer[byteIndex + 4] = (uint8_t)(pilotValue & 0xFF);         // 字节4 (最低8位)
                
                byteIndex += 5;
            }
        }
        
        /* 打印前16个输入数据 (十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("GWRxPilotPower SPI输入数据 (前%d字节): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("GWRxPilotPower 数据打包完成: %d用户, 每用户40bit, 总字节数: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_PILOT_POW, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("GWRxPilotPower 数据传输成功: %d个用户, 每用户40bit PilotPower\n", userCount);
        } else {
            printf("GWRxPilotPower 数据传输失败: %d\n", ret);
            return -1;
        }
    }
    
    /* 4. 读取 TxFreq 数据 (按用户格式处理: 128个用户，每个用户32bit TxFreq) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/TxFreq.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filePath);
        return -1;
    }
    
    uint32_t txFreqData[128];  // 128个用户，每个用户32bit TxFreq
    dataCount = 0;
    
    while (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL && dataCount < 128) {
        char *ptr = lineBuffer;
        while (*ptr != '\0' && dataCount < 128) {
            uint32_t value;
            if (sscanf(ptr, "%u", &value) == 1) {
                txFreqData[dataCount++] = value;
                /* 跳过当前数字 */
                while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ptr++;
                /* 跳过空白字符 */
                while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
            } else {
                break;
            }
        }
    }
    fclose(fp);
    
    printf("TxFreq 读取了 %d 个32bit数据值\n", dataCount);
    
    if (dataCount > 0) {
        /* 将32bit数据按用户格式打包到字节缓冲区
         * 每个用户32bit TxFreq = 4字节
         * 128个用户总共需要 128 * 4 = 512字节
         */
        int userCount = dataCount;  // 每个用户一个32bit值
        if (userCount > 128) userCount = 128;
        
        printf("处理 %d 个用户的TxFreq数据\n", userCount);
        
        /* 按用户顺序打包32bit数据到字节数组
         * 32bit数据可以直接拆分为4个字节
         */
        int byteIndex = 0;
        
        for (int user = 0; user < userCount; user++) {
            uint32_t freqValue = txFreqData[user];
            
            /* 将32bit数据拆分为4个字节 (大端序) */
            if ((size_t)(byteIndex + 3) < sizeof(spiDataBuffer)) {
                spiDataBuffer[byteIndex] = (uint8_t)((freqValue >> 24) & 0xFF);     // 字节0 (最高8位)
                spiDataBuffer[byteIndex + 1] = (uint8_t)((freqValue >> 16) & 0xFF); // 字节1
                spiDataBuffer[byteIndex + 2] = (uint8_t)((freqValue >> 8) & 0xFF);  // 字节2
                spiDataBuffer[byteIndex + 3] = (uint8_t)(freqValue & 0xFF);         // 字节3 (最低8位)
                
                byteIndex += 4;
            }
        }
        
        /* 打印前16个输入数据 (十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("TxFreq SPI输入数据 (前%d字节): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("TxFreq 数据打包完成: %d用户, 每用户32bit, 总字节数: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_TX_FREQ, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("TxFreq 数据传输成功: %d个用户, 每用户32bit TxFreq\n", userCount);
        } else {
            printf("TxFreq 数据传输失败: %d\n", ret);
            return -1;
        }
    }
    
    /* 5. 读取 TxPower 数据 (按用户格式处理: 128个用户，每个用户8bit TxPower) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/TxPower.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filePath);
        return -1;
    }
    
    uint8_t txPowerData[128];  // 128个用户，每个用户8bit TxPower
    dataCount = 0;
    
    while (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL && dataCount < 128) {
        char *ptr = lineBuffer;
        while (*ptr != '\0' && dataCount < 128) {
            uint16_t value;
            if (sscanf(ptr, "%hu", &value) == 1) {
                txPowerData[dataCount++] = (uint8_t)(value & 0xFF);  // 确保只取8bit
                /* 跳过当前数字 */
                while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ptr++;
                /* 跳过空白字符 */
                while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
            } else {
                break;
            }
        }
    }
    fclose(fp);
    
    printf("TxPower 读取了 %d 个8bit数据值\n", dataCount);
    
    if (dataCount > 0) {
        /* 将8bit数据按用户格式打包到字节缓冲区
         * 每个用户8bit TxPower = 1字节
         * 128个用户总共需要 128 * 1 = 128字节
         */
        int userCount = dataCount;  // 每个用户一个8bit值
        if (userCount > 128) userCount = 128;
        
        printf("处理 %d 个用户的TxPower数据\n", userCount);
        
        /* 按用户顺序直接复制8bit数据到字节数组 */
        int byteIndex = 0;
        int ret = 0;
        uint8_t Data[30];
        uint8_t Len = 22;

        for (int user = 0; user < userCount; user++) {
            for(int i = 0; i < Len; i++){
                if(i < 4){
                    Data[i] = user + i;
                }else{
                    Data[i] = rand()%255;
                }
                
            }
            if ((size_t)byteIndex < sizeof(spiDataBuffer)) {
                spiDataBuffer[byteIndex] = txPowerData[user];
                byteIndex++;
            }
            s_tx_pow_ctrl tx_pow_ctrl;
            tx_pow_ctrl.data = 0;
            tx_pow_ctrl.b.UserIndex = user;
            tx_pow_ctrl.b.power = txPowerData[user];
            
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);

            ret = TK8710WriteBuffer(user, Data, Len);
            if(user < 16){
                tx_pow_ctrl.data = 0;
                tx_pow_ctrl.b.UserIndex = user + 128;
                tx_pow_ctrl.b.power = txPowerData[user];
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, tx_pow_ctrl), tx_pow_ctrl.data);
                                    /* 发送广播数据 */
               ret = TK8710WriteBuffer(user + 128, Data, Len);
            }
        }
        
        /* 打印前16个输入数据 (十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("TxPower SPI输入数据 (前%d字节): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("TxPower 数据打包完成: %d用户, 每用户8bit, 总字节数: %d\n", userCount, byteIndex);

        if (ret == 0) {
            printf("TxPower 数据传输成功: %d个用户, 每用户8bit TxPower\n", userCount);
        } else {
            printf("TxPower 数据传输失败: %d\n", ret);
            return -1;
        }
    }
    
    printf("=== 所有仿真数据传输完成 ===\n");
    
    /* 设置仿真数据加载标志，通知中断处理系统 */
    TK8710SetSimulationDataLoaded(1);
    printf("仿真数据加载标志已设置\n\n");
    return 0;
}

/**
 * @brief 显示帮助信息
 */
void show_help(void)
{
    printf("\n=== TK8710 Driver + TRM Test Commands ===\n");
    printf("Driver Commands:\n");
    printf("  h/H - Show this help information\n");
    printf("  s/S - Show system status\n");
    printf("  i/I - Show interrupt statistics\n");
    printf("  c/C - Clear screen\n");
    printf("  l/L - Load and send simulation data from files\n");
    printf("  q/Q - Quit program\n");
    printf("\nTRM Commands (when TRM enabled):\n");
    printf("  t/T - Show TRM statistics\n");
    printf("\nDriver Test Commands:\n");
    printf("  a/A - Auto downlink test (all users)\n");
    printf("  m/M - Manual downlink test (single user)\n");
    printf("  r/R - Reset chip\n");
    printf("+--------------------------------------+\n");
}
void show_system_status(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint8_t rateMode = TK8710GetRateMode();
    uint8_t workType = TK8710GetWorkType();
    uint8_t brdUserNum = TK8710GetBrdUserNum();
    
    printf("\n=== System Status ===\n");
    const char* modeStr;
    switch (workType) {
        case TK8710_MODE_MASTER:
            modeStr = "Master";
            break;
        case TK8710_MODE_SLAVE:
            modeStr = "Slave";
            break;
        case TK8710_MODE_LOOPBACK:
            modeStr = "Loopback";
            break;
        default:
            modeStr = "Unknown";
            break;
    }
    printf("Work mode: %s\n", modeStr);
    printf("Rate mode: %d\n", rateMode);
    printf("Broadcast users: %d\n", brdUserNum);
    printf("Antenna enable: 0x%02X\n", slotCfg->antEn);
    printf("RF select: 0x%02X\n", slotCfg->rfSel);
    printf("Transmit mode: %s\n", slotCfg->txBeamCtrlMode ? "Specified info transmit" : "Auto transmit");
    printf("BCN antenna enable: %s\n", slotCfg->txBcnAntEn ? "Yes" : "No");
    printf("Slot configuration:\n");
    printf("  S1(FDL): %d bytes, freq: %u\n", slotCfg->s1Cfg[0].byteLen, slotCfg->s1Cfg[0].centerFreq);
    printf("  S2(ADL): %d bytes, freq: %u\n", slotCfg->s2Cfg[0].byteLen, slotCfg->s2Cfg[0].centerFreq);
    printf("  S3(UL):  %d bytes, freq: %u\n", slotCfg->s3Cfg[0].byteLen, slotCfg->s3Cfg[0].centerFreq);
    printf("=== Status End ===\n\n");
}

/**
 * @brief 显示中断统计
 */
void show_irq_statistics(void)
{
    uint32_t counters[10];
    uint32_t irqStatus;
    
    printf("\n=== Interrupt Statistics ===\n");
    
    /* 获取所有中断计数器 */
    TK8710GetAllIrqCounters(counters);
    
    /* 获取当前中断状态 */
    irqStatus = TK8710GetIrqStatus();
    
    printf("Interrupt counters:\n");
    printf("  RX_BCN (0):    %u\n", counters[0]);
    printf("  BRD_UD (1):    %u\n", counters[1]);
    printf("  BRD_DATA (2):  %u\n", counters[2]);
    printf("  MD_UD (3):      %u\n", counters[3]);
    printf("  MD_DATA (4):   %u\n", counters[4]);
    printf("  S0 (5):        %u\n", counters[5]);
    printf("  S1 (6):        %u\n", counters[6]);
    printf("  S2 (7):        %u\n", counters[7]);
    printf("  S3 (8):        %u\n", counters[8]);
    printf("  ACM (9):       %u\n", counters[9]);
    
    printf("Current interrupt status: 0x%08X\n", irqStatus);
    printf("=== Statistics End ===\n\n");
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[])
{
    // Set CPU affinity to core 2
    if (set_cpu_affinity(2) < 0) {
        return 1;
    }
    int ret;
    char input;
    int testMode = 6;  /* 默认模式6 */
    int classNum = 3;  /* 默认class序号 */
    int caseNum = 11;  /* 默认case序号 */
    int s1ByteLen = 22; /* 默认S1 byteLen */
    int s2ByteLen = 0;  /* 默认S2 byteLen */
    int s3ByteLen = 22; /* 默认S3 byteLen */
    
    /* 检查命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [mode] [class] [case] [s1_byteLen] [s2_byteLen] [s3_byteLen] [--help|-h] [--auto]\n", argv[0]);
            printf("  mode: Test mode (5,6,7,8,9,10,11,18), default: 6\n");
            printf("    Mode 5:  s0=40*256, s1=0, s2=0, s3=135072\n");
            printf("    Mode 6:  s0=46*256, s1=0, s2=0, s3=69536\n");
            printf("    Mode 7:  s0=113*256, s1=0, s2=0, s3=36768\n");
            printf("    Mode 8:  s0=146*256, s1=0, s2=0, s3=20384\n");
            printf("    Mode 9:  s0=64*256, s1=0, s2=0, s3=12192\n");
            printf("    Mode 10: s0=19*256, s1=0, s2=0, s3=8096\n");
            printf("    Mode 11: s0=256, s1=0, s2=0, s3=6084\n");
            printf("    Mode 18: s0=256, s1=0, s2=0, s3=6084\n");
            printf("  class: Simulation data class number (1,3,etc), default: 3\n");
            printf("  case:  Simulation data case number (11,17,etc), default: 11\n");
            printf("  s1_byteLen: S1 slot byte length, default: 22 (0-512)\n");
            printf("  s2_byteLen: S2 slot byte length, default: 0 (0-512)\n");
            printf("  s3_byteLen: S3 slot byte length, default: 22 (0-512)\n");
            printf("  --auto: Run automatic traversal test\n");
            printf("  --help, -h: Show this help\n");
            printf("\nExamples:\n");
            printf("  %s                        # Use default settings\n", argv[0]);
            printf("  %s --auto                 # Run automatic traversal test\n", argv[0]);
            printf("  %s 6 3 11                # Use mode 6, class 3, case 11, default byteLen 22,0,22\n", argv[0]);
            printf("  %s 6 3 11 30 0 25      # Use mode 6, class 3, case 11, S1=30, S2=0, S3=25\n", argv[0]);
            printf("  %s 6 1 17 22 10 30      # Use mode 6, class 1, case 17, S1=22, S2=10, S3=30\n", argv[0]);
            return 0;
        }
        
        /* 检查是否为自动测试模式 */
        if (strcmp(argv[1], "--auto") == 0) {
            printf("启动自动遍历测试模式...\n");
            
            /* 运行自动测试（每个测试会自己执行初始化和回调注册） */
            run_automatic_test();
            
            printf("测试完成，程序退出\n");
            return 0;
        }
        
        testMode = atoi(argv[1]);
        if (testMode < 5 || testMode > 18 || (testMode > 11 && testMode < 18)) {
            printf("Error: Invalid mode %d. Supported modes: 5,6,7,8,9,10,11,18\n", testMode);
            return 1;
        }
        
        /* 解析class参数 */
        if (argc > 2) {
            classNum = atoi(argv[2]);
            if (classNum <= 0) {
                printf("Error: Invalid class number %d. Must be positive integer\n", classNum);
                return 1;
            }
        }
        
        /* 解析case参数 */
        if (argc > 3) {
            caseNum = atoi(argv[3]);
            if (caseNum <= 0) {
                printf("Error: Invalid case number %d. Must be positive integer\n", caseNum);
                return 1;
            }
        }
        
        /* 解析S1 byteLen参数 */
        if (argc > 4) {
            s1ByteLen = atoi(argv[4]);
            if (s1ByteLen < 0 || s1ByteLen > 512) {
                printf("Error: Invalid S1 byteLen %d. Must be 0-512\n", s1ByteLen);
                return 1;
            }
        }
        
        /* 解析S2 byteLen参数 */
        if (argc > 5) {
            s2ByteLen = atoi(argv[5]);
            if (s2ByteLen < 0 || s2ByteLen > 512) {
                printf("Error: Invalid S2 byteLen %d. Must be 0-512\n", s2ByteLen);
                return 1;
            }
        }
        
        /* 解析S3 byteLen参数 */
        if (argc > 6) {
            s3ByteLen = atoi(argv[6]);
            if (s3ByteLen < 0 || s3ByteLen > 512) {
                printf("Error: Invalid S3 byteLen %d. Must be 0-512\n", s3ByteLen);
                return 1;
            }
        }
        
        printf("Using test mode: %d, class: %d, case: %d, S1 byteLen: %d, S2 byteLen: %d, S3 byteLen: %d\n", 
               testMode, classNum, caseNum, s1ByteLen, s2ByteLen, s3ByteLen);
    } else {
        printf("Using default: mode %d, class %d, case %d\n", testMode, classNum, caseNum);
    }
    
#ifdef _WIN32
    /* 设置控制台编码为UTF-8 */
    SetConsoleOutputCP(65001);  // UTF-8
    SetConsoleCP(65001);       // UTF-8
    setlocale(LC_ALL, ".UTF8");
#endif
    
    printf("\n");
    printf("+======================================+\n");
    printf("|   TK8710 Main Test Program           |\n");
    printf("|   Version: 1.0 (RK3506)             |\n");
    printf("|   Complete Init, Config, Workflow   |\n");
    printf("+======================================+\n");
    
#ifndef _WIN32
    /* 注册Linux信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    
    /* 注册Driver回调函数 */
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = OnDriverSlotEnd,
        .onError = OnDriverError
    };
    TK8710RegisterCallbacks(&driverCallbacks);
    printf("Driver callbacks registered\n");
    
    /* ========== 使用 HAL API 进行初始化 ========== */
    /* 1. 准备RF配置 */
    static ChiprfConfig __attribute__((unused)) rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_32M,
        .Freq = 509100000,
        .rxgain = 0x7e,
        .txgain = 0x2a,
        // .txadc = {//C号板
        //     {0x0bc0, 0x04a0}, {0x0a50, 0x0780}, {0x0750, 0x0820}, {0x0bc3, 0x0940},
        //     {0x0e83, 0x05e0}, {0xfbff, 0x0850}, {0x0880, 0x0500}, {0x02a0, 0x06ff}
        // }
        // .txadc = {//D号板
        //     {0x0450, 0x0450}, {0x0a00, 0x1080}, {0x0750, 0x1500}, {0x0400, 0x0b00},
        //     {0x08a0, 0x07a0}, {0x0990, 0xff00}, {0x0850, 0x08c8}, {0x0950, 0x0a00}
        // }
        .txadc = {//2号板
            {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
            {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        }
    };
    
    /* 2. 准备芯片配置 (与原 init_tk8710_chip 配置一致) */
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 1,
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = 0,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        // .rfConfig    = (struct ChiprfConfig_s*)&rfConfig  /* RF配置在TK8710Init中自动调用 */
        .rfConfig    = NULL  /* RF配置在TK8710Init中自动调用 */
    };
    
    /* 3. 初始化默认日志系统（如果尚未初始化） */
    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_INFO,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
        .enable_file_logging = 1,
        .log_file_dir = NULL
    };
    TK8710LogInit(&defaultLogConfig);
    
    /* 5. 调用 8710 init 完成芯片、RF初始化 */
    printf("Initializing 8710 (chip + RF)...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("8710 initialization failed: %d\n", ret);
        return -1;
    }

    printf("HAL initialization completed (including RF)\n");
    
    /* 6. 配置时隙参数 - 使用 TK8710HalCfg */
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    
    /* 配置基本参数 (与原配置一致) */
    slotCfg.msMode = TK8710_MODE_LOOPBACK;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 0;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
    slotCfg.txBcnAntEn = 0x7f;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;
    
    /* 配置BCN轮流发送 */
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;
    }
    
    /* 单速率配置 */
    printf("Using single-rate configuration for mode %d\n", testMode);
    slotCfg.rateCount = 1;
    slotCfg.rateModes[0] = testMode;
    
    /* 根据模式设置不同的da_m值 */
    switch (testMode) {
        case 5:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 65000;
            break;
        case 6:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 65536;
            break;
        case 7:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 32768;
            break;
        case 8:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 8000;
            break;
        case 9:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 8192;
            break;
        case 10:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 4096;
            break;
        case 11:
        case 18:
            slotCfg.s0Cfg[0].da_m = 0;
            slotCfg.s1Cfg[0].da_m = 60000;
            slotCfg.s2Cfg[0].da_m = 0;
            slotCfg.s3Cfg[0].da_m = 0;
            break;
        default:
            printf("Error: Unsupported mode %d\n", testMode);
            return -1;
    }
    slotCfg.s0Cfg[0].byteLen = 0;
    slotCfg.s0Cfg[0].centerFreq = 509100000;
    slotCfg.s1Cfg[0].byteLen = s1ByteLen;
    slotCfg.s1Cfg[0].centerFreq = 509100000;
    slotCfg.s2Cfg[0].byteLen = s2ByteLen;
    slotCfg.s2Cfg[0].centerFreq = 509100000;
    slotCfg.s3Cfg[0].byteLen = s3ByteLen;
    slotCfg.s3Cfg[0].centerFreq = 509100000;
    
    /* 调用 8710 config 配置时隙 */
    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slotCfg);
    if (ret != TK8710_OK) {
        return -1;
    }
    printf("Slot parameter configuration completed\n");
    
    /* 自动加载并发送仿真数据 */
    printf("\n自动加载仿真数据...\n");
    if (load_and_send_simulation_data(classNum, caseNum) != 0) {
        printf("警告: 仿真数据加载失败，程序将继续运行\n");
    }

    /* 7. 启动TK8710 */
    // 启动TK8710芯片，使用Loopback模式和连续工作模式
    ret = TK8710Start(TK8710_MODE_LOOPBACK, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        return TK8710_HAL_ERROR_START;
    }    

    printf("\nSystem initialization completed, starting runtime...\n");
    printf("Enter 'h' for help information\n\n");
    
    /* 8. 主循环 - 等待中断并进行中断处理 */
    while (g_running) {
        printf("TK8710> ");
        
#ifdef _WIN32
        input = _getch();
        if (input != '\r') {
            printf("%c\n", input);
        } else {
            printf("\n");
            continue;
        }
#else
        scanf(" %c", &input);
#endif
        
        switch (input) {
            case 'h':
            case 'H':
                show_help();
                break;
                
            case 's':
            case 'S':
                show_system_status();
                break;
                
            case 'i':
            case 'I':
                show_irq_statistics();
                break;
                
            case 'c':
            case 'C':
#ifdef _WIN32
                system("cls");
#else
                system("clear");
#endif
                break;
                
            case 'l':
            case 'L':
                if (load_and_send_simulation_data(classNum, caseNum) != 0) {
                    printf("仿真数据加载和传输失败\n");
                }
                break;
                
            case 't':
            case 'T':
                show_trm_statistics();
                break;
                
            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
                break;

            case 'r':
            case 'R':
                read_register();
                break;
                
            case 'w':
            case 'W':
                write_register();
                break;

            default:
                printf("Unknown command: %c (enter 'h' for help)\n", input);
                break;
        }
    }

    defaultLogConfig.level = TK8710_LOG_ALL;
    TK8710LogInit(&defaultLogConfig);
    /* 打印最终的中断时间统计报告 */
    printf("\n=== 最终中断处理时间统计报告 ===\n");
    TK8710PrintIrqTimeStats();

    TK8710Reset(TK8710_RST_STATE_MACHINE);
    TK8710Reset(TK8710_RST_ALL);
    printf("Program ended\n");
    return 0;
}

/*============================================================================
 * 编译说明
 *============================================================================
 * 
 * RK3506 编译命令 (在开发板上):
 *   gcc -I../inc -I../port test8710main.c ../port/tk8710_rk3506.c \
 *       ../src/tk8710_irq.c ../src/tk8710_core.c ../src/tk8710_config.c \
 *       ../src/tk8710_log.c -o test8710main -lgpiod -lpthread
 * 
 * RK3506 交叉编译命令 (在PC上):
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port test8710main.c \
 *       ../port/tk8710_rk3506.c ../src/tk8710_irq.c ../src/tk8710_core.c \
 *       ../src/tk8710_config.c ../src/tk8710_log.c \
 *       -o test8710main -lgpiod -lpthread
 * 
 * 运行:
 *   ./test8710main      (RK3506 Linux)
 * 
 * 功能说明:
 * 1. 完整的TK8710初始化流程
 * 2. RK3506 SPI接口初始化 (/dev/spidev0.0)
 * 3. GPIO中断初始化 (libgpiod)
 * 4. 射频系统配置
 * 5. 8710芯片初始化
 * 6. 时隙参数配置 (TK8710_CFG_TYPE_SLOT_CFG)
 * 7. 启动主模式连续工作
 * 8. 实时中断处理和状态监控
 * 9. 交互式命令行界面
 * 
 * 注意事项:
 * 1. 需要TK8710硬件连接到RK3506的SPI接口
 * 2. 确保/dev/spidev0.0存在且有访问权限
 * 3. 确保libgpiod已安装
 * 4. 可能需要root权限运行
 * 5. 按Ctrl+C可安全退出程序
 */


