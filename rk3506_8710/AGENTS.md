# AGENTS.md - TK8710 HAL 开发指南

本文档为 AI 代理在 TK8710 HAL 代码库中工作提供指导。

## 项目概述

TK8710 HAL 是 TK8710 无线通信芯片的硬件抽象层，提供分层架构：
- **Driver 层**：底层硬件控制（SPI、中断、芯片配置）
- **TRM 层**：高级业务逻辑（波束管理、数据传输）
- **HAL/Port 层**：平台适配（RK3506 Linux、Windows JTOOL）

语言：C（C99 标准）
构建系统：CMake + Shell 脚本

---

## 构建命令

### RK3506（Linux ARM）

```bash
# 设置交叉编译环境
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup

# 构建所有模块
./cmake/build_rk3506.sh

# 或使用 Makefile
make -f cmake/Makefile.rk3506
```

### Windows JTOOL

```bash
# 构建静态库
mingw32-make -f cmake/Makefile.jtool lib

# 构建示例程序
mingw32-make -f cmake/Makefile.jtool example

# 清理
mingw32-make -f cmake/Makefile.jtool clean

# 构建单个测试（编译并运行）
mingw32-make -f cmake/Makefile.jtool tests
```

### CMake 构建

```bash
# 配置 RK3506
cmake -B build -DPLATFORM_RK3506=ON

# 配置 JTOOL
cmake -B build -DPLATFORM_JTOOL=ON

# 构建
cmake --build build
```

### 编译单个测试文件

```bash
# JTOOL - 编译单个测试
gcc -Wall -Wextra -O2 -DPLATFORM_JTOOL \
    -I./inc -I./inc/driver -I./inc/trm -I./port/jtool -I./port \
    test/example/test8710main_jtool.c \
    -Lbuild_jtool -ltk8710_hal -lport/jtool/x64/jtool.dll \
    -o build_jtool/test8710main_jtool.exe
```

---

## 代码风格指南

### 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 函数 | PascalCase | `TK8710Init`, `TRM_Init`, `TK8710SetTxData` |
| 类型/结构体 | PascalCase | `ChipConfig`, `ChiprfConfig`, `TK8710IrqResult` |
| 枚举 | 模块前缀大写 | `TK8710_OK`, `TRM_TX_SUCCESS`, `TK8710_WORK_TYPE_MASTER` |
| 宏 | 大写加模块前缀 | `TK8710_MAX_USERS`, `TRM_BEAM_TABLE_SIZE` |
| 变量 | snake_case | `user_index`, `chip_config`, `irq_result` |
| 私有函数 | 下划线前缀 | `_trm_check_beam`, `_tk8710_spi_transfer` |

### 文件组织

```
inc/
├── driver/           # Driver 层头文件
│   ├── tk8710_driver_api.h
│   ├── tk8710_types.h
│   └── tk8710_regs.h
└── trm/              # TRM 层头文件
    ├── trm_api.h
    └── trm_types.h

src/
├── driver/           # Driver 层实现
│   ├── tk8710_core.c
│   └── tk8710_irq.c
└── trm/              # TRM 层实现
    ├── trm_core.c
    └── trm_beam.c

port/                 # 平台适配层
├── rk3506/
└── jtool/
```

### 头文件结构

```c
/**
 * @file tk8710_driver_api.h
 * @brief 简要描述
 * @note 附加说明
 */

#ifndef TK8710_DRIVER_API_H
#define TK8710_DRIVER_API_H

#include "tk8710_types.h"    // 优先 - 内部依赖
#include <stdint.h>         // 系统头文件

#ifdef __cplusplus
extern "C" {
#endif

/* 函数声明 */

/* ============================================================================
 * 按功能分组的章节
 * ============================================================================
 */

#ifdef __cplusplus
}
#endif

#endif /* TK8710_DRIVER_API_H */
```

### 导入顺序

1. 头文件自身的保护宏
2. 内部头文件（同一模块）
3. 外部头文件（其他模块）
4. 系统头文件（`<stdint.h>`、`<string.h>`）

### 格式化

- **缩进**：4 空格（不使用 Tab）
- **行长度**：软限制 100 字符
- **大括号**：K&R 风格
- **指针**：`int* ptr`（星号前有空格）
- **结构体初始化**：优先使用指定初始化器

```c
ChipConfig config = {
    .bcn_agc = 32,
    .interval = 32,
    .conti_mode = 1
};
```

---

## 错误处理

### 返回码

始终检查返回值。常用返回码：
- `0` = 成功（`TK8710_OK`、`TRM_OK`）
- `1` = 失败/错误
- `2` = 超时
- 负值 = 特定错误

```c
int ret = TK8710Init(&config);
if (ret != TK8710_OK) {
    TK8710_LOG_CORE_ERROR("Init failed: %d", ret);
    return ret;
}
```

### 错误日志

使用内置日志宏：
```c
TK8710_LOG_CORE_ERROR("关键错误: %s", msg);
TK8710_LOG_CORE_WARN("警告: %d", code);
TK8710_LOG_CORE_INFO("信息: %s", msg);
TK8710_LOG_CORE_DEBUG("调试: %d", value);
```

---

## 类型系统

### 标准类型

始终使用 `<stdint.h>` 中的定宽整数：
- `uint8_t`、`int8_t`、`uint16_t`、`int16_t`
- `uint32_t`、`int32_t`、`uint64_t`、`int64_t`

### 布尔值

```c
// 推荐：使用 uint8_t
uint8_t enable = 1;

// 避免：使用 bool
bool enable = true;
```

---

## 平台抽象

### Port 层接口

添加平台特定代码时，使用 port 抽象：

```c
int TK8710_Port_SpiTransfer(const uint8_t* tx, uint8_t* rx, uint16_t len);
int TK8710_Port_GpioIrqInit(TK8710_GpioIrqCallback callback, void* userData);
void TK8710_Port_DelayMs(uint32_t ms);
uint64_t TK8710_Port_GetTimeUs(void);
void* TK8710_Port_Malloc(size_t size);
void TK8710_Port_Free(void* ptr);
```

### 条件编译

```c
#ifdef PLATFORM_RK3506
    // RK3506 特定代码
#elif defined(PLATFORM_JTOOL)
    // JTOOL 特定代码
#else
    // 桩实现
#endif
```

---

## 测试

测试位于 `test/DriverTest/` 和 `test/example/`。没有自动化测试运行器，测试通过构建脚本手动编译。

### 构建单个测试

```bash
# RK3506 - 编译单个测试（需要交叉编译器）
arm-buildroot-linux-gnueabihf-gcc -Wall -Wextra -O2 -DPLATFORM_RK3506 \
    -I./inc -I./inc/driver -I./inc/trm -I./port -I./port/rk3506 \
    test/DriverTest/Test8710Slave.c \
    -Lbuild_rk3506 -ltk8710_hal_complete -lpthread -lgpiod \
    -o build_rk3506/Test8710Slave
```

---

## 常见模式

### 回调注册

```c
TK8710DriverCallbacks callbacks = {
    .onRxData = OnDriverRxData,
    .onTxSlot = OnDriverTxSlot,
    .onError = OnDriverError
};
TK8710RegisterCallbacks(&callbacks);
```

### 状态检查

```c
if (TRM_IsRunning()) {
    // TRM 正在运行
}
```

---

## 关键文件

| 文件 | 用途 |
|------|------|
| `inc/driver/tk8710_driver_api.h` | Driver 公共 API |
| `inc/trm/trm_api.h` | TRM 公共 API |
| `src/driver/tk8710_core.c` | Driver 核心实现 |
| `src/trm/trm_beam.c` | 波束管理 |
| `port/rk3506/tk8710_rk3506.c` | RK3506 port 层 |

---

## 调试技巧

```c
// 启用所有模块日志
TK8710LogSimpleInit(TK8710_LOG_DEBUG, TK8710_LOG_MODULE_ALL);

// 获取中断统计
uint32_t total, max, min, count;
TK8710GetIrqTimeStats(1, &total, &max, &min, &count);
```

---

最后更新：2026-04
