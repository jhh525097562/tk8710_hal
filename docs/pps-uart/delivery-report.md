# RK3506 外部 PPS UART 管理交付报告

**日期：** 2026-09-02
**集成状态：** patch-ready，未暂存、未提交、未推送

# 1. 完成内容

| 任务 | 状态 | 交付 |
| --- | --- | --- |
| T-01 | 完成 | 公共 API、默认配置、健康分类测试 |
| T-02 | 完成 | UART 行协议、EVENT/OK/ERR、结构解析、诊断和 GPIO 复位重连 |
| T-03 | 完成 | 10 秒长期监控程序、RK3506 CMake/Shell 构建目标 |
| T-04 | 完成 | 严格编译、协议测试、交叉构建、符号检查、全量构建审查 |
| H-01 | 部分完成 | `.62` 板端正常路径通过；异常恢复用伪终端验证；仪器波形未执行 |

# 2. 关键行为

- 默认 `/dev/ttyS4`、115200 8N1；复位为 `gpiochip1/19` 低 100 ms。
- 首次启动和每次复位后都严格执行 `OUT ON`，再执行 `SET PERIOD 10`。
- 每 2 秒轮询；首次对齐宽限 60 秒；健康后连续 3 次异常才恢复。
- 两次复位至少间隔 30 秒，最多连续恢复 3 次；恢复成功后连续次数清零。
- SIGINT/SIGTERM 关闭 UART，但不发送 `OUT OFF`。

# 3. 验证结果

| 检查 | 结果 | 证据摘要 |
| --- | --- | --- |
| 主机 API 测试 | 通过 | MinGW `-std=c99 -Wall -Wextra -Werror`，输出 `TK8710 PPS API tests passed` |
| Linux 协议集成测试 | 通过 | 伪终端覆盖 EVENT、状态、完整 UTC、五项诊断、GPIO 0/1、复位后命令顺序 |
| 监控程序严格编译 | 通过 | Ubuntu GCC `-std=c99 -Wall -Wextra -Werror` |
| Shell 语法 | 通过 | `bash -n cmake/build_rk3506.sh` |
| RK3506 定向构建 | 通过 | `test_tk8710_pps_api`、`tk8710_pps_test` 均成功 |
| 静态库符号 | 通过 | 12 个 `TK8710Pps*` 公共符号存在 |
| 全量 RK3506 构建 | 部分通过 | 新目标及现有主要目标生成；既有两个 DriverTest 链接错误见风险 |
| 差异检查 | 通过 | `git diff --check` 无错误，仅 Git 行尾提示 |
| `.62` 板端正常路径 | 通过 | 75 秒运行，退出 0，无 ERROR/WARN，持续 `RUNNING/PERIOD=10/ALIGN=1` |
| 板端异常注入 | 未执行 | 自动恢复由伪终端测试覆盖，未在真实板上破坏 UART/PPS |
| 10 秒物理波形 | 未执行 | 需要示波器或计数器 |

板端证据：`remote_logs/trm_run_20260902_170916_pps_uart_10s/`。远端目录为 `/userdata/trm_run_20260902_170916_pps_uart_10s`，命令为 `'/userdata/tk8710_pps_test'`，运行 75 秒后测试工具发送 Ctrl-C，程序捕获并正常退出。

# 4. 质量审查

- 正确性：修复完整 UTC 被截断、启动期过早复位和冷却期终止后继续复位的问题。
- 架构：外部 PPS 保持独立模块，未耦合 `TK8710HalInit()`；恢复策略位于测试程序。
- 安全：外部协议字段均作长度、数值和布尔校验；不调用 shell，不包含 Secret。
- 性能：固定缓冲区、单命令在途、2 秒轮询，无动态分配和无界响应累计。
- 兼容性：未知 KV 字段忽略；非 RK3506 返回 `UNSUPPORTED`。
- Review verdict：软件变更通过，patch-ready。

# 5. 动态补充与残余风险

- 新增 `TK8710PpsGetVersion()`，用于满足批准需求中的启动版本记录；不改变需求基线，无需重新过 Gate。
- 全量脚本虽然返回 0，但日志中有两个与本需求无关的既有链接错误：一个测试源没有 `main`，另一个缺少 `RfCalProcessCommand`；另有既有 unused 警告。本次未修改这些目标。
- `.140` 的一次误用默认目标运行已作废，不纳入验收；有效板端证据仅来自用户指定的 `.62`。
- 尚无真实异常注入和仪器波形证据，因此不能宣称硬件异常恢复及物理 10 秒脉冲已完成最终验收。

# 6. Git 与清理

- 模式：shared-working-tree serial + patch-ready。
- 未 stage、commit、push、PR 或 merge。
- `test/example/tk8710_gw.c` 和 `3506uart/` 保持用户原有状态。
- 构建产物位于既有 `build_rk3506/`；板端日志位于 `remote_logs/`。

# 7. V1.1 网关 GPS/PPS 集成交付

| 任务 | 状态 | 交付 |
| --- | --- | --- |
| T-06 | 完成 | 网关 GPS 生命周期、周期换算和连续状态判定测试 |
| T-07 | 完成 | 普通/快速启动统一同步寄存器配置，外部同步 ACM 立即触发 |
| T-08 | 完成 | 启动探测、搜星等待、NS 周期配置、运行监控和统一退出 |
| T-09 | 完成 | 严格回归、双板正常路径、测试钩子隔离和交付审查 |

## 7.1 软件验证

| 检查 | 结果 | 证据摘要 |
| --- | --- | --- |
| GPS 策略严格编译与运行 | 通过 | C99 `-Wall -Wextra -Werror`；覆盖周期换算、状态异常、UART 失败和恢复连续计数 |
| UART 命令顺序测试 | 通过 | 伪终端断言 `GET VERSION/STATUS` 后严格执行 `OUT ON`、`SET PERIOD`，并覆盖 EVENT 穿插 |
| RK3506 相关目标构建 | 通过 | API、策略、协议、设备测试、PPS 测试、正式网关和 NS 测试网关共 7 个目标成功 |
| 正式二进制隔离 | 通过 | `tk8710_gw` 不含 `--test-ns-config`；该参数仅存在于测试专用目标 |
| Shell 与差异检查 | 通过 | `bash -n` 成功；`git diff --check` 无错误，仅行尾提示 |
| 新增告警 | 无 | 仅有既存 `SaveTxDcFromRfRam` 未使用告警 |

## 7.2 板端验证

| 板卡 | 结果 | 关键证据 |
| --- | --- | --- |
| `192.168.100.140` 无 GPS 模块 | 通过 | 两次版本查询无响应后进入 ABSENT；日志确认 `ls_en=0、ls_master=1` |
| `192.168.100.62` 有 GPS 模块 | 通过 | 读取 2026-09-02 APP 版本；固定 NS 输入计算出 7 秒；命令顺序正确并进入 RUNNING |
| `.62` TK8710 外部同步 | 通过 | 日志确认 `ls_en=1、ls_master=0`，连续 6 次完整健康轮询无失败 |
| `.62` AG32 恢复为 10 秒 | 通过 | `OUT ON` 后 `SET PERIOD 10`，进入 RUNNING 并连续 3 次完整健康 |

主要日志：

- `.140`：`remote_logs/trm_run_20260902_185059_gw_gps_no_module_v11/`
- `.140` 本地同步寄存器：`remote_logs/trm_run_20260902_191742_gw_gps_local_sync_v11/`
- `.62` 外部 NS：`remote_logs/trm_run_20260902_190505_gw_gps_external_ns_v11/`
- `.62` 恢复 10 秒：`remote_logs/trm_run_20260902_190754_gw_gps_restore_10s_v11/`

## 7.3 验收边界

- 未在真实板上断开 UART 或 PPS，连续故障退出 `1` 目前由策略测试及正常通信路径共同覆盖。
- 未联调保护进程，GPS 恢复退出 `2` 的进程拉起行为尚未验证。
- 未使用示波器或计数器测量外部引脚，UART 的 RUNNING/ALIGN 状态不能替代 7 秒或 10 秒物理波形证据。
- `.140` 固定 NS 测试没有下行载荷，运行期产生既有广播数据缺失日志；本次仅用其判定无模块启动和同步寄存器值。
- Review verdict：代码与双板正常路径通过，patch-ready；上述三项保留为系统级硬件验收。

# 8. V1.2 GPS API 三场景测试集交付

| 任务 | 状态 | 交付与证据 |
| --- | --- | --- |
| T-10 | 完成 | 新增健康 AG32 伪串口场景测试，覆盖 `NO_MODULE`、`NO_PPS`、`NO_PPS_THEN_RECOVER` |
| T-11 | 完成 | 测试宏 API、真实值/有效值日志、run-id 校验、一次性恢复标记和网关 CLI |
| T-12 | 完成 | 严格主机测试、RK3506 构建、正式二进制隔离、`.62` ARM 自包含运行 |

## 8.1 验证结果

- 三个主机测试均以 C99 `-Wall -Wextra -Werror` 编译并退出 0：网关策略、UART
  命令顺序、三场景故障注入。
- RK3506 定向构建成功：`test_tk8710_gw_gps_faults`、`tk8710_gw`、
  `tk8710_gw_gps_ns_test`。仅保留既有 `SaveTxDcFromRfRam` 未使用告警。
- 正式 `tk8710_gw` 的字符串和符号检查均确认不存在测试参数、场景字符串或
  `GwGpsTest*` 符号；测试能力仅存在于 `TK8710_GPS_TEST_HOOKS` 构建。
- `.62` 运行 `/userdata/test_tk8710_gw_gps_faults_20260907_accept`，退出码 0、
  `forced_exit=false`。本地证据位于
  `remote_logs/trm_run_20260907_145003_gps_fault_api_accept/`。
- `git diff --check` 和 `bash -n cmake/build_rk3506.sh` 通过。

## 8.2 质量审查

- 正确性：注入前始终调用真实 UART API；`NO_MODULE` 清空有效版本，`NO_PPS`
  只改写有效 `PPS` 字段，恢复场景必须连续 3 次真实健康才生成重启标记。
- 架构：测试逻辑位于网关 GPS 适配层，没有进入通用 PPS 模块或生产 HAL 生命周期。
- 安全：run-id 限长并限制字符集；标记采用固定 `/tmp` 命名空间、`O_EXCL`、
  `O_NOFOLLOW`，读取和删除前校验普通文件及当前用户所有权。
- 性能：注入只在测试构建中执行；生产构建没有额外状态、日志或轮询开销。
- Review verdict：通过，patch-ready。

## 8.3 尚未执行的系统验证

没有停止或替换 `.62` 上当前运行的网关，因此尚未用完整测试网关执行真实 300 秒
`NO_PPS` 等待，也未联调保护进程根据退出码 2 重拉。上述属于后续板端系统用例，
不影响本次 API、构建隔离和 ARM 自包含测试结论。
