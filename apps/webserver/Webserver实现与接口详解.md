# RK3506 Webserver实现与接口详解

2026-09-22：`/api/system_info`新增`watchdog`对象，状态页新增“片内看门狗”，只读`/run/rk-watchdog/status.json`并验证启动标识、进程及新鲜度。详情及未进行的板端复位验证见[独立服务说明](../watchdog/README.md)。

2026-09-15增补：已实现并在192.168.100.227验证分类日志导出，详见[分类日志导出使用与接口说明](分类日志导出使用与接口说明.md)。本次没有新增TK8710控制台日志保存。升级功能的最新实现和板端结果以[SWU升级流程与制作说明](SWU升级流程与制作说明.md)为准；下文2026-09-11静态审阅结论属于当时版本。

分析日期：2026-09-11。依据本目录当前源码，本文为静态实现梳理，未启动服务、修改板端配置或执行升级。源码路径均相对于本目录；行号可能随修改移动，优先按函数名查找。

## 状态页扩展（2026-09-11更新）

### TK8710版本文件

tk8710_gw每次正常启动、工作目录就绪后发布 `/var/run/tk8710_gw/version.txt`，版本来自 `TK8710_GW_VERSION_STRING`，与启动横幅一致。目前为 `1.0.0 (RK3506)`。

```ini
version=1.0.0 (RK3506)
pid=123
start_ticks=5000
```

version为展示值；pid和start_ticks只供消费者验证该记录是否属于当前运行进程，不返回给浏览器。写入使用进程专属临时文件、fflush/fsync、rename原子替换，读取方不会看到半写文件。目录为运行时状态目录，重启后重新生成，不依赖程序工作目录或userdata持久化数据。

Web新增 `/api/system_info` 的 `tk8710_gw.version`（string或null），校验/proc中进程名、启动时刻及非僵尸状态后返回版本；文件缺失、进程退出、PID重用或格式错误返回null，状态页显示“未知”。版本在进程启动时发布不代表业务初始化成功，仍需结合运行状态和日志。

其他本机应用可以读取相同文件并使用同一身份校验规则；当前MQTT未新增版本上报字段。本次新增读取模块 `src/gateway_version.c` 和头文件 `inc/gateway_version.h`，页面在TK8710运行状态下面增加TK8710版本。

验证：Web/网关ARM构建通过；`tools/test_gateway_version.c`以-Wall -Wextra -Werror交叉编译并在QEMU运行，正确版本、过期启动时刻、进程不存在、缺字段、非法PID和缺文件6类测试通过。本次尚未在板端部署或进行浏览器视觉验收。

坐标已改为 `/userdata/GPS/GPSinfo.txt`，读取lon/lat；下文旧实现章节里的/var/run/bdgps描述已被本节替代。

`/api/system_info` 新增：

| 字段 | 类型 | 来源及含义 |
|---|---|---|
| tk8710_gw.running | boolean/null | /proc精确匹配进程；未运行false，查询失败null |
| tk8710_gw.uptime_s | integer/null | 当前网关进程运行秒数；不包含PID |
| rfstatus | string/null | /userdata/RFStatus/RFstatus.txt的8位0/1，天线7到0 |
| gps_enabled | boolean/null | 当前网关写出的NS GPS启用策略；未收到策略/旧版本/进程不存在为null |

页面增加进程状态、进程时长、天线0～7分别正常/异常标签，以及GPS已开启/已关闭/未知。每3秒随系统信息轮询刷新；请求失败清除新增字段的旧显示。网关不运行时RF标签附注“最后记录”，不把持久化文件视为实时健康检测。

Web复用mqtt/src/gateway_telemetry.c的进程和RF解析，Makefile增加对应对象及头路径。GPS启用信息来自 `/var/run/tk8710_gps_policy`，内容为内部PID、0/1、进程starttime滴答数；Web核对进程名与启动时刻后返回布尔值，内部PID不发给浏览器。GPS开关是NS策略，不代表GPS当前健康、PPS已锁定或硬件电源开关。

配套tk8710_gw.c在ApplyNsConfig接收有效策略时原子rename写此文件。需更新Web和配套网关程序才能显示GPS启用状态；旧网关没有文件时正确显示未知。

验证：Web和tk8710_gw的ARM构建通过；Node测试通过页面脚本语法、3661秒格式化、GPS关闭/未知及天线0和7异常位序。此次未部署到板端，未完成实际浏览器视觉验收。

## 1. 程序定位与架构

`web_server` 是网关本机HTTP管理程序，使用C、Mongoose、JSON-C和pthread。它提供页面、HTTP API、配置文件读写和本地任务控制；不直接实现MQTT Broker，也不直接操作TK8710 SPI。

```text
浏览器
  -> HTTP :8090 -> Mongoose event_handler
       -> 页面：内嵌HTML/CSS/JavaScript
       -> 系统：sysinfo/statvfs/proc
       -> 配置：config.json/interfaces/wpa_supplicant.conf
       -> 扫频：Unix socket -> tk8710_gw -> 状态/结果文件
       -> ACM：Unix socket -> 外部ACM服务 -> 状态/结果文件
       -> 升级：上传image.swu -> swupdate -> reboot
       -> 日志：目录列表/文件下载
```

## 2. 源文件与入口

| 文件 | 主要职责 |
|---|---|
| `src/main.c` | 版本参数、信号处理、启动HTTP监听 |
| `src/web_server.c` | 所有HTTP路由、页面拼装、扫频/ACM、统计、上传、网络质量线程 |
| `src/config_manager.c` | 网关JSON配置及tdaemon禁用接口同步 |
| `src/network_manager.c` | eth0配置、Wi-Fi配置、Ping |
| `src/system_manager.c` | 系统信息、RAM、磁盘、坐标 |
| `src/ipc_command_client.c` | AF_UNIX SOCK_STREAM命令客户端 |
| `src/upgrade_manager.c` | 后台执行swupdate及升级状态 |
| `inc/*.h` | 模块接口和结构定义 |
| `web_resources/html/`、`web_resources/css/` | 页面源码及样式，JavaScript包含在页面中 |
| `tools/gen_resources.py` | 生成 `inc/web_resources.h` |
| `../common/src/file_logger.c` | 共享文件日志实现 |

启动链：`main()` → `web_server_start("http://0.0.0.0:8090")` → `config_load()` → 启动网络质量线程 → `mg_mgr_init()` → `mg_http_listen()` → 无限循环 `mg_mgr_poll(&mgr,100)`。

入口支持 `-ver`、`--version`，当前硬编码版本V1.0.1；它与页面配置中的app_version不必相同。信号处理直接exit(0)，正常循环后的mg_mgr_free通常不可达。

## 3. HTTP路由全集

除/login、/logout外，均先经is_authed检查。下表GET表示页面实际使用的读取方式，不代表所有读取处理器都严格限制GET；只有部分写接口显式检查POST并返回405。

| URL | 使用方法/入参 | 处理函数 | 功能/下层 |
|---|---|---|---|
| `/login` | GET页面；POST username,password | handle_login | admin用户名与配置web_password比较 |
| `/logout` | 页面访问 | event_handler | 清Cookie，跳转/login |
| `/status` | GET | handle_status | 系统/网络总览 |
| `/config` | GET/POST表单 | handle_config | MQTT、网关ID、网络使能等配置 |
| `/manger` | GET/POST表单 | handle_manger | 隐藏管理/改密码；路径拼写确为manger |
| `/network` | GET/POST表单 | handle_network | 网口、Wi-Fi、Ping |
| `/rf` | GET | handle_rf | 射频操作/扫描页面 |
| `/upgrade` | GET；POST action=reboot | handle_upgrade | 升级页或设备重启 |
| `/api/active_ip` | GET | handle_active_ip_api | 活跃接口及IPv4/IPv6/网关 |
| `/api/system_info` | GET | handle_api_system_info | 系统、内存、磁盘JSON |
| `/api/traffic` | GET | handle_api_traffic | /proc/net/dev累计流量 |
| `/api/network_quality` | GET | handle_api_network_quality | 后台Ping结果缓存 |
| `/api/traffic_stats` | GET | handle_api_traffic_stats | /etc/turbotraffic统计 |
| `/api/traffic_stats/reset` | POST，query iface | handle_api_traffic_stats_reset | 清指定eth0/usb0/wlan0统计 |
| `/api/rf_stats` | GET | handle_api_rf_stats | MQTT上/下行计数相对基线 |
| `/api/rf_stats/reset` | POST | handle_api_rf_stats_reset | 保存当前计数为清零基线 |
| `/api/rf_scan/start` | POST表单freq_start,freq_stop,mode | handle_api_rf_scan_start | 本地socket发起扫频 |
| `/api/rf_scan/status` | GET | handle_api_rf_scan_status | 读扫频状态JSON |
| `/api/rf_scan/result` | GET | handle_api_rf_scan_result | 读8路噪底文本，返回均值曲线 |
| `/api/rf_acm/start` | POST | handle_api_rf_acm_start | 本地socket发送START acm |
| `/api/rf_acm/status` | GET | handle_api_rf_acm_status | 读ACM状态JSON |
| `/api/rf_acm/result` | GET | handle_api_rf_acm_result | 读ACM结果success/channels |
| `/api/logs` | GET，query types | log_export_list | 按七类白名单枚举日志、大小及类别可用情况 |
| `/api/log_export/start` | POST JSON types,files | log_export_start | 校验所选文件并启动后台打包，202返回id |
| `/api/log_export/status` | GET，query id | log_export_status | 查询准备/压缩/完成/失败状态 |
| `/api/log_export/download` | GET，query id | log_export_download | 分段下载单个tar.gz日志包 |
| `/api/log_download` | 旧接口 | HTTP410 | 提示刷新页面使用分类打包导出 |
| `/upload` | POST二进制块；query file,offset | handle_upload | 写/tmp/image.swu，返回下一偏移 |
| `/upload/start` | POST | handle_upgrade_start | 检查上传文件并调用upgrade_start |
| `/api/upgrade_status` | GET | handle_upgrade_status | 返回idle/installing/done/error及message |

已认证未知路径跳转/status，未认证跳转/login。API不是统一返回结构：有的直接返回字段对象，有的使用status/data包装，有的返回文本，调用者需逐接口处理。

## 4. 登录与权限

handle_login使用固定用户名admin，密码来自启动时加载的app_config.web_password。成功返回302到/status，设置 `session=admin; Path=/; HttpOnly`；失败返回HTTP 200的Login Failed文本。

当前is_authed仅判断Cookie头是否包含固定子串session=admin，没有随机会话、签名、服务端会话表或到期校验。这不是可靠的身份认证，不能据“有登录页”认为升级/配置接口受到安全保护。代码没有TLS监听和CSRF令牌；生产访问边界需要加固。本文记录现状，不更改认证代码。

## 5. 配置管理

### 5.1 网关/MQTT配置

`/config` POST通过mg_http_get_var读取表单，数字用atoi转换：

| 字段 | 用途 |
|---|---|
| mqtt_host,mqtt_port | Broker地址和端口 |
| username,password | MQTT账号密码，区别于Web登录密码 |
| gateway_id | 网关身份 |
| pub_topic,sub_topic | MQTT上下行Topic |
| heartbeat_interval,qos | 心跳周期和QoS |
| antenna_count | 配置中的天线数量 |
| net_eth,net_wifi,net_4g | 勾选允许使用的网络 |

`config_save()`写 `/etc/config.json`；`config_sync_tdaemon_disabled()`把未选接口转换为eth0/wlan0/usb0列表，更新 `/etc/tdaemon.conf` 的disabled。两步不是原子事务：前一步成功后后一步可能失败。

config_load还读取 `/etc/device_version.json` 的版本信息。config_save按AppConfig字段重新创建JSON，未知扩展字段可能丢失，未来扩展前应检查。MQTT进程是否立即重读配置由MQTT实现决定，Web保存不等于该进程已应用；当前页面提示重启生效。

### 5.2 网口

`network_load_config()`读取 `/etc/network/interfaces` 的eth0配置；DHCP时结合运行时IP与默认路由显示。POST字段proto、ip、mask、gateway交给network_save_config；函数以w方式重写interfaces，不能假定会保留所有自定义网卡段。保存成功后调度约6秒后reboot。

### 5.3 Wi-Fi

同一个/network POST以wifi_action分支处理wifi_ssid和wifi_psk。wifi_save_config检查SSID非空、密码8～63字符及引号/换行，写临时文件、设0600、fflush/fsync、rename到 `/etc/wpa_supplicant.conf`。

这里仅保存配置，没有启动驱动或切换STA；实际连接依赖系统wifibt和tdaemon。wifi_is_connected只检查wlan0是否有inet地址，因此AP的静态IP也可能被误标为Wi-Fi已连接，不能用它证明关联外部路由器成功。

### 5.4 改密码和Ping

/manger表单：manger_action=password、current_password、new_password、confirm_password。验证旧密码和两次新密码后config_save。
/network表单含ping_target时调用network_ping，内部popen执行ping。此手工Ping路径与网络质量线程的目标校验不是同一函数，需要独立检查输入安全和阻塞时间。

## 6. 系统信息和统计

### 6.1 系统采集

`system_get_info()`使用以下来源：

| 信息 | 来源与口径 |
|---|---|
| hostname/model/arch/platform、版本 | config_load读取的配置/版本文件；部分是配置描述，不是uname实时探测 |
| local_time | time/localtime/strftime |
| uptime | sysinfo.uptime，格式化为h m s，表示开机时长 |
| load_avg | sysinfo.loads/65536，1/5/15分钟平均负载 |
| mem_total | totalram × mem_unit |
| mem_used | total-free，包含缓存 |
| disk_total/disk_used | statvfs(/userdata)，按f_blocks/f_bfree计算 |
| device_coords | **仍读取/var/run/bdgps**中的lon/lat，拼成字符串 |

/api/system_info返回hostname、model、arch、device_coords、firmware_version、kernel_version、local_time、uptime、load_avg、app_version、disk_percent/disk_used/disk_total、mem_percent/mem_used/mem_total。HTTP中的used/total除以1024²，单位MiB；百分比取整数。与MQTT新接口的字节单位和数值uptime_s不同。

### 6.2 流量和网络质量

/api/traffic从/proc/net/dev读取累计字节，前端用连续采样差值计算速率；不能把累计字节直接当B/s。
/api/traffic_stats直接读取/proc/net/dev中eth0、usb0、wlan0的RX/TX累计字节，再减去/etc/turbotraffic保存的用户清零基线；不再依赖S39turbotraffic。基线缺失或损坏时显示本次开机累计值；网卡重建/重启导致内核计数小于旧基线时，该方向按新计数重新累计。reset只允许上述三个接口，原子更新对应基线，不修改内核网卡计数。
/api/network_quality不在HTTP请求内直接Ping，而是读取后台线程的互斥锁缓存。目标是app_config.mqtt_host，校验目标字符后popen ping解析丢包、RTT和抖动。返回ok、target、status、avg_latency_ms、jitter_ms、loss_percent；缓存尚未就绪时pending、ok=false。

### 6.3 射频收发统计

/api/rf_stats读取mqtt_bridge写出的 `/var/run/mqtt_stats.json`，结合 `/var/run/mqtt_stats_baseline.json` 计算展示差值。清零只是更新基线，不清空MQTT IPC、不复位芯片、不改变MQTT原始累计计数。

## 7. TK8710扫频接口

### 7.1 启动

浏览器POST表单示例：

```text
/api/rf_scan/start
freq_start=470M&freq_stop=510M&mode=500k
```

后端检查起止频率格式和顺序，支持62.5k、125k、250k、500k；后端缺省mode目前仍是125k，不能把前端页面默认500k当成后端默认值。

调用链：

```text
handle_api_rf_scan_start
 -> normalize_rf_scan_freq / parse_rf_scan_mode
 -> ipc_client_start_task_with_args
 -> /tmp/data_collect.sock (AF_UNIX SOCK_STREAM)
 -> START freq_start=470M freq_stop=510M mode=500k\n
 -> tk8710_gw扫描服务
```

服务答OK：HTTP 200、status=ok、state=running；BUSY：HTTP 200、status=busy；本地通信失败502；参数错误400；其他执行端答复500。启动受理不等于扫描完成。

### 7.2 状态与结果

状态读 `/tmp/data_collect_status.json` 的state/message/result_ready；文件缺失或非法时返回idle/result_ready=false。即使新执行端写了request_id等字段，当前Web只提取这三项。

结果读 `/userdata/SweepFreqResult/Result.txt`：每条记录一个频率Hz加8个浮点噪底，允许跨行。每个点用8路算术平均后按 `(int)(mean - 0.5)` 转整数作为rssi，频率格式化为M字符串。

```json
{"status":"ok","data":{"success":true,"duration":0,"freq_start":"470M","freq_stop":"510M","points":[{"freq":"470M","rssi":-120}],"count":1}}
```

示例仅演示字段，duration当前硬编码0。HTTP结果不保留8路原始数组。末尾半条记录会被忽略；结果读取函数本身不检查任务ID/完成状态，因此旧文件或未完成前的部分文件也可能被返回。前端应先轮询完成状态；严格任务一致性以新版MQTT的ID快照协议为准。

Web不直接写TRM寄存器，不参与噪底算法，也不负责GPS/业务配置恢复；这些是tk8710_gw执行端职责。

## 8. ACM接口

POST `/api/rf_acm/start`通过ipc_client_send_command向 `/tmp/acm.sock` 发 `START acm\n`，OK/BUSY/通信错误映射与扫频类似。

状态从 `/tmp/acm_status.json` 读state/message/result_ready；结果从 `/tmp/acm_result.json` 读取success及channels数组并封装给页面。

Web实现这些客户端接口不代表板上一定存在ACM服务。需单独确认socket监听者、执行服务版本和结果文件生产者；本次未做ACM硬件校准验证。

## 9. 本地IPC客户端细节

inc/ipc_command_client.h定义默认超时1000ms、命令最长128字符、响应缓存256字节。

流程：socket(AF_UNIX,SOCK_STREAM) → connect → select等待可写 → send命令加LF → select等待可读 → recv一次 → 去掉尾部CR/LF → close。

接口包括ipc_client_send_command、ipc_client_ping(PING)、ipc_client_start_task(START)、ipc_client_start_task_with_args(START加参数)。

当前实现没有循环保证全部send或按换行收全响应；connect为阻塞调用，1000ms不是严格覆盖connect的总期限。HTTP主线程同步等待IPC，因此服务端慢响应会短时拖慢页面请求。新版TK8710服务端支持更长命令，并不自动改变此客户端128字符限制。

## 10. 升级与重启

### 10.1 分块上传

POST `/upload?file=image.swu&offset=0`，body为原始二进制块。file必须匹配固定允许名称，offset为非负整数；write_upload_chunk负责写入固定 `/tmp/image.swu` 并返回下一偏移的纯文本。不是把任意用户文件路径传给shell，也不是直接烧写update.img。

### 10.2 开始安装

POST `/upload/start`要求文件是非空普通文件，然后upgrade_start启动pthread执行：

```sh
swupdate -i /tmp/image.swu -e stable,copy1 > /tmp/swupdate.log 2>&1
```

线程成功时标记done并3秒后重启；失败标记error并取日志末尾最多128字节作为message。HTTP `/api/upgrade_status`返回status和message。

这是SWUpdate的.swu升级路径，和PC端Rockchip完整update.img烧录是不同格式和流程。签名验证/分区选择/脚本执行取决于SWUpdate构建与sw-description，Web检查非空文件不能证明包可信或完整。

### 10.3 单独重启

POST `/upgrade`，action=reboot：先返回提示，再约1秒后执行reboot。网络保存成功则约6秒后重启。不能在仅检查接口时随意调用这些写接口。

## 11. 文件与外部依赖清单

| 路径/接口 | Web操作 | 对端/用途 |
|---|---|---|
| /etc/config.json | 读写 | MQTT及网关配置 |
| /etc/device_version.json | 读 | 版本元信息 |
| /etc/tdaemon.conf | 修改disabled | 网络守护策略 |
| /etc/network/interfaces | 读写 | 网口配置 |
| /etc/wpa_supplicant.conf | 读写 | STA配置 |
| /var/run/bdgps | 读 | 旧坐标文件 |
| /proc/net/route、/proc/net/ipv6_route | 读 | 活跃路由 |
| /proc/net/dev | 读 | 流量累计 |
| /etc/turbotraffic | 读写 | 持久化流量统计 |
| /var/run/mqtt_stats.json | 读 | MQTT业务统计 |
| /var/run/mqtt_stats_baseline.json | 读写 | 展示清零基线 |
| /tmp/data_collect.sock | 发送命令 | TK8710扫频服务 |
| /tmp/data_collect_status.json | 读 | 扫频状态 |
| /userdata/SweepFreqResult/Result.txt | 读 | 扫频原始8路数据 |
| /tmp/acm.sock | 发送命令 | ACM执行服务 |
| /tmp/acm_status.json、/tmp/acm_result.json | 读 | ACM状态及结果 |
| /userdata/logs | 枚举/下载 | 日志文件 |
| /var/log/web_server.log | 写 | Web自身共享日志 |
| /tmp/image.swu | 写/读取检查 | 上传包 |
| /tmp/swupdate.log | 读 | 安装错误详情 |

系统调用：sysinfo、statvfs、socket/ioctl、文件IO、pthread、select；网络地址与路由结合ioctl和proc文件查询。外部命令主要为ping、ifconfig、swupdate和reboot。Web没有直接mosquitto_publish，也没有新增NS配置gps_enable的发送入口。

## 12. 与最新MQTT功能的边界

1. Web仍读/var/run/bdgps，MQTT已读/userdata/GPS/GPSinfo.txt；Web未展示新GPS连续健康status。
2. Web当前未读取/userdata/RFStatus/RFstatus.txt，不能假定已有8路RF状态展示。
3. Web系统uptime是格式化开机时长；没有MQTT新增的tk8710_gw进程运行秒数字段。
4. Web扫频返回平均噪底曲线；MQTT返回带request_id的8路原始噪底完整结果。
5. Web网络勾选控制ETH/Wi-Fi/4G，不是NS的gps_enable。GPS严格启用策略由NS配置及tk8710_gw执行。
6. 两者共享扫频Unix服务，但Web不使用MQTT scan_result_ack，不负责NS重发队列。

## 13. 构建与二次开发定位

当前Makefile支持ARCH、TOOLCHAIN_ROOT、SYSROOT、DEPS_ROOT。ARM默认工具链根为 `$HOME/arm-buildroot-linux-gnueabihf_sdk-buildroot`，默认产物build_arm/web_server；ARCH=x86使用gcc，但仍需对应依赖。

在已经准备好依赖的WSL目录执行：

```sh
make -j4 ARCH=arm TOOLCHAIN_ROOT=/home/hjh/arm-buildroot-linux-gnueabihf_sdk-buildroot
file build_arm/web_server
```

此为源码构建方法，本次未实际重编译。链接JSON-C、Mongoose和Makefile列出的系统库，共享日志源也要编译。不要只复制webserver忽略common与third_party依赖。

页面改web_resources中的HTML/CSS，再用tools/gen_resources.py生成资源头；不应只改自动生成的inc/web_resources.h。新增API需要同时修改event_handler路由、处理函数和页面请求脚本。

推荐阅读顺序：main.c → web_server_start/event_handler → 目标handle函数 → 对应manager → 文件或socket的生产者。故障定位时先判断HTTP鉴权/路由、文件是否存在、服务是否监听，最后才进入TK8710驱动层。

## 14. 已识别限制及验证边界

- 固定Cookie鉴权可伪造，需要替换为真实会话；配置、上传和重启均受影响。
- 多处表单数字使用atoi和宽松字符串复制，缺少完整范围校验；部分JSON由snprintf拼接，不能假设全部字段已转义。
- upgrade线程状态与HTTP读取之间未见互斥锁保护；上传与安装并发、包完整性和容量限制需专门验证。
- config_save重写字段集合；interfaces保存也会重写，需关注未知字段和自定义网卡段丢失。
- Wi-Fi“已连接”仅由IP判断；GPS路径与MQTT不同；射频结果读取不绑定任务ID。
- 网络质量缓存有锁，但线程读取app_config与HTTP修改配置之间需继续审查同步。
- 当前日志仅作源码审阅，不证明板上依赖服务存在或升级可用。本次未执行任何写接口、刷机或部署。

这些是从实现读出的开发关注点，不是本次已修复项。
