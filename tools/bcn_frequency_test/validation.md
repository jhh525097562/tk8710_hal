# 开发验证记录

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
