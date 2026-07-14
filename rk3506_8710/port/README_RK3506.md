# TK8710 RK3506 Linux 移植层说明

## 概述

`tk8710_rk3506.c` 是 TK8710 驱动在 RK3506 Linux 开发板上的移植层实现。基于 Linux 标准的 `spidev` 和 `libgpiod` 接口实现 SPI 通信和 GPIO 中断功能。

## 硬件要求

- **开发板**: HD-RK3506-IOT 或兼容开发板
- **SPI接口**: 使用 `/dev/spidev1.0` (可在代码中修改)
- **GPIO中断**: 使用 libgpiod 接口，默认 GPIO0_C2 (line 18)

## 软件依赖

### 1. 编译工具链
使用 RK3506 SDK 提供的交叉编译工具链：
```bash
# 解压工具链
tar -xzf arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz
# 设置环境变量
export PATH=$PATH:/path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin
export CROSS_COMPILE=arm-buildroot-linux-gnueabihf-
```

### 2. libgpiod 库
确保目标系统已安装 libgpiod：
```bash
# 在开发板上检查
ls /usr/lib/libgpiod*
# 或通过 buildroot 配置启用 libgpiod
```

## 硬件配置

### SPI 配置
在代码头部修改以下宏定义以匹配实际硬件：

```c
#define TK8710_SPI_DEV_PATH         "/dev/spidev1.0"  /* SPI设备路径 */
```

### GPIO 配置
```c
#define TK8710_GPIO_CHIP_PATH       "/dev/gpiochip0"  /* GPIO芯片路径 */
#define TK8710_IRQ_LINE_OFFSET      18                /* 中断引脚偏移 */
```

GPIO 引脚计算方法：
- GPIO0_A0 ~ GPIO0_A7 = 0 ~ 7
- GPIO0_B0 ~ GPIO0_B7 = 8 ~ 15
- GPIO0_C0 ~ GPIO0_C7 = 16 ~ 23
- GPIO0_D0 ~ GPIO0_D7 = 24 ~ 31
- 例如: GPIO0_C2 = 16 + 2 = 18

## 编译方法

### 交叉编译示例
```bash
# 设置交叉编译器
export CC=arm-buildroot-linux-gnueabihf-gcc

# 编译移植层 (作为库的一部分)
$CC -c -I../inc tk8710_rk3506.c -o tk8710_rk3506.o -lgpiod -lpthread

# 链接时需要的库
# -lgpiod -lpthread
```

### Makefile 示例
```makefile
CC = arm-buildroot-linux-gnueabihf-gcc
CFLAGS = -I../inc -Wall -O2
LDFLAGS = -lgpiod -lpthread

OBJS = tk8710_rk3506.o ../src/tk8710_core.o ../src/tk8710_irq.o

all: libtk8710.a

tk8710_rk3506.o: tk8710_rk3506.c tk8710_hal.h
	$(CC) $(CFLAGS) -c $< -o $@

libtk8710.a: $(OBJS)
	ar rcs $@ $^

clean:
	rm -f *.o libtk8710.a
```

## API 使用说明

### 初始化流程

```c
#include "tk8710_hal.h"

int main(void)
{
    SpiConfig spiCfg = {
        .speed = 8000000,    /* 8MHz */
        .mode = 0,           /* SPI Mode 0 */
        .bits = 8,
        .lsb_first = 0,      /* MSB first */
        .cs_pin = 0
    };
    
    /* 1. 初始化 SPI */
    if (TK8710SpiInit(&spiCfg) != 0) {
        printf("SPI init failed\n");
        return -1;
    }
    
    /* 2. 初始化 GPIO 中断 */
    if (TK8710GpioInit(18, TK8710_GPIO_EDGE_RISING, MyIrqCallback, NULL) != 0) {
        printf("GPIO init failed\n");
        return -1;
    }
    
    /* 3. 使能中断 */
    TK8710GpioIrqEnable(18, 1);
    
    /* 4. 正常使用驱动... */
    
    /* 5. 清理资源 */
    TK8710Rk3506Cleanup();
    
    return 0;
}
```

### RK3506 平台特有函数

| 函数 | 说明 |
|------|------|
| `TK8710Rk3506SpiOpen(path)` | 打开指定路径的 SPI 设备 |
| `TK8710Rk3506SpiClose()` | 关闭 SPI 设备 |
| `TK8710Rk3506GpioClose()` | 释放 GPIO 资源 |
| `TK8710Rk3506Cleanup()` | 清理所有资源 |

## 设备树配置

确保 RK3506 设备树中已正确配置 SPI 和 GPIO：

### SPI 节点示例
```dts
&spi1 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&spi1_pins>;
    
    spidev@0 {
        compatible = "rockchip,spidev";
        reg = <0>;
        spi-max-frequency = <16000000>;
    };
};
```

### GPIO 中断配置
确保中断引脚未被其他驱动占用，可以通过 libgpiod 工具检查：
```bash
gpioinfo gpiochip0
```

## 调试方法

### 1. 检查 SPI 设备
```bash
ls -l /dev/spidev*
```

### 2. 检查 GPIO
```bash
# 查看 GPIO 芯片信息
gpiodetect
# 查看具体引脚状态
gpioinfo gpiochip0
# 监控 GPIO 事件
gpiomon gpiochip0 18
```

### 3. SPI 通信测试
```bash
# 使用 spidev_test 工具测试回环
./spidev_test -D /dev/spidev1.0 -s 1000000 -p "hello"
```

## 注意事项

1. **SPI 速度**: TK8710 协议最高支持 16MHz，建议使用 8MHz 以确保稳定性
2. **中断线程**: GPIO 中断使用独立线程监听，注意线程安全
3. **临界区**: 使用 pthread_mutex 实现临界区保护
4. **资源释放**: 程序退出前务必调用 `TK8710Rk3506Cleanup()` 释放资源

## 参考资料

- [HD-RK3506-IOT 开发文档](https://vanxoak.yuque.com/wb353n/hd-rk3506-iot)
- libgpiod 文档: https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git
- Linux spidev 文档: https://www.kernel.org/doc/Documentation/spi/spidev
