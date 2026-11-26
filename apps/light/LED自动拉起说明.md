# LED异常退出自动拉起

2026-10-08，独立于网络就绪和硬件看门狗。

## 启动关系及行为

```text
开机rcS → S37led_agent start → led_supervisor.sh run → led_agent
```

- S37不再直接启动灯控，而是启动独立监护脚本；已有led_agent可被监护接管，无需重复启动。
- 每约1秒检查LED进程是否存活，通过可执行文件路径排除同名无关进程、通过/proc状态排除僵尸。
- LED异常退出或被SIGKILL后自动启动。距离最近一次启动至少5秒，防止GPIO持续占用、程序缺失等造成高速重启循环。
- 内核flock保证单监护实例；子LED关闭继承的锁描述符。新LED自身仍有GPIO和实例锁。
- 普通启动恢复后由原仲裁器决定颜色，三项健康稳定3秒后绿常亮；监护不篡改MQTT/网络/TK8710快照。
- 日志：`/var/log/led_supervisor.log`，超过256KiB保留一份`.1`。
- LED PID同步到`/var/run/led_agent.pid`；监护PID/start_ticks保存在`/run/led/supervisor.pid`。
- 只检测“进程退出”，不检测SIGSTOP挂起或内部死锁；监护脚本自身异常退出也没有额外自动重启层。这些不在本次验收范围内。

## 服务操作

```sh
/etc/init.d/S37led_agent status
/etc/init.d/S37led_agent stop
/etc/init.d/S37led_agent start
/etc/init.d/S37led_agent restart
```

`stop`先设置主动停止标志，再终止监护和LED，保持停止，不会被本监护重新拉起；`start`解除停止标志。所有信号只发给核对身份后的监护或LED进程，不影响MQTT、TK8710、Web或rk_wdt。
不使用S99tdaemon_job stop实现灯控停止，不把LED加入依赖网络就绪的业务启动列表。

## 手工闪灯兼容

`test_run_net.sh`和`test_net_blink.sh`已更新：

1. 测试脚本在停止LED之前调用`led_supervisor.sh pause $$`。
2. 暂停租约记录测试PID、start_ticks、120秒截止时间。租约期间监护不拉起LED，不抢占测试GPIO。
3. 正常退出/可捕获信号清理时调用`resume $$`，等待监护恢复LED。
4. 测试进程崩溃或被SIGKILL时，身份失效；监护下一轮自动结束租约。即使进程仍在，超过120秒也结束暂停。
5. 租约结束不强杀其它GPIO消费者。若测试子进程仍占GPIO，LED可能启动失败并按5秒间隔重试，直到GPIO释放。

使用板端最新版脚本：

```sh
sh /userdata/test_run_net.sh run
sh /userdata/test_run_net.sh net
sh /userdata/test_net_blink.sh
```

不要运行不支持租约的旧脚本，否则它停止LED后，监护会将LED重新拉起。旧固件没有监护脚本时，最新版测试脚本仍支持原来的直接恢复方式。
若强杀了整个手工测试，旧测试互斥目录`/var/run/net-blink-test.lock`可能残留；它不会阻止LED自动恢复，但再次手工测试前需要确认无测试进程/无GPIO测试占用，再清理这个空目录。

## SDK及部署

本地和SDK light目录同步led_supervisor.sh、S37及两个测试脚本。SDK build_apps.sh --stage安装`/usr/bin/led_supervisor.sh`和S37，overlay已更新；当前BusyBox配置CONFIG_FLOCK=y。
原S37保存在`light/backup/before_supervisor_20261008_175509/`。本次不需要重编LED C程序或内核。
完整升级包尚未重制、未上传Git。

## 验证结果

9项隔离进程测试通过：TERM/SIGKILL重启、暂停/恢复、测试进程退出、租约过期、主动停止、重复监护、无效租约请求及过期PID防误杀。

192.168.100.202实测：

- 原LED PID2874被SIGKILL后，约1.328秒启动新PID2779。
- 实际RUN单灯测试期间，监护租约生效，LED进程保持停止超过5秒，没有GPIO抢占；测试退出后自动启动PID722，恢复绿亮红灭。
- 追加验证S37主动stop后3秒保持停止，再start恢复正常。
- arbiter_history前后保留同一boot_id、曾就绪标志和网关身份；本轮没有人为注入fault_latched=1，不能把它当作故障锁存跨进程重启的实板故障测试。
- MQTT PID3064、TK8710 PID386、rk_wdt PID667未改变；看门狗继续feeding。

证据：`light/logs/supervisor_192.168.100.202_20261008_175602/evidence.json`及`final_service.json`。该实板测试未重启整机，开机自启动仍由已更新的S37接入。
