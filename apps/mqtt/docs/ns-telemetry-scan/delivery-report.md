# NS遥测与扫频交付报告

日期：2026-09-10。完成用户要求的方案设计、两端实现、主机/ARM测试与192.168.200.106网关验证。生产NS服务器源码不在本次范围，板端使用真实Mosquitto Broker和测试NS客户端验证协议。

## 需求与证据

| 需求 | 实现与验证 |
|---|---|
| R1 系统/GPS遥测 | gateway_telemetry.c；心跳含系统运行秒数、RAM、userdata磁盘、负载，GPS保留新路径和status；ARM单元及board-run-06/07心跳 |
| R2 进程时长，无PID | /proc精确匹配tk8710_gw，缺失/未知区分；测试覆盖缺失，运行时长在真实心跳递增，无pid |
| R3 NS配置扫频 | ns_scan_protocol/manager解析set.scan；通过原sub_topic发起，TK8710注册托管扫描回调 |
| R4 受理/完整结果 | scan_ack及scan_result；board-run-07真实81x8数据，全部729个数值与快照相等 |
| R5 去重/互斥 | 重复ID返回running、参数冲突拒绝、第二NS任务busy；07执行时实际Web接口返回busy；随后Web单点任务成功 |
| R6 重发/确认 | 独立18884 Broker断线重连、MQTT重启补发、业务ACK后不再发送；结果QoS1，32份历史/1MiB结果上限 |
| R7 结果身份 | job-ID/exec-ID状态与快照；错误ID隔离单元测试；Web覆盖通用结果后NS快照不变 |
| R8 业务恢复 | ApplyNsConfig恢复已成功应用的配置和HAL回调；两次restore ret=0，PPS状态RUNNING，外部GPS/PPS同步恢复 |
| R9 测试与板端 | 三个ARM测试程序、两项服务主机测试、工作线程失败测试；board-run-07全链路退出0 |

## 改动文件

MQTT：新增gateway_telemetry.c/h、ns_scan_protocol.c/h、ns_scan_manager.c/h，main.c接入路由/任务轮询，mac_msg_parser.c/h扩展心跳。新增build_sdk.sh、tests下测试程序及board_scan_e2e.py。

TK8710：tk8710_scan_ipc_server.c/h扩展任务ID、快照、互斥、超时与停止；ipc_command_server.c增加流式命令与读期限；tk8710_gw.c注册启动/恢复回调，CLI统一入口，成功应用NS配置独立缓存；trm_core.c托管任务不重复请求配置、扫描期间暂停业务发送。

保留此前已有GPS/多速率等用户修改；未stage/commit/push，集成状态为当前工作目录已应用、patch-ready。

## 构建产物

MQTT：`D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\mqtt\build_arm\mqtt_bridge`

SHA256：`75e98d1fc855c4a990843de82bb6f92c9162f1d32c1f3839cd3d334bac84f754`

TK8710：`D:\CodexWorkSpace\tk8710_hal_v1.6\build_rk3506\tk8710_gw`

SHA256：`d7761f7b2906d47bf0d2786ce6db0e178603135a47e8f44254badef88bdf5988`

两者为32位ARM EABI5。旧build_rk3506已在WSL `/tmp/tk8710-build-before-ns-scan-20260910` 备份。

## 可复现测试

1. MQTT目录运行 `bash build_sdk.sh`，编译主程序及test_gateway_telemetry、test_ns_scan_protocol、test_ns_scan_manager；测试模块-Werror。
2. TK8710目录加载environment-setup后运行 `bash cmake/build_rk3506.sh tk8710_gw`。
3. 主机gcc编译test/example/test_scan_ipc_params.c（-ffunction-sections -fdata-sections -Wl,--gc-sections）及test_scan_ipc_transport.c，-Wall -Wextra -Werror，退出0。
4. 同样编译test_scan_worker_failures.c，验证启动失败、查询失败、恢复失败和停止恢复，退出0。
5. 三个MQTT ARM测试上传192.168.200.106并执行，均退出0；覆盖畸形输入、完整/截断/NaN结果、状态持久化、缺失存储/损坏日志、任务中断与ACK。
6. `python tests/board_scan_e2e.py --extended --host 192.168.200.106 --gateway <tk8710_gw产物> --mqtt <mqtt_bridge产物> --output <新的证据目录>`。

最新完整证据board-run-07：run_id=ns-scan-20260910-125351。summary内snapshot_exact_match、broker_reconnect_resend、process_restart_resend、web_compatible_snapshot_preserved、passed、config_restored均true。无forced_stop/cleanup_errors。

## 代码审查结果

已修复：扫描期间误发无载荷广播、待处理NS配置覆盖恢复依据、接收分段命令、空客户端阻塞、NUL请求ID、整数溢出、坏结果文件、任务恢复每轮重复写日志。

遵循现有JSON-C、libmosquitto、POSIX和libgpiod；没有新增运行库。原共享内存结构ABI不变，本地Unix命令向后兼容Web。测试仅改变独立Broker/Topic，未向生产NS发布测试命令。

git diff --check对本次TK8710源文件通过。构建存在既有strncpy截断提示及SaveTxDcFromRfRam未使用警告，未将其描述为零警告构建。

## 范围与运行限制

- 实际射频验证为470-510MHz、500k；四种模式的参数解析通过测试，但其他硬件频段须先确认模块支持。
- 此次没有测试终端配合的正常业务空口收发或ACM标定性能；恢复验收是配置/HAL回调恢复和PPS同步状态，不等同于全系统无线业务验收。
- board-run-07有373条ERROR、10条WARN。扫描期暂停了业务发送；恢复后的长度0广播错误源于测试NS未提供业务广播载荷（g_broadcastData仅在TRM_SetTxData下行业务时填充），另有既有启动ACM目标次数日志。不能将接口验收结果声称为全量运行零错误。
- 32份任务历史满时返回history_full，不自动删除未确认数据。已确认历史归档需要运维操作并理解ID去重范围。
- 断网补发等通过测试NS验证，正式NS仍需实现本报告接口与业务ACK。
- 最新测试之后仅增加可覆写状态文件宏以隔离工作线程单元测试，默认展开值及生产执行逻辑未改变。

## 板端交付状态

新程序在 `/userdata/ns-scan-20260910-125351/` 执行验证，原/userdata/mqtt_bridge和tk8710_gw未覆盖。原/etc/config.json已逐字节恢复，原MQTT和守护状态恢复；测试记录留在本地及板端独立目录。

生产部署需同时替换两个新程序并重启对应服务，使新增NS命令和执行端协议一致。当前未刷固件，也未修改生产NS服务器。
