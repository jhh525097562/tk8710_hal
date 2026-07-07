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

/* TRM相关变量 */
static uint32_t g_trmSendCount = 0;               /* TRM发送计数 */
static uint32_t g_trmRxCount = 0;                 /* TRM接收计数 */

/* 丢包统计变量 */
static uint32_t g_packetCount = 0;               /* 当前统计周期内的包计数 */
static uint32_t g_packetLostCount = 0;           /* 当前统计周期内的丢包计数 */
static uint32_t g_totalPacketCount = 0;         /* 总包计数 */
static uint32_t g_totalPacketLostCount = 0;     /* 总丢包计数 */
static uint32_t g_lastValidUserIndex = 0;       /* 上次有效用户索引 */

/* Driver回调函数声明 */
static void OnDriverRxData(TK8710IrqResult* irqResult);
static void OnDriverTxSlot(TK8710IrqResult* irqResult);
static void OnDriverSlotEnd(TK8710IrqResult* irqResult);
static void OnDriverError(TK8710IrqResult* irqResult);

/* 函数声明 */
static void read_register(void);
static void write_register(void);
static void read_rf_register(void);
static void write_rf_register(void);
static void test_register_rw(void);
static void show_help(void);
static void ShowSystemStatus(void);
static void ShowIrqStatistics(void);

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
                            
                            printf("用户%u: freq=%dHz (raw=0x%08X), rssi=%d, snr=%u, 丢包=%u/%u\n", 
                                   userIndex, freqValue/128, freq26, rssiValue, snrValue, g_totalPacketLostCount, g_totalPacketCount);
                            
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
static void __attribute__((unused)) OnTrmRxData(const TRM_RxDataList* rxDataList)
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
static void __attribute__((unused)) OnTrmTxComplete(const TRM_TxCompleteResult* txResult)
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
 * @brief 读取RF寄存器
 */
void read_rf_register(void)
{
    uint8_t rfSel;
    uint16_t addr;
    uint32_t value;
    int ret;
    
    printf("\n=== 读取RF寄存器 ===\n");
    printf("输入RF选择 (0-7): ");
    
    if (scanf("%hhu", &rfSel) != 1) {
        printf("无效的RF选择\n");
        return;
    }
    
    printf("输入寄存器地址 (十六进制，如 0x1234): ");
    
    if (scanf("%hx", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    ret = tk8710_rf_read(rfSel, addr, &value);
    if (ret == TK8710_OK) {
        printf("RF%d寄存器 0x%04X = 0x%08X (%u)\n", rfSel, addr, value, value);
    } else {
        printf("读取失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}

/**
 * @brief 写入RF寄存器
 */
void write_rf_register(void)
{
    uint8_t rfSel;
    uint16_t addr;
    uint32_t value;
    int ret;
    
    printf("\n=== 写入RF寄存器 ===\n");
    printf("输入RF选择 (0-7): ");
    
    if (scanf("%hhu", &rfSel) != 1) {
        printf("无效的RF选择\n");
        return;
    }
    
    printf("输入寄存器地址 (十六进制，如 0x1234): ");
    
    if (scanf("%hx", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    printf("输入写入值 (十六进制，如 0x12345678): ");
    
    if (scanf("%x", &value) != 1) {
        printf("无效的值格式\n");
        return;
    }
    
    ret = tk8710_rf_write(rfSel, addr, value);
    if (ret == TK8710_OK) {
        printf("RF%d寄存器 0x%04X = 0x%08X (%u)\n", rfSel, addr, value, value);
    } else {
        printf("写入失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}

/**
 * @brief 显示系统状态
 */
void ShowSystemStatus(void)
{
    printf("\n=== 系统状态 ===\n");
    printf("TK8710寄存器测试程序运行中\n");
    printf("TRM发送计数: %u\n", g_trmSendCount);
    printf("TRM接收计数: %u\n", g_trmRxCount);
    printf("总包计数: %u\n", g_totalPacketCount);
    printf("丢包计数: %u\n", g_totalPacketLostCount);
    printf("==================\n\n");
}

/**
 * @brief 显示中断统计
 */
void ShowIrqStatistics(void)
{
    printf("\n=== 中断统计 ===\n");
    TK8710PrintIrqTimeStats();
    printf("==================\n\n");
}

/**
 * @brief 显示帮助信息
 */
void show_help(void)
{
    printf("\n=== TK8710 Driver + Register Test Commands ===\n");
    printf("Basic Commands:\n");
    printf("  h/H - Show this help information\n");
    printf("  s/S - Show system status\n");
    printf("  i/I - Show interrupt statistics\n");
    printf("  c/C - Clear screen\n");
    printf("  q/Q - Quit program\n");
    printf("\nRegister Commands:\n");
    printf("  r/R - Read global register\n");
    printf("  w/W - Write global register\n");
    printf("\nRF Register Commands:\n");
    printf("  RRF/rrf - Read RF register\n");
    printf("  WRF/wrf - Write RF register\n");
    printf("\nCommand Line Options:\n");
    printf("  --testreg - Run register test and exit\n");
    printf("  --help, -h - Show help\n");
    printf("+--------------------------------------+\n");
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
    char cmd[32];
    
    /* RF配置必须在命令行参数检查前定义 */
    static ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .Freq = 509100000,
        .rxgain = 0x7e,
        .txgain = 0x2a,
        .txadc = {//2号板
            {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
            {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        }
    };
    
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
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig
    };
    
    /* 检查命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [--help|-h|--testreg]\n", argv[0]);
            printf("  --help, -h: Show this help\n");
            printf("  --testreg: Run register read/write test and exit\n");
            printf("\nExamples:\n");
            printf("  %s         # Run register test program\n", argv[0]);
            printf("  %s --testreg  # Run register test and exit\n", argv[0]);
            return 0;
        } else if (strcmp(argv[1], "--testreg") == 0) {
            /* 命令行参数--testreg：运行寄存器测试后退出 */
            printf("Initializing 8710 for register test...\n");
            ret = TK8710Init(&chipConfig);
            if (ret != TK8710_OK) {
                printf("8710 initialization failed: %d\n", ret);
                return -1;
            }
            TK8710LogConfig_t testLogConfig = {
                .level = TK8710_LOG_INFO,
                .module_mask = TK8710_LOG_MODULE_ALL,
                .callback = NULL,
                .enable_timestamp = 1,
                .enable_module_name = 1,
                .enable_file_logging = 1,
                .log_file_dir = NULL
            };
            TK8710LogInit(&testLogConfig);
            printf("Running register test...\n");
            test_register_rw();
            TK8710Reset(TK8710_RST_STATE_MACHINE);
            printf("Test completed, exiting.\n");
            return 0;
        }
    }
    
#ifdef _WIN32
    /* 设置控制台编码为UTF-8 */
    SetConsoleOutputCP(65001);  // UTF-8
    SetConsoleCP(65001);       // UTF-8
    setlocale(LC_ALL, ".UTF8");
#endif
    
    printf("\n");
    printf("+======================================+\n");
    printf("|   TK8710 Register Test Program     |\n");
    printf("|   Version: 1.0 (RK3506)             |\n");
    printf("|   Complete Register Test Workflow  |\n");
    printf("+======================================+\n");
    
#ifndef _WIN32
    /* 注册Linux信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    /* 初始化默认日志系统（如果尚未初始化） */
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
    printf("Initializing 8710 (chip + RF)...\n");
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("8710 initialization failed: %d\n", ret);
        return -1;
    }

    printf("HAL initialization completed (including RF)\n");
    
    /* 8. 主循环 - 等待中断并进行中断处理 */
    while (g_running) {
        printf("TK8710> ");
        
#ifdef _WIN32
        int ch = _getch();
        if (ch != '\r') {
            printf("%c\n", ch);
            cmd[0] = (char)ch;
            cmd[1] = '\0';
        } else {
            printf("\n");
            continue;
        }
#else
        scanf("%31s", cmd);
#endif
        
        /* 处理多字符命令 */
        if (strcasecmp(cmd, "RRF") == 0) {
            read_rf_register();
            continue;
        }
        if (strcasecmp(cmd, "WRF") == 0) {
            write_rf_register();
            continue;
        }
        
        /* 处理单字符命令 */
        char input = cmd[0];
        if (cmd[1] != '\0' && strlen(cmd) > 1) {
            /* 输入超过1个字符且不是RRF/WRF，显示错误 */
            printf("Unknown command: %s (enter 'h' for help)\n", cmd);
            continue;
        }
        
        switch (input) {
            case 'h':
            case 'H':
                show_help();
                break;
                
            case 's':
            case 'S':
                ShowSystemStatus();
                break;
                
            case 'i':
            case 'I':
                ShowIrqStatistics();
                break;
                
            case 'c':
            case 'C':
#ifdef _WIN32
                system("cls");
#else
                system("clear");
#endif
                break;
                
            case 'r':
            case 'R':
                read_register();
                break;
                
            case 'w':
            case 'W':
                write_register();
                break;
                
            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
                break;
                
            default:
                printf("Unknown command: %s (enter 'h' for help)\n", cmd);
                break;
        }
    }

    defaultLogConfig.level = TK8710_LOG_ALL;
    TK8710LogInit(&defaultLogConfig);
    /* 打印最终的中断时间统计报告 */
    printf("\n=== 最终中断处理时间统计报告 ===\n");
    TK8710PrintIrqTimeStats();

    TK8710Reset(TK8710_RST_STATE_MACHINE);
    
    printf("Program ended\n");
    return 0;
}

/**
 * @brief 测试寄存器读写功能
 */
void test_register_rw(void)
{
    int ret;
    
    printf("\n=== 开始寄存器读写测试 ===\n");
    
    /* 调用TK8710DebugCtrl进行寄存器读写测试 */
    ret = TK8710DebugCtrl(TK8710_DBG_TYPE_REG_RW, TK8710_DBG_OPT_SET, NULL, NULL);
    if (ret == TK8710_OK) {
        printf("寄存器读写测试完成\n");
    } else {
        printf("寄存器读写测试失败: 错误码=%d\n", ret);
    }
    
    printf("==================\n\n");
}

/*============================================================================
 * 编译说明
 *============================================================================
 * 
 * RK3506 编译命令 (在开发板上):
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port Test8710W_R_Reg.c \
 *       ../port/tk8710_rk3506.c ../src/driver/tk8710_irq.c ../src/driver/tk8710_core.c \
 *       ../src/driver/tk8710_config.c ../src/driver/tk8710_log.c \
 *       -o Test8710W_R_Reg -lgpiod -lpthread
 * 
 * RK3506 交叉编译命令 (在PC上):
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port Test8710W_R_Reg.c \
 *       ../port/tk8710_rk3506.c ../src/driver/tk8710_irq.c ../src/driver/tk8710_core.c \
 *       ../src/driver/tk8710_config.c ../src/driver/tk8710_log.c \
 *       -o Test8710W_R_Reg -lgpiod -lpthread
 * 
 * 运行:
 *   ./Test8710W_R_Reg      (RK3506 Linux)
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
 * 9. 寄存器读写测试功能
 * 10. 交互式命令行界面
 * 
 * 注意事项:
 * 1. 需要TK8710硬件连接到RK3506的SPI接口
 * 2. 确保/dev/spidev0.0存在且有访问权限
 * 3. 确保libgpiod已安装
 * 4. 可能需要root权限运行
 * 5. 按Ctrl+C可安全退出程序
 */


