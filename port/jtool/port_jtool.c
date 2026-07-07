/**
 * @file tk8710_jtool.c
 * @brief TK8710 JTOOL(USB转SPI)平台移植层实现
 * @note 基于JTOOL设备进行PC端调试使用
 */

#include "../tk8710_hal.h"
#include <stdio.h>

uint64_t TK8710GetTimeUs(void);

#ifdef _WIN32
#include <windows.h>
#include "x64/jtool.h"

#if defined(_MSC_VER)
#pragma comment(lib, "x64/jtool.lib")
#endif
#endif

int TK8710JtoolOpen(const char* sn);

/*============================================================================
 * 8700/8710 SPI 协议定义
 *============================================================================*/

/* SPI 命令码 (Opcode) */
#define TK8710_SPI_CMD_RST          0x00    /* 复位命令 */
#define TK8710_SPI_CMD_WR_REG       0x01    /* 写寄存器 */
#define TK8710_SPI_CMD_RD_REG       0x02    /* 读寄存器 */
#define TK8710_SPI_CMD_WR_BUFF      0x03    /* 写Buffer */
#define TK8710_SPI_CMD_RD_BUFF      0x04    /* 读Buffer */
#define TK8710_SPI_CMD_SET_INFO     0x06    /* 设置信息 */
#define TK8710_SPI_CMD_GET_INFO     0x07    /* 获取信息 */

/* 复位参数 */
#define TK8710_RST_SM_ONLY          0x01    /* 仅复位状态机 */
#define TK8710_RST_SM_AND_REG       0x02    /* 复位状态机和寄存器 */

/* 协议参数 */
#define TK8710_SPI_MAX_SPEED        16000000    /* 最高SPI速率 16MHz */
#define TK8710_REG_SIZE             4           /* 寄存器大小 4字节 */
#define TK8710_NOP_BYTE             0x00        /* NOP占位字节 */

/* 最大传输缓冲区大小 */
#define TK8710_SPI_TX_BUF_SIZE      10240
#define TK8710_SPI_RX_BUF_SIZE      10240
#define TK8710_JTOOL_SPI_VCC_5V     0
#define TK8710_JTOOL_SPI_VCC_EQ_VIO 1
#define TK8710_JTOOL_SPI_VCC_OFF    2
#define TK8710_JTOOL_SPI_VIO_3V3    0
#define TK8710_JTOOL_SPI_VIO_1V8    1
#define TK8710_JTOOL_IO_VCC_5V      0
#define TK8710_JTOOL_IO_VCC_3V3     1
#define TK8710_JTOOL_IO_VCC_OFF     2
#define TK8710_JTOOL_IO_VIO_3V3     0
#define TK8710_JTOOL_IO_VIO_1V8     1
#define TK8710_JTOOL_RESET_IO       2
#define TK8710_JTOOL_ENABLE_POWER_RESET 1
#define TK8710_JTOOL_POWER_RESET_OFF_DELAY_MS 100
#define TK8710_JTOOL_POWER_RESET_ON_DELAY_MS 100

/*============================================================================
 * 私有变量
 *============================================================================*/

/* JTOOL设备句柄 */
static void* g_jtoolSpiHandle = NULL;
static void* g_jtoolIoHandle = NULL;
static char g_jtoolSn[64] = {0};
static uint8_t g_jtoolIoUsesSpiHandle = 0;
static uint8_t g_jtoolIoUnavailable = 0;

/* SPI配置参数 - 8710协议要求: Mode0 (CPOL=0,CPHA=0), MSB first */
static SPICK_TYPE g_spiCkType = LOW_1EDG;
static SPIFIRSTBIT_TYPE g_spiFirstBit = ENDIAN_MSB;

/* SPI传输缓冲区 */
static uint8_t g_spiTxBuf[TK8710_SPI_TX_BUF_SIZE];
static uint8_t g_spiRxBuf[TK8710_SPI_RX_BUF_SIZE];

/* 中断回调 */
static TK8710GpioIrqCallback g_halIrqCallback = NULL;
static void* g_halUserContext = NULL;

static int TK8710JtoolEnsureSpiOpen(void)
{
#ifdef _WIN32
    if (g_jtoolSpiHandle == NULL && TK8710JtoolOpen(NULL) != 0) {
        return -1;
    }

    return 0;
#else
    return -1;
#endif
}

static int TK8710JtoolEnsureOpen(void)
{
#ifdef _WIN32
    if (TK8710JtoolEnsureSpiOpen() != 0) {
        return -1;
    }

    (void)JSPISetVio(g_jtoolSpiHandle, TK8710_JTOOL_SPI_VIO_3V3);
    (void)JSPISetVcc(g_jtoolSpiHandle, TK8710_JTOOL_SPI_VCC_EQ_VIO);
    return 0;
#else
    return -1;
#endif
}

static int TK8710JtoolEnsureIoOpen(void)
{
#ifdef _WIN32
    if (g_jtoolIoUnavailable) {
        return -1;
    }

    if (g_jtoolIoHandle == NULL) {
        if (g_jtoolSn[0] != '\0') {
            g_jtoolIoHandle = DevOpen(dev_io, g_jtoolSn, 0);
        }
        if (g_jtoolIoHandle == NULL) {
            g_jtoolIoHandle = DevOpen(dev_io, NULL, 0);
        }
        g_jtoolIoUsesSpiHandle = 0;

        if (g_jtoolIoHandle == NULL) {
            if (TK8710JtoolEnsureSpiOpen() != 0) {
                return -1;
            }
            g_jtoolIoHandle = g_jtoolSpiHandle;
            g_jtoolIoUsesSpiHandle = 1;
        }
    }

    if (g_jtoolIoHandle == NULL) {
        return -1;
    }

    if (g_jtoolIoUsesSpiHandle) {
        (void)JSPISetVio(g_jtoolIoHandle, TK8710_JTOOL_SPI_VIO_3V3);
        (void)JSPISetVcc(g_jtoolIoHandle, TK8710_JTOOL_SPI_VCC_EQ_VIO);
    } else {
        (void)JIOSetVio(g_jtoolIoHandle, TK8710_JTOOL_IO_VIO_3V3);
        (void)JIOSetVcc(g_jtoolIoHandle, TK8710_JTOOL_IO_VCC_3V3);
    }
    return 0;
#else
    return -1;
#endif
}

static int TK8710JtoolSetIoLevel(uint32_t ionum, uint8_t level)
{
#ifdef _WIN32
    ErrorType err;

    if (TK8710JtoolEnsureIoOpen() != 0) {
        printf("[JTOOL] open dev_io failed for IO%u\n", (unsigned int)ionum);
        return -1;
    }

    err = IOSetOutWithVal(g_jtoolIoHandle, ionum, 1, level ? 1 : 0);
    if (err != ErrNone) {
        printf("[JTOOL] set IO%u=%u failed via %s handle, err=0x%X\n",
               (unsigned int)ionum,
               (unsigned int)(level ? 1 : 0),
               g_jtoolIoUsesSpiHandle ? "dev_spi" : "dev_io",
               (unsigned int)err);
        g_jtoolIoUnavailable = 1;
        if (g_jtoolIoUsesSpiHandle) {
            g_jtoolIoHandle = NULL;
            g_jtoolIoUsesSpiHandle = 0;
        }
        return -1;
    }

    return 0;
#else
    (void)ionum;
    (void)level;
    return -1;
#endif
}

static void TK8710JtoolReleaseResetIo(void)
{
#ifdef _WIN32
    if (!g_jtoolIoUnavailable && g_jtoolIoHandle != NULL) {
        (void)IOSetOutWithVal(g_jtoolIoHandle, TK8710_JTOOL_RESET_IO, 1, 1);
    }
#endif
}

/* JTOOL中断回调包装函数 */
static void JtoolIntCallback(void)
{
    /* 声明外部中断处理函数 */
    extern void TK8710_IRQHandler(void);
    
    /* 调用驱动层中断处理 */
    TK8710_IRQHandler();
}

/**
 * @brief 扫描并打开JTOOL设备
 * @param sn 设备序列号，NULL表示打开第一个找到的设备
 * @return 0-成功, 非0-失败
 */
int TK8710JtoolOpen(const char* sn)
{
#ifdef _WIN32
    int devCnt = 0;
    char* devList;
    char snBuf[64] = {0};
    char* snStart;
    char* snEnd;
    
    /* 扫描SPI设备 */
    devList = DevicesScan(dev_spi, &devCnt);
    if (devCnt <= 0 || devList == NULL) {
        return -1;
    }
    
    /* 打开设备 */
    if (sn != NULL) {
        /* 使用用户指定的SN */
        strncpy(g_jtoolSn, sn, sizeof(g_jtoolSn) - 1);
        g_jtoolSn[sizeof(g_jtoolSn) - 1] = '\0';
        g_jtoolSpiHandle = DevOpen(dev_spi, g_jtoolSn, 0);
    } else {
        /* 从设备列表中解析SN: "JTool-SPI (SN:XXXXXXXX) (ID:0)" */
        snStart = strstr(devList, "SN:");
        if (snStart != NULL) {
            snStart += 3;  /* 跳过 "SN:" */
            snEnd = strchr(snStart, ')');
            if (snEnd != NULL && (snEnd - snStart) < 64) {
                strncpy(snBuf, snStart, snEnd - snStart);
                snBuf[snEnd - snStart] = '\0';
                strncpy(g_jtoolSn, snBuf, sizeof(g_jtoolSn) - 1);
                g_jtoolSn[sizeof(g_jtoolSn) - 1] = '\0';
                g_jtoolSpiHandle = DevOpen(dev_spi, g_jtoolSn, 0);
            }
        }
        
        /* 如果解析失败，尝试使用NULL打开第一个设备 */
        if (g_jtoolSpiHandle == NULL) {
            g_jtoolSpiHandle = DevOpen(dev_spi, NULL, 0);
            g_jtoolSn[0] = '\0';
        }
    }
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }

    return 0;
#else
    (void)sn;
    return -1;
#endif
}

/**
 * @brief 关闭JTOOL设备
 * @return 0-成功, 非0-失败
 */
int TK8710JtoolClose(void)
{
#ifdef _WIN32
    if (g_jtoolIoHandle != NULL && !g_jtoolIoUsesSpiHandle) {
        DevClose(g_jtoolIoHandle);
    }
    g_jtoolIoHandle = NULL;
    g_jtoolIoUsesSpiHandle = 0;
    g_jtoolIoUnavailable = 0;

    if (g_jtoolSpiHandle != NULL) {
        DevClose(g_jtoolSpiHandle);
        g_jtoolSpiHandle = NULL;
    }
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief 初始化SPI接口
 * @note 8710协议要求: Mode0 (CPOL=0, CPHA=0), MSB first, 最高16MHz
 */
int TK8710SpiInit(const SpiConfig* cfg)
{
    if (cfg == NULL) {
        return -1;
    }
    
#ifdef _WIN32
    ErrorType err;
    uint8_t speedVal;
    uint32_t actualSpeed;
    
    if (TK8710JtoolEnsureOpen() != 0) {
        return -1;
    }

    /* 8710协议固定使用 Mode 0 (CPOL=0, CPHA=0), MSB first */
    g_spiCkType = LOW_1EDG;
    g_spiFirstBit = ENDIAN_MSB;
    
    /* 限制最高速度为16MHz (协议要求) */
    actualSpeed = (cfg->speed > TK8710_SPI_MAX_SPEED) ? TK8710_SPI_MAX_SPEED : cfg->speed;
    
    /* 设置SPI速度 (根据频率映射到JTOOL速度值) */
    /* JTOOL速度映射: 0=1MHz, 1=2MHz, 2=4MHz, 3=8MHz, 4=16MHz, 5=32MHz */
    if (actualSpeed >= 32000000) {
        speedVal = 5;  /* 32MHz */
    } else if (actualSpeed >= 16000000) {
        speedVal = 4;  /* 16MHz */
    } else if (actualSpeed >= 8000000) {
        speedVal = 3;  /* 8MHz */
    } else if (actualSpeed >= 4000000) {
        speedVal = 2;  /* 4MHz */
    } else if (actualSpeed >= 2000000) {
        speedVal = 1;  /* 2MHz */
    } else {
        speedVal = 0;  /* 1MHz */
    }
    
    err = JSPISetSpeed(g_jtoolSpiHandle, speedVal);
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)cfg;
    return -1;
#endif
}

/**
 * @brief SPI写数据
 */
int TK8710SpiWrite(const uint8_t* tx, size_t len)
{
    if (tx == NULL || len == 0) {
        return -1;
    }
    
#ifdef _WIN32
    ErrorType err;
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    err = SPIWriteOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, (uint32_t)len, (uint8_t*)tx);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)tx;
    (void)len;
    return -1;
#endif
}

/**
 * @brief SPI读数据
 */
int TK8710SpiRead(uint8_t* rx, size_t len)
{
    if (rx == NULL || len == 0) {
        return -1;
    }
    
#ifdef _WIN32
    ErrorType err;
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    err = SPIReadOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, (uint32_t)len, rx);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)rx;
    (void)len;
    return -1;
#endif
}

/**
 * @brief SPI全双工读写
 */
int TK8710SpiTransfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    if (tx == NULL || rx == NULL || len == 0) {
        return -1;
    }
    
#ifdef _WIN32
    ErrorType err;
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    err = SPIWriteRead(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, (uint32_t)len, (uint8_t*)tx, rx);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)tx;
    (void)rx;
    (void)len;
    return -1;
#endif
}

/**
 * @brief SPI片选控制
 * @note JTOOL的SPI接口自动管理CS，此函数为兼容性保留
 */
void TK8710SpiCsControl(uint8_t active)
{
    /* JTOOL SPI接口在每次传输时自动控制CS */
    /* 如果需要手动控制，可以使用IO接口 */
    (void)active;
}

/**
 * @brief 初始化GPIO中断
 */
int TK8710GpioInit(int pin, TK8710GpioEdge edge, TK8710GpioIrqCallback cb, void* user)
{
#ifdef _WIN32
    ErrorType err;
    INT_TYPE intType;
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    /* 保存回调 */
    g_halIrqCallback = cb;
    g_halUserContext = user;
    
    /* 映射边沿类型 */
    switch (edge) {
        case TK8710_GPIO_EDGE_RISING:  intType = INT_RISE; break;
        case TK8710_GPIO_EDGE_FALLING: intType = INT_FALL; break;
        case TK8710_GPIO_EDGE_BOTH:    intType = INT_RISE_FALL; break;
        default: intType = INT_RISE; break;
    }
    
    /* 注册中断回调 */
    err = SPIRegisterIntCallback(g_jtoolSpiHandle, intType, JtoolIntCallback);
    if (err != ErrNone) {
        return -1;
    }
    
    (void)pin;
    return 0;
#else
    g_halIrqCallback = cb;
    g_halUserContext = user;
    (void)gpioPin;
    (void)edge;
    return -1;
#endif
}

/**
 * @brief GPIO中断使能控制
 */
int TK8710GpioIrqEnable(uint8_t gpioPin, uint8_t enable)
{
#ifdef _WIN32
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    if (!enable) {
        SPICloseIntCallback(g_jtoolSpiHandle);
    }
    
    (void)gpioPin;
    return 0;
#else
    (void)gpioPin;
    (void)enable;
    return -1;
#endif
}

/**
 * @brief GPIO输出控制
 * @note 使用JTOOL的IO功能
 */
void TK8710GpioWrite(int pin, uint8_t level)
{
#ifdef _WIN32
    (void)TK8710JtoolSetIoLevel((uint32_t)pin, level);
#else
    (void)pin;
    (void)level;
#endif
}

/**
 * @brief GPIO输入读取
 */
uint8_t TK8710GpioRead(int pin)
{
#ifdef _WIN32
    BOOL val = 0;
    
    if (TK8710JtoolEnsureIoOpen() == 0) {
        /* 设置为输入模式 */
        IOSetIn(g_jtoolIoHandle, (uint32_t)pin, 0, 0);
        /* 读取值 */
        IOGetInVal(g_jtoolIoHandle, (uint32_t)pin, &val);
    }
    
    return val ? 1 : 0;
#else
    (void)pin;
    return 0;
#endif
}

int TK8710GpioSet(const char* chipPath, unsigned int lineOffset, uint8_t level)
{
#ifdef _WIN32
    (void)chipPath;
    return TK8710JtoolSetIoLevel(lineOffset, level);
#else
    (void)chipPath;
    (void)lineOffset;
    (void)level;
    return -1;
#endif
}

int TK8710GpioGet(const char* chipPath, unsigned int lineOffset)
{
#ifdef _WIN32
    (void)chipPath;
    return (int)TK8710GpioRead((int)lineOffset);
#else
    (void)chipPath;
    (void)lineOffset;
    return -1;
#endif
}

void TK8710Rk3506Cleanup(void)
{
#ifdef _WIN32
    (void)TK8710JtoolClose();
#endif
}

/**
 * @brief 毫秒延时
 */
void TK8710DelayMs(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    (void)ms;
#endif
}

/**
 * @brief 微秒延时
 */
void TK8710DelayUs(uint32_t us)
{
#ifdef _WIN32
    /* Windows下使用高精度计时器实现微秒延时 */
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    do {
        QueryPerformanceCounter(&end);
    } while ((end.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart < us);
#else
    (void)us;
#endif
}

/**
 * @brief 获取系统时间戳
 */
int TK8710SleepUntilUs(uint64_t targetUs)
{
    uint64_t nowUs = TK8710GetTimeUs();

    if (targetUs <= nowUs) {
        return 0;
    }

    TK8710DelayUs((uint32_t)(targetUs - nowUs));
    return 0;
}

uint32_t TK8710GetTickMs(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    return 0;
#endif
}

/**
 * @brief 进入临界区
 * @note PC端单线程环境下一般不需要
 */
void TK8710EnterCritical(void)
{
    /* PC端通常不需要临界区保护 */
}

/**
 * @brief 退出临界区
 */
void TK8710ExitCritical(void)
{
    /* PC端通常不需要临界区保护 */
}

/**
 * @brief 设置JTOOL电源电压
 * @param vcc VCC电压值 (单位: 0.1V, 如33表示3.3V)
 * @return 0-成功, 非0-失败
 */
int TK8710JtoolSetVcc(uint8_t vcc)
{
#ifdef _WIN32
    uint8_t jtoolVcc;

    if (TK8710JtoolEnsureSpiOpen() != 0) {
        return -1;
    }

    if (vcc == 0) {
        jtoolVcc = TK8710_JTOOL_SPI_VCC_OFF;
    } else if (vcc == 33) {
        jtoolVcc = TK8710_JTOOL_SPI_VCC_EQ_VIO;
    } else if (vcc == 50 || vcc == 55) {
        jtoolVcc = TK8710_JTOOL_SPI_VCC_5V;
    } else {
        return -1;
    }

    if (JSPISetVcc(g_jtoolSpiHandle, jtoolVcc) != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)vcc;
    return -1;
#endif
}

/**
 * @brief 设置JTOOL IO电压
 * @param vio VIO电压值 (单位: 0.1V)
 * @return 0-成功, 非0-失败
 */
int TK8710JtoolSetVio(uint8_t vio)
{
#ifdef _WIN32
    uint8_t jtoolVio;

    if (TK8710JtoolEnsureSpiOpen() != 0) {
        return -1;
    }

    if (vio == 33) {
        jtoolVio = TK8710_JTOOL_SPI_VIO_3V3;
    } else if (vio == 18) {
        jtoolVio = TK8710_JTOOL_SPI_VIO_1V8;
    } else {
        return -1;
    }

    if (JSPISetVio(g_jtoolSpiHandle, jtoolVio) != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)vio;
    return -1;
#endif
}

int TK8710JtoolPowerReset(void)
{
#ifdef _WIN32
#if TK8710_JTOOL_ENABLE_POWER_RESET
    if (TK8710JtoolEnsureSpiOpen() != 0) {
        return -1;
    }

    printf("[JTOOL] power reset: VCC off\n");
    if (JSPISetVcc(g_jtoolSpiHandle, TK8710_JTOOL_SPI_VCC_OFF) != ErrNone) {
        return -1;
    }

    Sleep(TK8710_JTOOL_POWER_RESET_OFF_DELAY_MS);

    printf("[JTOOL] power reset: VCC=VIO, VIO 3.3V\n");
    if (JSPISetVio(g_jtoolSpiHandle, TK8710_JTOOL_SPI_VIO_3V3) != ErrNone) {
        return -1;
    }
    if (JSPISetVcc(g_jtoolSpiHandle, TK8710_JTOOL_SPI_VCC_EQ_VIO) != ErrNone) {
        return -1;
    }

    Sleep(TK8710_JTOOL_POWER_RESET_ON_DELAY_MS);
    return 0;
#else
    printf("[JTOOL] power reset is disabled: JTool-SPI VCC pin is fixed 5V on this setup\n");
    return -1;
#endif
#else
    return -1;
#endif
}

/**
 * @brief 重启JTOOL设备
 * @return 0-成功, 非0-失败
 */
int TK8710JtoolReboot(void)
{
#ifdef _WIN32
    if (TK8710JtoolEnsureIoOpen() != 0) {
        return -1;
    }
    
    if (JIOReboot(g_jtoolIoHandle) != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    return -1;
#endif
}

/*============================================================================
 * 8700/8710 SPI 协议层函数实现
 *============================================================================*/

/**
 * @brief 发送复位命令
 * @param resetConfig 复位配置: 1-仅复位状态机, 2-复位状态机和寄存器
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReset(uint8_t resetConfig)
{
#ifdef _WIN32
    ErrorType err;
    uint8_t txBuf[2];
    
    if (g_jtoolSpiHandle == NULL) {
        return -1;
    }
    
    /* 构造复位命令: [0x00] [reset_config] */
    txBuf[0] = TK8710_SPI_CMD_RST;
    txBuf[1] = resetConfig;
    
    err = SPIWriteOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, 2, txBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)resetConfig;
    return -1;
#endif
}

/**
 * @brief 写寄存器 (符合8700协议)
 * @param addr 寄存器地址 (16位)
 * @param data 寄存器数据数组 (每个寄存器4字节, MSB first)
 * @param regCount 寄存器数量
 * @return 0-成功, 非0-失败
 */
int TK8710SpiWriteReg(uint16_t addr, const uint32_t* data, uint8_t regCount)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t txLen;
    uint8_t i;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || regCount == 0) {
        return -1;
    }
    
    /* 计算传输长度: 1(opcode) + 2(addr) + regCount*4(data) */
    txLen = 1 + 2 + regCount * TK8710_REG_SIZE;
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造写寄存器命令: [0x01] [adr_high] [adr_low] [da1_byte3~0] ... */
    g_spiTxBuf[0] = TK8710_SPI_CMD_WR_REG;
    g_spiTxBuf[1] = (uint8_t)(addr >> 8);   /* 地址高字节 */
    g_spiTxBuf[2] = (uint8_t)(addr & 0xFF); /* 地址低字节 */
    
    /* 填充寄存器数据 (MSB first / 大端序) */
    for (i = 0; i < regCount; i++) {
        uint32_t regData = data[i];
        uint32_t offset = 3 + i * TK8710_REG_SIZE;
        g_spiTxBuf[offset + 0] = (uint8_t)(regData >> 24);
        g_spiTxBuf[offset + 1] = (uint8_t)(regData >> 16);
        g_spiTxBuf[offset + 2] = (uint8_t)(regData >> 8);
        g_spiTxBuf[offset + 3] = (uint8_t)(regData & 0xFF);
    }
    
    err = SPIWriteOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, txLen, g_spiTxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)addr;
    (void)data;
    (void)regCount;
    return -1;
#endif
}

/**
 * @brief 读寄存器 (符合8700协议)
 * @param addr 寄存器地址 (16位)
 * @param data 读取数据缓冲区
 * @param regCount 寄存器数量
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReadReg(uint16_t addr, uint32_t* data, uint8_t regCount)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t rxLen;
    uint8_t i;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || regCount == 0) {
        return -1;
    }
    
    /* 接收长度: 3(echo) + 1(NOP) + regCount*4(data) */
    rxLen = 3 + 1 + regCount * TK8710_REG_SIZE;
    
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造读寄存器命令: [0x02] [adr_high] [adr_low] */
    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_RD_REG;
    g_spiTxBuf[1] = (uint8_t)(addr >> 8);
    g_spiTxBuf[2] = (uint8_t)(addr & 0xFF);
    
    /* 全双工传输 */
    err = SPIWriteRead(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, rxLen, g_spiTxBuf, g_spiRxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    /* 解析返回数据 (跳过前4字节: opcode+addr+NOP) */
    for (i = 0; i < regCount; i++) {
        uint32_t offset = 4 + i * TK8710_REG_SIZE;
        data[i] = ((uint32_t)g_spiRxBuf[offset + 0] << 24) |
                  ((uint32_t)g_spiRxBuf[offset + 1] << 16) |
                  ((uint32_t)g_spiRxBuf[offset + 2] << 8)  |
                  ((uint32_t)g_spiRxBuf[offset + 3]);
    }
    
    return 0;
#else
    (void)addr;
    (void)data;
    (void)regCount;
    return -1;
#endif
}

/**
 * @brief 写Buffer (符合8700协议)
 * @param bufferIndex Buffer索引 (0-127: 自动TX, 128-143: 指定TX)
 * @param data 写入数据
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiWriteBuffer(uint8_t bufferIndex, const uint8_t* data, uint16_t len)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t txLen;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || len == 0) {
        return -1;
    }
    
    /* 计算传输长度: 1(opcode) + 1(index) + len(data) */
    txLen = 2 + len;
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造写Buffer命令: [0x03] [buffer_index] [data0] [data1] ... */
    g_spiTxBuf[0] = TK8710_SPI_CMD_WR_BUFF;
    g_spiTxBuf[1] = bufferIndex;
    memcpy(&g_spiTxBuf[2], data, len);
    
    err = SPIWriteOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, txLen, g_spiTxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)bufferIndex;
    (void)data;
    (void)len;
    return -1;
#endif
}

/**
 * @brief 读Buffer (符合8700协议)
 * @param bufferIndex Buffer索引 (0-127: 自动RX, 128-143: 广播RX)
 * @param data 读取数据缓冲区
 * @param len 要读取的数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReadBuffer(uint8_t bufferIndex, uint8_t* data, uint16_t len)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t rxLen;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || len == 0) {
        return -1;
    }
    
    /* 接收长度: 1(opcode) + 1(index) + 1(NOP) + len(data) */
    rxLen = 3 + len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造读Buffer命令: [0x04] [buffer_index] [NOP...] */
    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_RD_BUFF;
    g_spiTxBuf[1] = bufferIndex;
    
    /* 全双工传输 */
    err = SPIWriteRead(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, rxLen, g_spiTxBuf, g_spiRxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    /* 解析返回数据 (跳过前3字节: opcode+index+NOP) */
    memcpy(data, &g_spiRxBuf[3], len);
    
    return 0;
#else
    (void)bufferIndex;
    (void)data;
    (void)len;
    return -1;
#endif
}

/**
 * @brief 设置信息 (符合8700协议)
 * @param infoType 信息类型 (0-4)
 * @param data 设置数据
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiSetInfo(uint8_t infoType, const uint8_t* data, uint16_t len)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t txLen;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || len == 0) {
        return -1;
    }
    
    /* 计算传输长度: 1(opcode) + 1(type) + len(data) */
    txLen = 2 + len;
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造SetInfo命令: [0x06] [info_type] [data0] [data1] ... */
    g_spiTxBuf[0] = TK8710_SPI_CMD_SET_INFO;
    g_spiTxBuf[1] = infoType;
    memcpy(&g_spiTxBuf[2], data, len);
    
    /* 打印前64个字节用于调试 */
    // printf("[DEBUG] SPI TX buffer (first 64 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //        g_spiTxBuf[0], g_spiTxBuf[1], g_spiTxBuf[2], g_spiTxBuf[3], g_spiTxBuf[4],
    //        g_spiTxBuf[5], g_spiTxBuf[6], g_spiTxBuf[7], g_spiTxBuf[8], g_spiTxBuf[9],
    //        g_spiTxBuf[10], g_spiTxBuf[11], g_spiTxBuf[12], g_spiTxBuf[13], g_spiTxBuf[14], g_spiTxBuf[15]);
    // printf("[DEBUG] SPI TX buffer (bytes 16-31): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //        g_spiTxBuf[16], g_spiTxBuf[17], g_spiTxBuf[18], g_spiTxBuf[19], g_spiTxBuf[20],
    //        g_spiTxBuf[21], g_spiTxBuf[22], g_spiTxBuf[23], g_spiTxBuf[24], g_spiTxBuf[25],
    //        g_spiTxBuf[26], g_spiTxBuf[27], g_spiTxBuf[28], g_spiTxBuf[29], g_spiTxBuf[30], g_spiTxBuf[31]);
    // printf("[DEBUG] SPI TX buffer (bytes 32-47): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //        g_spiTxBuf[32], g_spiTxBuf[33], g_spiTxBuf[34], g_spiTxBuf[35], g_spiTxBuf[36],
    //        g_spiTxBuf[37], g_spiTxBuf[38], g_spiTxBuf[39], g_spiTxBuf[40], g_spiTxBuf[41],
    //        g_spiTxBuf[42], g_spiTxBuf[43], g_spiTxBuf[44], g_spiTxBuf[45], g_spiTxBuf[46], g_spiTxBuf[47]);
    // printf("[DEBUG] SPI TX buffer (bytes 48-63): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //        g_spiTxBuf[48], g_spiTxBuf[49], g_spiTxBuf[50], g_spiTxBuf[51], g_spiTxBuf[52],
    //        g_spiTxBuf[53], g_spiTxBuf[54], g_spiTxBuf[55], g_spiTxBuf[56], g_spiTxBuf[57],
    //        g_spiTxBuf[58], g_spiTxBuf[59], g_spiTxBuf[60], g_spiTxBuf[61], g_spiTxBuf[62], g_spiTxBuf[63]);
    
    err = SPIWriteOnly(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, txLen, g_spiTxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    return 0;
#else
    (void)infoType;
    (void)data;
    (void)len;
    return -1;
#endif
}

/**
 * @brief 获取信息 (符合8700协议)
 * @param infoType 信息类型 (0-4)
 * @param data 读取数据缓冲区
 * @param len 要读取的数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiGetInfo(uint8_t infoType, uint8_t* data, uint16_t len)
{
#ifdef _WIN32
    ErrorType err;
    uint32_t rxLen;
    
    if (g_jtoolSpiHandle == NULL || data == NULL || len == 0) {
        return -1;
    }
    
    /* 接收长度: 1(opcode) + 1(type) + len(data) */
    rxLen = 2 + len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
    }
    
    /* 构造GetInfo命令: [0x07] [info_type] [NOP...] */
    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_GET_INFO;
    g_spiTxBuf[1] = infoType;
    
    /* 全双工传输 */
    err = SPIWriteRead(g_jtoolSpiHandle, g_spiCkType, g_spiFirstBit, rxLen, g_spiTxBuf, g_spiRxBuf);
    TK8710JtoolReleaseResetIo();
    if (err != ErrNone) {
        return -1;
    }
    
    /* 解析返回数据 (跳过前3字节: opcode+type+NOP) */
    /* 打印前16个字节用于调试 */
    // printf("[DEBUG] SPI RX buffer (first 16 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //        g_spiRxBuf[0], g_spiRxBuf[1], g_spiRxBuf[2], g_spiRxBuf[3], g_spiRxBuf[4],
    //        g_spiRxBuf[5], g_spiRxBuf[6], g_spiRxBuf[7], g_spiRxBuf[8], g_spiRxBuf[9],
    //        g_spiRxBuf[10], g_spiRxBuf[11], g_spiRxBuf[12], g_spiRxBuf[13], g_spiRxBuf[14], g_spiRxBuf[15]);
    memcpy(data, &g_spiRxBuf[3], len);
    
    
    return 0;
#else
    (void)infoType;
    (void)data;
    (void)len;
    return -1;
#endif
}

/**
 * @brief 获取当前时间戳 (微秒)
 * @return 当前时间戳 (微秒)
 */
uint64_t TK8710GetTimeUs(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000 / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec);
#endif
}

