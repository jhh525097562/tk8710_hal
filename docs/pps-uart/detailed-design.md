# RK3506 外部 PPS UART 管理功能详细设计

**版本：** V1.0
**日期：** 2026-09-02

# 1. 详细设计范围

覆盖独立 C API、AG32 ASCII 行协议客户端、结构化状态解析、GPIO 复位、测试程序状态机和 RK3506 构建集成。不改变 AG32 固件协议、TK8710 HAL 初始化语义和 JTOOL 行为。

# 2. 核心模块详细设计

## 2.1 PPS API 模块

使用调用方分配的上下文，避免隐藏的全局状态和动态内存。上下文保存配置、UART fd、接收行缓存和最近事件。单个上下文非线程安全，调用方必须串行调用。

## 2.2 测试程序

测试程序只通过公共 API 操作设备。它负责信号处理、健康计数、启动宽限、复位冷却和最大恢复次数。退出时关闭本地 fd，但不发送 `OUT OFF`。

# 3. 协议详细设计

## 3.1 协议清单

| 协议 | 参与方 | 传输 | 编码/边界 | 版本 |
| --- | --- | --- | --- | --- |
| AG32 GPS PPS APP | RK3506/AG32 | 115200 8N1 UART | ASCII，每行 CR 或 LF 结束 | 当前固件协议 |

无鉴权、加密和校验和。发送命令统一以 LF 结束。接收端接受 CR、LF、CRLF，并限制单行和累计响应长度。

## 3.2 使用的命令与响应

| 命令 | 成功响应 | 用途 |
| --- | --- | --- |
| `GET VERSION` | `OK APP_VER=...` | 记录版本 |
| `OUT ON/OFF` | `OK OUT EN=0/1` | 输出控制 |
| `SET PERIOD <s>` | `OK PERIOD=...` | 周期配置 |
| `GET PERIOD` | `OK PERIOD=... PENDING=...` | 周期状态 |
| `GET STATUS` | `OK STATUS ...` | 常规轮询 |
| `GET PPS/OUT/SYNC/GPS/RMC DIAG` | 对应 `OK` 行 | 异常诊断 |

`EVENT ...` 为主动消息，可出现在命令回复之前或之间。客户端保存最近事件并继续等待当前命令的 `OK/ERR`。未知字段忽略；已知字段重复时以同一响应中最后一个合法值为准。缺失必需字段返回解析错误，输出结构保持清零。

## 3.3 生命周期与恢复

初始化仅复制和校验配置并打开 UART。关闭可重复调用。复位先关闭 UART，再拉低 GPIO 100 ms、拉高、等待设备启动时间并重新打开 UART；复位 API 不自动发送业务命令，调用者随后必须按顺序恢复配置。

## 3.4 错误码

| 错误码 | 名称 | 条件 | 可重试 |
| --- | --- | --- | --- |
| 0 | `TK8710_PPS_OK` | 成功 | - |
| -1 | `TK8710_PPS_ERROR_PARAM` | 空指针、非法配置或周期 0 | 否 |
| -2 | `TK8710_PPS_ERROR_NOT_INITIALIZED` | 未打开设备 | 初始化后 |
| -3 | `TK8710_PPS_ERROR_UNSUPPORTED` | 非 RK3506 平台 | 否 |
| -4 | `TK8710_PPS_ERROR_OPEN` | UART 打开/配置失败 | 是 |
| -5 | `TK8710_PPS_ERROR_WRITE` | 写入或 drain 失败 | 是 |
| -6 | `TK8710_PPS_ERROR_READ` | poll/read 错误 | 是 |
| -7 | `TK8710_PPS_ERROR_TIMEOUT` | 未及时收到最终响应 | 是 |
| -8 | `TK8710_PPS_ERROR_PROTOCOL` | 收到 `ERR` | 视命令而定 |
| -9 | `TK8710_PPS_ERROR_PARSE` | 最终响应格式或必需字段非法 | 是 |
| -10 | `TK8710_PPS_ERROR_OVERFLOW` | 命令/行/响应超限 | 否 |
| -11 | `TK8710_PPS_ERROR_GPIO` | 复位 GPIO 操作失败 | 是 |

# 4. API 说明

## 4.1 核心类型

```c
#define TK8710_PPS_PATH_MAX 128
#define TK8710_PPS_STATE_MAX 16
#define TK8710_PPS_UTC_MAX 32
#define TK8710_PPS_DIAG_TEXT_MAX 512

typedef enum { /* 上表错误码 */ } TK8710PpsError;
typedef enum {
    TK8710_PPS_HEALTHY,
    TK8710_PPS_WAITING,
    TK8710_PPS_HOLDOVER,
    TK8710_PPS_UNHEALTHY
} TK8710PpsHealth;

typedef struct {
    const char* uartDevice;
    uint32_t baudRate;
    uint32_t commandTimeoutMs;
    const char* resetGpioChip;
    uint32_t resetGpioLine;
    uint32_t resetLowMs;
    uint32_t resetBootWaitMs;
} TK8710PpsConfig;

typedef struct {
    int fd;
    uint8_t initialized;
    TK8710PpsConfig config;
    char uartDevice[TK8710_PPS_PATH_MAX];
    char resetGpioChip[TK8710_PPS_PATH_MAX];
    char lastEvent[TK8710_PPS_DIAG_TEXT_MAX];
} TK8710PpsContext;

typedef struct {
    uint32_t period;
    uint32_t pendingPeriod;
    uint8_t pending;
    uint8_t pause;
    uint8_t confirmPending;
} TK8710PpsPeriodInfo;

typedef struct {
    char state[TK8710_PPS_STATE_MAX];
    uint32_t period;
    uint32_t widthMs;
    uint32_t satellites;
    uint32_t snr;
    uint8_t outputEnabled;
    uint8_t pending;
    uint8_t gpsOnline;
    uint8_t ppsSeen;
    uint8_t fixValid;
    char rmcStatus;
    uint8_t aligned;
    uint8_t holdover;
    uint8_t badPeriod;
    uint8_t resyncPending;
    char utc[TK8710_PPS_UTC_MAX];
} TK8710PpsStatus;

typedef struct {
    char pps[TK8710_PPS_DIAG_TEXT_MAX];
    char output[TK8710_PPS_DIAG_TEXT_MAX];
    char sync[TK8710_PPS_DIAG_TEXT_MAX];
    char gps[TK8710_PPS_DIAG_TEXT_MAX];
    char rmc[TK8710_PPS_DIAG_TEXT_MAX];
    uint32_t successMask;
} TK8710PpsDiagnostics;
```

配置中的字符串在初始化时复制到上下文。默认值由 `TK8710PpsGetDefaultConfig()` 填充：`/dev/ttyS4`、115200、2000 ms、`gpiochip1`、19、100 ms、2000 ms。

## 4.2 API 清单与合同

| API | 输入 | 成功输出/副作用 | 主要失败 |
| --- | --- | --- | --- |
| `TK8710PpsGetDefaultConfig` | 非空配置指针 | 写入默认配置 | PARAM |
| `TK8710PpsInit` | 上下文、配置 | 打开并配置 UART | PARAM/OPEN/UNSUPPORTED |
| `TK8710PpsClose` | 上下文 | 关闭 fd，可重复 | 无 |
| `TK8710PpsSetOutput` | 上下文、0/1 | 校验 `OK OUT EN` | WRITE/READ/TIMEOUT/PROTOCOL/PARSE |
| `TK8710PpsGetVersion` | 上下文、调用方缓冲区 | 返回 `GET VERSION` 成功响应原文 | PARAM/WRITE/READ/TIMEOUT/PROTOCOL |
| `TK8710PpsSetPeriod` | 上下文、非零秒数 | 校验 `OK PERIOD` 等于请求值 | 同上/PARAM |
| `TK8710PpsGetPeriod` | 上下文、输出结构 | 填充周期切换字段 | 同上 |
| `TK8710PpsGetStatus` | 上下文、输出结构 | 填充全部必需健康字段 | 同上 |
| `TK8710PpsGetPosition` | 上下文、输出结构 | 查询 `GET POS`，返回十进制度；南纬、西经为负值，`NA ?` 以 `valid=0` 表示 | 同上 |
| `TK8710PpsEvaluateHealth` | 状态、期望周期 | 返回健康分类，无 I/O | PARAM |
| `TK8710PpsGetDiagnostics` | 上下文、输出结构 | 逐项查询；`successMask` 标识成功项 | 全部失败时返回首个错误 |
| `TK8710PpsResetDevice` | 上下文 | 关闭、低脉冲、拉高、等待、重开 | GPIO/OPEN |
| `TK8710PpsGetLastEvent` | 上下文 | 返回上下文内最近事件只读指针 | 未收到事件时返回空串 |

所有命令 API 非幂等并发安全；调用者不得对同一上下文并发调用。`Close` 可重复，`GetDefaultConfig` 和健康判定不访问硬件。

## 4.3 健康判定

给定期望周期 10 秒：

- `HEALTHY`：`STATE=RUNNING, PERIOD=10, OUT=1, PENDING=0, GPS=1, PPS=1, FIX=1, RMC=A, ALIGN=1, HOLD=0, BAD=0, RESYNC=0`。
- `HOLDOVER`：`OUT=1, ALIGN=1, HOLD=1`。
- `WAITING`：输出已开但处于 `WAIT_GPS/WAIT_TARGET/WAIT_PPS`，或周期仍 pending，且无 `BAD/STATE=ERROR`。
- `UNHEALTHY`：`STATE=ERROR`、`BAD=1`，或不满足以上类别。

辅助函数只分类，不执行复位。

# 5. 测试程序状态机

| 状态 | 进入动作 | 成功流转 | 失败流转 |
| --- | --- | --- | --- |
| INIT | 默认配置、GPIO 置高、UART 初始化 | CONFIGURE | 累计通信失败 |
| CONFIGURE | `OUT ON` 后 `SET PERIOD 10` | STARTUP_WAIT | DIAGNOSE |
| STARTUP_WAIT | 每 2 秒轮询，限时 60 秒 | HEALTHY_MONITOR | 超时或连续 3 次通信失败到 DIAGNOSE |
| HEALTHY_MONITOR | 每 2 秒轮询 | 保持 | 连续 3 次非健康到 DIAGNOSE |
| DIAGNOSE | 采集并打印五项诊断 | RESET_WAIT | - |
| RESET_WAIT | 等到距上次复位满 30 秒 | RESET | 信号退出 |
| RESET | 低 100 ms、拉高、等待、重开 | CONFIGURE | 三轮失败后 FAILED |
| FAILED | 打印最终诊断、关闭并非零退出 | 终态 | - |

`STATE=ERROR` 或 `BAD=1` 计入异常，但仍遵循连续 3 次规则；避免单次采样毛刺触发复位。复位成功后必须回到 CONFIGURE，因此命令顺序不会旁路。

# 6. 边界与兼容性

- 布尔字段仅接受 `0/1`；整数必须完整转换、无负号、无溢出。
- 状态结构在解析前清零；只有完整合法的 `OK STATUS` 才返回成功。
- UART EOF、POLLERR、POLLHUP、POLLNVAL 视为读取错误。
- `EINTR/EAGAIN/EWOULDBLOCK` 按可恢复 I/O 处理，但总超时使用单调时钟，不因碎片输入无限延长。
- 当前协议无请求 ID，因此同一上下文只允许一个在途命令。
- 非 RK3506 构建保留 API 符号并返回 `UNSUPPORTED`，便于公共头文件被跨平台代码包含。
- 无持久化数据、数据库迁移、鉴权或 Secret。

# 7. 设计充分性

无阻塞开放问题。实现时不得自行改变命令顺序、复位阈值和平台范围。

- [x] 协议和 API 已分章定义。
- [x] 输入、输出、错误、时序和兼容性已定义。
- [x] 状态机每个状态均有退出路径。
- [x] 超时、事件穿插、未知字段和依赖失败已定义。
- [x] 数据结构使用定宽类型且无持久化迁移。

# 8. V1.1 网关状态机与退出约定

| 网关状态 | 进入条件 | 运行行为 | 退出/流转 |
| --- | --- | --- | --- |
| ABSENT | 两次 1 秒版本查询均失败 | 固定本地同步 | 正常运行 |
| SEARCHING | `GET VERSION` 成功 | 每 10 秒检查 GPS 基础信号，最长 300 秒 | READY 或 LOCAL_DEGRADED |
| EXTERNAL_READY | `GPS/PPS/FIX=1、RMC=A` | 等待 NS 时隙计算结果 | 配置成功后 ACTIVE，失败后 DEGRADED |
| EXTERNAL_ACTIVE | AG32 周期生效并完整健康 | 每 10 秒完整健康检查 | 连续 3 次异常后 FAULT，退出 1 |
| LOCAL_DEGRADED | 搜星、通信、配置或对齐失败 | TK8710 本地同步，仅检查基础 GPS 恢复 | 连续 3 次恢复后退出 2 |
| LOCAL_STATIC | 默认时隙、扫频或无合法计算周期 | 固定本地同步 | 不因 GPS 状态退出 |

时隙周期使用 `uint64_t total_us = framePeriod * frameCount` 计算，要求 `total_us` 能被 1000000 整除且秒数非零、未超出 `uint32_t`。配置顺序固定为 `OUT ON` 后 `SET PERIOD <seconds>`，对齐等待最多 60 秒。网关退出只关闭 UART，不发送 `OUT OFF`，也不自动复位 AG32。

每次网关获取 `GET STATUS` 成功后紧接着查询 `GET POS`，并将快照原子更新到
`/userdata/GPS/GPSinfo.txt`：

```text
lon=114.005377
lat=30.004928
status=6
```

`lon/lat` 是保留六位小数的十进制度，南纬和西经为负值。`status` 是当前进程内连续
GPS 基础健康次数；`GPS=1、PPS=1、FIX=1、RMC=A` 且本次位置有效时饱和递增，否则
立即归零。进程启动时先写零，进程重启后重新从 1 计数，因此它是连续健康观察值而非
跨进程累计次数。状态或位置不可用时经纬度写 `0.000000`。文件写入失败只记录错误，
不改变 GPS/PPS 同步状态机，避免存储故障误触发无线业务退出。

网关统一退出时，先停止IPC、关闭GPS和清理HAL，再删除 `/userdata/GPS` 与
`/userdata/RFStatus` 内的状态文件及临时文件，保留目录。SIGKILL或断电无法执行此清理。
