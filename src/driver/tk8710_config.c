/**
 * @file tk8710_config.c
 * @brief TK8710 配置管理实现
 */

#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_rf_regs.h"
#include "driver/tk8710_log.h"
#include "../port/tk8710_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
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
    .local_sync = TK8710_SYNC_MODE_LOCAL, /* 保持未传入时的既有本地同步行为 */
    
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

#define TK8710_ACM_FACTOR_FILE_COUNT      2
#define TK8710_ACM_FACTOR_FILE_MAX_BYTES  (5ULL * 1024ULL * 1024ULL)
#define TK8710_ACM_FACTOR_RECORD_MAX_BYTES 2048

static int g_acmFactorFileIndex = -1;

typedef struct {
    TK8710AcmLogSource source;
    uint32_t initSequence;
    uint32_t sampleSequence;
    uint8_t attempt;
    uint8_t maxAttempts;
    uint8_t calibCount;
    uint8_t snrThreshold;
} TK8710AcmLogContext;

static TK8710AcmLogContext g_acmLogContext = {
    .source = TK8710_ACM_LOG_SOURCE_MANUAL
};

void TK8710SetAcmCalibrationLogContext(TK8710AcmLogSource source,
                                       uint32_t initSequence,
                                       uint8_t attempt,
                                       uint8_t maxAttempts,
                                       uint8_t calibCount,
                                       uint8_t snrThreshold)
{
    g_acmLogContext.source = source;
    g_acmLogContext.initSequence = initSequence;
    g_acmLogContext.sampleSequence = 0;
    g_acmLogContext.attempt = attempt;
    g_acmLogContext.maxAttempts = maxAttempts;
    g_acmLogContext.calibCount = calibCount;
    g_acmLogContext.snrThreshold = snrThreshold;
}

/*============================================================================
 * ACM校准内部辅助函数
 *============================================================================*/

/**
 * @brief 触发ACM校准
 */
static int tk8710_acm_trig(void)
{
    s_acm_ctrl acm_ctrl;
    acm_ctrl.data = 0;
    acm_ctrl.b.acm_trig = 1;
    return TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                          MAC_BASE + offsetof(struct mac, acm_ctrl), acm_ctrl.data);
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

/*============================================================================
 * ACM校准内部函数
 *============================================================================*/

static int tk8710_save_acm_gain(void)
{
    FILE* outputFile;
    const char* filename = "CaliFactor/gain.txt";
    time_t rawtime;
    struct tm* timeinfo;
    int i;

#ifdef _WIN32
    _mkdir("CaliFactor");
#else
    mkdir("CaliFactor", 0755);
#endif

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo == NULL) {
        TK8710_LOG_CONFIG_ERROR("Failed to get local time for ACM gain file\n");
        return TK8710_ERR;
    }

    outputFile = fopen(filename, "a");
    if (outputFile == NULL) {
        TK8710_LOG_CONFIG_ERROR("Failed to open ACM gain file: %s\n", filename);
        return TK8710_ERR;
    }

    if (fprintf(outputFile, "\n=== %04d-%02d-%02d %02d:%02d:%02d ===\n",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec) < 0 ||
        fprintf(outputFile, "index,dgain0,dgain1,again0,again1\n") < 0) {
        fclose(outputFile);
        TK8710_LOG_CONFIG_ERROR("Failed to write ACM gain header: %s\n", filename);
        return TK8710_ERR;
    }

    for (i = 0; i < 16; i++) {
        if (fprintf(outputFile, "%d,%u,%u,%u,%u\n", i,
                    g_acm_dgain0[i], g_acm_dgain1[i],
                    g_acm_again0[i], g_acm_again1[i]) < 0) {
            fclose(outputFile);
            TK8710_LOG_CONFIG_ERROR("Failed to write ACM gain data: %s\n", filename);
            return TK8710_ERR;
        }
    }

    if (fclose(outputFile) != 0) {
        TK8710_LOG_CONFIG_ERROR("Failed to close ACM gain file: %s\n", filename);
        return TK8710_ERR;
    }

    TK8710_LOG_CONFIG_INFO("ACM gain results saved: %s\n", filename);
    return TK8710_OK;
}

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
    int GainStep = 2;
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
    irqCtrl0.b.acm_irq_mask = 0;
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
                    g_acm_again1[7] = g_acm_again1[9] = g_acm_again1[11] = g_acm_again1[13] = (GainStep*i);
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
                    g_acm_dgain1[7] = g_acm_dgain1[9] = g_acm_dgain1[11] = g_acm_dgain1[13] = (GainStep*i);
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
        // g_acm_dgain0[i] = 6;
        // g_acm_again0[i] = 6;
        // g_acm_dgain1[i] = 6;
        // g_acm_again1[i] = 6;
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
    irqCtrl0.b.acm_irq_mask = 1;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
    if (ret != TK8710_OK) return ret;

    ret = tk8710_save_acm_gain();
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
    int ret;
    int timed_out = 0;
    int SNRNum = 0;
    
    /* 等待ACM中断 (bit9)，添加100ms超时机制 */
#ifdef _WIN32
    DWORD start_time = GetTickCount();
    DWORD timeout = 100; /* 100ms超时 */
#else
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    long timeout_ms = 100; /* 100ms超时 */
#endif

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        MAC_BASE + offsetof(struct mac, irq_res), &irq_res.data);
    if (ret != TK8710_OK) return -1;
    while ((irq_res.b.irq_type & (1 << TK8710_IRQ_ACM)) == 0) {
#ifdef _WIN32
        DWORD current_time = GetTickCount();
        if ((current_time - start_time) >= timeout) {
            timed_out = 1;
            TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, "ACM中断等待超时(100ms)，自动退出");
            break;
        }
#else
        gettimeofday(&current_time, NULL);
        long elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
        if (elapsed_ms >= timeout_ms) {
            timed_out = 1;
            TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, "ACM中断等待超时(100ms)，自动退出");
            break;
        }
#endif
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, irq_res), &irq_res.data);
        if (ret != TK8710_OK) return -1;
    }
    if (timed_out != 0) return -1;
    
    /* 清除ACM中断 */
    irq_ctrl1.data = 0;
    irq_ctrl1.b.acm_irq_clr = 1;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                         MAC_BASE + offsetof(struct mac, irq_ctrl1), irq_ctrl1.data);
    if (ret != TK8710_OK) return -1;
    
    // /* 延时30ms */
    // TK8710DelayMs(30);
    
    /* 读取acm_obv17~24获取SNR值 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv18), &value_tmp1);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv17), &value_tmp2);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv20), &value_tmp3);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv19), &value_tmp4);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv22), &value_tmp5);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv21), &value_tmp6);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv24), &value_tmp7);
    if (ret != TK8710_OK) return -1;
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        ACM_BASE + offsetof(struct acm, acm_obv23), &value_tmp8);
    if (ret != TK8710_OK) return -1;
    
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
    TK8710_LOG_CONFIG_INFO("=== ACM SNR值 ===\n");
    for (i = 0; i < 8; i++) {
        TK8710_LOG_CONFIG_INFO("天线%d: TX0_SNR=%d, RX0_SNR=%d, TX1_SNR=%d, RX1_SNR=%d\n", 
                            i, SNR_tx0[i], SNR_rx0[i], SNR_tx1[i], SNR_rx1[i]);
    }
    TK8710_LOG_CONFIG_INFO("有效SNR数量: %d (门限: %d)\n", SNRNum, snrThreshold);
    
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
    irqCtrl0.b.acm_irq_mask = 0;
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
            validCount++;
        }
    }
    
    /* 关闭ACM中断使能 */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
        MAC_BASE + offsetof(struct mac, irq_ctrl0), &regVal);
    if (ret != TK8710_OK) return ret;
    irqCtrl0.data = regVal;
    irqCtrl0.b.acm_irq_mask = 1;
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
static int tk8710_save_acm_factors(const AcmCalibrationFactors* calFactors)
{
    static const char* filenames[TK8710_ACM_FACTOR_FILE_COUNT] = {
        "CaliFactor/CaliFactor0.txt",
        "CaliFactor/CaliFactor1.txt"
    };
    struct stat fileStats[TK8710_ACM_FACTOR_FILE_COUNT];
    uint64_t currentSize = 0;
    char record[TK8710_ACM_FACTOR_RECORD_MAX_BYTES];
    size_t recordLength = 0;
    time_t rawtime;
    struct tm* timeinfo;
    FILE* outputFile;
    const char* openMode = "a";
    int fileExists[TK8710_ACM_FACTOR_FILE_COUNT];
    int written;
    int i;

#ifdef _WIN32
    _mkdir("CaliFactor");
#else
    mkdir("CaliFactor", 0755);
#endif

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo == NULL) {
        TK8710_LOG_CONFIG_ERROR("Failed to get local time for ACM factor file\n");
        return TK8710_ERR;
    }

    g_acmLogContext.sampleSequence++;

    if (g_acmLogContext.source == TK8710_ACM_LOG_SOURCE_INIT) {
        written = snprintf(record, sizeof(record),
                           "\n=== %04d-%02d-%02d %02d:%02d:%02d | "
                           "INIT seq=%u retry=%u/%u sample=%u count=%u snr=%u ===\n",
                           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                           timeinfo->tm_mday, timeinfo->tm_hour,
                           timeinfo->tm_min, timeinfo->tm_sec,
                           (unsigned int)g_acmLogContext.initSequence,
                           (unsigned int)g_acmLogContext.attempt,
                           (unsigned int)g_acmLogContext.maxAttempts,
                           (unsigned int)g_acmLogContext.sampleSequence,
                           (unsigned int)g_acmLogContext.calibCount,
                           (unsigned int)g_acmLogContext.snrThreshold);
    } else if (g_acmLogContext.source == TK8710_ACM_LOG_SOURCE_PERIODIC) {
        written = snprintf(record, sizeof(record),
                           "\n=== %04d-%02d-%02d %02d:%02d:%02d | PERIODIC ===\n",
                           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                           timeinfo->tm_mday, timeinfo->tm_hour,
                           timeinfo->tm_min, timeinfo->tm_sec);
    } else {
        written = snprintf(record, sizeof(record),
                           "\n=== %04d-%02d-%02d %02d:%02d:%02d | MANUAL ===\n",
                           timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                           timeinfo->tm_mday, timeinfo->tm_hour,
                           timeinfo->tm_min, timeinfo->tm_sec);
    }
    if (written < 0 || (size_t)written >= sizeof(record)) {
        TK8710_LOG_CONFIG_ERROR("Failed to format ACM factor header\n");
        return TK8710_ERR;
    }
    recordLength = (size_t)written;

    written = snprintf(record + recordLength, sizeof(record) - recordLength,
                       "ACM校准因子数据:\n");
    if (written < 0 || (size_t)written >= sizeof(record) - recordLength) {
        TK8710_LOG_CONFIG_ERROR("Failed to format ACM factor title\n");
        return TK8710_ERR;
    }
    recordLength += (size_t)written;

    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        uint32_t i_factor = calFactors->channels[i].i_factor & 0x3FFFF;
        uint32_t q_factor = calFactors->channels[i].q_factor & 0x3FFFF;

        written = snprintf(record + recordLength, sizeof(record) - recordLength,
                           "通道[%d]: I_factor=0x%05X, Q_factor=0x%05X\n",
                           i, i_factor, q_factor);
        if (written < 0 || (size_t)written >= sizeof(record) - recordLength) {
            TK8710_LOG_CONFIG_ERROR("ACM factor record exceeds buffer size\n");
            return TK8710_ERR;
        }
        recordLength += (size_t)written;
    }

    for (i = 0; i < TK8710_ACM_FACTOR_FILE_COUNT; i++) {
        fileExists[i] = (stat(filenames[i], &fileStats[i]) == 0);
    }

    if (g_acmFactorFileIndex < 0) {
        if (fileExists[0] && fileExists[1]) {
            g_acmFactorFileIndex =
                (fileStats[1].st_mtime > fileStats[0].st_mtime) ? 1 : 0;
        } else if (fileExists[1]) {
            g_acmFactorFileIndex = 1;
        } else {
            g_acmFactorFileIndex = 0;
        }
    }

    if (fileExists[g_acmFactorFileIndex]) {
        currentSize = (uint64_t)fileStats[g_acmFactorFileIndex].st_size;
    }

    if (currentSize + recordLength > TK8710_ACM_FACTOR_FILE_MAX_BYTES) {
        g_acmFactorFileIndex = (g_acmFactorFileIndex + 1) % TK8710_ACM_FACTOR_FILE_COUNT;
        openMode = "w";
        currentSize = 0;
        TK8710_LOG_CONFIG_INFO("ACM factor file reached 5 MiB, rotate to: %s\n",
                               filenames[g_acmFactorFileIndex]);
    }

    outputFile = fopen(filenames[g_acmFactorFileIndex], openMode);
    if (outputFile == NULL) {
        TK8710_LOG_CONFIG_ERROR("Failed to open ACM factor file: %s\n",
                                filenames[g_acmFactorFileIndex]);
        return TK8710_ERR;
    }

    if (fwrite(record, 1, recordLength, outputFile) != recordLength) {
        fclose(outputFile);
        TK8710_LOG_CONFIG_ERROR("Failed to write ACM factor file: %s\n",
                                filenames[g_acmFactorFileIndex]);
        return TK8710_ERR;
    }

    if (fclose(outputFile) != 0) {
        TK8710_LOG_CONFIG_ERROR("Failed to close ACM factor file: %s\n",
                                filenames[g_acmFactorFileIndex]);
        return TK8710_ERR;
    }

    TK8710_LOG_CONFIG_INFO("ACM factor record saved: %s, size=%u bytes\n",
                           filenames[g_acmFactorFileIndex],
                           (unsigned int)(currentSize + recordLength));
    return TK8710_OK;
}

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
    TK8710_LOG_CONFIG_INFO("=== ACM校准因子 ===\n");
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
        
        TK8710_LOG_CONFIG_INFO("通道%d: I路=0x%08X (%.3f), Q路=0x%08X (%.3f)\n", 
                            i, i_factor, i_float, q_factor, q_float);
    }
    
    if (tk8710_save_acm_factors(calFactors) != TK8710_OK) {
        return TK8710_ERR;
    }
    
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
            s_mac_config_0 macConfig0;
            s_mac_config_1 macConfig1;
            int ret;
            
            if (slotCfg == NULL) {
                return TK8710_ERR;
            }
            
            /* 配置mac_config_0 (0x00): s0_cfg, s1_cfg, mode */
            macConfig0.data = 0;
            macConfig0.b.s0_cfg = 0;  /* BCN时隙 */
            macConfig0.b.s1_cfg = slotCfg->s1Cfg[0].byteLen & 0x3FF;  /* 使用第一个速率模式 */
            macConfig0.b.mode = slotCfg->rateModes[0] & 0xFF;  /* 使用第一个速率模式 */
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_0), macConfig0.data);
            if (ret != TK8710_OK) return ret;
            
            /* 配置mac_config_1 (0x04): s2_cfg, s3_cfg, pl_crc_en */
            macConfig1.data = 0;
            macConfig1.b.s2_cfg = slotCfg->s2Cfg[0].byteLen & 0x3FF;  /* 使用第一个速率模式 */
            macConfig1.b.s3_cfg = slotCfg->s3Cfg[0].byteLen & 0x3FF;  /* 使用第一个速率模式 */
            macConfig1.b.pl_crc_en = slotCfg->plCrcEn & 0x01;
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_1), macConfig1.data);
            if (ret != TK8710_OK) return ret;
            
            /* 配置 init_1: md_agc, rx_delay */
            {
                s_init_1 init1;
                init1.data = 0;
                init1.b.md_agc = slotCfg->md_agc & 0xFFF;  /* DATA AGC长度, 12bit最大值4095 */
                init1.b.rx_delay = slotCfg->rx_delay & 0xFFFFF;
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_1), init1.data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_2: da1_m */
            {
                s_init_2 init2;
                init2.data = 0;
                init2.b.da1_m = slotCfg->s1Cfg[0].da_m & 0xFFFFFF;  /* da1_m from S1[0] */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_2), init2.data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_3: da2_m, tx_fix_freq */
            {
                s_init_3 init3;
                init3.data = 0;
                init3.b.da2_m = slotCfg->s2Cfg[0].da_m & 0xFFFFFF;  /* da2_m from S2[0] */
                init3.b.tx_fix_freq = slotCfg->txBeamCtrlMode & 0x01;     /* tx_fix_freq */
                //init3.b.rx_fix_freq = 1;     /* rx_fix_freq */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_3), init3.data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_4: da3_m */
            {
                s_init_4 init4;
                init4.data = 0;
                init4.b.da3_m = slotCfg->s3Cfg[0].da_m & 0xFFFFFF;  /* da3_m from S3[0] */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_4), init4.data);
                if (ret != TK8710_OK) return ret;
            }
            
            /* 配置 init_18: da0_m (完整24位) */
            {
                s_init_18 init18;
                init18.data = 0;
                init18.b.da0_m = slotCfg->s0Cfg[0].da_m & 0xFFFFFF;  /* da0_m完整24位 from S0[0] */
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                    MAC_BASE + offsetof(struct mac, init_18), init18.data);
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
                s_obv_1 obv1;
                s_obv_2 obv2;
                s_obv_3 obv3;
                s_obv_4 obv4;
                uint32_t regVal;
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_1), &regVal);
                obv1.data = regVal;
                g_slotCfg.s0Cfg[0].timeLen = obv1.b.s0_len;
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_2), &regVal);
                obv2.data = regVal;
                g_slotCfg.s1Cfg[0].timeLen = obv2.b.s1_len;
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_3), &regVal);
                obv3.data = regVal;
                g_slotCfg.s2Cfg[0].timeLen = obv3.b.s2_len;
                
                TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                    MAC_BASE + offsetof(struct mac, obv_4), &regVal);
                obv4.data = regVal;
                g_slotCfg.s3Cfg[0].timeLen = obv4.b.s3_len;
                
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
            cfg->bcn_agc = init0.b.bcn_agc;
            cfg->interval = init0.b.interval;
            cfg->tx_dly = init0.b.tx_freq_dly;  /* 保存到cfg */
            
            /* 读取 init_1 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_1), &init1.data);
            if (ret != TK8710_OK) return ret;
            /* md_agc字段已移至slotCfg_t */
            /* rx_delay字段已移至slotCfg_t */
            
            /* init_2, init_3, init_4, init_18 配置已移至slotCfg_t */
            
            /* 读取 init_5 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), &init5.data);
            if (ret != TK8710_OK) return ret;
            cfg->offset_adj = init5.b.offset_adj;
            cfg->tx_pre = init5.b.tx_pre;
            cfg->conti_mode = init5.b.conti_mode;
            cfg->bcn_scan = init5.b.conti_scan;
            
            /* 读取 init_9 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), &init9.data);
            if (ret != TK8710_OK) return ret;
            cfg->ant_en = init9.b.ant_en;
            cfg->rf_sel = init9.b.rf_sel;
            cfg->tx_bcn_en = init9.b.tx_bcn_ant_en;
            
            /* 读取 init_11 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), &init11.data);
            if (ret != TK8710_OK) return ret;
            cfg->rf_model = init11.b.rf_type;
            
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
            s_mac_config_0 macConfig0;
            s_mac_config_1 macConfig1;
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
            macConfig0.data = regVal;
            slotCfg->brdUserNum = TK8710GetBrdUserNum();  /* 从全局变量获取广播用户数 */
            slotCfg->s1Cfg[0].byteLen = macConfig0.b.s1_cfg;
            slotCfg->rateModes[0] = macConfig0.b.mode & 0xFF;
            
            /* 读取mac_config_1 (0x04): s2_cfg, s3_cfg, pl_crc_en */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, mac_config_1), &regVal);
            if (ret != TK8710_OK) return ret;
            macConfig1.data = regVal;
            slotCfg->s2Cfg[0].byteLen = macConfig1.b.s2_cfg;
            slotCfg->s3Cfg[0].byteLen = macConfig1.b.s3_cfg;
            slotCfg->plCrcEn = macConfig1.b.pl_crc_en;
            
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
            slotCfg->s1Cfg[0].timeLen = obv2.b.s1_len;
            
            /* 读取obv_3 (0xfc): s2_len - 时隙2时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_3), &regVal);
            if (ret != TK8710_OK) return ret;
            obv3.data = regVal;
            slotCfg->s2Cfg[0].timeLen = obv3.b.s2_len;
            
            /* 读取obv_4 (0x100): s3_len - 时隙3时间 */
            ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, obv_4), &regVal);
            if (ret != TK8710_OK) return ret;
            obv4.data = regVal;
            slotCfg->s3Cfg[0].timeLen = obv4.b.s3_len;
            
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
            int ret;
            ret = tk8710_rf_write(0xff, 0x8C7e >> 8, 0xfe);
            TK8710SetAcmCalibrationLogContext(TK8710_ACM_LOG_SOURCE_PERIODIC,
                                               0, 0, 0,
                                               acmParam->calibCount,
                                               acmParam->snrThreshold);
            ret =  tk8710_acm_calibrate(acmParam->calibCount, acmParam->snrThreshold);
            TK8710SetAcmCalibrationLogContext(TK8710_ACM_LOG_SOURCE_MANUAL,
                                               0, 0, 0, 0, 0);
            tk8710_rf_write(0xff, 0x8C7e >> 8, 0x7e);
            TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), 0x1ffff);
            return ret;
        }
        
        case TK8710_CTRL_TYPE_SEND_WAKEUP:
        {
            const wakeUpParam_t* wakeUpParam = (const wakeUpParam_t*)params;
            s_wakeup_ctrl wakeupCtrl;
            
            if (wakeUpParam == NULL) {
                return TK8710_ERR;
            }

            if (wakeUpParam->wakeUpMode < TK8710_WAKEUP_MODE_1 ||
                wakeUpParam->wakeUpMode > TK8710_WAKEUP_MODE_3 ||
                wakeUpParam->wakeUpLen > 0xFFFFFFu) {
                TK8710_LOG_CONFIG_ERROR("Invalid wakeup parameters: mode=%u, len=%u ms",
                                        (unsigned int)wakeUpParam->wakeUpMode,
                                        (unsigned int)wakeUpParam->wakeUpLen);
                return TK8710_ERR;
            }

            wakeupCtrl.data = 0;
            wakeupCtrl.b.wakeup_mode = (uint32_t)wakeUpParam->wakeUpMode;
            wakeupCtrl.b.wakeup_len = wakeUpParam->wakeUpLen;

            return TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                                  MAC_BASE + offsetof(struct mac, wakeup_ctrl),
                                  wakeupCtrl.data);
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
            /* 寄存器读写测试 - 参考8710测试代码实现 */
            TK8710_LOG_CONFIG_INFO("开始寄存器读写测试...\n");
            
            /* 测试RX FE寄存器 */
            TK8710_LOG_CONFIG_INFO("测试RX FE寄存器...\n");
            test_rx_fe_regs();
            
            /* 测试MAC寄存器 */
            TK8710_LOG_CONFIG_INFO("测试MAC寄存器...\n");
            test_mac_regs();
            
            /* 测试TX FE寄存器 */
            TK8710_LOG_CONFIG_INFO("测试TX FE寄存器...\n");
            test_tx_fe_regs();
            
            /* 测试RX BCN寄存器 */
            TK8710_LOG_CONFIG_INFO("测试RX BCN寄存器...\n");
            test_rx_bcn_regs();
            
            /* 测试TX MOD寄存器 */
            TK8710_LOG_CONFIG_INFO("测试TX MOD寄存器...\n");
            test_tx_mod_regs();
            
            /* 测试RX MUP寄存器 */
            TK8710_LOG_CONFIG_INFO("测试RX MUP寄存器...\n");
            test_rx_mup_regs();
            
            /* 测试ACM寄存器 */
            TK8710_LOG_CONFIG_INFO("测试ACM寄存器...\n");
            test_acm_regs();
            
            /* 测试RF频率寄存器 */
            TK8710_LOG_CONFIG_INFO("测试RF频率寄存器...\n");
            test_rf_freq_regs();
            
            /* Test efuse read/write */
            TK8710_LOG_CONFIG_INFO("Test efuse read/write...\n");
            {
                uint8_t spi_data_in[64];
                uint8_t spi_data_out[64] = {0};
                
                /* Initialize test data 0-63 */
                for (int i = 0; i < 64; i++) {
                    spi_data_in[i] = i;
                }
                int ret;
                
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
                    for (int i = 0; i < 64; i++) {
                        if (spi_data_in[i] != spi_data_out[i]) {
                            TK8710_LOG_CONFIG_ERROR("Efuse data mismatch at index %d: expected %d, got %d\n", 
                                                   i, spi_data_in[i], spi_data_out[i]);
                        }
                    }
                } else {
                    TK8710_LOG_CONFIG_ERROR("Efuse read failed: %d\n", ret);
                }
            }
            
            TK8710_LOG_CONFIG_INFO("Register read/write test completed\n");
            return TK8710_OK;
            break;
        }
        
        case TK8710_DBG_TYPE_FFT_OUT:
        case TK8710_DBG_TYPE_CAPTURE_DATA:
        {
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
                TK8710_LOG_CONFIG_WARN("创建8710CaptureData目录失败，继续执行...\n");
            }
            
            // 2. 配置s_ram_rd0寄存器，启用采集功能 (cap_en = 1)
            s_ram_rd0 ramRd0;
            ramRd0.data = 0;
            ramRd0.b.cap_en = 1;  // 启用采集
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("配置s_ram_rd0寄存器失败: ret=%d\n", ret);
                return ret;
            }
            TK8710_LOG_CONFIG_INFO("已配置s_ram_rd0寄存器，cap_en=1\n");
            
            // 3. 根据当前运行模式确定采集的数据长度
            uint8_t currentRateMode = TK8710GetRateMode();
            RateModeParams rateParams;
            
            // 获取当前速率模式参数
            if (TK8710GetRateModeParams(currentRateMode, &rateParams) != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("获取速率模式%d参数失败\n", currentRateMode);
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
                    TK8710_LOG_CONFIG_WARN("未知模式%d，使用默认采集长度16384\n", currentRateMode);
                    break;
            }
            
            TK8710_LOG_CONFIG_INFO("当前模式：%d，信号带宽：%d KHz，最大用户数：%d，采集长度：%d\n", 
                                        currentRateMode, rateParams.signalBwKHz, rateParams.maxUsers, captureLength);
            
            // 4. 分配采集缓冲区（按16bit数据分配）
            captureBuffer16 = (int16_t*)malloc(captureLength * sizeof(int16_t));
            if (captureBuffer16 == NULL) {
                TK8710_LOG_CONFIG_ERROR("分配采集缓冲区失败\n");
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
                    TK8710_LOG_CONFIG_ERROR("获取天线%d数据失败: ret=%d\n", antenna + 1, ret);
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
                    TK8710_LOG_CONFIG_ERROR("创建文件%s失败\n", filename);
                    continue;
                }
                
                size_t written = fwrite(captureBuffer, 1, captureLength * 2, outputFile);
                fclose(outputFile);
                
                if (written == captureLength * 2) {
                    TK8710_LOG_CONFIG_INFO("天线%d数据采集完成，保存到%s，数据长度: %u bytes (%d个16bit采样点)\n", 
                                        antenna + 1, filename, (unsigned int)written, captureLength);
                } else {
                    TK8710_LOG_CONFIG_ERROR("保存天线%d数据失败，写入字节数: %u，期望: %u\n", 
                                        antenna + 1, (unsigned int)written, (unsigned int)(captureLength * 2));
                }
            }
            
            // 6. 释放缓冲区
            free(captureBuffer16);

            ramRd0.data = 0;
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("配置s_ram_rd0寄存器失败: ret=%d\n", ret);
                return ret;
            }

            TK8710_LOG_CONFIG_INFO("采集数据功能执行完成\n");
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_CAL_FACTOR:
        {
            AcmCalibrationFactors calFactors;
            /* 获取ACM校准因子 */
            int ret = TK8710GetAcmCalibrationFactors(&calFactors);
            if (ret != TK8710_OK) {
                TK8710_LOG_CONFIG_ERROR("获取ACM校准因子失败: ret=%d\n", ret);
                return ret;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_SNR:
        {
            int TmpSNR[32];
            uint8_t snrThreshold = 10; /* 默认SNR门限值 */
            int validSnrCount;
            
            TK8710_LOG_CONFIG_INFO("获取ACM SNR值...\n");
            validSnrCount = tk8710_acm_get_snr(TmpSNR, snrThreshold);
            
            if (validSnrCount >= 0) {
                TK8710_LOG_CONFIG_INFO("ACM SNR获取完成，有效SNR数量: %d\n", validSnrCount);
            } else {
                TK8710_LOG_CONFIG_INFO("ACM SNR获取失败\n");
                return TK8710_ERR;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_AUTO_GAIN:
        {
            int ret;
            TK8710_LOG_CONFIG_INFO("执行ACM增益自动获取...\n");
            ret = tk8710_acm_get_gain();
            if (ret == TK8710_OK) {
                TK8710_LOG_CONFIG_INFO("ACM增益自动获取完成\n");
            } else {
                TK8710_LOG_CONFIG_INFO("ACM增益自动获取失败: ret=%d\n", ret);
                return ret;
            }
            
            return TK8710_OK;
        }
        case TK8710_DBG_TYPE_ACM_CALIBRATE:
        {
            int ret;
            uint8_t calibCount = 5;   /* 默认校准次数 */
            uint8_t snrThreshold = 32; /* 默认SNR门限值 */
            
            /* 从输入参数获取校准配置 */
            if (inputParams != NULL) {
                AcmCalibParams* params = (AcmCalibParams*)inputParams;
                calibCount = params->calibCount;
                snrThreshold = params->snrThreshold;
            }
            int* SuccessNum = (int*)outputParams;
            TK8710_LOG_CONFIG_INFO("执行ACM校准 (校准次数: %d, SNR门限: %d)...\n", calibCount, snrThreshold);
            ret = tk8710_acm_calibrate(calibCount, snrThreshold);
            *SuccessNum = ret;
            if (ret >= 0) {
                TK8710_LOG_CONFIG_INFO("ACM校准完成，有效校准次数: %d\n", ret);
            } else {
                TK8710_LOG_CONFIG_INFO("ACM校准失败: ret=%d\n", ret);
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
            TK8710_LOG_CONFIG_ERROR("写入rx_ds_flt_0失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取rx_ds_flt_0失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试rx_dfe2寄存器 */
        uint32_t addr2 = RX_FE_BASE + offsetof(struct rx_top, rx_dfe2);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入rx_dfe2失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取rx_dfe2失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX FE寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入init_6失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取init_6失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!mac_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试init_7寄存器 */
        uint32_t addr2 = MAC_BASE + offsetof(struct mac, init_7);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入init_7失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取init_7失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!mac_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("MAC寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入tx_bcn_4失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取tx_bcn_4失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试tx_config_27寄存器 */
        uint32_t addr2 = TX_FE_BASE + offsetof(struct tx_dac_if, tx_config_27);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入tx_config_27失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取tx_config_27失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_fe_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("TX FE寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入bcn_3失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取bcn_3失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_bcn_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试bcn_5寄存器 */
        uint32_t addr2 = RX_BCN_BASE + offsetof(struct rx_bcn, bcn_5);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入bcn_5失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取bcn_5失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_bcn_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX BCN寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入tm_config_1失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取tm_config_1失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_mod_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试tm_config_3寄存器 */
        uint32_t addr2 = TX_MOD_BASE + offsetof(struct tx_top, tm_config_3);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入tm_config_3失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取tm_config_3失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!tx_mod_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("TX MOD寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入ud_ctrl0失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取ud_ctrl0失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_mup_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试ud_ctrl7寄存器 */
        uint32_t addr2 = RX_MUP_BASE + offsetof(struct rx_mup, ud_ctrl7);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入ud_ctrl7失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取ud_ctrl7失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!rx_mup_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RX MUP寄存器测试完成，错误计数: %d\n", count);
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
            TK8710_LOG_CONFIG_ERROR("写入acm_ctrl1失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr1, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取acm_ctrl1失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!acm_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr1, (unsigned long)value_tmp, i, count);
        }

        /* 测试acm_ctrl11寄存器 */
        uint32_t addr2 = ACM_BASE + offsetof(struct acm, acm_ctrl11);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr2, i);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("写入acm_ctrl11失败: ret=%d\n", ret);
            continue;
        }
        
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr2, &value_tmp);
        if (ret != TK8710_OK) {
            TK8710_LOG_CONFIG_ERROR("读取acm_ctrl11失败: ret=%d\n", ret);
            continue;
        }
        
        if (value_tmp != (uint32_t)i) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!acm_reg = 0x%08lX, read=%lu not match write=%d, count=%d!!!!\n", 
                                   (unsigned long)addr2, (unsigned long)value_tmp, i, count);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("ACM寄存器测试完成，错误计数: %d\n", count);
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
    
    TK8710_LOG_CONFIG_INFO("测试RF频率寄存器，起始频率: 470MHz，步进: 50KHz\n");
    
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
            TK8710_LOG_CONFIG_ERROR("!!!!RF RX频率不匹配: write=0x%06X, read=0x%06X, freq=%u Hz, count=%d!!!!\n",
                                   (unsigned int)freq_reg, (unsigned int)rx_freq_reg, (unsigned int)freq_hz, count);
        }
        
        if (tx_freq_reg != freq_reg) {
            count++;
            TK8710_LOG_CONFIG_ERROR("!!!!RF TX频率不匹配: write=0x%06X, read=0x%06X, freq=%u Hz, count=%d!!!!\n",
                                   (unsigned int)freq_reg, (unsigned int)tx_freq_reg, (unsigned int)freq_hz, count);
        }
        
        /* 每10次打印一次进度 */
        if ((i + 1) % 10 == 0) {
            TK8710_LOG_CONFIG_INFO("RF频率测试进度: %d/100, 当前频率: %u Hz\n", i + 1, (unsigned int)freq_hz);
        }
    }
    
    TK8710_LOG_CONFIG_INFO("RF频率寄存器测试完成，错误计数: %d\n", count);
}
