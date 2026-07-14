#if defined(PLATFORM_TMS570)
/* PC/RK test source is intentionally empty for the TMS570 target build. */
#else

/**
 * @file test8710main.c
 * @brief TK8710 主测试程�?
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
#include "driver/tk8710_regs.h"

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <sched.h>
#include <sys/time.h>

#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <locale.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

/*============================================================================
 * 全局变量和配�?
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

 int set_cpu_affinity(int cpu_core) {
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


/* 从TxDC目录读取txadc配置的函�?*/
int LoadTxadcConfig(const char* filename, uint16_t txadc[8][2]) {
    FILE* file;
    char filepath[256];
    char line[256];
    int index = 0;
    
    /* 构建文件路径 */
    snprintf(filepath, sizeof(filepath), "TxDC/%s", filename);
    
    file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Warning: ?????? %s???????\n", filepath);
        return -1;
    }
    
    printf("????%s ??txadc??...\n", filepath);
    
    /* 逐行读取文件 */
    while (fgets(line, sizeof(line), file) != NULL && index < 8) {
        /* 跳过注释行和空行 */
        if (line[0] == '/' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        /* 解析两个十六进制数�?*/
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
        printf("????8?txadc??\n");
        return 0;
    } else {
        printf("??: ????%d?txadc???????\n", index);
        return -1;
    }
}

/* 下行发送状态跟�?*/
static volatile bool g_hasValidUsers = false;     /* 是否有有效用�?*/
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

/* TRM相关变量 */
static uint32_t g_trmSendCount = 0;               /* TRM发送计�?*/
static uint32_t g_trmRxCount = 0;                 /* TRM接收计数 */
static uint32_t __attribute__((unused)) g_irqCount = 0;                  /* 中断计数 */

/* 丢包统计变量 */
static volatile uint32_t g_packetCount = 0;               /* 当前统计周期内的包计�?*/
static volatile uint32_t g_packetLostCount = 0;           /* 当前统计周期内的丢包计数 */
static volatile uint32_t g_totalPacketCount = 0;         /* 总包计数 */
static volatile uint32_t g_lastValidUserIndex = 0;       /* 上次有效用户索引 */
static volatile uint32_t g_simulationDataLoaded = 0;
static int g_testMode = 6;  /* 全局测试模式 */

/* 采集数据控制变量 */
static volatile uint8_t g_captureDataPending = 0;         /* 采集数据待执行标�?*/
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
 * @brief 测试指定信息发送功�?
 * @param userIndices 用户索引数组
 * @param userCount 用户数量
 * @return 0-成功, 其他-失败
 */
static int test_manual_tx_setup(const uint8_t* userIndices, uint8_t userCount)
{
    int ret;
    
    printf("Setting up manual TX for %d users\n", userCount);
    
    /* 为每个用户设置发送信�?*/
    for (int i = 0; i < userCount; i++) {
        uint8_t userIndex = userIndices[i];
        
        /* 从接收的用户信息Buffer获取数据 */
        uint32_t freq;
        uint32_t ahData[16];
        uint64_t pilotPower;
        
        /* 为每个用户生成发送数�?*/
        uint8_t txData[26];
        for (int j = 0; j < 26; j++) {
            txData[j] = i + j;  /* 基于用户序号生成数据 */
        }
        
        /* 设置发送下�?数据 */
        uint8_t txUserIndex = i;  /* 从第一个用户开始，依次增加 */
        ret = TK8710SetTxData(TK8710_DOWNLINK_B, txUserIndex, txData, 26, 35, TK8710_DATA_TYPE_DED);  /* 使用连续索引，功�?5 */
        if (ret != TK8710_OK) {
            printf("Failed to set TX user data for user[%d]: %d\n", txUserIndex, ret);
            return ret;
        }
        
        ret = TK8710GetRxUserInfo(userIndex, &freq, ahData, &pilotPower);
        if (ret != TK8710_OK) {
            printf("Failed to get RX user info for user[%d]: %d\n", userIndex, ret);
            /* 如果获取失败，使用默认�?*/
            freq = 20000;  /* 默认频率 */
            for (int j = 0; j < 16; j++) {
                ahData[j] = 8192 + j;  /* 默认AH�?*/
            }
            pilotPower = 1000000;  /* 默认Pilot功率 */
            printf("Using default values for user[%d]\n", userIndex);
        }
        
        /* 设置发送用用户信息 - 使用�?开始的连续索引 */
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
    printf("????: %d\n", irqResult->irq_type);
    
    switch (irqResult->irq_type) {
        case TK8710_IRQ_RX_BCN:
            printf("BCN??: bits=%u, freq_offset=%d, status=%u\n", 
                   irqResult->rx_bcnbits, irqResult->bcn_freq_offset, irqResult->rxbcn_status);
            break;
            
        case TK8710_IRQ_BRD_DATA:
            printf("BRD????: ??=%d, CRC??=%d, CRC??=%d\n", 
                   irqResult->brdDataValid, irqResult->crcValidCount, irqResult->crcErrorCount);
            break;
            
        case TK8710_IRQ_MD_DATA:
            printf("MD????: ??=%d, CRC??=%d, CRC??=%d\n", 
                   irqResult->mdDataValid, irqResult->crcValidCount, irqResult->crcErrorCount);
            
            /* 检查是否需要执行采集数�?*/
            if (g_captureDataPending && g_captureDataPendingNum == 2) {
                printf("????????...\n");
                int captureRet = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, TK8710_DBG_OPT_GET, NULL, NULL);
                if (captureRet == TK8710_OK) {
                    printf("??????????\n");
                } else {
                    printf("??????????: ret=%d\n", captureRet);
                }
                g_captureDataPending = 0;  /* 清除待执行标�?*/
                g_captureDataPendingNum = 0;
            }
            g_captureDataPendingNum++;
            /* 丢包统计逻辑 */
            g_packetCount++;
            g_totalPacketCount++;
            
            /* 如果有CRC正确的用户，获取用户信号质量信息 */
            if (irqResult->crcValidCount > 0) {
                printf("=== ???????? ===\n");
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
                            /* SNR转换：uint8_t最�?55，直接除�? */
                            uint8_t snrValue = snr / 4;
                            
                            /* RSSI转换�?1位有符号数，需要转换为有符号�?*/
                            uint32_t rssiRaw = rssi;
                            int16_t rssiValue = (int16_t)(rssiRaw - 2048) / 4;
                            
                            /* 频率转换�?6-bit格式转换为实际频率Hz */
                            uint32_t freq26 = freqSignal & 0x03FFFFFF;  /* �?6�?*/
                            int32_t freqValue = freq26 > (1<<25) ? (int32_t)(freq26 - (1<<26)) : (int32_t)freq26;
                            
                            /* 只打印前10个用户的信息 */
                            if (userIndex < 128) {
                                printf("??%u: freq=%dHz (raw=0x%08X), rssi=%d, snr=%u\n", 
                                       userIndex, freqValue/128, freq26, rssiValue, snrValue);
                            }
                            /* 收集有效用户信息用于批量发�?*/
                            validUsers[validUserCount] = userIndex;
                            validUserCount++;
                            g_lastValidUserIndex = userIndex;
                        } else {
                            printf("??%u: ????????\n", userIndex);
                        }
                        // TK8710ReleaseRxData(userIndex);
                    }
                }
                /* 一次性设置所有有效用户的发送数�?*/
                if (validUserCount > 0) {
                    // test_auto_downlink_tx_all(validUsers, validPowers, validUserCount);
                    test_manual_tx_setup(validUsers, validUserCount);
                }
                /* 如果接收用户数小于期望用户数，计为丢�?*/
                if (validUserCount < expectedUserCount) {
                    g_packetLostCount = g_packetLostCount + (expectedUserCount - validUserCount);
                    printf("????%d), ??: ??????%u) < ??????%u)\n", g_packetCount,validUserCount, expectedUserCount);
                }
                
                printf("========================\n");
            } else {
                /* 没有CRC正确的用户，计为丢包 */
                uint32_t expectedUserCount = GetExpectedUserCount(g_testMode);
                g_packetLostCount = g_packetLostCount + expectedUserCount;
                printf("????%d), ??: ??????0) < ??????%u)\n", g_packetCount,expectedUserCount);
            }

            /* �?00包重新开始统�?*/
            if (g_packetCount >= 100) {
                uint32_t expectedUserCount = GetExpectedUserCount(g_testMode);
                printf("\n=== ???? (100?? ===\n");
                printf("????: %d, ?????? %u\n", g_testMode, expectedUserCount);
                printf("????: %u\n", g_packetCount);
                printf("???? %u\n", g_packetLostCount);
                printf("????: %.2f%% (????????)\n", (float)g_packetLostCount * 1.0f / (g_totalPacketCount * expectedUserCount));
                printf("========================\n\n");
                
                /* 重置当前统计周期 */
                g_packetCount = 0;
                g_packetLostCount = 0;
            }
            break;
            
        default:
            printf("??????: %d\n", irqResult->irq_type);
            break;
    }
    
    printf("============================\n");
}

/**
 * @brief Driver发送时隙回�?
 * @param irqResult 中断结果
 */
static void OnDriverTxSlot(TK8710IrqResult* irqResult)
{
    if (!irqResult) return;
    
    printf("=== Driver TX Slot Callback ===\n");
    printf("????: %d\n", irqResult->irq_type);
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
    printf("????: %d\n", irqResult->irq_type);
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
    printf("????: %d\n", irqResult->irq_type);
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
    printf("=== TRM?????? (??=%u) ===\n", rxDataList->frameNo);
    printf("??: ????%d\n", 
           rxDataList->userCount);
    
    /* 打印第一个用户的速率模式信息（如果有用户数据�?*/
    if (rxDataList->userCount > 0 && rxDataList->users) {
        TRM_RxUserData* firstUser = &rxDataList->users[0];
        printf("???????? ID=0x%08X, ????=%d, ????=%u\n", 
               firstUser->userId, firstUser->rateMode, firstUser->dataLen);
    }
    
    g_trmRxCount += rxDataList->userCount;
    
    // for (uint8_t i = 0; i < rxDataList->userCount; i++) {
    //     TRM_RxUserData* user = &rxDataList->users[i];
    //     printf("  ??[%d]: ID=0x%08X, ??=%d, RSSI=%d, SNR=%d, Freq=%d Hz\n", 
    //            i, user->userId, user->dataLen, user->rssi, user->snr, user->freq/128);
        
    //     /* 显示数据内容 */
    //     if (user->data != NULL && user->dataLen > 0) {
    //         printf("    ??: ");
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
    //     printf("  ???????? ????%d\n", ret);
    // }
    // /* 显示验证统计信息 */
    // TRM_TxValidatorStats stats;
    // if (TRM_TxValidatorGetStats(&stats) == TRM_OK) {
    //     printf("  ????? ????%u, ??=%u, ??=%u, ??????%u\n", 
    //            stats.totalTriggerCount, stats.successSendCount, stats.failedSendCount, g_trmRxCount);
    // }
    
    printf("==================\n");
}


/**
 * @brief TRM发送完成回�?
 * @param txResult 发送完成结�?
 */
static void OnTrmTxComplete(const TRM_TxCompleteResult* txResult)
{
    if (!txResult) return;
    
    printf("=== TRM???????===\n");
    printf("??????: %u, ????: %u\n", txResult->totalUsers, txResult->remainingQueue);
    g_trmSendCount += txResult->userCount;
    /* 打印每个用户的发送结�?*/
    // const char* resultStr[] = {"OK", "NO_BEAM", "TIMEOUT", "ERROR"};
    // for (uint32_t i = 0; i < txResult->userCount; i++) {
    //     const TRM_TxUserResult* userResult = &txResult->users[i];
    //     printf("  ??ID: 0x%08X, ??: %s\n", userResult->userId, resultStr[userResult->result]);
    // }
    printf("==================\n");
}

/**
 * @brief 显示TRM统计信息
 */
void show_trm_statistics(void)
{
    printf("\n=== TRM???? ===\n");
    printf("????? %u\n", g_trmSendCount);
    printf("????: %u\n", g_trmRxCount);
    
    /* 获取TRM内部统计 */
    TRM_Stats stats;
    if (TRM_GetStats(&stats) == TRM_OK) {
        printf("TRM????:\n");
        printf("  ????? %u\n", stats.txCount);
        printf("  ????? %u\n", stats.txSuccessCount);
        printf("  ????: %u\n", stats.rxCount);
        printf("  ????: %u\n", stats.beamCount);
        printf("????: %u\n", TRM_GetCurrentFrame());
    }
    
    printf("==================\n\n");
}

/*============================================================================
 * 主程�?
 *============================================================================*/

/**
 * @brief 从文件读取数据并通过SPI传输
 * @param classNum class序号 (�? 1, 3, �?
 * @param caseNum case序号 (�? 11, 17, �?
 * @return 0-成功, �?-失败
 */
int load_and_send_simulation_data(int classNum, int caseNum)
{
    char filePath[256];
    FILE *fp;
    char lineBuffer[16384];  /* 增加缓冲区大小以容纳2048�?0bit�?(2048×8=16384字符) */
    uint8_t spiDataBuffer[5120];  /* 增加缓冲区大小以容纳128用户×16AH×20bit=5120字节 */
    int dataCount;
    
    printf("\n=== ???????/class%d/case%d???????? ===\n", classNum, caseNum);
    
    /* 1. 读取 ANoise 数据 */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/ANoise.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("??????: %s\n", filePath);
        return -1;
    }
    
    if (fgets(lineBuffer, sizeof(lineBuffer), fp) != NULL) {
        /* 解析 ANoise 数据 (8个�? */
        uint16_t anoiseData[8];
        dataCount = sscanf(lineBuffer, "%hu %hu %hu %hu %hu %hu %hu %hu",
                          &anoiseData[0], &anoiseData[1], &anoiseData[2], &anoiseData[3],
                          &anoiseData[4], &anoiseData[5], &anoiseData[6], &anoiseData[7]);
        
        if (dataCount == 8) {
            /* 转换为字节数组并发�?*/
            for (int i = 0; i < 8; i++) {
                spiDataBuffer[i*2] = (uint8_t)(anoiseData[i] >> 8);
                spiDataBuffer[i*2+1] = (uint8_t)(anoiseData[i] & 0xFF);
            }
            
            /* 打印�?6个输入数�?(十六进制格式) */
            printf("ANoise SPI???? (??6??): ");
            for (int i = 0; i < 16; i++) {
                printf("0x%02X ", spiDataBuffer[i]);
            }
            printf("\n");
            
            int ret = TK8710SpiSetInfo(TK8710_SET_INFO_ANOISE, spiDataBuffer, 16);
            if (ret == 0) {
                printf("ANoise ??????: 8??\n");
            } else {
                printf("ANoise ??????: %d\n", ret);
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);
    
    /* 2. 读取 GWRXAH 数据 (按用户格式处�? 128个用户，每个用户16�?0bit AH) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/GWRXAH.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("??????: %s\n", filePath);
        return -1;
    }
    
    uint32_t gwrxahData[2048];  // 128用户 * 16AH = 2048�?0bit�?
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
    
    printf("GWRXAH ????%d ??0bit???\n", dataCount);
    
    if (dataCount > 0) {
        /* �?0bit数据按用户格式打包到字节缓冲�?
         * 每个用户16�?0bit AH = 320bit = 40字节
         * 128个用户总共需�?128 * 40 = 5120字节
         */
        int userCount = dataCount / 16;  // 计算实际用户�?
        if (userCount > 128) userCount = 128;
        
        printf("?? %d ????GWRXAH??\n", userCount);
        
        /* 按用户顺序打�?0bit数据到字节数�?
         * 使用位操作实现紧凑的20bit数据打包
         */
        int byteIndex = 0;
        uint32_t bitBuffer = 0;
        int bitsInBuffer = 0;
        
        for (int user = 0; user < userCount; user++) {
            for (int ah = 0; ah < 16; ah++) {
                uint32_t ahValue = gwrxahData[user * 16 + ah] & 0xFFFFF;  // 确保只取20bit
                
                /* �?0bit数据添加到位缓冲�?*/
                bitBuffer = (bitBuffer << 20) | ahValue;
                bitsInBuffer += 20;
                
                /* 当缓冲区�?位或更多时，提取字节 */
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
        
        /* 打印�?6个输入数�?(十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("GWRXAH SPI???? (??d??): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("GWRXAH ??????: %d??, ????6?AH, ????: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_AH, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("GWRXAH ??????: %d???? ????6?AH\n", userCount);
        } else {
            printf("GWRXAH ??????: %d\n", ret);
            return -1;
        }
    }
    
    /* 3. 读取 GWRxPilotPower 数据 (按用户格式处�? 128个用户，每个用户40bit PilotPower) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/GWRxPilotPower.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("??????: %s\n", filePath);
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
    
    printf("GWRxPilotPower ????%d ??0bit???\n", dataCount);
    
    if (dataCount > 0) {
        /* �?0bit数据按用户格式打包到字节缓冲�?
         * 每个用户40bit PilotPower = 5字节
         * 128个用户总共需�?128 * 5 = 640字节
         */
        int userCount = dataCount;  // 每个用户一�?0bit�?
        if (userCount > 128) userCount = 128;
        
        printf("?? %d ????GWRxPilotPower??\n", userCount);
        
        /* 按用户顺序打�?0bit数据到字节数�?
         * 40bit数据可以直接拆分�?个字�?
         */
        int byteIndex = 0;
        
        for (int user = 0; user < userCount; user++) {
            uint64_t pilotValue = pilotPowerData[user] & 0xFFFFFFFFFFULL;  // 确保只取40bit
            
            /* �?0bit数据拆分�?个字�?(大端�? */
            if ((size_t)(byteIndex + 4) < sizeof(spiDataBuffer)) {
                spiDataBuffer[byteIndex] = (uint8_t)((pilotValue >> 32) & 0xFF);    // 字节0 (最�?�?
                spiDataBuffer[byteIndex + 1] = (uint8_t)((pilotValue >> 24) & 0xFF); // 字节1
                spiDataBuffer[byteIndex + 2] = (uint8_t)((pilotValue >> 16) & 0xFF); // 字节2
                spiDataBuffer[byteIndex + 3] = (uint8_t)((pilotValue >> 8) & 0xFF);  // 字节3
                spiDataBuffer[byteIndex + 4] = (uint8_t)(pilotValue & 0xFF);         // 字节4 (最�?�?
                
                byteIndex += 5;
            }
        }
        
        /* 打印�?6个输入数�?(十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("GWRxPilotPower SPI???? (??d??): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("GWRxPilotPower ??????: %d??, ????0bit, ????: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_PILOT_POW, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("GWRxPilotPower ??????: %d???? ????0bit PilotPower\n", userCount);
        } else {
            printf("GWRxPilotPower ??????: %d\n", ret);
            return -1;
        }
    }
    
    /* 4. 读取 TxFreq 数据 (按用户格式处�? 128个用户，每个用户32bit TxFreq) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/TxFreq.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("??????: %s\n", filePath);
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
    
    printf("TxFreq ????%d ??2bit???\n", dataCount);
    
    if (dataCount > 0) {
        /* �?2bit数据按用户格式打包到字节缓冲�?
         * 每个用户32bit TxFreq = 4字节
         * 128个用户总共需�?128 * 4 = 512字节
         */
        int userCount = dataCount;  // 每个用户一�?2bit�?
        if (userCount > 128) userCount = 128;
        
        printf("?? %d ????TxFreq??\n", userCount);
        
        /* 按用户顺序打�?2bit数据到字节数�?
         * 32bit数据可以直接拆分�?个字�?
         */
        int byteIndex = 0;
        
        for (int user = 0; user < userCount; user++) {
            uint32_t freqValue = txFreqData[user];
            
            /* �?2bit数据拆分�?个字�?(大端�? */
            if ((size_t)(byteIndex + 3) < sizeof(spiDataBuffer)) {
                spiDataBuffer[byteIndex] = (uint8_t)((freqValue >> 24) & 0xFF);     // 字节0 (最�?�?
                spiDataBuffer[byteIndex + 1] = (uint8_t)((freqValue >> 16) & 0xFF); // 字节1
                spiDataBuffer[byteIndex + 2] = (uint8_t)((freqValue >> 8) & 0xFF);  // 字节2
                spiDataBuffer[byteIndex + 3] = (uint8_t)(freqValue & 0xFF);         // 字节3 (最�?�?
                
                byteIndex += 4;
            }
        }
        
        /* 打印�?6个输入数�?(十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("TxFreq SPI???? (??d??): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("TxFreq ??????: %d??, ????2bit, ????: %d\n", userCount, byteIndex);
        
        int ret = TK8710SpiSetInfo(TK8710_SET_INFO_TX_FREQ, spiDataBuffer, byteIndex);
        if (ret == 0) {
            printf("TxFreq ??????: %d???? ????2bit TxFreq\n", userCount);
        } else {
            printf("TxFreq ??????: %d\n", ret);
            return -1;
        }
    }
    
    /* 5. 读取 TxPower 数据 (按用户格式处�? 128个用户，每个用户8bit TxPower) */
    snprintf(filePath, sizeof(filePath), 
        "./class%d/case%d/TxPower.txt", classNum, caseNum);
    fp = fopen(filePath, "r");
    if (fp == NULL) {
        printf("??????: %s\n", filePath);
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
    
    printf("TxPower ????%d ??bit???\n", dataCount);
    
    if (dataCount > 0) {
        /* �?bit数据按用户格式打包到字节缓冲�?
         * 每个用户8bit TxPower = 1字节
         * 128个用户总共需�?128 * 1 = 128字节
         */
        int userCount = dataCount;  // 每个用户一�?bit�?
        if (userCount > 128) userCount = 128;
        
        printf("?? %d ????TxPower??\n", userCount);
        
        /* 按用户顺序直接复�?bit数据到字节数�?*/
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
                                    /* 发送广播数�?*/
               ret = TK8710WriteBuffer(user + 128, Data, Len);
            }
        }
        
        /* 打印�?6个输入数�?(十六进制格式) */
        int printLen = (byteIndex < 16) ? byteIndex : 16;
        printf("TxPower SPI???? (??d??): ", printLen);
        for (int i = 0; i < printLen; i++) {
            printf("0x%02X ", spiDataBuffer[i]);
        }
        printf("\n");
        
        printf("TxPower ??????: %d??, ????bit, ????: %d\n", userCount, byteIndex);

        if (ret == 0) {
            printf("TxPower ??????: %d???? ????bit TxPower\n", userCount);
        } else {
            printf("TxPower ??????: %d\n", ret);
            return -1;
        }
    }
    
    printf("=== ???????????===\n");
    
    /* 设置仿真数据加载标志，通知中断处理系统 */
    TK8710SetSimulationDataLoaded(1);
    printf("???????????\n\n");
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
            return 1;  // 默认�?
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
    
    /* 获取当前中断状�?*/
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
 * @brief 主函�?
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
    uint32_t testFreq = 509100000;
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
    
    /* ========== 使用 HAL API 进行初始�?========== */
    /* 1. 准备RF配置 */
    static ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,//
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
        // .txadc = {//2号板 Master (默认值，将被文件配置覆盖)
        //     {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
        //     {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        // }
        // .txadc = {//3号板 Master (默认值，将被文件配置覆盖)
        //     {0x0400, 0x0350}, {0x0400, 0x0450}, {0x0250, 0x0230}, {0x0400, 0x0290},
        //     {0x0390, 0x0500}, {0x0390, 0x0400}, {0x0300, 0x0350}, {0x0490, 0x04a0}
        // }
        .txadc = {//703：板（slave�?
            {0x0450, 0x04a0}, {0x0500, 0x0500}, {0x0490, 0x0350}, {0x0490, 0x0420},
            {0x0300, 0x0250}, {0x05c0, 0x0450}, {0x0200, 0x0250}, {0x0330, 0x0390}
        }
        // .txadc = {//710：板（master�?
        //     {0x0300, 0x0250}, {0x0450, 0x0590}, {0x0350, 0x0490}, {0x0450, 0x02c0},
        //     {0x02a0, 0x0390}, {0x01a0, 0x0220}, {0x0240, 0x0250}, {0x0250, 0x0680}
        // }
    };
    rfConfig.Freq = testFreq;
    /* 尝试从TxDC目录加载txadc配置 */
    if (LoadTxadcConfig("txadc.txt", (uint16_t (*)[2])rfConfig.txadc) != 0) {
        printf("????txadc??\n");
    } else {
        printf("???????txadc??\n");
    }
    
    /* 2. 准备芯片配置 (与原 init_tk8710_chip 配置一�? */
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
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig  /* RF配置在TK8710Init中自动调�?*/
    };
    
    /* 4. 调用 8710 init 完成芯片、RF初始�?*/
    printf("Initializing 8710 (chip + RF)...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("8710 initialization failed: %d\n", ret);
        return -1;
    }

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

    printf("HAL initialization completed (including RF)\n");
    
    /* 6. 配置时隙参数 - 使用 TK8710HalCfg */
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    
    /* 配置基本参数 (与原配置一�? */
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

    /* 配置BCN轮流发�?*/
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;
    }
    
    /* 单速率配置 */
    printf("Using single-rate configuration for mode %d\n", testMode);
    slotCfg.rateCount = 1;
    slotCfg.rateModes[0] = testMode;
    
    /* 根据模式设置不同的da_m�?*/
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
    
    // /* 自动加载并发送仿真数�?*/
    // printf("\n????????...\n");
    // if (load_and_send_simulation_data(classNum, caseNum) != 0) {
    //     printf("??: ????????????????\n");
    // }

    /* 7. 启动TK8710 */
    // 启动TK8710芯片，使用Master模式和连续工作模�?
    // ret = TK8710Start(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    // if (ret != TK8710_OK) {
    //     return TK8710_HAL_ERROR_START;
    // }    

    printf("\nSystem initialization completed, starting runtime...\n");
    printf("Enter 'h' for help information\n\n");
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x980c, 0x000FF200);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x11100010);
    /* 8. 主循�?- 等待中断并进行中断处�?*/
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
                
            case 'x':
            case 'X':
                printf("??TK8710??...\n");
                ret = TK8710Start(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
                if (ret == TK8710_OK) {
                    printf("TK8710?????? (Master??, ????)\n");
                } else {
                    printf("TK8710??????: ret=%d\n", ret);
                }
                break;
                
            case 'l':
            case 'L':
                if (load_and_send_simulation_data(classNum, caseNum) != 0) {
                    printf("???????????\n");
                }
                break;
            
            
            case 't':
            case 'T':
                show_trm_statistics();
                break;
                
            case '1':
                printf("??????????...\n");
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc02c, 0x00010101);
                if (ret == TK8710_OK) {
                    printf("????????????(0xc02c = 0x00010101)\n");
                } else {
                    printf("???????????? ret=%d\n", ret);
                }
                break;
                
            case '2':
                printf("??????????...\n");
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc02c, 0x0001ffff);
                if (ret == TK8710_OK) {
                    printf("????????????(0xc02c = 0x0001ffff)\n");
                } else {
                    printf("???????????? ret=%d\n", ret);
                }
                break;
                
            case 'd':
            case 'D':
                printf("??????????????MD_DATA?????\n");
                s_ram_rd0 ramRd0;
                ramRd0.data = 0;
                ramRd0.b.cap_en = 1;  // 启用采集
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
                if (ret != TK8710_OK) {
                    TK8710_LOG_CONFIG_ERROR("??s_ram_rd0?????? ret=%d\n", ret);
                    return ret;
                }
                g_captureDataPending = 1;
                g_captureDataPendingNum = 0;
                break;
                
            case 'f':
            case 'F':
                printf("??ACM????...\n");
                
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CAL_FACTOR, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM????????\n");
                } else {
                    printf("ACM????????: ret=%d\n", ret);
                }
                break;
                
            case 'n':
            case 'N':
                printf("??ACM SNR??..\n");
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_SNR, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM SNR?????\n");
                } else {
                    printf("ACM SNR?????? ret=%d\n", ret);
                }
                break;
                
            case 'g':
            case 'G':
                printf("??ACM??????...\n");
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, TK8710_DBG_OPT_GET, NULL, NULL);
                if (ret == TK8710_OK) {
                    printf("ACM????????\n");
                } else {
                    printf("ACM????????: ret=%d\n", ret);
                }
                break;
                
            case 'k':
            case 'K':
            {
                AcmCalibParams calibParams;
                // char input[100];
                // int c;
                
                // printf("??ACM??...\n");
                
                // /* 清理输入缓冲�?*/
                // while ((c = getchar()) != '\n' && c != EOF);
                
                // /* 获取校准次数 */
                // printf("????????(??5): \n");
                // fflush(stdout);  /* 确保提示信息立即显示 */
                // if (fgets(input, sizeof(input), stdin) != NULL) {
                //     /* 移除换行�?*/
                //     input[strcspn(input, "\n")] = 0;
                //     if (strlen(input) > 0) {
                //         calibParams.calibCount = (uint8_t)atoi(input);
                //     } else {
                //         calibParams.calibCount = 5;  /* 默认�?*/
                //     }
                // } else {
                //     calibParams.calibCount = 5;  /* 默认�?*/
                // }
                
                // /* 获取SNR门限�?*/
                // printf("???SNR????(??32): \n");
                // fflush(stdout);  /* 确保提示信息立即显示 */
                // if (fgets(input, sizeof(input), stdin) != NULL) {
                //     /* 移除换行�?*/
                //     input[strcspn(input, "\n")] = 0;
                //     if (strlen(input) > 0) {
                //         calibParams.snrThreshold = (uint8_t)atoi(input);
                //     } else {
                //         calibParams.snrThreshold = 32;  /* 默认�?*/
                //     }
                // } else {
                //     calibParams.snrThreshold = 32;  /* 默认�?*/
                // }
                
                // printf("??ACM?? (????: %d, SNR??: %d)...\n", 
                //        calibParams.calibCount, calibParams.snrThreshold);
                calibParams.calibCount = 100;
                calibParams.snrThreshold = 28;
                int calibRet; 
                ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CALIBRATE, TK8710_DBG_OPT_EXE, 
                                    &calibParams, &calibRet);
                if (ret == TK8710_OK) {
                    printf("ACM????\n");
                } else {
                    printf("ACM????: ret=%d\n", ret);
                }
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
    printf("\n=== ?????????????===\n");
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
 * RK3506 编译命令 (在开发板�?:
 *   gcc -I../inc -I../port test8710main.c ../port/tk8710_rk3506.c \
 *       ../src/tk8710_irq.c ../src/tk8710_core.c ../src/tk8710_config.c \
 *       ../src/tk8710_log.c -o test8710main -lgpiod -lpthread
 * 
 * RK3506 交叉编译命令 (在PC�?:
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port test8710main.c \
 *       ../port/tk8710_rk3506.c ../src/tk8710_irq.c ../src/tk8710_core.c \
 *       ../src/tk8710_config.c ../src/tk8710_log.c \
 *       -o test8710main -lgpiod -lpthread
 * 
 * 运行:
 *   ./test8710main      (RK3506 Linux)
 * 
 * 功能说明:
 * 1. 完整的TK8710初始化流�?
 * 2. RK3506 SPI接口初始�?(/dev/spidev0.0)
 * 3. GPIO中断初始�?(libgpiod)
 * 4. 射频系统配置
 * 5. 8710芯片初始�?
 * 6. 时隙参数配置 (TK8710_CFG_TYPE_SLOT_CFG)
 * 7. 启动主模式连续工�?
 * 8. 实时中断处理和状态监�?
 * 9. 交互式命令行界面
 * 
 * 注意事项:
 * 1. 需要TK8710硬件连接到RK3506的SPI接口
 * 2. 确保/dev/spidev0.0存在且有访问权限
 * 3. 确保libgpiod已安�?
 * 4. 可能需要root权限运行
 * 5. 按Ctrl+C可安全退出程�?
 */



#endif /* PLATFORM_TMS570 */
