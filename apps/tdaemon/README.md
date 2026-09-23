# TurMass Link Gateway Daemon (tdaemon)

## 概述
tdaemon 是专为 TurMass 网关设计的守护进程系统，负责关键进程保活与多链路（Ethernet/4G/Wi-Fi）自动故障转移。

## 功能变更说明 (v1.1.0)
**重要：回切功能已删除**
- 系统不再自动从低优先级链路（如 Wi-Fi）切回高优先级链路（如 Ethernet）。
- 仅当当前活跃链路失效（丢包/延迟超标）时，才会触发新一轮的链路选择。
- 链路优先级固化为：`eth0 >  > wlan0`。

## 目录结构
- `tdaemon.sh`: 核心逻辑脚本
- `tdaemon.conf`: 配置文件（INI格式）
- `S99tdaemon_job`: Init.d 启动脚本
- `tests/`: 自动化测试套件

## 部署
1. 复制 `tdaemon.sh` 到 `/usr/bin/`。
2. 复制 `S99tdaemon_job` 到 `/etc/init.d/` 并赋予执行权限。
3. 复制 `tdaemon.conf` 到 `/etc/`。
4. 启动服务：`/etc/init.d/S99tdaemon_job start`。

## 配置
见 `tdaemon.conf.example`。注意：`recovery_threshold` 配置项已废弃，`priority` 配置项已废弃（硬编码）。

## 状态查看
- 进程状态：`/etc/init.d/S99tdaemon_job status`
- 活跃网络接口：`cat /var/run/tdaemon_job.state`
