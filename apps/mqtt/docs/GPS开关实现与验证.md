# NS GPS严格控制开关

## 接口

新增必填integer字段 `set.config.gps_enable`，只接受0/1。MQTT拒绝缺失、2、-1、true、null、字符串和浮点数，沿用现有非法配置丢弃行为（不新增配置应答协议）。

0：使用本地同步，不探测GPS、不打开GPS UART，关闭已经申请的GPS句柄；不对外部设备断电。
1：先检测GPS、等待基础健康和PPS对齐，通过后才启动业务。无设备/基础健康失败/对齐失败均非零退出，不允许静默回退本地同步。

首次启动先等待NS配置，不再提前探测GPS，不再无配置默认启动；GPS采集最长300秒，配置完成等待上限调整为420秒。运行中切换开关触发配置更新，GPS轮询与配置更新共享锁。扫频恢复使用成功应用配置中的同一开关。

## 文件

- MQTT inc/mac_msg_parser.h和TK8710 inc/mac_msg_parser.h：NsConfigDown_t末尾追加int gps_enable。
- MQTT src/mac_msg_parser.c：严格检查JSON类型和值。
- TK8710 src/tk8710_ipc_comm.c：配置消息长度必须匹配新结构，gps_enable仅0/1；变化检测增加该字段。
- TK8710 test/example/tk8710_gw.c：延迟GPS启动到ApplyNsConfig，关闭时本地同步；启用失败拒绝；删除无配置默认启动分支；轮询加配置锁。

这是IPC结构长度变更，必须配套部署新MQTT和新tk8710_gw，并更新NS发送配置。旧消息长度会被新网关拒绝。不要只替换其中一个程序。

## 验证

`bash build_sdk.sh`通过ARM构建，test_ns_gps_config在QEMU运行退出0，覆盖9类输入。
`bash cmake/build_rk3506.sh tk8710_gw tk8710_gw_gps_ns_test`通过ARM链接。保留既有unused函数/strncpy警告，不宣称零警告。

目标192.168.200.106，tests/board_scan_e2e.py使用原MQTT连接实现路径与测试Topic，经NS JSON下发到IPC、再到网关，不直接绕过配置接口。

| 目录 | 证据 |
|---|---|
| gps-policy-off | 真实设备存在，配置0：local同步、不持有ttyS4 |
| gps-policy-on | 真实GPS开启：外部同步、业务启动成功 |
| gps-policy-required-missing-02 | 板端测试构建no-module故障注入：配置1拒绝启动，无HAL started |
| gps-policy-toggle-02 | 正式网关构建，0→1→0，仅开关变化即触发配置，外部/本地切换均成功 |

这些测试summary.passed及config_restored均true，无forced_stop。无GPS场景为已有测试钩子故障注入，未物理拔除GPS。

共享内存保留旧的合法新格式配置时，重启可能先处理该配置再处理NS新配置；测试判据以本轮配置应用后为准，未擅自清空IPC队列。

生产配置与原进程在每轮测试后恢复；新程序在测试目录运行，未覆盖板端生产二进制。

## 最终扫频回归

gps-policy-scan-regression：启用gps_enable=1，真实470-510MHz/500k扫频81x8成功；MQTT结果逐值匹配、Broker断线补发、进程重启补发、Web兼容和NS结果快照保持通过，脚本退出0，原配置恢复true。NS扫描及Web扫描后的恢复继续使用外部GPS/PPS同步。

最终产物SHA256：

- mqtt/build_arm/mqtt_bridge：64d7083dc45d8cbc121b4c657b1938cf8a18d806888852965fecc0a501bab83e。
- tk8710_hal_v1.6/build_rk3506/tk8710_gw：b3f6b934f4f490425fdf5bcac1094f0b9308dedb886bf18f255701e73ec15530。

验证不等同于正式NS服务器已完成字段接入；上线前NS必须发送gps_enable，并同时更新MQTT和网关产物。
