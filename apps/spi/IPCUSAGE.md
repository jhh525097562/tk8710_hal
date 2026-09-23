# ipc_smp 使用说明

## 1. 简介
`ipc_smp` 是一个面向 Linux 的跨进程/跨核高性能通信库，采用：
- 共享内存 SPSC RingBuffer 作为数据面（单生产者/单消费者，无锁）
- Unix Domain Socket DGRAM 作为通知面（“门铃”唤醒，便于 epoll 集成）
- 建议所有经过共享内存的数据都要遵循打印格式为：消息头(语义解析结构体字段)+长度+payload(base64文本)
典型用法：
- SPI 专核进程（采集/控制）与 MQTT/网络进程之间双向通信
- 一个方向高频数据上报（SPI→MQTT），另一个方向低频控制下发（MQTT→SPI）

## 2. 适用环境与依赖
- 操作系统：Linux（需要 `shm_open/mmap` 与 AF_UNIX DGRAM）
- 链接依赖：`-lrt`

## 3. 基本概念
### 3.1 角色（Role）
每个通道只能是 SPSC：
- `IPC_SMP_ROLE_PRODUCER`：写入 ring（发送）
- `IPC_SMP_ROLE_CONSUMER`：读取 ring（接收）

双向通信必须使用两个通道：
- 通道 A：MQTT→SPI（MQTT=PRODUCER，SPI=CONSUMER）
- 通道 B：SPI→MQTT（SPI=PRODUCER，MQTT=CONSUMER）

### 3.2 共享内存命名（shm_name）
`shm_name` 为 POSIX SHM 名称（例如 `/ipc_mqtt_to_spi`），通常映射到 `/dev/shm` 下的对象。

建议用宏固定命名（两个方向用两个不同名称）。

### 3.3 消息格式与校验
每条消息由以下结构组成：
- `ipc_smp_msg_hdr_t`（固定长度，8 字节对齐）
- payload（长度为 `hdr.len`）
- padding（8 字节对齐填充）

CRC32 用于校验 header（不含 crc 字段）+ payload，消费者在接收时会校验，不通过则丢弃该记录。

### 3.4 seq 的含义
`hdr.seq` 不是“消息计数器 +1”，而是“写指针 head 的 8 字节单位偏移”：
- `seq = head / 8`
- 每条消息占用 `rec_size = ALIGN8(sizeof(hdr) + len)`
- 因此 seq 的递增步长与消息大小有关，常见为固定步长（例如 len=11 时每条占 48 字节，seq 步长为 6）

`seq` 用作 `ipc_smp_ack()` 的确认 token。

## 4. API 说明
头文件：[ipc_smp.h]

### 4.1 初始化与关闭
#### `ipc_smp_channel_init(cfg, &h)`
关键参数：
- `cfg.shm_name`：通道名称（以 `/` 开头）
- `cfg.ring_size`：ring 大小（建议 >= 1MB，按负载调整）
- `cfg.max_msg_size`：单条消息最大长度
- `cfg.batch_count`：通知批量阈值（累计多少条后触发一次 notify）
- `cfg.flush_timeout_ms`：通知超时时间（即使未达到 batch_count 也会在超时后触发 notify）
- `cfg.role`：PRODUCER / CONSUMER
- `cfg.create`：是否创建/初始化共享内存（见 4.3）

#### `ipc_smp_channel_close(h)`
释放句柄、关闭 socket、解除 mmap 映射。

### 4.2 发送侧（Producer）
#### `ipc_smp_send(h, type, buf, len, flags)`
- 成功：`IPC_SMP_OK`
- ring 满：`IPC_SMP_ERR_FULL`
- 参数非法：`IPC_SMP_ERR_INVAL`

建议处理方式：
- 若返回 `IPC_SMP_ERR_FULL`：短暂 sleep/backoff 后重试，或按业务丢弃低优先级数据

#### `ipc_smp_periodic_check(h)`
当你发送频率较低或希望更及时唤醒消费者时，建议周期性调用，确保累计的 notify 会按超时发出。

#### `ipc_smp_get_last_hdr(h, &hdr)`
调试用途：获取“最后一次发送的消息 header”（用于打印/比对）。

### 4.3 接收侧（Consumer）
#### `ipc_smp_get_eventfd(h)`
返回用于通知的 socket fd。推荐把该 fd 加入 `epoll` 监听 `EPOLLIN`。

#### `ipc_smp_clear_notify(h)`
在 epoll 命中后调用，用于清空 socket 里的门铃字节（避免重复触发）。

#### `ipc_smp_recv(h, &msg, timeout_ms)`
接收一条消息：
- `timeout_ms == 0`：非阻塞
- `timeout_ms > 0`：最多等待指定时间（内部采用短 sleep 轮询）
- `timeout_ms < 0`：一直等待（内部采用短 sleep 轮询）

推荐模式：epoll 唤醒 + `timeout_ms=0` 拉取 ring 直到返回 `IPC_SMP_ERR_EMPTY`。

#### `ipc_smp_ack(h, msg.seq)`
确认已处理该条消息，推进 tail。

约束：
- 每次 `ipc_smp_recv()` 成功后必须 `ipc_smp_ack()`，才能继续下一次 recv
- 若不 ack，会得到 `IPC_SMP_ERR_BUSY`

### 4.4 统计与重置
#### `ipc_smp_get_stats(h, &stats)`
读取统计计数（32 位计数器）：
- send_ok/send_full/recv_ok/recv_empty/dropped_low_prio/wakeups

#### `ipc_smp_channel_reset(h)`
将 head/tail 置 0，清空本句柄的 inflight/pending 状态。
通常用于“明确要丢弃历史数据并重新开始”的场景。

## 5. 启动策略（强烈推荐）
### 5.1 “有则用，无则建”（附着优先）
为避免误清共享内存导致历史数据丢失，建议应用侧按如下流程启动：
1. 先 `create=0` 尝试附着
2. 若失败（例如对象不存在），再 `create=1` 创建并初始化

示例封装（伪代码）：
```c
cfg.create = 0;
rc = ipc_smp_channel_init(&cfg, &h);
if (rc != IPC_SMP_OK) {
  cfg.create = 1;
  rc = ipc_smp_channel_init(&cfg, &h);
}
```

说明：
- 设备重启后 `/dev/shm` 一般会清空，此时自然走创建
- 仅进程重启时共享内存通常仍在，附着优先可保留未消费数据

### 5.2 关于 create=1 的语义
当前实现中，只要 `cfg.create==1` 就会对映射区 `memset(0)` 并初始化控制块，因此会丢弃历史数据。
除非你明确需要“重置队列”，否则不建议在进程重启时直接使用 `create=1`。

## 6. 推荐的事件循环模型（epoll）
### 6.1 Consumer：epoll + drain ring
建议逻辑：
1. epoll 等待 notify fd 变为可读
2. `ipc_smp_clear_notify(h)`
3. 循环 `ipc_smp_recv(h, &msg, 0)` 直到返回 `IPC_SMP_ERR_EMPTY`
4. 每条消息处理后 `ipc_smp_ack(h, msg.seq)`

### 6.2 Producer：发送 + periodic_check
高频发送：`ipc_smp_send()` 内部会按 batch_count/flush_timeout 自动触发 notify。  
低频发送：建议在业务循环中额外调用 `ipc_smp_periodic_check()`。

## 7. 典型集成：MQTT ↔ SPI（双通道）
建议通道命名（宏定义）：
- `IPC_CH_MQTT_TO_SPI = "/ipc_mqtt_to_spi"`
- `IPC_CH_SPI_TO_MQTT = "/ipc_spi_to_mqtt"`

进程角色：
- MQTT 进程：
  - 下行通道（MQTT→SPI）：PRODUCER
  - 上行通道（SPI→MQTT）：CONSUMER（epoll 监听 notify）
- SPI 进程：
  - 下行通道（MQTT→SPI）：CONSUMER（epoll 监听 notify，收到后写 SPI）
  - 上行通道（SPI→MQTT）：PRODUCER（中断/采集线程拿到数据后发送）

SPI 中断建议：
- 中断处理本身不要做复杂 IPC，尽量在用户态线程/事件循环里读取 SPI 数据并调用 `ipc_smp_send()`。

## 8. Demo 程序
源码：
- [ipc_demo_producer.c](file:///e:/Product/TurMassLink2026/SMP/ipc/demo/ipc_demo_producer.c)
- [ipc_demo_consumer.c](file:///e:/Product/TurMassLink2026/SMP/ipc/demo/ipc_demo_consumer.c)

当前 demo 参数：
- Producer：`ipc_demo_producer [total] [cpu_id] [shm_name]`
  - `total < 0`：无限发送
- Consumer：`ipc_demo_consumer [cpu_id] [shm_name] [total]`
  - `total < 0`：无限接收

## 9. 常见问题（FAQ）
### 9.1 对端异常退出，如何感知？
当前库未内置“对端存活”协议。推荐在应用层增加：
- 心跳（共享内存控制块或业务层消息）
- 代次 epoch（对端重启可检测并执行 reset）
- notify 发送失败（ENOENT/ECONNREFUSED）作为辅助信号

### 9.2 为什么会看到 `ipc_smp empty head=... tail=...`？
表示当前 ring 为空（head==tail），属于正常现象。消费者空转/等待时会周期性打印。

