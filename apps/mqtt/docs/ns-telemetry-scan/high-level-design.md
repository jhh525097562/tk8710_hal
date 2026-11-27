# 概要设计

## 架构

NS -> 同一MQTT连接/sub_topic -> mqtt_bridge扫描协调模块 -> /tmp/data_collect.sock -> tk8710_gw扫描服务 -> TRM。
TRM完成 -> 任务专属结果快照和状态 -> mqtt_bridge完整JSON -> 原pub_topic -> NS业务确认。

系统遥测由小型独立模块读取sysinfo、statvfs和/proc，采用与Web一致的RAM口径(total-free)。Web的HTTP鉴权、页面及进程无需参与采集。

## 兼容

保留alive.uptime时间戳，新增alive.system.uptime_s开机秒数。GPS status仍是健康计数。tk8710_gw只暴露running/uptime_s。
保留原START命令供Web使用；扩展START的request_id，NS经本地socket请求。共享内存NS配置/业务收发结构不改。

## 执行风险

现有扫频服务会重新初始化HAL，须检查并补上配置、GPS同步和回调恢复，不能仅证明生成结果文件。
每个任务的结果必须在解除忙状态前完成快照，避免Web下一次任务覆盖。

