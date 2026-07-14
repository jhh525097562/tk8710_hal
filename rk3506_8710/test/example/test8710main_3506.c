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
#include "hal_api.h"
#include "phy/phy_api.h"
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_log.h"

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
 * 外部函数声明 (RK3506 HAL相关)
 *============================================================================*/

/* RK3506 清理函数 */
extern void TK8710Rk3506Cleanup(void);

/* SPI 协议层函�?*/
extern int TK8710SpiInit(const SpiConfig* cfg);

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

/* 下行发送状态跟�?*/
static volatile bool g_hasValidUsers = false;     /* 是否有有效用�?*/
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

static void app_gpio_irq_handler(void* user);

/* 中断回调函数声明 */
void app_irq_handler(TK8710IrqResult irqResult);

/*============================================================================
 * 自动下行发送测试函�?
 *============================================================================*/

/**
 * @brief 测试自动下行发�?- 一次性设置所有用�?
 * @param userIndices 用户索引数组
 * @param txPowers 对应的发送功率数�?
 * @param userCount 用户数量
 * @return 0-成功, 其他-失败
 */
static int __attribute__((unused)) test_auto_downlink_tx_all(const uint8_t* userIndices, const uint8_t* txPowers, uint8_t userCount)
{
    static uint8_t txCounter = 0;
    int ret;
    
    printf("Setting up auto downlink TX for %d users\n", userCount);
    
    /* 为每个用户生成并发送数�?*/
    for (uint8_t i = 0; i < userCount; i++) {
        uint8_t userIndex = userIndices[i];
        uint8_t txPower = txPowers[i];
        uint8_t txData[26];
        
        /* 生成发送数�?*/
        for (int j = 0; j < 26; j++) {
            txData[j] = txCounter + j + i;  /* 基于用户索引、计数器和用户序号生成数�?*/
        }
        
        /* 设置自动发送下�?数据和功�?*/
        ret = TK8710SetTxData(TK8710_DOWNLINK_B, userIndex, txData, 26, txPower, TK8710_DATA_TYPE_DED);
        if (ret != TK8710_OK) {
            printf("Failed to set TX user data for user[%d]: %d\n", userIndex, ret);
            return ret;
        }
        
        printf("Auto downlink TX data set successfully: user[%d], counter=%d, power=%d\n", 
               userIndex, txCounter, txPower);
    }
    
    txCounter++;
    printf("All %d users TX data set successfully\n", userCount);
    
    return TK8710_OK;
}

/**
 * @brief 测试广播发送功�?
 * @param counter 广播计数�?
 * @return 0-成功, 1-失败
 */
int test_broadcast_tx(uint8_t counter)
{
    uint8_t brdData[26];  /* 广播数据，长度与S1时隙长度一�?*/
    
    /* 准备广播数据 */
    for (int i = 0; i < 26; i++) {
        brdData[i] = counter + i;  /* 简单的递增数据 */
    }
    
    /* 使用新的下行1发送API设置下行1数据 */
    int ret = TK8710SetTxData(TK8710_DOWNLINK_A, 0, brdData, 26, 35, TK8710_DATA_TYPE_BRD);  /* 下行A用户0，功�?5 */
    if (ret == TK8710_OK) {
        printf("Broadcast data set successfully: counter=%d\n", counter);
        return 0;
    } else {
        printf("Failed to set broadcast data: ret=%d\n", ret);
        return 1;
    }
}

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
    for (uint8_t i = 0; i < userCount; i++) {
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
 * 中断处理函数
 *============================================================================*/

/**
 * @brief 应用层中断处理函�?
 * @param irqResult 中断结果数据
 */
void app_irq_handler(TK8710IrqResult irqResult)
{
    switch (irqResult.irq_type) {
        case TK8710_IRQ_RX_BCN:
            printf("BCN receive interrupt\n");
            break;
            
        case TK8710_IRQ_BRD_UD:
            printf("Broadcast UD interrupt\n");
            break;
            
        case TK8710_IRQ_BRD_DATA:
            printf("Broadcast DATA interrupt\n");
            break;
            
        case TK8710_IRQ_MD_UD:
            printf("MD UD interrupt\n");
            break;
            
        case TK8710_IRQ_MD_DATA:
            if (irqResult.mdDataValid) {
                printf("CRC valid users: %d, CRC error users: %d\n", 
                       irqResult.crcValidCount, irqResult.crcErrorCount);
                
                /* 更新有效用户状�?*/
                g_hasValidUsers = (irqResult.crcValidCount > 0);
                
                /* 读取有效用户的数�?*/
                uint8_t validUsers[128];
                uint8_t validUserCount = 0;
                
                for (uint8_t i = 0; i < 128; i++) {
                    if (irqResult.crcResults[i].crcValid) {
                        uint8_t* userData;
                        uint16_t dataLen;
                        
                        /* 使用新的Buffer API获取数据 */
                        if (TK8710GetRxUserData(i, &userData, &dataLen) == TK8710_OK) {
                            // printf("User[%d] data (len=%d): ", i, dataLen);
                            
                            // /* 打印�?6字节数据 */
                            // uint16_t printLen = dataLen > 16 ? 16 : dataLen;
                            // for (uint16_t j = 0; j < printLen; j++) {
                            //     printf("%02X ", userData[j]);
                            // }
                            // if (dataLen > 16) {
                            //     printf("...");
                            // }
                            // printf("\n");
                            
                            /* 获取信号质量信息 */
                            uint32_t rssi, freq;
                            uint8_t snr;
                            if (TK8710GetRxUserSignalQuality(i, &rssi, &snr, &freq) == TK8710_OK) {
                                /* SNR转换：大�?56时默认为0，其他时候除�? */
                                uint8_t snrValue = snr / 4;
                                
                                /* RSSI转换�?1位有符号数，需要转换为有符号�?*/
                                uint32_t rssiRaw = rssi;
                                int16_t rssiValue = (int16_t)(rssiRaw - 2048) / 4;
                                
                                /* 频率转换�?6-bit格式转换为实际频率Hz */
                                uint32_t freq26 = freq & 0x03FFFFFF;  /* �?6�?*/
                                int32_t freqValue = (freq26 > (1U << 25)) ?
                                    (int32_t)(freq26 - (1U << 26)) : (int32_t)freq26;
                                printf("User[%d] Signal: SNR=%d, RSSI=%d, Freq=%d Hz (raw=0x%08X)\n", 
                                       i, snrValue, rssiValue, freqValue/128, freq26);
                                
                                /* 释放接收数据Buffer */
                                TK8710ReleaseRxData(i);
                                
                                /* 收集有效用户信息用于批量发�?*/
                                validUsers[validUserCount] = i;
                                validUserCount++;
                            }
                        }
                    }
                }
                
                /* 一次性设置所有有效用户的发送数�?*/
                if (validUserCount > 0) {
                    // test_auto_downlink_tx_all(validUsers, validPowers, validUserCount);
                    test_manual_tx_setup(validUsers, validUserCount);
                }
            }
            
            break;
            
        case TK8710_IRQ_S0:
            printf("S0 slot interrupt (BCN)\n");
            break;
            
        case TK8710_IRQ_S1:
            printf("S1 slot interrupt\n");
            
            /* 设置广播发送数�?*/
            {
                static uint8_t brdCounter = 0;
                test_broadcast_tx(brdCounter);
                brdCounter++;
            }
            break;
            
        case TK8710_IRQ_S2:
            printf("S2 slot interrupt\n");
            break;
            
        case TK8710_IRQ_S3:
            printf("S3 slot interrupt\n");
            break;
            
        case TK8710_IRQ_ACM:
            printf("ACM calibration interrupt\n");
            break;
            
        default:
            printf("Unknown interrupt type: %d\n", irqResult.irq_type);
            break;
    }
}

/*============================================================================
 * 初始化函�?
 *============================================================================*/

/**
 * @brief 初始化中断系�?
 * @return 0-成功, �?-失败
 */
int init_interrupt_system(void)
{
    int ret;
    
    printf("Initializing interrupt system...\n");
    
    /* 1. 初始化GPIO中断 (使用JTOOL的GPIO功能) */
    const TK8710GpioIrqCallback gpio_callback = app_gpio_irq_handler;
    ret = TK8710GpioInit(0, TK8710_GPIO_EDGE_RISING, gpio_callback, NULL);//TK8710_GPIO_EDGE_FALLING  TK8710_GPIO_EDGE_RISING
    if (ret != TK8710_OK) {
        printf("GPIO initialization failed: %d (no hardware support)\n", ret);
        /* 不返回错误，继续执行 */
    }
    
    /* 4. 使能中断 */
    ret = TK8710GpioIrqEnable(0, 1);
    if (ret != TK8710_OK) {
        printf("GPIO interrupt enable failed: %d\n", ret);
    }
    
    printf("Interrupt system initialization completed\n");
    return TK8710_OK;
}

/**
 * @brief 初始化射�?
 * @return 0-成功, �?-失败
 */
int init_rf_system(void)
{
    int ret;
    ChiprfConfig rfConfig;
    
    printf("Initializing RF system...\n");
    
    /* 配置射频参数 */
    memset(&rfConfig, 0, sizeof(ChiprfConfig));
    rfConfig.rftype = TK8710_RF_TYPE_1255_32M;  /* SX1255 32MHz */
    rfConfig.Freq = 503100000;  /* 503.1MHz */
    rfConfig.rxgain = 0x7e;       /* 接收增益 */
    rfConfig.txgain = 0x2a;       /* 发送增�?*/
    
    /* 配置所有天线的直流偏置 */
    uint16_t txadc_i[TK8710_MAX_ANTENNAS] = {0x0bc0,0x0a50,0x0750,0x0bc3,0x0e83,0xfbff,0x0880,0x02a0};
    uint16_t txadc_q[TK8710_MAX_ANTENNAS] = {0x04a0,0x0780,0x0820,0x0940,0x05e0,0x0850,0x0500,0x06ff};
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        rfConfig.txadc[i].i = txadc_i[i];  /* I路直�?*/
        rfConfig.txadc[i].q = txadc_q[i];  /* Q路直�?*/
    }
    
    /* 初始化射�?*/
    ret = TK8710RfConfig(&rfConfig);
    if (ret != TK8710_OK) {
        printf("RF initialization failed: %d\n", ret);
        return ret;
    }
    
    printf("RF system initialization completed\n");
    printf("RF type: %d, Frequency: %u Hz\n", rfConfig.rftype, rfConfig.Freq);
    return TK8710_OK;
}

/**
 * @brief 初始�?710芯片
 * @return 0-成功, �?-失败
 */
int init_tk8710_chip(void)
{
    int ret;
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
    };
    
    printf("Initializing TK8710 chip...\n");
    
    /* 使用默认配置初始化芯�?*/
    /* 注意：中断回调通过GPIO中断处理，不使用Driver回调 */
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("TK8710 chip initialization failed: %d\n", ret);
        return ret;
    }
    
    printf("TK8710 chip initialization completed\n");
    return TK8710_OK;
}

/**
 * @brief 配置时隙参数
 * @return 0-成功, �?-失败
 */
int config_slot_parameters(void)
{
    int ret;
    slotCfg_t slotCfg;
    
    printf("Configuring slot parameters...\n");
    
    /* 清零配置结构 */
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    
    /* 配置基本参数 */
    slotCfg.msMode = TK8710_MODE_MASTER;        /* 主模�?*/
    slotCfg.plCrcEn = 0;                         /* 使能CRC校验 */
    slotCfg.rateCount = 1;                       /* 速率个数，先设置�?个速率模式 */
    slotCfg.rateModes[0] = TK8710_RATE_MODE_8;   /* 第一个速率模式 */
    slotCfg.brdUserNum = 1;                      /* 广播用户�?*/
    slotCfg.antEn = 0xFF;                        /* 使能所有天�?*/
    slotCfg.rfSel = 0xFF;                        /* 选择所有RF */
    slotCfg.txBeamCtrlMode = 1;                      /* 波束控制模式 */
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);  /* 0=芯片自动控制, 1=外部指定信息控制 */
    slotCfg.txBcnAntEn = 0x7f;                         /* 发送BCN天线使能 */
    
    /* 配置BCN轮流发�?*/
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;  /* 轮流使用所有天�?0,1,2,3,4,5,6,7 */
    }
    
    /* 配置广播频率 */
    slotCfg.brdFreq[0] = 20000.0;  /* 广播用户0频率: 503.1 MHz */
    
    /* 配置时隙参数 */
    slotCfg.rx_delay = 0;                        /* RX delay */
    slotCfg.md_agc = 1024;                       /* DATA AGC长度 */
    
    /* 配置时隙DAC参数 */
    /* S0时隙 (BCN) */
    slotCfg.s0Cfg[0].da_m = 0;        /* da0_m */
    
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
    
    /* 配置时隙 */
    slotCfg.s0Cfg[0].byteLen = 0;                  /* S0 (BCN) 时隙长度 */
    slotCfg.s0Cfg[0].centerFreq = 503100000;          /* S0中心频点 */
    
    slotCfg.s1Cfg[0].byteLen = 26;                 /* S1 (FDL) 时隙长度 */
    slotCfg.s1Cfg[0].centerFreq = 503100000;          /* S1中心频点 */
    
    slotCfg.s2Cfg[0].byteLen = 26;                 /* S2 (ADL) 时隙长度 */
    slotCfg.s2Cfg[0].centerFreq = 503100000;          /* S2中心频点 */
    
    slotCfg.s3Cfg[0].byteLen = 26;                 /* S3 (UL) 时隙长度 */
    slotCfg.s3Cfg[0].centerFreq = 503100000;          /* S3中心频点 */
    
    /* 计算帧时间长�?(由硬件自动计算，这里先设�?) */
    slotCfg.frameTimeLen = 0;
    
    /* 设置时隙配置 */
    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slotCfg);
    if (ret != TK8710_OK) {
        printf("Slot configuration failed: %d\n", ret);
        return ret;
    }
    
    printf("Slot parameter configuration completed\n");
    printf("Rate count: %d, Rate mode[0]: %d, Broadcast users: %d\n", 
           slotCfg.rateCount, slotCfg.rateModes[0], slotCfg.brdUserNum);
    printf("Slot length: S1=%d, S2=%d, S3=%d\n", 
           slotCfg.s1Cfg[0].byteLen, slotCfg.s2Cfg[0].byteLen, slotCfg.s3Cfg[0].byteLen);
    
    return TK8710_OK;
}

/**
 * @brief 启动工作
 * @return 0-成功, �?-失败
 */
int start_work(void)
{
    int ret;
    
    printf("Starting TK8710 work...\n");
    
    /* 启动主模式连续工�?*/
    ret = TK8710Start(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        printf("Start work failed: %d\n", ret);
        return ret;
    }
    
    printf("TK8710 work started (Master mode, Continuous work)\n");
    return TK8710_OK;
}

/*============================================================================
 * 主程�?
 *============================================================================*/

/**
 * @brief 显示帮助信息
 */
void show_help(void)
{
    printf("\n");
    printf("+--------------------------------------+\n");
    printf("|   TK8710 Test Program Help           |\n");
    printf("+--------------------------------------+\n");
    printf("Commands:\n");
    printf("  h/H - Show help information\n");
    printf("  s/S - Show system status\n");
    printf("  i/I - Show interrupt statistics\n");
    printf("  q/Q - Exit program\n");
    printf("  c/C - Clear screen\n");
    printf("+--------------------------------------+\n");
}

/**
 * @brief 显示系统状�?
 */
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
    
    /* 0. 初始化SPI接口 (RK3506 spidev) */
    printf("Initializing SPI interface (RK3506)...\n");
    SpiConfig spiCfg;
    spiCfg.speed = 16000000;  /* 16MHz */
    spiCfg.mode = 0;         /* Mode 0 */
    spiCfg.bits = 8;         /* 8位数�?*/
    spiCfg.lsb_first = 0;    /* MSB优先 */
    spiCfg.cs_pin = 0;       /* CS引脚0 */
    
    ret = TK8710SpiInit(&spiCfg);
    if (ret != 0) {
        printf("SPI initialization failed: %d\n", ret);
        printf("Please check:\n");
        printf("  1. /dev/spidev0.0 exists\n");
        printf("  2. SPI driver is loaded\n");
        printf("  3. User has permission to access SPI device\n");
        return -1;
    }
    printf("SPI initialized successfully (16MHz, Mode0, MSB)\n\n");
    
    /* 1. 初始化日志系�?*/
    printf("Initializing log system...\n");
    TK8710LogConfig(TK8710_LOG_ERROR, 0xFFFFFFFF, 0);//TK8710_LOG_INFO   TK8710_LOG_ALL  TK8710_LOG_NONE
    
    /* 2. 初始化中断系�?*/
    ret = init_interrupt_system();
    if (ret != TK8710_OK) {
        printf("Interrupt system initialization failed\n");
        return -1;
    }
    
    /* 3. 初始�?710芯片 */
    ret = init_tk8710_chip();
    if (ret != TK8710_OK) {
        printf("TK8710 chip initialization failed\n");
        return -1;
    }

    /* 4. 配置时隙参数 (TK8710_CFG_TYPE_SLOT_CFG) */
    ret = config_slot_parameters();
    if (ret != TK8710_OK) {
        printf("Slot parameter configuration failed\n");
        return -1;
    }

    /* 5. 初始化射频系�?*/
    ret = init_rf_system();
    if (ret != TK8710_OK) {
        printf("RF system initialization failed\n");
        return -1;
    }

    /* 6. 配置测试选项 */
    printf("Configure test options:\n");
    printf("Enable force process all users for testing? (y/n): ");
    char testChoice;
    scanf(" %c", &testChoice);
    if (testChoice == 'y' || testChoice == 'Y') {
        TK8710SetForceProcessAllUsers(1);
        printf("Force process all users: ENABLED\n");
    } else {
        TK8710SetForceProcessAllUsers(0);
        printf("Force process all users: DISABLED\n");
    }
    
    printf("Enable force max users TX for testing? (y/n): ");
    scanf(" %c", &testChoice);
    if (testChoice == 'y' || testChoice == 'Y') {
        TK8710SetForceMaxUsersTx(1);
        printf("Force max users TX: ENABLED\n");
    } else {
        TK8710SetForceMaxUsersTx(0);
        printf("Force max users TX: DISABLED\n");
    }

    /* 7. 启动工作 */
    ret = start_work();
    if (ret != TK8710_OK) {
        printf("Start work failed\n");
        return -1;
    }
    
    printf("\nSystem initialization completed, starting runtime...\n");
    printf("Enter 'h' for help information\n\n");
    
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
                
            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
                break;
                
            default:
                printf("Unknown command: %c (enter 'h' for help)\n", input);
                break;
        }
        
        /* 检查是否有中断发生 */
        uint32_t irqStatus = TK8710GetIrqStatus();
        if (irqStatus != 0) {
            printf("Interrupt detected: 0x%08X\n", irqStatus);
        }
        
//         /* 短暂延时，避免CPU占用过高 */
// #ifdef _WIN32
//         Sleep(1);
// #else
//         usleep(10);
// #endif
        
        // /* 每隔一段时间打印中断时间统�?*/
        // static uint32_t printCounter = 0;
        // printCounter++;
        // if (printCounter >= 1000) {
        //     printCounter = 0;
        //     TK8710PrintIrqTimeStats();
        // }
    }
    
    /* 8. 清理资源 */
    printf("Cleaning up resources...\n");
    
    /* 打印最终的中断时间统计报告 */
    printf("\n=== ?????????????===\n");
    TK8710PrintIrqTimeStats();
    
    /* 禁用中断 */
    TK8710GpioIrqEnable(0, 0);
    
    /* 重置芯片 */
    TK8710Reset(TK8710_RST_STATE_MACHINE);
    
    /* 清理RK3506资源 */
    TK8710Rk3506Cleanup();
    printf("RK3506 resources cleaned up\n");
    
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


static void app_gpio_irq_handler(void* user)
{
    (void)user;
    TK8710IrqResult irqResult;

    if (TK8710GetIrqResult(&irqResult) == TK8710_OK) {
        app_irq_handler(irqResult);
    }
}

#endif /* PLATFORM_TMS570 */
