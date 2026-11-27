# 进度

## 当前结论（2026-09-10最终）

T1至T5已完成本次网关接口范围内的设计、实现与验证，交付报告见delivery-report.md，最终板端证据board-run-07。下文保留分阶段历史，不代表当前仍待实现。

最终回归包含真实扫频81x8、数据逐值匹配、重复/冲突/busy、真实Web并发拒绝及随后成功、Broker断线重连、MQTT重启补发、ACK、系统/进程遥测。GPS同步和两次恢复成功。扫描期间业务发送已暂停；恢复后无广播载荷错误是测试NS未提供业务载荷的既有条件，已列入报告的验证范围与限制。

板端原配置与实例恢复，新二进制位于原本地build输出；未提交Git、未覆盖板端生产二进制。下一步是按文档对接正式NS和部署，不是本次接口实现遗留项。

2026-09-10：用户授权先设计再实现测试及指定板验证。设计文件已生成，未声称完成实现。

- T1 已实现gateway_telemetry.c/h并接入本地GPS心跳；新增build_sdk.sh隔离旧Makefile自动SCP。ARM全量链接成功，测试程序在192.168.200.106执行退出0。运行中进程时长和真实MQTT报文仍待后续验证，证据见telemetry-test-evidence.md。
- T2进行中：scan_ipc_server已增加48字符request_id校验、状态带ID/参数、按ID保存结果快照与状态、托管任务抑制TRM提前写done。新增重复键/数值溢出校验。尚未完成执行端去重、工作线程关闭/超时、与NS配置及CLI互斥、业务恢复协调；不可部署此中间版本。
- T2传输：ipc_command_server命令容量增到256，按换行累积流式输入，2秒总期限，拒绝NUL并使用MSG_NOSIGNAL。主机tests/test_scan_ipc_params.c与test_scan_ipc_transport.c（实际位置test/example）均-Werror通过；scan_ipc_server ARM对象编译通过，完整gw链接和硬件待验证。
- T2新增证据：TRM_ProcessSweepCaptureInRx写结果后由TRM_UpdateSweepFrequencyAfterCapture置sweep_active=0、通知done、清理配置缓存并重新向NS请求配置。需协调该恢复路径与任务工作线程，防止完成状态提前发布，以及配置回调与扫描互相重置HAL。gw.c已有用户GPS修改，ApplyNsConfig/HandleNsConfig负责恢复GPS同步。
- T3已新增ns_scan_manager.c/h并接入on_message和主循环：32任务持久化日志、NS请求/ACK、socket发起扫描、按ID读取快照、QoS1结果10秒重发、重复/冲突/忙处理、重启恢复。ARM编译和板端模拟执行端测试通过，详见scan-manager-test-evidence.md。仍需真实Broker集成、磁盘故障/边界审查以及T2完成后射频闭环。
- T4 已完成只读连接，目标存在旧扫频文件，未见tk8710_gw运行，需部署前再次查询。
- T5 待完成。

## 真实板端闭环进展

T2补充：tk8710_gw注册StartManagedScan/RestoreManagedScan回调；NS配置和托管扫频通过配置锁串行，扫频后应用缓存NS配置。CLI改走同一Submit入口；工作线程540秒超时，停止服务等待任务结束；托管扫频不再从TRM中重复发配置请求。

完整tk8710_gw已通过SDK构建（既有SaveTxDcFromRfRam unused警告）。构建前原build_rk3506已备份至WSL /tmp/tk8710-build-before-ns-scan-20260910。

board-run-01：旧MQTT退出等待2秒不足，改15秒并避免重复恢复实例。
board-run-02：心跳2秒导致MQTT keepalive4被库拒绝，测试改5秒并增加已收到心跳前置判据。
board-run-03：真实扫频启动，但测试NS把普通业务上行数组当对象解析而中止；修复测试分类，退出时避免重新初始化射频。
board-run-04：真实Broker(本机1883隔离Topic) -> 新mqtt_bridge -> socket -> 新tk8710_gw -> TRM扫频，完成470-510MHz/500k的81x8，结果一次上报；restore ret=0。结果729个数值与exec-ID.txt逐值相同，job状态acknowledged。心跳进程时长0..107秒，running=true，无pid。原/etc/config.json恢复，未强制停止测试进程，未覆盖原二进制。

剩余：真实Broker重发/断线/重启与重复请求端到端测试、Web兼容、故障路径复核、审查、更新说明和交付报告。恢复日志EXTERNAL_READY变为LOCAL_STATIC，需要检查是当时GPS健康变化还是恢复问题；扫频时有既有自动广播参数错误日志，需分析后再做最终结论。整体目标仍未完成。

## 最新回归（board-run-06）

已解决GPS疑问：LOCAL_STATIC为扫频结束时暂态，ApplyNsConfig之后PPS达到RUNNING，实际同步恢复external GPS/PPS；NS和Web两次恢复ret=0。

配置缓存已修正为HandleNsConfig成功应用后的独立缓存（配置锁保护），不再从IPC刚收到的待处理配置恢复。MQTT重启恢复标志在识别running后清除，避免每轮重复写日志。

board-run-05和最新06扩展测试均退出0：独立本地Broker18884断线/重连补发、MQTT进程重启补发、ACK停止重发、重复请求不重扫、冲突拒绝、busy响应、Web单点再次扫描、NS快照不变全部通过。06原配置已恢复，无forced_stop/cleanup_errors。

06新产物SHA256：mqtt_bridge=75e98d1fc855c4a990843de82bb6f92c9162f1d32c1f3839cd3d334bac84f754；tk8710_gw=4487b2cdf54d1d4fb4a4e22bd6b7d5c952adea6c84517f8104c36f9152edb33c。与本地build输出一致。

06心跳35条，进程运行时长0..159秒，无PID；全部729个结果值与快照一致。新增存储缺失/损坏日志/中断任务恢复测试在板上退出0；参数和流式通信主机测试最终重跑通过。

MQTT实现详解新增第13节，已记录协议、ACK、构建、历史上限和现场验证范围。

剩余核查：gateway.log有1346条[ERROR]和10条[WARN]，需区分无广播载荷的既有TRM错误与本次扫描回归；本次RX users计数0，不能宣称正常业务空口收发验证通过。最终交付报告和验收状态尚未完成。

Git：tk8710_hal_v1.6工作树已有用户GPS/多速率改动，mqtt目录不是独立Git仓库。采用原目录串行局部修改，不提交。没有覆盖现有改动。
