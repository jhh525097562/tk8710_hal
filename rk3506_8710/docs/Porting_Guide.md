# TK8710 HAL 移植指南

本文档介绍如何将TK8710 HAL移植到新的硬件平台。

## 移植概述

移植HAL到新平台需要实现平台移植层接口，主要包括：

1. SPI通信接口
2. GPIO中断接口
3. 系统时间和延时接口
4. 临界区保护接口
5. 内存管理接口

## 移植步骤

### 1. 创建移植文件

在`port/`目录下创建新的移植文件，例如`tk8710_port_myplatform.c`。

### 2. 实现SPI接口

```c
#include "driver/tk8710_port.h"

// SPI设备句柄
static int g_spiFd = -1;

int TK8710_Port_SpiInit(void)
{
    // 打开SPI设备
    g_spiFd = open("/dev/spidev0.0", O_RDWR);
    if (g_spiFd < 0) {
        return TK8710_ERR_INIT;
    }
    
    // 配置SPI参数
    uint8_t mode = SPI_MODE_0;
    uint32_t speed = 10000000;  // 10MHz
    uint8_t bits = 8;
    
    ioctl(g_spiFd, SPI_IOC_WR_MODE, &mode);
    ioctl(g_spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    ioctl(g_spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    
    return TK8710_OK;
}

void TK8710_Port_SpiDeinit(void)
{
    if (g_spiFd >= 0) {
        close(g_spiFd);
        g_spiFd = -1;
    }
}

int TK8710_Port_SpiTransfer(const uint8_t* txBuf, uint8_t* rxBuf, uint16_t len)
{
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)txBuf,
        .rx_buf = (unsigned long)rxBuf,
        .len = len,
        .speed_hz = 10000000,
        .bits_per_word = 8,
    };
    
    int ret = ioctl(g_spiFd, SPI_IOC_MESSAGE(1), &tr);
    return (ret >= 0) ? TK8710_OK : TK8710_ERR_SPI;
}
```

### 3. 实现GPIO中断接口

```c
static TK8710_GpioIrqCallback g_irqCallback = NULL;
static void* g_irqUserData = NULL;
static pthread_t g_irqThread;
static volatile int g_irqRunning = 0;

// 中断线程
static void* IrqThreadFunc(void* arg)
{
    struct gpiod_line* line = (struct gpiod_line*)arg;
    
    while (g_irqRunning) {
        struct gpiod_line_event event;
        int ret = gpiod_line_event_wait(line, NULL);
        
        if (ret > 0) {
            gpiod_line_event_read(line, &event);
            if (g_irqCallback) {
                g_irqCallback(g_irqUserData);
            }
        }
    }
    
    return NULL;
}

int TK8710_Port_GpioIrqInit(TK8710_GpioIrqCallback callback, void* userData)
{
    g_irqCallback = callback;
    g_irqUserData = userData;
    
    // 配置GPIO为中断输入
    // ... 平台相关代码
    
    // 创建中断处理线程
    g_irqRunning = 1;
    pthread_create(&g_irqThread, NULL, IrqThreadFunc, line);
    
    return TK8710_OK;
}

void TK8710_Port_GpioIrqDeinit(void)
{
    g_irqRunning = 0;
    pthread_join(g_irqThread, NULL);
    g_irqCallback = NULL;
}
```

### 4. 实现系统接口

```c
void TK8710_Port_DelayUs(uint32_t us)
{
    usleep(us);
}

void TK8710_Port_DelayMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t TK8710_Port_GetTickMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

uint64_t TK8710_Port_GetTimeUs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000);
}
```

### 5. 实现临界区保护

```c
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void TK8710_Port_EnterCritical(void)
{
    pthread_mutex_lock(&g_mutex);
}

void TK8710_Port_ExitCritical(void)
{
    pthread_mutex_unlock(&g_mutex);
}
```

### 6. 实现内存接口

```c
void* TK8710_Port_Malloc(size_t size)
{
    return malloc(size);
}

void TK8710_Port_Free(void* ptr)
{
    free(ptr);
}
```

## 接口参考

完整的移植接口定义在`inc/driver/tk8710_port.h`中：

```c
// SPI接口
int TK8710_Port_SpiInit(void);
void TK8710_Port_SpiDeinit(void);
int TK8710_Port_SpiTransfer(const uint8_t* txBuf, uint8_t* rxBuf, uint16_t len);
int TK8710_Port_SpiWrite(const uint8_t* data, uint16_t len);
int TK8710_Port_SpiRead(uint8_t* data, uint16_t len);

// GPIO中断接口
typedef void (*TK8710_GpioIrqCallback)(void* userData);
int TK8710_Port_GpioIrqInit(TK8710_GpioIrqCallback callback, void* userData);
void TK8710_Port_GpioIrqDeinit(void);
int TK8710_Port_GpioIrqEnable(void);
int TK8710_Port_GpioIrqDisable(void);

// 系统接口
void TK8710_Port_DelayUs(uint32_t us);
void TK8710_Port_DelayMs(uint32_t ms);
uint32_t TK8710_Port_GetTickMs(void);
uint64_t TK8710_Port_GetTimeUs(void);

// 临界区接口
void TK8710_Port_EnterCritical(void);
void TK8710_Port_ExitCritical(void);

// 内存接口
void* TK8710_Port_Malloc(size_t size);
void TK8710_Port_Free(void* ptr);
```

## 编译配置

### CMake配置

在`CMakeLists.txt`中添加新平台支持：

```cmake
option(PLATFORM_MYPLATFORM "Build for MyPlatform" OFF)

if(PLATFORM_MYPLATFORM)
    set(PORT_SOURCES port/tk8710_port_myplatform.c)
    # 添加平台相关的库
    target_link_libraries(tk8710_hal pthread)
endif()
```

### Makefile配置

创建`Makefile.myplatform`：

```makefile
CC = my-platform-gcc
CFLAGS = -Wall -O2
PORT_SRC = port/tk8710_port_myplatform.c
# ... 其他配置
```

## 参考实现

HAL提供了两个参考移植实现：

1. **RK3506 Linux** - `port/tk8710_port_rk3506.c`
   - 使用spidev和libgpiod
   - 适用于Linux嵌入式平台

2. **Windows JTOOL** - `port/tk8710_port_jtool.c`
   - 使用JTOOL USB转SPI设备
   - 适用于Windows开发调试

## 测试验证

移植完成后，运行测试用例验证：

```bash
# 编译测试
make -f Makefile.myplatform test

# 运行测试
./build/test_driver
./build/test_trm
./build/test_boundary
```

确保所有88个测试用例通过。

## 常见问题

### Q: SPI通信失败

检查：
- SPI设备路径是否正确
- SPI模式（CPOL/CPHA）是否匹配
- SPI速率是否在芯片支持范围内

### Q: 中断无响应

检查：
- GPIO引脚配置是否正确
- 中断触发方式（上升沿/下降沿）是否正确
- 中断回调是否正确注册

### Q: 时间戳不准确

检查：
- 系统时钟源是否稳定
- 时间获取函数精度是否满足要求（微秒级）
