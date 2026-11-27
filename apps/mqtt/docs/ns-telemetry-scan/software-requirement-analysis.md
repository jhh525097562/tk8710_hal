# NS遥测与扫频需求

日期：2026-09-10。状态：实现基线；用户已授权设计、实现、测试及192.168.200.106板端验证。

## 范围

R1 保留当前心跳字段与GPSinfo.txt读取，增加system运行秒数、RAM字节与占用率、userdata磁盘字节与占用率、负载。
R2 增加tk8710_gw.running和uptime_s，不对NS发送PID；进程缺失为false/null，读取失败为null/null。
R3 NS通过现有sub_topic发送set.scan，包含request_id、start_freq_hz、end_freq_hz、mode。
R4 返回scan_ack；在任务成功或失败后，一次发送scan_result，含ID、参数及8天线每频点数据。
R5 Web与NS任务互斥；重复ID不重复执行，参数冲突拒绝。
R6 断网补发、持久化待确认结果、NS业务ACK与去重；完整报文容量有上限。
R7 不以旧Result.txt存在判完成；结果快照与任务身份绑定。
R8 TK8710侧接入NS扫频，并验证扫频后恢复正常配置与回调。
R9 完成主机测试、ARM构建、目标板MQTT到真实扫频完整验证。

## 非目标

不修改NS服务器代码（源码未提供），以测试NS客户端证明协议；不提交Git或刷整包固件。实际NS端仍需按交付协议接入。

## 现场基线

目标板SSH可达，2026-09-10首次查询运行mqtt_bridge但未见tk8710_gw。旧Result.txt存在且状态idle，不能作成功证据。TK8710工作树有用户GPS/多速率改动，必须保留。

