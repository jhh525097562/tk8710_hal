#include "tk8710_scan_service.h"

#include "hal_api.h"
#include "tk8710_hal.h"
#include "driver/tk8710_driver_api.h"
#include "trm/trm_api.h"

#include <stdio.h>
#include <string.h>

static uint8_t ConvertSweepModeToTk8710Rate(int sweep_mode)
{
    switch (sweep_mode) {
        case 0: return TK8710_RATE_MODE_5;
        case 1: return TK8710_RATE_MODE_6;
        case 2: return TK8710_RATE_MODE_7;
        case 3: return TK8710_RATE_MODE_8;
        default:
            printf("[扫频] 未知的扫频模式: %d，使用默认模式8\n", sweep_mode);
            return TK8710_RATE_MODE_8;
    }
}

static void FillTxAdcConfig(ChiprfConfig* rf_config)
{
    static const uint16_t txadc_data[8][2] = {
        {0x0450, 0x0450}, {0x0a00, 0x1080}, {0x0750, 0x1500}, {0x0400, 0x0b00},
        {0x08a0, 0x07a0}, {0x0990, 0xff00}, {0x0850, 0x08c8}, {0x0950, 0x0a00}
    };

    for (int i = 0; i < 8; i++) {
        rf_config->txadc[i].i = (int16_t)txadc_data[i][0];
        rf_config->txadc[i].q = (int16_t)txadc_data[i][1];
    }
}

static void FillSlotDaM(slotCfg_t* slot_cfg, uint8_t rate_mode)
{
    switch (rate_mode) {
        case TK8710_RATE_MODE_5:
            slot_cfg->s1Cfg[0].da_m = 21492;
            slot_cfg->s2Cfg[0].da_m = 21492;
            slot_cfg->s3Cfg[0].da_m = 21492;
            break;
        case TK8710_RATE_MODE_6:
            slot_cfg->s1Cfg[0].da_m = 19728;
            slot_cfg->s2Cfg[0].da_m = 19728;
            slot_cfg->s3Cfg[0].da_m = 19728;
            break;
        case TK8710_RATE_MODE_7:
            slot_cfg->s1Cfg[0].da_m = 12000;
            slot_cfg->s2Cfg[0].da_m = 12000;
            slot_cfg->s3Cfg[0].da_m = 12000;
            break;
        case TK8710_RATE_MODE_8:
            slot_cfg->s1Cfg[0].da_m = 5600;
            slot_cfg->s2Cfg[0].da_m = 5600;
            slot_cfg->s3Cfg[0].da_m = 5600;
            break;
        default:
            slot_cfg->s1Cfg[0].da_m = 12000;
            slot_cfg->s2Cfg[0].da_m = 12000;
            slot_cfg->s3Cfg[0].da_m = 12000;
            break;
    }
}

static int DoFrequencySweep(uint32_t start_freq, uint32_t end_freq, int sweep_mode)
{
    if (start_freq == 0 || end_freq == 0 || start_freq > end_freq) {
        printf("[扫频] 错误: 无效的频率参数\n");
        return -1;
    }

    if (sweep_mode < 0 || sweep_mode > 3) {
        printf("[扫频] 错误: 无效的扫频模式 %d\n", sweep_mode);
        return -1;
    }

    uint8_t rate_mode = ConvertSweepModeToTk8710Rate(sweep_mode);
    printf("[扫频] 扫频模式: %d, 速率模式: %d\n", sweep_mode, rate_mode);
    printf("[扫频] 开始配置扫频前置参数\n");

    int trm_ret = TRM_Deinit();
    if (trm_ret != TRM_OK) {
        return TK8710_HAL_ERROR_RESET;
    }

    int ret = TK8710Reset(TK8710_RST_STATE_MACHINE);
    ret = TK8710Reset(TK8710_RST_ALL);
    if (ret != TK8710_OK) {
        return TK8710_HAL_ERROR_RESET;
    }

    ChiprfConfig rf_config;
    memset(&rf_config, 0, sizeof(rf_config));
    rf_config.rftype = TK8710_RF_TYPE_1255_1M;
    rf_config.Freq = start_freq;
    rf_config.rxgain = 0x7e;
    rf_config.txgain = 0x2a;
    FillTxAdcConfig(&rf_config);

    ChipConfig chip_config = {
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
        .rfConfig    = (struct ChiprfConfig_s*)&rf_config
    };

    TK8710HalInitCfg hal_config = {
        .chipInitCfg = &chip_config,
        .trmCfg = {
            .beamMaxUsers = 3000,
            .beamTimeoutMs = 20000,
            .maxFrameCount = 0,
            .onRxData = NULL,
            .onTxComplete = NULL
        }
    };

    TK8710HalError hal_ret = TK8710HalInit(&hal_config);
    if (hal_ret != TK8710_HAL_OK) {
        printf("[扫频] HAL初始化失败: %d\n", hal_ret);
        return -1;
    }

    slotCfg_t slot_cfg;
    memset(&slot_cfg, 0, sizeof(slot_cfg));
    slot_cfg.msMode = TK8710_MODE_MASTER;
    slot_cfg.plCrcEn = 0;
    slot_cfg.brdUserNum = 1;
    slot_cfg.antEn = 0xFF;
    slot_cfg.rfSel = 0xFF;
    slot_cfg.txBeamCtrlMode = 1;
    slot_cfg.txBcnAntEn = 0x7f;
    slot_cfg.rx_delay = 0;
    slot_cfg.md_agc = 1024;
    slot_cfg.brdFreq[0] = 20000.0;
    slot_cfg.frameTimeLen = 0;
    slot_cfg.rateCount = 1;
    slot_cfg.rateModes[0] = rate_mode;
    FillSlotDaM(&slot_cfg, rate_mode);

    slot_cfg.s0Cfg[0].byteLen = 0;
    slot_cfg.s0Cfg[0].centerFreq = start_freq;
    slot_cfg.s1Cfg[0].byteLen = 26;
    slot_cfg.s1Cfg[0].centerFreq = start_freq;
    slot_cfg.s2Cfg[0].byteLen = 26;
    slot_cfg.s2Cfg[0].centerFreq = start_freq;
    slot_cfg.s3Cfg[0].byteLen = 26;
    slot_cfg.s3Cfg[0].centerFreq = start_freq;

    hal_ret = TK8710HalCfg(&slot_cfg);
    if (hal_ret != TK8710_HAL_OK) {
        printf("[扫频] HAL时隙配置失败: %d\n", hal_ret);
        return -1;
    }

    hal_ret = TK8710HalStart();
    if (hal_ret != TK8710_HAL_OK) {
        printf("[扫频] HAL启动失败: %d\n", hal_ret);
        return -1;
    }

    ret = TRM_StartFrequencySweep(start_freq, end_freq, (uint8_t)sweep_mode, rate_mode);
    if (ret != TRM_OK) {
        printf("[扫频] TRM_StartFrequencySweep失败: %d\n", ret);
        return ret;
    }

    printf("[扫频] 扫频已启动: 起始=%u Hz, 结束=%u Hz, 模式=%d\n",
           start_freq, end_freq, sweep_mode);
    return 0;
}

int TK8710ScanStart(uint32_t start_freq, uint32_t end_freq, int sweep_mode)
{
    return DoFrequencySweep(start_freq, end_freq, sweep_mode);
}
