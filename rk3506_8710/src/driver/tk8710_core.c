/**
 * @file tk8710_core.c
 * @brief TK8710 核心功能实现
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
#include <string.h>
#include <stddef.h>
#if TK8710_PLATFORM_HAS_FILE_IO
#include <unistd.h>
#else
#define usleep(us) TK8710DelayUs((uint32_t)(us))
#endif
#include <stdbool.h>

#define TK8710_TXADC_CONFIG_PATH "TxDC/txadc.txt"
#define TK8710_TXADC_STORAGE_KEY "txadc.bin"
#define TK8710_TXADC_STORAGE_SIZE (TK8710_MAX_ANTENNAS * 4U)
#define TK8710_INIT10_RF_READY_VALUE (1U << 2)
#define SLAVE_BCN_WATCHDOG_INTERVAL_MS 30000U
#define IRQ_MASK_WATCHDOG_INTERVAL_MS 100U
#define TK8710_IRQ_CTRL0_VALID_MASK 0x7FFU

/* 默认GPIO中断包装函数 */
static void default_gpio_irq_handler(void* user)
{
    (void)user;
    
    /* 调用Driver层中断处理函数 */
    TK8710_IRQHandler();
}

static int TK8710HardwareResetPulse(void)
{
#if defined(PLATFORM_TMS570)
    TK8710GpioWrite(1, 0);
    TK8710DelayMs(10U);
    TK8710GpioWrite(1, 1);
    return TK8710_OK;
#else
    int ret;

    ret = TK8710GpioSet("gpiochip0", 13, 0);
    if (ret != TK8710_OK) {
        return ret;
    }
    usleep(10000);
    return TK8710GpioSet("gpiochip0", 13, 1);
#endif
}

/* 速率模式参数查找表 */
static const RateModeParams g_rateModeParams[] = {
    /* mode, signalBwKHz, systemBwKHz(x10), maxUsers, maxPayloadLen */
    { 5,    2,    62500,   128,  260 },  /* 62.5KHz, 最大载荷260字节 */
    { 6,    4,   125000,   128,  260 },  /* 125KHz, 最大载荷260字节 */
    { 7,    8,   250000,   128,  260 },  /* 250KHz, 最大载荷260字节 */
    { 8,   16,   500000,   128,  260 },  /* 500KHz, 最大载荷260字节 */
    { 9,   32,   500000,    64,  520 },  /* 500KHz, 最大载荷520字节 */
    { 10,  64,   500000,    32,  520 },  /* 500KHz, 最大载荷520字节 */
    { 11, 128,   500000,    16,  520 },  /* 500KHz, 最大载荷520字节 */
    { 18, 128,   500000,    16,  520 },  /* 500KHz, 最大载荷520字节 */
};
#define RATE_MODE_COUNT (sizeof(g_rateModeParams) / sizeof(g_rateModeParams[0]))

/* 默认芯片配置 */
static const ChipConfig g_defaultChipConfig = {
    .bcn_agc     = 32,
    .interval    = 32,
    .tx_dly      = 1,
    .tx_fix_info = 0,
    .offset_adj  = 0,
    .tx_pre      = 0,
    .conti_mode  = 1,
    .bcn_scan    = 0,
    .ant_en      = 0xFF,
    .rf_sel      = 0xFF,
    .tx_bcn_en   = 0xff,
    .ts_sync     = 0,
    .rf_model    = 1,
    .bcnbits     = 10,
    .anoiseThe1  = 0,
    .power2rssi  = 0,
    .irq_ctrl0   = 0x7FF,
    .irq_ctrl1   = 0,
    .spiConfig   = NULL    /* 使用默认SPI配置 */
};

static uint8_t g_currentBcnBits = 10;
static volatile uint32_t g_lastInit10Config = TK8710_INIT10_RF_READY_VALUE;
static volatile uint8_t g_lastInit10ConfigValid = 0U;
static volatile uint8_t g_slaveBcnWatchdogActive = 0U;
static uint32_t g_slaveBcnWatchdogLastRecoveryMs = 0U;
static volatile uint8_t g_irqMaskWatchdogActive = 0U;
static uint8_t g_irqMaskWatchdogWorkType = TK8710_MODE_MASTER;
static uint8_t g_irqMaskWatchdogWorkMode = TK8710_WORK_MODE_CONTINUOUS;
static uint32_t g_irqMaskWatchdogExpected = TK8710_IRQ_CTRL0_VALID_MASK;
static uint32_t g_irqMaskWatchdogLastCheckMs = 0U;
static ChipConfig g_runtimeChipConfig;
static ChiprfConfig g_runtimeRfConfig;
static SpiConfig g_runtimeSpiConfig;
static uint8_t g_runtimeChipConfigValid = 0U;

static void TK8710RecordInit10Config(uint32_t value)
{
    g_lastInit10Config = value;
    g_lastInit10ConfigValid = 1U;
}

/* 注：工作类型、速率模式、天线使能、RF选择、广播用户数均已迁移到g_slotCfg中 */

static uint32_t tk8710BuildIrqCtrl0ForStart(uint8_t workType, const slotCfg_t* slotCfg)
{
    uint32_t irqCtrl0Data = TK8710_IRQ_CTRL0_VALID_MASK;

    if (slotCfg == NULL) {
        return irqCtrl0Data;
    }

    if (workType == TK8710_MODE_MASTER) {
        irqCtrl0Data = TK8710_S_IRQ_CTRL0_S0_IRQ_MASK_SET(irqCtrl0Data, 0U);
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S1_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_UD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S2_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S3_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
    } else if (workType == TK8710_MODE_LOOPBACK) {
        irqCtrl0Data = TK8710_S_IRQ_CTRL0_S0_IRQ_MASK_SET(irqCtrl0Data, 0U);
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_BRD_UD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_BRD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S1_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S2_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_UD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
    } else if (workType == TK8710_MODE_SLAVE) {
        irqCtrl0Data = TK8710_S_IRQ_CTRL0_RXBCN_IRQ_MASK_SET(irqCtrl0Data, 0U);
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_BRD_UD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_BRD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S1_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_S2_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_UD_IRQ_MASK_SET(irqCtrl0Data, 0U);
            irqCtrl0Data = TK8710_S_IRQ_CTRL0_MD_IRQ_MASK_SET(irqCtrl0Data, 0U);
        }
    }

    return irqCtrl0Data;
}

static int tk8710WriteAndVerifyIrqMask(uint32_t expected, const char* phase)
{
    uint32_t actual = TK8710_IRQ_CTRL0_VALID_MASK;
    int ret = TK8710_ERR;
    uint8_t attempt;

    expected &= TK8710_IRQ_CTRL0_VALID_MASK;
    for (attempt = 0U; attempt < 2U; attempt++) {
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, irq_ctrl0),
                             expected);
        if (ret != TK8710_OK) {
            continue;
        }

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, irq_ctrl0),
                            &actual);
        if ((ret == TK8710_OK) &&
            ((actual & TK8710_IRQ_CTRL0_VALID_MASK) == expected)) {
            TK8710_LOG_CORE_WARN("IRQ mask %s expected=0x%03X readback=0x%03X",
                                 phase,
                                 (unsigned int)expected,
                                 (unsigned int)(actual & TK8710_IRQ_CTRL0_VALID_MASK));
            return TK8710_OK;
        }
    }

    TK8710_LOG_CORE_ERROR("IRQ mask %s failed expected=0x%03X readback=0x%03X ret=%d",
                          phase,
                          (unsigned int)expected,
                          (unsigned int)(actual & TK8710_IRQ_CTRL0_VALID_MASK),
                          ret);
    return (ret == TK8710_OK) ? TK8710_ERR : ret;
}

static int tk8710ApplyIrqCtrl0ForStart(uint8_t workType,
                                      const slotCfg_t* slotCfg,
                                      const char* phase)
{
    uint32_t expected = tk8710BuildIrqCtrl0ForStart(workType, slotCfg);

    return tk8710WriteAndVerifyIrqMask(expected, phase);
}

static void tk8710ArmIrqMaskWatchdog(uint8_t workType, uint8_t workMode)
{
    g_irqMaskWatchdogWorkType = workType;
    g_irqMaskWatchdogWorkMode = workMode;
    g_irqMaskWatchdogExpected =
        tk8710BuildIrqCtrl0ForStart(workType, TK8710GetSlotConfig()) &
        TK8710_IRQ_CTRL0_VALID_MASK;
    g_irqMaskWatchdogLastCheckMs = TK8710GetTickMs();
    g_irqMaskWatchdogActive = 1U;
}

/**
 * @brief 获取当前速率模式
 * @return 当前速率模式
 */
static bool TK8710TxAdcConfigIsUnset(const TxAdcConfig txadc[TK8710_MAX_ANTENNAS])
{
    int i;

    if (txadc == NULL) {
        return true;
    }

    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        if (txadc[i].i != 0 || txadc[i].q != 0) {
            return false;
        }
    }

    return true;
}

static int TK8710LoadTxAdcConfigFromFile(TxAdcConfig txadc[TK8710_MAX_ANTENNAS])
{
#if TK8710_PLATFORM_HAS_FILE_IO
    FILE* file;
    char line[256];
    int index = 0;
    TxAdcConfig loaded[TK8710_MAX_ANTENNAS] = {0};

    if (txadc == NULL) {
        return TK8710_ERR;
    }

    file = fopen(TK8710_TXADC_CONFIG_PATH, "r");
    if (file == NULL) {
        TK8710_LOG_CORE_WARN("TXADC config is unset and %s cannot be opened",
                             TK8710_TXADC_CONFIG_PATH);
        return TK8710_ERR;
    }

    while (fgets(line, sizeof(line), file) != NULL && index < TK8710_MAX_ANTENNAS) {
        unsigned int i_value;
        unsigned int q_value;

        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r' ||
            line[0] == '#' || line[0] == '/') {
            continue;
        }

        if (sscanf(line, "0x%x, 0x%x", &i_value, &q_value) == 2 ||
            sscanf(line, "%x, %x", &i_value, &q_value) == 2) {
            loaded[index].i = (int16_t)(i_value & 0xFFFFu);
            loaded[index].q = (int16_t)(q_value & 0xFFFFu);
            index++;
        }
    }

    fclose(file);

    if (index != TK8710_MAX_ANTENNAS) {
        TK8710_LOG_CORE_WARN("TXADC config file %s has %d entries, expected %d",
                             TK8710_TXADC_CONFIG_PATH, index, TK8710_MAX_ANTENNAS);
        return TK8710_ERR;
    }

    memcpy(txadc, loaded, sizeof(loaded));
    TK8710_LOG_CORE_INFO("Loaded TXADC config from %s", TK8710_TXADC_CONFIG_PATH);
    return TK8710_OK;
#elif defined(PLATFORM_TMS570)
    uint8_t stored[TK8710_TXADC_STORAGE_SIZE];
    int index;

    if (txadc == NULL) {
        return TK8710_ERR;
    }
    if (TK8710PortStorageRead(TK8710_TXADC_STORAGE_KEY, 0U,
                              stored, sizeof(stored)) != 0) {
        TK8710_LOG_CORE_WARN("TXADC storage %s is not available",
                             TK8710_TXADC_STORAGE_KEY);
        return TK8710_ERR;
    }

    for (index = 0; index < TK8710_MAX_ANTENNAS; index++) {
        uint32_t offset = (uint32_t)index * 4U;
        uint16_t iValue = ((uint16_t)stored[offset] << 8) |
                          (uint16_t)stored[offset + 1U];
        uint16_t qValue = ((uint16_t)stored[offset + 2U] << 8) |
                          (uint16_t)stored[offset + 3U];
        txadc[index].i = (int16_t)iValue;
        txadc[index].q = (int16_t)qValue;
    }
    TK8710_LOG_CORE_INFO("Loaded TXADC config from platform storage");
    return TK8710_OK;
#else
    (void)txadc;
    return TK8710_ERR;
#endif
}

/**
 * @brief 鑾峰彇褰撳墠閫熺巼妯″紡
 * @return 褰撳墠閫熺巼妯″紡
 */
uint8_t TK8710GetRateMode(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    /* 获取当前中断结果中的速率序号，如果未初始化则使用第一个 */
    TK8710IrqResult irqResult;
    int ret = TK8710GetIrqResult(&irqResult);
    uint8_t currentIndex = 0;  /* 默认使用第一个 */
    
    if (ret == TK8710_OK) {
        currentIndex = irqResult.currentRateIndex;
    }
    
    /* 检查速率序号是否有效 */
    if (currentIndex >= slotCfg->rateCount) {
        currentIndex = 0;  /* 无效时使用第一个 */
    }
    
    return slotCfg->rateModes[currentIndex];
}


/**
 * @brief 获取广播用户数
 * @return 当前广播用户数
 */
uint8_t TK8710GetBrdUserNum(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    return slotCfg->brdUserNum;
}

/**
 * @brief 获取速率模式参数
 * @param rateMode 速率模式
 * @param params 输出参数结构体指针
 * @return 0-成功, 1-失败
 */
int TK8710GetRateModeParams(uint8_t rateMode, RateModeParams* params)
{
    size_t i;
    if (params == NULL) {
        return TK8710_ERR;
    }
    for (i = 0; i < RATE_MODE_COUNT; i++) {
        if (g_rateModeParams[i].mode == rateMode) {
            *params = g_rateModeParams[i];
            return TK8710_OK;
        }
    }
    return TK8710_ERR;
}


/**
 * @brief 获取当前工作类型
 * @return 当前工作类型
 */
uint8_t TK8710GetWorkType(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    return slotCfg->msMode;
}

/*============================================================================
 * RF寄存器读写内部函数
 *============================================================================*/

/**
 * @brief 写RF寄存器 (内部函数)
 * @param rfSel RF选择 (bit0-7对应RF0-7)
 * @param addr RF寄存器地址 (7位)
 * @param data 写入数据
 * @return 0-成功, 非0-失败
 */
int tk8710_rf_write(uint8_t rfSel, uint16_t addr, uint32_t data)
{
    int ret;
    s_init_9 init9;
    s_spi_cfg1 spiCfg1;
    s_spi_cfg2 spiCfg2;
    
    /* 1. 写mac->init_9 选择RF: (rf_sel << 8) + 0xff */
    // if (rfSel < 0xff) {
        init9.data = ((uint32_t)rfSel << 8) | 0xff;
        ret = TK8710SpiWriteReg(MAC_BASE + offsetof(struct mac, init_9), &init9.data, 1);
        if (ret != 0) return TK8710_ERR;
    // }
    
    /* 2. 写rx_fe->spi_cfg1: (0x1<<31)|(1<<15)|(addr<<8)|data */
    spiCfg1.data = ((uint32_t)0x1 << 31) | ((uint32_t)1 << 15) | ((uint32_t)(addr & 0xff) << 8) | (data & 0xff);
    ret = TK8710SpiWriteReg(RX_FE_BASE + offsetof(struct rx_top, spi_cfg1), &spiCfg1.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    /* 3. 写rx_fe->spi_cfg2 发送写命令: 0x80<<8 | 0x01 */
    spiCfg2.data = (0x80 << 8) | 0x01;
    ret = TK8710SpiWriteReg(RX_FE_BASE + offsetof(struct rx_top, spi_cfg2), &spiCfg2.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    return TK8710_OK;
}

/**
 * @brief 读RF寄存器 (内部函数)
 * @param rfSel RF选择 (bit0-7对应RF0-7)
 * @param addr RF寄存器地址 (7位)
 * @param data 读取数据输出
 * @return 0-成功, 非0-失败
 */
int tk8710_rf_read(uint8_t rfSel, uint16_t addr, uint32_t* data)
{
    int ret;
    s_init_9 init9;
    s_spi_cfg1 spiCfg1;
    s_spi_cfg2 spiCfg2;
    s_spi_res0 spiRes0;
    
    /* 1. 写mac->init_9 选择RF: (1 << (rf_sel + 8)) + 0xff */
    // init9.data = ((uint32_t)rfSel << 8) | 0xff;
    init9.data = (1 << (rfSel + 8)) + 0xff;
    ret = TK8710SpiWriteReg(MAC_BASE + offsetof(struct mac, init_9), &init9.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    /* 2. 写rx_fe->spi_cfg1 设置地址: (addr&0x7f)<<8 */
    spiCfg1.data = (addr & 0x7f) << 8;
    ret = TK8710SpiWriteReg(RX_FE_BASE + offsetof(struct rx_top, spi_cfg1), &spiCfg1.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    /* 3. 写rx_fe->spi_cfg2 发送读命令: 0x80<<8 | 0x01 */
    spiCfg2.data = (0x80 << 8) | 0x01;
    ret = TK8710SpiWriteReg(RX_FE_BASE + offsetof(struct rx_top, spi_cfg2), &spiCfg2.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    /* 4. 读rx_fe->spi_res0 获取结果 */
    ret = TK8710SpiReadReg(RX_FE_BASE + offsetof(struct rx_top, spi_res0), &spiRes0.data, 1);
    if (ret != 0) return TK8710_ERR;
    
    *data = spiRes0.data & 0xFFU;
    return TK8710_OK;
}

/**
 * @brief 初始化TK8710芯片
 * @param initConfig 初始化配置参数，为NULL时使用默认配置
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Init(const ChipConfig* initConfig)
{
    int ret;
    s_init_0 init0;
    s_init_5 init5;
    s_init_9 init9;
    // s_init_11 init11;
    s_irq_ctrl1 irqCtrl1;
    const ChipConfig* cfg = initConfig ? initConfig : &g_defaultChipConfig;

    if (cfg != &g_runtimeChipConfig) {
        g_runtimeChipConfig = *cfg;
        if (cfg->rfConfig != NULL) {
            g_runtimeRfConfig = *(const ChiprfConfig*)cfg->rfConfig;
            g_runtimeChipConfig.rfConfig = &g_runtimeRfConfig;
        }
        if (cfg->spiConfig != NULL) {
            g_runtimeSpiConfig = *cfg->spiConfig;
            g_runtimeChipConfig.spiConfig = &g_runtimeSpiConfig;
        }
        g_runtimeChipConfigValid = 1U;
    }

    ret = TK8710HardwareResetPulse();
    if (ret != TK8710_OK) {
        return ret;
    }
    
    
    /* 初始化默认日志系统（如果尚未初始化） */
    TK8710LogConfig_t defaultLogConfig = {
#if defined(PLATFORM_TMS570)
        .level = TK8710_LOG_WARN,
#else
        .level = TK8710_LOG_INFO,
#endif
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
#if defined(PLATFORM_TMS570)
        .enable_file_logging = 0,
#else
        .enable_file_logging = 1,
#endif
        .log_file_dir = NULL
    };
    TK8710LogInit(&defaultLogConfig);

    /* 初始化SPI接口 */
    SpiConfig spiConfigToUse;
    if (cfg->spiConfig != NULL) {
        /* 使用自定义SPI配置 */
        spiConfigToUse = *cfg->spiConfig;
        TK8710_LOG_CORE_INFO("Using custom SPI config: speed=%u, mode=%u, bits=%u", 
                             spiConfigToUse.speed, spiConfigToUse.mode, spiConfigToUse.bits);
    } else {
        /* 使用默认SPI配置 */
        spiConfigToUse.speed = 16000000;    /* 16MHz */
        spiConfigToUse.mode = 0;           /* Mode 0 */
        spiConfigToUse.bits = 8;           /* 8位数据 */
        spiConfigToUse.lsb_first = 0;      /* MSB优先 */
        spiConfigToUse.cs_pin = 0;          /* CS引脚0 */
        TK8710_LOG_CORE_INFO("Using default SPI config: 16MHz, Mode0, MSB");
    }
    TK8710SpiInit(&spiConfigToUse);
    
    usleep(20000);  /* 20ms等待RF完全打开和稳定 */
    /* SPI配置完成后复位芯片，确保复位操作能正常执行 */
    TK8710_LOG_CORE_INFO("Resetting TK8710 chip after SPI configuration...");
    int resetRet = TK8710Reset(2);  /* 复位状态机+寄存器 */
    if (resetRet != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TK8710 chip reset failed: %d", resetRet);
        return resetRet;
    }
    TK8710_LOG_CORE_INFO("TK8710 chip reset completed");
    
    /* 初始化默认GPIO中断 */
    TK8710GpioInit(0, TK8710_GPIO_EDGE_RISING, default_gpio_irq_handler, NULL);
    
    /* 使能GPIO中断 */
    TK8710GpioIrqEnable(0, 1);
    
    TK8710_LOG_CORE_INFO("TK8710 initializing...");

    // 读取并打印8700 FPGA版本
    s_obv_8 obv_8;
    TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, obv_8), &obv_8.data);
    TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, "8710 FPGA Version : %08lX", (unsigned long)TK8710_S_OBV_8_VERSION_GET(obv_8.data));

    /* 如果配置了射频参数，则进行射频初始化 */
    if (cfg->rfConfig != NULL) {
        const ChiprfConfig* rfCfg = (const ChiprfConfig*)cfg->rfConfig;
        TK8710_LOG_CORE_INFO("RF config provided, initializing RF (type=%d, freq=%u Hz)...", 
                             rfCfg->rftype, rfCfg->Freq);
        ret = TK8710RfConfig(rfCfg);
        ret = TK8710RfConfig(rfCfg);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("RF initialization failed: %d", ret);
            return ret;
        }
        TK8710_LOG_CORE_INFO("RF initialization completed");
    }
    /* 注意：中断回调由TK8710IrqInit设置，这里不需要重复设置 */

    /* 配置 init_0: bcn_agc, interval, tx_freq_dly */
    init0.data = TK8710_S_INIT_0_ENCODE(cfg->bcn_agc, cfg->interval, cfg->tx_dly);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_0), init0.data);
    if (ret != TK8710_OK) return ret;

    /* init_1, init_2, init_3, init_4, init_18 配置已移至 TK8710SetConfig TK8710_CFG_TYPE_SLOT_CFG */

    /* 配置 init_5: offset_adj, tx_pre, conti_mode, conti_scan */
    init5.data = TK8710_S_INIT_5_ENCODE(cfg->offset_adj, cfg->tx_pre, cfg->conti_mode, cfg->bcn_scan);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), init5.data);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_9: ant_en, rf_sel, tx_bcn_en */
    init9.data = TK8710_S_INIT_9_ENCODE(cfg->ant_en, cfg->rf_sel, cfg->tx_bcn_en);
    /* 更新antEn、rfSel和txBcnEn到g_slotCfg */
    {
        slotCfg_t* slotCfg = (slotCfg_t*)TK8710GetSlotConfig();
        slotCfg->antEn = cfg->ant_en;
        slotCfg->rfSel = cfg->rf_sel;
        slotCfg->txBcnAntEn = cfg->tx_bcn_en;
    }
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
    if (ret != TK8710_OK) return ret;
    g_currentBcnBits = cfg->bcnbits;
    uint32_t TmpBcnBits = cfg->bcnbits << 4;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x1814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x2814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x3814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x4814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x5814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x6814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x7814, TmpBcnBits);
    if (ret != TK8710_OK) return ret;
    // /* 配置 init_11: rf_type */
    // init11.data = 0;
    // init11 rf_type is packed explicitly below.
    // ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), init11.data);
    // if (ret != TK8710_OK) return ret;

    /* init_18 配置已移至 TK8710SetConfig TK8710_CFG_TYPE_SLOT_CFG */

    /* 配置 irq_ctrl0: 中断使能 (注释掉，由TK8710Start配置) */
    /* irqCtrl0.data = cfg->irq_ctrl0; */
    /* ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data); */
    /* if (ret != TK8710_OK) return ret; */

    /* 配置 irq_ctrl1: 中断清理 */
    irqCtrl1.data = cfg->irq_ctrl1;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl1), irqCtrl1.data);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_13: 初始化为0 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_13), 0);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_14: 初始化为0 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_14), 0);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_15: 初始化为0 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_15), 0);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_16: 初始化为0 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_16), 0);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_17: 初始化为0 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_17), 0);
    if (ret != TK8710_OK) return ret;
    
    /* 初始化默认日志系统（如果尚未初始化） */
#if defined(PLATFORM_TMS570)
    defaultLogConfig.level = TK8710_LOG_INFO;
#else
    defaultLogConfig.level = TK8710_LOG_INFO;
#endif
    TK8710LogInit(&defaultLogConfig);
    AcmCalibParams calibParams;
    calibParams.calibCount = 20;
    calibParams.snrThreshold = 28;

     int calibRet;
     int maxRetryCount = 3;
     int retryCount = 0;
     bool calibSuccess = false;

     /* 校准重试逻辑：如果有效校准次数小于目标校准次数，则重新校准 */
     while (retryCount < maxRetryCount && !calibSuccess) {

         ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, TK8710_DBG_OPT_GET, NULL, NULL);
         if (ret == TK8710_OK) {
             TK8710_LOG_CORE_INFO("ACM auto-gain get completed\n");
         } else {
             TK8710_LOG_CORE_INFO("ACM auto-gain get failed: ret=%d\n", ret);
         }

         TK8710_LOG_CORE_INFO("Start ACM calibration %d (target count: %d, SNR threshold: %d)...\n",
                             retryCount + 1, calibParams.calibCount, calibParams.snrThreshold);
        
         ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CALIBRATE, TK8710_DBG_OPT_EXE,
                             &calibParams, &calibRet);
        
         if (ret == TK8710_OK) {
             TK8710_LOG_CORE_INFO("ACM calibration %d completed, valid count: %d\n", retryCount + 1, calibRet);

             calibSuccess = true;

             /* 检查校准是否成功：有效校准次数是否达到目标校准次数 */
             if (calibRet >= calibParams.calibCount) {
                 TK8710_LOG_CORE_INFO("ACM calibration succeeded\n");
                 calibSuccess = true;
             } else {
                 TK8710_LOG_CORE_INFO("ACM calibration target not reached, retry required\n");
                 retryCount++;
             }
         } else {
             TK8710_LOG_CORE_INFO("ACM calibration %d failed: ret=%d\n", retryCount + 1, ret);
             retryCount++;
         }
     }

    /* 检查最终校准结果 */
    if (!calibSuccess) {
        TK8710_LOG_CORE_INFO("ACM calibration finally failed after %d retries\n", maxRetryCount);
        return TK8710_ERR;
    }
    /* 初始化默认日志系统（如果尚未初始化） */
    defaultLogConfig.level = TK8710_LOG_WARN;
    TK8710LogInit(&defaultLogConfig);
    
    TK8710_LOG_CORE_INFO("TK8710 initialized successfully");
    return TK8710_OK;
}

/**
 * @brief 芯片进入收发状态
 * @param workType 工作类型: 0=Slave, 1=Master
 * @param workMode 工作模式: 1=连续, 2=单次
 * @return 0-成功, 1-失败, 2-超时
 */
static int tk8710_start_once(uint8_t workType, uint8_t workMode)
{
    int ret;
    s_init_5 init5;
    s_trx_trig0 trig0;
    s_trx_trig1 trig1;
    
    /* 先读取init_5寄存器，设置conti_mode */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), &init5.data);
    if (ret != TK8710_OK) return ret;
    
    /* 设置连续模式: 1=连续, 0=单次 */
    init5.data = TK8710_S_INIT_5_CONTI_MODE_SET(init5.data, (workMode == TK8710_WORK_MODE_CONTINUOUS) ? 1U : 0U);
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), init5.data);
    if (ret != TK8710_OK) return ret;

    // /* 配置寄存器0x9810为0x03481400 */
    // ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9810, 0x03481400);
    // if (ret == TK8710_OK) {
    //     TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, "Set register 0x9810 = 0x03481400");
    // } else {
    //     TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to set register 0x9810: %d", ret);
    //     return ret;
    // }

    /* 配置寄存器0x980c为0x000FF200 */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x980c, 0x000FF200);
    if (ret == TK8710_OK) {
        TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, "Set register 0x980c = 0x000FF200");
    } else {
        TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to set register 0x980c: %d", ret);
        return ret;
    }


    /* 根据工作类型启动传输 */
    if (workType == TK8710_MODE_MASTER) {
        /* Master模式: 配置trx_trig0寄存器 (0x74) 启动主动传输 */
        {
            slotCfg_t* slotCfg = (slotCfg_t*)TK8710GetSlotConfig();
            slotCfg->msMode = TK8710_MODE_MASTER;
            
            /* 配置init12寄存器，启用本地同步功能 */
            {
                s_init_12 init12;
                ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), &init12.data);
                if (ret == TK8710_OK) {
                    init12.data = TK8710_S_INIT_12_LS_EN_SET(init12.data, 1U);
                    init12.data = TK8710_S_INIT_12_LS_MASTER_SET(init12.data, 1U); //1表示Master模式启用本地同步，0表示Slave模式启用本地同步
                    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), init12.data);
                    if (ret == TK8710_OK) {
                        TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, "Set init12.ls_en = 1 for local sync mode");
                    } else {
                        TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to set init12.ls_en: %d", ret);
                        return ret;
                    }
                } else {
                    TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to read init12 register: %d", ret);
                    return ret;
                }
            }

            ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_MASTER,
                                              slotCfg,
                                              "pre-trigger");
            if (ret != TK8710_OK) return ret;
        }
        trig0.data = TK8710_S_TRX_TRIG0_ENCODE(1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, trx_trig0), trig0.data);
    } else if (workType == TK8710_MODE_SLAVE) {
        /* Slave模式: 配置trx_trig1寄存器 (0x78) 启动被动传输 */
        {
            slotCfg_t* slotCfg = (slotCfg_t*)TK8710GetSlotConfig();
            slotCfg->msMode = TK8710_MODE_SLAVE;
            
            // /* 配置init12寄存器，启用本地同步功能 */
            // {
            //     s_init_12 init12;
            //     ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), &init12.data);
            //     if (ret == TK8710_OK) {
            //         init12 local sync enable/master fields are configured with pack macros when this block is enabled.
            //         ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), init12.data);
            //         if (ret == TK8710_OK) {
            //             TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, "Set init12.ls_en = 1 for local sync mode");
            //         } else {
            //             TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to set init12.ls_en: %d", ret);
            //             return ret;
            //         }
            //     } else {
            //         TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to read init12 register: %d", ret);
            //         return ret;
            //     }
            // }

            // /* 配置rx_fe_regs->ddc寄存器 */
            // {
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x0000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x1000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x2000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x3000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x4000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x5000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x6000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            //     ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_FE_BASE + offsetof(struct rx_top, ddc) + 0x7000, 0x1b33333);
            //     if (ret != TK8710_OK) return ret;
            // }
            
            ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_SLAVE,
                                              slotCfg,
                                              "pre-trigger");
            if (ret != TK8710_OK) return ret;
        }
        trig1.data = TK8710_S_TRX_TRIG1_ENCODE(1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, trx_trig1), trig1.data);
    } else if (workType == TK8710_MODE_LOOPBACK) {
        /* Loopback模式: 配置类似Master模式但启用回环功能 */
        {
            slotCfg_t* slotCfg = (slotCfg_t*)TK8710GetSlotConfig();
            
            /* 配置init12寄存器，启用loopback模式 */
            {
                s_init_12 init12;
                ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), &init12.data);
                if (ret == TK8710_OK) {
                    init12.data = TK8710_S_INIT_12_LOOP_SET(init12.data, 1U);  /* 启用loopback模式 */
                    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), init12.data);
                    if (ret == TK8710_OK) {
                        TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, "Set init12.loop = 1 for loopback mode");
                    } else {
                        TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to set init12.loop: %d", ret);
                        return ret;
                    }
                } else {
                    TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "Failed to read init12 register: %d", ret);
                    return ret;
                }
            }
            ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_12), 0x01011000);
            ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_LOOPBACK,
                                              slotCfg,
                                              "pre-trigger");
            if (ret != TK8710_OK) return ret;
            
        }
        trig0.data = TK8710_S_TRX_TRIG0_ENCODE(1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, trx_trig0), trig0.data);
    } else {
        TK8710_LOG_CORE_ERROR("Invalid work type: %d", workType);
        return TK8710_ERR;
    }

    if (ret != TK8710_OK) {
        return ret;
    }

    ret = tk8710ApplyIrqCtrl0ForStart(workType,
                                      TK8710GetSlotConfig(),
                                      "post-trigger");
    if (ret == TK8710_OK) {
        TK8710_LOG_CORE_INFO("Work started: type=%d, mode=%d", workType, workMode);
    }
    return ret;
}

int TK8710Start(uint8_t workType, uint8_t workMode)
{
    uint8_t enableSlaveBcnWatchdog =
        (workType == TK8710_MODE_SLAVE &&
         workMode == TK8710_WORK_MODE_CONTINUOUS) ? 1U : 0U;
    int ret;

    g_slaveBcnWatchdogActive = enableSlaveBcnWatchdog;
    if (enableSlaveBcnWatchdog != 0U) {
        g_slaveBcnWatchdogLastRecoveryMs = TK8710GetTickMs();
    }

    ret = tk8710_start_once(workType, workMode);
    if (ret == TK8710_OK) {
        tk8710ArmIrqMaskWatchdog(workType, workMode);
    } else {
        g_irqMaskWatchdogActive = 0U;
    }
    if (ret == TK8710_OK && enableSlaveBcnWatchdog != 0U) {
        if (g_slaveBcnWatchdogActive != 0U) {
            TK8710_LOG_CORE_INFO("Slave first-BCN watchdog started, recovery interval=%u ms",
                                 SLAVE_BCN_WATCHDOG_INTERVAL_MS);
        } else {
            TK8710_LOG_CORE_INFO("Slave received first RX_BCN during startup");
        }
    } else if (ret != TK8710_OK) {
        g_slaveBcnWatchdogActive = 0U;
    }

    return ret;
}

void TK8710NotifySlaveBcnReceived(void)
{
    if (g_slaveBcnWatchdogActive != 0U) {
        g_slaveBcnWatchdogActive = 0U;
        TK8710_LOG_CORE_INFO("Slave received RX_BCN, BCN watchdog stopped");
    }
}

void TK8710StartSlaveBcnWatchdog(void)
{
    if (TK8710GetWorkType() != TK8710_MODE_SLAVE) {
        return;
    }

    g_slaveBcnWatchdogLastRecoveryMs = TK8710GetTickMs();
    g_slaveBcnWatchdogActive = 1U;
    TK8710_LOG_CORE_INFO("Slave BCN watchdog restarted, recovery interval=%u ms",
                         SLAVE_BCN_WATCHDOG_INTERVAL_MS);
}

void TK8710ProcessRuntimeWatchdog(void)
{
    uint32_t nowMs;
    int ret;

    nowMs = TK8710GetTickMs();

    if ((g_irqMaskWatchdogActive != 0U) &&
        ((uint32_t)(nowMs - g_irqMaskWatchdogLastCheckMs) >=
         IRQ_MASK_WATCHDOG_INTERVAL_MS)) {
        uint32_t actualMask = TK8710_IRQ_CTRL0_VALID_MASK;

        g_irqMaskWatchdogLastCheckMs = nowMs;
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, irq_ctrl0),
                            &actualMask);
        if ((ret == TK8710_OK) &&
            ((actualMask & TK8710_IRQ_CTRL0_VALID_MASK) !=
             g_irqMaskWatchdogExpected)) {
            uint32_t init5Data = 0U;
            uint32_t triggerData = 0U;
            uint32_t irqStatus = 0U;
            uint16_t triggerAddress =
                (g_irqMaskWatchdogWorkType == TK8710_MODE_SLAVE) ?
                (uint16_t)(MAC_BASE + offsetof(struct mac, trx_trig1)) :
                (uint16_t)(MAC_BASE + offsetof(struct mac, trx_trig0));

            (void)TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                                MAC_BASE + offsetof(struct mac, init_5),
                                &init5Data);
            (void)TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                                triggerAddress,
                                &triggerData);
            (void)TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                                MAC_BASE + offsetof(struct mac, irq_res),
                                &irqStatus);
            TK8710_LOG_CORE_ERROR(
                "Runtime IRQ mask changed expected=0x%03X actual=0x%03X "
                "init5=0x%08X conti=%u trig=0x%08X irq=0x%08X",
                (unsigned int)g_irqMaskWatchdogExpected,
                (unsigned int)(actualMask & TK8710_IRQ_CTRL0_VALID_MASK),
                (unsigned int)init5Data,
                (unsigned int)TK8710_S_INIT_5_CONTI_MODE_GET(init5Data),
                (unsigned int)triggerData,
                (unsigned int)irqStatus);

            if ((init5Data == 0U) && (triggerData == 0U) &&
                (g_runtimeChipConfigValid != 0U)) {
                slotCfg_t slotCfgCopy = *TK8710GetSlotConfig();
                uint8_t workType = g_irqMaskWatchdogWorkType;
                uint8_t workMode = g_irqMaskWatchdogWorkMode;

                TK8710_LOG_CORE_ERROR(
                    "TK8710 runtime register state lost, rebuilding driver state");
                g_irqMaskWatchdogActive = 0U;
                ret = TK8710Init(&g_runtimeChipConfig);
                if (ret == TK8710_OK) {
                    ret = TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG,
                                          &slotCfgCopy);
                }
                if (ret == TK8710_OK) {
                    ret = TK8710Start(workType, workMode);
                }
                if (ret == TK8710_OK) {
                    TK8710_LOG_CORE_WARN(
                        "TK8710 runtime driver state rebuild completed");
                } else {
                    TK8710_LOG_CORE_ERROR(
                        "TK8710 runtime driver state rebuild failed: %d", ret);
                }
                return;
            }

            if ((g_irqMaskWatchdogWorkMode == TK8710_WORK_MODE_CONTINUOUS) &&
                (TK8710_S_INIT_5_CONTI_MODE_GET(init5Data) == 0U)) {
                init5Data = TK8710_S_INIT_5_CONTI_MODE_SET(init5Data, 1U);
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                                     MAC_BASE + offsetof(struct mac, init_5),
                                     init5Data);
                if (ret != TK8710_OK) {
                    TK8710_LOG_CORE_ERROR("Runtime conti_mode restore failed: %d", ret);
                }
            }

            (void)tk8710WriteAndVerifyIrqMask(g_irqMaskWatchdogExpected,
                                              "runtime-restore");
        }
    }

    if (g_slaveBcnWatchdogActive == 0U ||
        TK8710GetWorkType() != TK8710_MODE_SLAVE) {
        return;
    }

    if ((uint32_t)(nowMs - g_slaveBcnWatchdogLastRecoveryMs) <
        SLAVE_BCN_WATCHDOG_INTERVAL_MS) {
        return;
    }
    g_slaveBcnWatchdogLastRecoveryMs = nowMs;

    TK8710_LOG_CORE_WARN("Slave has not received RX_BCN for %u ms, restarting state machine",
                         SLAVE_BCN_WATCHDOG_INTERVAL_MS);

    ret = TK8710SpiReset(TK8710_RST_STATE_MACHINE);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("Slave BCN watchdog state-machine reset failed: %d", ret);
        return;
    }
    TK8710DelayMs(10U);

    ret = tk8710_start_once(TK8710_MODE_SLAVE, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("Slave BCN watchdog restart failed: %d", ret);
        return;
    }

    if (g_slaveBcnWatchdogActive != 0U) {
        TK8710_LOG_CORE_INFO("Slave BCN watchdog restart completed");
    }
}

/**
 * @brief 芯片快速进入收发状态
 * @param workType 工作类型: 0=Slave, 1=Master, 2=Loopback
 * @param workMode 工作模式: 1=连续, 2=单次
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710FastStart(uint8_t workType, uint8_t workMode)
{
    int ret;

    ret = TK8710FastStartPrepare(workType, workMode);
    if (ret != TK8710_OK) {
        return ret;
    }

    ret = TK8710FastStartTrigger(workType);
    if (ret == TK8710_OK) {
        TK8710_LOG_CORE_INFO("Work fast started: type=%d, mode=%d", workType, workMode);
    }
    return ret;
}

int TK8710FastStartPrepare(uint8_t workType, uint8_t workMode)
{
    int ret;
    slotCfg_t* slotCfg = (slotCfg_t*)TK8710GetSlotConfig();
    s_init_5 init5;

    g_irqMaskWatchdogWorkType = workType;
    g_irqMaskWatchdogWorkMode = workMode;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), &init5.data);
    if (ret != TK8710_OK) return ret;

    init5.data = TK8710_S_INIT_5_CONTI_MODE_SET(init5.data, (workMode == TK8710_WORK_MODE_CONTINUOUS) ? 1U : 0U);

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), init5.data);
    if (ret != TK8710_OK) return ret;

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x980c, 0x000FF200);
    if (ret != TK8710_OK) return ret;

    if (workType == TK8710_MODE_MASTER) {
        uint32_t bcnBits;
        s_init_12 init12;
        s_init_9 init9;

        if (slotCfg == NULL) return TK8710_ERR;
        slotCfg->msMode = TK8710_MODE_MASTER;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, init_9),
                            &init9.data);
        if (ret != TK8710_OK) return ret;

        init9.data = TK8710_S_INIT_9_ANT_EN_SET(init9.data, slotCfg->antEn);
        init9.data = TK8710_S_INIT_9_RF_SEL_SET(init9.data, slotCfg->rfSel);
        init9.data = TK8710_S_INIT_9_TX_BCN_ANT_EN_SET(init9.data, slotCfg->txBcnAntEn);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, init_9),
                             init9.data);
        if (ret != TK8710_OK) return ret;

        bcnBits = g_currentBcnBits << 4;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x1814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x2814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x3814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x4814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x5814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x6814, bcnBits);
        if (ret != TK8710_OK) return ret;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x7814, bcnBits);
        if (ret != TK8710_OK) return ret;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, init_12),
                            &init12.data);
        if (ret != TK8710_OK) return ret;

        init12.data = TK8710_S_INIT_12_LS_EN_SET(init12.data, 1U);
        init12.data = TK8710_S_INIT_12_LS_MASTER_SET(init12.data, 1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, init_12),
                             init12.data);
        if (ret != TK8710_OK) return ret;

        ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_MASTER,
                                          slotCfg,
                                          "fast-prepare");
        if (ret != TK8710_OK) return ret;
    } else if (workType == TK8710_MODE_LOOPBACK) {
        s_init_12 init12;

        if (slotCfg == NULL) return TK8710_ERR;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, init_12),
                            &init12.data);
        if (ret != TK8710_OK) return ret;

        init12.data = TK8710_S_INIT_12_LOOP_SET(init12.data, 1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, init_12),
                             init12.data);
        if (ret != TK8710_OK) return ret;

        ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_LOOPBACK,
                                          slotCfg,
                                          "fast-prepare");
        if (ret != TK8710_OK) return ret;
    } else if (workType == TK8710_MODE_SLAVE) {
        if (slotCfg == NULL) return TK8710_ERR;
        slotCfg->msMode = TK8710_MODE_SLAVE;

        ret = tk8710ApplyIrqCtrl0ForStart(TK8710_MODE_SLAVE,
                                          slotCfg,
                                          "fast-prepare");
        if (ret != TK8710_OK) return ret;
    } else {
        TK8710_LOG_CORE_ERROR("Invalid work type for fast start prepare: %d", workType);
        return TK8710_ERR;
    }

    return TK8710_OK;
}

int TK8710FastStartTrigger(uint8_t workType)
{
    int ret;

    if (workType == TK8710_MODE_MASTER || workType == TK8710_MODE_LOOPBACK) {
        s_trx_trig0 trig0;

        trig0.data = TK8710_S_TRX_TRIG0_ENCODE(1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, trx_trig0),
                             trig0.data);
    } else if (workType == TK8710_MODE_SLAVE) {
        s_trx_trig1 trig1;

        trig1.data = TK8710_S_TRX_TRIG1_ENCODE(1U);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, trx_trig1),
                             trig1.data);
    } else {
        TK8710_LOG_CORE_ERROR("Invalid work type for fast start: %d", workType);
        return TK8710_ERR;
    }

    if (ret != TK8710_OK) {
        return ret;
    }

    ret = tk8710ApplyIrqCtrl0ForStart(workType,
                                      TK8710GetSlotConfig(),
                                      "fast-trigger");
    if (ret == TK8710_OK) {
        tk8710ArmIrqMaskWatchdog(workType, g_irqMaskWatchdogWorkMode);
    }
    return ret;
}

/**
 * @brief 初始化芯片连接射频
 * @param initrfConfig 射频初始化配置参数
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710RfConfig(const ChiprfConfig* initrfConfig)
{
    int ret;
    int i;
    s_tx_config_29 txConfig29;
    uint32_t txFeAddr;
    uint8_t rfSel;
    TxAdcConfig txadcToUse[TK8710_MAX_ANTENNAS];
    
    TK8710_LOG_CORE_INFO("Starting RF initialization...");
    
    if (initrfConfig == NULL) {
        TK8710_LOG_CORE_ERROR("RF init config is NULL");
        return TK8710_ERR;
    }
    
    /* 从g_slotCfg获取RF选择 */
    rfSel = TK8710GetSlotConfig()->rfSel;
    TK8710_LOG_CORE_INFO("RF config: type=%d, freq=%u Hz, rxgain=0x%02X, txgain=0x%02X, rfSel=0x%02X", 
                        initrfConfig->rftype, initrfConfig->Freq, initrfConfig->rxgain, initrfConfig->txgain, rfSel);

    memcpy(txadcToUse, initrfConfig->txadc, sizeof(txadcToUse));
    if (TK8710TxAdcConfigIsUnset(txadcToUse)) {
        if (TK8710LoadTxAdcConfigFromFile(txadcToUse) != TK8710_OK) {
            TK8710_LOG_CORE_WARN("Using zero TXADC config");
        }
    }
    
    /* 配置mac.init_11: rf_type (射频数字接口类型) */
    {
        s_init_11 init11;
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), &init11.data);
        if (ret != TK8710_OK) return ret;
        init11.data = TK8710_S_INIT_11_RF_TYPE_SET(init11.data, initrfConfig->rftype);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), init11.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to set RF type: %d", ret);
            return ret;
        }
        TK8710_LOG_CORE_DEBUG("RF type set to: %d", TK8710_S_INIT_11_RF_TYPE_GET(init11.data));
    }
    
    /* 配置每根天线的txadc (i/q直流) */
    /* 每根天线的tx_fe地址 = TX_FE_BASE + antenna * 0x1000 */
    TK8710_LOG_CORE_DEBUG("Configuring TX ADC DC offsets for %d antennas", TK8710_MAX_ANTENNAS);
    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        txFeAddr = TX_FE_BASE + i * 0x1000;
        
        /* 读取tx_config_29寄存器 */
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, 
                            txFeAddr + offsetof(struct tx_dac_if, tx_config_29), 
                            &txConfig29.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to read TX config for antenna %d: %d", i, ret);
            return ret;
        }
        
        /* 配置tx_dci和tx_dcq (各16bit) */
        txConfig29.data = TK8710_S_TX_CONFIG_29_ENCODE(txadcToUse[i].q, txadcToUse[i].i);
        
        /* 写回tx_config_29寄存器 */
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                             txFeAddr + offsetof(struct tx_dac_if, tx_config_29), 
                             txConfig29.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to write TX DC offset for antenna %d: %d", i, ret);
            return ret;
        }
        
        TK8710_LOG_CORE_DEBUG("Antenna %d: DC I=0x%04X, DC Q=0x%04X", i,
                              (unsigned int)TK8710_S_TX_CONFIG_29_TX_DCI_GET(txConfig29.data),
                              (unsigned int)TK8710_S_TX_CONFIG_29_TX_DCQ_GET(txConfig29.data));
    }
    
    /* RF基础配置 */
    TK8710_LOG_CORE_DEBUG("Starting RF basic configuration sequence...");
    
    /* 1. RF关闭/复位序列 - 首次上电需要更完整的复位 */
    /* 复位序列1: 完全关闭 */
    ret = tk8710_rf_write(rfSel, RF_CMD_CLOSE_0 >> 8, RF_CMD_CLOSE_0 & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RF close 0 sequence failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RF close 0 sequence completed");
    
    /* 复位序列2: 复位状态 */
    ret = tk8710_rf_write(rfSel, RF_CMD_CLOSE_1 >> 8, RF_CMD_CLOSE_1 & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RF close 1 sequence failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RF close 1 sequence completed");
    
    /* 等待复位稳定 */
    usleep(10000);  /* 10ms等待复位完成 */

    /* 2. 采样率配置 (Sampling Rate) */
    uint16_t rx_filter_cmd;
    if (initrfConfig->rftype == TK8710_RF_TYPE_1255_1M) {
        rx_filter_cmd = RF_CMD_RX_FILTER_1M;
        TK8710_LOG_CORE_DEBUG("Using 1M RX filter configuration");
    } else {
        rx_filter_cmd = RF_CMD_RX_FILTER;
        TK8710_LOG_CORE_DEBUG("Using 32M RX filter configuration");
    }
    
    ret = tk8710_rf_write(rfSel, rx_filter_cmd >> 8, rx_filter_cmd & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RX filter configuration failed: %d", ret);
        return ret;
    }

    /* 3. DIG_BRIDGE设置 */
    ret = tk8710_rf_write(rfSel, RF_CMD_RX_BW >> 8, RF_CMD_RX_BW & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("DIG_BRIDGE setting failed: %d", ret);
        return ret;
    }
    
    /* 4. TX DAC带宽 */
    ret = tk8710_rf_write(rfSel, RF_CMD_TX_DAC_BW >> 8, RF_CMD_TX_DAC_BW & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TX DAC bandwidth configuration failed: %d", ret);
        return ret;
    }
    
    /* 5. CLK设置: bit[1] output clock enabled on pad CLK_OUT */
    ret = tk8710_rf_write(rfSel, RF_CMD_CLK_SETTING >> 8, RF_CMD_CLK_SETTING & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("Clock setting failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RF basic configuration completed");

    usleep(20000);  /* 20ms等待RF完全打开和稳定 */

    /* 6. RX频率配置 (24bit: MSB/MID/LSB) */
    double freq_step;
    uint32_t freq_reg;
    
    /* 根据射频类型选择频率步进 */
    if (initrfConfig->rftype == TK8710_RF_TYPE_1257_32M) {
        freq_step = RF_SX1257_FREQ_STEP;
    } else {
        freq_step = RF_SX1255_FREQ_STEP;
    }
    freq_reg = (uint32_t)((double)initrfConfig->Freq / freq_step);
    
    TK8710_LOG_CORE_INFO("Configuring frequency: %u Hz (step=%.2f, reg=0x%06X)", 
                        initrfConfig->Freq, freq_step, freq_reg);
    
    /* RX频率 */
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_RX_MSB >> 8, (freq_reg >> 16) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RX frequency MSB write failed: %d", ret);
        return ret;
    }
    
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_RX_MID >> 8, (freq_reg >> 8) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RX frequency MID write failed: %d", ret);
        return ret;
    }
    
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_RX_LSB >> 8, (freq_reg >> 0) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RX frequency LSB write failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RX frequency configuration completed");
    
    /* 7. RX增益配置 */
    ret = tk8710_rf_write(rfSel, RF_CMD_RX_GAIN >> 8, initrfConfig->rxgain);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RX gain configuration failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RX gain set to: 0x%02X", initrfConfig->rxgain);
    
    /* 8. 打开lna */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_10), 0);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("LNA enable failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("LNA enabled");

    /* 9. TX频率 */
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_TX_MSB >> 8, (freq_reg >> 16) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TX frequency MSB write failed: %d", ret);
        return ret;
    }
    
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_TX_MID >> 8, (freq_reg >> 8) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TX frequency MID write failed: %d", ret);
        return ret;
    }
    
    ret = tk8710_rf_write(rfSel, RF_CMD_FRF_TX_LSB >> 8, (freq_reg >> 0) & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TX frequency LSB write failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("TX frequency configuration completed");

    /* 10. TX增益配置 */
    ret = tk8710_rf_write(rfSel, RF_CMD_TX_GAIN >> 8, initrfConfig->txgain);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("TX gain configuration failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("TX gain set to: 0x%02X", initrfConfig->txgain);
    
    /* 11.  RF打开 */
    ret = tk8710_rf_write(rfSel, RF_CMD_CLOSE_F >> 8, RF_CMD_CLOSE_F & 0xFF);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("RF open failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("RF opened");
    
    /* 等待RF打开稳定 */
    usleep(20000);  /* 20ms等待RF完全打开和稳定 */

    /* 12. 打开PA */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                         MAC_BASE + offsetof(struct mac, init_10),
                         TK8710_INIT10_RF_READY_VALUE);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("PA enable failed: %d", ret);
        return ret;
    }
    TK8710RecordInit10Config(TK8710_INIT10_RF_READY_VALUE);
    TK8710_LOG_CORE_DEBUG("PA enabled");
    
    /* 等待PA稳定 */
    usleep(10000);  /* 10ms等待PA稳定 */
    
    /* 13. 配置rx_bcn.acm_ctrl30 (0x78) */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x9478, 0x11100018);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("BCN ACM control configuration failed: %d", ret);
        return ret;
    }
    TK8710_LOG_CORE_DEBUG("BCN ACM control configured");
    
    TK8710_LOG_CORE_INFO("RF initialization completed successfully");
    return ret;
}

/**
 * @brief 控制发送BCN的天线和bcnbits
 * @param bcnantennasel 发送BCN天线配置 (配置mac.init_9.tx_bcn_en)
 * @param bcnbits 发送BCN信号中的bcnbits (配置tx_fe.tx_bcn_0.bcn_bits)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Txbcnctl(uint8_t bcnantennasel, uint8_t bcnbits)
{
    int ret;
    s_init_9 init9;
    s_tm_bcn_0 tmBcn0;
    
    /* 1. 读取并配置mac.init_9.tx_bcn_en */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), &init9.data);
    if (ret != TK8710_OK) return ret;
    
    init9.data = TK8710_S_INIT_9_TX_BCN_ANT_EN_SET(init9.data, bcnantennasel);
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
    if (ret != TK8710_OK) return ret;
    
    /* 2. 读取并配置tx_top.tm_bcn_0.bcn_bits */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, TX_MOD_BASE + offsetof(struct tx_top, tm_bcn_0), &tmBcn0.data);
    if (ret != TK8710_OK) return ret;
    
    tmBcn0.data = TK8710_S_TM_BCN_0_BCN_BITS_SET(tmBcn0.data, bcnbits);
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, TX_MOD_BASE + offsetof(struct tx_top, tm_bcn_0), tmBcn0.data);
    
    return ret;
}

/**
 * @brief 芯片复位
 * @param rstType 复位类型: 1=仅复位状态机, 2=复位状态机+寄存器
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Reset(uint8_t rstType)
{
    int ret;
    uint8_t resetConfig = 0;

    g_slaveBcnWatchdogActive = 0U;
    g_irqMaskWatchdogActive = 0U;
    ret = TK8710HardwareResetPulse();
    if (ret != TK8710_OK) {
        return ret;
    }
    /* 根据复位类型设置复位配置 */
    switch (rstType) {
        case TK8710_RST_STATE_MACHINE:
            resetConfig = 0x01;  /* bit0: SM复位 */
            break;
        case TK8710_RST_ALL:
            resetConfig = 0x03;  /* SM+REG复位 */
            break;
        default:
            return TK8710_ERR;
    }
    
    /* 调用HAL层SPI复位命令 */
    ret = TK8710SpiReset(resetConfig);
    
    return (ret == 0) ? TK8710_OK : TK8710_ERR;
}

/**
 * @brief 写寄存器
 * @param regType 寄存器类型: 0=全局, 非0=RF寄存器(bit0-7对应RF0-7)
 * @param addr 寄存器地址 (16位)
 * @param data 写入数据 (32位)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710WriteReg(uint8_t regType, uint16_t addr, uint32_t data)
{
    int ret;
    
    if (regType == TK8710_REG_TYPE_GLOBAL) {
        /* 全局寄存器: 直接通过SPI写入 */
        ret = TK8710SpiWriteReg(addr, &data, 1);
        return (ret == 0) ? TK8710_OK : TK8710_ERR;
    } else {
        /* RF寄存器: 通过内部SPI接口写入 */
        return tk8710_rf_write(regType, addr, data);
    }
}

/**
 * @brief 读寄存器
 * @param regType 寄存器类型: 0=全局, 非0=RF寄存器(bit0-7对应RF0-7)
 * @param addr 寄存器地址 (16位)
 * @param data 读取数据输出指针 (32位)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710ReadReg(uint8_t regType, uint16_t addr, uint32_t* data)
{
    int ret;
    
    if (data == NULL) {
        return TK8710_ERR;
    }
    
    if (regType == TK8710_REG_TYPE_GLOBAL) {
        /* 全局寄存器: 直接通过SPI读取 */
        ret = TK8710SpiReadReg(addr, data, 1);
        return (ret == 0) ? TK8710_OK : TK8710_ERR;
    } else {
        /* RF寄存器: 通过内部SPI接口读取 */
        return tk8710_rf_read(regType, addr, data);
    }
}

int TK8710CheckAndRestoreInit10(void)
{
    uint32_t currentValue;
    uint32_t expectedValue = (g_lastInit10ConfigValid != 0U) ?
        g_lastInit10Config : TK8710_INIT10_RF_READY_VALUE;
    int ret;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        MAC_BASE + offsetof(struct mac, init_10),
                        &currentValue);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("init_10 check read failed: ret=%d", ret);
        return ret;
    }

    if (currentValue == expectedValue) {
        return TK8710_OK;
    }

    TK8710_LOG_CORE_ERROR("init_10 mismatch: read=0x%08X expected=0x%08X, restoring",
                          currentValue, expectedValue);
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                         MAC_BASE + offsetof(struct mac, init_10),
                         expectedValue);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("init_10 restore failed: ret=%d", ret);
    }
    return ret;
}

/**
 * @brief 写发送数据缓冲区
 * @param start_index 数据开始index (0-127数据用户, 128-143广播用户)
 * @param data 用户数据指针
 * @param len 数据长度 (mode5/6/7/8: 单块26字节,最大260; mode9/10/11: 单块26字节,最大520; mode18: 单块40字节,最大520)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710WriteBuffer(uint8_t start_index, const uint8_t* data, size_t len)
{
    int ret;
    
    if (data == NULL || len == 0) {
        return TK8710_ERR;
    }
    
    /* 调用HAL层SPI写Buffer命令 */
    ret = TK8710SpiWriteBuffer(start_index, data, (uint16_t)len);
    
    return (ret == 0) ? TK8710_OK : TK8710_ERR;
}

/**
 * @brief 读接收数据缓冲区
 * @param start_index 数据开始index (0-127数据用户, 128-143广播用户)
 * @param data_out 数据输出指针
 * @param len 数据长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710ReadBuffer(uint8_t start_index, uint8_t* data_out, size_t len)
{
    int ret;
    
    if (data_out == NULL || len == 0) {
        return TK8710_ERR;
    }
    
    /* 调用HAL层SPI读Buffer命令 */
    ret = TK8710SpiReadBuffer(start_index, data_out, (uint16_t)len);
    
    return (ret == 0) ? TK8710_OK : TK8710_ERR;
}

/**
 * @brief 写Efuse
 * @param start_bit 起始bit
 * @param data_bits 写入数据
 * @param len_bits 写入长度 (len_bits=1等价于单bit)
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710EfuseWrite(uint16_t start_bit, const uint8_t* data_bits, size_t len_bits)
{
    int ret;
    s_obv_6 obv6;
    s_efuse_0 efuse0;
    // int timeout;
    uint16_t i, j;
    
    if (data_bits == NULL) {
        return TK8710_ERR;
    }
    
    if (start_bit > 64) {
        TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "efuse start addr[%d] error\n", start_bit);
        return TK8710_ERR;
    }
    
    /* 按位写入efuse数据 */
    for (i = 0; i < len_bits; i++) {
        for (j = 0; j < 8; j++) {
            if (((data_bits[i] >> j) & 0x1) == 1) {
                /* 1. 等待efuse不忙 */
                // timeout = 1000;
                do {
                    ret = TK8710SpiReadReg(MAC_BASE + offsetof(struct mac, obv_6), &obv6.data, 1);
                    if (ret != 0) return TK8710_ERR;
                    // if (--timeout <= 0) return TK8710_TIMEOUT;
                } while (TK8710_S_OBV_6_EFUSE_BUSY_GET(obv6.data));
                
                /* 2. 写efuse_0: (1 << 12) | addr */
                efuse0.data = ((uint32_t)1 << 12) | (start_bit + i + j * 64);
                ret = TK8710SpiWriteReg(MAC_BASE + offsetof(struct mac, efuse_0), &efuse0.data, 1);
                if (ret != 0) return TK8710_ERR;
            }
        }
        TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, "efuse write addr[%d],data = %d\n", start_bit + i, data_bits[i]);
    }
    
    return TK8710_OK;
}

/**
 * @brief 读Efuse
 * @param start_bit 起始bit
 * @param data_bits 读取数据输出
 * @param len_bits 读取长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710EfuseRead(uint16_t start_bit, uint8_t* data_bits, size_t len_bits)
{
    int ret;
    s_efuse_0 efuse0;
    s_obv_6 obv6;
    uint16_t i;
    
    if (data_bits == NULL) {
        return TK8710_ERR;
    }
    
    if (start_bit > 64) {
        TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, "efuse start addr[%d] error\n", start_bit);
        return TK8710_ERR;
    }
    
    /* 按字节读取efuse数据 */
    for (i = 0; i < len_bits; i++) {
        /* 1. 写efuse_0: addr */
        efuse0.data = start_bit + i;
        ret = TK8710SpiWriteReg(MAC_BASE + offsetof(struct mac, efuse_0), &efuse0.data, 1);
        if (ret != 0) return TK8710_ERR;
        
        /* 2. 读efuse_0 (触发读取) */
        ret = TK8710SpiReadReg(MAC_BASE + offsetof(struct mac, efuse_0), &efuse0.data, 1);
        if (ret != 0) return TK8710_ERR;
        
        /* 3. 读obv_6 获取结果 */
        ret = TK8710SpiReadReg(MAC_BASE + offsetof(struct mac, obv_6), &obv6.data, 1);
        if (ret != 0) return TK8710_ERR;
        
        /* 4. 返回efuse_out */
        data_bits[i] = (uint8_t)TK8710_S_OBV_6_EFUSE_OUT_GET(obv6.data);
        
        TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, "efuse read addr[%d],data = %d\n", start_bit + i, data_bits[i]);
    }
    
    return TK8710_OK;
}
