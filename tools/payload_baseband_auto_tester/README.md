# 载荷基带软件3.6自动测试工具

该工具自动执行《载荷基带软件测试报告_v0.1.md》3.6章节的RF-01～RF-13测试。它同时采集SPI1遥控/遥测、SPI2数传、TMS570调试串口、终端串口和NS MQTT证据。

## 硬件连接


| 接口               | 用途                              | 固定标识                   |
| ------------------ | --------------------------------- | -------------------------- |
| JTool SPI1         | 遥控、144字节遥测                 | `9a85a46b0453`，SPI mode 0 |
| JTool SPI2         | 512字节数传                       | `9a85B9a50453`，SPI mode 2 |
| TMS570 SCI/LIN串口 | `RC OK/ERR`、`AT+TM`、`AT+FPGATM` | 1000000 8N1，自动识别      |
| 业务终端串口       | 入网、ACK业务                     | 115200 8N1，COM14优先      |

JTool只能作为SPI master。使用PC接收SPI2数传时，TMS570固件必须启用`DATA_TRANSFER_SPI2_SLAVE_TEST`。正式的“TMS570 SPI2 master”路径不能由当前JTool DLL模拟slave或被动抓取。

新版固件将遥控`CMD_09`定义为数传开关：Byte3=`1`持续开启数传，Byte3=`0`停止数传。工具会在每次SPI2采集前发送`CMD_09=1`，在空闲超时或采集异常后通过`finally`路径发送`CMD_09=0`，再用`AT+FPGATM`复核`active=0`。

建议使用以下命令构建并烧录测试固件：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -Image Combined -DataTransferSpi2SlaveTest
```

不要同时启动旧版`combined_spi_gui_tester.exe`，两个程序不能同时占用同一JTool。

## 启动

首次构建SPI桥接程序：

```powershell
cd D:\ti_workspace\tms570ls3137_halcogen_base_570RAM_add\tools\payload_baseband_auto_tester
powershell -ExecutionPolicy Bypass -File .\build-bridge.ps1
python -m pip install -r .\requirements.txt
```

图形界面：

```powershell
.\启动工具.bat
```

命令行无人值守：

```powershell
$env:PAYLOAD_TEST_MQTT_PASSWORD = '<NS密码>'
python .\app.py --config .\config.example.json --run-all
python .\app.py --config .\config.example.json --cases RF-01,RF-02,RF-04,RF-05
```

无硬件模拟和自测：

```powershell
python .\app.py --simulate
python .\app.py --selftest
```

MQTT密码不会写入配置文件或测试报告。可在GUI临时输入，或通过`PAYLOAD_TEST_MQTT_PASSWORD`环境变量传入。

## 串口自动识别

工具严格按以下顺序扫描，避免误把570当终端复位：

1. 按`tms570_baudrate`（当前默认1000000）被动匹配`Satellite payload control ready`等570启动打印。
2. 按570波特率发送`AT+FPGATM`，同时匹配`FPGA_TM`、`FPGA_PARAM`和`DT head`确认570。
3. 排除570串口后，按`terminal_baudrate`（默认115200）向剩余端口发送`AT+RST`。
4. 匹配`MAC AT CMD!`或`TurMass`确认业务终端。

GUI允许覆盖自动结果。配置中填写`tms570_port`或`terminal_ports`后，将直接使用人工配置，不再主动扫描和复位端口。

## 自动判据

- RF-01/02/04/05要求570串口存在；缺失时为`BLOCKED`。
- RF-02按当前固件固定504～508 MHz、125 kHz步进、33点一轮执行，默认等待两轮结果并校验SPI2导出的完整频点记录。
- RF-03会进入模式4并检查软件状态，但没有频谱仪时固定为`SKIP`。
- RF-06依次进入模式A/B/C，并要求每次均有板端状态和连续两帧SPI1遥测确认。
- RF-10～13少于16个已注册终端时为`SKIP`。
- RF-13要求同一轮RF-12先通过；发送和健康检查间隔分别由`rf13_send_interval_s`、`rf13_health_interval_s`控制。
- 每个用例由`case_timeout_s`限制总执行时间，当前为120秒。RF-13在该配置下只形成短时稳定性证据，报告会明确标注“不替代24小时稳定性测试”。
- RF-07～12每个终端最多尝试3次；`+TXSTATUS:7`和相同DevEUI/端口/payload的MQTT上行同时出现才算成功。
- RF-07～13先完成570频率、速率和模式配置，并用连续两帧SPI1遥测确认生效；之后才通过MQTT重建地面站网关。
- 网关时隙结构固定为第二种`BCN+UP+DOWN`（MQTT API字段`slot_cfg_num=2`），重建后必须查询回读一致。
- SPI2的0x02首用户记录按终端ID、速率和接收测量频率判定；载荷配置中心频率仍须严格等于477.8 MHz，8710接收测量频率按当前速率2实板结果允许默认±250 kHz偏差（可通过`user_frequency_tolerance_hz`收紧）。

模式0、4、5、6结束后会自动切回模式3。正式用例开始前若发现历史待传数据，工具会先隔离旧记录，避免旧generation混入本轮结果：不超过`preexisting_archive_limit_bytes`（默认64 KiB）时下传并保存为`preexisting`证据；超过上限时发送`AT+DTCLEAR`并复核`active=0`后再测试。RF-01结束以及RF-02、RF-05、RF-07～RF-09开始前还会再次清空并复核数传队列；清理后的实时用户记录允许让`pending`重新增长，但`active`必须为0。扫频和采数记录必须属于各自用例开始后的generation，旧记录不能用于通过判定。

## 输出

每次运行输出到：

```text
results/YYYY-MM-DD/PAYLOAD_YYYYMMDD_HHMMSS/
```

主要文件：

- `summary.json`：机器可读的用例结论和环境信息。
- `report.xlsx`：测试汇总。
- `载荷基带软件测试报告_v0.1_<run_id>.md`：仅填写3.6章节的报告副本。
- `events.jsonl`：完整时间线。
- `serial_<COM>.log`、`mqtt.log`、`spi_bridge.log`：各接口日志。
- `data_transfer_<case>.bin/json`：SPI2原始帧和解析记录。

原始`docs/载荷基带软件测试报告_v0.1.md`不会被修改。`SKIP/BLOCKED`会在“真实结果”列写入原因，“判定”列保持空白。

## 当前边界

- 工具不会自动新建或修改NS终端档案；终端未登记时对应业务用例为`BLOCKED`。
- 网关`0599999999999999`会按当前测试速率删除后重建，测试结束保留载荷速率2（NS速率3）配置。
- `slotConfig`和`txPower`会保留在配置及遥测证据中，但不作为TK8710硬件实际生效的通过条件。
- RF-03的射频频率和功率必须后续接入频谱仪才能形成PASS/FAILED结论。
