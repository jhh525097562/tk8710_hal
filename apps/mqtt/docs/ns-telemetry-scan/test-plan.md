# 测试方案

## 单元

遥测：真实/proc采集、缺失进程、未知进程状态、stat含括号、starttime解析、无PID上报、内存口径。
扫频：合法4种模式、范围/ID/JSON类型错误、重复和冲突、busy、半记录/NaN、过大结果、旧文件隔离、业务ACK。

## 集成

模拟Unix socket执行端+隔离MQTT Broker验证从NS下发到完整结果、QoS1重投、重启后补发、ACK停止重发。
ARM交叉构建两端，file/readelf校验依赖；不触发旧Makefile自动SCP。

## 板端

SSH 192.168.200.106 root；保存原二进制、启动参数、配置哈希。停止已确认的测试目标进程并控制守护重启，上传两个ARM产物到/userdata测试目录。
使用隔离Broker/Topic模拟NS，保留原生产Broker配置。验证心跳system/tk8710_gw/GPS字段；实际470-510MHz 500k扫描、81点*8天线、结果与文件逐值一致；重复ID不重扫；断网补发和ACK。
验证扫频结束恢复业务初始化与回调、Web扫描接口兼容。结束恢复原配置与守护策略，记录是否保留新二进制。

## 证据

每次测试保存命令、退出码、stdout、MQTT JSON、任务文件及SHA256；区分主机测试、ARM编译、真实射频和NS模拟器，不将模拟NS称为生产NS接入成功。

