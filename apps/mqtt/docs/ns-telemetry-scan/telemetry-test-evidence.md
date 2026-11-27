# 遥测阶段验证记录

日期：2026-09-10。此文件仅证明T1阶段，不表示扫频或整体目标完成。

## ARM构建

命令：`wsl -d Ubuntu-22.04 -- bash /mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app/mqtt/build_sdk.sh`

退出码0。mqtt_bridge和test_gateway_telemetry均为32位ARM EABI5。主程序仍有既有strncpy截断警告；新增遥测测试使用-Werror编译通过。

## 板端执行

目标192.168.200.106，上传test_gateway_telemetry到 `/userdata/test_gateway_telemetry_20260910`，执行退出码0，未替换运行中的mqtt_bridge。

```json
{"system":{"uptime_s":260478,"ram_total_bytes":501231616,"ram_used_bytes":144863232,"ram_used_percent":28.901455410187054,"disk_total_bytes":1014894592,"disk_used_bytes":43548672,"disk_used_percent":4.2909551734018896,"load_avg":[0.08740234375,0.1357421875,0.10107421875]},"tk8710_gw":{"running":false,"uptime_s":null}}
```

stdout：`PASS telemetry parser, missing/unknown process, system collection`。

覆盖stat进程名含空格/括号、无效stat、未知/proc路径、缺失进程、真实系统采集、未输出PID。仍需补运行中的tk8710_gw计时和MQTT心跳端到端测试。
