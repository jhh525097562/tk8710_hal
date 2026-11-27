# 最新回归证据（接口通过，健康性待审）

board-run-06/run_id：ns-scan-20260910-124615。配置恢复true，整体脚本passed=true，未强杀测试进程。

## 已核对

- 真实81频点x8天线扫描；729值与快照完全一致。
- Broker断线重连和MQTT进程重启均补发相同结果。
- 业务ACK停止重发，磁盘记录acknowledged。
- 重复ID返回running，不重扫；不同参数返回request_id_conflict；另一个任务返回busy。
- Web原有接口启动第二次单点扫描，完成后NS快照不变。
- 两次restore ret=0；后续同步源external GPS/PPS且对齐RUNNING。
- 心跳35条，tk8710_gw运行秒数0..159，未出现PID字段。
- test_gateway_telemetry/test_ns_scan_protocol/test_ns_scan_manager均在ARM板退出0。
- 参数/传输主机测试-Werror最终重跑通过。

## 日志指标

gateway.log：[ERROR]=1346，[WARN]=10；ACM calibration requested=0，ACM hidden in slot3=0，valid=0/valid=1均0，ACM calibration has no valid result=0，ACM calibration request rejected=0，RX users=0，business configuration restore ret=0出现2次。

这不是ACM或正常业务空口收发测试，不能把上述0事件当作这些功能验证通过。TRM自动广播相关错误仍需定位来源并记录限制。

## 产物

mqtt_bridge：75e98d1fc855c4a990843de82bb6f92c9162f1d32c1f3839cd3d334bac84f754。
tk8710_gw：4487b2cdf54d1d4fb4a4e22bd6b7d5c952adea6c84517f8104c36f9152edb33c。

板端执行位于/userdata/ns-scan-20260910-124615，新文件未覆盖原业务二进制；原MQTT配置与守护状态已恢复。
