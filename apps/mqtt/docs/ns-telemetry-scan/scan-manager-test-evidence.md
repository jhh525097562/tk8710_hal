# MQTT扫描协调阶段测试

日期：2026-09-10。本记录不代替真实射频与生产NS联调。

## 实现

ns_scan_manager.c/h在Mosquitto回调中校验NS扫描请求并持久化受理记录。main线程轮询任务，通过Unix socket发START，按exec-ID.json/exec-ID.txt判定完成并封装结果。

job-ID.json通过临时文件、fsync、rename、目录fsync持久化。ready结果以QoS1每10秒尝试发布，NS scan_result_ack使状态变acknowledged。最多保留32个任务历史；满时明确拒绝history_full，不删除未确认数据。此容量策略需要部署文档说明后续维护方法。

已有相同ID相同参数返回当前状态pending/running/ready/acknowledged；不同参数request_id_conflict。目录存在孤立exec-ID状态时拒绝request_id_reserved，防止旧结果重用。

## 实际测试

build_sdk.sh退出0，新增manager测试-Werror构建通过。
上传并执行192.168.200.106:/userdata/test_ns_scan_manager_20260910，退出0。

stdout：

```text
PASS manager: IPC start, duplicate/conflict, wrong-task isolation, 8-antenna result, restart resend, durable ACK; fixture=/tmp/mqtt_scan_manager_xA20Ps publications=8
```

测试在临时目录使用真实Unix socket与持久化文件，publish回调用测试记录器替代Broker。没有操作TRM或替换板端业务进程。

## 尚需验证

真实MQTT发布订阅、断网重连；执行端超时停止、业务恢复；磁盘故障与损坏日志；新扫频81x8完整数据；运行中tk8710_gw时长；Web兼容。
