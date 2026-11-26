# 唤醒信号遍历测试工具

通过SSH运行RK3506发射程序，通过电脑串口配置TK8620。当前流程为定时唤醒自动恢复，
每个模式/ID最多尝试3次，不再等待人工复位。频率默认492300000 Hz，可在界面修改。

## 界面启动

双击本目录 `start.cmd`，或在仓库根目录执行：

```powershell
python -m pip install -r tools/wakeup_signal_test/requirements.txt
python tools/wakeup_signal_test/gui.py
```

界面可设置SSH地址/端口/用户名/密码、终端串口/波特率、模式、ID、频率、
发射时长、接收时长、TX增益、重复次数和“定时唤醒 ms”。默认定时周期5000 ms。
ID可填 `0-255` 或 `0,31,32,47,128`，模式可填 `3` 或 `1,2,3`。
“重复次数”表示独立重复测量次数；每一次独立测量内部仍最多尝试3次。

点击开始后会把SSH地址/端口/用户名及串口/波特率保存到settings.local.json，
重开界面自动恢复，避免修改IP后又回到示例地址。此文件Git忽略，不保存密码。
日志会显示SSH连接、占用检查、校准准备、上传和SHA256核对阶段；超时提示包含阶段及目标地址。

密码框隐藏显示；初始值可从环境变量TK8710_SSH_PASSWORD或本机password.local.json读取。
password.local.json被Git忽略，密码不写入运行配置、结果或命令行。
新SSH地址首次连接自动登记指纹到known_hosts.local，记录地址、算法、SHA256；
已有地址指纹变化时停止，不覆盖原记录。不同端口作为独立SSH身份。

自动串口检测会逐个打开串口发送一次 `AT+RST\r\n`，等待默认3秒复位日志。
唯一候选自动选择，多个候选需选择；占用端口跳过。探测记录保存到results/serial_probe_*.json。
这是开始测试前的端口识别，重试过程中只用定时唤醒恢复，不发送AT+RST。
仅有命令回显或乱码不算候选；“有日志”不单独证明设备型号，测试前仍需有效AT查询。

## 新的终端配置指令

ID47、模式3、定时周期5000ms、接收时长1000ms、492.3MHz的配置为：

```text
AT+WORKMODE=21
AT+WAKEUPCFG=8,5000;9,47,1000,492300000,3
AT+ENTERSLEEP
```

`8,<timer_wakeup_ms>` 设置定时唤醒周期，单位ms。
分号后为 `<wakeup_cfg_first>,<ID>,<receiver_duration_ms>,<frequency_hz>,<mode>`，
第一项默认9，其余来自界面。每次尝试都重新发送完整指令并检查AT_OK。

## 每项测试如何执行

1. 查询AT+FREQ?，确认终端已可操作；没有响应时自动等待定时恢复。
2. 整轮首次启动 `Test8710RFTxTone 6 <frequency_hz> 46`，保持同一进程跨ID复用。
3. 发WORKMODE、组合WAKEUPCFG，确认查询可用，再发ENTERSLEEP。
4. 以发送ENTERSLEEP前的单调时间为定时起点，短暂稳定后查询两次确认无响应。
5. SSH输入u和 `<mode> <duration_ms> <id>`，开始对应的无线唤醒信号发送。
6. 在定时截止前的有效窗口内轮询AT+FREQ?，收到+FREQ和AT_OK才判本次PASS。
7. 成功后等待Driver完成；失败到达判定边界后停止发射，自动等待定时唤醒及有效AT响应。
8. 未通过则重新配置同一ID再试，最多3次；任一次通过即结束该项，3次均未通过则记录
   WAKE_TIMEOUT并继续下一ID。整轮结束才q退出。

默认保护时间timer_guard_ms=200。5000ms定时周期下，**从ENTERSLEEP起4800ms后**的响应
不计为无线成功。查询开始在窗口内但返回时跨过截止点，同样不计PASS。
截图中的P2P AT CMD!/compile time等启动文字和定时周期之后的AT响应只作为恢复证据，
不能作为无线成功。原始字节和启动行仍完整记录。

终端没有提供明确唤醒原因字段，所以这里采用保守时间窗口判据，不能仅凭串口日志
证明唤醒的物理原因。接近定时边界的真实无线唤醒也可能被归为未检测到成功。
如需更长的无线判断窗口，增加定时周期；不沿用旧流程1.5秒稳定加长间隔预检。

默认发送时长也为5000ms，休眠确认和SSH输入会消耗部分定时周期。因此失败项的实际
发射窗口可能短于配置时长：工具会在保护边界主动p取消，避免定时恢复后仍把它算作无线唤醒。
成功项按Driver自身计时，在到期后的第一个S0正常完成。停止按钮或错误也会尝试取消再q退出。
定时截止后再等待recovery_timeout（默认8秒）仍无有效查询响应，视为终端/串口/固件异常，
保存并停止整个测试；不会在终端尚未恢复时盲目配置下一个ID，也不弹人工复位继续按钮。

## ARM程序和校准

需使用支持WAKEUP_SESSION_V2的ARM程序。WSL在仓库根目录编译：

```bash
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup
./cmake/build_rk3506.sh Test8710RFTxTone
```

local_bin指向build_rk3506/Test8710RFTxTone，不能选择.c源码。
工具自动上传、核对SHA256，将该板 `/userdata/TxDC/txadc.txt` 复制到独立运行目录，
保持Driver相对路径校准加载条件。可用remote_txadc指定板端其他绝对路径；缺少则停止。
不会自动关闭其他网关进程/守护服务；若硬件被占用会停止并提示。
Driver接口说明见 `../../Document/Driver唤醒发送接口说明.md`。

## 命令行

```powershell
Copy-Item tools/wakeup_signal_test/config.example.json tools/wakeup_signal_test/config.json
python tools/wakeup_signal_test/run.py --config tools/wakeup_signal_test/config.json --plan
python tools/wakeup_signal_test/run.py --config tools/wakeup_signal_test/config.json
```

SSH密码可提前设置到环境变量。PowerShell 7：

```powershell
$env:TK8710_SSH_PASSWORD = Read-Host 'SSH 密码' -MaskInput
```

Windows PowerShell 5.1：

```powershell
$secret = Read-Host 'SSH 密码' -AsSecureString
$env:TK8710_SSH_PASSWORD = [System.Net.NetworkCredential]::new('', $secret).Password
```

新增/重要配置：

| 配置项 | 含义 |
|---|---|
| timer_wakeup_ms | 定时唤醒周期，默认5000ms，界面可修改 |
| timer_guard_ms | 定时判定保护区，默认200ms，JSON可修改 |
| recovery_timeout | 定时周期之后等待串口恢复的额外秒数，默认8 |
| frequency_hz | 同时用于终端接收和网关发送，默认492300000 |
| duration_ms | 网关请求发射时长，默认5000ms |
| receiver_duration_ms | 组合指令分号后接收时长，默认1000ms |
| wakeup_cfg_first | 分号后第一参数，默认9 |
| sleep_settle | 休眠后的稳定等待，默认0.2秒 |
| serial_timeout / poll_interval | 查询超时/间隔，默认0.7/0.2秒 |
| wake_grace | 发送窗口后的额外判定时间，始终受定时保护边界限制 |
| startup_timeout / stop_timeout | SSH程序启动及Driver完成/退出超时，默认60/30秒 |
| repeats | 独立重复测量次数，每次最多3次尝试 |

已取消旧recovery人工恢复选项及interactive-stdin参数。
旧配置需补充timer_wakeup_ms、timer_guard_ms、recovery_timeout，并将sleep_settle改为0.2。
旧运行目录与新指令/判据不同，建议新建运行；不直接把旧结果混入新测试。

## 结果和断点继续

结果在本目录results/时间_随机号/：

- results.jsonl：每项最终结果，嵌套保存各次attempts及attempt_count。
- results.csv：每个模式/ID/独立重复一次的最终结果和尝试次数。
- attempts.csv：逐次尝试的明细，包含尝试序号、结果及定时周期。
- journal_*.jsonl：每次attempt_start/attempt_result、SSH输出、串口原始字节Base64及解析行。
- config.json / summary.json：配置快照/最终项数统计；不含密码。
- remote_logs/：收集本次板端日志，不删除板端文件。

一次PASS表示3次内至少一次在有效窗口收到响应；WAKE_TIMEOUT表示3次均未通过。
不能把123/129等旧单次测量结果与新最多三次的通过比例直接比较。
未完成项和软件/串口错误与无线未唤醒分开记录，不能当作RF丢包率。
query_latency_s包含查询周期、命令处理及SSH延时，不是精确芯片唤醒延时。

```powershell
python tools/wakeup_signal_test/run.py --config tools/wakeup_signal_test/config.json --resume tools/wakeup_signal_test/results/原运行目录
```

恢复时配置须一致，已完成PASS或三次失败项跳过，ERROR/未完成项重测。
界面“从结果目录继续”会加载原参数，使用当前密码框的密码。
每次尝试一结束立即写入journal，程序意外中断也可查到已完成尝试的证据。
退出码0=全项通过，1=完成但有三次失败项，2=异常/停止。

## 离线验证

```powershell
python -m unittest discover -s tools/wakeup_signal_test -p 'test_*.py' -v
```

本次定时唤醒改造通过离线流程测试和GUI检查，尚未进行新版AT指令的板端联合测试。
