# RK3506片内看门狗独立服务

2026-09-22。板型：HD-RK3506B-IOT-EMMC。只使用片内wdt0，不使用片外看门狗。

后续板端验证更新：已在用户烧录的192.168.100.239完成两轮断喂复位及业务恢复，实际超时读回为89秒（申请60秒）。详见[板端验证报告](板端看门狗验证_192.168.100.239_20260922.md)。下文“未烧录/未实板验证”为实现阶段记录，当前验收范围以该报告为准。

## 行为约定

### 喂狗进程保护（2026-09-22新增）

- 获取单实例锁后、打开硬件前，设置并读回 `oom_score_adj=-900`，降低内存不足时被选中终止的概率；不是绝对免杀。
- 设置并读回 `nice=-10`，保持普通调度策略，不使用实时调度、不绑核。喂狗仍每5秒一次，不提高业务线程优先级。
- 设置失败记录 `warning_oom_protection_failed` 或 `warning_priority_protection_failed`，继续正常初始化和喂狗；状态文件的 `oom_protected`、`priority_protected` 分别记录是否成功。
- S99停止流程排除 `rk_wdt` 和 `S15rk-watchdog` 配置项，扫描进程时按真实可执行文件/comm保护喂狗进程；PID文件中的进程必须匹配tdaemon脚本身份才发送停止信号。
- 当前三App SWU只停止指定业务程序，独立喂狗继续运行。不要把 `rk_wdt` 加入tdaemon的proc列表或升级包停止名单；禁止运维使用全局 `killall`、按目录批量杀进程等操作。
- 这些措施不能阻止root明确执行SIGKILL/SIGSTOP，也不屏蔽硬件故障复位；不增加自动拉起或实时调度，保留断喂复位机制。

查看实际保护值（板端）：

```sh
pid=$(pidof rk_wdt)
cat /proc/$pid/oom_score_adj
ps -o pid,comm,ni -p "$pid"  # 若板端BusyBox不支持该格式，查看/proc/$pid/stat第19字段
cat /run/rk-watchdog/status.json
```

本轮新增保护经隔离故障测试、停止匹配测试和ARM交叉编译验证；不代表新版已在板上启动。替换镜像后通过受控重启生效，不停止正在喂狗的进程来热切换。既有实板断喂测试结论属于此前版本。

启动前置条件不满足时，允许MQTT、Web、tk8710_gw、tdaemon和FRP继续按既有流程运行；记录错误并由Web显示失败原因。不将喂狗服务加入tdaemon的proc列表，不使用init respawn，不因设备缺失循环重启整机。

必须区分：设备尚未打开时可以安全降级；设备打开后硬件可能已经计时，此时不能承诺喂狗失败仍无限保持业务运行。后者采用有限重试，失败请求系统重启，并由仍处于计时状态的片内WDT兜底。

## 组成与启动

| 文件 | 用途 |
|---|---|
| rk_wdt.c | 专用C喂狗进程，仅依赖libc |
| S15rk-watchdog | rcS按序启动独立后台服务，失败不阻断后续启动脚本 |
| ../webserver/src/watchdog_status.c | 只读状态及校验所属启动、进程名、PID和状态新鲜度 |
| ../webserver/web_resources/html/status_resources.html | 状态页新增“片内看门狗” |

板端程序：`/usr/sbin/rk_wdt`；启动脚本：`/etc/init.d/S15rk-watchdog`；状态：`/run/rk-watchdog/status.json`；日志：`/var/log/rk_watchdog.log`，超过256KiB时保留一份`.1`。

```text
Linux/init → rcS → S15rk-watchdog start（后台启动，立即返回）
  → 单实例文件锁
  → 等待/dev/watchdog0，最多30秒
  → 确认字符设备、DesignWare身份、nowayout=1
  → 打开设备（此时可能已开始倒计时）
  → 设置60秒 → 读回实际超时 → 首次KEEPALIVE
  → 每5秒KEEPALIVE，检查返回值并发布状态
```

不能把启动脚本返回0或PID存在视为保护已建立。`feeding`表示设置/读回超时与首次喂狗成功；`degraded`表示设置超时失败，但读回的既有超时在15～600秒范围且喂狗成功，继续使用已验证的既有值。该范围是本服务允许的裕量政策，不代表硬件所有档位。

## 故障分支

| 阶段/原因 | 行为 |
|---|---|
| 程序缺失/后台启动命令失败 | init记录failed并返回0，继续启动业务 |
| 设备缺失 | 最多等待30秒；记录device_missing并退出，不发起重启 |
| 非字符设备、身份不符、sysfs缺失、nowayout未启用 | 在打开设备前拒绝，记录失败；业务继续 |
| open报忙或权限等错误 | 不杀占用者，不反复拉起；退出并告警。占用者可能另有保护，不能断言整机一定没有任何看门狗 |
| open成功但设置/读取/首次喂狗不满足安全条件 | 最多3次初始化尝试，间隔200ms；仍失败则sync并请求reboot |
| 运行时KEEPALIVE失败 | 最多3次尝试，失败则记录fault并请求reboot；不假装仍在正常喂狗 |
| 软件重启调用失败 | 保持设备打开并停止喂狗，等待片内WDT超时；不退出后无限被拉起 |
| 正常运行时被TERM/INT终止 | 发布stopped，不写magic close、不调用DISABLECARD；nowayout保持保护，后续可能超时复位 |
| SIGKILL/崩溃 | 无法更新状态，Web凭进程/状态时效显示异常；硬件复位仍须实板验收 |

`S15rk-watchdog stop`有意不杀喂狗进程，让rcK停止业务和卸载阶段继续得到保护；最终由init/系统重启处理进程。`restart/reload`拒绝执行，不能对已经nowayout的看门狗随意热重启。`rk_wdt --help`只打印说明，不打开设备。

## 业务兼容边界

- 不修改MQTT协议、TK8710程序、GPS策略、射频初始化或tdaemon的网络恢复条件。
- 不加入`/etc/tdaemon.conf`的proc列表，不被S99 stop的默认业务名单停止。
- 现有三App SWU只停止/替换MQTT、Web、TK8710，不包含本服务；应用升级期间喂狗独立继续。
- watchdog不检查MQTT连接、TK8710初始化或射频收发健康，只建立系统级保护。业务进程故障仍由现有守护处理。
- 没有CPU绑核，不更改CPU2隔离和已有业务亲和性。
- 不同时启用BusyBox S15watchdog或其它喂狗服务；设备占用时本服务不会强行接管。
- 活跃WDT启用后，系统关机/重启的最长时间受实际超时预算约束。不能保证长时间阻塞的关机/升级永不触发硬件复位。

## SDK修改

SDK根：`/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0`。

板型DTS追加：

```dts
&wdt0 {
    status = "okay";
    timeout-sec = <60>;
};
```

板型defconfig新增`CONFIG_WATCHDOG_NOWAYOUT=y`、`CONFIG_WATCHDOG_SYSFS=y`，沿用已有`CONFIG_DW_WATCHDOG=y`。其它板型未同步启用。U-Boot没有启用WDT，Loader未修改。

`external/rk3506app/build_apps.sh --stage`编译`rk_wdt`和Web，将服务及S15脚本放入Buildroot overlay，并预置新版Web。**需要一起更新内核/DTB和rootfs**；仅刷userdata或旧三App SWU不会安装本服务。新服务在旧内核上遇到设备/sysfs缺失会降级，不能因此认为硬件已受保护。

本次已运行：应用构建及--stage、`./build.sh kernel`。已生成新的`kernel-6.1/zboot.img`和板型DTB；未重新制作rootfs.img/update.img，未烧录设备。

完整固件后续构建沿用：

```sh
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
bash external/rk3506app/build_apps.sh --stage &&
./build.sh &&
./build.sh extra-parts &&
./build.sh updateimg
```

烧录应在可现场恢复的测试板上进行，首次先确认其余业务正常，再安排喂狗中断复位试验。WDT复位后经过Bootloader；本次没有测量复位作用范围或验证Bootloader接管，所以不能声称硬件闭环验收完成。

## Web接口与查看

`GET /api/system_info`新增`watchdog`对象，复用既有登录鉴权及页面轮询。状态值：waiting、feeding、degraded、failed、fault、stopped、stale、unavailable。

状态文件含reason、pid、boot_id、updated_uptime_s、last_feed_uptime_s、armed、requested_timeout_s、actual_timeout_s和feed_interval_s。PID用于内部状态确认；时间使用CLOCK_BOOTTIME，不受NTP校时影响。更新超过20秒、进程不符或启动标识不符，不显示正常喂狗。`armed`仅表示本服务成功打开过设备，不是物理复位能力证明。

板端只读查看：

```sh
/etc/init.d/S15rk-watchdog status
tail -30 /var/log/rk_watchdog.log
ls -l /dev/watchdog*
cat /sys/class/watchdog/watchdog0/identity
cat /sys/class/watchdog/watchdog0/nowayout
cat /sys/class/watchdog/watchdog0/timeout
pidof rk_wdt mqtt_bridge web_server tk8710_gw
```

不要为检查而随意打开/写入watchdog设备或kill喂狗进程，后者在启用后正是复位触发条件。设备未就绪的前置故障修正后，可再次调用start；已armed的故障应走受控重启。

## 验证结果与限制

- 13项隔离服务测试通过：设备缺失、占用、错误身份、nowayout缺失、正常/延迟启动、超时设置降级、读回失败、超时过短、首次及运行喂狗失败、启动前终止、重复实例。
- init故障测试通过：程序缺失和启动失败不阻断rcS；stop不杀服务，restart拒绝。
- ARM/QEMU状态读取测试通过：文件缺失/损坏、六种服务状态、进程不存在、状态过期/未来时间和跨启动记录。
- 浏览器用实际页面JS测试八种状态，原网关运行、GPS显示保持正常，JavaScript错误为0。截图是隔离夹具数据，不是实板测量。
- ARM应用与内核构建通过。最终DTB仅wdt0为okay、timeout-sec=60；内核nowayout/sysfs已选；overlay程序/脚本一致。
- 没有修改板端、停止现有业务或触发任何真实WDT。首次真机喂狗、停止喂狗复位、异常退出、升级持续喂狗、正常关机及Bootloader重启闭环仍需实板验证。

测试命令：WSL内依次运行test_watchdog.py、test_init.py、test_web_status.py、verify_sdk.py；Windows运行test_ui.py。证据目录为`validation-20260922/`。
