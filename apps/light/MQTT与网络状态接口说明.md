# MQTT与网络状态接口实现说明

日期：2026-10-08。用于后续外接双色灯统一仲裁，也可供Web读取。本次未修改NS报文或切换LED仲裁程序。

## 1. 文件与兼容性

| 文件 | 生产者 | 本次行为 |
|---|---|---|
| `/run/led/mqtt_status.json` | mqtt_bridge | 新增，真实连接及订阅就绪状态 |
| `/run/led/network_status.json` | tdaemon及其观察子进程 | 新增，链路/检测结果/发布者进度 |
| `/run/led/mqtt.state` | mqtt_bridge | 保持原格式和更新方式，供旧led_agent使用 |
| `/run/led/net.state` | tdaemon | 保持原格式和更新方式，供旧led_agent使用 |

新灯控应只消费新JSON，不把旧mqtt.state中的connected当作真实broker连接确认。保留旧逻辑意味着此次升级后旧灯语不会自动变成新方案。
新增状态不包含账号、密码、Topic或NS载荷；pid/start_ticks只用于本地核查，不新增到NS心跳。

## 2. MQTT接口

实现文件：`mqtt/src/mqtt_status.c`、`mqtt/inc/mqtt_status.h`，在`mqtt/src/main.c`中接入。
JSON schema=1，clock=`monotonic`；示例：

```json
{
  "schema": 1,
  "pid": 1234,
  "start_ticks": 12345,
  "boot_id": "01234567-89ab-cdef-0123-456789abcdef",
  "clock": "monotonic",
  "state": "connected",
  "reason": "subscription_confirmed",
  "error_code": 0,
  "connack_ok": true,
  "subscribed": true,
  "ready": true,
  "exited": false,
  "pending_mid": -1,
  "state_since_mono_ms": 100000,
  "updated_mono_ms": 101000
}
```

| 状态 | 条件 |
|---|---|
| connecting | 开始连接，或CONNACK成功但尚未收到订阅成功确认 |
| connected | CONNACK成功，且当前SUBSCRIBE对应MID的SUBACK授予QoS 0/1/2 |
| reconnecting | 断连回调，ready立即取消；等待现有Mosquitto重连 |
| fault | broker拒绝、订阅调用/授予失败、连接/SUBACK等待超过10秒，或本进程初始化/运行故障 |
| stopped | 正常退出，exited=true、ready=false；错误退出保留fault原因 |

实际下行仅订阅一个Topic，因此SUBACK必须恰好一个有效授予值。旧MID、不匹配MID、断开后到达的ACK不会恢复ready。
连接类fault可被后续成功CONNACK/SUBACK恢复；进程级显式fault锁存至退出，迟到回调不能覆盖。
新接口的10秒期限只改变显示状态，不退出程序、不修改Mosquitto重连或NS重试策略。
主循环每轮调用tick，但文件最多每秒刷新一次。tick不会无条件写connected；发送成功也不代表NS已实际接收。
写入采用互斥锁、同目录临时文件及原子rename，status.lock避免多个进程争写此快照。发布失败只告警，不中断原业务。
异常退出或SIGKILL无法写stopped；消费者必须校验进程身份，不能仅看文件里的ready。

## 3. 网络接口

实现文件：`tdaemon/tdaemon_status.sh`；在`tdaemon.sh`启动，在`tdaemon_net.sh`的检测、候选准备、切换和最终结果处通知。
观察子进程不执行ping、ifup、路由切换或GPIO操作，仅每约1秒读取状态文件、当前接口carrier和IPv4地址。

### 时钟说明

BusyBox shell从`/proc/uptime`读取单调开机时间，包含挂起时间，对应CLOCK_BOOTTIME。
因此网络JSON使用`clock="boottime"`及`*_boottime_ms`字段，不冒充CLOCK_MONOTONIC。
MQTT和TK8710仍为CLOCK_MONOTONIC。消费者应按每份快照的clock取当前时间，不能把两种绝对时间直接相减；均不受NTP墙钟调整影响。

主要字段：schema、pid、start_ticks、boot_id、clock、state、reason、ready、active_iface、allowed_ifaces、link_up、ipv4_ready、probe_state、probe_iface，以及以下时间：

| 时间字段 | 含义 |
|---|---|
| updated_boottime_ms | 观察器本次刷新时间 |
| checked_boottime_ms | 最近一次正式up/down结果的完成时间，观察器不会替它续期 |
| publisher_progress_boottime_ms | tdaemon主循环或网络操作通知的最近进度 |
| probe_since_boottime_ms | 当前探测/切换操作开始时间 |

`pid/start_ticks`属于tdaemon父进程，不是观察子进程；消费者必须核对该身份及时间有效性。

### 状态判断

- `up`：最近一次正式检测通过，结果不超过45秒；当前接口仍允许使用、carrier存在且有IPv4。
- `checking`：尚待检测或拔线重插后等待新检测。周期检查期间可继续保留45秒内的已验证up/down，同时probe_state显示checking，不会每次ping都制造灯语闪烁。
- `switching`：已有恢复/切换通知，或观察器发现当前接口物理断开、IPv4丢失、被配置禁用。
- `down`：原有网络选择逻辑已确认没有达标接口，结果仍有效；不是观察器仅凭eth0掉线就认定全网断开。
- `unknown`：启动未确认、维护中、结果/主流程进度超过45秒、检测/切换超过60秒，或时间异常。停机通知对应unknown/publisher_stopped。

判为up要求link_up和ipv4_ready同时成立，两者分别记录。字段针对当前最近验证的活跃接口，不是所有网卡的状态。
拔网线而4G已成为经过检测的活跃接口时，active_iface=usb0仍可为up；观察器不自行选择备用路由。
观察到物理失效后，即使重插后carrier和IP恢复，也不得复用失效前的up结果，必须等新的正式检测。失败恢复通知同样会作废旧up结果。
观察子进程持续写文件不能掩盖父进程卡死：主流程进度超45秒即unknown。原有慢恢复耗时超过预算时暂时显示unknown是有意的保守行为，实际预算还需板端测试校准。

### 生命周期

- 获得tdaemon原有单实例锁、加载配置后启动观察器。
- 修改禁用接口配置会通过下一轮主流程进度通知更新allowed_ifaces。
- 子进程每秒检查父进程PID和starttime，以及network_status.owner归属；父进程退出或被新实例替代后停止，不会长时间遗留。
- 不独立自动拉起观察器；若观察器退出，消费者用快照过期识别故障。
- 内部控制文件位于`/run/led/network-observer.*`，消费者只读network_status.json，不解析内部文件。
- tdaemon继续按原有周期探测、恢复和切换；本次改善的是状态观察时效，不承诺实际切网也在1秒内完成。

## 4. 消费者检查建议

先校验schema和clock，再校验boot_id、PID/start_ticks及进程非僵尸。MQTT检查进程名mqtt_bridge；网络父进程为tdaemon shell，应按完整脚本argv核对。
MQTT和network观察器正常约每秒更新，可用5秒新鲜度作为初始建议；TK8710有10秒正常循环，应遵循其独立45秒建议，不能共用5秒阈值。
任何身份不符或快照过期都不能绿常亮；网络unknown不等于已确认全网down。实际灯语仍按`双色状态灯详细设计.md`的仲裁优先级执行。

## 5. 构建、同步及验证

SDK `external/rk3506app`同步MQTT新增源码/头文件和三个tdaemon脚本。MQTT Makefile及SDK入口均按src/*.c收集文件，无需硬编码新C文件列表。
SDK build_apps.sh --stage新增安装tdaemon_net.sh与tdaemon_status.sh，避免新启动脚本找不到依赖。新版mqtt_bridge和守护脚本也更新固件预置目录，完整升级包仍需重新制作。
备份：本地auto_deployment/backup/status_interfaces_20261008_170839。

隔离验证：

```sh
python3 mqtt/tests/test_connection_status.py
python3 tdaemon/test_network_status.py
```

MQTT：15项ARM/QEMU测试，覆盖CONNACK/SUBACK成功与拒绝、错误MID、断连、重连、超时、停机及身份。因QEMU模拟的proc/stat启动时间为0，测试对此项使用夹具，生产仍读取真实proc。
网络：17项BusyBox ash测试，覆盖carrier/IP丢失、重插重验、4G活跃、禁用接口、慢探测、旧结果/父进程过期、嵌套观察进程结束；真实net_tick在隔离环境验证新JSON通知和旧net.state同时更新。
新版MQTT完成ARM交叉编译，存在原有strncpy截断告警。本轮没有在实板制造网络断开、验证broker实际拒绝或切换LED，隔离测试不等于板端全链路验收。
