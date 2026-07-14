/**
 * @file tk8710_rk3506.c
 * @brief TK8710 RK3506 Linux平台移植层实现
 * @note 基于RK3506开发板的Linux环境，使用spidev和libgpiod接口
 */

#if defined(PLATFORM_TMS570)
/* RK3506/Linux port is intentionally empty for the TMS570 build. */
#else

#include "tk8710_hal.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <pthread.h>
#include <signal.h>

/*============================================================================
 * 硬件配置定义 (根据实际硬件修改)
 *============================================================================*/

/* SPI设备路径 */
#define TK8710_SPI_DEV_PATH         "/dev/spidev0.0"

/* GPIO配置 - 使用libgpiod */
#define TK8710_GPIO_CHIP_PATH       "/dev/gpiochip0"
#define TK8710_IRQ_LINE_OFFSET      18      /* 3506开发板：20,3506网关板：18*/
#define TK8710_CS_LINE_OFFSET       0       /* CS引脚偏移，根据实际硬件配置 */
#define TK8710_RST_LINE_OFFSET      0       /* RST引脚偏移，根据实际硬件配置 */

/* SPI协议参数 */
#define TK8710_SPI_MAX_SPEED        16000000     /* 最高SPI速率 8MHz (可根据实际情况调整) */
#define TK8710_SPI_MODE             SPI_MODE_0  /* Mode 0 (CPOL=0, CPHA=0) */
#define TK8710_SPI_BITS_PER_WORD    8

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

/* 协议参数 */
#define TK8710_REG_SIZE             4           /* 寄存器大小 4字节 */
#define TK8710_NOP_BYTE             0x00        /* NOP占位字节 */

/* 最大传输缓冲区大小 */
#define TK8710_SPI_TX_BUF_SIZE      16384*2+4
#define TK8710_SPI_RX_BUF_SIZE      16384*2+4

/*============================================================================
 * 私有变量
 *============================================================================*/

/* SPI设备文件描述符 */
static int g_spiFd = -1;

/* SPI配置参数 */
static uint32_t g_spiSpeed = 16000000;   /* 默认8MHz */
static uint8_t  g_spiMode = TK8710_SPI_MODE;
static uint8_t  g_spiBits = TK8710_SPI_BITS_PER_WORD;

/* SPI传输缓冲区 */
static uint8_t g_spiTxBuf[TK8710_SPI_TX_BUF_SIZE];
static uint8_t g_spiRxBuf[TK8710_SPI_RX_BUF_SIZE];

/* GPIO相关 */
static struct gpiod_chip *g_gpioChip = NULL;
static struct gpiod_line *g_irqLine = NULL;
static struct gpiod_line *g_csLine = NULL;
static struct gpiod_line *g_rstLine = NULL;

/* 中断回调 */
static TK8710GpioIrqCallback g_halIrqCallback = NULL;
static void* g_halUserContext = NULL;

/* 中断线程 */
static pthread_t g_irqThread;
static volatile int g_irqThreadRunning = 0;

/* 临界区互斥锁 */
static pthread_mutex_t g_criticalMutex = PTHREAD_MUTEX_INITIALIZER;

/*============================================================================
 * 私有函数
 *============================================================================*/

/**
 * @brief GPIO中断监听线程
 */
static void* IrqThreadFunc(void* arg)
{
    struct gpiod_line_event event;
    int ret;
    
    (void)arg;
    
    while (g_irqThreadRunning) {
        /* 等待中断事件，超时1秒 */
        struct timespec timeout = {1, 0};
        ret = gpiod_line_event_wait(g_irqLine, &timeout);
        
        if (ret < 0) {
            /* 错误 */
            printf("gpiod_line_event_wait");
            break;
        } else if (ret == 0) {
            /* 超时，继续等待 */
            s_irq_ctrl1 irqCtrl1;
            irqCtrl1.data = 0x1ff;
            TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 
                MAC_BASE + offsetof(struct mac, irq_ctrl1), 
                irqCtrl1.data);
            continue;
        }
        
        /* 读取事件 */
        ret = gpiod_line_event_read(g_irqLine, &event);
        if (ret < 0) {
            printf("gpiod_line_event_read");
            continue;
        }
        
        /* 调用注册的回调函数 */
        if (g_halIrqCallback != NULL) {
            /* GPIO中断回调，传递用户上下文 */
            g_halIrqCallback(g_halUserContext);
        }
    }
    
    return NULL;
}

/*============================================================================
 * HAL接口实现
 *============================================================================*/

/**
 * @brief 初始化SPI接口
 * @note RK3506使用spidev接口
 */
int TK8710SpiInit(const SpiConfig* cfg)
{
    int ret;
    
    if (cfg == NULL) {
        return -1;
    }
    
    /* 如果已打开，先关闭 */
    if (g_spiFd >= 0) {
        close(g_spiFd);
        g_spiFd = -1;
    }
    
    /* 打开SPI设备 */
    g_spiFd = open(TK8710_SPI_DEV_PATH, O_RDWR);
    if (g_spiFd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }
    
    /* 设置SPI模式 - 8710协议要求Mode 0 */
    g_spiMode = SPI_MODE_0;
    ret = ioctl(g_spiFd, SPI_IOC_WR_MODE, &g_spiMode);
    if (ret < 0) {
        perror("Failed to set SPI mode");
        close(g_spiFd);
        g_spiFd = -1;
        return -1;
    }
    
    /* 设置SPI速度 */
    g_spiSpeed = (cfg->speed > TK8710_SPI_MAX_SPEED) ? TK8710_SPI_MAX_SPEED : cfg->speed;
    ret = ioctl(g_spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &g_spiSpeed);
    if (ret < 0) {
        perror("Failed to set SPI speed");
        close(g_spiFd);
        g_spiFd = -1;
        return -1;
    }
    
    /* 设置位宽 */
    g_spiBits = TK8710_SPI_BITS_PER_WORD;
    ret = ioctl(g_spiFd, SPI_IOC_WR_BITS_PER_WORD, &g_spiBits);
    if (ret < 0) {
        perror("Failed to set SPI bits per word");
        close(g_spiFd);
        g_spiFd = -1;
        return -1;
    }
    
    /* 设置MSB优先 (LSB_FIRST = 0) */
    uint8_t lsbFirst = 0;
    ret = ioctl(g_spiFd, SPI_IOC_WR_LSB_FIRST, &lsbFirst);
    if (ret < 0) {
        perror("Failed to set SPI MSB first");
        close(g_spiFd);
        g_spiFd = -1;
        return -1;
    }
    
    /* 读回配置验证 */
    uint8_t rdMode = 0;
    uint32_t rdSpeed = 0;
    uint8_t rdBits = 0;
    ioctl(g_spiFd, SPI_IOC_RD_MODE, &rdMode);
    ioctl(g_spiFd, SPI_IOC_RD_MAX_SPEED_HZ, &rdSpeed);
    ioctl(g_spiFd, SPI_IOC_RD_BITS_PER_WORD, &rdBits);
    
    printf("[TK8710] SPI initialized: %s, speed=%u Hz\n", TK8710_SPI_DEV_PATH, g_spiSpeed);
    printf("[TK8710] SPI config readback: mode=%d, speed=%u Hz, bits=%d\n", rdMode, rdSpeed, rdBits);
    return 0;
}

/**
 * @brief SPI写数据
 */
int TK8710SpiWrite(const uint8_t* tx, size_t len)
{
    struct spi_ioc_transfer tr;
    int ret;
    
    if (tx == NULL || len == 0 || g_spiFd < 0) {
        return -1;
    }
    
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = 0;
    tr.len = len;
    tr.speed_hz = g_spiSpeed;
    tr.bits_per_word = g_spiBits;
    
    ret = ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        perror("SPI write failed");
        return -1;
    }
    
    return 0;
}

/**
 * @brief SPI读数据
 */
int TK8710SpiRead(uint8_t* rx, size_t len)
{
    struct spi_ioc_transfer tr;
    int ret;
    
    if (rx == NULL || len == 0 || g_spiFd < 0) {
        return -1;
    }
    
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = 0;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = g_spiSpeed;
    tr.bits_per_word = g_spiBits;
    
    ret = ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        perror("SPI read failed");
        return -1;
    }
    
    return 0;
}

/**
 * @brief SPI全双工读写
 */
int TK8710SpiTransfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    struct spi_ioc_transfer tr;
    int ret;
    
    if (tx == NULL || rx == NULL || len == 0 || g_spiFd < 0) {
        return -1;
    }
    
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = g_spiSpeed;
    tr.bits_per_word = g_spiBits;
    
    ret = ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        perror("SPI transfer failed");
        return -1;
    }
    
    return 0;
}

/**
 * @brief SPI片选控制
 * @note 如果使用硬件CS，此函数可为空；如果使用软件CS，需要控制GPIO
 */
void TK8710SpiCsControl(uint8_t active)
{
    /* RK3506 spidev驱动通常自动管理CS */
    /* 如果需要软件控制CS，可以使用libgpiod控制CS引脚 */
    if (g_csLine != NULL) {
        gpiod_line_set_value(g_csLine, active ? 0 : 1);  /* CS低有效 */
    }
    (void)active;
}

/**
 * @brief 初始化GPIO中断
 */
int TK8710GpioInit(int pin, TK8710GpioEdge edge, TK8710GpioIrqCallback cb, void* user)
{
    int ret;
    int lineOffset = (pin > 0) ? pin : TK8710_IRQ_LINE_OFFSET;
    
    /* 保存回调 */
    g_halIrqCallback = cb;
    g_halUserContext = user;
    
    /* 打开GPIO芯片 */
    if (g_gpioChip == NULL) {
        g_gpioChip = gpiod_chip_open(TK8710_GPIO_CHIP_PATH);
        if (g_gpioChip == NULL) {
            perror("Failed to open GPIO chip");
            return -1;
        }
    }
    
    /* 获取中断线 */
    g_irqLine = gpiod_chip_get_line(g_gpioChip, lineOffset);
    if (g_irqLine == NULL) {
        perror("Failed to get GPIO line");
        return -1;
    }
    
    /* 配置中断边沿 */
    switch (edge) {
        case TK8710_GPIO_EDGE_RISING:
            ret = gpiod_line_request_rising_edge_events(g_irqLine, "tk8710_irq");
            break;
        case TK8710_GPIO_EDGE_FALLING:
            ret = gpiod_line_request_falling_edge_events(g_irqLine, "tk8710_irq");
            break;
        case TK8710_GPIO_EDGE_BOTH:
            ret = gpiod_line_request_both_edges_events(g_irqLine, "tk8710_irq");
            break;
        default:
            ret = gpiod_line_request_rising_edge_events(g_irqLine, "tk8710_irq");
            break;
    }
    
    if (ret < 0) {
        perror("Failed to request GPIO events");
        return -1;
    }
    
    printf("[TK8710] GPIO IRQ initialized: chip=%s, line=%d\n", TK8710_GPIO_CHIP_PATH, lineOffset);
    return 0;
}

/**
 * @brief GPIO中断使能控制
 */
int TK8710GpioIrqEnable(uint8_t gpioPin, uint8_t enable)
{
    (void)gpioPin;
    
    if (enable) {
        /* 启动中断监听线程 */
        if (!g_irqThreadRunning && g_irqLine != NULL) {
            g_irqThreadRunning = 1;
            if (pthread_create(&g_irqThread, NULL, IrqThreadFunc, NULL) != 0) {
                perror("Failed to create IRQ thread");
                g_irqThreadRunning = 0;
                return -1;
            }
            printf("[TK8710] IRQ thread started\n");
        }
    } else {
        /* 停止中断监听线程 */
        if (g_irqThreadRunning) {
            g_irqThreadRunning = 0;
            pthread_join(g_irqThread, NULL);
            printf("[TK8710] IRQ thread stopped\n");
        }
    }
    
    return 0;
}

/**
 * @brief GPIO输出控制
 */
void TK8710GpioWrite(int pin, uint8_t level)
{
    struct gpiod_line *line;
    
    if (g_gpioChip == NULL) {
        g_gpioChip = gpiod_chip_open(TK8710_GPIO_CHIP_PATH);
        if (g_gpioChip == NULL) {
            return;
        }
    }
    
    line = gpiod_chip_get_line(g_gpioChip, pin);
    if (line == NULL) {
        return;
    }
    
    /* 请求为输出模式 */
    if (gpiod_line_request_output(line, "tk8710_gpio", level) < 0) {
        /* 如果已被请求，尝试直接设置值 */
        gpiod_line_set_value(line, level);
    }
}

/**
 * @brief GPIO输入读取
 */
uint8_t TK8710GpioRead(int pin)
{
    struct gpiod_line *line;
    int val;
    
    if (g_gpioChip == NULL) {
        g_gpioChip = gpiod_chip_open(TK8710_GPIO_CHIP_PATH);
        if (g_gpioChip == NULL) {
            return 0;
        }
    }
    
    line = gpiod_chip_get_line(g_gpioChip, pin);
    if (line == NULL) {
        return 0;
    }
    
    /* 请求为输入模式 */
    if (gpiod_line_request_input(line, "tk8710_gpio") < 0) {
        /* 如果已被请求，尝试直接读取值 */
    }
    
    val = gpiod_line_get_value(line);
    return (val > 0) ? 1 : 0;
}

/**
 * @brief 毫秒延时
 */
void TK8710DelayMs(uint32_t ms)
{
    usleep(ms * 1000);
}

/**
 * @brief 微秒延时
 */
void TK8710DelayUs(uint32_t us)
{
    usleep(us);
}

/**
 * @brief 获取系统时间戳 (毫秒)
 */
uint32_t TK8710GetTickMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 获取当前时间戳 (微秒)
 */
uint64_t TK8710GetTimeUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec);
}

/**
 * @brief 进入临界区
 */
void TK8710EnterCritical(void)
{
    pthread_mutex_lock(&g_criticalMutex);
}

/**
 * @brief 退出临界区
 */
void TK8710ExitCritical(void)
{
    pthread_mutex_unlock(&g_criticalMutex);
}

/*============================================================================
 * RK3506平台特有函数
 *============================================================================*/

/**
 * @brief 打开RK3506 SPI设备
 * @param devPath SPI设备路径，如 "/dev/spidev1.0"
 * @return 0-成功, 非0-失败
 */
int TK8710Rk3506SpiOpen(const char* devPath)
{
    if (devPath == NULL) {
        devPath = TK8710_SPI_DEV_PATH;
    }
    
    if (g_spiFd >= 0) {
        close(g_spiFd);
    }
    
    g_spiFd = open(devPath, O_RDWR);
    if (g_spiFd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }
    
    printf("[TK8710] SPI device opened: %s\n", devPath);
    return 0;
}

/**
 * @brief 关闭RK3506 SPI设备
 * @return 0-成功, 非0-失败
 */
int TK8710Rk3506SpiClose(void)
{
    if (g_spiFd >= 0) {
        close(g_spiFd);
        g_spiFd = -1;
    }
    return 0;
}

/**
 * @brief 关闭GPIO资源
 */
void TK8710Rk3506GpioClose(void)
{
    /* 停止中断线程 */
    if (g_irqThreadRunning) {
        g_irqThreadRunning = 0;
        pthread_join(g_irqThread, NULL);
    }
    
    /* 释放GPIO线 */
    if (g_irqLine != NULL) {
        gpiod_line_release(g_irqLine);
        g_irqLine = NULL;
    }
    
    if (g_csLine != NULL) {
        gpiod_line_release(g_csLine);
        g_csLine = NULL;
    }
    
    if (g_rstLine != NULL) {
        gpiod_line_release(g_rstLine);
        g_rstLine = NULL;
    }
    
    /* 关闭GPIO芯片 */
    if (g_gpioChip != NULL) {
        gpiod_chip_close(g_gpioChip);
        g_gpioChip = NULL;
    }
}

/**
 * @brief 清理所有资源
 */
void TK8710Rk3506Cleanup(void)
{
    TK8710Rk3506SpiClose();
    TK8710Rk3506GpioClose();
    printf("[TK8710] RK3506 resources cleaned up\n");
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
    uint8_t txBuf[2];
    
    if (g_spiFd < 0) {
        return -1;
    }
    
    /* 构造复位命令: [0x00] [reset_config] */
    txBuf[0] = TK8710_SPI_CMD_RST;
    txBuf[1] = resetConfig;
    
    return TK8710SpiWrite(txBuf, 2);
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
    uint32_t txLen;
    uint8_t i;
    
    if (g_spiFd < 0 || data == NULL || regCount == 0) {
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
        
        /* 打印寄存器写入信息 */
        // printf("write addr = 0x%08X,data = 0x%08X\n", addr + i * 4, regData);
    }
    
    return TK8710SpiWrite(g_spiTxBuf, txLen);
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
    uint32_t rxLen;
    uint8_t i;
    int ret;
    
    if (g_spiFd < 0 || data == NULL || regCount == 0) {
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
    ret = TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen);
    if (ret != 0) {
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
    uint32_t txLen;
    
    if (g_spiFd < 0 || data == NULL || len == 0) {
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
    
    return TK8710SpiWrite(g_spiTxBuf, txLen);
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
    uint32_t rxLen;
    int ret;
    
    if (g_spiFd < 0 || data == NULL || len == 0) {
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
    ret = TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen);
    if (ret != 0) {
        return -1;
    }
    
    /* 解析返回数据 (跳过前3字节: opcode+index+NOP) */
    memcpy(data, &g_spiRxBuf[3], len);
    
    return 0;
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
    uint32_t txLen;
    
    if (g_spiFd < 0 || data == NULL || len == 0) {
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
    
    return TK8710SpiWrite(g_spiTxBuf, txLen);
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
    uint32_t rxLen;
    int ret;
    
    if (g_spiFd < 0 || data == NULL || len == 0) {
        printf("1\n");
        return -1;
    }
    
    /* 接收长度: 1(opcode) + 1(type) + len(data) */
    rxLen = 2 + len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        printf("2\n");
        return -1;
    }
    
    /* 构造GetInfo命令: [0x07] [info_type] [NOP...] */
    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_GET_INFO;
    g_spiTxBuf[1] = infoType;
    
    /* 全双工传输 */
    ret = TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen);
    if (ret != 0) {
        printf("3\n");
        return -1;
    }
    
    /* Parse return data (skip first 3 bytes: opcode+type+NOP) */
    memcpy(data, &g_spiRxBuf[3], len);
    
    return 0;
}

/**
 * @brief GPIO control function (similar to gpioset command)
 * @param chipPath GPIO chip path (e.g., "gpiochip0")
 * @param lineOffset GPIO line offset (e.g., 9)
 * @param level GPIO level (0=low, 1=high)
 * @return 0- success, -1- failure
 */
int TK8710GpioSet(const char* chipPath, unsigned int lineOffset, uint8_t level)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int ret = -1;
    
    /* Open GPIO chip */
    chip = gpiod_chip_open_by_name(chipPath);
    if (chip == NULL) {
        printf("[TK8710] Failed to open GPIO chip: %s\n", chipPath);
        return -1;
    }
    
    /* Get GPIO line */
    line = gpiod_chip_get_line(chip, lineOffset);
    if (line == NULL) {
        printf("[TK8710] Failed to get GPIO line: %d\n", lineOffset);
        goto cleanup;
    }
    
    /* Request as output mode */
    if (gpiod_line_request_output(line, "tk8710_gpio", level) < 0) {
        /* If already requested, try to set value directly */
        if (gpiod_line_set_value(line, level) < 0) {
            printf("[TK8710] Failed to set GPIO line: %d\n", lineOffset);
            goto cleanup;
        }
    }
    
    printf("[TK8710] GPIO %s %d=%d (success)\n", chipPath, lineOffset, level);
    ret = 0;
    
cleanup:
    gpiod_chip_close(chip);
    return ret;
}

/**
 * @brief GPIO read function (similar to gpioget command)
 * @param chipPath GPIO chip path (e.g., "gpiochip0")
 * @param lineOffset GPIO line offset (e.g., 9)
 * @return GPIO level (0=low, 1=high), -1- error
 */
int TK8710GpioGet(const char* chipPath, unsigned int lineOffset)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int value = -1;
    
    /* Open GPIO chip */
    chip = gpiod_chip_open_by_name(chipPath);
    if (chip == NULL) {
        printf("[TK8710] Failed to open GPIO chip: %s\n", chipPath);
        return -1;
    }
    
    /* Get GPIO line */
    line = gpiod_chip_get_line(chip, lineOffset);
    if (line == NULL) {
        printf("[TK8710] Failed to get GPIO line: %d\n", lineOffset);
        goto cleanup;
    }
    
    /* Request as input mode */
    if (gpiod_line_request_input(line, "tk8710_gpio") < 0) {
        /* If already requested, try to get value directly */
        value = gpiod_line_get_value(line);
        if (value < 0) {
            printf("[TK8710] Failed to get GPIO line: %d\n", lineOffset);
            goto cleanup;
        }
    } else {
        value = gpiod_line_get_value(line);
    }
    
    printf("[TK8710] GPIO %s %d=%d\n", chipPath, lineOffset, value);
    
cleanup:
    gpiod_chip_close(chip);
    return value;
}
#endif /* PLATFORM_TMS570 */
