# DNS统一管理与FRP恢复验证

更新日期：2026-09-15；验证板：192.168.100.63。

## 原因与修复

此前/etc/resolv.conf指向/tmp/resolv.conf，内容被dhcpcd写成只有注释、没有nameserver。frpc每约5秒启动一次，但域名解析失败后立即退出。不是FRP启动脚本未执行。

现在以/etc/tdaemon.conf的dns_primary、dns_secondary为DNS服务器地址的唯一配置来源。/etc/tdaemon-dns-owner标记启用此策略：

- udhcpc仍管理IP、路由，将自己的DNS输出写到/run/udhcpc-resolv.conf，不覆盖系统解析文件。
- dhcpcd的20-resolv.conf钩子检测到标记即返回，不改写系统DNS；DHCP本身继续运行。
- tdaemon启动、配置检查循环及网络切换时调用dns_ensure_config，恢复空DNS、错误DNS或与配置不一致的nameserver。
- 通过解析符号链接、在目标目录创建临时文件并rename，避免先截空解析文件。已有search/domain/options等非nameserver内容保留。
- 配置正常时不重写文件，降低写入频率；非法IPv4配置保留当前文件并报错。当前板不支持IPv6，此实现校验IPv4 DNS。
- 升级维护标记存在时暂停DNS修复，遵循现有升级维护逻辑。

检查随守护循环执行，不保证固定1秒；网络检测、进程检查会影响周期。本次空DNS恢复实测5.718秒。

## 设置及查看

修改/etc/tdaemon.conf中已有字段，勿重复追加同名配置项：

```ini
dns_primary=223.5.5.5
dns_secondary=180.76.76.76
resolv_conf=/etc/resolv.conf
```

守护原有配置重载机制读取变更后，会应用DNS。不要再依赖直接修改/etc/resolv.conf来永久配置DNS，该临时修改会被守护纠正。

```sh
cat /etc/resolv.conf
nslookup amqtt.taolink-tech.com
pidof frpc
tail -30 /var/log/frpc.log
```

若修改了其它组件的DNS设置，应同时更新这里的权威配置，避免下一轮被恢复成旧值。

## 文件与SDK集成

| 文件 | 用途 |
|---|---|
| tdaemon_dns.sh | 校验、比较、原子更新DNS |
| tdaemon.sh | 加载模块，在启动及每轮配置检查后调用 |
| tdaemon_net.sh | 网络切换改为调用统一DNS函数 |
| dns-hooks/default.script | udhcpc DNS输出与系统DNS分离 |
| dns-hooks/20-resolv.conf | 禁用dhcpcd对系统DNS的改写 |
| dns-hooks/tdaemon-dns-owner | 基础固件的DNS策略开关 |
| stage_dns.sh | 将模块、钩子和标记安装到SDK overlay |
| test_dns.sh | 临时目录中的功能回归测试 |
| verify_dns_board.py | 明确指定目标板的安装和故障恢复验证，不是只读脚本 |

本地与SDK均已更新；既有tdaemon_net.sh在两处存在差异，本次仅修改其原DNS写入点，没有用整文件覆盖无关实现。

SDK位置：external/rk3506app/tdaemon，以及buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay。build_apps.sh --stage已接入stage_dns.sh。overlay的tdaemon_net.sh另有4G/Wi-Fi恢复定制，本次对其DNS写入点单独修改，构建步骤不使用应用目录的较旧网络脚本覆盖它。

```sh
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
bash external/rk3506app/build_apps.sh --stage
```

这些更改在rootfs内，仅更新userdata或此前制作的三App SWU不能带入DNS钩子。下一次完整固件制作需重新编入rootfs。本次没有重新制作update.img或重启网关。

## 验证结果

- 隔离测试通过：空文件/错误DNS恢复、符号链接保留、search保留、正常文件inode不变、非法配置拒绝、维护期间不改写。
- 板上恢复两条nameserver，使用系统默认DNS解析amqtt.taolink-tech.com成功，返回47.92.195.235。
- 模拟清空解析文件，首轮恢复耗时5.718秒；最终复核恢复耗时4.797秒，文件带有tdaemon管理标记，守护日志在18:18:04记录DNS配置已恢复。
- 在owner标记下执行dhcpcd解析钩子，没有改写DNS。
- frpc日志出现login to server success、SSH/Web两条start proxy success；不是仅凭PID判断。
- 仅重启tdaemon解释器，未执行S99tdaemon_job stop；MQTT/TK8710 PID保持3143、4050，/etc/config.json哈希不变。
- 未强制DHCP真实续租或网关重启，未验证外部客户端经FRP穿透访问的全链路。

板端原文件备份：/userdata/dns-owner-backup-VfdE4f。original-0至original-3分别为tdaemon.sh、tdaemon_net.sh、udhcpc default.script、dhcpcd 20-resolv.conf；完整映射及日志在[board-result.json](validation-20260915/board-result.json)。

需要回退时应在维护窗口恢复原文件、移除owner标记并重启守护，勿调用会强杀所有业务进程的旧S99 stop逻辑。新脚本和标记均为持久文件，但本次验收范围不包含重启测试。
