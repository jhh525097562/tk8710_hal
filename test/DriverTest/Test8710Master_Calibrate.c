/**
 * @file test8710main.c
 * @brief TK8710 主测试程序
 * @note 完整的初始化、配置、工作和中断处理流程
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
#include "driver/tk8710_rf_regs.h"
#include "driver/tk8710_regs.h"

#include "driver_test_platform.h"

/*============================================================================
 * 全局变量和配置
 *============================================================================*/

/* 运行标志 */
static volatile int g_running = 1;


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


/* 从TxDC目录读取txadc配置的函数 */
int LoadTxadcConfig(const char* filename, uint16_t txadc[8][2]) {
    FILE* file;
    char filepath[256];
    char line[256];
    int index = 0;

    /* 构建文件路径 */
    snprintf(filepath, sizeof(filepath), "TxDC/%s", filename);

    file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Warning: 无法打开文件 %s，使用默认配置\n", filepath);
        return -1;
    }

    printf("从文件 %s 读取txadc配置...\n", filepath);

    /* 逐行读取文件 */
    while (fgets(line, sizeof(line), file) != NULL && index < 8) {
        /* 跳过注释行和空行 */
        if (line[0] == '/' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        /* 解析两个十六进制数值 */
        uint32_t val1, val2;
        if (sscanf(line, "0x%x, 0x%x", &val1, &val2) == 2) {
            txadc[index][0] = (uint16_t)val1;
            txadc[index][1] = (uint16_t)val2;
            printf("  [%d] 0x%04x, 0x%04x\n", index, txadc[index][0], txadc[index][1]);
            index++;
        }
    }

    fclose(file);

    if (index == 8) {
        printf("成功读取8组txadc配置\n");
        return 0;
    } else {
        printf("警告: 只读取到%d组txadc配置，期望8组\n", index);
        return -1;
    }
}

static const TxAdcConfig g_txadc_gain_2a[TK8710_MAX_ANTENNAS] = {
    {0x0bc9, 0x0830}, {0x0da0, 0x0510}, {0x0b70, 0x0650}, {0x0300, 0x07e0},
    {0x0e30, 0x0dc0}, {0x0a20, 0x09a0}, {0x0900, 0x0890}, {0x0960, 0x15b0}
};

static const TxAdcConfig g_txadc_gain_2e[TK8710_MAX_ANTENNAS] = {
    {0x0500, 0x0350}, {0x05a0, 0x0250}, {0x04c0, 0x0290}, {0x0150, 0x0350},
    {0x0600, 0x05b0}, {0x0420, 0x0400}, {0x03a0, 0x0390}, {0x03c0, 0x08d0}
};

static const TxAdcConfig* GetTxadcConfigByGain(uint8_t txgain)
{
    if (txgain == 0x2a) {
        return g_txadc_gain_2a;
    }
    if (txgain == 0x2e) {
        return g_txadc_gain_2e;
    }
    return NULL;
}

static int ApplyTxadcConfig(const TxAdcConfig txadc[TK8710_MAX_ANTENNAS])
{
    int ret;
    uint8_t antenna;

    for (antenna = 0; antenna < TK8710_MAX_ANTENNAS; antenna++) {
        uint32_t tx_fe_addr = TX_FE_BASE + antenna * 0x1000;
        s_tx_config_29 tx_config_29;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            tx_fe_addr + offsetof(struct tx_dac_if, tx_config_29),
                            &tx_config_29.data);
        if (ret != TK8710_OK) {
            printf("读取天线%u的1255直流分量失败: ret=%d\n", antenna, ret);
            return ret;
        }

        tx_config_29.b.tx_dci = (uint16_t)txadc[antenna].i;
        tx_config_29.b.tx_dcq = (uint16_t)txadc[antenna].q;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             tx_fe_addr + offsetof(struct tx_dac_if, tx_config_29),
                             tx_config_29.data);
        if (ret != TK8710_OK) {
            printf("配置天线%u的1255直流分量失败: ret=%d\n", antenna, ret);
            return ret;
        }
    }

    return TK8710_OK;
}

/* 下行发送状态跟踪 */
static volatile bool g_hasValidUsers = false;     /* 是否有有效用户 */
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

/* TRM相关变量 */
static uint32_t g_trmSendCount = 0;               /* TRM发送计数 */
static uint32_t g_trmRxCount = 0;                 /* TRM接收计数 */
static uint32_t __attribute__((unused)) g_irqCount = 0;                  /* 中断计数 */

/* 丢包统计变量 */
static volatile uint32_t g_packetCount = 0;               /* 当前统计周期内的包计数 */
static volatile uint32_t g_packetLostCount = 0;           /* 当前统计周期内的丢包计数 */
static volatile uint32_t g_totalPacketCount = 0;         /* 总包计数 */
static volatile uint32_t g_lastValidUserIndex = 0;       /* 上次有效用户索引 */
static volatile uint32_t g_simulationDataLoaded = 0;
static int g_testMode = 6;  /* 全局测试模式 */

/* 采集数据控制变量 */
static volatile uint8_t g_captureDataPending = 0;         /* 采集数据待执行标志 */
static volatile uint8_t g_captureDataPendingNum = 0;         /* 采集数据等待次数 */

/* TRM回调函数声明 */
static void __attribute__((unused)) OnTrmRxData(const TRM_RxDataList* rxDataList);
static void __attribute__((unused)) OnTrmTxComplete(const TRM_TxCompleteResult* txResult);

/* Driver回调函数声明 */
static void OnDriverRxData(TK8710IrqResult* irqResult);
static void OnDriverTxSlot(TK8710IrqResult* irqResult);
static void OnDriverSlotEnd(TK8710IrqResult* irqResult);
static void OnDriverError(TK8710IrqResult* irqResult);

/* 辅助函数声明 */
static uint32_t GetExpectedUserCount(int mode);

/**
 * @brief 测试指定信息发送功能
 * @param userIndices 用户索引数组
 * @param userCount 用户数量
 * @return 0-成功, 其他-失败
 */
static int test_manual_tx_setup(const uint8_t* userIndices, uint8_t userCount)
{
    int ret;

    printf("Setting up manual TX for %d users\n", userCount);

    /* 为每个用户设置发送信息 */
    for (int i = 0; i < userCount; i++) {
        uint8_t userIndex = userIndices[i];

        /* 从接收的用户信息Buffer获取数据 */
        uint32_t freq;
        uint32_t ahData[16];
        uint64_t pilotPower;

        /* 为每个用户生成发送数据 */
        uint8_t txData[26];
        for (int j = 0; j < 26; j++) {
            txData[j] = i + j;  /* 基于用户序号生成数据 */
        }

        /* 设置发送下行2数据 */
        uint8_t txUserIndex = i;  /* 从第一个用户开始，依次增加 */
        ret = TK8710SetTxData(TK8710_DOWNLINK_B, txUserIndex, txData, 26, 35, TK8710_DATA_TYPE_DED);  /* 使用连续索引，功率35 */
        if (ret != TK8710_OK) {
            printf("Failed to set TX user data for user[%d]: %d\n", txUserIndex, ret);
            return ret;
        }

        ret = TK8710GetRxUserInfo(userIndex, &freq, ahData, &pilotPower);
        if (ret != TK8710_OK) {
            printf("Failed to get RX user info for user[%d]: %d\n", userIndex, ret);
            /* 如果获取失败，使用默认值 */
            freq = 20000;  /* 默认频率 */
            for (int j = 0; j < 16; j++) {
                ahData[j] = 8192 + j;  /* 默认AH值 */
            }
            pilotPower = 1000000;  /* 默认Pilot功率 */
            printf("Using default values for user[%d]\n", userIndex);
        }

        /* 设置发送用用户信息 - 使用从0开始的连续索引 */
        ret = TK8710SetTxUserInfo(txUserIndex, freq, ahData, pilotPower);
        if (ret != TK8710_OK) {
            printf("Failed to set TX user info for user[%d]: %d\n", txUserIndex, ret);
            return ret;
        }
    }

    printf("All %d users manual TX info set successfully\n", userCount);
    return TK8710_OK;
}

/*============================================================================
 * Driver回调函数实现
============================================================================*/

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

            /* 检查是否需要执行采集数据 */
            if (g_captureDataPending && g_captureDataPendingNum == 2) {
                printf("执行采集数据功能...\n");
                int captureRet = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, TK8710_DBG_OPT_GET, NULL, NULL);
                if (captureRet == TK8710_OK) {
                    printf("采集数据功能执行成功\n");
                } else {
                    printf("采集数据功能执行失败: ret=%d\n", captureRet);
                }
                g_captureDataPending = 0;  /* 清除待执行标志 */
                g_captureDataPendingNum = 0;
            }
            g_captureDataPendingNum++;
            /* 丢包统计逻辑 */
            g_packetCount++;
            g_totalPacketCount++;

            /* 如果有CRC正确的用户，获取用户信号质量信息 */
            if (irqResult->crcValidCount > 0) {
                printf("=== 用户信号质量信息 ===\n");
                uint8_t validUsers[128];
                uint8_t validUserCount = 0;
                uint32_t expectedUserCount = GetExpectedUserCount(g_testMode);

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
                            if (userIndex < 128) {
                                printf("用户%u: freq=%dHz (raw=0x%08X), rssi=%d, snr=%u\n",
                                       userIndex, freqValue/128, freq26, rssiValue, snrValue);
                            }
                            /* 收集有效用户信息用于批量发送 */
                            validUsers[validUserCount] = userIndex;
                            validUserCount++;
                            g_lastValidUserIndex = userIndex;
                        } else {
                            printf("用户%u: 获取信号质量失败\n", userIndex);
                        }
                        // TK8710ReleaseRxData(userIndex);
                    }
                }
                /* 一次性设置所有有效用户的发送数据 */
                if (validUserCount > 0) {
                    // test_auto_downlink_tx_all(validUsers, validPowers, validUserCount);
                    test_manual_tx_setup(validUsers, validUserCount);
                }
                /* 如果接收用户数小于期望用户数，计为丢包 */
                if (validUserCount < expectedUserCount) {
                    g_packetLostCount = g_packetLostCount + (expectedUserCount - validUserCount);
                    printf("帧号：(%d), 丢包: 接收用户数(%u) < 期望用户数(%u)\n", g_packetCount,validUserCount, expectedUserCount);
                }

                printf("========================\n");
            } else {
                /* 没有CRC正确的用户，计为丢包 */
                uint32_t expectedUserCount = GetExpectedUserCount(g_testMode);
                g_packetLostCount = g_packetLostCount + expectedUserCount;
                printf("帧号：(%d), 丢包: 接收用户数(0) < 期望用户数(%u)\n", g_packetCount,expectedUserCount);
            }

            /* 每100包重新开始统计 */
            if (g_packetCount >= 100) {
                uint32_t expectedUserCount = GetExpectedUserCount(g_testMode);
                printf("\n=== 丢包统计 (100包) ===\n");
                printf("测试模式: %d, 期望用户数: %u\n", g_testMode, expectedUserCount);
                printf("接收包数: %u\n", g_packetCount);
                printf("丢包数: %u\n", g_packetLostCount);
                printf("总丢包率: %.2f%% (基于期望用户总数)\n", (float)g_packetLostCount * 1.0f / (g_totalPacketCount * expectedUserCount));
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
        size_t byteIndex = 0;
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
            if (byteIndex < sizeof(spiDataBuffer)) {
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

        printf("TxPower 数据打包完成: %d用户, 每用户8bit, 总字节数: %zu\n", userCount, byteIndex);

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
 * @brief 根据测试模式获取期望的发送用户数
 * @param mode 测试模式
 * @return 期望的用户数
 */
static uint32_t GetExpectedUserCount(int mode) {
    switch (mode) {
        case 5:
        case 6:
        case 7:
        case 8:
            return 128;
        case 9:
            return 64;
        case 10:
            return 32;
        case 11:
        case 18:
            return 16;
        default:
            return 1;  // 默认值
    }
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
    printf("  x/X - Start TK8710 chip (Master mode, continuous)\n");
    printf("  l/L - Load and send simulation data from files\n");
    printf("  d/D - Set capture data pending (execute once on next MD_DATA interrupt)\n");
    printf("  f/F - Get ACM calibration factors and save to file\n");
    printf("  n/N - Get ACM SNR values\n");
    printf("  g/G - Execute ACM auto gain acquisition\n");
    printf("  k/K - Execute ACM calibration\n");
    printf("  1   - Configure single antenna receive mode (0xc02c = 0x00010101)\n");
    printf("  2   - Configure multi-antenna receive mode (0xc02c = 0x0001ffff)\n");
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
    printf("Work mode: %s\n", workType == TK8710_MODE_MASTER ? "Master" : "Slave");
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
    int caseNum = 17;  /* 默认case序号 */
    int s1ByteLen = 22;  /* 默认s1 byteLen */
    int s2ByteLen = 22;  /* 默认s2 byteLen */
    int s3ByteLen = 22;  /* 默认s3 byteLen */
    uint32_t testFreq = 503100000;
    uint32_t Rxgain = 0x7e;
    /* 设置全局测试模式 */
    g_testMode = testMode;

    /* 检查命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [mode] [class] [case] [s1ByteLen] [s2ByteLen] [s3ByteLen] [--help|-h]\n", argv[0]);
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
            printf("  s1ByteLen: Slot1 byte length, default: 22\n");
            printf("  s2ByteLen: Slot2 byte length, default: 22\n");
            printf("  s3ByteLen: Slot3 byte length, default: 22\n");
            printf("  --help, -h: Show this help\n");
            printf("\nExamples:\n");
            printf("  %s 6 3 11           # Use mode 6, class 3, case 11, default byteLen\n", argv[0]);
            printf("  %s 6 3 11 24 24 24  # Use mode 6, class 3, case 11, s1=24, s2=24, s3=24\n", argv[0]);
            printf("  %s 6 1 17 20 30 25  # Use mode 6, class 1, case 17, s1=20, s2=30, s3=25\n", argv[0]);
            return 0;
        }

        testMode = atoi(argv[1]);
        if (testMode < 5 || testMode > 18 || (testMode > 11 && testMode < 18)) {
            printf("Error: Invalid mode %d. Supported modes: 5,6,7,8,9,10,11,18\n", testMode);
            return 1;
        }
        /* 更新全局测试模式 */
        g_testMode = testMode;

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

        /* 解析s1ByteLen参数 */
        if (argc > 4) {
            s1ByteLen = atoi(argv[4]);
            if (s1ByteLen <= 0 || s1ByteLen > 255) {
                printf("Error: Invalid s1ByteLen %d. Must be positive integer <= 255\n", s1ByteLen);
                return 1;
            }
        }

        /* 解析s2ByteLen参数 */
        if (argc > 5) {
            s2ByteLen = atoi(argv[5]);
            if (s2ByteLen <= 0 || s2ByteLen > 255) {
                printf("Error: Invalid s2ByteLen %d. Must be positive integer <= 255\n", s2ByteLen);
                return 1;
            }
        }

        /* 解析s3ByteLen参数 */
        if (argc > 6) {
            s3ByteLen = atoi(argv[6]);
            if (s3ByteLen <= 0 || s3ByteLen > 255) {
                printf("Error: Invalid s3ByteLen %d. Must be positive integer <= 255\n", s3ByteLen);
                return 1;
            }
        }

        /* 解析工作频率参数 */
        if (argc > 7) {
            testFreq = atoi(argv[7]);
        }
        Rxgain = atoi(argv[8]);
        printf("Using test mode: %d, class: %d, case: %d, s1ByteLen: %d, s2ByteLen: %d, s3ByteLen: %d\n",
               testMode, classNum, caseNum, s1ByteLen, s2ByteLen, s3ByteLen);
    } else {
        printf("Using default: mode %d, class %d, case %d, s1ByteLen: %d, s2ByteLen: %d, s3ByteLen: %d\n",
               testMode, classNum, caseNum, s1ByteLen, s2ByteLen, s3ByteLen);
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
    static ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,//
        .Freq = 509100000,
        .rxgain = 0x7e,
        .txgain = 0x2e,
        // .txadc = {//C号板
        //     {0x0bc0, 0x04a0}, {0x0a50, 0x0780}, {0x0750, 0x0820}, {0x0bc3, 0x0940},
        //     {0x0e83, 0x05e0}, {0xfbff, 0x0850}, {0x0880, 0x0500}, {0x02a0, 0x06ff}
        // }
        // .txadc = {//D号板
        //     {0x0450, 0x0450}, {0x0a00, 0x1080}, {0x0750, 0x1500}, {0x0400, 0x0b00},
        //     {0x08a0, 0x07a0}, {0x0990, 0xff00}, {0x0850, 0x08c8}, {0x0950, 0x0a00}
        // }
        // .txadc = {//2号板 Master (默认值，将被文件配置覆盖)
        //     {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
        //     {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        // }
        // .txadc = {//3号板 Master (默认值，将被文件配置覆盖)
        //     {0x0400, 0x0350}, {0x0400, 0x0450}, {0x0250, 0x0230}, {0x0400, 0x0290},
        //     {0x0390, 0x0500}, {0x0390, 0x0400}, {0x0300, 0x0350}, {0x0490, 0x04a0}
        // }
        // .txadc = {//703：板（slave）
        //     {0x0450, 0x04a0}, {0x0500, 0x0500}, {0x0490, 0x0350}, {0x0490, 0x0420},
        //     {0x0300, 0x0250}, {0x05c0, 0x0450}, {0x0200, 0x0250}, {0x0330, 0x0390}
        // }
        // .txadc = {//710：板（master）
        //     {0x0300, 0x0250}, {0x0450, 0x0590}, {0x0350, 0x0490}, {0x0450, 0x02c0},
        //     {0x02a0, 0x0390}, {0x01a0, 0x0220}, {0x0240, 0x0250}, {0x0250, 0x0680}
        // }
    };
    rfConfig.Freq = testFreq;
    rfConfig.rxgain = Rxgain;
    /* 尝试从TxDC目录加载txadc配置 */
    if (LoadTxadcConfig("txadc.txt", (uint16_t (*)[2])rfConfig.txadc) != 0) {
        printf("使用默认txadc配置\n");
    } else {
        printf("已加载文件中的txadc配置\n");
    }

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
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig  /* RF配置在TK8710Init中自动调用 */
    };

    /* 4. 调用 8710 init 完成芯片、RF初始化 */
    printf("Initializing 8710 (chip + RF)...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("8710 initialization failed: %d\n", ret);
        return -1;
    }

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

    printf("HAL initialization completed (including RF)\n");

    /* 6. 配置时隙参数 - 使用 TK8710HalCfg */
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));

    /* 配置基本参数 (与原配置一致) */
    slotCfg.msMode = TK8710_MODE_MASTER;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 1;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
    slotCfg.txBcnAntEn = 0x7f;//0x7f
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
            case TK8710_RATE_MODE_5:
                slotCfg.s1Cfg[0].da_m = 21492;    /* da1_m */
                slotCfg.s2Cfg[0].da_m = 21492;    /* da2_m */
                slotCfg.s3Cfg[0].da_m = 21492;    /* da3_m */
                break;
            case TK8710_RATE_MODE_6:
                slotCfg.s1Cfg[0].da_m = 19728;    /* da1_m */
                slotCfg.s2Cfg[0].da_m = 19728;    /* da2_m */
                slotCfg.s3Cfg[0].da_m = 19728;    /* da3_m */
                break;
            case TK8710_RATE_MODE_7:
                slotCfg.s1Cfg[0].da_m = 12000;    /* da1_m */
                slotCfg.s2Cfg[0].da_m = 12000;    /* da2_m */
                slotCfg.s3Cfg[0].da_m = 12000;    /* da3_m */
                break;
            case TK8710_RATE_MODE_8:
                slotCfg.s1Cfg[0].da_m = 5600;    /* da1_m */
                slotCfg.s2Cfg[0].da_m = 5600;    /* da2_m */
                slotCfg.s3Cfg[0].da_m = 5600;    /* da3_m */
                break;
            case TK8710_RATE_MODE_9:
                slotCfg.s1Cfg[0].da_m = 2800;     /* da1_m */
                slotCfg.s2Cfg[0].da_m = 2800;     /* da2_m */
                slotCfg.s3Cfg[0].da_m = 2800;     /* da3_m */
                break;
            case TK8710_RATE_MODE_10:
                slotCfg.s1Cfg[0].da_m = 1400;     /* da1_m */
                slotCfg.s2Cfg[0].da_m = 1400;     /* da2_m */
                slotCfg.s3Cfg[0].da_m = 1400;     /* da3_m */
                break;
            case TK8710_RATE_MODE_11:
            case TK8710_RATE_MODE_18:
                slotCfg.s1Cfg[0].da_m = 800;      /* da1_m */
                slotCfg.s2Cfg[0].da_m = 800;      /* da2_m */
                slotCfg.s3Cfg[0].da_m = 800;      /* da3_m */
                break;
            default:
                /* 默认值，保持原有设置 */
                slotCfg.s1Cfg[0].da_m = 12000;    /* da1_m */
                slotCfg.s2Cfg[0].da_m = 12000;    /* da2_m */
                slotCfg.s3Cfg[0].da_m = 12000;    /* da3_m */
                break;
    }
    slotCfg.s0Cfg[0].byteLen = 0;
    slotCfg.s0Cfg[0].centerFreq = testFreq;
    slotCfg.s1Cfg[0].byteLen = s1ByteLen;
    slotCfg.s1Cfg[0].centerFreq = testFreq;
    slotCfg.s2Cfg[0].byteLen = s2ByteLen;
    slotCfg.s2Cfg[0].centerFreq = testFreq;
    slotCfg.s3Cfg[0].byteLen = s3ByteLen;
    slotCfg.s3Cfg[0].centerFreq = testFreq;

    /* 调用 8710 config 配置时隙 */
    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slotCfg);
    if (ret != TK8710_OK) {
        return -1;
    }
    printf("Slot parameter configuration completed\n");

    // /* 自动加载并发送仿真数据 */
    // printf("\n自动加载仿真数据...\n");
    // if (load_and_send_simulation_data(classNum, caseNum) != 0) {
    //     printf("警告: 仿真数据加载失败，程序将继续运行\n");
    // }

    /* 7. 启动TK8710 */
    // 启动TK8710芯片，使用Master模式和连续工作模式
    // ret = TK8710Start(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    // if (ret != TK8710_OK) {
    //     return TK8710_HAL_ERROR_START;
    // }

    printf("\nSystem initialization completed, starting runtime...\n");
    printf("Enter 'h' for help information\n\n");

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x980c, 0x000FF200);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x11100010);
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

            case 'r':
            case 'R':
                read_register();
                break;

            case 'w':
            case 'W':
                write_register();
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

            case 'x':
            case 'X':
                printf("启动TK8710芯片...\n");
                ret = TK8710Start(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
                if (ret == TK8710_OK) {
                    printf("TK8710芯片启动成功 (Master模式, 连续工作)\n");
                } else {
                    printf("TK8710芯片启动失败: ret=%d\n", ret);
                }
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

            case '1':
                printf("配置为单天线接收模式...\n");
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc02c, 0x00010101);
                if (ret == TK8710_OK) {
                    printf("单天线接收模式配置成功 (0xc02c = 0x00010101)\n");
                } else {
                    printf("单天线接收模式配置失败: ret=%d\n", ret);
                }
                break;

            case '2':
                printf("配置为多天线接收模式...\n");
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc02c, 0x0001ffff);
                if (ret == TK8710_OK) {
                    printf("多天线接收模式配置成功 (0xc02c = 0x0001ffff)\n");
                } else {
                    printf("多天线接收模式配置失败: ret=%d\n", ret);
                }
                break;

            case 'd':
            case 'D':
                printf("采集数据功能已设置，将在下次MD_DATA中断时执行\n");
                s_ram_rd0 ramRd0;
                ramRd0.data = 0;
                ramRd0.b.cap_en = 1;  // 启用采集
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
                if (ret != TK8710_OK) {
                    TK8710_LOG_CONFIG_ERROR("配置s_ram_rd0寄存器失败: ret=%d\n", ret);
                    return ret;
                }
                g_captureDataPending = 1;
                g_captureDataPendingNum = 0;
                break;

            case 'f':
            case 'F':
                printf("获取ACM校准因子...\n");

                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CAL_FACTOR, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM校准因子获取完成\n");
                } else {
                    printf("ACM校准因子获取失败: ret=%d\n", ret);
                }
                break;

            case 'n':
            case 'N':
                printf("获取ACM SNR值...\n");
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_SNR, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM SNR值获取完成\n");
                } else {
                    printf("ACM SNR值获取失败: ret=%d\n", ret);
                }
                break;

            case 'g':
            case 'G':
            {
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM增益自动获取完成\n");
                } else {
                    printf("ACM增益自动获取失败: ret=%d\n", ret);
                }
                break;
            }

            case 'k':
            case 'K':
            {
                uint32_t calibration_rxgain;
                uint32_t calibration_txgain;
                const TxAdcConfig* calibration_txadc;
                const TxAdcConfig* default_txadc;

                printf("请输入1255 rxgain (hex, e.g. 0x7e): ");
                fflush(stdout);
                if (scanf("%x", &calibration_rxgain) != 1 || calibration_rxgain > 0xFF) {
                    printf("无效的1255 rxgain，范围: 0x00~0xFF\n");
                    break;
                }

                printf("请输入1255 txgain (0x2a或0x2e): ");
                fflush(stdout);
                if (scanf("%x", &calibration_txgain) != 1 ||
                    (calibration_txgain != 0x2a && calibration_txgain != 0x2e)) {
                    printf("无效的1255 txgain，仅支持0x2a或0x2e\n");
                    break;
                }

                calibration_txadc = GetTxadcConfigByGain((uint8_t)calibration_txgain);
                default_txadc = GetTxadcConfigByGain(rfConfig.txgain);

                ret = tk8710_rf_write(0xff, RF_CMD_RX_GAIN >> 8, calibration_rxgain);
                if (ret != TK8710_OK) {
                    printf("校准时1255 rxgain设置失败: ret=%d\n", ret);
                    break;
                }
                ret = tk8710_rf_write(0xff, RF_CMD_TX_GAIN >> 8, calibration_txgain);
                if (ret != TK8710_OK) {
                    printf("校准时1255 txgain设置失败: ret=%d\n", ret);
                    goto restore_calibration_rf_config;
                }
                ret = ApplyTxadcConfig(calibration_txadc);
                if (ret != TK8710_OK) {
                    goto restore_calibration_rf_config;
                }

                printf("执行ACM增益自动获取, rxgain=0x%02X, txgain=0x%02X...\n",
                       (uint8_t)calibration_rxgain, (uint8_t)calibration_txgain);
                AcmCalibParams calibParams;
                calibParams.calibCount = 100;
                calibParams.snrThreshold = 5;//20
                int calibRet;
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, TK8710_DBG_OPT_GET, NULL, NULL);
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CALIBRATE, TK8710_DBG_OPT_EXE,
                                    &calibParams, &calibRet);
                if (ret == TK8710_OK) {
                    printf("ACM校准完成\n");
                } else {
                    printf("ACM校准失败: ret=%d\n", ret);
                }

restore_calibration_rf_config:
                ret = tk8710_rf_write(0xff, RF_CMD_RX_GAIN >> 8, rfConfig.rxgain);
                if (ret != TK8710_OK) {
                    printf("校准后1255 rxgain恢复失败: ret=%d\n", ret);
                }
                ret = tk8710_rf_write(0xff, RF_CMD_TX_GAIN >> 8, rfConfig.txgain);
                if (ret != TK8710_OK) {
                    printf("校准后1255 txgain恢复失败: ret=%d\n", ret);
                }
                ret = ApplyTxadcConfig(default_txadc != NULL ? default_txadc : rfConfig.txadc);
                if (ret == TK8710_OK) {
                    printf("校准后1255默认增益和直流分量恢复完成\n");
                } else {
                    printf("校准后1255直流分量恢复失败: ret=%d\n", ret);
                }
                s_init_9 init9;
                init9.data = 0x1ffff;
                ret = TK8710SpiWriteReg(MAC_BASE + offsetof(struct mac, init_9), &init9.data, 1);
                if (ret != 0) return TK8710_ERR;
                break;
            }

            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
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

    TK8710GpioIrqEnable(0, 0);

    TK8710Rk3506Cleanup();

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


