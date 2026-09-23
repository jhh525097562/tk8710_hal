# 真实NS扫描板端证据

目标192.168.200.106；日期2026-09-10；运行目录/userdata/ns-scan-20260910-123524。
详细日志和MQTT报文见board-run-04/。

测试执行：tests/board_scan_e2e.py，显式指定新build_rk3506/tk8710_gw和mqtt/build_arm/mqtt_bridge。原业务二进制未覆盖，原配置已恢复。

真实链路为本机Broker隔离Topic模拟NS，测试客户端使用mosquitto_pub/sub；射频由目标板TK8710实际执行。不是生产NS服务器验证。

- 470000000至510000000Hz，500k模式，81频点x8天线。
- MQTT scan_result.state=completed，point_count=81。
- 81个频率和648个噪底值与任务快照逐项完全相同。
- scan_result_ack落盘后job.phase=acknowledged。
- [Scan] business configuration restore ret=0。
- 心跳system/tk8710_gw/GPS正常出现；tk8710_gw uptime_s从0增加至107，无pid字段。
- summary passed=true、config_restored=true，无forced_stop/cleanup_errors。

未据此判定总体完成：真实断网/重启、重复ID、Web兼容及恢复状态仍需进一步验收。
