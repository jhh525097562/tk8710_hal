# 同频同步组网：BCN CFO 与 bcnbits 自动测试

本工具让网关 A 固定运行，网关 B 按指定中心频差逐点运行，并连续采集终端串口，生成分点日志及汇总报告。

生产程序 `src/gateway/tk8710_gw.c` 保持不变。新增的 `tk8710_gw_freq_test.c` 是测试主程序，`tk8710_gw_freq_runtime.c` 参考生产网关实现 RF、TX DC、GPS/PPS、TRM 和 NS/IPC 初始化。新目标不编译或包含生产网关主程序。

## 1. 测试前准备

1. 由操作者配置并启动网关 A：固定中心频率、bcnbits、GPS 同步、单速率及 TDD 参数，保持整个测试期间运行。
2. 网关 B 保留现有 NS/MQTT/IPC 通信服务，NS 上的 `gps_enable=1`、TDD 数量、速率、上下行包块数须与本次输入一致；NS 广播数据仍由现有流程提供。两台的时隙布局、TDD 周期必须一致。
3. 停止 B 上普通 `tk8710_gw` 及它的自动拉起任务，确保没有其他程序占用 TK8710/GPS 资源。工具会检查常见冲突程序并拒绝并行占用，不会任意杀死服务。测试后由操作者恢复原网关服务。测试程序、频差及 bcnbits 不写回 NS 配置。
4. 连接一个终端串口，关闭 SSCOM 等占用串口的软件。工具默认用只读 `AT+DEVEUI?` 探测；恰好一个匹配才自动选择，不发送 `AT+RST`。零个或多个匹配时使用 `--serial-port COM14` 指定。
5. 首次使用 SSH 时，在本机终端执行 `ssh root@网关B_IP` 核对并保存主机指纹。本工具使用已有 known_hosts；可在 JSON 中指定 `known_hosts` 文件，拒绝未信任的主机密钥。密码通过交互输入，不保存到配置或日志。

上述准备针对实际运行。离线分析、参数检查和模拟测试不连接设备。

## 2. 构建网关测试程序

在 WSL Ubuntu-22.04 中：

```bash
cd /mnt/d/CodexWorkSpace/tk8710_hal_v1.6
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup
TK8710_BUILD_DIR=build_freq_test bash cmake/build_rk3506.sh tk8710_gw_freq_test
```

产物：`build_freq_test/tk8710_gw_freq_test`。新增构建目录不清理已有 `build_rk3506`，构建脚本也不再删除整个输出目录。

本目标使用本项目的 RK3506 SDK 和 `lib/libipc_smp.so`，脚本会一并上传 IPC 动态库。板端还须有对应固件运行环境的 `libgpiod`、C 运行库等。

测试目标为 RK3506 port 单独生成 `tk8710_freq_port_time64.o`，设置 `_TIME_BITS=64` 和 `_FILE_OFFSET_BITS=64`，匹配 SDK/板端 libgpiod 的时间结构布局。不要绕过构建脚本直接链接普通 time32 port 对象：中断等待可能返回 EINVAL，事件读取也存在结构大小不匹配风险。该修正只作用于新测试目标，未修改共享 port 源码。

## 3. 一键运行

在仓库根目录的 PowerShell 中安装已有工具使用的 Python 依赖：

```powershell
python -m pip install -r tools/bcn_frequency_test/requirements.txt
python tools/bcn_frequency_test/run.py --password
```

也可以双击 `tools/bcn_frequency_test/start.cmd`。

脚本提示输入 B 的 IP、终端主次信道、测试速率、A 的中心频率、两台 bcnbits、NS 的 TDD/包块数、起止频差、步进、每点时长和波特率。默认示例值不代表当前设备配置，须按实际填写。

重复测试推荐复制并修改 `config.example.json`：

```powershell
# 仅检查计划，不连接网关、不打开串口。
python tools/bcn_frequency_test/run.py --config tools/bcn_frequency_test/config.example.json --dry-run

# 使用密码登录，指定串口，其他参数从配置读取。
python tools/bcn_frequency_test/run.py --config tools/bcn_frequency_test/config.example.json --serial-port COM14 --password
```

`--password` 不带密码值；不指定时使用 SSH agent/密钥。相对 `binary` 路径相对于仓库根目录。默认本地输出为 `remote_logs/bcn_frequency/时间戳/`，可用 `--output` 指定一个尚不存在的目录。

终端配置顺序：

```text
AT+DEVEUI=0000000000000003
AT+DEVMODE=2
AT+PRINTMODE=MAC,1
AT+FREQCFG=<主信道号>,<次信道号>
AT+RATE=<测试速率>
AT+JOIN=0
```

每条配置检查 AT 返回；只发送一次 JOIN，以 `+NWKINFO:4` 为成功。失败会保存日志并退出，不自动重置或重复 JOIN。终端先通过 A 入网，再采集 A 单独运行的基线，确认有广播且 bcnbits 与 A 参数一致后才启动 B。扫频期间终端持续运行，自动掉线/重入网只记录、不干预。

## 4. 网关 B 的执行与退出

每点执行：停止 IPC 处理 → 停用并等待 IRQ 线程退出 → 停止旧状态机并完整释放 HAL/GPIO/SPI 资源 → GPS/PPS 就绪检查 → RF/芯片校准和时隙重配 → 启动外部同步 → 恢复 IPC 并校验 NS 参数 → 收到广播数据且 S3 推进 → 稳定等待 → POINT_START → 有效运行 → POINT_END。

- 用户输入速率为 5～11 或 18；程序转换为 NS 的 0～7 索引，避免不同编号体系混用。
- 频率为 `A中心频率 + 本点频差`，同时配置 RF 与各时隙中心频率；B 的 bcnbits 固定为输入值。
- 起止端点均包含，支持正、负步进，必须精确到达终点；不支持步进为 0。若只测一点，起止相同且步进非零即可。
- 每点时长使用网关单调时钟，从就绪后的 POINT_START 开始。GPS 等待、初始校准、切频及稳定等待不计入。
- 有效计时前若 PPS 帧/边沿校验失败，默认最多完整重配 2 次（`startup_retries` / `--startup-retries`，范围 0～5），输出 POINT_RETRY。重试不重置该点总就绪超时，也不重置终端；报告记录次数。有效计时后的同步异常仍立即失败，不重试或拼接计时。此机制是有限启动恢复，不代表 PPS 首帧异常根因已解决。
- GPS 每约 1 秒轮询，一次观测到异常即中止，当前点作废，不切换为本地同步。瞬时异常能否被捕获受轮询周期限制。
- 最多等待 `ready_timeout` 秒就绪。底层 GPS/校准调用有自身的等待过程，本程序的超时检查在这些调用返回后进行；电脑端另设总时限。IRQ 无推进也会使测试失败。
- 日志中的 `rf_register` / `nominal_rf_hz` 来自 SX1255 频率步进 30.517578125 Hz 的寄存器量化计算，不是射频仪表实测值。小于量化步进的配置可能落到相同寄存器值。
- 全部点完成才输出 SWEEP_DONE 并退出。故障、中断或电脑停止会输出失败事件并清理；Ctrl+C 会请求本次进程停止并向精确匹配本次上传路径的进程发送 SIGTERM，不使用 killall/SIGKILL。
- 远端文件保存在唯一的 `/userdata/bcn_freq_test/运行ID/`，下载后保留，不自动删除。原 NS 配置和生产二进制不被覆盖；退出时 B 停止发射，不自动恢复普通服务。

必要时可直接在 B 上运行（需要现有 NS 服务提供匹配配置及广播）：

```bash
LD_LIBRARY_PATH=./lib:/userdata/lib ./tk8710_gw_freq_test \
  --base-hz 496800000 --start-hz -1000 --end-hz 1000 --step-hz 100 \
  --hold-seconds 60 --bcnbits 1 --rate 8 --tdd-count 10 \
  --ul-blocks 2 --dl-blocks 2 --work-dir .
```

TX DC 先从当前板 RF RAM 读取。读取失败或无已存值时沿用原网关行为：保留已复制的 `/userdata/TxDC/txadc.txt`，没有文件则使用芯片初始化配置的默认 DC 值。启动日志明确记录 `TXDC source`；默认值运行不能作为 TX DC 校准验证通过的证据。

## 5. 输出与统计解释

```text
运行目录/
  config.json                 参数快照，不含密码
  run_status.json             完成状态、错误、二进制 SHA-256
  journal.jsonl               原始串口字节(base64)、终端行、配置命令、网关事件
  gateway.log                网关标准输出及错误输出
  remote/                    本次远端目录副本及驱动日志
  analysis/
    report.html              汇总报告与 CFO/bcnbits 共用时间轴图
    summary.json / points.csv
    samples.csv / transitions.csv / events.csv
    cfo_bcnbits_matching.csv / bcnbits_errors.csv / gaps.csv
    terminal_raw.log / transition_terminal.log
    point_0000_-1000Hz/terminal.log
```

`RX_TDD:TDD序号,广播RSSI,广播SNR,CFO_Hz,bcnbits` 按五字段解析。解析失败和集合外 bcnbits 分别记录。bcnbits 错误占比只以收到且可解析的记录为分母。

频点边界使用电脑接收 POINT_START/END 的时间，默认两端各排除 500 ms；过渡日志完整保留。此保护区只降低 SSH/串口延迟污染，不是硬件级时钟对齐保证。未结束、失败频点不进入有效窗口统计。

TDD 从 1 到 N 循环。缺失估计同时检查序号与广播周期；周期可用 `frame_period_ms` 明确输入，0 表示从至少 3 个连续序号间隔推断，剔除远离中心分布的间隔后取均值，降低 USB 接收时间量化带来的中位数偏差。时间与序号不一致标为未知，不强行计数；整圈缺失结合时间判断。20% 周期容差只用于串口时间一致性，不是 CFO 阈值。

缺失数量是相邻已收到记录之间的下界，不含无法定位的窗口首尾丢失，不直接等同射频丢包率。报告另列最大无日志时长（含窗口首尾）。整点无接收标为 no_samples，缺失数量为 null；不能将其判为通过。

CFO 不设置异常阈值。报告保存每次变化量、bcnbits 切换、两组 CFO 分布、A 相对单网关基线的漂移、B-A 中位数差及其绝对值相对配置频差的残差。逐条匹配以本点观测分组的中位数为参考，输出符合观测分组、疑似跨组、无法区分或参考不足；该关联不是独立解码真值。零配置频差及重合中心不强判来源，集合外 bcnbits 则直接计错。

网络事件中 `NWKINFO:4` 为入网成功；沿用现有 WAN 工具的 `NWKINFO:6` 作为掉线状态，已入网后再次出现 2/3 记录为重入网开始。记录重新入网次数、事件频点和恢复时长。没有网络状态只出现接收空档时，不直接判为掉线；新终端固件仍需实测核对该状态约定。

## 6. 离线分析和回归

```powershell
python tools/bcn_frequency_test/analyze.py docs/bcn_frequency_offset_sample_20260924/source.log --output remote_logs/bcn_sample --tdd-count 10 --bits-a 0 --bits-b 1
python tools/bcn_frequency_test/analyze.py remote_logs/bcn_frequency/运行目录/journal.jsonl --output remote_logs/bcn_reanalysis --tdd-count 10 --bits-a 0 --bits-b 1
python -W error::ResourceWarning -m unittest discover -s tools/bcn_frequency_test -p "test_*.py" -v
```

C 调度器模拟测试（WSL；与板端硬件无关）：

```bash
gcc -std=c99 -Wall -Wextra -Werror -Iinc -Itest/example \
  test/example/tk8710_gw_freq_test.c test/example/test_gw_freq_runtime_mock.c \
  -o /tmp/test_gw_freq_scheduler
FREQ_SCHEDULER_MOCK=/tmp/test_gw_freq_scheduler python3 tools/bcn_frequency_test/test_scheduler.py -v
```

历史样例期望为 178 条 RX_TDD、83 次 CFO 变化、79 次 bcnbits 变化、4 次同 bcnbits 下 CFO 变化；入网成功同一接收块起有 176 条记录、推算缺失 22 条。

编译通过、模拟测试通过不代表板端通过。首次上板建议先测单点或三个点，核对 B 的 NS 参数校验、GPS/PPS、BCN 实际频差、每点有效计时、终端日志及退出后硬件状态，再执行完整范围。

ssh root@192.168.200.106

yes

exit




cd /userdata
chmod +x tk8710_gw_freq_test

cd /userdata
chmod +x tk8710_gw_freq_test

LD_LIBRARY_PATH=/userdata/lib ./tk8710_gw_freq_test \
  --base-hz 479308000 \
  --start-hz -5000 \
  --end-hz -2000 \
  --step-hz 300 \
  --hold-seconds 60 \
  --bcnbits 1 \
  --rate 8 \
  --tdd-count 10 \
  --ul-blocks 2 \
  --dl-blocks 2 \
  --rf-gain 46 \
  --ready-timeout 600 \
  --settle-seconds 2 \
  --startup-retries 2 \
  --work-dir /userdata
