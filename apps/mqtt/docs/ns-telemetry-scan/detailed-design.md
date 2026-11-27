# 详细设计

## MQTT协议

下行：{"request_id":"scan-001","set":{"scan":{"start_freq_hz":470000000,"end_freq_hz":510000000,"mode":"500k"}}}
受理：{"scan_ack":{"request_id":"scan-001","state":"accepted"}}。
状态取accepted/busy/rejected，拒绝含error_code/message；重复相同请求复用已有状态。
重复请求应答state可为pending/running/ready/acknowledged，以便NS判断已有任务位置。新请求受理为accepted。
完成：{"scan_result":{"request_id":"scan-001","state":"completed","point_count":81,"antenna_count":8,"points":[{"freq_hz":470000000,"noise_dbm":[-120,-121,-122,-123,-124,-125,-126,-127]}]}}。
失败结果含state=failed、error_code/message与原参数。
确认：{"scan_result_ack":{"request_id":"scan-001"}}。
结果QoS1；同一ID重发保持内容一致。NS按ID幂等存储后确认；Broker PUBACK不能替代业务确认。

## 任务数据与边界

ID限定ASCII字母数字、下划线和连字符，最长48字符；禁止路径字符。模式仅62.5k/125k/250k/500k。
检查频率为整数、起点不大于终点、硬件频段和频点数上限。硬件范围与TRM实际实现核对后固化；禁止猜测超范围硬件值。
结果上限1MiB，点数上限4096；写完整记录，拒绝不完整/非有限噪底。
持久化状态放/userdata/mqtt_scan；临时文件+fsync+rename，目录fsync。限制历史任务数量/容量，拒绝过载；不静默丢弃未确认结果。
当前实现最多32份任务历史（含已确认），满时拒绝history_full；每份最多1MiB结果加4096字节元数据。生产运维需在保留ID去重要求下定义归档策略，不能直接删未确认任务。
状态pending -> running -> completed/failed -> acknowledged。执行端在任务专属快照完成后发布完成状态。重启后未完成任务标记interrupted，禁止盲目重跑。
轮询与结果发送不能在Mosquitto回调中等待扫频，回调只校验/入队，主循环处理有界工作。

## 遥测

system字段uptime_s、ram_total_bytes、ram_used_bytes、ram_used_percent、disk_total_bytes、disk_used_bytes、disk_used_percent、load_avg。
/proc扫描精确匹配comm=tk8710_gw，再校验stat进程名与starttime；不误匹配slave/sat/ground。HZ由sysconf获取。未知字段JSON null，不能用0伪装成功。

## 本地协议

扩展扫描服务START请求参数request_id，保留旧Web命令。状态增加请求ID、起止频率、mode、结果快照路径、错误代码。仅使用经过严格校验的ID生成路径。
关机时等待/收敛任务线程；扫频超时需进入恢复流程。Web和CLI直接扫频也必须纳入同一互斥策略。

## 安全与观测

复用Broker已有认证，不写日志密码。拒绝JSON类型混淆、超长ID、shell元字符。记录每任务受理/启动/完成/持久化/重发/ACK及耗时。
