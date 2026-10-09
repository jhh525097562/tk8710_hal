# 开发验证记录

## 2026-10-08：自动检查并启动 mqtt_bridge

- 电脑脚本在终端配置前及网关测试程序启动前检查 mqtt_bridge。已有进程不重启；缺失时以 /userdata 为工作目录，nohup后台启动 /userdata/mqtt_bridge，并连续两次确认同一PID存活（最多5次检查）。
- 缺少可执行文件、启动后退出或反复更换PID时停止测试。启动日志位于本次远端目录 mqtt_bridge_startup.log，下载固定长度快照；测试结束保留 mqtt_bridge 运行。
- 启动确认只证明进程存活，GPS、NS配置和广播载荷仍须通过原有就绪检查。
- 48项回归测试通过；随后加入持续写入日志下载用例，7项MQTT启动/下载专项测试全部通过，启用ResourceWarning错误检查。本次未连接设备验证自动启动，不需要重新编译网关C程序。

## 2026-10-08：按用户要求恢复完整就绪门槛（当前行为）

- 已撤回“仅GPS就绪即开始计时”：需要GPS/PPS健康、NS配置已收到并校验通过、广播载荷有效且S3至少推进2次，再经过稳定期输出POINT_START。广播状态获取失败仍作为故障处理。
- 保留FREQ_STATUS和每10秒等待提示，区分等待GPS、NS配置、参数校验、广播载荷和时隙推进。dataLen=0时继续等待。
- 实际GwFreqPoll主机桩测试验证每个条件缺失均不启动，全部满足才ready=1；42项Python/C调度器回归测试全部通过。RK3506目标重新编译、链接成功，新产物SHA-256：`d457e8268c75002a2488d211cca20ed95497c1ee23e32b596a55641cc7ebfeaa`。
- 未修改生产网关或共享GPS模块，未启动设备测试。下次运行脚本会上传新产物，正在运行的旧进程需先正常退出。

## 2026-10-08：GPS就绪即计时及等待状态展示（历史版本，已撤回）

- 根据用户要求，硬件初始化成功后仅以GPS/PPS健康作为开始计时的就绪条件，仍保留配置的稳定等待。NS配置消息是否已收到、广播载荷有效性、初始S3数量不再阻止开始计时；dataLen=0仅作为诊断信息。明确NS参数冲突、GPS失步、PPS帧检查故障以及运行中IRQ停滞保护仍有效。
- 测试专属FREQ_STATUS输出初始化阶段和GPS/NS/广播/S3状态，独立于TRM日志等级和输出位置。电脑端每10秒显示最后已知等待阶段；只接收POINT_START作为计时边界，不以GPS日志文本或状态快照替代。
- 已对实际GwFreqPoll执行主机桩测试：NS未收到、广播valid=0、S3=0且GPS/PPS健康时返回ready=1；广播状态读取失败也不阻塞；GPS异常、PPS帧异常、明确NS冲突和IRQ长期停滞仍返回故障。
- 42项Python/C调度器离线模拟测试全部通过，无跳过，ResourceWarning作为错误。RK3506完整目标交叉构建通过，无warning/error。
- 新产物 `build_freq_test/tk8710_gw_freq_test` 为324272字节，SHA-256 `4f38c207d47ad1334865f92800ab8d4fc0dd6068d5170033e0236619189f3220`。本次未启动新的设备测试，未修改生产网关或共享GPS模块。
- 此行为变更需要使用新二进制及更新后的电脑脚本（含progress.py）。重新运行脚本会上传新产物，已经运行中的旧进程不会自动更新。

## 2026-10-08：TRX BCN / 广播统计更新

- 终端配置在 `AT+PRINTMODE=MAC,1` 后增加 `AT+PRINTMODE=TRX,1`，检查返回，仍只执行一次 JOIN。
- 新增 `trx.py`：BCN 时隙0与广播时隙1分别按 s/f 统计；其他数据时隙单列。BCN失败记录不将data当作有效bcnbits，广播失败且没有RX_TDD时仍能统计。
- 全局、逐频差汇总及 HTML/CSV 增加明确接收成功/失败数和比例；保留原始行、时间、TRX帧号、时隙、质量字段及错误记录。旧 RX_TDD 日志和无时间戳附件兼容。
- 附件固化为 `fixtures/terminal_trx_sample.txt`，回放核对：BCN成功29/失败0，广播成功30/失败0，时隙3失败28，RX_TDD30条。缺少的BCN记录不推定为失败。
- 37项离线/模拟测试全部通过（WSL、C调度器mock、ResourceWarning作为错误，无跳过）。新增覆盖真实附件、BCN/广播失败、集合外bcnbits、十六进制、旧日志、有效窗口过滤、无时间戳导出、配置顺序、串口分片。
- 样例报告：`remote_logs/bcn_frequency/trx_sample_20261008/report.html`，已核对生成的BCN图表。仅修改电脑端采集分析及文档，无网关C代码改动；本次未连接设备执行新一轮射频测试。

日期：2026-09-24。

## 实现范围

- 独立测试主程序 `test/example/tk8710_gw_freq_test.c`、硬件运行支持 `tk8710_gw_freq_runtime.c/.h`。
- Python 交互配置、SSH 上传运行、终端只读发现、AT 配置与首次入网、连续采集、分频点分析和报告。
- 项目构建脚本注册测试目标，支持独立 `build_freq_test` 输出目录。
- 用户明确要求的生产 `test/example/tk8710_gw.c` 无 Git 差异，没有被测试目标编译或包含。

## 已执行检查

| 检查 | 结果 | 证明范围 |
|---|---|---|
| RK3506 SDK 交叉构建 | 通过，构建日志无 warning/error | ARM 编译及可执行链接 |
| Python + C mock 联合离线测试 | 24 项通过，无跳过 | 解析、统计、串口模拟、SSH事件模拟、C调度器模拟 |
| ResourceWarning 作为错误 | 已启用 | 测试覆盖路径中的资源清理 |
| 历史串口日志回放 | 178 条、CFO变化83次、bcnbits变化79次 | 与前期人工核对及脚本统计一致 |
| 入网后样例窗口 | 176 条、推算缺失22条 | TDD循环与350ms时间栅格交叉一致 |
| 参数 dry-run | 通过，-1000..1000/100 共21点 | 不连接设备的计划生成 |
| 独立代码审查 | 已修复必修项 | 速率编码、IRQ/IPC清理、空窗口、采集错误上报 |

构建命令：

```bash
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup
TK8710_BUILD_DIR=build_freq_test bash cmake/build_rk3506.sh tk8710_gw_freq_test
```

首次离线构建产物 `build_freq_test/tk8710_gw_freq_test`，319708 字节，ARM ELF 32-bit EABI5，动态链接；后续板端修正后的产物以板端验证报告为准。

SHA-256：`8528450619d4120bf70c2ca0657bb73a72e7ab4467217ea21165a2bef5ee38fa`。

联合测试命令（WSL）：

```bash
gcc -std=c99 -Wall -Wextra -Werror -Iinc -Itest/example \
  test/example/tk8710_gw_freq_test.c test/example/test_gw_freq_runtime_mock.c \
  -o /tmp/test_gw_freq_scheduler
FREQ_SCHEDULER_MOCK=/tmp/test_gw_freq_scheduler python3 -W error::ResourceWarning \
  -m unittest discover -s tools/bcn_frequency_test -p 'test_*.py' -v
```

模拟场景包含有符号步进/反向扫频、端点校验、GPS与配置失败、就绪后计时、控制端EOF、NS速率转换、bcnbits错误、整圈缺失与重复序号、跨日、掉线重入网、零频差无法区分、串口分片与命令交织、采集线程故障、事件不完整/乱序及有效时长不足。

## 板端验证边界

首次离线开发阶段没有连接网关或串口。随后在用户指定的 192.168.100.231 完成三点板端流程验证，并修复 TX DC 启动兼容、GPIO 释放及 libgpiod time64 ABI 问题，详情见[板端验证报告](board_validation_192.168.100.231_20260924.md)。尚未验证终端接收和两网关实际射频频差；板上存在 RF7 禁用及零 TX DC 条件。

第一次运行建议只设一个点或三个点，使用匹配 A/NS 的 TDD、单速率和包块配置。`run_status.completed=true` 只代表流程完整完成；射频接收正常与否应结合 samples、bcnbits错误、CFO分布、日志缺失和网络事件判断。

CFO逐条匹配使用观测分组作为参考，未提供独立的B单网关CFO校准或误差容限，因此不输出独立逐帧解码正确率。无接收窗口明确标注无样本、缺失未知，不认定为通过。

## 完整配置脚本执行

2026-09-24 按用户确认的 NS RF 基准 496808000 Hz、RF gain=46，在 COM6 与 192.168.100.231 上完成 -1000～+1000 Hz / 100 Hz、每点 60 秒的 21 点测试。运行约 30 分 35 秒，启动阶段完整重配重试 16 次，21 个有效窗口均完成；改进启动恢复和 USB 时间量化周期估计后，离线/模拟回归共 28 项通过。

原始 RX_TDD 5066 条，有效窗口 3416 条，均为 bcnbits=0，没有 bcnbits=1。终端日志周期约 350 ms，而 B 配置帧周期为 300 ms，不能据此判定预期双网关同步接收通过。详见[完整测试报告](../../remote_logs/bcn_frequency/20260924_132645_765661/analysis/测试报告.md)及[图表报告](../../remote_logs/bcn_frequency/20260924_132645_765661/analysis/report.html)。
