/**
 * @file tk8710_config.c
 * @brief TK8710 配置管理实现
 */

#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_reg_pack.h"
#include "../inc/driver/tk8710_rf_regs.h"
#include "../inc/driver/tk8710_platform.h"
#include "driver/tk8710_log.h"
#include "../port/tk8710_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <string.h>
#if TK8710_PLATFORM_HAS_FILE_IO
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#endif
#endif

#if defined(PLATFORM_TMS570)
#define malloc(size) TK8710_MALLOC(size)
#define free(ptr)    TK8710_FREE(ptr)
#endif

/*============================================================================
 * 全局变量 (保存配置状态)
 *============================================================================*/

/* 时隙配置全局变量 - 默认配置参考test_Driver_TRM_main_3506.c */
static slotCfg_t g_slotCfg = {
    /* 基本参数 */
    .msMode = TK8710_MODE_MASTER,        /* 主模式 */
    .plCrcEn = 0,                         /* 使能CRC校验 */
    .rateCount = 1,                       /* 速率个数，设置为1个速率模式 */
    .rateModes = {TK8710_RATE_MODE_8},    /* 第一个速率模式 */
    .brdUserNum = 1,                      /* 广播用户数 */
    .antEn = 0xFF,                        /* 使能所有天线 */
    .rfSel = 0xFF,                        /* 选择所有RF */
    .txBeamCtrlMode = 1,                      /* 外部指定信息控制模式 */
    .txBcnAntEn = 0xff,                      /* 发送BCN天线使能 */
    
    /* BCN轮流发送配置 */
    .bcnRotation = {0, 1, 2, 3, 4, 5, 6, 7},  /* 轮流使用所有天线 */
    
    /* 广播频率 */
    .brdFreq = {20000.0},                 /* 广播用户0频率: 503.1 MHz */
    
    /* 时隙参数 */
    .rx_delay = 0,                        /* RX delay */
    .md_agc = 1024,                       /* DATA AGC长度 */
    
    /* 时隙DAC参数 */
    .s0Cfg = {
        {
            .da_m = 0,                    /* S0 (BCN) da0_m */
            .byteLen = 0,                 /* S0 (BCN) 时隙长度 */
            .centerFreq = 503100000       /* S0中心频点 */
        }
    },
    .s1Cfg = {
        {
            .da_m = 5600,                 /* S1 (FDL) da1_m - RATE_MODE_8 */
            .byteLen = 26,                /* S1 (FDL) 时隙长度 */
            .centerFreq = 503100000       /* S1中心频点 */
        }
    },
    .s2Cfg = {
        {
            .da_m = 5600,                 /* S2 (ADL) da2_m - RATE_MODE_8 */
            .byteLen = 26,                /* S2 (ADL) 时隙长度 */
            .centerFreq = 503100000       /* S2中心频点 */
        }
    },
    .s3Cfg = {
        {
            .da_m = 5600,                 /* S3 (UL) da3_m - RATE_MODE_8 */
            .byteLen = 26,                /* S3 (UL) 时隙长度 */
            .centerFreq = 503100000       /* S3中心频点 */
        }
    },
    
    /* 帧时间长度 (由硬件自动计算) */
    .frameTimeLen = 0
};

/* ACM校准增益 */
static uint16_t g_acm_dgain0[16] = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0};
static uint16_t g_acm_dgain1[16] = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0};
static uint16_t g_acm_again0[16] = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0};
static uint16_t g_acm_again1[16] = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,0};
static AcmCalibrationFactors g_lastAcmCalibrationFactors;
static uint8_t g_lastAcmCalibrationFactorsValid = 0U;

/*============================================================================
 * ACM校准内部辅助函数
 *============================================================================*/

/**
 * @brief 触发ACM校准
 */
static void tk8710_acm_trig(void)
{
    uint32_t acm_ctrl_data = TK8710_S_ACM_CTRL_ENCODE(1U);
    TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
        MAC_BASE + offsetof(struct mac, acm_ctrl), acm_ctrl_data);
}

/* 前向声明 */
static int tk8710_acm_get_snr(int* TmpSNR, uint8_t snrThreshold);

/* 寄存器测试函数声明 */
static void test_rx_fe_regs(void);
static void test_mac_regs(void);
static void test_tx_fe_regs(void);
static void test_rx_bcn_regs(void);
static void test_tx_mod_regs(void);
static void test_rx_mup_regs(void);
static void test_acm_regs(void);
static void test_rf_freq_regs(void);
static int g_regRwTestErrorCount = 0;

/*============================================================================
 * ACM校准内部函数
 *============================================================================*/

/**
 * @brief ACM校准步骤1: 自动获取校准增益
 * @return 0-成功, 非0-失败
 */
static int tk8710_acm_get_gain(void)
{
    int ret;
    int i, j, i1, i2;
    int SumSNR_0[32], SumSNR_1[32], TmpSNR[32], MaxSNR[32];
    int SNR_THE = 32;
    int GainStep = 4;
    uint32_t regVal;
    s_irq_ctrl0 irqCtrl0;
    
    /* 校准信号频点和相位配置数据 */
    uint16_t freqPose[16] = {0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0};
    uint16_t dphase[16] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0};
    uint16_t aphase[16] = {0xa6,0x1a4,0x1c6,0x3c4,0x32e,0x2a4,0xa4,0x3de,0x1ec,0x368,0x182,0x3ec,0xe,0x358,0x3c4,0};
    uint16_t dtone_en[8] = {0x2aab,0x4,0x10,0x40,0x100,0x400,0x1000,0x4000};
    uint16_t atone_en = 0x7fff;
    
    /* 初始化SNR数组 */
    memset((void*)SumSNR_0, 0, sizeof(SumSNR_0));
    memset((void*)MaxSNR, 0, sizeof(MaxSNR));
    
    /* 重置增益到初始值 */
    for (i = 0; i < 16; i++) {
        g_acm_dgain0[i] = 16;
        g_acm_dgain1[i] = 16;
        g_acm_again0[i] = 16;
        g_acm_again1[i] = 16;
    }
    g_acm_dgain0[15] = 0;
    g_acm_dgain1[15] = 0;
    g_acm_again0[15] = 0;
    g_acm_again1[15] = 0;
    
    /* 步骤1: 关闭PA (mac.init_10, 值1<<3) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), (1 << 3));
    if (ret != TK8710_OK) return ret;
    
    /* 步骤2: 关闭LNA (0x9478=0x10100010, mac.init_10=0x1a) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x10100010);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), 0x1a);
    if (ret != TK8710_OK) return ret;
    
    /* 步骤3: 设置校准信号频率 (acm_ctrl1) */
    for (i = 0; i < 8; i++) {
        regVal = (freqPose[i*2] << 16) | freqPose[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl1) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤4: 设置数据通道校准信号相位 (acm_ctrl25) */
    for (i = 0; i < 8; i++) {
        regVal = (dphase[i*2] << 16) | dphase[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl25) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤5: 设置数据通道校准信号tone使能 (acm_ctrl41) */
    for (i = 0; i < 4; i++) {
        regVal = (dtone_en[i*2] << 16) | dtone_en[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl41) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤6: 设置ACM通道校准信号相位 (acm_ctrl33) */
    for (i = 0; i < 8; i++) {
        regVal = (aphase[i*2] << 16) | aphase[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl33) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤7: 设置ACM通道校准信号tone使能 (acm_ctrl45) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_ctrl45), atone_en);
    if (ret != TK8710_OK) return ret;
    
    /* 步骤8: 设置ACM中断使能 (irq_ctrl0.acm_irq_mask = 0) */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), &regVal);
    if (ret != TK8710_OK) return ret;
    irqCtrl0.data = regVal;
    irqCtrl0.data = TK8710_S_IRQ_CTRL0_ACM_IRQ_MASK_SET(irqCtrl0.data, 0U);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
    if (ret != TK8710_OK) return ret;
    
    /* 步骤9: 校准增益自动获取算法 */
    for (i = 1; i < 16; i++) {
        memset((void*)SumSNR_1, 0, sizeof(SumSNR_1));
        
        for (j = 0; j < 5; j++) {
            /* 设置acm_ctrl17~24 (again) 和 acm_ctrl9~16 (dgain) */
            for (i2 = 0; i2 < 8; i2++) {
                regVal = ((GainStep*i) << 24) | ((GainStep*i) << 16) | 
                         ((GainStep*i) << 8) | (GainStep*i);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                    ACM_BASE + offsetof(struct acm, acm_ctrl17) + 0x4 * i2, regVal);
                if (ret != TK8710_OK) return ret;
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                    ACM_BASE + offsetof(struct acm, acm_ctrl9) + 0x4 * i2, regVal);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 触发ACM */
            tk8710_acm_trig();
            
            /* 读取SNR */
            tk8710_acm_get_snr(TmpSNR, SNR_THE);
            
            /* 累加SNR */
            for (i2 = 0; i2 < 8; i2++) {
                SumSNR_1[i2 + 8*0] += TmpSNR[i2 + 8*0];
                SumSNR_1[i2 + 8*1] += TmpSNR[i2 + 8*1];
                SumSNR_1[i2 + 8*2] += TmpSNR[i2 + 8*2];
                SumSNR_1[i2 + 8*3] += TmpSNR[i2 + 8*3];
            }
            
            /* 只复位状态机 */
            TK8710SpiReset(1);
        }
        
        /* 比较并更新最佳增益 */
        for (i1 = 0; i1 < 8; i1++) {
            /* again0 */
            if (SumSNR_1[i1+8*0] > SumSNR_0[i1+8*0] && SumSNR_1[i1+8*0] > MaxSNR[i1+8*0] + 4) {
                if (i1 == 0) {
                    g_acm_again0[0] = g_acm_again0[1] = g_acm_again0[3] = g_acm_again0[5] = (GainStep*i);
                    g_acm_again0[7] = g_acm_again0[9] = g_acm_again0[11] = g_acm_again0[13] = (GainStep*i);
                } else {
                    g_acm_again0[i1*2] = (GainStep*i);
                }
                TK8710_LOG_CONFIG_DEBUG("Antenna = %d,again0 = %d,SumSNRTx0_last = %d,MaxSNRTx0 = %d\n",i1,(GainStep*i),SumSNR_0[i1+8*0],SumSNR_1[i1+8*0]);
                MaxSNR[i1+8*0] = SumSNR_1[i1+8*0];
            }
            /* dgain0 */
            if (SumSNR_1[i1+8*1] > SumSNR_0[i1+8*1] && SumSNR_1[i1+8*1] > MaxSNR[i1+8*1] + 4) {
                if (i1 == 0) {
                    g_acm_dgain0[0] = g_acm_dgain0[1] = g_acm_dgain0[3] = g_acm_dgain0[5] = (GainStep*i);
                    g_acm_dgain0[7] = g_acm_dgain0[9] = g_acm_dgain0[11] = g_acm_dgain0[13] = (GainStep*i);
                } else {
                    g_acm_dgain0[i1*2] = (GainStep*i);
                }
                TK8710_LOG_CONFIG_DEBUG("Antenna = %d,dgain0 = %d,SumSNRRx0_last = %d,MaxSNRRx0 = %d\n",i1,(GainStep*i),SumSNR_0[i1+8*1],SumSNR_1[i1+8*1]);
                MaxSNR[i1+8*1] = SumSNR_1[i1+8*1];
            }
            /* again1 */
            if (SumSNR_1[i1+8*2] > SumSNR_0[i1+8*2] && SumSNR_1[i1+8*2] > MaxSNR[i1+8*2] + 4) {
                if (i1 == 0) {
                    g_acm_again1[0] = g_acm_again1[1] = g_acm_again1[3] = g_acm_again1[5] = (GainStep*i);
                    g_acm_again1[7] = g_acm_again1[9] = g_acm_again1[11] = g_acm_again1[13] = g_acm_again1[15] = (GainStep*i);
                } else {
                    g_acm_again1[i1*2] = (GainStep*i);
                }
                TK8710_LOG_CONFIG_DEBUG("Antenna = %d,again1 = %d,SumSNRTx1_last = %d,MaxSNRTx1 = %d\n",i1,(GainStep*i),SumSNR_0[i1+8*2],SumSNR_1[i1+8*2]);
                MaxSNR[i1+8*2] = SumSNR_1[i1+8*2];
            }
            /* dgain1 */
            if (SumSNR_1[i1+8*3] > SumSNR_0[i1+8*3] && SumSNR_1[i1+8*3] > MaxSNR[i1+8*3] + 4) {
                if (i1 == 0) {
                    g_acm_dgain1[0] = g_acm_dgain1[1] = g_acm_dgain1[3] = g_acm_dgain1[5] = (GainStep*i);
                    g_acm_dgain1[7] = g_acm_dgain1[9] = g_acm_dgain1[11] = g_acm_dgain1[13] = g_acm_dgain1[15] = (GainStep*i);
                } else {
                    g_acm_dgain1[i1*2] = (GainStep*i);
                }
                TK8710_LOG_CONFIG_DEBUG("Antenna = %d,dgain1 = %d,SumSNRRx1_last = %d,MaxSNRRx1 = %d\n",i1,(GainStep*i),SumSNR_0[i1+8*3],SumSNR_1[i1+8*3]);
                MaxSNR[i1+8*3] = SumSNR_1[i1+8*3];
            }
            
            /* 保存当前SNR作为下一轮比较基准 */
            SumSNR_0[i1+8*0] = SumSNR_1[i1+8*0];
            SumSNR_0[i1+8*1] = SumSNR_1[i1+8*1];
            SumSNR_0[i1+8*2] = SumSNR_1[i1+8*2];
            SumSNR_0[i1+8*3] = SumSNR_1[i1+8*3];
        }
    }
    
    /* 计算最终增益 (dgain和again的平均值) */
    for (i = 0; i < 15; i++) {
        uint16_t Tmp0, Tmp1;
        if (i == 12) {  /* i==2*6 */
            Tmp0 = (g_acm_dgain0[i] + g_acm_again0[i] * 0);
            Tmp1 = (g_acm_dgain1[i] + g_acm_again1[i]) / 2;
        } else if (i == 14) {  /* i==2*7 */
            Tmp0 = (g_acm_dgain0[i] + g_acm_again0[i]) / 2;
            Tmp1 = (g_acm_dgain1[i] + g_acm_again1[i] * 0);
        } else {
            Tmp0 = (g_acm_dgain0[i] + g_acm_again0[i]) / 2;
            Tmp1 = (g_acm_dgain1[i] + g_acm_again1[i]) / 2;
        }
        g_acm_dgain0[i] = Tmp0;
        g_acm_again0[i] = Tmp0;
        g_acm_dgain1[i] = Tmp1;
        g_acm_again1[i] = Tmp1;
        TK8710_LOG_CONFIG_INFO("i = %d,Tmp0 = %d,Tmp1 = %d\n",i,Tmp0,Tmp1);
    }
    
    /* 步骤10: 增益更新 (写入acm_ctrl9和acm_ctrl17) */
    for (i = 0; i < 8; i++) {
        regVal = ((uint32_t)g_acm_dgain0[i*2] << 16) | g_acm_dgain0[i*2+1] |
                 ((uint32_t)g_acm_dgain1[i*2] << 24) | ((uint32_t)g_acm_dgain1[i*2+1] << 8);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl9) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    for (i = 0; i < 8; i++) {
        regVal = ((uint32_t)g_acm_again0[i*2] << 16) | g_acm_again0[i*2+1] |
                 ((uint32_t)g_acm_again1[i*2] << 24) | ((uint32_t)g_acm_again1[i*2+1] << 8);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl17) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 关闭ACM中断使能 (irq_ctrl0.acm_irq_mask = 1) */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), &regVal);
    if (ret != TK8710_OK) return ret;
    irqCtrl0.data = regVal;
    irqCtrl0.data = TK8710_S_IRQ_CTRL0_ACM_IRQ_MASK_SET(irqCtrl0.data, 1U);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
    if (ret != TK8710_OK) return ret;
    return TK8710_OK;
}

/**
 * @brief ACM校准步骤2: 获取校准信号SNR
 * @param TmpSNR SNR输出缓冲区 (32个int)
 * @param snrThreshold SNR门限值
 * @return SNR大于门限的天线个数
 */
static int tk8710_acm_get_snr(int* TmpSNR, uint8_t snrThreshold)
{
    s_irq_res irq_res;
    s_irq_ctrl1 irq_ctrl1;
    uint32_t value_tmp1, value_tmp2, value_tmp3, value_tmp4;
    uint32_t value_tmp5, value_tmp6, value_tmp7, value_tmp8;
    int8_t SNR_tx0[8], SNR_rx0[8], SNR_tx1[8], SNR_rx1[8];
    int i;
    int SNRNum = 0;
    
    /* 等待ACM中断 (bit9)，添加100ms超时机制 */
#if defined(PLATFORM_TMS570)
    uint64_t start_time = TK8710GetTimeUs();
    uint64_t timeout_us = 100000ULL;
#elif defined(_WIN32)
    DWORD start_time = GetTickCount();
    DWORD timeout = 100; /* 100ms超时 */
#else
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    long timeout_ms = 100; /* 100ms超时 */
#endif

    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_res), &irq_res.data);
    while ((TK8710_S_IRQ_RES_IRQ_TYPE_GET(irq_res.data) & (1 << TK8710_IRQ_ACM)) == 0) {
#if defined(PLATFORM_TMS570)
        if ((TK8710GetTimeUs() - start_time) >= timeout_us) {
            TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, "ACM IRQ wait timeout (100ms), exit");
            break;
        }
#elif defined(_WIN32)
        DWORD current_time = GetTickCount();
        if ((current_time - start_time) >= timeout) {
            TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, "ACM IRQ wait timeout (100ms), exit");
            break;
        }
#else
        gettimeofday(&current_time, NULL);
        long elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
        if (elapsed_ms >= timeout_ms) {
            TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, "ACM IRQ wait timeout (100ms), exit");
            break;
        }
#endif
        TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
            MAC_BASE + offsetof(struct mac, irq_res), &irq_res.data);
    }
    
    /* 清除ACM中断 */
    irq_ctrl1.data = TK8710_S_IRQ_CTRL1_ACM_IRQ_CLR_ENCODE(1U);
    TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl1), irq_ctrl1.data);
    
    // /* 延时30ms */
    // TK8710DelayMs(30);
    
    /* 读取acm_obv17~24获取SNR值 */
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv18), &value_tmp1);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv17), &value_tmp2);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv20), &value_tmp3);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv19), &value_tmp4);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv22), &value_tmp5);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv21), &value_tmp6);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv24), &value_tmp7);
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_obv23), &value_tmp8);
    
    /* 解析SNR值到各个天线 */
    for (i = 0; i < 4; i++) {
        SNR_tx0[i]   = ((value_tmp1 >> (i*8)) & 0xff);
        SNR_tx0[i+4] = ((value_tmp2 >> (i*8)) & 0xff);
        SNR_rx0[i]   = ((value_tmp3 >> (i*8)) & 0xff);
        SNR_rx0[i+4] = ((value_tmp4 >> (i*8)) & 0xff);
        SNR_tx1[i]   = ((value_tmp5 >> (i*8)) & 0xff);
        SNR_tx1[i+4] = ((value_tmp6 >> (i*8)) & 0xff);
        SNR_rx1[i]   = ((value_tmp7 >> (i*8)) & 0xff);
        SNR_rx1[i+4] = ((value_tmp8 >> (i*8)) & 0xff);
        
        /* 填充TmpSNR数组 */
        TmpSNR[i+8*0] = SNR_tx0[i];
        TmpSNR[i+4+8*0] = SNR_tx0[i+4];
        TmpSNR[i+8*1] = SNR_rx0[i];
        TmpSNR[i+4+8*1] = SNR_rx0[i+4];
        TmpSNR[i+8*2] = SNR_tx1[i];
        TmpSNR[i+4+8*2] = SNR_tx1[i+4];
        TmpSNR[i+8*3] = SNR_rx1[i];
        TmpSNR[i+4+8*3] = SNR_rx1[i+4];
    }
    
    /* 比较SNR与门限，统计通过个数 */
    for (i = 0; i < 8; i++) {
        if ((SNR_tx0[i] >= snrThreshold) && (SNR_tx0[i] < 60)) {
            SNRNum++;
        }
        if ((SNR_rx0[i] >= snrThreshold) && (SNR_rx0[i] < 60)) {
            SNRNum++;
        }
        if ((SNR_tx1[i] >= snrThreshold) && (SNR_tx1[i] < 60)) {
            SNRNum++;
        }
        if ((SNR_rx1[i] >= snrThreshold) && (SNR_rx1[i] < 60)) {
            SNRNum++;
        }
    }
    
    /* 打印SNR值 */
    TK8710_LOG_CONFIG_INFO("=== ACM SNR values ===\n");
    for (i = 0; i < 8; i++) {
        TK8710_LOG_CONFIG_INFO("Antenna%d: TX0_SNR=%d, RX0_SNR=%d, TX1_SNR=%d, RX1_SNR=%d\n", 
                            i, SNR_tx0[i], SNR_rx0[i], SNR_tx1[i], SNR_rx1[i]);
    }
    TK8710_LOG_CONFIG_INFO("Valid SNR count: %d (threshold: %d)\n", SNRNum, snrThreshold);
    
    return SNRNum;
}

/**
 * @brief ACM校准步骤3: 执行校准流程
 * @param calibCount 连续校准次数
 * @param snrThreshold SNR门限值
 * @return 有效校准次数
 */
static int tk8710_acm_calibrate(uint8_t calibCount, uint8_t snrThreshold)
{
    int ret;
    int i;
    uint32_t regVal;
    s_irq_ctrl0 irqCtrl0;
    int TmpSNR[32];
    int TmpSNRNum;
    int validCount = 0;

    g_lastAcmCalibrationFactorsValid = 0U;
    
    /* 校准信号频点和相位配置数据 (与tk8710_acm_get_gain相同) */
    uint16_t freqPose[16] = {0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0};
    uint16_t dphase[16] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0};
    uint16_t aphase[16] = {0xa6,0x1a4,0x1c6,0x3c4,0x32e,0x2a4,0xa4,0x3de,0x1ec,0x368,0x182,0x3ec,0xe,0x358,0x3c4,0};
    uint16_t dtone_en[8] = {0x2aab,0x4,0x10,0x40,0x100,0x400,0x1000,0x4000};
    uint16_t atone_en = 0x7fff;
    
    /* 步骤1: 关闭PA (mac.init_10, 值1<<3) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), (1 << 3));
    if (ret != TK8710_OK) return ret;
    
    /* 步骤2: 关闭LNA (0x9478=0x10100010, mac.init_10=0x1a) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x10100010);//0x10100010
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), 0x1a);
    if (ret != TK8710_OK) return ret;
    
    /* 步骤3: 设置校准信号频率 (acm_ctrl1) */
    for (i = 0; i < 8; i++) {
        regVal = (freqPose[i*2] << 16) | freqPose[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl1) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤4: 设置数据通道校准信号相位 (acm_ctrl25) */
    for (i = 0; i < 8; i++) {
        regVal = (dphase[i*2] << 16) | dphase[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl25) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤5: 设置数据通道校准信号tone使能 (acm_ctrl41) */
    for (i = 0; i < 4; i++) {
        regVal = (dtone_en[i*2] << 16) | dtone_en[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl41) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤6: 设置ACM通道校准信号相位 (acm_ctrl33) */
    for (i = 0; i < 8; i++) {
        regVal = (aphase[i*2] << 16) | aphase[i*2+1];
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
            ACM_BASE + offsetof(struct acm, acm_ctrl33) + 0x4 * i, regVal);
        if (ret != TK8710_OK) return ret;
    }
    
    /* 步骤7: 设置ACM通道校准信号tone使能 (acm_ctrl45) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        ACM_BASE + offsetof(struct acm, acm_ctrl45), atone_en);
    if (ret != TK8710_OK) return ret;
    
    /* 步骤8: 设置ACM中断使能 (irq_ctrl0.acm_irq_mask = 0) */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), &regVal);
    if (ret != TK8710_OK) return ret;
    irqCtrl0.data = regVal;
    irqCtrl0.data = TK8710_S_IRQ_CTRL0_ACM_IRQ_MASK_SET(irqCtrl0.data, 0U);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
    if (ret != TK8710_OK) return ret;
    
    /* 循环执行校准 */
    for (i = 0; i < calibCount; i++) {
        /* 步骤9: 复位状态机 */
        TK8710SpiReset(1);
        
        /* 步骤10: 触发校准、获取SNR */
        tk8710_acm_trig();
        TmpSNRNum = tk8710_acm_get_snr(TmpSNR, snrThreshold);
        
        /* 步骤11: 判断满足SNR门限个数是否大于25 */
        if (TmpSNRNum >= 25) {
            /* 获取ACM校准因子 */
            AcmCalibrationFactors calFactors;
            ret = TK8710GetAcmCalibrationFactors(&calFactors);
            if (ret != TK8710_OK) return ret;
            g_lastAcmCalibrationFactors = calFactors;
            g_lastAcmCalibrationFactorsValid = 1U;
            validCount++;
        }
    }
    
    /* 关闭ACM中断使能 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), &regVal);
    if (ret != TK8710_OK) return ret;
    irqCtrl0.data = regVal;
    irqCtrl0.data = TK8710_S_IRQ_CTRL0_ACM_IRQ_MASK_SET(irqCtrl0.data, 1U);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
    if (ret != TK8710_OK) return ret;
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), (1 << 3));
    if (ret != TK8710_OK) return ret;
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x11100018);
    if (ret != TK8710_OK) return ret;

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, init_10), (1 << 2));
    if (ret != TK8710_OK) return ret;

    return validCount;
}


/**
 * @brief 获取ACM校准因子
 * @param calFactors 输出校准因子结构体指针
 * @return 0-成功, 1-失败
 */
int TK8710GetAcmCalibrationFactors(AcmCalibrationFactors* calFactors)
{
    int ret;
    uint32_t regVal;
    int i;
    
    if (calFactors == NULL) {
        return TK8710_ERR_PARAM;
    }

    /* 读取8个射频通道的I路和Q路校准因子 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv1), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[0].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv2), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[0].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv3), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[1].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv4), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[1].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv5), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[2].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv6), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[2].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv7), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[3].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv8), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[3].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv9), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[4].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv10), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[4].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv11), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[5].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv12), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[5].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv13), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[6].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv14), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[6].q_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv15), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[7].i_factor = regVal;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,ACM_BASE + offsetof(struct acm, acm_obv16), &regVal);
    if (ret != TK8710_OK) return ret;
    calFactors->channels[7].q_factor = regVal;
    /* 打印校准因子 */
//    TK8710_LOG_CONFIG_INFO("=== ACM calibration factors ===\n");
    TK8710_LOG_CONFIG_WARN("=== ACM calibration factors ===\n");
    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        /* 解析18位校准因子：使用补码转换 */
        uint32_t i_factor = calFactors->channels[i].i_factor & 0x3FFFF;  // 18位掩码
        uint32_t q_factor = calFactors->channels[i].q_factor & 0x3FFFF;  // 18位掩码
        
        /* 转换为浮点数 - 参考MATLAB实现 */
        float i_float, q_float;
        
        /* I路转换：18位补码转浮点 */
        int32_t i_signed = (int32_t)(i_factor << 14) >> 14;  // 符号位扩展到32位
        i_float = (float)i_signed / 32768.0f;
        
        /* Q路转换：18位补码转浮点 */
        int32_t q_signed = (int32_t)(q_factor << 14) >> 14;  // 符号位扩展到32位
        q_float = (float)q_signed / 32768.0f;
        
//        TK8710_LOG_CONFIG_INFO("Channel%d: I=0x%08X (%.3f), Q=0x%08X (%.3f)\n",
//                            i, i_factor, i_float, q_factor, q_float);
        TK8710_LOG_CONFIG_WARN("Channel%d: I=0x%08X (%.3f), Q=0x%08X (%.3f)\n",
                            i, i_factor, i_float, q_factor, q_float);
    }
    
    /* 保存校准因子到文件 - TXT格式 */
    {
#if TK8710_PLATFORM_HAS_FILE_IO
        FILE* outputFile;
        char filename[256];
        time_t rawtime;
        struct tm* timeinfo;
        
        /* 创建CaliFactor目录 */
        #ifdef _WIN32
            _mkdir("CaliFactor");
        #else
            mkdir("CaliFactor", 0755);
        #endif
        
        /* 获取当前时间用于记录 */
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        
        /* 使用固定文件名，以文本追加模式打开 */
        snprintf(filename, sizeof(filename), "CaliFactor/CaliFactor.txt");
        outputFile = fopen(filename, "a");  // "a"模式：追加文本写入
        if (outputFile == NULL) {
            TK8710_LOG_CONFIG_ERROR("Open calibration factor file %s failed\n", filename);
            return TK8710_ERR;
        }
        
        /* 写入时间戳 */
        fprintf(outputFile, "\n=== %04d-%02d-%02d %02d:%02d:%02d ===\n",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        
        /* 写入校准因子数据为文本格式 */
        fprintf(outputFile, "ACM calibration factor data:\n");
        for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
            /* 获取原始校准因子 */
            uint32_t i_factor = calFactors->channels[i].i_factor & 0x3FFFF;  // 18位掩码
            uint32_t q_factor = calFactors->channels[i].q_factor & 0x3FFFF;  // 18位掩码
            
            fprintf(outputFile, "Channel[%d]: I_factor=0x%05X, Q_factor=0x%05X\n",
                    i, i_factor, q_factor);
        }
        
        fclose(outputFile);
        TK8710_LOG_CONFIG_INFO("ACM calibration factors appended to TXT file: %s\n", filename);
#endif
    }
    
    return TK8710_OK;
}

int TK8710GetLastAcmCalibrationFactors(AcmCalibrationFactors* calFactors)
{
    if ((calFactors == NULL) || (g_lastAcmCalibrationFactorsValid == 0U)) {
        return TK8710_ERR_STATE;
    }
    *calFactors = g_lastAcmCalibrationFactors;
    return TK8710_OK;
}


/*============================================================================
 * 全局配置访问函数
 *============================================================================*/

/**
 * @brief 获取时隙配置
 * @return 时隙配置指针
 */
const slotCfg_t* TK8710GetSlotConfig(void)
{
    return &g_slotCfg;
}

/**
 * @brief 获取下行发送模式
 * @return 0=自动发送, 1=指定信息发送
 */
uint8_t TK8710GetTxBeamCtrlMode(void)
{
    return g_slotCfg.txBeamCtrlMode;
}

/*============================================================================
 * 配置接口实现
 *============================================================================*/

/**
 * @brief 设置芯片配置
 * @param type 配置类型:
 *        - TK8710_CFG_TYPE_CHIP_INFO: 配置芯片参数 (ChipConfig)
 *        - TK8710_CFG_TYPE_SLOT_CFG: 配置时隙参数 (slotCfg_t)
 *        - TK8710_CFG_TYPE_ADDTL: 配置附加位参数 (addtlCfg_u)
 * @param params 配置参数指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710SetConfig(TK8710ConfigType type, const void* params)
{
    if (params == NULL) {
        TK8710_LOG_CONFIG_ERROR("Config params is NULL for type %d", type);
        return TK8710_ERR;
    }
    
    switch (type) {
        case TK8710_CFG_TYPE_CHIP_INFO:
            /* 配置芯片参数，调用TK8710Init */
            return TK8710Init((const ChipConfig*)params);
        
        case TK8710_CFG_TYPE_SLOT_CFG:
        {
            /* 配置时隙参数 */
            slotCfg_t* slotCfg = (slotCfg_t*)params;
            int ret;
            uint32_t macConfig0Data;
            uint32_t macConfig1Data;
            
            if (slotCfg == NULL) {
                return TK8710_ERR;
            }
            
            /* 配置mac_config_0 (0x00): s0_cfg, s1_cfg, mode */
            macConfig0Data = TK8710_S_MAC_CONFIG_0_ENCODE(0U,
                                                          slotCfg->s1Cfg[0].byteLen,
                                                          slotCfg->rateModes[0]);
            TK8710_LOG_CONFIG_INFO("Write mac_config_0: s0_cfg=%u, s1_cfg=%u, mode=%u, data=0x%08lX",
                                   0U,
                                   (unsigned int)(slotCfg->s1Cfg[0].byteLen & 0x3FFU),
                                   (unsigned int)(slotCfg->rateModes[0] & 0xFFU),
                                   (unsigned long)macConfig0Data);
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_0), macConfig0Data);
            if (ret != TK8710_OK) return ret;
            {
                uint32_t regReadback = 0U;
                ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, mac_config_0), &regReadback);
                if (ret != TK8710_OK) return ret;
                TK8710_LOG_CONFIG_INFO("Readback mac_config_0: 0x%08lX",
                                       (unsigned long)regReadback);
            }
            
            /* 配置mac_config_1 (0x04): s2_cfg, s3_cfg, pl_crc_en */
            macConfig1Data = TK8710_S_MAC_CONFIG_1_ENCODE(slotCfg->s2Cfg[0].byteLen,
                                                          slotCfg->s3Cfg[0].byteLen,
                                                          slotCfg->plCrcEn);
            TK8710_LOG_CONFIG_INFO("Write mac_config_1: s2_cfg=%u, s3_cfg=%u, pl_crc_en=%u, data=0x%08lX",
                                   (unsigned int)(slotCfg->s2Cfg[0].byteLen & 0x3FFU),
                                   (unsigned int)(slotCfg->s3Cfg[0].byteLen & 0x3FFU),
                                   (unsigned int)(slotCfg->plCrcEn & 0x01U),
                                   (unsigned long)macConfig1Data);
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_1), macConfig1Data);
            if (ret != TK8710_OK) return ret;
            {
                uint32_t regReadback = 0U;
                ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, mac_config_1), &regReadback);
                if (ret != TK8710_OK) return ret;
                TK8710_LOG_CONFIG_INFO("Readback mac_config_1: 0x%08lX",
                                       (unsigned long)regReadback);
            }
            
            /* 配置 init_1: md_agc, rx_delay */
            {
                uint32_t init1Data = TK8710_S_INIT_1_ENCODE(slotCfg->md_agc, slotCfg->rx_delay);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_1), init1Data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_2: da1_m */
            {
                uint32_t init2Data = TK8710_S_INIT_2_ENCODE(slotCfg->s1Cfg[0].da_m);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_2), init2Data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_3: da2_m, tx_fix_freq */
            {
                uint32_t init3Data = TK8710_S_INIT_3_ENCODE(slotCfg->s2Cfg[0].da_m, slotCfg->txBeamCtrlMode, 0U);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_3), init3Data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_4: da3_m */
            {
                uint32_t init4Data = TK8710_S_INIT_4_ENCODE(slotCfg->s3Cfg[0].da_m);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_4), init4Data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_18: da0_m (完整24位) */
            {
                uint32_t init18Data = TK8710_S_INIT_18_ENCODE(slotCfg->s0Cfg[0].da_m);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_18), init18Data);
                if (ret != TK8710_OK) return ret;
                
                /* 配置0x9810寄存器，根据速率模式设置不同的值 */
                {
                    uint32_t regValue;
                    uint8_t mode = slotCfg->rateModes[0] & 0xFF;
                    
                    if (mode <= TK8710_RATE_MODE_8) {
                        regValue = 0x03381400;
                    } else if (mode == TK8710_RATE_MODE_9) {
                        regValue = 0x03201400;
                    } else if (mode == TK8710_RATE_MODE_10) {
                        regValue = 0x03081400;
                        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9814, 0x0e2d140a);
                    } else if (mode == TK8710_RATE_MODE_11 || mode == TK8710_RATE_MODE_18) {
                        regValue = 0x02f01400;
                    } else {
                        regValue = 0x03381400; /* 默认值 */
                    }
                    
                    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9810, regValue);
                    if (ret != TK8710_OK) return ret;
                    
                    TK8710_LOG_CONFIG_INFO("Set register 0x9810 = 0x%08X for mode %d", regValue, mode);
                }
            }
            
            /* 保存时隙配置到全局变量 (包含brdUserNum) */
            memcpy(&g_slotCfg, slotCfg, sizeof(slotCfg_t));
            
            TK8710_LOG_CONFIG_INFO("Slot config updated: rateCount=%d, rateModes[0]=%d, brdUserNum=%d, txBeamCtrlMode=%d", 
                                 slotCfg->rateCount, slotCfg->rateModes[0], slotCfg->brdUserNum, slotCfg->txBeamCtrlMode);
            
            /* 从寄存器读取时隙时间长度并更新 */
            {
                uint32_t regVal;
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_1), &regVal);
                g_slotCfg.s0Cfg[0].timeLen = TK8710_S_OBV_1_S0_LEN_GET(regVal);
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_2), &regVal);
                g_slotCfg.s1Cfg[0].timeLen = TK8710_S_OBV_2_S1_LEN_GET(regVal);
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_3), &regVal);
                g_slotCfg.s2Cfg[0].timeLen = TK8710_S_OBV_3_S2_LEN_GET(regVal);
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_4), &regVal);
                g_slotCfg.s3Cfg[0].timeLen = TK8710_S_OBV_4_S3_LEN_GET(regVal);
                
                /* 计算帧总时间 */
                g_slotCfg.frameTimeLen = g_slotCfg.s0Cfg[0].timeLen + 
                    g_slotCfg.s1Cfg[0].timeLen + g_slotCfg.s2Cfg[0].timeLen + 
                    g_slotCfg.s3Cfg[0].timeLen;
                
                /* 打印每个slot的时间长度和帧总时间 */
                TK8710_LOG_CONFIG_WARN("Slot time lengths - S0:%d, S1:%d, S2:%d, S3:%d (us)",
                    g_slotCfg.s0Cfg[0].timeLen, g_slotCfg.s1Cfg[0].timeLen, 
                    g_slotCfg.s2Cfg[0].timeLen, g_slotCfg.s3Cfg[0].timeLen);
                TK8710_LOG_CONFIG_WARN("Frame total time length: %d (us)", g_slotCfg.frameTimeLen);              
            }
            
            return TK8710_OK;
        }
        
        case TK8710_CFG_TYPE_ADDTL:
            /* TODO: 待实现 */
            return TK8710_OK;
        
        default:
            return TK8710_ERR;
    }
}

/**
 * @brief 获取芯片配置
 * @param type 配置类型:
 *        - TK8710_CFG_TYPE_CHIP_INFO: 获取芯片参数 (ChipConfig)
 *        - TK8710_CFG_TYPE_SLOT_CFG: 获取时隙参数 (slotCfg_t)
 *        - TK8710_CFG_TYPE_ADDTL: 获取附加位参数 (addtlCfg_u)
 * @param params 配置参数输出指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710GetConfig(TK8710ConfigType type, void* params)
{
    int ret;
    
    if (params == NULL) {
        return TK8710_ERR;
    }
    
    switch (type) {
        case TK8710_CFG_TYPE_CHIP_INFO:
        {
            ChipConfig* cfg = (ChipConfig*)params;
            s_init_0 init0;
            s_init_1 init1;
            s_init_5 init5;
            s_init_9 init9;
            s_init_11 init11;
            s_irq_ctrl1 irqCtrl1;
            
            /* 读取 init_0 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_0), &init0.data);
            if (ret != TK8710_OK) return ret;
            cfg->bcn_agc = TK8710_S_INIT_0_BCN_AGC_GET(init0.data);
            cfg->interval = TK8710_S_INIT_0_INTERVAL_GET(init0.data);
            cfg->tx_dly = TK8710_S_INIT_0_TX_FREQ_DLY_GET(init0.data);  /* 保存到cfg */
            
            /* 读取 init_1 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_1), &init1.data);
            if (ret != TK8710_OK) return ret;
            /* md_agc字段已移至slotCfg_t */
            /* rx_delay字段已移至slotCfg_t */
            
            /* init_2, init_3, init_4, init_18 配置已移至slotCfg_t */
            
            /* 读取 init_5 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), &init5.data);
            if (ret != TK8710_OK) return ret;
            cfg->offset_adj = TK8710_S_INIT_5_OFFSET_ADJ_GET(init5.data);
            cfg->tx_pre = TK8710_S_INIT_5_TX_PRE_GET(init5.data);
            cfg->conti_mode = TK8710_S_INIT_5_CONTI_MODE_GET(init5.data);
            cfg->bcn_scan = TK8710_S_INIT_5_CONTI_SCAN_GET(init5.data);
            
            /* 读取 init_9 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), &init9.data);
            if (ret != TK8710_OK) return ret;
            cfg->ant_en = TK8710_S_INIT_9_ANT_EN_GET(init9.data);
            cfg->rf_sel = TK8710_S_INIT_9_RF_SEL_GET(init9.data);
            cfg->tx_bcn_en = TK8710_S_INIT_9_TX_BCN_ANT_EN_GET(init9.data);
            
            /* 读取 init_11 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), &init11.data);
            if (ret != TK8710_OK) return ret;
            cfg->rf_model = TK8710_S_INIT_11_RF_TYPE_GET(init11.data);
            
            /* 读取 irq_ctrl1 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl1), &irqCtrl1.data);
            if (ret != TK8710_OK) return ret;
            cfg->irq_ctrl1 = irqCtrl1.data;
            
            return TK8710_OK;
        }
        
        case TK8710_CFG_TYPE_SLOT_CFG:
        {
            /* 获取时隙配置 */
            slotCfg_t* slotCfg = (slotCfg_t*)params;
            s_obv_2 obv2;
            s_obv_3 obv3;
            s_obv_4 obv4;
            uint32_t regVal;
            
            if (slotCfg == NULL) {
                return TK8710_ERR;
            }
            
            /* 读取mac_config_0 (0x00): s0_cfg, s1_cfg, mode */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_0), &regVal);
            if (ret != TK8710_OK) return ret;
            slotCfg->brdUserNum = TK8710GetBrdUserNum();  /* 从全局变量获取广播用户数 */
            slotCfg->s1Cfg[0].byteLen = (uint16_t)TK8710_S_MAC_CONFIG_0_S1_CFG_GET(regVal);
            slotCfg->rateModes[0] = (rateMode_e)TK8710_S_MAC_CONFIG_0_MODE_GET(regVal);
            
            /* 读取mac_config_1 (0x04): s2_cfg, s3_cfg, pl_crc_en */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_1), &regVal);
            if (ret != TK8710_OK) return ret;
            slotCfg->s2Cfg[0].byteLen = (uint16_t)TK8710_S_MAC_CONFIG_1_S2_CFG_GET(regVal);
            slotCfg->s3Cfg[0].byteLen = (uint16_t)TK8710_S_MAC_CONFIG_1_S3_CFG_GET(regVal);
            slotCfg->plCrcEn = (uint8_t)TK8710_S_MAC_CONFIG_1_PL_CRC_EN_GET(regVal);
            
            /* 读取obv_1 (0xf4): s0_len - BCN时隙时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_1), &regVal);
            if (ret != TK8710_OK) return ret;
            /* s0为BCN时隙，暂不存储 */
            
            /* 读取obv_2 (0xf8): s1_len - 时隙1时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_2), &regVal);
            if (ret != TK8710_OK) return ret;
            obv2.data = regVal;
            slotCfg->s1Cfg[0].timeLen = TK8710_S_OBV_2_S1_LEN_GET(obv2.data);
            
            /* 读取obv_3 (0xfc): s2_len - 时隙2时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_3), &regVal);
            if (ret != TK8710_OK) return ret;
            obv3.data = regVal;
            slotCfg->s2Cfg[0].timeLen = TK8710_S_OBV_3_S2_LEN_GET(obv3.data);
            
            /* 读取obv_4 (0x100): s3_len - 时隙3时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_4), &regVal);
            if (ret != TK8710_OK) return ret;
            obv4.data = regVal;
            slotCfg->s3Cfg[0].timeLen = TK8710_S_OBV_4_S3_LEN_GET(obv4.data);
            
            /* 从全局状态获取主从模式 */
            slotCfg->msMode = TK8710GetWorkType();
            
            return TK8710_OK;
        }
        
        case TK8710_CFG_TYPE_ADDTL:
            /* TODO: 待实现 */
            return TK8710_OK;
        
        default:
            return TK8710_ERR;
    }
}

/**
 * @brief 芯片控制命令
 * @param type 控制类型:
 *        - TK8710_CTRL_TYPE_ACM_START: 触发ACM校准 (acmParam_t)
 *        - TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY: 仅执行ACM校准 (acmParam_t)
 *        - TK8710_CTRL_TYPE_SEND_WAKEUP: 发送唤醒信号 (wakeUpParam_t)
 * @param params 控制参数指针
 * @return 0-成功, 非0-失败 (ACM时返回值非0表示异常天线位)
 */
int TK8710Ctrl(TK8710CtrlType type, const void* params)
{
    switch (type) {
        case TK8710_CTRL_TYPE_ACM_START:
        {
            acmParam_t* acmParam = (acmParam_t*)params;
            int ret;
            
            if (acmParam == NULL) {
                return TK8710_ERR;
            }
            
            /* 步骤1: 自动获取校准增益 */
            ret = tk8710_acm_get_gain();
            if (ret != TK8710_OK) {
                return ret;
            }
            
            /* 步骤2: 执行校准流程 */
            ret = tk8710_acm_calibrate(acmParam->calibCount, acmParam->snrThreshold);
            return ret;
        }

        case TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY:
        {
            acmParam_t* acmParam = (acmParam_t*)params;

            if (acmParam == NULL) {
                return TK8710_ERR;
            }

            return tk8710_acm_calibrate(acmParam->calibCount, acmParam->snrThreshold);
        }
        
        case TK8710_CTRL_TYPE_SEND_WAKEUP:
        {
            wakeUpParam_t* wakeUpParam = (wakeUpParam_t*)params;
            
            if (wakeUpParam == NULL) {
                return TK8710_ERR;
            }
            
            /* TODO: 实现唤醒信号发送逻辑 */
            (void)wakeUpParam;
            return TK8710_OK;
        }
        
        default:
            return TK8710_ERR;
    }
}

/**
 * @brief 调试控制接口
 * @param ctrlType 控制类型:
 *        - TK8710_DBG_TYPE_FFT_OUT: 获取FFT运算输出 (仅Get)
 *        - TK8710_DBG_TYPE_CAPTURE_DATA: 获取原始采样数据 (仅Get)
 *        - TK8710_DBG_TYPE_ACM_CAL_FACTOR: 获取ACM校准因子 (仅Get)
 *        - TK8710_DBG_TYPE_ACM_SNR: 获取ACM信噪比 (仅Get)
 *        - TK8710_DBG_TYPE_TX_TONE: 配置并发送单Tone信号 (仅Set)
 * @param optType 操作类型: 0=Set, 1=Get, 2=Exe
 * @param inputParams 输入参数指针 (Set/Exe时使用)
 * @param outputParams 输出参数指针 (Get时使用)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710DebugCtrl(TK8710DebugCtrlType ctrlType, CtrlOptType optType,
                    const void* inputParams, void* outputParams)
{
    switch (ctrlType) {
        case TK8710_DBG_TYPE_TX_TONE:
        {
            if (optType != TK8710_DBG_OPT_SET) {
                return TK8710_ERR;
            }
            
            TxToneConfig* txTone = (TxToneConfig*)inputParams;
            int i;
            int ret;
            uint32_t toneFreq;
            uint32_t toneGain;
            
            if (txTone == NULL) {
                return TK8710_ERR;
            }
            
            toneFreq = txTone->freq;
            toneGain = txTone->gain;
            
            /* 循环配置8个天线的TX Tone */
            for (i = 0; i < 8; i++) {
                /* 配置tx_config_31 (0xd0): Tone频率 */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                    TX_FE_BASE + 0xd0 + 0x1000 * i,
                    toneFreq + 167772 * i);
                if (ret != TK8710_OK) return ret;
                
                /* 配置tx_config_33 (0xd8): Tone增益 */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                    TX_FE_BASE + 0xd8 + 0x1000 * i,
                    toneGain);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置寄存器 0xc030 = 0x8 */
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc030, 0x8);
            if (ret != TK8710_OK) return ret;
            
            /* 配置寄存器 0x9478 = 0x10100010 */
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x10100010);
            if (ret != TK8710_OK) return ret;
            
            /* 配置寄存器 0xc030 = 0x4 */
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc030, 0x4);
            if (ret != TK8710_OK) return ret;

            return TK8710_OK;
        }
        
        case TK8710_DBG_TYPE_REG_RW:
        {
            g_regRwTestErrorCount = 0;
            /* 寄存器读写测试 - 参考8710测试代码实现 */
            TK8710_LOG_CONFIG_INFO("Start register read/write test...\n");
            
            /* 测试RX FE寄存器 */
            TK8710_LOG_CONFIG_INFO("Test RX FE registers...\n");
            test_rx_fe_regs();
            
            /* 测试MAC寄存器 */
            TK8710_LOG_CONFIG_INFO("Test MAC registers...\n");
            test_mac_regs();
            
            /* 测试TX FE寄存器 */
            TK8710_LOG_CONFIG_INFO("Test TX FE registers...\n");
            test_tx_fe_regs();
            
            /* 测试RX BCN寄存器 */
            TK8710_LOG_CONFIG_INFO("Test RX BCN registers...\n");
            test_rx_bcn_regs();
            
            /* 测试TX MOD寄存器 */
            TK8710_LOG_CONFIG_INFO("Test TX MOD registers...\n");
            test_tx_mod_regs();
            
            /* 测试RX MUP寄存器 */
            TK8710_LOG_CONFIG_INFO("Test RX MUP registers...\n");
            test_rx_mup_regs();
            
            /* 测试ACM寄存器 */
            TK8710_LOG_CONFIG_INFO("Test ACM registers...\n");
            test_acm_regs();
            
            /* 测试RF频率寄存器 */
            TK8710_LOG_CONFIG_INFO("Test RF frequency registers...\n");
            test_rf_freq_regs();
            
#if defined(PLATFORM_TMS570)
            TK8710_LOG_CONFIG_INFO("Skip efuse read/write test on PLATFORM_TMS570\n");
#else
            /* Test efuse read/write */
            TK8710_LOG_CONFIG_INFO("Test efuse read/write...\n");
            {
                uint8_t spi_data_in[64];
                uint8_t spi_data_out[64] = {0};
                int i;
                int ret;
                
                /* Initialize test data 0-63 */
                for (i = 0; i < 64; i++) {
                    spi_data_in[i] = (uint8_t)i;
                }
                
                TK8710GpioSet("gpiochip0", 9, 1);
                /* Write efuse data */
                ret = TK8710EfuseWrite(0, spi_data_in, 64);
                if (ret == TK8710_OK) {
                    TK8710_LOG_CONFIG_INFO("Efuse write success\n");
                } else {
                    TK8710_LOG_CONFIG_ERROR("Efuse write failed: %d\n", ret);
                }
                TK8710GpioSet("gpiochip0", 9, 0);
                /* Read efuse data */
                ret = TK8710EfuseRead(0, spi_data_out, 64);
                if (ret == TK8710_OK) {
                    TK8710_LOG_CONFIG_INFO("Efuse read success\n");
                    /* Verify data */
                    for (i = 0; i < 64; i++) {
                        if (spi_data_in[i] != spi_data_out[i]) {
                            TK8710_LOG_CONFIG_ERROR("Efuse data mismatch at index %d: expected %d, got %d\n", 
                                                   i, spi_data_in[i], spi_data_out[i]);
                        }
                    }
                } else {
                    TK8710_LOG_CONFIG_ERROR("Efuse read failed: %d\n", ret);
                    g_regRwTestErrorCount++;
                }
            }
#endif

            if (g_regRwTestErrorCount != 0) {
                TK8710_LOG_CONFIG_ERROR("Register read/write test failed, error count=%d\n",
                                        g_regRwTestErrorCount);
                return TK8710_ERR;
            }
            
            TK8710_LOG_CONFIG_INFO("Register read/write test completed\n");
            return TK8710_OK;
        }
        
        case TK8710_DBG_TYPE_FFT_OUT:
        case TK8710_DBG_TYPE_CAPTURE_DATA:
        {
#if TK8710_PLATFORM_HAS_FILE_IO
            int ret;
            int16_t* captureBuffer16;
            uint8_t* captureBuffer;
            uint32_t captureLength;
            char filename[256];
            FILE* outputFile;
            int antenna;
            
            // 1. 创建8710CaptureData目录
            ret = system("mkdir -p 8710CaptureData");
            if (ret != 0) {
                TK8710_LOG_CONFIG_WARN("Create 8710CaptureData directory failed, continue...\n");
            }
            
            // 2. 配置s_ram_rd0寄存器，启用采集功能 (cap_en = 1)
            s_ram_rd0 ramRd0;
            ramRd0.data = TK8710_S_RAM_RD0_CAP_EN_ENCODE(1U);  // 启用采集
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("Configure s_ram_rd0 register failed: ret=%d\n", ret);
                return ret;
            }
            TK8710_LOG_CONFIG_INFO("Configured s_ram_rd0 register, cap_en=1\n");
            
            // 3. 根据当前运行模式确定采集的数据长度
            uint8_t currentRateMode = TK8710GetRateMode();
            RateModeParams rateParams;
            
            // 获取当前速率模式参数
            if (TK8710GetRateModeParams(currentRateMode, &rateParams) != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("Get rate mode %d parameters failed\n", currentRateMode);
                return TK8710_ERR;
            }
            
            // 根据模式设置采集长度：
            // mode5-8：每根天线16384个数据；mode9：每根天线4096、mode10:2048、mode11和18:1024
            switch (currentRateMode) {
                case 5: case 6: case 7: case 8:
                    captureLength = 16384;
                    break;
                case 9:
                    captureLength = 4096;
                    break;
                case 10:
                    captureLength = 2048;
                    break;
                case 11: case 18:
                    captureLength = 1024;
                    break;
                default:
                    captureLength = 16384;  // 默认值
                    TK8710_LOG_CONFIG_WARN("Unknown mode %d, use default capture length 16384\n", currentRateMode);
                    break;
            }
            
            TK8710_LOG_CONFIG_INFO("Current mode: %d, signal bandwidth: %d KHz, max users: %d, capture length: %d\n", 
                                        currentRateMode, rateParams.signalBwKHz, rateParams.maxUsers, captureLength);
            
            // 4. 分配采集缓冲区（按16bit数据分配）
            captureBuffer16 = (int16_t*)malloc(captureLength * sizeof(int16_t));
            if (captureBuffer16 == NULL) {
                TK8710_LOG_CONFIG_ERROR("Allocate capture buffer failed\n");
                return TK8710_ERR;
            }
            // 转换为uint8_t指针用于SPI传输
            captureBuffer = (uint8_t*)captureBuffer16;
            
            // 5. 循环采集8根天线的数据
            for (antenna = 0; antenna < 8; antenna++) {
                // 调用TK8710SpiGetInfo获取天线数据
                // infotype: 12-19 对应天线0-7
                uint8_t infoType = TK8710_GET_INFO_CAPTURE_0 + antenna;
                ret = TK8710SpiGetInfo(infoType, captureBuffer, captureLength * 2 - 10);
                if (ret != TK8710_OK) {
                    TK8710_LOG_CONFIG_ERROR("Get antenna %d data failed: ret=%d\n", antenna + 1, ret);
                    continue;
                }
                
                // 转换大端字节序为小端（TK8710输出大端：0x33,0x11 → 需要转换为0x11,0x33）
                for (uint32_t i = 0; i < captureLength; i++) {
                    uint8_t temp = captureBuffer[i * 2];        // 保存高字节
                    captureBuffer[i * 2] = captureBuffer[i * 2 + 1];  // 低字节移到前面
                    captureBuffer[i * 2 + 1] = temp;              // 高字节移到后面
                }
                
                // 生成文件名：AntennaData1到AntennaData8
                snprintf(filename, sizeof(filename), "8710CaptureData/AntennaData%d.bin", antenna + 1);
                
                // 保存数据到文件（按字节写入转换后的16bit数据）
                outputFile = fopen(filename, "wb");
                if (outputFile == NULL) {
                    TK8710_LOG_CONFIG_ERROR("Create file %s failed\n", filename);
                    continue;
                }
                
                size_t written = fwrite(captureBuffer, 1, captureLength * 2, outputFile);
                fclose(outputFile);
                
                if (written == captureLength * 2) {
                    TK8710_LOG_CONFIG_INFO("Antenna %d data captured, saved to %s, length: %u bytes (%d 16-bit samples)\n", 
                                        antenna + 1, filename, (unsigned int)written, captureLength);
                } else {
                    TK8710_LOG_CONFIG_ERROR("Save antenna %d data failed, written bytes: %u, expected: %u\n", 
                                        antenna + 1, (unsigned int)written, (unsigned int)(captureLength * 2));
                }
            }
            
            // 6. 释放缓冲区
            free(captureBuffer16);

            ramRd0.data = 0;
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("Configure s_ram_rd0 register failed: ret=%d\n", ret);
                return ret;
            }

            TK8710_LOG_CONFIG_INFO("Capture data operation completed\n");
            return TK8710_OK;
#elif defined(PLATFORM_TMS570)
            int captureRet = TK8710CaptureRequest(TK8710GetRateMode());
            if (captureRet != 0) {
                TK8710_LOG_CONFIG_ERROR("Queue TMS570 capture failed: ret=%d",
                                        captureRet);
                return TK8710_ERR_STATE;
            }
            return TK8710_OK;
#else
            return TK8710_ERR_STATE;
#endif
        }
        case TK8710_DBG_TYPE_ACM_CAL_FACTOR:
        {
            AcmCalibrationFactors calFactors;
            /* 获取ACM校准因子 */
            int ret = TK8710GetAcmCalibrationFactors(&calFactors);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("Get ACM calibration factors failed: ret=%d\n", ret);
                return ret;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_SNR:
        {
            int TmpSNR[32];
            uint8_t snrThreshold = 10; /* 默认SNR门限值 */
            int validSnrCount;
            
            TK8710_LOG_CONFIG_INFO("Get ACM SNR values...\n");
            validSnrCount = tk8710_acm_get_snr(TmpSNR, snrThreshold);
            
            if (validSnrCount >= 0) {
                TK8710_LOG_CONFIG_INFO("ACM SNR get completed, valid SNR count: %d\n", validSnrCount);
            } else {
                TK8710_LOG_CONFIG_INFO("ACM SNR get failed\n");
                return TK8710_ERR;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_AUTO_GAIN:
        {
            int ret;
            TK8710_LOG_CONFIG_INFO("Run ACM auto-gain...\n");
            ret = tk8710_acm_get_gain();
            if (ret == TK8710_OK) {
                TK8710_LOG_CONFIG_INFO("ACM auto-gain completed\n");
            } else {
                TK8710_LOG_CONFIG_INFO("ACM auto-gain failed: ret=%d\n", ret);
                return ret;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_CALIBRATE:
        {
            int ret;
            uint8_t calibCount = 1;   /* 默认校准次数 */
            uint8_t snrThreshold = 28; /* 默认SNR门限值 */
            
            /* 从输入参数获取校准配置 */
            if (inputParams != NULL) {
                AcmCalibParams* params = (AcmCalibParams*)inputParams;
                calibCount = params->calibCount;
                snrThreshold = params->snrThreshold;
            }
            int* SuccessNum = (int*)outputParams;
            TK8710_LOG_CONFIG_INFO("Run ACM calibration (count: %d, SNR threshold: %d)...\n", calibCount, snrThreshold);
            ret = tk8710_acm_calibrate(calibCount, snrThreshold);
            *SuccessNum = ret;
            if (ret >= 0) {
                TK8710_LOG_CONFIG_INFO("ACM calibration completed, valid count: %d\n", ret);
            } else {
                TK8710_LOG_CONFIG_INFO("ACM calibration failed: ret=%d\n", ret);
                return TK8710_ERR;
            }
            
            return TK8710_OK;
        }
            
        default:
            return TK8710_ERR;
    }
}

/*============================================================================
 * 寄存器测试函数实现
 *============================================================================*/

/**
 * @brief 测试RX FE寄存器
 */
static void test_rx_fe_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试rx_ds_flt_0寄存器 */
        uint32_t addr1 = RX_FE_BASE + offsetof(struct rx_top, rx_ds_flt_0);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write rx_ds_flt_0 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read rx_ds_flt_0 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试rx_dfe2寄存器 */
        uint32_t addr2 = RX_FE_BASE + offsetof(struct rx_top, rx_dfe2);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write rx_dfe2 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read rx_dfe2 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX FE register test done, error count: %d\n", count);
}

/**
 * @brief 测试MAC寄存器
 */
static void test_mac_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试init_6寄存器 */
        uint32_t addr1 = MAC_BASE + offsetof(struct mac, init_6);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write init_6 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read init_6 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!mac_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试init_7寄存器 */
        uint32_t addr2 = MAC_BASE + offsetof(struct mac, init_7);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write init_7 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read init_7 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!mac_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("MAC register test done, error count: %d\n", count);
}

/**
 * @brief 测试TX FE寄存器
 */
static void test_tx_fe_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试tx_bcn_4寄存器 */
        uint32_t addr1 = TX_FE_BASE + offsetof(struct tx_dac_if, tx_bcn_4);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write tx_bcn_4 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read tx_bcn_4 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试tx_config_27寄存器 */
        uint32_t addr2 = TX_FE_BASE + offsetof(struct tx_dac_if, tx_config_27);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write tx_config_27 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read tx_config_27 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("TX FE register test done, error count: %d\n", count);
}

/**
 * @brief 测试RX BCN寄存器
 */
static void test_rx_bcn_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试bcn_3寄存器 */
        uint32_t addr1 = RX_BCN_BASE + offsetof(struct rx_bcn, bcn_3);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write bcn_3 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read bcn_3 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_bcn_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试bcn_5寄存器 */
        uint32_t addr2 = RX_BCN_BASE + offsetof(struct rx_bcn, bcn_5);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write bcn_5 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read bcn_5 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_bcn_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX BCN register test done, error count: %d\n", count);
}

/**
 * @brief 测试TX MOD寄存器
 */
static void test_tx_mod_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试tm_config_1寄存器 */
        uint32_t addr1 = TX_MOD_BASE + offsetof(struct tx_top, tm_config_1);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write tm_config_1 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read tm_config_1 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_mod_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试tm_config_3寄存器 */
        uint32_t addr2 = TX_MOD_BASE + offsetof(struct tx_top, tm_config_3);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write tm_config_3 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read tm_config_3 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_mod_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("TX MOD register test done, error count: %d\n", count);
}

/**
 * @brief 测试RX MUP寄存器
 */
static void test_rx_mup_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试ud_ctrl0寄存器 */
        uint32_t addr1 = RX_MUP_BASE + offsetof(struct rx_mup, ud_ctrl0);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write ud_ctrl0 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read ud_ctrl0 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_mup_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试ud_ctrl7寄存器 */
        uint32_t addr2 = RX_MUP_BASE + offsetof(struct rx_mup, ud_ctrl7);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write ud_ctrl7 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read ud_ctrl7 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_mup_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX MUP register test done, error count: %d\n", count);
}

/**
 * @brief 测试ACM寄存器
 */
static void test_acm_regs(void)
{
    int i;
    uint32_t value_tmp;
    int count = 0;
    
    for (i = 0; i < 100; i++) {
        /* 测试acm_ctrl1寄存器 */
        uint32_t addr1 = ACM_BASE + offsetof(struct acm, acm_ctrl1);
        int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr1, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write acm_ctrl1 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read acm_ctrl1 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!acm_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试acm_ctrl11寄存器 */
        uint32_t addr2 = ACM_BASE + offsetof(struct acm, acm_ctrl11);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Write acm_ctrl11 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("Read acm_ctrl11 failed: ret=%d\n", ret);
            count++;
            g_regRwTestErrorCount++;
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            g_regRwTestErrorCount++;
            TK8710_LOG_CONFIG_ERROR("!!!!acm_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("ACM register test done, error count: %d\n", count);
}

/**
 * @brief 测试RF频率寄存器
 * @note  写入TX和RX频率，然后读取验证，测试100次
 *        频率从470000000Hz开始，步进50000Hz
 */
static void test_rf_freq_regs(void)
{
    int i;
    int count = 0;
    uint32_t freq_hz;
    uint32_t freq_reg;
    uint8_t rfSel = 0xFF;  /* 使用RF0-7 */
    double freq_step = RF_SX1257_FREQ_STEP;  /* 使用SX1257频率步进 */
    
    /* RF频率寄存器地址 */
    uint16_t rx_msb_addr = RF_CMD_FRF_RX_MSB >> 8;
    uint16_t rx_mid_addr = RF_CMD_FRF_RX_MID >> 8;
    uint16_t rx_lsb_addr = RF_CMD_FRF_RX_LSB >> 8;
    uint16_t tx_msb_addr = RF_CMD_FRF_TX_MSB >> 8;
    uint16_t tx_mid_addr = RF_CMD_FRF_TX_MID >> 8;
    uint16_t tx_lsb_addr = RF_CMD_FRF_TX_LSB >> 8;
    
    TK8710_LOG_CONFIG_INFO("Test RF frequency registers, start: 470MHz, step: 50KHz\n");
    
    for (i = 0; i < 100; i++) {
        /* 计算频率值：从470MHz开始，步进50KHz */
        freq_hz = 470000000 + (i * 50000);
        freq_reg = (uint32_t)((double)freq_hz / freq_step);
        
        /* 写入RX频率 */
        tk8710_rf_write(rfSel, rx_msb_addr, (freq_reg >> 16) & 0xFF);
        tk8710_rf_write(rfSel, rx_mid_addr, (freq_reg >> 8) & 0xFF);
        tk8710_rf_write(rfSel, rx_lsb_addr, (freq_reg >> 0) & 0xFF);
        
        /* 写入TX频率 */
        tk8710_rf_write(rfSel, tx_msb_addr, (freq_reg >> 16) & 0xFF);
        tk8710_rf_write(rfSel, tx_mid_addr, (freq_reg >> 8) & 0xFF);
        tk8710_rf_write(rfSel, tx_lsb_addr, (freq_reg >> 0) & 0xFF);
        
        /* 读取RX频率验证 */
        uint32_t rx_msb, rx_mid, rx_lsb;
        uint32_t tx_msb, tx_mid, tx_lsb;
        uint32_t rx_freq_reg, tx_freq_reg;
        
        tk8710_rf_read(rfSel, rx_msb_addr, &rx_msb);
        tk8710_rf_read(rfSel, rx_mid_addr, &rx_mid);
        tk8710_rf_read(rfSel, rx_lsb_addr, &rx_lsb);
        rx_freq_reg = ((rx_msb & 0xFF) << 16) | ((rx_mid & 0xFF) << 8) | (rx_lsb & 0xFF);
        
        tk8710_rf_read(rfSel, tx_msb_addr, &tx_msb);
        tk8710_rf_read(rfSel, tx_mid_addr, &tx_mid);
        tk8710_rf_read(rfSel, tx_lsb_addr, &tx_lsb);
        tx_freq_reg = ((tx_msb & 0xFF) << 16) | ((tx_mid & 0xFF) << 8) | (tx_lsb & 0xFF);
        
        /* 验证 */
        if (rx_freq_reg != freq_reg) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!RF RX frequency mismatch: write=0x%06X, read=0x%06X, freq=%u Hz, count=%d!!!!\n",
                                   (unsigned int)freq_reg, (unsigned int)rx_freq_reg, (unsigned int)freq_hz, count);
        }
        
        if (tx_freq_reg != freq_reg) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!RF TX frequency mismatch: write=0x%06X, read=0x%06X, freq=%u Hz, count=%d!!!!\n",
                                   (unsigned int)freq_reg, (unsigned int)tx_freq_reg, (unsigned int)freq_hz, count);
        }
        
        /* 每10次打印一次进度 */
        if ((i + 1) % 10 == 0) {
            TK8710_LOG_CONFIG_INFO("RF frequency test progress: %d/100, current frequency: %u Hz\n", i + 1, (unsigned int)freq_hz);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RF frequency register test done, error count: %d\n", count);
}
