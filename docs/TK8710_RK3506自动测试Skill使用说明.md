# TK8710 RK3506 自动测试 Skill 使用说明

## 1. 目的

`tk8710-rk3506-test` 是一个用于 TK8710 RK3506 板端自动测试的 Codex Skill。它把原来需要通过 MobaXterm 手动完成的流程固化为自动化步骤：

1. 自动 SSH 连接 RK3506 主控。
2. 上传本地编译好的测试程序到远端 `/userdata`。
3. 在远端创建独立运行目录并执行测试程序。
4. 按设定时长运行，结束时发送退出指令。
5. 拉取 stdout、TRM log、driver log。
6. 自动统计关键日志并给出 pass/fail 分析依据。

适用场景：功能开发完成后，需要快速把新的 RK3506 可执行文件上传到 8710 主控板并运行验证。

## 2. Skill 位置

当前 Skill 安装在本机：

```text
C:\Users\HJH\.codex\skills\tk8710-rk3506-test
```

核心脚本：

```text
C:\Users\HJH\.codex\skills\tk8710-rk3506-test\scripts\run_rk3506_test.py
```

## 3. 默认连接配置

| 配置项 | 默认值 |
|---|---|
| SSH IP | `192.168.100.140` |
| 用户名 | `root` |
| 密码 | `123456` |
| 远端上传目录 | `/userdata` |
| 默认本地程序 | `build_rk3506\TestTRMmain` |
| 默认运行时长 | `300` 秒 |
| 正常退出输入 | `q` |
| 本地日志目录 | `remote_logs\trm_run_时间戳...` |

## 4. 推荐使用方式

后续可以直接用自然语言让 Codex 执行，例如：

```text
用 tk8710-rk3506-test 跑一下 TestTRMmain，参数是 6 20 20 20 400 126 509100000 42，运行 5 分钟后退出并分析日志。
```

如果要换 IP：

```text
用 tk8710-rk3506-test 跑一下，IP 改成 192.168.100.141，执行文件用 build_rk3506\TestTRMmain，参数是 6 20 20 20 400 126 509100000 42，运行 5 分钟后分析日志。
```

如果要换执行文件：

```text
用 tk8710-rk3506-test 跑一下，IP 是 192.168.100.140，执行文件用 build_rk3506\Test8710Slave，参数是 6 20 20 20 400 126 509100000 42，运行 300 秒后退出并分析日志。
```

如果用户名或密码也变化：

```text
用 tk8710-rk3506-test 跑一下，IP 是 192.168.100.150，用户名 root，密码 123456，上传 build_rk3506\TestTRMmain，运行参数 6 20 20 20 400 126 509100000 42，运行 300 秒，退出后分析日志。
```

## 5. 手动命令方式

也可以在工程根目录直接运行脚本：

```powershell
python C:\Users\HJH\.codex\skills\tk8710-rk3506-test\scripts\run_rk3506_test.py `
  --host 192.168.100.140 `
  --user root `
  --password 123456 `
  --local-bin build_rk3506\TestTRMmain `
  --duration 300 `
  --args 6 20 20 20 400 126 509100000 42
```

常用参数说明：

| 参数 | 说明 | 示例 |
|---|---|---|
| `--host` | RK3506 主控 IP | `192.168.100.140` |
| `--user` | SSH 用户名 | `root` |
| `--password` | SSH 密码 | `123456` |
| `--local-bin` | 本地待上传执行文件 | `build_rk3506\TestTRMmain` |
| `--remote-dir` | 远端上传/运行根目录 | `/userdata` |
| `--remote-name` | 上传后的远端文件名 | `TestTRMmain` |
| `--args` | 程序运行参数，必须放在脚本命令最后 | `6 20 20 20 400 126 509100000 42` |
| `--duration` | 运行时长，单位秒 | `300` |
| `--exit-input` | 正常退出输入 | `q` |
| `--local-log-root` | 本地日志保存根目录 | `remote_logs` |
| `--run-label` | 本次运行标签，便于区分 | `tx42` |
| `--extra-log-dir` | 额外远端日志目录，可重复传入 | `/userdata/8710log` |

带标签的例子：

```powershell
python C:\Users\HJH\.codex\skills\tk8710-rk3506-test\scripts\run_rk3506_test.py `
  --local-bin build_rk3506\TestTRMmain `
  --duration 300 `
  --run-label tx42 `
  --args 6 20 20 20 400 126 509100000 42
```

注意：`--args` 后面的内容都会原样传给被测程序。因此 `--duration`、`--run-label`、`--extra-log-dir` 等脚本参数必须写在 `--args` 前面。

`tk8710_gw` 使用 `-w /userdata` 时，程序会把 8710 文件日志写入 `/userdata/8710log`。脚本会自动识别 `-w /userdata` 或 `--work-dir /userdata`，额外下载 `/userdata/8710log` 并纳入统计。

## 6. 自动执行流程

脚本内部会按以下顺序执行：

1. 检查本地执行文件是否存在。
2. 建立 SSH 连接。
3. 在远端确认 `/userdata` 目录。
4. 上传本地执行文件到 `/userdata/<文件名>`。
5. 对远端文件执行 `chmod 755`。
6. 创建远端运行目录，例如：

```text
/userdata/trm_run_20260706_152706_tx42
```

7. 切换到该运行目录执行测试程序。
8. 本地保存 PTY stdout 到：

```text
remote_logs\trm_run_...\stdout_pty.log
```

9. 到达运行时长后发送 `q`。
10. 如果 `q` 后仍未退出，会发送 `Ctrl-C`，并在结果中标记 `forced_exit=true`。
11. 下载远端运行目录中的日志。
12. 生成 `summary.json`。

## 7. 输出文件

每次运行会在本地生成一个独立目录：

```text
remote_logs\trm_run_YYYYMMDD_HHMMSS...
```

常见文件如下：

| 文件 | 说明 |
|---|---|
| `stdout_pty.log` | 程序终端输出，包括初始化、运行过程、最终 IRQ 统计 |
| `8710log\trm_log_0.log` | TRM 文件日志 |
| `8710log\tk8710_driver_0.log` | driver 文件日志 |
| `summary.json` | 自动统计结果 |

## 8. 日志分析重点

Skill 会统计以下关键项：

| 指标 | 含义 |
|---|---|
| `[ERROR]` | 硬错误，通常优先判断异常 |
| `[WARN]` | 警告，需要结合业务场景判断 |
| `ACM calibration requested` | ACM 请求次数 |
| `ACM hidden in slot3` | ACM 是否进入 slot3 隐藏执行路径 |
| `valid=1` | ACM 至少产生一次有效校准结果 |
| `valid=0` | ACM 执行了，但没有得到有效校准结果 |
| `ACM calibration has no valid result` | ACM 明确报告无有效结果 |
| `ACM calibration request rejected` | 后续请求被拒绝，常见原因是 pending 未清除 |
| `RX users` | 收到上行用户数据 |
| `Sent N/N users` | TRM 下行响应发送结果 |
| 最终 IRQ 统计 | 程序退出前的中断处理次数和耗时 |

## 9. ACM 测试判据

ACM 类测试不要只看程序是否正常退出。需要分开判断：

| 现象 | 判断 |
|---|---|
| 程序 exit 0，且无 `[ERROR]` | 运行链路正常 |
| 有 `ACM hidden in slot3` | ACM slot3 隐藏执行路径被触发 |
| 全部是 `valid=0` | ACM 校准未成功 |
| 出现 `valid=1` | 至少有一次 ACM 有效 |
| 持续 `valid=0` 且 `retryNext=1` | ACM 一直重试但没有有效结果 |
| 出现 `request rejected, pending=1 running=0` | 上一次 ACM pending 状态未释放，后续请求被拒绝 |
| `irqAfterCalib=0x00000000` | 仅说明该字段无 IRQ 残留，不能代表 ACM 成功 |

示例结论：

```text
程序运行链路正常，exit_status=0，TRM/driver 无 ERROR；
但 ACM hidden in slot3 共 42 次，valid=0 共 42 次，valid=1 为 0，
因此本次 ACM 校准未成功。
```

## 10. 常见问题

### 10.1 找不到本地执行文件

现象：

```text
ERROR: local binary not found
```

处理：

1. 确认已完成 RK3506 编译。
2. 确认 `--local-bin` 路径正确。
3. 在工程根目录运行脚本时可以使用相对路径，例如 `build_rk3506\TestTRMmain`。

### 10.2 SSH 连接失败

处理：

1. 确认 PC 与 RK3506 网络互通。
2. 确认 IP、用户名、密码正确。
3. 可先用 MobaXterm 或 `ssh root@192.168.100.140` 验证连接。

### 10.3 程序没有正常退出

脚本会先发送 `q`。如果超时未退出，会发送 `Ctrl-C`，并在 `summary.json` 中记录：

```json
"forced_exit": true
```

这种情况下要在结论里说明本次是强制退出，不能直接当作完全正常运行。

### 10.4 日志目录混淆

每次运行都会创建带时间戳的目录。分析时以脚本输出的 `local_log_dir` 和 `summary_json` 为准，不要混用旧日志。

## 11. 后续使用建议

功能开发完成后，推荐按这个顺序测试：

1. 先确认本地 RK3506 程序已重新编译。
2. 说明目标 IP、执行文件、参数、运行时长。
3. 使用 `tk8710-rk3506-test` 自动上传并运行。
4. 查看 `summary.json` 和关键日志。
5. 让 Codex 给出明确结论：运行链路是否正常、业务功能是否通过、失败证据是什么。


先用 tk8710-rk3506-test 跑一下D:\CodexWorkSpace\tk8710_hal_v1.6\build_rk3506\tk8710_gw_sat ，ip是：192.168.100.140，参数是-w /userdata，运行 5 分钟；然后再用tk8710-rk3506-test 跑一下D:\CodexWorkSpace\tk8710_hal_v1.6\build_rk3506\tk8710_gw_ground ，ip是：192.168.100.69，参数是-w /userdata，运行 5 分钟；然后退出两个程序，然后分析一下两个程序的log。