# TK8710主测试程序（集成核间通信）

## 概述

这是一个集成了核间通信功能的TK8710主测试程序，在原有的TRM测试基础上添加了与另一个核（NS）的通信能力。

## 新增功能

### 核间通信模块

**接收功能（网关角色）：**
- **接收NS配置下行消息** (`MSG_TYPE_NS_CONFIG_DOWN`)
  - 用于动态配置HAL参数
  - 包含频点、网络、TDD、时隙、速率配置

- **接收NS数据下行消息** (`MSG_TYPE_NS_DATA_DOWN`)
  - 自动调用 `TK8710HalSendData()` 发送数据
  - 支持用户数据下行传输

**发送功能（网关角色）：**
- **发送网关数据上行消息** (`MSG_TYPE_GW_DATA_UP`)
  - 在 `OnTrmRxData()` 回调中自动触发
  - 将接收到的用户数据转发给NS

## 文件结构

```
test/example/
├── TestTRMmain_IPC.c          # 主程序（集成核间通信）
├── tk8710_ipc_comm.h          # 核间通信模块头文件
├── tk8710_ipc_comm.c          # 核间通信模块实现
├── Makefile_IPC.mk            # 编译配置文件
└── README_IPC.md              # 本说明文件
```

## 编译和运行

### 1. 检查依赖
```bash
make -f Makefile_IPC.mk check-deps
```

### 2. 编译程序
```bash
make -f Makefile_IPC.mk
```

### 3. 运行测试
```bash
make -f Makefile_IPC.mk run
# 或者
sudo ./TestTRMmain_IPC
```

### 4. 交叉编译（RK3506）
```bash
make -f Makefile_IPC.mk cross-compile
```

## 使用说明

### 启动顺序
1. **先启动NS程序**（另一个核的程序）
2. **再启动本测试程序**（网关角色）

### CPU绑定
- 主程序绑定到CPU核心2
- 核间通信线程在主程序中运行

### IPC通道配置
程序使用以下共享内存通道：
- **接收通道**: `/ipc_mqtt_to_spi` (NS -> 网关)
- **发送通道**: `/ipc_spi_to_mqtt` (网关 -> NS)

## 工作流程

### 初始化阶段
1. HAL初始化（芯片、RF、TRM）
2. 时隙参数配置
3. 启动TRM工作
4. **启动核间通信线程**
5. 进入主循环

### 运行阶段
- **接收NS消息**: 核间通信线程持续监听NS发送的配置和数据消息
- **处理下行数据**: 收到NS数据下行消息时，自动调用 `TK8710HalSendData()` 发送
- **上行数据转发**: 收到用户数据时，自动通过核间通信发送给NS

### 退出阶段
1. 停止主循环
2. **停止核间通信线程**
3. 清理TRM和HAL资源
4. 程序退出

## 输出示例

```
TK8710 Main Test Program
=== 初始化核间通信 ===
成功附加到现有共享内存: /ipc_mqtt_to_spi
成功附加到现有共享内存: /ipc_spi_to_mqtt
核间通信初始化完成
核间通信线程已启动
核间通信线程进入主循环

=== TRM接收数据事件 (帧号=123) ===
时隙: 用户数=2
第一个用户信息: ID=0x12345678, 速率模式=6, 数据长度=26
[TX: 网关->NS] 消息头: ver=1 type=8193 len=48 seq=1 ts_ns=1234567890 flags=0 crc32=0x12345678
  [网关数据上行] TDD=0, 速率=6, 时隙=3, 载荷长度=26
    载荷数据: 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10...
==================

[RX: NS->网关] 消息头: ver=1 type=4097 len=84 seq=1 ts_ns=1234567891 flags=0 crc32=0xABCDEF12
  [NS配置下行] 频点=433000000 Hz, 网络编号=1, TDD编号=2, 时隙配置=3
    速率配置数量=3
    速率0: 上行包数=10, 下行包数=5
    速率1: 上行包数=8, 下行包数=4
    速率2: 上行包数=6, 下行包数=3

[RX: NS->网关] 消息头: ver=1 type=4097 len=48 seq=2 ts_ns=1234567892 flags=0 crc32=0xDEF12345
  [NS数据下行] TDD=0, 速率=6, 时隙=1, 载荷长度=22
    载荷数据: AA BB CC DD EE FF 11 22 33 44 55 66 77 88 99 00...
收到NS下行数据，调用TK8710HalSendData发送...
TK8710HalSendData调用成功
```

## 核心接口

### 核间通信模块接口

```c
// 初始化核间通信
int IpcCommInit(IpcCommContext *ctx);

// 启动核间通信线程
int IpcCommStart(IpcCommContext *ctx);

// 停止核间通信
void IpcCommStop(IpcCommContext *ctx);

// 清理核间通信资源
void IpcCommCleanup(IpcCommContext *ctx);

// 发送上行数据给NS
int IpcSendUplinkData(IpcCommContext *ctx, const TRM_RxDataList* rxDataList);
```

### 消息处理流程

1. **NS配置下行消息** → 可用于动态重新配置HAL
2. **NS数据下行消息** → 自动调用 `TK8710HalSendData()` 发送
3. **TRM接收数据** → 自动调用 `IpcSendUplinkData()` 发送给NS

## 调试和故障排除

### 常见问题

1. **共享内存初始化失败**
   - 确保NS程序已启动
   - 检查共享内存权限
   - 以root权限运行

2. **核间通信线程启动失败**
   - 检查依赖库是否安装
   - 确认系统支持pthread

3. **消息发送失败**
   - 检查共享内存空间
   - 确认NS程序正常运行

### 调试模式

编译调试版本：
```bash
make -f Makefile_IPC.mk debug
```

调试版本包含：
- 详细的调试信息输出
- GDB调试符号
- 额外的错误检查

### 日志分析

程序输出包含：
- 核间通信初始化和状态信息
- 详细的IPC消息头和载荷解析
- HAL API调用结果
- TRM回调处理信息

## 扩展功能

可以通过修改代码实现：
- 自定义消息处理逻辑
- 性能统计和监控
- 与实际硬件接口集成
- 支持更多消息类型

## 技术细节

- **通信机制**: 基于共享内存的IPC SMP协议
- **线程模型**: 主线程 + 核间通信线程
- **事件驱动**: 使用epoll进行异步I/O处理
- **内存管理**: 静态分配，避免内存碎片
- **错误处理**: 完整的错误检查和恢复机制

## 注意事项

1. **启动顺序**: 必须先启动NS程序，再启动本程序
2. **权限要求**: 需要root权限运行（共享内存访问）
3. **CPU亲和性**: 程序会绑定到CPU核心2
4. **依赖库**: 需要安装json-c、mbedtls、gpiod等库
5. **信号处理**: 支持Ctrl+C安全退出

## 与原版本的区别

| 功能 | 原版本 | IPC版本 |
|------|--------|---------|
| TRM测试 | ✅ | ✅ |
| 核间通信 | ❌ | ✅ |
| NS配置接收 | ❌ | ✅ |
| NS数据下行 | ❌ | ✅ |
| 上行数据转发 | ❌ | ✅ |
| 多线程架构 | 单线程 | 双线程 |
