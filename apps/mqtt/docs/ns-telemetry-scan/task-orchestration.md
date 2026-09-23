# 任务编排

单主代理串行写入，patch-ready，不stage/commit/push。用户现有改动保留。

|任务|依赖|范围|验收与测试|回退|
|---|---|---|---|---|
|T1 遥测|无|mqtt遥测模块/心跳|单元+ARM构建，字段正确，无PID|恢复新增字段调用|
|T2 扫频执行端|设计|scan_ipc/service、gw入口|stub状态机测试+ARM构建，任务身份/快照/恢复|还原本次差异|
|T3 MQTT任务协调|T2|mqtt解析/队列/持久化|隔离Broker集成，重复/恢复/ACK|还原本次差异|
|T4 板端验收|T1,T2,T3|192.168.200.106|真实81点扫描、遥测、去重、重发、恢复|原二进制/参数/配置备份|
|T5 文档交付|T4|实现说明/报告|逐项检查R1-R9证据|保留历史记录|

执行顺序T1 -> T2 -> T3 -> T4 -> T5。诊断：编译-Wall/-Wextra、git diff --check；具体测试入口随实现写入此表。外部NS源码未提供，以模拟NS验证网关接口。

已落实命令：`bash mqtt/build_sdk.sh`完成MQTT ARM链接和遥测测试构建；目标板运行 `/userdata/test_gateway_telemetry_20260910`。T1板端孤立测试已过，不替代T4真实MQTT验收。

细分T3：T3a协议解析/结果文件校验不依赖T2，已先行实现测试；T3b持久化任务协调依赖T2执行端协议。T3a测试：`/userdata/test_ns_scan_protocol_20260910 /userdata/SweepFreqResult/Result.txt`，预期退出0且81x8。后续顺序T2 -> T3b -> T4 -> T5不变。
