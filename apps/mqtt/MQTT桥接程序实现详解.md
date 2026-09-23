# RK3506 MQTT 桥接程序实现详解

> 2026-09-10扩展：系统遥测、tk8710_gw进程时长、NS扫频请求/结果/业务ACK已实现。原章节描述的五类报文是旧接口基础，新增接口以文末第13节为准。设计及板端证据见 `docs/ns-telemetry-scan/`。

## 1. 分析范围

本文基于当前源码静态分析：

- `mqtt/src/main.c`：主流程、MQTT、IPC、心跳、GPIO和事件循环。
- `mqtt/src/config.c`、`mqtt/inc/config.h`：`/etc/config.json` 配置。
- `mqtt/src/mac_msg_parser.c`、`mqtt/inc/mac_msg_parser.h`：NS JSON与内部结构体互转。
- `mqtt/inc/ipc_smp.h`、`mqtt/IPCUSAGE.md`：共享内存IPC。
- `tk8710_hal_v1.6/src/tk8710_ipc_comm.c`：`tk8710_gw` 对端实现。
- `tk8710_hal_v1.6/test/example/tk8710_gw.c`：TK8710业务接入点。

本文描述源码行为，不等同于NS联调或板端运行验证。

## 2. 程序定位

`mqtt_bridge` 是NS与 `tk8710_gw` 之间的双向协议桥：

```text
NS / MQTT Broker
       | MQTT + JSON
       v
+---------------------+
| mqtt_bridge         |
| Mosquitto + JSON-C  |
+---------------------+
       | 共享内存SPSC IPC
       v
+---------------------+
| tk8710_gw           |
| TK8710 HAL / TRM    |
+---------------------+
       | SPI/驱动
       v
TK8710及射频链路
```

它不直接操作TK8710 SPI。配置执行、射频下发和接收由 `tk8710_gw` 完成。

## 3. 文件和依赖

| 文件                     | 作用                                         |
| ------------------------ | -------------------------------------------- |
| `src/main.c`           | Mosquitto回调、IPC桥接、心跳、GPIO和状态文件 |
| `src/config.c`         | 读取`/etc/config.json`                     |
| `src/mac_msg_parser.c` | JSON解析、Base64编解码、上行JSON封装         |
| `src/log_udp.c`        | UDP日志                                      |
| `inc/mac_msg_parser.h` | NS/TK8710内部消息结构                        |
| `inc/ipc_smp.h`        | 共享内存环形队列API                          |
| `config.json`          | 开发目录配置示例，板端不直接使用它           |
| `Makefile`             | ARM交叉编译和依赖                            |

主要依赖：`libmosquitto`、`libjson-c`、`libmbedcrypto`、`libipc_smp`、`libgpiod`、pthread、rt和m。

## 4. 启动和事件循环

入口为 `src/main.c:972` 的 `main()`：

1. 初始化上下文并解析 `raw/ver/help/logip=/logport=` 参数。
2. 从 `/etc/config.json` 加载MQTT地址、认证、Topic、心跳周期和QoS。
3. 从 `/etc/device_version.json` 读取 `comm_version`。
4. 监听 `gpiochip1 line 17` 的设备电源状态双边沿事件。
5. 初始化MQTT到TK8710、TK8710到MQTT两条IPC通道。
6. 将TK8710上行通知FD和GPIO事件FD加入 `epoll`。
7. 以 `gateway_id` 为Client ID创建Mosquitto客户端并连接Broker。
8. Mosquitto网络线程处理MQTT；主线程每100ms处理IPC、GPIO和定时任务。
9. 收到SIGINT/SIGTERM后关闭MQTT、IPC、GPIO、epoll和互斥锁。

MQTT使用clean session；keepalive为 `heartbeat_interval * 2`。MQTT keepalive与业务JSON心跳是两套机制。

## 5. 对上NS接口

### 5.1 MQTT配置

运行时配置文件是 `/etc/config.json`：

| 字段                   | 用途                     | 代码默认值     |
| ---------------------- | ------------------------ | -------------- |
| `mqtt_host`          | Broker地址               | 空，必须配置   |
| `mqtt_port`          | Broker端口               | 1883           |
| `username/password`  | MQTT认证                 | 空             |
| `gateway_id`         | MQTT Client ID           | 空             |
| `pub_topic`          | 心跳、配置请求和业务上行 | 空             |
| `sub_topic`          | 配置和业务下行           | 空             |
| `heartbeat_interval` | 业务心跳周期，秒         | 60             |
| `qos`                | 发布/订阅QoS             | 0，非法值回退0 |
| `net_interface`      | 配置网络类型             | Ethernet       |

源码不拼接Topic，完全使用配置值。所有发布使用配置QoS且 `retain=false`。

### 5.2 NS配置下行（新增GPS控制字段）

**新增必填开关 `set.config.gps_enable`（integer）：1开启GPS外部同步，0关闭并使用本地同步。** 必须发送JSON整数0或1，缺失、字符串、布尔值及其他整数均拒绝解析。

```json
{"set":{"config":{"gps_enable":1,"freq":470300000,"nwk_num":4,"tdd_num":1,"slot_cfg":0,"rate_num":1,"rate_cfgs":[{"rate":3,"uplink_pkt":1,"downlink_pkt":1}]}}}
```

行为矩阵：

| NS开关     | GPS硬件状态                             | 网关行为                                                 |
| ---------- | --------------------------------------- | -------------------------------------------------------- |
| 0          | 存在或不存在                            | 不探测/初始化GPS UART，关闭已申请的GPS句柄，使用本地同步 |
| 1          | 存在且健康、PPS可对齐                   | GPS初始化和对齐成功后启动业务，外部同步                  |
| 1          | 不存在、检测失败、健康检查或PPS对齐失败 | 非零退出，不回退本地同步、不继续业务启动                 |
| 缺失或非法 | 任意                                    | MQTT拒绝配置，不发送给网关                               |

网关启动时先等待NS配置，再按开关初始化GPS，未收到配置时禁止旧的默认业务启动。只修改gps_enable也会触发配置更新；扫频恢复沿用最近成功应用的配置及同一GPS开关。关闭GPS指关闭网关的GPS使用/同步逻辑，不表示对外部GPS设备断电。

兼容性：两端NsConfigDown_t末尾新增int gps_enable，必须配套部署MQTT和tk8710_gw；网关严格检查新结构长度和0/1取值，拒绝旧长度配置，防止共享内存遗留消息误用默认0。原消息类型枚举保持不变。NS升级必须给每条set.config补齐开关。

识别 `set.config`：

```json
{"set":{"config":{"gps_enable":1,"freq":470300000,"nwk_num":4,"tdd_num":1,"slot_cfg":0,"rate_num":1,"rate_cfgs":[{"rate":0,"uplink_pkt":1,"downlink_pkt":2}]}}}
```

解析为 `NsConfigDown_t`，包含 `freq/nwk_num/tdd_num/slot_cfg/rate_num/rate_cfgs/gps_enable`，再通过IPC交给 `tk8710_gw`。TK8710侧缓存配置，判断首次配置或更新后调用配置回调。gps_enable变化也属于配置更新。

### 5.3 NS数据下行

根对象包含 `tdd` 和字符串 `msg`，且没有 `rssi` 时识别为下行：

```json
{"tdd":0,"rate":2,"slot":3,"msg":"QIAAAQAAAAAAAEQETAQIDBAUGBwgJEA=="}
```

流程：

```text
NS JSON -> on_message -> Base64解码msg(最多520字节)
-> NsDataDown_t -> /ipc_mqtt_to_spi -> tk8710_gw
-> TK8710HalSendData -> 射频下发
```

TK8710侧映射：

| NS字段        | TK8710行为                    |
| ------------- | ----------------------------- |
| `slot=1`    | `TK8710_DOWNLINK_A`         |
| 其他slot      | `TK8710_DOWNLINK_B`         |
| `rate=0..7` | 模式5、6、7、8、9、10、11、18 |
| `tdd=255`   | `TK8710_DATA_TYPE_DED`      |
| 其他tdd       | `TK8710_DATA_TYPE_BRD`      |

当前TK8710实现中用户ID固定0、发射功率固定31，需要协议负责人确认。

### 5.4 网关数据上行

`tk8710_gw` 将 `TRM_RxDataList` 转为最多128项的 `GwBatchDataUp_t`。MQTT桥校验后每64项拆成一个JSON数组发布：

```json
[{"tdd":12,"rate":2,"slot":0,"rssi":-80,"snr":15,"freq_offset":3,"msg":"...Base64..."}]
```

| MQTT字段        | TK8710来源                       |
| --------------- | -------------------------------- |
| `tdd`         | `TRM_RxDataList.frameNo`       |
| `rate`        | 模式5/6/7/8/9/10/11/18映射为0..7 |
| `slot`        | 当前固定0                        |
| `rssi/snr`    | `TRM_RxUserData`               |
| `freq_offset` | 当前为`user->freq / 128`       |
| `msg`         | `user->data` Base64编码        |

### 5.5 配置请求

TK8710通过IPC发送 `ConfigReq_t{req_data_type=1}`，桥发布：

```json
{"get":{"config":true}}
```

若没有收到 `NS_CONFIG_DOWN`，每10秒重发；收到配置后停止。MQTT重连会清空桥内请求状态，需要TK8710再次触发。

## 6. 对下tk8710_gw接口

### 6.1 IPC通道

| 方向       | 共享内存             | MQTT角色 | TK8710角色 | IPC type   |
| ---------- | -------------------- | -------- | ---------- | ---------- |
| NS到TK8710 | `/ipc_mqtt_to_spi` | Producer | Consumer   | `0x1001` |
| TK8710到NS | `/ipc_spi_to_mqtt` | Consumer | Producer   | `0x2001` |

两端参数：4MiB ring、最大消息 `sizeof(GwBatchDataUp_t)+256`、批量8条、通知超时20ms。启动时先附着，失败再创建。

### 6.2 两层类型

- `ipc_smp_msg_hdr_t.type` 是传输方向类型 `0x1001/0x2001`。
- payload首字段 `MacMsgType_e` 是业务类型：配置下行、数据下行、数据上行、心跳、配置请求。

结构体是同机ABI，不是稳定网络协议。两端必须保持枚举值、字段顺序、类型、对齐、`size_t`宽度及容量宏完全一致。

### 6.3 可靠性

MQTT到IPC遇到ring满最多重试3次，每次间隔2ms，仍失败就丢弃，没有持久化和NS业务ACK。

TK8710上行时，桥清通知、排空ring、转JSON、调用 `mosquitto_publish()`，然后ACK IPC。发布调用成功仅表示Mosquitto客户端接受，不代表NS收到；QoS 0没有端到端送达保证。

## 7. NS心跳重点

### 7.1 实际报文

```json
{"alive":{"ver":"<comm_version>","uptime":1789000000,"net":"eth0","power":1,"lon":"113.123456","lat":"23.123456"}}
```

发布到 `pub_topic`，QoS取配置，retain为false。

### 7.2 字段来源和真实语义

| 字段        | 来源                                                | 真实语义                              | 异常值                                |
| ----------- | --------------------------------------------------- | ------------------------------------- | ------------------------------------- |
| `ver`     | `/etc/device_version.json` 的 `comm_version`    | 通信/固件版本，不是APP_VERSION        | `unknown`                           |
| `uptime`  | `time(NULL)`                                      | Unix时间戳，不是运行时长              | 系统时间不准时异常                    |
| `net`     | `/proc/net/route` 第一条默认路由                  | `eth0/usb0/wlan0`等接口名           | `unknown`                           |
| `power`   | `gpiochip1 line 17`                               | GPIO原始逻辑电平                      | 读取失败沿用缓存，初值1               |
| `lon/lat` | `/userdata/GPS/GPSinfo.txt` 的 `lon=`、`lat=` | 字符串经纬度                          | `unknown`                           |
| `status`  | 同文件的`status=`                                 | 连续GPS基础健康检测次数，JSON非负整数 | 缺失、非法、读取失败或经纬度缺失时为0 |

`/userdata/GPS/GPSinfo.txt` 期望：

```text
lon=113.123456
lat=23.123456
status=12
```

文件缺失时，代码仍上报字符串形式的 `lon:"unknown",lat:"unknown"`。

新增心跳字段 `alive.status` 是检测次数，不是布尔标志。GPS采集程序每次完整通过基础健康检测后累计；任一状态查询、位置查询失败或基础状态异常时，必须将文件中的 `status` 写为0。MQTT只读取并转发，不执行GPS检测或自行累计。旧文件缺少status时兼容为0。

文件由GPS程序维护，建议通过临时文件加rename原子替换，避免MQTT读到半个文件。文件位于持久化userdata中；GPS程序启动时需重置status，停止检测时也应归零。当前未定义时间戳/有效期，MQTT无法判断完整文件中的正数是否已经过期。

新心跳的位置部分示例：`"lon":"114.005377","lat":"30.004928","status":12`。既有TK8710 IPC结构不增加字段；仅本地GPS心跳增加status，旧IPC心跳封包保持原格式。

### 7.3 触发时机

| 触发         | 行为                                        |
| ------------ | ------------------------------------------- |
| MQTT连接成功 | 订阅下行后立即发一次                        |
| 周期         | 每`heartbeat_interval` 秒发送             |
| 电源GPIO变化 | GPIO1_C1双边沿触发，200ms软件限频后立即发送 |

GPIO事件会重置周期心跳计时。

### 7.4 协议差异

现有TGMP只定义 `ver` 和“运行时长” `uptime`；当前代码增加 `net/power/lon/lat`，而且 `uptime` 实际为Unix时间戳。NS与网关必须统一：要么正式定义为时间戳，要么改为 `CLOCK_BOOTTIME` 的系统运行秒数。

### 7.5 TK8710心跳分支

IPC上行还接受TK8710生成的 `GW_ALIVE_UP` 并发布 `ver/uptime/net/power`，不带位置。当前 `tk8710_ipc_comm.c` 主要发送配置请求和业务数据，没有找到主动发送该结构的调用，所以周期心跳实际来自RK3506本地 `send_heartbeat()`。

## 8. 辅助状态

- `/run/led/mqtt.state`：连接时 `CONNECTED`，成功收发后短时 `TXRX`，2秒后恢复。
- `/var/run/mqtt_stats.json`：记录MQTT下行消息数、TK8710上行用户条目数和更新时间；最多每秒落盘一次。
- `raw` 参数开启NS原始JSON和结构语义日志；`logip=`、`logport=` 设置UDP日志目的地。
- `APP_VERSION` 当前为 `V1.0.2`，只由 `mqtt_bridge ver` 输出，不用于心跳版本。

## 9. 完整流程

### NS下行

```text
Broker -> on_message -> JSON分类/Base64解码 -> 内部结构体
-> ipc_smp_send(/ipc_mqtt_to_spi) -> tk8710_gw epoll
-> 配置回调或TK8710HalSendData
```

### TK8710上行

```text
TK8710接收 -> OnTrmRxData -> IpcSendUplinkData
-> /ipc_spi_to_mqtt -> mqtt_bridge epoll
-> 每64项转JSON/Base64 -> mosquitto_publish(pub_topic)
```

## 10. 风险和建议

### 高优先级

1. 统一 `uptime` 的时间戳/运行时长语义。
2. 两工程共用同一协议头；当前MQTT叫 `MSG_TYPE_CONFIG_REQ`，TK8710相同数值叫 `MSG_TYPE_CONFIG_FEQ`，后续容易漂移。
3. 严格校验 `freq/nwk_num/tdd/rate/slot/rate_num/payload_len` 范围。
4. Base64失败当前只打印错误，仍可能下发零长度payload，应直接拒绝。
5. 定义MQTT发布、Broker确认和NS处理之间的送达级别。

### 中优先级

1. `on_connect()` 在 `sub_topic` 为空时直接返回，连首次心跳也不发。
2. `heartbeat_interval` 没有正数范围校验。
3. `power` 极性硬编码，应在协议或配置中明确。
4. 无定位时建议上报JSON `null`，而非字符串 `unknown`。
5. 补充下行业务ACK，让NS知道射频发送是否成功。
6. 确认上行slot固定0、频偏除128、下行功率31和用户ID0是否符合正式协议。

## 11. 联调检查

```sh
ps | grep '[m]qtt_bridge'
cat /etc/config.json
cat /etc/device_version.json
cat /userdata/GPS/GPSinfo.txt
cat /var/run/mqtt_stats.json
cat /run/led/mqtt.state
ls -l /dev/shm | grep ipc
```

详细日志运行前先停止现有实例，SPSC通道不能有两个同角色进程：

```sh
/userdata/mqtt_bridge raw logip=<日志服务器IP> logport=5514
```

联调至少覆盖首次/周期/GPIO心跳、配置10秒重试、配置执行、下行Base64、1/64/65/128项上行、Broker断网、IPC满、非法JSON和非法Base64。

## 12. 源码索引

| 功能           | 位置                                      |
| -------------- | ----------------------------------------- |
| 主入口         | `mqtt/src/main.c:972`                   |
| MQTT连接       | `mqtt/src/main.c:688`                   |
| NS下行         | `mqtt/src/main.c:712`                   |
| TK8710上行     | `mqtt/src/main.c:783`                   |
| 本地心跳       | `mqtt/src/main.c:898`                   |
| 配置请求       | `mqtt/src/main.c:942`                   |
| IPC初始化      | `mqtt/src/main.c:621`                   |
| 电源GPIO       | `mqtt/src/main.c:95`                    |
| 配置加载       | `mqtt/src/config.c:107`                 |
| JSON解析       | `mqtt/src/mac_msg_parser.c:33`          |
| 上行封装       | `mqtt/src/mac_msg_parser.c:132`         |
| 心跳封装       | `mqtt/src/mac_msg_parser.c:169`         |
| TK8710 IPC对端 | `tk8710_hal_v1.6/src/tk8710_ipc_comm.c` |

## 13. NS遥测与扫频扩展（2026-09-10）

本节为当前接口参数表。“新增”表示相对原来的五类MQTT业务消息新增；GPS `status` 是此前已增加的字段，本次保留。上行仍使用配置文件的 `pub_topic`，下行仍使用 `sub_topic`，不新增Topic，不发送统一的 `type` 字段。

| 方向     | JSON识别结构                  | 变更           | 作用                         | QoS           |
| -------- | ----------------------------- | -------------- | ---------------------------- | ------------- |
| 网关到NS | `alive`                     | 扩展           | 心跳、GPS、系统及进程状态    | 配置的qos     |
| 网关到NS | `get.config`                | 保留           | 请求NS无线配置               | 配置的qos     |
| NS到网关 | `set.config`                | 保留           | 无线参数配置                 | NS选择，建议1 |
| NS到网关 | `tdd/rate/slot/msg`         | 保留           | TK8710业务下发               | NS选择        |
| 网关到NS | 根节点为数组                  | 保留           | 批量业务上行                 | 配置的qos     |
| NS到网关 | `request_id` + `set.scan` | **新增** | 发起扫频任务                 | NS建议1       |
| 网关到NS | `scan_ack`                  | **新增** | 请求受理、拒绝或重复任务状态 | 固定1         |
| 网关到NS | `scan_result`               | **新增** | 扫频完整结果或失败信息       | 固定1         |
| NS到网关 | `scan_result_ack`           | **新增** | NS确认结果已接收保存         | NS建议1       |

所有网关发布均为retain=false。NS控制请求也应使用retain=false，避免旧控制请求因订阅重新建立而被重新投递。一个下行报文只承载一种业务，不把set.config和set.scan混在同一个JSON内。

### 13.1 心跳新增字段

保留alive.uptime的Unix时间戳。新增字段如下，示例数值仅展示类型：

```json
{"alive":{"system":{"uptime_s":260478,"ram_total_bytes":501231616,"ram_used_bytes":144863232,"ram_used_percent":28.90,"disk_total_bytes":1014894592,"disk_used_bytes":43548672,"disk_used_percent":4.29,"load_avg":[0.08,0.13,0.10]},"tk8710_gw":{"running":true,"uptime_s":107}}}
```

system.uptime_s为Linux开机秒数。RAM口径与Web一致：totalram-freeram，包含缓存；磁盘统计/userdata。tk8710_gw.uptime_s从/proc进程启动时刻计算，精确匹配进程名，不上报PID。进程不存在为false/null，无法查询为null/null。新模块为src/gateway_telemetry.c。

GPS仍读取/userdata/GPS/GPSinfo.txt，文件包含lon、lat和连续健康计数status；alive.status与scan任务state不是同一含义。

#### 心跳参数全集

以下是本地周期心跳的字段；原IPC转发的GW_ALIVE_UP分支仍保留旧格式，不保证有system、tk8710_gw和GPS字段。

| 字段路径                           | JSON类型        | 单位/含义                                         | 变更和异常值                                       |
| ---------------------------------- | --------------- | ------------------------------------------------- | -------------------------------------------------- |
| `alive.ver`                      | string          | device_version.json的comm_version                 | 保留；失败为unknown                                |
| `alive.uptime`                   | integer         | Unix时间戳，秒                                    | 保留；不是开机时长                                 |
| `alive.net`                      | string          | 默认路由接口名                                    | 保留；失败为unknown                                |
| `alive.power`                    | integer         | GPIO1_C1原始电平0或1                              | 保留；读失败沿用缓存，初值1                        |
| `alive.rfstatus`                 | string或null    | 8位二进制字符串，从左到右为天线7～0，1正常、0异常 | **新增**；读取失败或格式错误为null           |
| `alive.lon`                      | string          | GPS经度原文                                       | 路径已改为GPSinfo.txt；缺失为unknown               |
| `alive.lat`                      | string          | GPS纬度原文                                       | 路径已改为GPSinfo.txt；缺失为unknown               |
| `alive.status`                   | integer         | 连续GPS基础健康检测次数                           | 此前新增，保留；缺失/非法/读失败为0                |
| `alive.system.uptime_s`          | integer或null   | Linux开机运行秒数                                 | **新增**；sysinfo失败为null                  |
| `alive.system.ram_total_bytes`   | integer或null   | RAM总量，字节                                     | **新增**                                     |
| `alive.system.ram_used_bytes`    | integer或null   | RAM已用量，字节，total-free                       | **新增**；包含缓存                           |
| `alive.system.ram_used_percent`  | number或null    | RAM占用百分比，0到100                             | **新增**；不是0到1比例，不带百分号           |
| `alive.system.disk_total_bytes`  | integer或null   | /userdata分区总量，字节                           | **新增**；不是整片eMMC总量                   |
| `alive.system.disk_used_bytes`   | integer或null   | /userdata已用量，字节                             | **新增**                                     |
| `alive.system.disk_used_percent` | number或null    | /userdata占用百分比                               | **新增**；statvfs失败为null                  |
| `alive.system.load_avg`          | number[3]或null | 1、5、15分钟平均负载                              | **新增**；不是CPU占用百分比                  |
| `alive.tk8710_gw.running`        | boolean或null   | 精确匹配tk8710_gw是否运行                         | **新增**；无进程false、查询失败null          |
| `alive.tk8710_gw.uptime_s`       | integer或null   | 当前tk8710_gw进程运行秒数                         | **新增**；进程重启从0计时；无进程/未知为null |

不增加PID字段。系统采集失败时对应数值为null；总量为0时百分比为null。百分比小数位未固定，NS负责显示舍入。不要把null或unknown转换为“健康的0”。

#### 新增RF射频状态：alive.rfstatus

每次封装心跳从 `/userdata/RFStatus/RFstatus.txt` 重新读取，文件名大小写必须一致：

```ini
rfstatus=11111111
```

| 文件内容                               | 心跳字段                  | 含义                               |
| -------------------------------------- | ------------------------- | ---------------------------------- |
| `rfstatus=11111111`                  | `"rfstatus":"11111111"` | 天线0～7均正常                     |
| `rfstatus=11111110`                  | `"rfstatus":"11111110"` | bit0=0，天线0异常                  |
| `rfstatus=01111111`                  | `"rfstatus":"01111111"` | bit7=0，天线7异常                  |
| `rfstatus=00000000`                  | `"rfstatus":"00000000"` | 8路均异常                          |
| 文件缺失、不可读、不是8位0/1或重复记录 | `"rfstatus":null`       | 状态未知，不等同全部异常或全部正常 |

NS应按字符串处理，保留前导零；天线n取字符串索引 `7-n`，或转换为二进制整数后检查bit n。此字段与GPS连续健康次数 `alive.status` 相互独立。

支持LF、CRLF或无末尾换行。MQTT不修改文件、不主动检测射频、不反转位序。本地周期心跳及IPC心跳封包均包含该字段。建议文件生产程序采用临时文件加rename原子替换；当前协议未含采样时间，因此无法判断一个格式正确但长期未更新的文件是否过期。

实现位置：`src/gateway_telemetry.c:telemetry_rf_status()`读取校验，`src/mac_msg_parser.c:pack_alive()`封包。单独验证RF解析：`build_arm/test_gateway_telemetry --rf-only`，覆盖12种输入情况。

GPS采集程序负责累计status；任一状态查询/位置查询失败或基础状态异常即写0。MQTT只读取转发，不自行累计，也不根据文件mtime判断GPS数据是否过期。

### 13.2 NS发起扫频

向原sub_topic发布：

```json
{"request_id":"scan-001","set":{"scan":{"start_freq_hz":470000000,"end_freq_hz":510000000,"mode":"500k"}}}
```

request_id只允许ASCII字母、数字、下划线和连字符，最多48字符。频率是整数Hz，模式为62.5k/125k/250k/500k。实际硬件验证范围为470-510MHz，其他频率须先确认射频硬件支持，不可把uint32范围等同于射频支持范围。

| 字段                       | JSON类型 | 必填 | 参数约束                                                              | 变更           |
| -------------------------- | -------- | ---- | --------------------------------------------------------------------- | -------------- |
| `request_id`             | string   | 是   | 1到48字符，`A-Z a-z 0-9 _ -`，禁止空格、路径符和NUL；位于JSON根节点 | **新增** |
| `set.scan`               | object   | 是   | 独立扫描任务，不修改set.config协议                                    | **新增** |
| `set.scan.start_freq_hz` | integer  | 是   | 正整数Hz，不能传字符串或带M单位文本                                   | **新增** |
| `set.scan.end_freq_hz`   | integer  | 是   | 不小于start，且不超过4294967295减去步进值                             | **新增** |
| `set.scan.mode`          | string   | 是   | 严格匹配下表，大小写敏感，无默认值                                    | **新增** |

| mode      | 步进Hz | 扫频模式编号 | TK8710速率模式 |
| --------- | -----: | -----------: | -------------: |
| `62.5k` |  62500 |            0 |              5 |
| `125k`  | 125000 |            1 |              6 |
| `250k`  | 250000 |            2 |              7 |
| `500k`  | 500000 |            3 |              8 |

频点数为 `floor((end-start)/step)+1`，范围1到4096；末频点不一定恰等于end。例如无法被step整除时，扫描停在不超过end的最后一个步进点。NS发送Hz整数，与Web输入470M等文本的接口不同。

受理、拒绝与重复请求均在原pub_topic返回scan_ack：

```json
{"scan_ack":{"request_id":"scan-001","state":"accepted"}}
```

新请求可返回accepted/busy/rejected；重复且参数相同返回原任务阶段pending/running/ready/acknowledged，不重扫；同ID不同参数拒绝request_id_conflict。

| scan_ack字段   | 类型           | 说明                                     |
| -------------- | -------------- | ---------------------------------------- |
| `request_id` | string         | 回显请求ID；非法或缺失ID时可能为空字符串 |
| `state`      | string         | 下表中的状态                             |
| `error_code` | string，可省略 | 拒绝/忙的机器可读原因；成功时不输出      |

| state            | 实际含义                                              |
| ---------------- | ----------------------------------------------------- |
| `accepted`     | 请求已经持久化并排队，尚不表示射频启动成功            |
| `pending`      | 重复请求对应任务尚待执行                              |
| `running`      | 重复请求对应任务处于执行/等待完成阶段                 |
| `ready`        | 原任务已有待确认结果，成功或失败需看scan_result.state |
| `acknowledged` | NS已经确认过原任务结果；再次请求不会重扫              |
| `busy`         | 另一个NS任务尚未结束，当前新请求未受理                |
| `rejected`     | 参数、ID、容量或存储错误，当前请求未受理              |

当前scan_ack不含message字段，NS应根据error_code生成显示文字。

### 13.3 完整结果与确认

扫频成功且恢复回调成功后，一次发布包含全部频点和8路噪底的报文：

```json
{"scan_result":{"request_id":"scan-001","state":"completed","start_freq_hz":470000000,"end_freq_hz":510000000,"mode":"500k","point_count":81,"antenna_count":8,"points":[{"freq_hz":470000000,"noise_dbm":[-120,-121,-122,-123,-124,-125,-126,-127]}]}}
```

示例points省略其余80个点；实际发送完整数组。失败报文包含state=failed、error_code和原任务参数。

NS保存结果后，向sub_topic确认：

```json
{"scan_result_ack":{"request_id":"scan-001"}}
```

结果使用QoS1，未收到业务ACK每10秒重发；断网和MQTT进程重启后补发。NS须按request_id去重。Broker确认不替代业务确认。

#### scan_result字段（全部为新增）

| 字段路径                           | JSON类型  | 出现条件/说明                                                |
| ---------------------------------- | --------- | ------------------------------------------------------------ |
| `scan_result.request_id`         | string    | 成功/失败均有，关联原请求                                    |
| `scan_result.state`              | string    | completed或failed                                            |
| `scan_result.start_freq_hz`      | integer   | 成功/失败均有，原起点Hz                                      |
| `scan_result.end_freq_hz`        | integer   | 成功/失败均有，原终点Hz                                      |
| `scan_result.mode`               | string    | 成功/失败均有，原模式                                        |
| `scan_result.point_count`        | integer   | 仅成功，实际频点数，等于points长度                           |
| `scan_result.antenna_count`      | integer   | 仅成功，当前固定8                                            |
| `scan_result.points`             | array     | 仅成功，完整频点数组，按频率升序                             |
| `scan_result.points[].freq_hz`   | integer   | 该频点频率Hz                                                 |
| `scan_result.points[].noise_dbm` | number[8] | 8路原始噪底，单位dBm；按结果文件天线顺序，索引0对应第1路     |
| `scan_result.error_code`         | string    | 仅失败，错误码；失败报文没有points/point_count/antenna_count |

噪底保持浮点值，不取Web显示用的整数均值。当前结果不含message、完成时间戳或单独平均值字段。一次上报指一份完整JSON；网络重试可能重复投递同一份结果。

失败报文示例：

```json
{"scan_result":{"request_id":"scan-001","state":"failed","error_code":"executor_failed","start_freq_hz":470000000,"end_freq_hz":510000000,"mode":"500k"}}
```

业务确认只有一个必填字段 `scan_result_ack.request_id`（string），其字符约束与请求ID一致。成功和失败结果均需确认。未知ID、格式非法的确认会被忽略；确认仅对ready任务有效，不返回额外确认报文。重复确认不会重新执行任务。

#### 当前错误码

| 所属消息    | error_code           | 含义                                                              |
| ----------- | -------------------- | ----------------------------------------------------------------- |
| scan_ack    | invalid_request      | 请求ID或请求结构非法                                              |
| scan_ack    | invalid_frequency    | 起止频率缺失、类型错误、非正数、超出uint32或逆序                  |
| scan_ack    | invalid_mode         | 模式缺失、类型或值非法                                            |
| scan_ack    | scan_too_large       | 超过4096点或末点递增存在整数溢出风险                              |
| scan_ack    | request_id_conflict  | 历史同ID请求与新参数不同                                          |
| scan_ack    | scan_running         | 另一个NS任务未结束，state=busy                                    |
| scan_ack    | history_full         | 已达到32份历史记录上限                                            |
| scan_ack    | request_id_reserved  | 已存在执行端同ID状态，拒绝复用旧结果                              |
| scan_ack    | no_memory            | 不能分配任务记录                                                  |
| scan_ack    | storage_failed       | 请求持久化失败                                                    |
| scan_result | executor_busy        | 请求排队后发现本地执行端忙，例如Web正在扫频                       |
| scan_result | executor_unavailable | socket连接/应答失败，或执行端未返回OK/BUSY                        |
| scan_result | executor_failed      | 执行端报告failed，包含扫描失败或业务恢复失败                      |
| scan_result | interrupted          | MQTT恢复任务时找不到匹配的执行端状态                              |
| scan_result | timeout              | MQTT观察任务超过600秒仍未完成                                     |
| scan_result | invalid_result       | 文件缺失、为空、超限，或频点/数量/8路噪底不完整、不匹配、非有限值 |
| scan_result | result_too_large     | 封包后的完整JSON超过1MiB                                          |

执行端状态文件中的scan_start_failed、scan_failed_or_timeout、restore_failed、result_snapshot_failed等详细message目前不会逐字透传给NS；NS收到executor_failed时应结合执行端状态/日志定位。完全不可解析JSON或消息超出MQTT入口长度限制可能直接被丢弃，不保证一定收到scan_ack。

### 13.4 本地任务接口与恢复

MQTT复用Web已有/tmp/data_collect.sock，发送扩展START命令，未改共享内存无线配置/数据结构体。tk8710_gw注册StartManagedScan/RestoreManagedScan；NS配置、Web和CLI扫频协调到同一配置锁/任务入口。恢复使用已成功应用的NS配置，恢复HAL回调和GPS/PPS同步；没有已应用NS配置时拒绝启动托管扫频。

工作线程最长540秒，MQTT任务观察最长600秒。执行结束后先保存快照、恢复业务再发布完成状态；恢复失败返回failed。正常进程退出时等待扫描工作线程收敛。

持久化目录/userdata/mqtt_scan：

| 文件         | 内容                           |
| ------------ | ------------------------------ |
| job-ID.json  | MQTT请求、阶段、完整待确认结果 |
| exec-ID.json | 执行端状态，含ID与扫描参数     |
| exec-ID.txt  | 本次扫描不可变结果快照         |

历史记录上限32份；达到上限返回history_full。未实现自动归档，应制定运维清理周期；只能在停用扫描服务并确认NS已保存/确认后归档已确认任务，清理会使对应ID失去历史去重依据。每条结果上限1MiB；最多4096点。不要删除未确认记录。

### 13.5 构建和验证

MQTT：WSL进入本目录执行 `bash build_sdk.sh`，产物build_arm/mqtt_bridge。该脚本不会调用旧Makefile中的自动SCP。

TK8710：进入tk8710_hal_v1.6，加载交叉编译环境后执行 `bash cmake/build_rk3506.sh tk8710_gw`；此既有脚本会重建build_rk3506，需先保留所需旧产物。

板端验收脚本：tests/board_scan_e2e.py，参数--host、--gateway、--mqtt、--output，增加--extended验证独立Broker断网/重连、MQTT重启补发、ACK停止重发和Web再扫频。脚本临时暂停守护、替换运行时MQTT配置；结束恢复配置/原实例，新业务二进制只上传测试目录。

源文件：src/ns_scan_protocol.c负责协议与严格结果解析，src/ns_scan_manager.c负责持久化和状态机；TK8710侧为src/tk8710_scan_ipc_server.c及test/example/tk8710_gw.c。
