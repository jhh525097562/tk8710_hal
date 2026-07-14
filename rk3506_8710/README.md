# TK8710 HAL v1.3

TK8710芯片硬件抽象层（Hardware Abstraction Layer），当前按 `HAL / TRM / PHY` 分层架构组织，对外提供新的 HAL API 与 PHY API，用于控制 TK8710 无线通信芯片。

## 🏗️ 架构设计

### 分层架构

```
┌─────────────────────────────────────┐
│           应用层 (Application)        │
├─────────────────────────────────────┤
│         HAL层 (HAL Layer)           │  ← 对外统一接口
├─────────────────────────────────────┤
│         TRM层 (TRM Layer)           │  ← 资源管理与调度
├─────────────────────────────────────┤
│         PHY层 (PHY Layer)           │  ← 底层芯片控制
├─────────────────────────────────────┤
│         port层 (port Layer)           │  ← 平台适配，硬件抽象
├─────────────────────────────────────┤
│        硬件层 (Hardware)             │  ← TK8710芯片
└─────────────────────────────────────┘
```

### 模块独立性

- ✅ **HAL层**：对外统一接口，屏蔽底层复杂性
- ✅ **TRM层**：资源管理与业务调度层
- ✅ **PHY层**：底层硬件控制层
- ✅ **松耦合**：HAL 通过 TRM 编排，TRM 通过 PHY 驱动硬件

## 特性

- **分层独立**：HAL API、TRM内部模块、PHY API 职责分离
- **跨平台支持**：RK3506 Linux、Windows JTOOL
- **完整功能**：SPI通信、中断处理、波束管理、数据收发
- **高级特性**：黄金比例哈希、延时释放、队列监控
- **调试支持**：日志系统、中断时间统计、性能监控
- **测试覆盖**：88个测试用例全部通过

## 目录结构

```
tk8710_hal_v1.3/
├── .gitignore              # Git忽略文件配置
├── README.md               # 项目说明文档
├── TK8710 用户指南_V1.4.pdf # 用户指南文档
├── inc/                    # 头文件目录
│   ├── hal_api.h           # HAL API头文件
│   ├── phy/                # PHY层头文件
│   │   ├── phy_api.h       # PHY公共API接口
│   │   ├── phy_irq.h       # PHY中断接口
│   │   ├── phy_regs.h      # PHY寄存器接口
│   │   ├── phy_log.h       # PHY日志接口
│   │   └── phy_test.h      # PHY测试接口
│   ├── driver/             # Driver层头文件
│   │   ├── tk8710_driver_api.h # Driver公共API接口
│   │   ├── tk8710_internal.h # Driver内部函数和调试接口
│   │   ├── tk8710_log.h      # 日志系统
│   │   ├── tk8710_regs.h     # 寄存器定义
│   │   ├── tk8710_rf_regs.h  # RF寄存器定义
│   │   └── tk8710_types.h    # 数据类型定义
│   └── trm/                    # TRM层头文件
│       ├── trm_api.h      # TRM公共API接口
│       ├── trm_internal.h # TRM内部函数和调试接口
│       ├── trm_beam.h      # 波束管理
│       ├── trm_data.h      # 数据管理
│       ├── trm_queue.h     # 队列接口
│       ├── phy_cfg.h       # TRM->PHY配置编排
│       ├── phy_data.h      # TRM->PHY数据编排
│       ├── phy_stat.h      # TRM统计接口
│       └── trm_log.h       # TRM日志系统
├── src/                    # 源文件目录
│   ├── hal/                # HAL层实现
│   │   ├── hal_api.c       # HAL API实现
│   │   ├── hal_cb.c        # HAL回调管理
│   │   └── hal_status.c    # HAL状态管理
│   ├── phy/                # PHY层实现
│   │   ├── phy_api.c       # PHY API实现
│   │   ├── phy_irq.c       # PHY中断实现
│   │   ├── phy_regs.c      # PHY寄存器实现
│   │   ├── phy_log.c       # PHY日志实现
│   │   └── phy_test.c      # PHY测试实现
│   ├── driver/             # Driver层实现
│   │   ├── tk8710_config.c # 芯片配置
│   │   ├── tk8710_core.c   # Driver核心功能
│   │   ├── tk8710_irq.c    # 中断处理
│   │   └── tk8710_log.c    # 日志系统实现
│   └── trm/                # TRM层实现
│       ├── trm_beam.c      # 波束管理实现
│       ├── trm_core.c      # TRM核心功能
│       ├── trm_data.c      # 数据管理实现
│       ├── trm_freq.c      # 频率管理
│       ├── trm_log.c       # TRM日志实现
│       ├── trm_power.c     # 功率管理
│       ├── trm_queue.c     # 队列接口实现
│       ├── phy_cfg.c       # TRM->PHY配置编排
│       ├── phy_data.c      # TRM->PHY数据编排
│       ├── phy_stat.c      # TRM统计实现
│       └── trm_slot.c      # 时隙管理
├── port/                   # 平台移植层
│   ├── tk8710_hal.h        # HAL主接口
│   ├── rk3506/             # RK3506平台实现
│   │   ├── tk8710_rk3506.c # RK3506 Linux实现
│   │   └── README_RK3506.md # RK3506移植说明
│   ├── jtool/              # Windows JTOOL实现
│   │   ├── tk8710_jtool.c  # JTOOL主实现
│   │   ├── jtool.dll       # JTOOL动态库
│   │   ├── jtool.h         # JTOOL头文件
│   │   ├── x64/            # 64位JTOOL库
│   │   └── x86/            # 32位JTOOL库
│   └── README_RK3506.md    # RK3506移植说明
├── test/                   # 测试用例目录
│   ├── DriverTest/         # Driver测试
│   │   ├── TestDriverSlave.c # Slave模式测试
│   │   ├── test_file_read.c # 文件读取测试
│   │   ├── test_gwrxah_packing.c # GWRXAH打包测试
│   │   └── 仿真数据加载功能说明.md # 功能说明
│   ├── unit/               # 单元测试（空目录）
│   ├── integration/        # 集成测试（空目录）
│   └── example/            # 示例程序
│       ├── test8710main_3506.c # 3506主测试程序
│       ├── test8710main_jtool.c # JTOOL主测试程序
│       ├── test_Driver_TRM_main_3506.c # Driver+TRM测试
│       ├── trm_tx_validator.c # TRM验证器
│       └── trm_tx_validator.h # TRM验证器头文件
├── cmake/                  # 构建工具目录
│   ├── Build.md            # 构建说明
│   ├── CMakeLists.txt      # CMake配置
│   ├── Makefile.jtool      # JTOOL Makefile
│   ├── Makefile.rk3506     # RK3506 Makefile
│   ├── build_rk3506.sh     # RK3506构建脚本
│   └── toolchain-rk3506.cmake # RK3506工具链
├── docs/                   # 文档目录
│   ├── TK8710_HAL_API.md   # HAL API文档
│   ├── TK8710_TRM_Driver_API.md # TRM和Driver API文档
│   ├── Porting_Guide.md    # 移植指南
│   ├── TRM_Driver_功能与流程说明.md # 功能说明
│   ├── TurMass™ MAC 协议规范 - v3.10.md # 协议规范
│   ├── 8710_HAL编程指南*.md # HAL编程指南（多个版本）
│   ├── 卫星物联网网关和一般地面网关T层设计-qdm20260305.md # 网关设计
│   ├── TK8710_Driver_API_Parameters.md # API参数说明
│   ├── assets/             # 文档资源
│   └── image/              # 文档图片
├── build_jtool/            # JTOOL编译输出
│   ├── basic_example       # JTOOL基础示例程序
│   ├── test8710main_jtool  # JTOOL测试程序
│   └── [其他编译输出文件]
└── build_rk3506/           # RK3506编译输出
    ├── basic_example       # RK3506基础示例程序
    ├── test8710main_3506   # RK3506测试程序
    ├── test_Driver_TRM_main_3506 # RK3506 Driver+TRM测试
    └── [其他编译输出文件]
```

## 🚀 快速开始

### 1. 编译（RK3506 Linux）

```bash
# 使用WSL进入Ubuntu
wsl -d Ubuntu

# 设置交叉编译环境
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup

# 使用提供的脚本编译
./cmake/build_rk3506.sh

# 输出产物位于 build_rk3506/
```

### 2. 编译（Windows JTOOL）

```bash
# 使用批处理脚本编译
compile_test.bat

# 编译并运行测试
compile_test_suite.bat
```

## 📚 API使用指南

### TRM API（高级业务层）

**适用场景**：需要完整业务逻辑的应用，包括波束管理、数据收发、队列处理等

```c
#include "driver/tk8710_driver_api.h"
#include "trm_api.h"
#include "trm/trm_log.h"

// TRM上层回调接口实现 - TRM调用上层应用
void OnTrmRxData(const TRM_RxDataList* rxDataList) {
    // 数据处理流程简述：
    // 1. 验证接收数据有效性
    // 2. 提取用户数据和业务信息
    // 3. 更新业务状态和统计信息
    // 4. 触发相应的业务逻辑处理
  
    printf("TRM回调: 接收到数据 - 用户数=%d, 帧号=%u\n", 
           rxDataList->userCount, rxDataList->frameNo);
  
    // 业务数据处理逻辑（具体实现根据应用需求）
    for (int i = 0; i < rxDataList->userCount; i++) {
        TRM_RxUserData* user = &rxDataList->users[i];
        printf("处理用户数据: ID=0x%08X, 长度=%u\n", user->userId, user->dataLen);
        // ... 具体的业务处理
    }
}

void OnTrmTxComplete(uint32_t userId, TRM_TxResult result) {
    // 发送完成处理流程简述：
    // 1. 检查发送结果状态
    // 2. 更新发送统计信息
    // 3. 处理重发或确认逻辑
    // 4. 释放相关资源
  
    printf("TRM回调: 发送完成 - 用户ID=0x%08X, 结果=%s\n", 
           userId, result == TRM_TX_SUCCESS ? "成功" : "失败");
  
    if (result == TRM_TX_SUCCESS) {
        // 发送成功处理
    } else {
        // 发送失败处理（可能需要重发）
    }
}

// Driver回调函数实现 - Driver调用TRM
void OnDriverRxData(TK8710IrqResult* irqResult) {
    // Driver中断数据处理流程简述：
    // 1. 验证中断结果有效性
    // 2. 提取硬件状态信息
    // 3. 调用TRM核心处理函数
    // 4. 更新内部状态和统计
  
    printf("Driver回调: 接收数据中断 - 类型=%d, 用户数=%d\n", 
           irqResult->irqType, irqResult->userCount);
  
    // 调用TRM核心处理函数
    TRM_ProcessDriverIrq(irqResult);
}

void OnDriverTxSlot(TK8710IrqResult* irqResult) {
    // 发送时隙中断处理流程简述：
    // 1. 检查发送时隙状态
    // 2. 更新发送统计信息
    // 3. 触发TRM发送逻辑
    // 4. 准备下一时隙数据
  
    printf("Driver回调: 发送时隙中断 - 时隙=%d\n", irqResult->slotIndex);
  
    // 通知TRM发送完成
    TRM_NotifyTxSlotComplete(irqResult);
}

void OnDriverSlotEnd(TK8710IrqResult* irqResult) {
    // 时隙结束中断处理流程简述：
    // 1. 清理当前时隙资源
    // 2. 准备下一时隙配置
    // 3. 更新系统状态
    // 4. 触发时隙切换逻辑
  
    printf("Driver回调: 时隙结束中断 - 时隙=%d\n", irqResult->slotIndex);
  
    // 通知TRM时隙结束
    TRM_NotifySlotEnd(irqResult);
}

void OnDriverError(TK8710IrqResult* irqResult) {
    // 错误中断处理流程简述：
    // 1. 分析错误类型和严重程度
    // 2. 记录错误信息和统计
    // 3. 执行错误恢复策略
    // 4. 通知上层应用
  
    printf("Driver回调: 错误中断 - 类型=%d, 代码=%d\n", 
           irqResult->irqType, irqResult->errorCode);
  
    // 通知TRM发生错误
    TRM_NotifyError(irqResult);
}

int main(void) {
    int ret;
  
    // 0. GPIO中断初始化配置
    ret = TK8710GpioInit();
    if (ret != TK8710_OK) {
        printf("GPIO初始化失败: %d\n", ret);
        return -1;
    }

    // 1. 启用GPIO中断
    ret = TK8710GpioIrqEnable();
    if (ret != TK8710_OK) {
        printf("GPIO中断使能失败: %d\n", ret);
        return -1;
    }
  
    // 2. 8710芯片初始化（数字/MAC层）
    ChipConfig chipConfig = {
        .bcn_agc = 32,           // BCN AGC长度
        .interval = 32,          // Intval长度
        .tx_dly = 1,             // 下行发送时延
        .tx_fix_info = 0,        // TX固定频点
        .offset_adj = 0,        // BCN sync窗口微调
        .tx_pre = 0,             // 发送数据窗口调整
        .conti_mode = 1,         // 连续工作模式
        .bcn_scan = 0,           // BCN SCAN模式
        .ant_en = 0xFF,          // 天线使能
        .rf_sel = 0xFF,          // 射频使能
        .tx_bcn_en = 1,          // 发送BCN使能
        .ts_sync = 0,            // 本地同步
        .rf_model = 2,           // 射频芯片型号: 2=SX1257
        .bcnbits = 0x1F,         // 信标标识位
        .anoiseThe1 = 0,         // 用户检测门限
        .power2rssi = 0,         // RSSI换算
        .irq_ctrl0 = 0xFFFFFFFF,  // 中断使能
        .irq_ctrl1 = 0           // 中断清理
    };
  
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("芯片初始化失败: %d\n", ret);
        return -1;
    }
  
    // 3. 8710射频初始化
    ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1257_32M,  // SX1257 32MHz
        .Freq = 2400000000,                  // 2.4GHz
        .rxgain = 1,                         // 接收增益
        .txgain = 20,                        // 发送增益
        .txadc = {
            {.i = 0, .q = 0},  // 天线1直流校准
            {.i = 0, .q = 0},  // 天线2直流校准
            // ... 其他天线
        }
    };
  
    ret = TK8710RfConfig(&rfConfig);
    if (ret != TK8710_OK) {
        printf("射频初始化失败: %d\n", ret);
        return -1;
    }
  
    // 4. 日志系统初始化
    ret = TK8710LogSimpleInit(TK8710_LOG_INFO, TK8710_LOG_MODULE_ALL);
    if (ret != TK8710_OK) {
        printf("日志系统初始化失败: %d\n", ret);
        return -1;
    }
  
    // 5. 注册Driver回调函数
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = OnDriverSlotEnd,
        .onError = OnDriverError
    };
    TK8710RegisterCallbacks(&driverCallbacks);
  
    // 6. 初始化TRM系统
    TRM_InitConfig trmConfig = {
        .beamMode = TRM_BEAM_MODE_FULL_STORE,
        .beamMaxUsers = 3000,
        .beamTimeoutMs = 10000,
        .callbacks = {
            .onRxData = OnTrmRxData,
            .onTxComplete = OnTrmTxComplete
        },
        .platformConfig = NULL
    };
  
    ret = TRM_Init(&trmConfig);
    if (ret != TRM_OK) {
        printf("TRM初始化失败: %d\n", ret);
        return -1;
    }
  
    // 7. 8710配置（时隙配置、启动工作）
    // 配置时隙参数
    TK8710SetConfig(TK8710_CONFIG_TYPE_SLOT, &slotConfig);
  
    // 启动芯片工作
    ret = TK8710Start(TK8710_WORK_TYPE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        printf("芯片启动失败: %d\n", ret);
        return -1;
    }
  
    // 8. 启动TRM系统
    ret = TRM_Start();
    if (ret != TRM_OK) {
        printf("TRM启动失败: %d\n", ret);
        return -1;
    }
  
    printf("系统初始化完成\n");
  
    // 发送数据（TRM自动处理波束查找、队列管理等）
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint32_t currentFrame = TRM_GetCurrentFrame();
    ret = TRM_SendData(0x12345678, data, sizeof(data), 20, currentFrame + 1, 0);
    if (ret == TRM_OK) {
        printf("数据发送成功\n");
    }
  
    // 发送广播数据
    uint8_t brdData[] = {0xAA, 0xBB, 0xCC};
    ret = TRM_SetTxData(TK8710_DOWNLINK_1, 0, brdData, sizeof(brdData), 35, 0, 0, TK8710_DATA_TYPE_BRD);
    if (ret == TRM_OK) {
        printf("广播数据发送成功\n");
    }
  
    // 运行主循环
    while (running) {
        // TRM自动处理中断、数据收发、波束管理等
        sleep(1);
  
        // 可选：获取TRM运行状态
        if (TRM_IsRunning()) {
            printf("TRM is running\n");
        }
  
        // 可选：获取统计信息
        TRM_Stats stats;
        if (TRM_GetStats(&stats) == TRM_OK) {
            printf("发送次数: %u, 接收次数: %u\n", stats.txCount, stats.rxCount);
        }
    }
  
    // 清理资源
    TRM_Stop();
    TRM_Deinit();
  
    return 0;
}
```

**TRM API 主要函数：**

| 函数                      | 说明          | 特性                   |
| ------------------------- | ------------- | ---------------------- |
| `TRM_Init()`            | 初始化TRM系统 | 配置波束、时隙、回调等 |
| `TRM_Start()`           | 启动TRM系统   | 启动中断处理、数据收发 |
| `TRM_Stop()`            | 停止TRM系统   | 停止所有处理           |
| `TRM_SendData()`        | 发送用户数据  | 自动波束查找、队列管理 |
| `TRM_GetBeamInfo()`     | 获取波束信息  | 线程安全读取           |
| `TRM_IsRunning()`       | 检查运行状态  | 状态查询               |
| `TRM_GetStats()`        | 获取统计信息  | 性能监控               |
| `TRM_GetCurrentFrame()` | 获取当前帧号  | 帧同步                 |

### Driver API（底层硬件层）

**适用场景**：需要直接控制硬件的应用，或实现自定义业务逻辑

```c
#include "tk8710_driver_api.h"
#include "tk8710_log.h"

// Driver回调函数实现 - Driver调用TRM
void OnDriverRxData(TK8710IrqResult* irqResult) {
    printf("Driver回调: 接收数据中断 - 类型=%d, 用户数=%d\n", 
           irqResult->irqType, irqResult->userCount);
  
    // 遍历所有有效用户
    for (uint8_t i = 0; i < 128; i++) {
        if (irqResult->crcResults[i].crcValid) {
            // 获取用户数据
            uint8_t* userData;
            uint16_t dataLen;
            int ret = TK8710GetRxUserData(i, &userData, &dataLen);
            if (ret == TK8710_OK) {
                // 处理数据
                printf("User[%d] received %d bytes\n", i, dataLen);
          
                // 获取信号质量信息
                uint32_t rssi, freq;
                uint8_t snr;
                TK8710GetRxUserSignalQuality(i, &rssi, &snr, &freq);
          
                // 获取用户波束信息
                uint32_t userFreq;
                uint32_t ahInfo[16];
                uint64_t pilotPowerInfo;
                TK8710GetRxUserInfo(i, &userFreq, ahInfo, &pilotPowerInfo);
          
                // 释放数据缓冲区
                TK8710ReleaseRxData(i);
            }
        }
    }
}

void OnDriverTxSlot(TK8710IrqResult* irqResult) {
    printf("Driver回调: 发送时隙中断 - 时隙=%d\n", irqResult->slotIndex);
}

void OnDriverError(TK8710IrqResult* irqResult) {
    printf("Driver回调: 错误中断 - 类型=%d, 代码=%d\n", 
           irqResult->irqType, irqResult->errorCode);
}

int main(void) {
    int ret;
  
    // 0. GPIO中断初始化配置
    ret = TK8710GpioInit();
    if (ret != TK8710_OK) {
        printf("GPIO初始化失败: %d\n", ret);
        return -1;
    }

    // 1. 启用GPIO中断
    ret = TK8710GpioIrqEnable();
    if (ret != TK8710_OK) {
        printf("GPIO中断使能失败: %d\n", ret);
        return -1;
    }
  
    // 2. 8710芯片初始化（数字/MAC层）
    ChipConfig chipConfig = {
        .bcn_agc = 32,           // BCN AGC长度
        .interval = 32,          // Intval长度
        .tx_dly = 1,             // 下行发送时延
        .conti_mode = 1,         // 连续工作模式
        .ant_en = 0xFF,          // 天线使能
        .rf_sel = 0xFF,          // 射频使能
        .tx_bcn_en = 1,          // 发送BCN使能
        .rf_model = 2,           // 射频芯片型号: 2=SX1257
        .irq_ctrl0 = 0xFFFFFFFF,  // 中断使能
        .irq_ctrl1 = 0           // 中断清理
    };
  
    ret = TK8710Init(&chipConfig);
    if (ret != TK8710_OK) {
        printf("芯片初始化失败: %d\n", ret);
        return -1;
    }
  
    // 3. 8710射频初始化
    ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1257_32M,  // SX1257 32MHz
        .Freq = 2400000000,                  // 2.4GHz
        .rxgain = 1,                         // 接收增益
        .txgain = 20,                        // 发送增益
    };
  
    ret = TK8710RfConfig(&rfConfig);
    if (ret != TK8710_OK) {
        printf("射频初始化失败: %d\n", ret);
        return -1;
    }
  
    // 4. 日志系统初始化
    ret = TK8710LogSimpleInit(TK8710_LOG_INFO, TK8710_LOG_MODULE_ALL);
    if (ret != TK8710_OK) {
        printf("日志系统初始化失败: %d\n", ret);
        return -1;
    }
  
    // 5. 注册Driver回调函数
    TK8710DriverCallbacks driverCallbacks = {
        .onRxData = OnDriverRxData,
        .onTxSlot = OnDriverTxSlot,
        .onSlotEnd = NULL,  // 可选
        .onError = OnDriverError
    };
    TK8710RegisterCallbacks(&driverCallbacks);
  
    // 6. 启动芯片工作
    ret = TK8710Start(TK8710_WORK_TYPE_MASTER, TK8710_WORK_MODE_CONTINUOUS);
    if (ret != TK8710_OK) {
        printf("芯片启动失败: %d\n", ret);
        return -1;
    }
  
    // 7. 直接控制硬件发送
    uint8_t txData[] = {0xAA, 0xBB, 0xCC};
    uint8_t userIndex = 0;
  
    // 设置下行2数据
    ret = TK8710SetTxUserData(TK8710_DOWNLINK_2, userIndex, txData, sizeof(txData), 20, TK8710_DATA_TYPE_DED);
    if (ret != TK8710_OK) {
        printf("设置下行数据失败: %d\n", ret);
    }
  
    // 设置用户信息（频率、波束等）
    uint32_t freqInfo = 2400000000;
    uint32_t ahInfo[16] = {0};  // AH配置数据
    uint64_t pilotPowerInfo = 20;
  
    ret = TK8710SetTxUserInfo(userIndex, freqInfo, ahInfo, pilotPowerInfo);
    if (ret != TK8710_OK) {
        printf("设置用户信息失败: %d\n", ret);
    }
  
    // 8. 运行主循环
    while (running) {
        // 应用层自定义处理逻辑
        sleep(1);
  
        // 可选：获取当前配置
        ChipConfig currentChipConfig;
        ret = TK8710GetConfig(TK8710_CONFIG_TYPE_CHIP, &currentChipConfig);
        if (ret == TK8710_OK) {
            printf("当前芯片配置: 工作模式=%s\n", 
                   currentChipConfig.conti_mode ? "连续" : "单次");
        }
  
        // 可选：获取时隙配置
        const slotCfg_t* slotConfig = TK8710GetSlotCfg();
        if (slotConfig != NULL) {
            printf("时隙配置: 字节数=%u, 时间长度=%uus\n",
                   slotConfig->byteLen, slotConfig->timeLen);
        }
  
        // 可选：调试接口使用
        if (debug_mode) {
            // 中断时间统计
            uint32_t totalTime, maxTime, minTime, count;
            ret = TK8710GetIrqTimeStats(1, &totalTime, &maxTime, &minTime, &count);
            if (ret == 0) {
                printf("IRQ stats: avg=%uus, max=%uus, count=%u\n", 
                       totalTime/count, maxTime, count);
            }
        }
    }
  
    // 清理资源
    TK8710Deinit();
  
    return 0;
}
```

**Driver API 主要函数：**

| 函数                               | 说明             | 特性                 |
| ---------------------------------- | ---------------- | -------------------- |
| `TK8710Init()`                   | 初始化Driver     | SPI、中断、基础配置  |
| `TK8710RfConfig()`               | 初始化射频子系统 | 频率、增益、直流校准 |
| `TK8710LogConfig()`              | 初始化日志系统   | 应用层、日志系统     |
| `TK8710RegisterCallbacks()`      | 注册Driver回调   | 多回调架构           |
| `TK8710Start()`                  | 启动芯片工作     | 主模式/从模式        |
| `TK8710SetTxUserData()`          | 设置下行数据     | 统一下行数据发送接口 |
| `TK8710SetTxUserInfo()`          | 设置用户信息     | 频率、波束、功率     |
| `TK8710GetRxUserData()`          | 获取接收数据     | 用户数据读取         |
| `TK8710GetRxUserSignalQuality()` | 获取信号质量     | RSSI、SNR、频率      |
| `TK8710SetConfig()`              | 设置芯片配置     | 时隙、射频等配置     |
| `TK8710GetConfig()`              | 获取芯片配置     | 读取当前配置         |
| `TK8710GetSlotCfg()`             | 获取时隙配置     | 时隙参数查询         |
| `TK8710ResetChip()`              | 芯片复位         | 状态机/寄存器复位    |

### 调试接口使用

**性能监控和测试功能：**

```c
// 1. 中断时间统计
uint32_t totalTime, maxTime, minTime, count;
int ret = TK8710GetIrqTimeStats(1, &totalTime, &maxTime, &minTime, &count);
if (ret == 0) {
    printf("中断类型1统计: 总时间=%uus, 最大=%uus, 最小=%uus, 次数=%u\n", 
           totalTime, maxTime, minTime, count);
}

// 打印完整的中断时间统计报告
TK8710PrintIrqTimeStats();

// 重置统计信息
TK8710ResetIrqTimeStats(255);  // 重置所有中断统计

// 2. 测试接口（仅用于开发测试）
// 强制处理所有用户数据（用于测试中断处理能力）
TK8710SetForceProcessAllUsers(1);
printf("当前强制处理所有用户: %s\n", 
       TK8710GetForceProcessAllUsers() ? "是" : "否");

// 强制按最大用户数发送（用于测试发送能力）
TK8710SetForceMaxUsersTx(1);
printf("当前强制最大用户发送: %s\n", 
       TK8710GetForceMaxUsersTx() ? "是" : "否");

// 3. 调试控制接口
// 获取FFT输出数据
uint8_t fftData[1024];
ret = TK8710DebugCtrl(TK8710_DBG_TYPE_FFT_OUT, 1, NULL, fftData);
if (ret == 0) {
    printf("FFT数据获取成功\n");
}

// 获取捕获数据
uint8_t captureData[2048];
ret = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, 1, NULL, captureData);
if (ret == 0) {
    printf("捕获数据获取成功\n");
}

// 4. 日志系统使用
// 初始化日志系统
TK8710LogSimpleInit(TK8710_LOG_INFO, TK8710_LOG_MODULE_ALL);

// 使用日志宏输出日志
TK8710_LOG_CORE_ERROR("系统初始化失败: %d", ret);
TK8710_LOG_CONFIG_WARN("配置参数无效: %s", param_name);
TK8710_LOG_IRQ_INFO("中断处理完成: 类型=%d", irq_type);
TK8710_LOG_HAL_DEBUG("寄存器读写: 地址=0x%04X, 值=0x%08X", reg_addr, value);
```

## 🔧 核心特性

### 1. 黄金比例哈希波束管理

```c
// 使用黄金比例哈希，减少冲突
static uint32_t BeamHashFunc(uint32_t userId) {
    return (userId * 2654435761U) % BEAM_TABLE_SIZE;
}
```

### 2. 延时释放机制

```c
// 发送完成后自动延时释放波束RAM
TRM_ScheduleBeamRamRelease(userId, 16);  // 16帧后释放
```

### 3. 队列监控

```c
// 实时监控队列状态
TRM: Queue status - BeamRelease: 123/512, TxQueue: 45/256, processed=5 this frame
```

### 4. 线程安全设计

```c
// 读写分离锁机制
TRM_GetBeamInfo()  // 读操作无锁
TRM_SetBeamInfo()  // 写操作加锁
```

## 🎯 使用场景选择

### 选择TRM API当：

- ✅ 需要完整的业务逻辑
- ✅ 需要自动波束管理
- ✅ 需要队列和缓存管理
- ✅ 快速开发应用

### 选择Driver API当：

- ✅ 需要精细硬件控制
- ✅ 实现自定义业务逻辑
- ✅ 性能要求极高
- ✅ 学习硬件协议

## 🧪 测试

```bash
# 运行所有测试
./cmake/build_rk3506.sh

# 测试结果：88/88 通过
# - Driver单元测试: 25个
# - TRM集成测试: 22个  
# - 边界/压力测试: 41个
```

## 📖 示例程序

- `test/example/basic_example.c` - TRM基础使用示例
- `test/example/advanced_example.c` - TRM高级功能演示
- `test/example/test_Driver_TRM_main_3506.c` - Driver+TRM综合测试
- `test/example/gateway_demo.c` - 网关模式演示

## 🚀 平台移植

移植到新平台需要实现HAL接口（参考 `port/tk8710_rk3506.c`）：

```c
// SPI接口
int TK8710_Port_SpiInit(void);
int TK8710_Port_SpiTransfer(const uint8_t* tx, uint8_t* rx, uint16_t len);

// GPIO中断  
int TK8710_Port_GpioIrqInit(TK8710_GpioIrqCallback callback, void* userData);

// 系统接口
void TK8710_Port_DelayMs(uint32_t ms);
uint64_t TK8710_Port_GetTimeUs(void);
void TK8710_Port_EnterCritical(void);
void TK8710_Port_ExitCritical(void);

// 内存接口
void* TK8710_Port_Malloc(size_t size);
void TK8710_Port_Free(void* ptr);
```

## 📊 性能特性

- **波束查找**：O(1)平均时间复杂度（黄金比例哈希）
- **内存管理**：自动延时释放，防止内存泄漏
- **并发安全**：读写分离锁，支持高并发
- **队列管理**：循环队列，高效内存使用
- **实时监控**：队列状态、处理统计实时报告

## 📋 版本历史

- **v1.3** (2026-02-25)

  - ✨ 新增：TRM API和Driver API分层独立
  - ✨ 新增：黄金比例哈希波束管理
  - ✨ 新增：延时释放机制
  - ✨ 新增：队列实时监控
  - 🐛 修复：波束查找冲突问题
  - 🐛 修复：并发访问死锁问题
  - 🔧 优化：发送队列大小可配置
  - 🔧 优化：日志系统模块化
- **v1.0** (2026-02-09)

  - 🎉 初始版本发布
  - 📦 Driver层：SPI协议、中断处理、芯片配置
  - 📦 TRM层：波束管理、数据收发
  - 📦 平台支持：RK3506 Linux、Windows JTOOL
  - 📦 日志系统、调试功能
  - 📦 88个测试用例

## 📄 许可证

内部使用

## 🌐 GitHub仓库

https://github.com/jhh525097562/tk8710_hal_v1.3
