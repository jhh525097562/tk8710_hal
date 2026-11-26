# GPS卫星数及平均SNR心跳扩展

日期：2026-10-28。

生产网关将GPS状态写入`/userdata/GPS/GPSinfo.txt`。保留`lon/lat/status`，
增加`count/avgsnr`，GPS开启后的正常业务循环约每10秒更新。
MQTT在每次原有心跳发送时读取完整快照，心跳周期仍由`heartbeat_interval`决定。

```json
{"alive":{"ver":"mqtt_spi_v1.0.0","uptime":1793150000,"net":"eth0","power":1,"lon":"114.537037","lat":"30.492981","status":18,"count":12,"avgsnr":35}}
```

示例仅节选字段，原`rfstatus/system/tk8710_gw`等字段继续存在。
经纬度仍是字符串，`status`仍为连续健康检查次数；`count/avgsnr`是JSON整数。
`count`来自AG32 `GET STATUS`的`SATS`（参与定位的卫星数），
`avgsnr`来自`SNR`（AG32对新鲜GSV有效信号值求整数平均，非最大SNR）。
读取兼容LF/CRLF；旧三行文件、空值、负数、溢出或非法整数对应字段上报`null`。
合法的0上报数字0。UART查询失败时网关写0/0，状态健康计数也归零。
网关每次启动及运行中关闭GPS时，主动重置为`lon=unknown`、`lat=unknown`、
`status=0`、`count=null`、`avgsnr=null`。此快照是有效文件，不触发旧bdgps回退，
因此心跳对应输出字符串unknown、整数0以及真正的JSON null，不继续使用上轮GPS观测。
GPS主文件不可读或缺少经纬度时，沿用`/var/run/bdgps`回退；两者不可用时经纬度为`unknown`。

实现：`src/gps_info.c`解析文件，`main.c:send_heartbeat()`装配，
`mac_msg_parser.c:pack_gw_alive_up_msg_with_gps_metrics()`序列化。
保留原封包函数签名，不改变MQTT/SPI共享内存结构，不改NS下行或心跳发送周期。
NS服务端未在本工程内修改；服务器需兼容这两个附加字段。

验证：`bash build_sdk.sh`构建ARM程序及`test_gps_heartbeat`，
用例覆盖新旧文件、0值、CRLF、缺失、非法值及溢出，并检查JSON整数/null和原遥测字段。
`tests/verify_gps_telemetry_board.py`必须显式指定目标板、两个二进制和证据目录；
脚本备份原程序，暂停守护自动拉起，更换程序，并验证GPS文件、SIGTERM退出历史和独立Broker订阅收到的心跳。
测试失败自动恢复旧程序；不改连接参数和业务配置。
