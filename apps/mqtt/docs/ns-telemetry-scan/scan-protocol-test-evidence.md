# 扫频协议与结果解析阶段验证

2026-09-10，属于T3前置模块验证，尚未接通MQTT到扫频任务执行。

新增inc/ns_scan_protocol.h、src/ns_scan_protocol.c、tests/test_ns_scan_protocol.c。

## 功能

严格解析set.scan及request_id；限制ID字符、48字符长度、拒绝内嵌NUL。频率必须是JSON整数且正数，起点不大于终点；限制4096点及末点后递增溢出。模式只接受62.5k/125k/250k/500k。射频硬件范围还需执行端配置约束，当前协议数值校验不等同于硬件支持。

完整结果文件解析，兼容实际生产者的每个数值独立一行格式；验证预期频率序列和点数、每点8个有限浮点值，拒绝截断和多余记录。JSON上限1MiB。

## 命令与结果

`wsl -d Ubuntu-22.04 -- bash /mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app/mqtt/build_sdk.sh` 退出0，新模块测试-Werror构建通过。

上传到192.168.200.106的 `/userdata/test_ns_scan_protocol_20260910` 后执行：

```sh
/userdata/test_ns_scan_protocol_20260910 /userdata/SweepFreqResult/Result.txt
```

退出0；包含四种模式、无关报文、非法ID和NUL、字符串频率、逆序频率、整数溢出、超量点数、重复参数比较，以及结果缺项/错频/NaN/多记录测试。

实板旧Result.txt解析成功，81频点x8天线。旧文件仅作格式fixture，未用于证明本次执行完成。

## 未完成

尚未接入Mosquitto on_message、任务存储、业务ACK、执行端请求ID与结果快照、业务恢复；未替换生产mqtt_bridge或tk8710_gw。
