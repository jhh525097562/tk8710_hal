# 多网关 GPS 同步组网自动测试

该工具自动执行《多网关GPS同步功能验证》中的用例4、5、6。它从
`docs/trm_slot_analysis` 的合法结果中固定种子抽样，经 MQTT 同时配置两台网关，
等待30秒 GPS/PPS 对齐，再配置终端、入网并逐包发送带 ACK 上行。

终端每次重新入网前先执行配置项 `fixed_frequency_slot` 对应的
`AT+FREQSLOT=496800000,496820000,496828000,1`，避免终端跳频与两台网关晶振频差
共同影响双网关接收；随后固定执行 `AT+PWRCTRL=0` 关闭功控，再执行 `AT+TXP=15`
设置最大发射功率。入网前固定执行 `AT+SENDPOL=0,0`，关闭终端内部自动重传，避免
一次 `AT+SENDB` 在收到 ACK 前连续产生多次空口上行。终端配置或入网遇到瞬态串口
错误时，默认关闭并重新打开串口后完整重试三次，重试流程也会重新设置上述参数。

## 测试口径

- 用例4：`tdd_num=1`，模式5、6、7、8、9、10、11、18各抽40组。
- 用例5：不固定20；从 `super_frame_num>1` 的全部合法单速率组合中，每个模式抽40组。
- 用例6：六个指定多速率组各生成40组合法配置，终端只配置组内最低速率。
- 每组发送10个带唯一payload的ACK包。每包必须同时满足：两个网关均通过MQTT上报；
  RSSI较强网关是唯一出现TRM发送证据的网关；终端出现`RX_DATA`和`+TXSTATUS:7`。
- 逐组合记录成功包数和成功率（成功包数/计划发送的10包）。成功率大于等于70%且10包全部完成发送时，
  该组合判定通过；因此7/10通过，6/10不通过，未发满10包不通过。
- 用例5额外要求两台网关TRM日志的`superFrame`相同；用例6额外要求两台网关
  `rateMode`相同，不要求等于配置组的最低速率。RSSI相同无法判断唯一较强网关，按失败记录。
- CSV记录的是物理包块数，NS接口接收的是MAC payload字节长度。模式5～11按26字节
  包块换算，模式18按40字节包块换算，均扣除15字节协议开销。当前NS下发范围限制
  模式5～8和18最多10块、模式9/10/11最多16块。每组业务测试前还会解析两台网关TRM日志，确认实际应用的速率和上下行
  块数与抽样清单完全一致；不一致时该组直接失败。

网关映射默认为：`0499999999999999 → 192.168.200.106 → NS nwk_num=2 → bcnbits=1`，
`0599999999999999 → 192.168.100.63 → NS nwk_num=3 → bcnbits=2`。

## 准备

```powershell
python -m pip install -r tools\multi_gateway_sync_test\requirements.txt
Copy-Item tools\multi_gateway_sync_test\config.example.json `
  tools\multi_gateway_sync_test\config.local.json
$env:TK8710_SYNC_MQTT_PASSWORD = "实际MQTT密码"
$env:TK8710_SYNC_SSH_PASSWORD = "实际SSH密码"
$env:TK8710_SYNC_ROOT_KEY = "实际终端RootKey"
```

修改本地配置中的终端串口、信道、`fixed_frequency_slot` 及账号环境变量。
`config.local.json`含现场参数，不要提交到版本库。

两台网关必须使用相同固件，并以`--trm-log-level info`运行。`log_mode=attach`只读取
现有`/userdata/8710log`的增量；`log_mode=manage`会执行配置中的`stop_command`，再以
`start_command`启动测试进程。管理模式会改变网关运行状态，必须在受控测试窗口使用。

## 命令

先只生成并审阅随机组合：

```powershell
python tools\multi_gateway_sync_test\sync_network_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --cases 4,5,6 --seed 20260908 --sample-count 40 --generate-only
```

执行实机测试：

```powershell
python tools\multi_gateway_sync_test\sync_network_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --cases 4,5,6 --seed 20260908 --sample-count 40
```

单独运行一个用例时使用`--cases 4`、`--cases 5`或`--cases 6`。抽样清单首先写入
`selection.json`，即使实机阶段中断也能保留本轮配置。结果目录还包含脱敏配置、
逐包网关TRM增量、`evidence.json`、`summary.json/csv`、`report.md`和完整事件日志。
只验证用例4或5中的指定速率时可增加`--rates 8`，多个速率用逗号分隔。

按默认参数完整执行共880组，仅每组30秒同步等待就需要约7小时20分钟，尚未计入
终端入网、10包收发和日志收敛时间。现场建议按用例分别执行，并先用
`--sample-count 1`完成链路冒烟测试；正式结果仍必须使用`--sample-count 40`。

按用例4、5、6顺序执行小样本验证（每个单速率模式或多速率配置组抽2组）：

```powershell
python tools\multi_gateway_sync_test\run_cases_5_6.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --cases 4,5,6 --seed 20260909 --sample-count 2
```

调度目录中的`aggregate_summary.json`和`aggregate_summary.md`汇总各用例的组合通过率与逐包成功率。

按顺序执行完整长测：先运行用例4、5、6（各速率或多速率组抽40个样本），第一阶段全部
执行结束后，再运行用例5的固定模式8、`tdd_num=20`、上下行各50字节、1000个带ACK
逻辑包。第一阶段存在失败组合时仍继续第二阶段；`campaign_status.json`记录两个阶段的
命令、状态、退出码和日志路径。

```powershell
$env:TK8710_SYNC_MQTT_PASSWORD = "实际MQTT密码"
$env:TK8710_SYNC_SSH_PASSWORD = "实际SSH密码"
$env:TK8710_SYNC_ROOT_KEY = "实际终端RootKey"
python tools\multi_gateway_sync_test\run_full_campaign.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --seed 20260909 --sample-count 40 `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 `
  --packet-count 1000 --max-send-attempts 3 `
  --location-description "重叠覆盖区域-用例5模式8-1000帧"
```

这里的“1000帧”按1000个逻辑业务包统计；每包首次只发送一次，仅在该包未得到完整ACK
结果时最多再尝试两次。完整40样本阶段共880个配置组合，运行时间通常超过一天，需保证
Windows主机、COM3、网络、两台网关和终端在整个测试期间持续可用。

## 指定配置与广播稳定性测试

`configured_sync_test.py`用于固定现场位置下的长时间重复测试。速率模式、`tdd_num`、
上下行长度和逻辑数据包数均由命令行传入，不在脚本中固定。上下行长度是NS接口使用的
MAC payload字节数；脚本会换算物理包块数、计算时隙周期，并检查两台网关实际应用的
配置。每个逻辑包只先发送一次；仅当终端没有同时收到`RX_DATA`和`+TXSTATUS:7`时，
才最多追加两次主机侧发送尝试。

模式8、`tdd_num=20`、上下行各50字节、发送20个逻辑包，在重叠覆盖区域运行：

```powershell
python tools\multi_gateway_sync_test\configured_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 --packets 20 `
  --location-description "重叠覆盖区域"
```

靠近网关04或网关05时，可分别执行以下命令；`--expected-bcnbits`会把非期望值计为
广播异常：

```powershell
python tools\multi_gateway_sync_test\configured_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 --packets 20 `
  --location-description "靠近网关04" --expected-bcnbits 1

python tools\multi_gateway_sync_test\configured_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 --packets 20 `
  --location-description "靠近网关05" --expected-bcnbits 2
```

结果目录名包含`--location-description`的安全化文本，可使用“重叠覆盖区域”、
“靠近网关04”、“靠近网关05”或其他现场说明。`--cfo-jump-threshold`设置相邻广播
CFO绝对差阈值，默认500；`--output-root`可修改结果根目录。

脚本解析`RX_TDD:<tdd>,<rssi>,<snr>,<cfo>,<bcnbits>`，保留全部原始样本，并按
`bcnbits`分别统计RSSI、SNR和CFO。TDD序号应按`1..tdd_num`递增，`tdd_num`到1的
回绕属于正常；CFO相邻差超过阈值计为跳变；所有bcnbits切换都会记录。重叠区域允许
1和2之间切换，靠近单一网关时按`--expected-bcnbits`判定。脚本兼容缺少bcnbits的
旧四字段日志以便保留证据，但该情况判为失败。串口漏读同样可能表现为观测到的TDD
不连续，因此TDD异常需要结合`events.log`和原始串口日志判断是设备跳变还是采集丢行。

每轮生成`report.md`、`summary.json`、`packet_summary.csv`、
`broadcast_samples.csv`、`broadcast_anomalies.csv`、逐次发送证据和脱敏配置。
报告同时统计两台网关收到的逻辑包数、接收事件、ACK发送尝试和ACK发送事件。

### 2026-09-09 实机验证命令

以下命令已在网关`192.168.200.106`和`192.168.100.63`、终端固定频点的现场环境中
完整执行成功。运行前将占位密码替换为现场实际值：

```powershell
$env:TK8710_SYNC_MQTT_PASSWORD = "实际MQTT密码"
$env:TK8710_SYNC_SSH_PASSWORD = "实际SSH密码"
$env:TK8710_SYNC_ROOT_KEY = "实际终端RootKey"
python tools\multi_gateway_sync_test\run_cases_5_6.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --cases 4,5,6 --seed 20260909 --sample-count 2 `
  --output-root sync_network_results\all_cases_sample2_fixedfreq_verified_v2
```

结果目录：
`sync_network_results\all_cases_sample2_fixedfreq_verified_v2\cases_5_6_20260909_121158`。
本轮用例4为16/16组、158/160包（98.75%），用例5为16/16组、160/160包
（100%），用例6为12/12组、118/120包（98.33%）；合计44/44组通过、
436/440包通过（99.09%）。该结果是每个模式或配置组抽2组的小样本验证，不替代
每组抽40组的正式覆盖测试。

## 判定边界

该工具验证数字日志层面的双网关接收、强RSSI网关唯一ACK、超帧号和速率一致性。
它不能代替示波器对PPS/BCN/时隙边界相位差的物理测量。终端必须通过60 dB衰减并
放在两台网关都能稳定收到上行的位置；脚本不能自动完成终端的物理移动。

运行单元测试：

```powershell
Push-Location tools\multi_gateway_sync_test
python -m unittest -v
Pop-Location
```

## 单组固定配置与广播稳定性测试

`fixed_config_sync_test.py`用于在一个指定位置连续验证一组参数。速率模式、`tdd_num`、
NS上下行长度及逻辑发包数均由运行命令配置；默认值分别为模式8、20、50字节、50字节和
20包。脚本仍复用本目录的双网关MQTT配置、终端固定频点、最大发射功率以及
`AT+SENDPOL=0,0`配置。

每个逻辑包只先发送一次；仅当终端未同时返回`+TXSTATUS:7`和`RX_DATA`时才重试，默认
最多尝试3次。每次尝试均独立抓取两个网关的TRM日志。逐包判定要求两个网关均收到上行、
两边超帧号一致、RSSI唯一较强的网关发送ACK，并且终端收到ACK。

重叠覆盖区域的完整命令：

```powershell
python tools\multi_gateway_sync_test\fixed_config_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 `
  --packet-count 20 --max-send-attempts 3 `
  --cfo-jump-threshold 500 `
  --location-description "重叠覆盖区域"
```

上版后的长样本验证可将逻辑包数调整为100，其他测试参数仍由命令行显式指定：

```powershell
python tools\multi_gateway_sync_test\fixed_config_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 `
  --packet-count 100 --max-send-attempts 3 `
  --cfo-jump-threshold 500 `
  --location-description "重叠覆盖区域-上版100包验证" `
  --output-root sync_network_results\fixed_config_sync_test
```

靠近网关04和靠近网关05时，分别显式指定期望广播来源：

```powershell
python tools\multi_gateway_sync_test\fixed_config_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 --packet-count 20 `
  --location-description "靠近网关04" --expected-bcnbits 1

python tools\multi_gateway_sync_test\fixed_config_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 --packet-count 20 `
  --location-description "靠近网关05" --expected-bcnbits 2
```
python tools\multi_gateway_sync_test\fixed_config_sync_test.py --config tools\multi_gateway_sync_test\config.local.json --rate 8 --tdd-num 20 --uplink-length 50 --downlink-length 50 --packet-count 20 --location-description "靠近网关05" --expected-bcnbits 2

需要验证单网关退出和恢复时，增加`--interrupt-one-gateway`。下例仍由命令配置模式8、
`tdd_num=20`、上下行50字节和总计20包；第5包完成后随机停止一台网关，第10包完成后
重新启动该网关，第11～20包用于观察恢复情况：

```powershell
python tools\multi_gateway_sync_test\fixed_config_sync_test.py `
  --config tools\multi_gateway_sync_test\config.local.json `
  --rate 8 --tdd-num 20 `
  --uplink-length 50 --downlink-length 50 `
  --count 20 --max-send-attempts 3 `
  --cfo-jump-threshold 500 `
  --location-description "重叠覆盖区域-单网关中断恢复" `
  --interrupt-one-gateway --gateway-interruption-seed 20260909
```

python tools\multi_gateway_sync_test\fixed_config_sync_test.py --config tools\multi_gateway_sync_test\config.local.json --rate 8 --tdd-num 20 --uplink-length 50 --downlink-length 50 --count 20 --max-send-attempts 3 --cfo-jump-threshold 500 --location-description "重叠覆盖区域-单网关中断恢复" --interrupt-one-gateway --gateway-interruption-seed 20260909

`--gateway-interruption-seed`用于复现随机选择结果；省略时每次运行随机选择。该功能要求
两台网关均配置`log_mode=manage`，以便脚本只停止自己启动并记录PID的进程。恢复时沿用
对应网关的`start_command`和`gateway_start_wait_seconds`，重启日志保存为独立的
`stdout.restart_01.log`。开始发送恢复阶段数据前，脚本会从新日志重新核对速率及上下行
块数；未重新取得本轮NS配置则判为恢复失败。脚本结束时仍沿用原受管模式清理流程，停止
本轮启动的测试进程。非4的整数倍包数按向上取整计算25%和50%触发点。

中断窗口内仍使用原有“双网关均收到”的严格逐包判定，因此该阶段通常显示失败，这是
预期故障证据；终端若仍从存活网关收到ACK，不会因为双网关判定失败而重复发送。默认
20包时有5包处于中断窗口，理想严格通过率为15/20，即75%。报告按
`before_interruption`、`gateway_stopped`和`after_restart`三个阶段分别统计，恢复阶段应
重新满足双网关接收和唯一强RSSI网关ACK条件。成功重启会建立新的广播连续性区段，
跨停机窗口的TDD/CFO差值记为`continuity_reset`而不算跳变；重启后区段内部仍严格检测。

`--location-description`会写入结果目录名和报告，可填写“重叠覆盖区域-点位A”、
“靠近网关04-距离1米”等现场说明。包含“靠近网关04/05”的说明可自动推断期望
`bcnbits`，命令行显式传入的`--expected-bcnbits`优先。

终端广播格式按`RX_TDD:<tdd>,<rssi>,<snr>,<cfo>,<bcnbits>`解析。TDD连续性和CFO
跳变按相同`bcnbits`的相邻样本分别计算，避免重叠区两个网关广播交错造成误判；CFO
阈值沿用终端上报原始数值单位。重叠区允许`bcnbits=1/2`切换并记录切换详情，靠近指定
网关时出现另一`bcnbits`则判为异常。缺少第五字段的旧格式仍会保留，但广播检查不通过。

结果目录格式为
`R<速率>_TDD<超帧>_UL<长度>_DL<长度>_<位置说明>_<时间>`，其中包含：

- `report.md`：总体结论、双网关收发统计和广播统计；
- `summary.json`：逐包、逐次尝试及所有跳变明细；
- `packet_summary.csv`：20个逻辑包的尝试次数和双网关收发结果；
- `broadcast_samples.csv`：每条广播的TDD、RSSI、SNR、CFO和`bcnbits`原始统计；
- `events.log`及`packet_XX/attempt_XX/`：终端、MQTT和双网关原始证据。
