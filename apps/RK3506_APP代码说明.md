# RK3506 App 代码说明

2026-09-21补充：看门狗及软件守护的实际实现、参数和边界见[第17节](#17-看门狗与软件守护实现2026-09-21核查)。本节依据本地应用源码核查，不代表板端硬件看门狗已启用。

## 1. 项目定位

本目录是一套运行在 RK3506/RK3506B 嵌入式 Linux 板卡上的 TurMass Link/TK8710 网关应用集合。整体不是单一 App，而是由多个 C 进程、Shell 守护脚本、配置文件、Web 页面资源和升级打包脚本组成。

现有部署说明中把系统概括为“三核八进一守两库”：

- 三个 CPU 核心协同运行，其中 `tk8710_gw`/SPI 侧业务倾向绑定 CPU2，网络、Web、守护、日志运行在 CPU0-1。
- 八类业务进程包括 `log_daemon`、`mqtt_bridge`、`tk8710_gw`、`lan_discoveryd`、`web_server`、`swupdate`、`led_agent` 和硬件 IO/I2C/SPI 相关程序。
- 一个超级守护脚本 `tdaemon.sh` 负责进程保活、网络选路和异常恢复。
- 两个核心库是 `mosquitto` MQTT 底层库和自研 `ipc_smp` 跨进程通信库。

核心业务链路可以理解为：

```text
NS/MQTT 云端
    <-> mqtt_bridge
    <-> ipc_smp 双向共享内存通道
    <-> tk8710_gw / SPI 业务进程
    <-> TK8710 射频/网关硬件
```

辅助链路包括：

```text
浏览器
    <-> web_server:8090
    <-> /etc/config.json、/etc/network/interfaces、/etc/wpa_supplicant.conf、swupdate、本地 IPC Socket
```

```text
tdaemon.sh
    -> 守护 log_daemon、mqtt_bridge、web_server、tk8710_gw
    -> 检测 eth0/4G/wlan0 网络质量
    -> 写入 /run/led/net.state
    -> 在配置变更或进程退出时重启相关进程
```

```text
mqtt_bridge / tdaemon
    -> 写入 /run/led/*.state
    -> led_agent 轮询状态文件
    -> libgpiod 控制 RUN/NET LED
```

## 2. 目录结构

| 目录 | 作用 |
| --- | --- |
| `app/mqtt` | MQTT 桥接进程 `mqtt_bridge`，连接 NS/MQTT 云端并通过 `ipc_smp` 与 SPI/TK8710 业务进程通信。 |
| `app/spi` | SPI 侧示例/网关进程 `tk8710_gw`，当前代码包含 IPC 双通道、CPU2 绑定和 SPI 设备访问模拟。 |
| `app/webserver` | Web 管理后台 `web_server`，基于 Mongoose，监听 `0.0.0.0:8090`。 |
| `app/tdaemon` | 超级守护脚本和 SysV init 脚本，负责进程保活、网络切换、配置热加载、异常恢复。 |
| `app/SMP/ipc` | 自研 `ipc_smp` 通信库源码，使用 POSIX 共享内存和 Unix Domain Socket 通知。 |
| `app/log` | UDP 日志守护 `log_daemon`、日志发送工具和测试程序。 |
| `app/light` | LED 状态代理 `led_agent`，读取状态文件并通过 GPIO 控制指示灯。 |
| `app/Discovery` | 局域网发现服务 `lan_discoveryd`，监听 UDP 20165。 |
| `app/wlan0` | Wi-Fi AP/STA 切换配置和脚本。 |
| `app/gpio`、`app/i2c`、`app/i2c-tools` | GPIO/I2C 工具、示例和移植工具。 |
| `app/auto_deployment` | Windows/WSL 下制作 RK3506 完整升级包的脚本和说明。 |
| `app/swupdattool` | `.swu` 升级包制作和升级测试资源。 |
| `app/milestone` | 历史里程碑版本、预编译库和备份文件。 |

## 3. 运行部署模型

板端关键文件路径来自 `app/auto_deployment/Rk3506需要部署的文件.md` 和 `app/tdaemon/tdaemon.conf`：

| 程序/文件 | 板端路径 | 说明 |
| --- | --- | --- |
| `log_daemon` | `/usr/bin/log_daemon` | UDP 日志收集与落盘。 |
| `mqtt_bridge` | `/userdata/mqtt_bridge` | NS/MQTT 与本地 SPI/TK8710 进程之间的桥。 |
| `web_server` | `/userdata/web_server` | Web 管理后台。 |
| `tk8710_gw` | `/userdata/tk8710_gw` | TK8710 网关驱动和 TRM 业务主进程。 |
| `tdaemon.sh` 及模块脚本 | `/usr/bin` | 守护、网络、GPIO、恢复逻辑。 |
| `S99tdaemon_job` | `/etc/init.d/S99tdaemon_job` | 启动 `tdaemon` 的 init.d 脚本。 |
| `tdaemon.conf` | `/etc/tdaemon.conf` | 进程守护和网络检测配置。 |
| `config.json` | `/etc/config.json` | MQTT、网关 ID、主题、Web 密码、版本等业务配置。 |
| `device_version.json` | `/etc/device_version.json` | 通信版本、App 版本、构建信息。 |

`tdaemon.conf` 当前配置守护以下进程：

```ini
proc=/usr/bin/log_daemon 5514 /userdata/logs 262144 1000 10485760 20
proc=/userdata/mqtt_bridge
proc=/userdata/web_server
proc=/userdata/tk8710_gw
```

升级包制作说明中要求新版 `tk8710_gw` 使用完整启动参数：

```ini
proc=/userdata/tk8710_gw -w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level warn
```

## 4. 核心通信协议

### 4.1 ipc_smp 双通道

`ipc_smp` 是 MQTT 进程和 SPI/TK8710 进程之间的高性能 IPC 库，源码在 `app/SMP/ipc`，接口定义在 `app/SMP/ipc/include/ipc_smp.h`。

它采用两层机制：

- 数据面：POSIX 共享内存中的 SPSC RingBuffer，单生产者、单消费者。
- 通知面：Unix Domain Socket DGRAM，作为“门铃”通知，可接入 `epoll`。

两个方向必须使用两个独立通道：

| 通道名 | 方向 | MQTT 进程角色 | SPI/TK8710 进程角色 |
| --- | --- | --- | --- |
| `/ipc_mqtt_to_spi` | MQTT 下行到 SPI | Producer | Consumer |
| `/ipc_spi_to_mqtt` | SPI 上行到 MQTT | Consumer | Producer |

通用消息头 `ipc_smp_msg_hdr_t` 包含：协议版本、消息类型、payload 长度、seq、时间戳、flags、CRC32。消费者收到消息后必须调用 `ipc_smp_ack()`，否则后续读取会进入 busy 状态。

启动策略采用“先附着，失败再创建”：

```text
create=0 尝试附着已有共享内存
    成功：保留未消费数据
    失败：create=1 创建新共享内存
```

这样可以避免普通进程重启时误清共享内存中的历史消息。

### 4.2 MAC/NS 业务消息结构

MQTT 下行 JSON 和 SPI 上行结构体之间的业务协议定义在 `app/mqtt/inc/mac_msg_parser.h`，SPI 侧 `app/spi/inc/spi_app.h` 也有一份对应定义。

主要消息类型：

| 类型 | 方向 | 结构体 | 说明 |
| --- | --- | --- | --- |
| `MSG_TYPE_NS_CONFIG_DOWN` | NS -> 网关 | `NsConfigDown_t` | NS 下发频点、网络数、TDD、slot、速率配置。 |
| `MSG_TYPE_NS_DATA_DOWN` | NS -> 网关 | `NsDataDown_t` | NS 下发业务数据，payload 经 Base64 解码后送本地业务。 |
| `MSG_TYPE_GW_DATA_UP` | 网关 -> NS | `GwBatchDataUp_t` | 网关批量上报用户数据，最多 `MAX_UP_USER_NUM=128` 条。 |
| `MSG_TYPE_GW_ALIVE_UP` | 网关 -> NS | `GwAliveUp_t` | 网关心跳，上报版本、运行时间、当前网络、电源状态、定位信息等。 |
| `MSG_TYPE_CONFIG_REQ` | 本地业务 -> NS | `ConfigReq_t` | 本地业务请求 NS 下发配置。 |

关键限制：

- `MAX_PAYLOAD_LEN=520`，单条业务 payload 最大 520 字节。
- `MAX_RATE_CFGS=8`，最多 8 组速率配置。
- `MAX_UP_USER_NUM=128`，上行批量最多 128 个用户数据。
- `IPC_RING_SIZE=4MB`，`IPC_MAX_MSG_SIZE` 按 `GwBatchDataUp_t` 大小动态估算。

## 5. mqtt_bridge

代码位置：`app/mqtt/src/main.c`。

编译目标：`mqtt_bridge`。

依赖库：`mosquitto`、`json-c`、`mongoose`、`ipc_smp`、`mbedcrypto`、`gpiod`、`pthread`、`rt`。

### 5.1 启动流程

`main()` 主要步骤：

1. 解析运行参数：`raw`、`ver`、`help`、`logip=<ip>`、`logport=<port>`。
2. 加载 `/etc/config.json` 到 `AppConfig`。
3. 加载 `/etc/device_version.json` 中的通信版本。
4. 初始化互斥锁和电源 GPIO 监测。
5. 初始化两个 `ipc_smp` 通道。
6. 把 SPI 上行通道的通知 fd 加入 `epoll`。
7. 初始化 mosquitto，设置账号密码、连接回调、消息回调、断连回调。
8. 连接 MQTT Broker，启动 `mosquitto_loop_start()` 网络线程。
9. 进入主循环，处理 IPC 上行、GPIO 电源事件、周期心跳、配置请求重发、LED 状态刷新。

### 5.2 MQTT 下行处理

`on_message()` 负责处理 NS 下发消息：

```text
MQTT message payload JSON
    -> parse_mac_msg()
    -> 得到 NsConfigDown_t 或 NsDataDown_t
    -> send_mqtt_payload_to_spi()
    -> ipc_smp_send(/ipc_mqtt_to_spi)
    -> SPI/TK8710 进程消费
```

处理特点：

- 超过 `IPC_MAX_MSG_SIZE` 的 MQTT payload 直接丢弃。
- JSON 解析失败时记录 warning 并丢弃。
- 收到 `MSG_TYPE_NS_CONFIG_DOWN` 会标记 `config_ack_received=1`。
- 成功下发到 SPI 后会调用 `mqtt_led_mark_txrx()`，更新 `/run/led/mqtt.state`。

### 5.3 SPI 上行处理

`process_spi_uplink()` 负责处理本地业务上行：

```text
epoll 监听 /ipc_spi_to_mqtt 通知 fd
    -> ipc_smp_clear_notify()
    -> ipc_smp_recv() drain ring
    -> 根据 msg_type 分发
    -> pack_*_msg() 打包 JSON
    -> mosquitto_publish(pub_topic)
    -> ipc_smp_ack()
```

支持的上行类型：

- `MSG_TYPE_GW_DATA_UP`：批量数据上报。为了控制单次 MQTT payload 大小，代码按每 64 条拆分发布。
- `MSG_TYPE_GW_ALIVE_UP`：本地业务主动上报心跳，打包后发布。
- `MSG_TYPE_CONFIG_REQ`：本地业务请求配置，`mqtt_bridge` 发布 `{"get":{"config":true}}` 到上行主题。

### 5.4 心跳和状态文件

`send_heartbeat()` 周期发布网关心跳：

- 版本：来自 `/etc/device_version.json`。
- 当前网络：读取 `/proc/net/route` 的默认路由网卡。
- 电源状态：读取 `gpiochip1` offset 17，失败时默认 `power=1`。
- 经纬度：读取 `/var/run/bdgps`。
- 发布主题：`config.pub_topic`。

同时 `mqtt_bridge` 会写 `/run/led/mqtt.state`：

```text
STATUS=connected 或 txrx
TS_MS=<毫秒时间戳>
```

`led_agent` 使用该文件判断 RUN 灯常亮、快闪或熄灭。

## 6. SPI/TK8710 侧进程

代码位置：`app/spi/src/spi_app.c`。

编译目标：`tk8710_gw`。

当前这份代码更像 SPI/IPC 集成样例，真实部署说明中的 `tk8710_gw` 可能来自另一个 TK8710 HAL 工程，但此目录展示了与 `mqtt_bridge` 对接的基本方式。

### 6.1 启动流程

`main()`：

```text
bind_cpu2()
    -> spi_app_init()
    -> spi_app_run()
    -> spi_app_cleanup()
```

`bind_cpu2()` 使用 `sched_setaffinity()` 将进程绑定到 CPU2，符合部署说明中 TK8710 业务运行在 CPU2 的设计。

`spi_app_init()`：

- 初始化 `/ipc_mqtt_to_spi` 为 Consumer。
- 初始化 `/ipc_spi_to_mqtt` 为 Producer。
- 打开 `/dev/spidev0.0`。
- 如果 SPI 设备不存在，当前代码不会退出，而是进入模拟模式，方便非板端环境调试。

### 6.2 主循环

`spi_app_run()` 使用 `epoll` 监听 MQTT 下行通道通知：

```text
epoll_wait(100ms)
    -> 收到 MQTT->SPI 通知
    -> ipc_smp_clear_notify()
    -> process_incoming_messages()
    -> 写 SPI 或模拟写 SPI
    -> 周期 process_outgoing_simulation()
    -> ipc_smp_periodic_check()
```

当前模拟上行逻辑每 2 秒生成一次：

- 一定概率发送 `ConfigReq_t`，请求 NS 配置。
- 否则构造 `GwBatchDataUp_t`，在 1 条和 127 条上报之间切换，用于测试大批量 IPC/MQTT 链路。

## 7. web_server

代码位置：`app/webserver/src`。

编译目标：`web_server`。

入口：`app/webserver/src/main.c`。

监听地址：`http://0.0.0.0:8090`。

Web 服务基于 Mongoose，入口很薄：支持 `-ver`/`--version` 输出版本，然后调用 `web_server_start()`。

### 7.1 请求路由

`app/webserver/src/web_server.c` 中 `event_handler()` 按 URI 分发。除 `/login`、`/logout` 外，其余接口需要 Cookie 中包含 `session=admin`。

主要页面和 API：

| URI | 作用 |
| --- | --- |
| `/login` | 登录页面和登录处理。 |
| `/status` | 设备状态页。 |
| `/config` | MQTT/网关/系统配置查看与保存。 |
| `/network` | 有线和 Wi-Fi 网络配置。 |
| `/rf` | 射频相关页面。 |
| `/upgrade` | 固件升级页面。 |
| `/api/system_info` | 返回系统信息和版本。 |
| `/api/active_ip` | 返回当前活跃 IP。 |
| `/api/network_quality` | 返回网络质量检测结果。 |
| `/api/traffic`、`/api/traffic_stats` | 流量统计。 |
| `/api/rf_stats` | RF/MQTT 收发统计。 |
| `/api/rf_scan/start|status|result` | 扫频任务启动、状态、结果读取。 |
| `/api/rf_acm/start|status|result` | ACM 任务启动、状态、结果读取。 |
| `/upload/start`、`/upload`、`/api/upgrade_status` | `.swu` 上传、安装和状态查询。 |
| `/api/logs`、`/api/log_download` | 日志列表和下载。 |

### 7.2 配置管理

`config_manager.c` 读写 `/etc/config.json`，同时读取 `/etc/device_version.json` 覆盖版本字段。

默认配置包括：

- MQTT Broker 地址、端口、用户名、密码。
- `gateway_id`、`pub_topic`、`sub_topic`。
- `heartbeat_interval`、`qos`、`net_interface`。
- `antenna_count`、`web_password`。
- `hostname`、`model`、`arch`、`platform`、版本号和构建信息。

保存配置时会重写 `/etc/config.json`。`tdaemon.sh` 监测到 `/etc/config.json` mtime 变化后，会 `killall -9 mqtt_bridge tk8710_gw`，随后由进程守护逻辑重新拉起，让新配置生效。

`config_sync_tdaemon_disabled()` 会更新 `/etc/tdaemon.conf` 中的网络禁用项，用于 Web 页面控制参与选路的接口。

### 7.3 网络管理

`network_manager.c` 负责：

- 读写 `/etc/network/interfaces` 中 `eth0` 的 DHCP/静态 IP 配置。
- 保留固定直连 IP `eth0:0 = 12.13.14.16/24`。
- 读写 `/etc/wpa_supplicant.conf` 的 Wi-Fi SSID/PSK。
- 通过 `ifconfig wlan0 | grep "inet "` 判断 Wi-Fi 是否已连接。

### 7.4 本地任务 IPC

Web 页面通过 Unix Domain Socket 触发业务进程任务：

- 扫频 socket：`/tmp/data_collect.sock`。
- 扫频状态：`/tmp/data_collect_status.json`。
- 扫频结果：`/userdata/SweepFreqResult/Result.txt`。
- ACM socket：`/tmp/acm.sock`。
- ACM 状态：`/tmp/acm_status.json`。
- ACM 结果：`/tmp/acm_result.json`。

以扫频为例：

```text
POST /api/rf_scan/start
    -> 校验 freq_start/freq_stop/mode
    -> ipc_client_start_task_with_args(/tmp/data_collect.sock, args)
    -> 业务进程快速返回 OK/BUSY/ERROR
    -> 页面轮询 /api/rf_scan/status
    -> 页面读取 /api/rf_scan/result
```

### 7.5 升级管理

`upgrade_manager.c` 通过后台线程执行：

```sh
swupdate -i /tmp/image.swu -e stable,copy1 > /tmp/swupdate.log 2>&1
```

成功后状态变为 `UPGRADE_DONE`，并在 3 秒后执行 `reboot`。失败时读取 `/tmp/swupdate.log` 尾部作为错误信息返回给页面。

## 8. tdaemon 超级守护

代码位置：`app/tdaemon`。

核心脚本：`tdaemon.sh`。

模块脚本：

- `tdaemon_common.sh`：配置解析、日志、锁、PID 文件等公共逻辑。
- `tdaemon_proc.sh`：进程扫描、启动和保活。
- `tdaemon_net.sh`：网络质量检测、默认路由和链路选择。
- `tdaemon_gpio.sh`：4G modem reset/power GPIO 控制。
- `tdaemon_recovery.sh`：网络恢复、软恢复、GPIO 恢复、无网络重启。

`tdaemon.sh` 启动流程：

1. 解析 `/etc/tdaemon.conf`。
2. 获取单实例锁 `/var/run/tdaemon_job.lock`。
3. 写 PID 文件 `/var/run/tdaemon_job.pid`。
4. 启动 Wi-Fi AP bootstrap 逻辑。
5. 初始化网络选择。
6. 每秒 tick 一次，按配置间隔执行网络检查和进程检查。
7. 热加载 `tdaemon.conf`。
8. 监测 `/etc/config.json` mtime，变化时重启 `mqtt_bridge` 和 `tk8710_gw`。
9. 长时间无网络 ready 时触发设备重启。
10. 定期截断 debug 日志，避免日志无限增长。

网络策略来自 README：系统不再自动从低优先级链路回切高优先级链路；只有当前活跃链路失效时才重新选路。链路优先级文档写为 `eth0 >  > wlan0`，中间空缺通常对应 4G/USB 链路，需要结合 `tdaemon_net.sh` 进一步确认实际接口名。

## 9. led_agent

代码位置：`app/light/src/led_agent.c`。

编译目标：`led_agent`。

硬件控制：`gpiochip1`。

| LED | GPIO offset | 说明 |
| --- | --- | --- |
| RUN LED | 23 | GPIO1_C7，低电平亮，高电平灭。 |
| NET LED | 24 | GPIO1_D0，低电平亮，高电平灭。 |

状态输入文件：

- `/run/led/mqtt.state`：由 `mqtt_bridge` 写入，表示 MQTT/业务收发状态。
- `/run/led/net.state`：由 `tdaemon` 网络模块写入，表示 L3 网络状态。

仲裁逻辑：

- RUN 灯：`connected` 常亮，`txrx` 时按时间窗口快闪，超过 3 秒无更新时间则熄灭。
- NET 灯：`L3_OK` 常亮，`RECOVERING` 约 2Hz 闪烁，超时或异常熄灭。
- 主循环每 100ms 执行一次。

## 10. log_daemon

代码位置：`app/log/daemon/log_daemon.c`。

功能：监听 UDP 日志端口，将其他进程通过 UDP 发来的日志写入 `/userdata/logs`。

`tdaemon.conf` 当前启动参数：

```text
/usr/bin/log_daemon 5514 /userdata/logs 262144 1000 10485760 20
```

含义按 `main(argc)` 解析顺序为：

- `5514`：UDP 监听端口。
- `/userdata/logs`：日志输出目录。
- `262144`：内存 ring buffer 大小。
- `1000`：flush 间隔，毫秒。
- `10485760`：单个日志文件最大大小。
- `20`：最多保留文件数。

主循环使用 `select()` 每 100ms 轮询 UDP socket，收到日志后先进入 ring buffer，达到容量或 flush 超时后落盘。

## 11. lan_discoveryd

代码位置：`app/Discovery/src/main.c`。

监听端口：UDP `20165`。

请求格式要求包含：

```json
{"cmd":"discovery_req","magic":"turmass_link"}
```

收到合法请求后，服务会：

1. 从 `/proc/net/route` 获取默认路由网卡。
2. 用 `ioctl` 获取该网卡 IP 和 MAC。
3. 从 `/etc/config.json` 读取 `gateway_id`。
4. 构造 `discovery_rsp` JSON。
5. 对请求来源单播回复 3 次，每次间隔 100ms。

## 12. 编译方式

各模块基本使用独立 Makefile。

常见变量：

- `ARCH ?= arm`：默认交叉编译 ARM 版本。
- `ARCH=x86`：部分模块可切到本机 gcc 编译。
- `BUILD_DIR=build_$(ARCH)`：输出目录如 `build_arm`。
- `TARGET`：输出程序名。

`webserver/Makefile` 相对更规范，默认从 RK3506 SDK 推导工具链路径：

```make
SDK_ROOT ?= $(abspath $(CURDIR)/../../..)
BUILDROOT_CFG ?= rockchip_hd_rk3506b_iot_emmc
HOST_DIR ?= $(SDK_ROOT)/buildroot/output/$(BUILDROOT_CFG)/host
SYSROOT ?= $(HOST_DIR)/arm-buildroot-linux-gnueabihf/sysroot
CC = $(HOST_DIR)/bin/arm-buildroot-linux-gnueabihf-gcc
```

`mqtt/Makefile`、`spi/Makefile`、`light/Makefile` 等仍硬编码了开发机路径，例如：

```make
/home/wxg/dwork/apptoolchain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
/home/wxg/dwork/rk3506sdk/rk3506_linux6.1_sdk_v1.2.0/...
```

如果换开发环境，需要优先整理这些路径变量。

## 13. 升级包制作

说明文件：`app/auto_deployment/制作最新版App升级包.md`。

脚本：`app/auto_deployment/制作最新版App升级包.ps1`。

流程要点：

1. Windows 侧先完成三个 ARM 程序编译：`tk8710_gw`、`web_server`、`mqtt_bridge`。
2. 脚本通过 WSL 检查文件是否为 ARM ELF。
3. 复制到 RK3506 SDK 的 `extra-parts/userdata/normal/`。
4. 更新 SDK 中的 `tdaemon.conf`。
5. 执行 `./build.sh buildroot`、`./build.sh extra-parts`、`./build.sh updateimg`。
6. 复制最终 `output/update/Image/update.img` 到 `D:\RK3506_Flash\rk3506b_iot_emmc_update.img`。
7. 输出 SHA-256，确认源升级包和目标升级包一致。

板端验证命令：

```sh
grep '^proc=' /etc/tdaemon.conf
ps | grep -E '[t]k8710_gw|[w]eb_server|[m]qtt_bridge'
ls -l /userdata/tk8710_gw /userdata/web_server /userdata/mqtt_bridge
tr '\0' ' ' </proc/$(pidof tk8710_gw)/cmdline; echo
```

## 14. 关键文件关系

```text
/etc/config.json
    -> mqtt_bridge 读取 MQTT Broker、topic、gateway_id
    -> web_server 读取和保存页面配置
    -> lan_discoveryd 读取 gateway_id
    -> tdaemon 比较内容指纹，变化后重启 mqtt_bridge/tk8710_gw，并通知FRP子进程退出重载

/etc/device_version.json
    -> mqtt_bridge 心跳版本
    -> web_server 系统信息页面

/etc/tdaemon.conf
    -> tdaemon 读取进程列表、网络检测参数、恢复参数
    -> web_server 可同步 disabled 网络接口

/run/led/mqtt.state
    -> mqtt_bridge 写
    -> led_agent 读，控制 RUN LED

/run/led/net.state
    -> tdaemon 写
    -> led_agent 读，控制 NET LED

/var/run/mqtt_stats.json
    -> mqtt_bridge 写 RF/MQTT 统计
    -> web_server 读并展示

/tmp/image.swu
    -> web_server 上传
    -> swupdate 安装
```

## 15. 代码阅读建议

建议按以下顺序继续深入：

1. `app/auto_deployment/Rk3506需要部署的文件.md`：先理解板端有哪些进程和文件。
2. `app/tdaemon/tdaemon.conf`、`app/tdaemon/tdaemon.sh`：理解系统如何启动、守护和恢复。
3. `app/mqtt/src/main.c`：理解 NS/MQTT 与本地业务之间的主通信链路。
4. `app/mqtt/inc/mac_msg_parser.h`、`app/mqtt/src/mac_msg_parser.c`：理解 JSON 与 C 结构体协议。
5. `app/SMP/ipc/include/ipc_smp.h`、`app/SMP/ipc/src/ipc_smp.c`：理解共享内存 IPC 的语义。
6. `app/spi/src/spi_app.c`：理解本地 SPI/TK8710 侧如何消费下行、生成上行。
7. `app/webserver/src/web_server.c`：理解页面/API 如何管理配置、网络、升级、RF 任务和日志。
8. `app/light/src/led_agent.c`、`app/log/daemon/log_daemon.c`、`app/Discovery/src/main.c`：理解外围状态展示、日志和发现服务。

## 16. 当前代码中需要注意的点

- `app/spi` 中的 `tk8710_gw` 当前像模拟/集成样例；部署文档中真实 `tk8710_gw` 来源写的是另一个 HAL 工程路径，需要确认最终固件使用哪份源码。
- 多个 Makefile 仍包含开发者个人路径，迁移环境时会直接影响交叉编译。
- `mqtt/inc/mac_msg_parser.h` 和 `spi/inc/spi_app.h` 中协议结构体存在重复定义，后续修改协议时必须同步，否则 IPC 两端结构体布局可能不一致。
- `web_server.c` 文件超过 2000 行，页面、API、文件解析、业务调度混在一起，后续维护建议按配置、网络、RF、升级、日志拆分。
- `tdaemon.sh` 当前开头启用了 `set -x` 并把 stderr 写入 `/root/tdaemon_debug_full.log`，部署注意事项里提到量产应删除 `set -x`，避免日志过多。
- `tdaemon.conf` 中部分中文注释存在编码异常，建议统一保存为 UTF-8。
- Web 登录认证当前通过 Cookie 字符串 `session=admin` 判断，适合内网设备管理，若暴露到不可信网络需要加强认证和 CSRF 防护。
- `upgrade_manager.c` 使用 `system()` 调用 `swupdate`，上传路径固定为 `/tmp/image.swu`，应保证 Web 层文件名和大小校验持续有效。

## 17. 看门狗与软件守护实现（2026-09-21核查）

2026-09-22更新：新增`watchdog/rk_wdt.c`及S15rk-watchdog，已实现独立片内喂狗、前置失败业务继续运行及Web状态告警。以下为新增服务之前的软件守护审阅快照；最新硬件WDT服务流程、SDK配置及验证边界见[watchdog/README.md](watchdog/README.md)。

### 17.1 核查结论及范围

本目录存在多层软件守护，但在可检查的C/H、Shell、启动脚本和配置文件中，没有找到应用直接打开`/dev/watchdog`、调用`WDIOC_KEEPALIVE`/`WDIOC_SETTIMEOUT`或周期写入硬件看门狗设备的实现。FRP脚本中的`watchdog`和LED注释中的“看门狗”均指软件逻辑。

这不能推出RK3506内核或Bootloader未启用硬件看门狗：本节未检查板端驱动、最终设备树或外部服务的实际运行状态。`tk8710app/`中是预编译ARM程序及测试数据，不能仅凭本目录源码搜索排除二进制内部保护；实际TK8710程序源码在独立HAL工程中，需匹配所部署版本另行审查。

| 机制 | 检测对象 | 异常动作 | 实现位置 |
|---|---|---|---|
| 进程守护 | 配置列表中的进程是否存在 | 不存在时重新启动 | [tdaemon_proc.sh](tdaemon/tdaemon_proc.sh)，`proc_tick/proc_is_running/proc_restart_and_wait` |
| 网络恢复 | 物理接口、IP、Ping结果 | 接口重启、4G复位/电源循环、选路切换 | [tdaemon_net.sh](tdaemon/tdaemon_net.sh)、[tdaemon_recovery.sh](tdaemon/tdaemon_recovery.sh)、[tdaemon_gpio.sh](tdaemon/tdaemon_gpio.sh) |
| 连续无网络保护 | `NETWORK_READY`持续为0 | 请求整机重启 | [tdaemon.sh](tdaemon/tdaemon.sh)，`net_reboot_due_to_no_network()` |
| FRP子进程守护 | `frpc`是否退出 | 等待5秒后重新生成配置并启动 | [start_frpc.sh](frp/start_frpc.sh) |
| LED状态超时 | MQTT/网络状态文件的时间戳 | 熄灭对应指示灯 | [led_agent.c](light/src/led_agent.c) |
| DNS自修复 | 解析文件与配置是否一致 | 原子恢复DNS文件 | [tdaemon_dns.sh](tdaemon/tdaemon_dns.sh) |
| TK8710停止复位 | 手动执行S99 stop后芯片可能仍工作 | GPIO复位脉冲 | [S99tdaemon_job](tdaemon/S99tdaemon_job)、[tk8710_reset.c](tdaemon/tk8710_reset.c) |

以上都需要Linux及相关用户态进程继续获得调度。内核死锁、CPU卡死或tdaemon自身失去响应时，本目录没有已确认的独立硬件喂狗机制兜底。

### 17.2 tdaemon启动、检查及拉起流程

`S99tdaemon_job start`通过`start-stop-daemon`启动`/usr/bin/tdaemon.sh`。`tdaemon_main()`调用`tdaemon_run()`，加载common、GPIO、network、DNS、recovery、proc模块，获取单实例锁，读取配置，初始化AP和网络选择，最后进入主循环。

```text
S99tdaemon_job start
  → tdaemon_run：锁、配置、PID文件、网络初始化
  → 循环：配置热加载 → DNS检查 → config.json指纹检查
  → 到期net_tick：检测/恢复网络
  → 到期proc_tick：检查/拉起配置中的程序
  → 调试日志容量控制 → 无网络计数 → sleep 1
```

当前[tdaemon.conf](tdaemon/tdaemon.conf)的`[process]`默认守护：

```text
/usr/bin/log_daemon 5514 /userdata/logs 262144 1000 10485760 20
/userdata/mqtt_bridge
/userdata/web_server
/userdata/tk8710_gw -w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level warn
/userdata/frp/start_frpc.sh
```

以板端`/etc/tdaemon.conf`实际内容为准。`led_agent`、`lan_discoveryd`不在这份默认proc列表内，不能认为所有应用都由tdaemon自动拉起。

`proc_is_running()`对普通程序首先使用`pidof`按名称判断，对Shell脚本及后续检查读取`/proc/<pid>/cmdline`匹配命令行。`proc_restart_and_wait()`把配置命令拆成参数，以后台方式启动并将stdout/stderr重定向到`/dev/null`，在指定窗口内检查进程是否出现。

进程已存在则继续下一项。MQTT/TK8710进程不存在且`NETWORK_READY!=1`时，记录“启动阻断：网络未就绪”，不会启动；其它默认进程不受这个网络门控限制。进程已存在时不会通过这个分支主动终止它。

“进程重启成功”只表示检查时进程存在，不代表完成NS配置、射频初始化、Broker登录或Web健康检查。当前没有定期调用业务健康API，也不能发现一个PID仍存在但线程死锁的程序；没有自动限制总重启次数或指数退避。

### 17.3 当前默认参数与时间语义

| 配置节/参数 | 文件默认值 | 实际作用 |
|---|---|---|
| `[daemon] scan_interval` | 30 | 每30个主循环tick安排一次进程巡检 |
| `[daemon] restart_timeout` | 3 | 拉起后轮询进程存在，窗口约3秒，另有命令执行耗时 |
| `[network] check_interval` | 10 | 每10个tick安排一次网络检测 |
| `[network] fail_threshold` | 3 | 连续网络检测不达标达到3次后进入恢复 |
| `[network] targets` | 223.5.5.5,119.29.29.29,180.76.76.76 | 多目标探测，任一目标符合阈值即可 |
| `[network] ping_count` | 5 | 每个目标发送5个Ping包 |
| `[network] loss_threshold` | 5 | 丢包百分比上限5% |
| `[network] latency_threshold_ms` | 1000 | 延迟指标上限1000ms |
| `[network] recovery_light_threshold/recovery_soft_threshold` | 1 / 2 | 恢复尝试次数对应的升级阈值 |
| `[network] recovery_gpio_cooldown_s` | 100 | 两次硬件恢复之间的冷却限制 |
| `[daemon] no_ready_reboot_timeout` | 1200 | 无网络计数阈值；0关闭整机自动重启 |

主循环的tick在每轮增加1，末尾sleep 1，但Ping、接口等待及恢复在同一循环中执行。因此30和10不能解释为严格墙上时间的30秒、10秒。`no_net_ready_seconds`也是每轮加1，没有按单调时钟累计；1200在配置注释中写成20分钟，实际故障恢复耗时会使整机重启晚于20分钟，阻塞严重时可能更久。

### 17.4 网络分级恢复和整机重启

`net_tick()`维护活跃接口和失败次数。正常检测重置失败/恢复状态并更新NET灯状态；持续失败触发`recovery_attempt_iface()`，按尝试次数提升恢复层级，恢复后重新探测。

- level 1：轻量恢复，eth0/usb0调用`net_ifup_iface()`；wlan0调用`switch_to_sta.sh`，等待链路后Ping。
- level 2：`net_release_iface()`释放接口，再执行轻量恢复。
- level 3：`gpio_modem_reset_pulse()`，只允许usb0，随后重新启用并验证网络。
- level 4：`gpio_modem_power_cycle()`，只允许usb0，随后重新启用并验证网络。

配置中4G复位为gpiochip0 line19、高有效、1000ms；4G电源为gpiochip0 line20、高电平开机、断电保持3000ms，上电等待15秒、枚举等待20秒。它们控制4G模块，不能与TK8710的gpiochip0 line13低有效复位混淆。

源码边界：`recovery_iface_max_level()`给eth0最大2级、wlan0最大3级、usb0最大4级，但level 3的`recovery_gpio_reset_attempt()`只接受usb0。因此当前wlan0升级到level 3会返回失败，未实现对应Wi-Fi硬件复位，不能只根据“最大3级”声称已覆盖。

当`NETWORK_READY=1`时，无网络计数和已请求重启标志归零；否则每轮加1，到阈值后调用`net_reboot_due_to_no_network()`。该函数先sync，再尝试`reboot -f`，失败则普通reboot，按命令可用性寻找`/sbin/reboot`或BusyBox入口。多处使用`|| true`，调用返回不证明设备已经重启；标志设为1后，持续无网络期间也不会每轮重复请求。

这是软件主动重启，依赖Shell和系统调用正常工作。Ping连通性不等价于DNS正常、MQTT业务成功或射频正常。

### 17.5 配置变化、DNS和维护暂停

`/etc/config.json`变化通过内容指纹检测，已非旧文档中的mtime。检测到变化后强制停止mqtt_bridge和tk8710_gw，调用`proc_reload_frp_config()`通知匹配路径的frpc子进程退出，安排下一轮进程检查。FRP外层脚本保持运行并重新生成配置。

`dns_ensure_config()`在`/etc/tdaemon-dns-owner`存在时检查并恢复主备DNS，解决解析文件被其它网络服务清空的问题。详见[DNS统一管理与FRP恢复验证](tdaemon/DNS统一管理与FRP恢复验证.md)。

`/var/run/app-upgrade-maintenance`存在时，主循环暂停配置/网络/进程控制，`proc_tick()`入口及逐项检查也会退出；期间无网络计数不增加。这个标记不是硬件看门狗控制，也不能中断已经进入的阻塞恢复动作。升级失败或异常遗留标记会抑制守护，应结合升级事务确认后处理。

### 17.6 FRP自己的守护循环

`frp/start_frpc.sh`使用`/var/run/start_frpc.lock`及PID防止重复启动，`prepare_config()`读取`/etc/config.json`的gateway_id，替换frpc.toml中的DEVICE_ID_STRING，生成frpc.generated.toml。随后后台启动frpc、保存子PID并wait。

子进程不论正常还是异常退出，记录退出码并默认sleep 5秒，然后重新读取配置并启动。配置缺失、程序不可执行或gateway_id非法时同样等待再试。RESTART_DELAY等路径/参数可由环境变量覆盖。

TERM/INT及EXIT trap调用cleanup，尝试停止子进程并清理锁。SIGKILL不能执行trap，所以强杀启动脚本不保证frpc子进程退出。外层脚本存在也不代表FRP登录成功，应查看`/var/log/frpc.log`中的`login to server success`、`start proxy success`；DNS错误下“反复启动→立即退出→等5秒”仍属于守护循环运行中。

### 17.7 LED里的“看门狗”仅用于显示

`light/src/led_agent.c`每100ms读取`/run/led/mqtt.state`和`/run/led/net.state`。MQTT通过`mqtt_led_tick_connected()`及收发事件更新状态文件；tdaemon通过`net_led_write_state()`发布网络状态。

- MQTT状态为空，或时间戳超过3000ms没有更新：熄灭RUN灯。
- `txrx`状态小于200ms：保持当前灯；200～1000ms快闪；1000～3000ms常亮。
- 网络状态超过15000ms未更新：熄灭NET灯；新鲜状态中L3_OK常亮、RECOVERING闪烁、其它熄灭。

这段逻辑不会kill进程、重启设备或操作/dev/watchdog。时间戳基于系统时间，校时跳变会影响判断；网络探测本身可能超过15秒，因此NET灯超时也不能单独证明tdaemon死亡。

### 17.8 S99 stop复位和守护自身的边界

当前S99 stop停止守护及proc配置的程序，确认tk8710_gw退出后调用`/usr/bin/tk8710_reset`，以gpiochip0 line13拉低至少10ms再拉高。失败返回非零，restart不继续start。这是用户停止流程的硬件复位，不是周期喂狗，配置变化或其它kill路径不会自动经过它。详见[S99停止与TK8710硬件复位说明](tdaemon/S99停止与TK8710硬件复位说明.md)。

S99 start和PID/锁只能启动及抑制重复实例，不能保证tdaemon自己退出后再启动。脚本注释提及inittab的`respawn`，但注释不会配置init；需核对最终固件。若板端另行启用respawn，直接stop可能被init重新拉起，要协调停用行为。

### 17.9 排查命令与验收边界

以下为板端只读检查，不执行stop、复位或故障注入：

```sh
grep '^proc=' /etc/tdaemon.conf
grep -E 'scan_interval|check_interval|fail_threshold|no_ready_reboot_timeout' /etc/tdaemon.conf
ps | grep -E '[t]daemon.sh|[m]qtt_bridge|[t]k8710_gw|[s]tart_frpc.sh|[f]rpc'
cat /var/run/tdaemon_job.pid
cat /var/run/tdaemon_job.state
ls -l /var/run/app-upgrade-maintenance
tail -80 /root/daemon_guard.log
tail -40 /var/log/frpc.log
cat /run/led/mqtt.state /run/led/net.state
grep -n 'respawn.*tdaemon' /etc/inittab
ls -l /dev/watchdog* /sys/class/watchdog/
```

`ls /dev/watchdog*`有设备只证明驱动注册，不证明已有人喂狗；不要为了检查随意打开或写入watchdog设备，以免启动倒计时。本节于2026-09-21完成本地源码交叉核查与文档更新，没有对板卡进行停进程、断网、超时重启、硬件喂狗或GPIO动作验证。

建议后续维护优先明确三项：进程存在与业务健康分开检查；超时计时改用实际单调时钟；若需要内核卡死后自动恢复，单独设计并验证硬件看门狗及喂狗条件。上述建议尚未作为本次代码变更实施。
