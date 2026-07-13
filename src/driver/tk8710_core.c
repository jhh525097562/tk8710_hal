/**
 * @file tk8710_core.c
 * @brief TK8710 核心功能实现
 */

#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_rf_regs.h"
#include "driver/tk8710_log.h"
#include "../port/tk8710_hal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define TK8710_TXADC_CONFIG_PATH "TxDC/txadc.txt"
#define TK8710_CALI_FACTOR_DIR "CaliFactor"
#define TK8710_INIT10_RF_READY_VALUE (1U << 2)
#ifndef PLATFORM_JTOOL
#define TK8710_RESET_GPIO_CHIP "gpiochip0"
#define TK8710_RESET_GPIO_LINE 13U
#endif

/* 默认GPIO中断包装函数 */
static void default_gpio_irq_handler(void* user)
{
    (void)user;
    
    /* 调用Driver层中断处理函数 */
    TK8710_IRQHandler();
}

static int TK8710HardwareResetPulse(void)
{
#ifdef PLATFORM_JTOOL
    return TK8710JtoolPowerReset();
#else
    int ret;

    ret = TK8710GpioSet(TK8710_RESET_GPIO_CHIP, TK8710_RESET_GPIO_LINE, 0);
    if (ret != TK8710_OK) {
        return ret;
    }

    usleep(10000);  /* 10ms等待复位完成 */

    ret = TK8710GpioSet(TK8710_RESET_GPIO_CHIP, TK8710_RESET_GPIO_LINE, 1);
    return ret;
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
static volatile uint8_t g_lastInit10ConfigValid = 0;

static int TK8710RemovePathTree(const char* path)
{
    if (path == NULL) {
        return TK8710_ERR;
    }

#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? TK8710_OK : TK8710_ERR;
    }

    if (attrs & FILE_ATTRIBUTE_READONLY) {
        SetFileAttributesA(path, attrs & ~FILE_ATTRIBUTE_READONLY);
    }

    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return DeleteFileA(path) ? TK8710_OK : TK8710_ERR;
    }

    {
        WIN32_FIND_DATAA findData;
        HANDLE findHandle;
        char searchPath[MAX_PATH];
        char childPath[MAX_PATH];
        int written;
        int result = TK8710_OK;

        written = snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
        if (written < 0 || written >= (int)sizeof(searchPath)) {
            return TK8710_ERR;
        }

        findHandle = FindFirstFileA(searchPath, &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }

                written = snprintf(childPath, sizeof(childPath), "%s\\%s", path, findData.cFileName);
                if (written < 0 || written >= (int)sizeof(childPath)) {
                    result = TK8710_ERR;
                    break;
                }

                if (TK8710RemovePathTree(childPath) != TK8710_OK) {
                    result = TK8710_ERR;
                    break;
                }
            } while (FindNextFileA(findHandle, &findData));

            if (result == TK8710_OK && GetLastError() != ERROR_NO_MORE_FILES) {
                result = TK8710_ERR;
            }

            FindClose(findHandle);
        } else {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
                return TK8710_ERR;
            }
        }

        if (result != TK8710_OK) {
            return result;
        }
    }

    return RemoveDirectoryA(path) ? TK8710_OK : TK8710_ERR;
#else
    struct stat st;
    DIR* dir;
    struct dirent* entry;
    int result = TK8710_OK;

    if (lstat(path, &st) != 0) {
        return (errno == ENOENT) ? TK8710_OK : TK8710_ERR;
    }

    if (!S_ISDIR(st.st_mode)) {
        return (remove(path) == 0) ? TK8710_OK : TK8710_ERR;
    }

    dir = opendir(path);
    if (dir == NULL) {
        return (errno == ENOENT) ? TK8710_OK : TK8710_ERR;
    }

    while ((entry = readdir(dir)) != NULL) {
        char childPath[1024];
        int written;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        written = snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);
        if (written < 0 || written >= (int)sizeof(childPath)) {
            result = TK8710_ERR;
            break;
        }

        if (TK8710RemovePathTree(childPath) != TK8710_OK) {
            result = TK8710_ERR;
            break;
        }
    }

    if (closedir(dir) != 0 && result == TK8710_OK) {
        result = TK8710_ERR;
    }

    if (result != TK8710_OK) {
        return result;
    }

    return (rmdir(path) == 0) ? TK8710_OK : TK8710_ERR;
#endif
}

static void TK8710RecordInit10Config(uint32_t value)
{
    g_lastInit10Config = value;
    g_lastInit10ConfigValid = 1;
}

/* 注：工作类型、速率模式、天线使能、RF选择、广播用户数均已迁移到g_slotCfg中 */

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
    
    *data = spiRes0.b.spi_rd_data;
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

    ret = TK8710HardwareResetPulse();
    if (ret != TK8710_OK) {
#ifdef PLATFORM_JTOOL
        printf("[JTOOL] hardware reset GPIO pulse failed: %d, continue without hard reset\n", ret);
#else
        return ret;
#endif
    }
    
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
    TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, "8710 FPGA Version : %08lX", (unsigned long)obv_8.b.version);

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
    init0.data = 0;
    init0.b.bcn_agc = cfg->bcn_agc & 0xFFF;
    init0.b.interval = cfg->interval & 0xFF;
    init0.b.tx_freq_dly = cfg->tx_dly & 0x07;  /* 从cfg获取tx_dly */
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_0), init0.data);
    if (ret != TK8710_OK) return ret;

    /* init_1, init_2, init_3, init_4, init_18 配置已移至 TK8710SetConfig TK8710_CFG_TYPE_SLOT_CFG */

    /* 配置 init_5: offset_adj, tx_pre, conti_mode, conti_scan */
    init5.data = 0;
    init5.b.offset_adj = cfg->offset_adj & 0x1FFF;
    init5.b.tx_pre = cfg->tx_pre & 0xFFF;
    init5.b.conti_mode = cfg->conti_mode & 0x01;
    init5.b.conti_scan = cfg->bcn_scan & 0x01;
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), init5.data);
    if (ret != TK8710_OK) return ret;

    /* 配置 init_9: ant_en, rf_sel, tx_bcn_en */
    init9.data = 0;
    init9.b.ant_en = cfg->ant_en;
    init9.b.rf_sel = cfg->rf_sel;
    init9.b.tx_bcn_ant_en = cfg->tx_bcn_en;
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
    // init11.b.rf_type = cfg->rf_model & 0x03;
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
    defaultLogConfig.level = TK8710_LOG_INFO;
    TK8710LogInit(&defaultLogConfig);
    AcmCalibParams calibParams;
    calibParams.calibCount = 100;
    calibParams.snrThreshold = 28;

    int calibRet;
    int maxRetryCount = 3;
    int retryCount = 0;
    bool calibSuccess = false;

    /* 校准重试逻辑：如果有效校准次数小于目标校准次数，则重新校准 */
    while (retryCount < maxRetryCount && !calibSuccess) {
        if (TK8710RemovePathTree(TK8710_CALI_FACTOR_DIR) != TK8710_OK) {
            TK8710_LOG_CORE_WARN("Remove %s before ACM calibration failed", TK8710_CALI_FACTOR_DIR);
        } else {
            TK8710_LOG_CORE_INFO("Removed %s before ACM calibration", TK8710_CALI_FACTOR_DIR);
        }

        ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, TK8710_DBG_OPT_GET, NULL, NULL);
        if (ret == TK8710_OK) {
            TK8710_LOG_CORE_INFO("ACM增益自动获取完成\n");
        } else {
            TK8710_LOG_CORE_INFO("ACM增益自动获取失败: ret=%d\n", ret);
        }

        TK8710_LOG_CORE_INFO("开始第%d次ACM校准 (目标校准次数: %d, SNR门限: %d)...\n",
                            retryCount + 1, calibParams.calibCount, calibParams.snrThreshold);

        ret = tk8710_rf_write(0xff, 0x8C7e >> 8, 0x9e);

        ret = TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_CALIBRATE, TK8710_DBG_OPT_EXE,
                            &calibParams, &calibRet);
        
        ret = tk8710_rf_write(0xff, 0x8C7e >> 8, 0x7e);
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
        if (ret != TK8710_OK) return ret;
        if (ret == TK8710_OK) {
            TK8710_LOG_CORE_INFO("第%d次校准完成，有效校准次数: %d\n", retryCount + 1, calibRet);
            calibSuccess = true;
            /* 检查校准是否成功：有效校准次数是否达到目标校准次数 */
            if (calibRet >= calibParams.calibCount) {
                TK8710_LOG_CORE_INFO("ACM校准成功\n");
                calibSuccess = true;
            } else {
                TK8710_LOG_CORE_INFO("ACM校准未达到目标次数，需要重新校准\n");
                retryCount++;
            }
        } else {
            TK8710_LOG_CORE_INFO("第%d次ACM校准失败: ret=%d\n", retryCount + 1, ret);
            retryCount++;
        }
    }
    
    /* 检查最终校准结果 */
    if (!calibSuccess) {
        TK8710_LOG_CORE_INFO("ACM校准最终失败：已重试%d次，仍未达到目标校准次数\n", maxRetryCount);
        return TK8710_ERR;
    }
        /* 初始化默认日志系统（如果尚未初始化） */
    defaultLogConfig.level = TK8710_LOG_WARN;
    TK8710LogInit(&defaultLogConfig);
    
    TK8710_LOG_CORE_INFO("TK8710 initialized successfully");
    return TK8710_OK;
}

#define SLAVE_BCN_WATCHDOG_INTERVAL_MS 30000u

static volatile uint8_t g_slaveBcnWatchdogActive = 0;
static uint32_t g_slaveBcnWatchdogLastRecoveryMs = 0;

/**
 * @brief 执行一次芯片收发启动
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
    init5.b.conti_mode = (workMode == TK8710_WORK_MODE_CONTINUOUS) ? 1 : 0;
    
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
                    init12.b.ls_en = 1;
                    init12.b.ls_master = 1; //1表示Master模式启用本地同步，0表示Slave模式启用本地同步
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

            /* 配置中断使能 */
            {
                s_irq_ctrl0 irqCtrl0;
                irqCtrl0.data = 0xFFFF;  /* 默认全部关闭 */
                
                /* Master模式中断配置 */
                irqCtrl0.b.s0_irq_mask = 0;  /* S0中断使能 */

                if (slotCfg->s1Cfg[0].byteLen > 0) {
                    irqCtrl0.b.s1_irq_mask = 0;  /* S1中断使能 */
                }
                
                if (slotCfg->s2Cfg[0].byteLen > 0) {
                    irqCtrl0.b.md_ud_irq_mask = 0;  /* MD UD中断使能 */
                    irqCtrl0.b.md_irq_mask = 0;     /* MD中断使能 */
                    irqCtrl0.b.s2_irq_mask = 0;     /* S2中断使能，用于TRM窗口任务 */
                }
                
                if (slotCfg->s3Cfg[0].byteLen > 0) {
                    irqCtrl0.b.s3_irq_mask = 0;  /* S3中断使能 */
                }

                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
                if (ret != TK8710_OK) return ret;
            }
        }
        trig0.data = 0;
        trig0.b.active_trans = 1;
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
            //         init12.b.ls_en = 1;
            //         init12.b.ls_master = 0; 
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
            
            /* 配置中断使能 */
            {
                s_irq_ctrl0 irqCtrl0;
                irqCtrl0.data = 0xFFFF;  /* 默认全部关闭 */
                
                /* Slave模式中断配置 */
                irqCtrl0.b.rxbcn_irq_mask = 0;  /* RX BCN中断使能 */
                // irqCtrl0.b.s0_irq_mask = 0;  /* S0中断使能 */
                if (slotCfg->s1Cfg[0].byteLen > 0) {
                    irqCtrl0.b.brd_ud_irq_mask = 0;  /* BRD UD中断使能 */
                    irqCtrl0.b.brd_irq_mask = 0;     /* BRD中断使能 */
                    irqCtrl0.b.s1_irq_mask = 0;  /* S1中断使能 */
                }
                
                if (slotCfg->s2Cfg[0].byteLen > 0) {
                    irqCtrl0.b.s2_irq_mask = 0;  /* S2中断使能 */
                }
                
                if (slotCfg->s3Cfg[0].byteLen > 0) {
                    irqCtrl0.b.md_ud_irq_mask = 0;  /* MD UD中断使能 */
                    irqCtrl0.b.md_irq_mask = 0;     /* MD中断使能 */
                    irqCtrl0.b.s3_irq_mask = 0;  /* S3中断使能 */
                }
                
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
                if (ret != TK8710_OK) return ret;
            }
        }
        trig1.data = 0;
        trig1.b.passive_trans = 1;
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
                    init12.b.loop = 1;  /* 启用loopback模式 */
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
            /* 配置中断使能 - 类似Master模式 */
            {
                s_irq_ctrl0 irqCtrl0;
                irqCtrl0.data = 0xFFFF;  /* 默认全部关闭 */
                
                /* Loopback模式中断配置 */
                irqCtrl0.b.s0_irq_mask = 0;  /* S0中断使能 */

                if (slotCfg->s1Cfg[0].byteLen > 0) {
                    irqCtrl0.b.brd_ud_irq_mask = 0;  /* BRD UD中断使能 */
                    irqCtrl0.b.brd_irq_mask = 0;     /* BRD中断使能 */
                    irqCtrl0.b.s1_irq_mask = 0; /* s1中断使能 */
                }
                
                if (slotCfg->s2Cfg[0].byteLen > 0) {
                    irqCtrl0.b.s2_irq_mask = 0;  /* S2中断使能 */
                }
                
                if (slotCfg->s3Cfg[0].byteLen > 0) {
                    irqCtrl0.b.md_ud_irq_mask = 0;  /* MD UD中断使能 */
                    irqCtrl0.b.md_irq_mask = 0;     /* MD中断使能 */
                }
                
                ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, irq_ctrl0), irqCtrl0.data);
                if (ret != TK8710_OK) return ret;
            }
            
        }
        trig0.data = 0;
        trig0.b.active_trans = 1;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, trx_trig0), trig0.data);
    } else {
        TK8710_LOG_CORE_ERROR("Invalid work type: %d", workType);
        return TK8710_ERR;
    }

    TK8710_LOG_CORE_INFO("Work started: type=%d, mode=%d", workType, workMode);
    return ret;
}

/**
 * @brief 芯片进入收发状态
 * @param workType 工作类型: 0=Slave, 1=Master
 * @param workMode 工作模式: 1=连续, 2=单次
 * @return 0-成功, 1-失败
 */
int TK8710Start(uint8_t workType, uint8_t workMode)
{
    uint8_t enableSlaveBcnWatchdog =
        (workType == TK8710_MODE_SLAVE &&
         workMode == TK8710_WORK_MODE_CONTINUOUS);
    int ret;

    g_slaveBcnWatchdogActive = enableSlaveBcnWatchdog;
    if (enableSlaveBcnWatchdog) {
        g_slaveBcnWatchdogLastRecoveryMs = TK8710GetTickMs();
    }

    ret = tk8710_start_once(workType, workMode);
    if (ret == TK8710_OK && enableSlaveBcnWatchdog) {
        if (g_slaveBcnWatchdogActive) {
            TK8710_LOG_CORE_INFO("Slave first-BCN watchdog started, recovery interval=%u ms",
                                 SLAVE_BCN_WATCHDOG_INTERVAL_MS);
        } else {
            TK8710_LOG_CORE_INFO("Slave received first RX_BCN during startup");
        }
    } else if (ret != TK8710_OK) {
        g_slaveBcnWatchdogActive = 0;
    }

    return ret;
}

void TK8710NotifySlaveBcnReceived(void)
{
    if (g_slaveBcnWatchdogActive) {
        g_slaveBcnWatchdogActive = 0;
        TK8710_LOG_CORE_INFO("Slave received RX_BCN, BCN watchdog stopped");
    }
}

void TK8710StartSlaveBcnWatchdog(void)
{
    if (TK8710GetWorkType() != TK8710_MODE_SLAVE) {
        return;
    }

    g_slaveBcnWatchdogLastRecoveryMs = TK8710GetTickMs();
    g_slaveBcnWatchdogActive = 1;
    TK8710_LOG_CORE_INFO("Slave BCN watchdog restarted, recovery interval=%u ms",
                         SLAVE_BCN_WATCHDOG_INTERVAL_MS);
}

void TK8710ProcessRuntimeWatchdog(void)
{
    uint32_t nowMs;
    int ret;

    if (!g_slaveBcnWatchdogActive || TK8710GetWorkType() != TK8710_MODE_SLAVE) {
        return;
    }

    nowMs = TK8710GetTickMs();
    if ((uint32_t)(nowMs - g_slaveBcnWatchdogLastRecoveryMs) <
        SLAVE_BCN_WATCHDOG_INTERVAL_MS) {
        return;
    }
    g_slaveBcnWatchdogLastRecoveryMs = nowMs;

    TK8710_LOG_CORE_WARN("Slave has not received first RX_BCN for %u ms, reset state machine and restart",
                         SLAVE_BCN_WATCHDOG_INTERVAL_MS);

    ret = TK8710SpiReset(TK8710_RST_STATE_MACHINE);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("Slave BCN watchdog state-machine reset failed: %d", ret);
        return;
    }
    TK8710DelayMs(10);

    ret = tk8710_start_once(TK8710_MODE_SLAVE, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("Slave BCN watchdog restart failed: %d", ret);
        return;
    }

    if (g_slaveBcnWatchdogActive) {
        TK8710_LOG_CORE_INFO("Slave BCN watchdog restart completed, continue waiting for RX_BCN");
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

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), &init5.data);
    if (ret != TK8710_OK) return ret;

    init5.b.conti_mode = (workMode == TK8710_WORK_MODE_CONTINUOUS) ? 1 : 0;

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_5), init5.data);
    if (ret != TK8710_OK) return ret;

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0x980c, 0x000FF200);
    if (ret != TK8710_OK) return ret;

    if (workType == TK8710_MODE_MASTER) {
        uint32_t bcnBits;
        s_init_12 init12;
        s_init_9 init9;
        s_irq_ctrl0 irqCtrl0;

        if (slotCfg == NULL) return TK8710_ERR;
        slotCfg->msMode = TK8710_MODE_MASTER;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, init_9),
                            &init9.data);
        if (ret != TK8710_OK) return ret;

        init9.b.ant_en = slotCfg->antEn;
        init9.b.rf_sel = slotCfg->rfSel;
        init9.b.tx_bcn_ant_en = slotCfg->txBcnAntEn;
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

        init12.b.ls_en = 1;
        init12.b.ls_master = 1;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, init_12),
                             init12.data);
        if (ret != TK8710_OK) return ret;

        irqCtrl0.data = 0xFFFF;
        irqCtrl0.b.s0_irq_mask = 0;
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0.b.s1_irq_mask = 0;
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0.b.md_ud_irq_mask = 0;
            irqCtrl0.b.md_irq_mask = 0;
            irqCtrl0.b.s2_irq_mask = 0;
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0.b.s3_irq_mask = 0;
        }

        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, irq_ctrl0),
                             irqCtrl0.data);
        if (ret != TK8710_OK) return ret;
    } else if (workType == TK8710_MODE_LOOPBACK) {
        s_init_12 init12;
        s_irq_ctrl0 irqCtrl0;

        if (slotCfg == NULL) return TK8710_ERR;

        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, init_12),
                            &init12.data);
        if (ret != TK8710_OK) return ret;

        init12.b.loop = 1;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, init_12),
                             init12.data);
        if (ret != TK8710_OK) return ret;

        irqCtrl0.data = 0xFFFF;
        irqCtrl0.b.s0_irq_mask = 0;
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0.b.brd_ud_irq_mask = 0;
            irqCtrl0.b.brd_irq_mask = 0;
            irqCtrl0.b.s1_irq_mask = 0;
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0.b.s2_irq_mask = 0;
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0.b.md_ud_irq_mask = 0;
            irqCtrl0.b.md_irq_mask = 0;
        }

        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, irq_ctrl0),
                             irqCtrl0.data);
        if (ret != TK8710_OK) return ret;
    } else if (workType == TK8710_MODE_SLAVE) {
        s_irq_ctrl0 irqCtrl0;

        if (slotCfg == NULL) return TK8710_ERR;
        slotCfg->msMode = TK8710_MODE_SLAVE;

        irqCtrl0.data = 0xFFFF;
        irqCtrl0.b.rxbcn_irq_mask = 0;
        if (slotCfg->s1Cfg[0].byteLen > 0) {
            irqCtrl0.b.brd_ud_irq_mask = 0;
            irqCtrl0.b.brd_irq_mask = 0;
            irqCtrl0.b.s1_irq_mask = 0;
        }
        if (slotCfg->s2Cfg[0].byteLen > 0) {
            irqCtrl0.b.s2_irq_mask = 0;
        }
        if (slotCfg->s3Cfg[0].byteLen > 0) {
            irqCtrl0.b.md_ud_irq_mask = 0;
            irqCtrl0.b.md_irq_mask = 0;
        }

        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, irq_ctrl0),
                             irqCtrl0.data);
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

        trig0.data = 0;
        trig0.b.active_trans = 1;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, trx_trig0),
                             trig0.data);
    } else if (workType == TK8710_MODE_SLAVE) {
        s_trx_trig1 trig1;

        trig1.data = 0;
        trig1.b.passive_trans = 1;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                             MAC_BASE + offsetof(struct mac, trx_trig1),
                             trig1.data);
    } else {
        TK8710_LOG_CORE_ERROR("Invalid work type for fast start: %d", workType);
        return TK8710_ERR;
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
        init11.b.rf_type = initrfConfig->rftype & 0x03;
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_11), init11.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to set RF type: %d", ret);
            return ret;
        }
        TK8710_LOG_CORE_DEBUG("RF type set to: %d", init11.b.rf_type);
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
        txConfig29.b.tx_dci = txadcToUse[i].i & 0xFFFF;
        txConfig29.b.tx_dcq = txadcToUse[i].q & 0xFFFF;
        
        /* 写回tx_config_29寄存器 */
        ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                             txFeAddr + offsetof(struct tx_dac_if, tx_config_29), 
                             txConfig29.data);
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to write TX DC offset for antenna %d: %d", i, ret);
            return ret;
        }
        
        TK8710_LOG_CORE_DEBUG("Antenna %d: DC I=0x%04X, DC Q=0x%04X", i, txConfig29.b.tx_dci, txConfig29.b.tx_dcq);
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
    
    init9.b.tx_bcn_ant_en = bcnantennasel;
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, MAC_BASE + offsetof(struct mac, init_9), init9.data);
    if (ret != TK8710_OK) return ret;
    
    /* 2. 读取并配置tx_top.tm_bcn_0.bcn_bits */
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, TX_MOD_BASE + offsetof(struct tx_top, tm_bcn_0), &tmBcn0.data);
    if (ret != TK8710_OK) return ret;
    
    tmBcn0.b.bcn_bits = bcnbits;
    
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

    g_slaveBcnWatchdogActive = 0;

    ret = TK8710HardwareResetPulse();
    if (ret != TK8710_OK) {
#ifdef PLATFORM_JTOOL
        printf("[JTOOL] hardware reset GPIO pulse failed: %d, fallback to SPI reset\n", ret);
#else
        return ret;
#endif
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
    uint32_t current_value;
    uint32_t expected_value = g_lastInit10ConfigValid ?
        g_lastInit10Config : TK8710_INIT10_RF_READY_VALUE;
    int ret;

    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                        MAC_BASE + offsetof(struct mac, init_10),
                        &current_value);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("init_10 check read failed: ret=%d", ret);
        return ret;
    }

    if (current_value == expected_value) {
        return TK8710_OK;
    }

    TK8710_LOG_CORE_ERROR("init_10 mismatch: read=0x%08X, expected=0x%08X; restoring",
                          current_value, expected_value);

    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                         MAC_BASE + offsetof(struct mac, init_10),
                         expected_value);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("init_10 restore failed: ret=%d", ret);
        return ret;
    }

    return TK8710_OK;
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
                } while (obv6.b.efuse_busy);
                
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
        data_bits[i] = (uint8_t)obv6.b.efuse_out;
        
        TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, "efuse read addr[%d],data = %d\n", start_bit + i, data_bits[i]);
    }
    
    return TK8710_OK;
}
