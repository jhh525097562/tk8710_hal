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
#include "tk8710_hal.h"
#include "hal_api.h"   /* HAL API接口 */
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_driver_api.h"  /* Driver API接口 */
#include "driver/tk8710_regs.h"
#include "trm/trm_api.h"        /* 添加TRM头文件 */
#include "trm/trm_log.h"     /* 添加TRM日志头文件 */
#include "trm_tx_validator.h"  /* 添加发送验证模块 */
#include "tk8710_scan_service.h"

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

/* 下行发送状态跟踪 */
static volatile bool g_hasValidUsers = false;     /* 是否有有效用户 */
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

/* TRM相关变量 */
static uint32_t g_trmSendCount = 0;               /* TRM发送计数 */
static uint32_t g_trmRxCount = 0;                 /* TRM接收计数 */

/* TRM回调函数声明 */
static void OnTrmRxData(const TRM_RxDataList* rxDataList);
static void OnTrmTxComplete(const TRM_TxCompleteResult* txResult);

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

    for (uint8_t i = 0; i < rxDataList->userCount; i++) {
        TRM_RxUserData* user = &rxDataList->users[i];
        printf("  用户[%d]: ID=0x%08X, 长度=%d, RSSI=%d, SNR=%d, Freq=%d Hz\n",
               i, user->userId, user->dataLen, user->rssi, user->snr, user->freq/128);

        /* 显示数据内容 */
        if (user->data != NULL && user->dataLen > 0) {
            printf("    数据: ");
            for (int k = 0; k < user->dataLen && k < 8; k++) {
                printf("%02X ", user->data[k]);
            }
            if (user->dataLen > 8) printf("...");
            printf("\n");
        }
    }

    // static int CalibrateCount = 0; // 默认值
    // CalibrateCount++;
    // int Tmp = 400;
    // // if(CalibrateCount % 10 == 0 && CalibrateCount <= 20000) {
    // if(CalibrateCount % 10 == 0) {
    //     TRM_AcmCalibRequest acmRequest = {
    //         .calibCount = 1,
    //         .snrThreshold = 28,
    //         .restartAdvanceUs = Tmp,//mode5-6:200,mode7:212,mode8:
    //         .guardUs = 1000
    //     };
    //     int ret0 = TRM_RequestAcmCalibration(&acmRequest);
    //     if (ret0 == TRM_OK) {
    //         printf("TRM ACM calibration request submitted; it will run at last-frame S2 end\n");
    //     } else {
    //         printf("TRM ACM calibration request failed: ret=%d\n", ret0);
    //     }
    // }
    /* 调用发送验证器 */
    int ret = TRM_TxValidatorOnRxData(rxDataList);
    if (ret != TRM_OK) {
        printf("  验证器处理失败: 错误码=%d\n", ret);
    }
    /* 显示验证统计信息 */
    TRM_TxValidatorStats stats;
    if (TRM_TxValidatorGetStats(&stats) == TRM_OK) {
        printf("  发送统计: 总触发=%u, 成功=%u, 失败=%u, 接收总次数=%u\n",
               stats.totalTriggerCount, stats.successSendCount, stats.failedSendCount, g_trmRxCount);
    }

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
    printf("  q/Q - Quit program\n");
    printf("\nTRM Commands (when TRM enabled):\n");
    printf("  t/T - Show TRM statistics\n");
    printf("  k/K - Request TRM ACM calibration\n");
    printf("  a/A - Start frequency sweep\n");
    printf("\nDriver Test Commands:\n");
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

static int read_sweep_frequency_hz(const char* prompt, uint32_t* freq_hz)
{
    char text[32];
    char* end_ptr;
    double value;
    double hz;

    if (prompt == NULL || freq_hz == NULL) {
        return -1;
    }

    printf("%s", prompt);
    fflush(stdout);

    if (scanf("%31s", text) != 1) {
        clearerr(stdin);
        return -1;
    }

    errno = 0;
    value = strtod(text, &end_ptr);
    if (end_ptr == text || errno != 0 || value <= 0.0) {
        return -1;
    }

    if (*end_ptr == '\0') {
        hz = (value < 1000000.0) ? value * 1000000.0 : value;
    } else if (strcmp(end_ptr, "M") == 0 || strcmp(end_ptr, "m") == 0 ||
               strcmp(end_ptr, "MHz") == 0 || strcmp(end_ptr, "mhz") == 0) {
        hz = value * 1000000.0;
    } else if (strcmp(end_ptr, "K") == 0 || strcmp(end_ptr, "k") == 0 ||
               strcmp(end_ptr, "kHz") == 0 || strcmp(end_ptr, "khz") == 0) {
        hz = value * 1000.0;
    } else {
        return -1;
    }

    if (hz <= 0.0 || hz > (double)UINT32_MAX) {
        return -1;
    }

    *freq_hz = (uint32_t)(hz + 0.5);
    return 0;
}

static void start_frequency_sweep_from_console(void)
{
    uint32_t start_freq;
    uint32_t end_freq;
    const int sweep_mode = 3;
    int ret;

    printf("\n=== Frequency Sweep ===\n");
    printf("Input format: Hz or MHz, for example 470000000 or 470.5M\n");

    if (read_sweep_frequency_hz("Start frequency: ", &start_freq) != 0 ||
        read_sweep_frequency_hz("End frequency: ", &end_freq) != 0) {
        printf("[Sweep] Invalid frequency input\n");
        return;
    }

    if (start_freq > end_freq) {
        printf("[Sweep] Invalid range: start=%u Hz, end=%u Hz\n", start_freq, end_freq);
        return;
    }

    printf("[Sweep] Starting: start=%u Hz, end=%u Hz, mode=%d (500 kHz step)\n",
           start_freq, end_freq, sweep_mode);
    ret = TK8710ScanStart(start_freq, end_freq, sweep_mode);
    if (ret != 0) {
        printf("[Sweep] Start failed: ret=%d\n", ret);
    }
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
    bool use_multi_rate = false;
    int testMode = 6;  /* 默认模式6 */
    int s1ByteLen = 26;  /* 默认s1 byteLen */
    int s2ByteLen = 26;  /* 默认s2 byteLen */
    int s3ByteLen = 26;  /* 默认s3 byteLen */
    int Tmp = 90;
    uint32_t Rxgain = 0x7e;
    uint32_t Txgain = 0x2a;
    uint32_t freq = 509100000U;
    /* 检查命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "--multi-rate") == 0 || strcmp(argv[1], "-m") == 0) {
            use_multi_rate = true;
            printf("Multi-rate mode enabled\n");
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [--multi-rate|-m] [--help|-h]\n", argv[0]);
            printf("  --multi-rate, -m : Use multi-rate configuration\n");
            printf("  --help, -h       : Show this help\n");
            return 0;
        }
        testMode = atoi(argv[1]);
        if (testMode < 5 || testMode > 18 || (testMode > 11 && testMode < 18)) {
            printf("Error: Invalid mode %d. Supported modes: 5,6,7,8,9,10,11,18\n", testMode);
            return 1;
        }

        /* 解析s1ByteLen参数 */
        if (argc > 2) {
            s1ByteLen = atoi(argv[2]);
            if (s1ByteLen < 0 || s1ByteLen > 255) {
                printf("Error: Invalid s1ByteLen %d. Must be positive integer <= 255\n", s1ByteLen);
                return 1;
            }
        }

        /* 解析s2ByteLen参数 */
        if (argc > 3) {
            s2ByteLen = atoi(argv[3]);
            if (s2ByteLen < 0 || s2ByteLen > 255) {
                printf("Error: Invalid s2ByteLen %d. Must be positive integer <= 255\n", s2ByteLen);
                return 1;
            }
        }

        /* 解析s3ByteLen参数 */
        if (argc > 4) {
            s3ByteLen = atoi(argv[4]);
            if (s3ByteLen < 0 || s3ByteLen > 255) {
                printf("Error: Invalid s3ByteLen %d. Must be positive integer <= 255\n", s3ByteLen);
                return 1;
            }
        }
        /* 解析s3ByteLen参数 */
        if (argc > 5) {
            Tmp = atoi(argv[5]);
        }

        Rxgain = atoi(argv[6]);
        freq = atoi(argv[7]);
        Txgain = atoi(argv[8]);
        printf("Using test mode: %d, s1ByteLen: %d, s2ByteLen: %d, s3ByteLen: %d\n",
               testMode, s1ByteLen, s2ByteLen, s3ByteLen);
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

    /* ========== 使用 HAL API 进行初始化 ========== */
    /* 1. 准备RF配置 */
    static ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .Freq = 503100000,
        .rxgain = 0x7e,
        .txgain = 0x2a,
        // .txadc = {//C号板
        //     {0x0bc0, 0x04a0}, {0x0a50, 0x0780}, {0x0750, 0x0820}, {0x0bc3, 0x0940},
        //     {0x0e83, 0x05e0}, {0xfbff, 0x0850}, {0x0880, 0x0500}, {0x02a0, 0x06ff}
        // }
        // .txadc = {//2号板
        //     {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
        //     {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
        // }
    };
    rfConfig.rxgain = Rxgain;
    rfConfig.Freq = freq;
    rfConfig.txgain = Txgain;
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

    /* 3. 准备TRM配置 (与原 init_trm_system 配置一致) */
    TRM_InitConfig trmConfig;
    memset(&trmConfig, 0, sizeof(trmConfig));
    trmConfig.beamMode = TRM_BEAM_MODE_FULL_STORE;
    trmConfig.beamMaxUsers = 3000;
    trmConfig.beamTimeoutMs = 10000;
    trmConfig.callbacks.onRxData = OnTrmRxData;
    trmConfig.callbacks.onTxComplete = OnTrmTxComplete;
    trmConfig.maxFrameCount = 10;
    /* 4. 准备HAL初始化配置 */
    TK8710HalInitCfg halConfig = {
        .chipInitCfg = &chipConfig,
        .trmCfg = {
            .beamMaxUsers = trmConfig.beamMaxUsers,
            .beamTimeoutMs = trmConfig.beamTimeoutMs,
            .maxFrameCount = trmConfig.maxFrameCount,
            .onRxData = trmConfig.callbacks.onRxData,
            .onTxComplete = trmConfig.callbacks.onTxComplete
        }
    };

    /* 5. 调用 TK8710HalInit 完成芯片、RF、日志、TRM初始化 */
    printf("Initializing HAL (chip + RF + log + TRM)...\n");
    TK8710HalError halRet = TK8710HalInit(&halConfig);
    if (halRet != TK8710_HAL_OK) {
        printf("HAL initialization failed: %d\n", halRet);
        return -1;
    }

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
    slotCfg.txBcnAntEn = 0x7f;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;

    /* 配置BCN轮流发送 */
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;
    }

    if (use_multi_rate) {
        /* 多速率配置 */
        uint8_t rateModes[] = {
            TK8710_RATE_MODE_5,
            TK8710_RATE_MODE_6,
            TK8710_RATE_MODE_7,
            TK8710_RATE_MODE_8
        };
        uint8_t rateCount = sizeof(rateModes) / sizeof(rateModes[0]);
        slotCfg.rateCount = rateCount;

        printf("Using multi-rate configuration with %d rates\n", rateCount);

        for (int i = 0; i < (int)rateCount; i++) {
            slotCfg.rateModes[i] = rateModes[i];

            /* 根据速率模式设置da_m参数 */
            switch (rateModes[i]) {
                case TK8710_RATE_MODE_5:
                    slotCfg.s1Cfg[i].da_m = 21492;
                    slotCfg.s2Cfg[i].da_m = 21492;
                    slotCfg.s3Cfg[i].da_m = 21492;
                    break;
                case TK8710_RATE_MODE_6:
                    slotCfg.s1Cfg[i].da_m = 19728;
                    slotCfg.s2Cfg[i].da_m = 19728;
                    slotCfg.s3Cfg[i].da_m = 19728;
                    break;
                case TK8710_RATE_MODE_7:
                    slotCfg.s1Cfg[i].da_m = 12000;
                    slotCfg.s2Cfg[i].da_m = 12000;
                    slotCfg.s3Cfg[i].da_m = 12000;
                    break;
                case TK8710_RATE_MODE_8:
                    slotCfg.s1Cfg[i].da_m = 5600;
                    slotCfg.s2Cfg[i].da_m = 5600;
                    slotCfg.s3Cfg[i].da_m = 5600;
                    break;
                default:
                    slotCfg.s1Cfg[i].da_m = 12000;
                    slotCfg.s2Cfg[i].da_m = 12000;
                    slotCfg.s3Cfg[i].da_m = 12000;
                    break;
            }

            /* 配置时隙长度和频点 */
            slotCfg.s0Cfg[i].byteLen = 0;
            slotCfg.s0Cfg[i].centerFreq = 503100000;
            slotCfg.s1Cfg[i].byteLen = 26;
            slotCfg.s1Cfg[i].centerFreq = 503100000;
            slotCfg.s2Cfg[i].byteLen = 26;
            slotCfg.s2Cfg[i].centerFreq = 503100000;
            slotCfg.s3Cfg[i].byteLen = 26;
            slotCfg.s3Cfg[i].centerFreq = 503100000;
        }
    } else {
        /* 单速率配置 */
        printf("Using single-rate configuration\n");
        slotCfg.rateCount = 1;
        slotCfg.rateModes[0] = testMode;
        slotCfg.s0Cfg[0].da_m = 0;
        /* 根据速率模式设置S1、S2、S3时隙的da_m参数 */
        switch (slotCfg.rateModes[0]) {
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
                slotCfg.s1Cfg[0].da_m = 5600;     /* da1_m */
                slotCfg.s2Cfg[0].da_m = 5600;     /* da2_m */
                slotCfg.s3Cfg[0].da_m = 5600;     /* da3_m */
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
        slotCfg.s0Cfg[0].centerFreq = 503100000;
        slotCfg.s1Cfg[0].byteLen = s1ByteLen;
        slotCfg.s1Cfg[0].centerFreq = 503100000;
        slotCfg.s2Cfg[0].byteLen = s2ByteLen;
        slotCfg.s2Cfg[0].centerFreq = 503100000;
        slotCfg.s3Cfg[0].byteLen = s3ByteLen;
        slotCfg.s3Cfg[0].centerFreq = 503100000;
    }
        // 使用时隙计算函数计算gap参数
        TRM_SlotCalcInput slotInput = {
            .rateMode = slotCfg.rateModes[0],
            .brdBlockNum = slotCfg.s1Cfg[0].byteLen / 26,  // 广播包块数
            .ulBlockNum = slotCfg.s2Cfg[0].byteLen / 26,     // 上行包块数
            .dlBlockNum = slotCfg.s3Cfg[0].byteLen / 26,   // 下行包块数
            .superFrameNum = 10,
            .calcType = TRM_SLOT_CALC_TYPE_GROUND_WAN
        };

    TRM_SlotCalcOutput slotOutput;
    if (trm_calc_slot_config(&slotInput, &slotOutput) == 0) {
        printf("✅ 速率模式%d计算得到gap参数: BRD=%u, UL=%u, DL=%u\n",
                slotCfg.rateModes[0], slotOutput.brdGap, slotOutput.ulGap, slotOutput.dlGap);
    }
    /* 调用 TK8710HalCfg 配置时隙 */
    halRet = TK8710HalCfg(&slotCfg);
    if (halRet != TK8710_HAL_OK) {
        printf("HAL config (slot) failed: %d\n", halRet);
        return -1;
    }
    printf("Slot parameter configuration completed\n");

    /* 7. 初始化发送验证器 */
    TRM_TxValidatorConfig validatorConfig = {
        .txPower = 35,
        .frameOffset = 2,
        .userIdBase = 0x30000000,
        .enableAutoResponse = true,
        .enablePeriodicTest = false,
        .periodicIntervalFrames = 10,
        .responseDataLength = 26
    };
    ret = TRM_TxValidatorInit(&validatorConfig);
    if (ret != TRM_OK) {
        printf("TX validator initialization failed: %d (non-fatal)\n", ret);
    }

    // /* 8. 配置测试选项 */
    // printf("Configure test options:\n");
    // printf("Enable force process all users for testing? (y/n): ");
    // char testChoice;
    // scanf(" %c", &testChoice);
    // if (testChoice == 'y' || testChoice == 'Y') {
    //     TK8710SetForceProcessAllUsers(1);
    //     printf("Force process all users: ENABLED\n");
    // } else {
    //     TK8710SetForceProcessAllUsers(0);
    //     printf("Force process all users: DISABLED\n");
    // }

    // printf("Enable force max users TX for testing? (y/n): ");
    // scanf(" %c", &testChoice);
    // if (testChoice == 'y' || testChoice == 'Y') {
    //     TK8710SetForceMaxUsersTx(1);
    //     printf("Force max users TX: ENABLED\n");
    // } else {
    //     TK8710SetForceMaxUsersTx(0);
    //     printf("Force max users TX: DISABLED\n");
    // }
    uint8_t data[30] = {0};
    halRet = TK8710HalSendData(
        TK8710_DOWNLINK_A,  // slot=1->A, slot=3->B
        0,                  // 用户ID（示例）
        data,         // 数据载荷
        20,     // 数据长度
        35,                       // 发射功率
        1,             // 帧号
        testMode,
        TK8710_DATA_TYPE_BRD  // tdd=255->DED, tdd=0->BRD
    );
    /* 9. 调用 TK8710HalStart 启动工作 */
    halRet = TK8710HalStart();
    if (halRet != TK8710_HAL_OK) {
        printf("HAL start failed: %d\n", halRet);
        return -1;
    }
    printf("HAL started successfully (Master mode, Continuous work)\n");

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

            case 'r':
            case 'R':
                read_register();
                break;

            case 'w':
            case 'W':
                write_register();
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

            case 'a':
            case 'A':
                start_frequency_sweep_from_console();
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

            // case 'k':
            // case 'K':
            // {
            //     uint32_t gain1255;
            //     printf("请输入1255gain (hex, e.g. 0x2a): ");
            //     fflush(stdout);
            //     if (scanf("%x", &gain1255) != 1 || gain1255 > 0xFF) {
            //         printf("无效的1255gain，范围: 0x00~0xFF\n");
            //         break;
            //     }
            //     ret = tk8710_rf_write(0xff, 0x8C7e >> 8, gain1255);
            //     if (ret == TK8710_OK) {
            //         printf("校准时1255增益设置成功\n");
            //     } else {
            //         printf("校准时1255增益设置失败: ret=%d\n", ret);
            //     }
            //     printf("执行ACM增益自动获取, 1255gain=0x%02X...\n", (uint8_t)gain1255);
            //     AcmCalibParams calibParams;
            //     calibParams.calibCount = 100;
            //     calibParams.snrThreshold = 5;//20
            //     int calibRet;
            //     ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CALIBRATE, TK8710_DBG_OPT_EXE,
            //                         &calibParams, &calibRet);
            //     if (ret == TK8710_OK) {
            //         printf("ACM校准完成\n");
            //     } else {
            //         printf("ACM校准失败: ret=%d\n", ret);
            //     }

            //     ret = tk8710_rf_write(0xff, 0x8C7e >> 8, Rxgain);
            //     if (ret == TK8710_OK) {
            //         printf("校准后1255增益恢复成功\n");
            //     } else {
            //         printf("校准后1255增益恢复失败: ret=%d\n", ret);
            //     }
            //     break;
            // }
            case 'k':
            case 'K':
            {
                TRM_AcmCalibRequest acmRequest = {
                    .calibCount = 1,
                    .snrThreshold = 28,
                    .restartAdvanceUs = Tmp,//mode5-6:200,mode7:212,mode8:
                    .guardUs = 1000
                };

                ret = TRM_RequestAcmCalibration(&acmRequest);
                if (ret == TRM_OK) {
                    printf("TRM ACM calibration request submitted; it will run at last-frame S2 end\n");
                } else {
                    printf("TRM ACM calibration request failed: ret=%d\n", ret);
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

    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_ALL,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1
    };
    TK8710LogInit(&defaultLogConfig);
    /* 打印最终的中断时间统计报告 */
    printf("\n=== 最终中断处理时间统计报告 ===\n");
    TK8710PrintIrqTimeStats();

    /* 清理发送验证器 */
    TRM_TxValidatorDeinit();

    TK8710HalReset();

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


