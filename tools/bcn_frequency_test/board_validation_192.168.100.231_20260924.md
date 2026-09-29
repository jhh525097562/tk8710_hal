# 192.168.100.231 网关板端验证

日期：2026-09-24。结论：**三点自动测试流程通过；存在射频通道和校准限制，尚未验证终端接收。**

## 1. 实际配置与结果

从该板现有 MQTT/NS 的 IPC 配置响应读取，而非沿用示例值：

- 基准中心频率：507808000 Hz。
- NS 速率索引 3，即模式 8；单速率，TDD=10，上下行包块数各 3。
- `gps_enable=1`，NS 原 `nwk_num=0`。测试程序只在本地将 B 的 bcnbits 设为 1，未修改 NS 配置。
- 时隙计算结果：每帧 350000 us，GPS/PPS 周期 7 秒。
- 扫频：-1000、0、+1000 Hz；每点有效运行 10 秒，稳定等待 2 秒。

| 频差 | 配置中心频率 / Hz | 配置到有效开始 / 秒 | 实际有效运行 / 秒 | 结果 |
|---:|---:|---:|---:|---|
| -1000 Hz | 507807000 | 18.136 | 10.003 | POINT_END，status=0 |
| 0 Hz | 507808000 | 31.999 | 10.004 | POINT_END，status=0 |
| +1000 Hz | 507809000 | 31.986 | 10.004 | POINT_END，status=0 |

三点依次输出 POINT_CONFIG → POINT_START → POINT_END，最后输出 SWEEP_DONE。退出码 0，无强制退出；测试包装器记录总耗时 116 秒。初始化/对齐等待明显长于有效计时，未被计入每点的 10 秒。

运行期间 56 条 GPS/PPS 状态均为 `STATE=RUNNING PERIOD=7 GPS=1 PPS=1 FIX=1 RMC=A ALIGN=1 HOLD=0 BAD=0 RESYNC=0 failures=0`。每点开始还要求 NS 配置匹配、广播数据有效且 S3 中断计数推进，不能仅靠 GPS 状态进入计时。

频率表是配置值。日志中的 nominal_rf_hz 是寄存器量化计算值，未使用频谱仪测量实际中心频率或两台设备的物理频差。

## 2. 执行路径与命令

- 上传二进制：`/userdata/bcn_freq_validation_20260924/tk8710_gw_freq_test`。
- 成功运行目录：`/userdata/bcn_freq_validation_20260924/trm_run_20260924_120211_sweep3_time64`。
- 本地日志：[成功运行目录](../../remote_logs/bcn_231_validation/trm_run_20260924_120211_sweep3_time64/)。
- [完整标准输出](../../remote_logs/bcn_231_validation/trm_run_20260924_120211_sweep3_time64/stdout_pty.log)、[频点事件](../../remote_logs/bcn_231_validation/trm_run_20260924_120211_sweep3_time64/frequency_events.json)、[包装器统计](../../remote_logs/bcn_231_validation/trm_run_20260924_120211_sweep3_time64/summary.json)。

实际板端命令：

```bash
cd /userdata/bcn_freq_validation_20260924/trm_run_20260924_120211_sweep3_time64
/userdata/bcn_freq_validation_20260924/tk8710_gw_freq_test \
  --base-hz 507808000 --start-hz -1000 --end-hz 1000 --step-hz 1000 \
  --hold-seconds 10 --bcnbits 1 --rate 8 --tdd-count 10 \
  --ul-blocks 3 --dl-blocks 3 --ready-timeout 90 --settle-seconds 2 --work-dir .
```

成功验证的 ELF 大小为 319836 字节，本地及远端 SHA-256 相同：

```text
9a40a32140e328d65143b1ab449e84573622573403bb18850734df6bed77ebe2
```

退出后检查：测试进程不存在，`gpioinfo gpiochip0` 显示 line 18 为 unused，`fuser /dev/ttyS4` 未列出占用进程。原 `mqtt_bridge` 仍为 PID 3129。测试前没有普通网关进程，测试后保持该状态。未覆盖生产网关二进制，未删除远端日志。

## 3. 实机发现与修复

1. 第一轮在读取 8 路 RF I2C TX DC 时退出，没有启动射频。测试程序之前增加的硬性校准文件要求比参考网关严格。现已恢复为与参考网关一致的文件/默认值回退，并明确记录实际配置来源。
2. 第二轮第一点完成，切第二点出现 `Device or resource busy`，由操作者代理发送 SIGTERM 停止；包装器 `forced_exit=false` 不代表该轮自然完成。现切频在 IRQ disable/join 后执行完整 HAL reset，释放 GPIO 和 SPI，再重新初始化。
3. 释放修复后，第二点仍未就绪。专用诊断捕获 `gpiod_line_event_wait errno=22 (Invalid argument)`，其 fd 指向有效的 `anon_inode:gpio-event`。结合仓库 PPS 模块已有的 time64 ABI 说明，修正测试目标的 port 编译：增加 `_TIME_BITS=64`、`_FILE_OFFSET_BITS=64`，以专用 `tk8710_freq_port_time64.o` 优先链接，同时匹配 wait/read 的时间结构。修正后的三点实机复测通过，未再出现该错误。

修改限于独立测试程序及其构建目标。生产 `test/example/tk8710_gw.c` 无差异，共享 port 源码未因本次验证改动。原工作区已有的其他修改保留。

## 4. 警告、异常与证据边界

- 启动明确记录 `TXDC source=zero_txadc`：本板未读到完整 I2C 校准值，且没有可用 TX DC 文件。本次不是 TX DC 校准验收。
- 12:03:56，最后一个频点运行期间出现一次真实错误：`RF channel[7] disabled: ANoise=1024 unchanged for 15 samples, ant_en=0x7F, rf_sel=0x7F`。当前策略为异常 RF 数达到 3 才整体停止，因此本轮流程完成，但不能标为全部 8 路 RF 健康。测试前遗留 RFstatus 文件已显示 `01111111`，仅作背景，不将旧状态当作当前诊断。
- 标准输出和 Driver 日志分别统计到同一条 RF7 错误各 1 次，不是两次独立故障。
- 标准输出与 Driver 日志各 18 条 WARN：12 条为校准文件缺失/使用零 TXADC，6 条为原驱动以 WARN 级别打印的时隙长度信息。TRM ERROR/WARN 均为 0。
- 包装器 `generic_failed=57` 包含 56 条正常 `failures=0` 状态行及 1 条 I2C 查找失败，不能解释为 57 个运行故障。
- 包装器额外尝试下载相对路径 `./8710log` 提示路径不存在；本次运行目录内的 `8710log/tk8710_driver_0.log` 和 `trm_log_0.log` 已通过主目录递归下载，日志并未因此缺失。
- ACM requested/starts/hidden/valid=0/valid=1/no-valid-result/rejected 计数均为 0；当前日志级别没有独立 ACM 校准明细，不据此额外宣称 ACM 性能验收。RX user、sent-user、queue-status 事件均为 0。程序未输出最终 IRQ 表，就绪门限中的 S3 推进仅证明中断活跃。
- 本机检测到 COM6，但未确认它是本次终端，因此没有修改或采集串口。没有对另一台固定网关进行配置操作，未验证终端 CFO/bcnbits、掉线重入网及两网关实际射频差。

本次硬件验证覆盖自动配置、GPS/业务就绪、有效计时、两次连续切频、自动退出及资源释放；正式终端接收测试仍需 A/B/终端共同参与，并记录 RF7 禁用与零 DC 的环境条件。
