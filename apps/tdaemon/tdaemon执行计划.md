**计划 A**

- **目标**：重构 `tdaemon.sh`，将三网切换与进程守护解耦；引入“轻量恢复 → 软件重置 → GPIO 恢复 → 降级切网”的链路恢复状态机。
- **架构**：保留 [tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh) 作为主入口与调度器，把公共能力、网络探测、恢复状态机、GPIO 适配、进程守护拆到独立脚本；先保证行为等价，再逐层接入 GPIO 恢复。
- **技术栈**：POSIX shell、SysV init、`ip/route/ifup/ifdown`、`libgpiod` 命令行工具、现有 `switch_to_sta.sh` / `switch_to_ap.sh`。
- **仓库与部署映射**：仓库内新增脚本统一放在 `e:\Product\TurMassLink2026\tdaemon\` 目录，与 `tdaemon.sh` 同级；目标板部署时统一复制到 `/usr/bin/`，由 `tdaemon.sh` 通过 `SCRIPT_DIR=/usr/bin` 加载同级模块。
- **明确非目标**：本计划不修改 [S99tdaemon_job](file:///e:/Product/TurMassLink2026/tdaemon/S99tdaemon_job)、[inittab](file:///e:/Product/TurMassLink2026/tdaemon/inittab) 启动方式，不在本阶段实现 USB 检测与升级刷写，不改变现有 AP/STA 外部脚本接口。
- **模块加载顺序**：`tdaemon.sh` 中固定按 `tdaemon_common.sh -> tdaemon_gpio.sh -> tdaemon_net.sh -> tdaemon_recovery.sh -> tdaemon_proc.sh` 的顺序 `.` 加载，避免公共函数与恢复接口未定义。
- **第一阶段硬约束**：拆分阶段禁止改变对外配置键名、禁止改变日志格式、禁止改变主循环调度频率、禁止引入新的后台常驻进程。
- **硬件约束更新**：`wlan0` 当前无可用 GPIO 控制引脚，只支持软件恢复；`usb0/4G` 具备 reset 与电源控制引脚，其中 4G reset 为高电平有效，4G 供电引脚为低电平关机、高电平开机，对应配置应为 `modem_reset_active_value=1`、`modem_power_active_value=1`。
- **当前进展**：任务 0～8 已完成，已落地公共模块、网络模块、进程守护模块、GPIO 适配层、恢复状态机及主循环接入；下一步进入板级验证、测试补强与 USB 扩展预留。

---

### 任务 0：补充执行约束与部署清单

**文件**
- 修改：[tdaemon执行计划.md](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon执行计划.md)
- 读取：[S99tdaemon_job](file:///e:/Product/TurMassLink2026/tdaemon/S99tdaemon_job)
- 读取：[inittab](file:///e:/Product/TurMassLink2026/tdaemon/inittab)

- [x] 明确仓库文件路径与目标板 `/usr/bin` 部署路径映射
- [x] 明确本阶段不修改的文件与能力边界
- [x] 明确模块 `source` 顺序与依赖关系
- [x] 明确新增脚本部署清单，避免目标板漏拷贝模块导致启动失败
- [x] 明确第一阶段只做“同级拆分 + 行为等价”，不提前接入 GPIO 恢复

**验收**
- [x] 开发人员可直接按计划知道要创建哪些同级脚本
- [x] 目标板部署时不会遗漏新增模块文件

---

### 任务 1：冻结现状与回归基线

**文件**
- 读取：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)
- 读取：[tdaemon.conf](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.conf)
- 读取：[switch_to_sta.sh](file:///e:/Product/TurMassLink2026/tdaemon/switch_to_sta.sh)
- 读取：[switch_to_ap.sh](file:///e:/Product/TurMassLink2026/tdaemon/switch_to_ap.sh)
- [x] 列出当前功能清单：配置加载、热更新、选路、metric、AP/STA、无网重启、进程守护
- [x] 标记“不可改变”的行为：启动参数、PID/锁、扫描周期、信号处理、日志路径
- [x] 梳理当前函数归属表，形成“原函数 -> 新模块”映射
- [x] 定义回归检查项：配置校验、活跃链路选择、metric 调整、进程拉起、AP 切换
- [x] 记录板级验证命令清单，作为后续每阶段固定回归步骤

**验收**
- [x] 形成一份明确的“重构不改行为边界”列表
- [x] 所有现有函数都能归类到后续模块中
- [x] 明确 `switch_to_sta.sh` / `switch_to_ap.sh` 继续作为外部能力脚本，不并入第一阶段拆分

---

### 任务 2：拆出公共基础模块

**文件**
- 新建：`e:\Product\TurMassLink2026\tdaemon\tdaemon_common.sh`
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)

- [x] 提取通用函数：时间、日志、字符串处理、mtime、通用锁、公共常量
- [x] 提取配置解析与配置应用的公共部分
- [x] 在主脚本中通过 `.` 引入公共模块
- [x] 保持对外入口、参数与日志行为不变
- [x] 做一次语法检查与启动路径检查

**建议归属**
- `trim`
- `to_upper`
- `basename_simple`
- `now_ts`
- `get_mtime`
- `log_write/log/log_level_num`
- `cfg_*`
- `lock_*`

**验收**
- [x] `tdaemon.sh` 仍可单独启动
- [x] 配置加载与热加载行为不变

---

### 任务 3：拆出进程守护模块

**文件**
- 新建：`e:\Product\TurMassLink2026\tdaemon\tdaemon_proc.sh`
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)

- [x] 提取进程扫描、运行判断、异常拉起逻辑
- [x] 保持 `mqtt_bridge`、`tk8710_gw` 的网络门控规则不变
- [x] 主循环继续只调用 `proc_tick`
- [x] 验证进程保活与超时拉起行为不变

**建议归属**
- `proc_is_running`
- `proc_restart_and_wait`
- `proc_tick`

**验收**
- [x] 手工杀掉被守护进程后，仍能按原逻辑拉起
- [x] 网络未 ready 时，门控进程仍不会被启动

---

### 任务 4：拆出网络探测与选路模块

**文件**
- 新建：`e:\Product\TurMassLink2026\tdaemon\tdaemon_net.sh`
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)

- [x] 提取接口探测、IP/DHCP 判定、L3 探测、metric 路由调整、候选接口选择
- [x] 保留当前 `eth0 -> usb0 -> wlan0` 优先级
- [x] 保留“当前链路可用时不主动回切”的既有策略
- [x] 保留 `switch_to_sta.sh` / `switch_to_ap.sh` 调用方式
- [x] 验证 metric 调整幂等性与锁行为

**建议归属**
- `net_priority_to_list`
- `net_detect_presence`
- `net_iface_link_up`
- `net_check_dhcp_ready`
- `net_probe_quality`
- `net_select_best_iface`
- `net_switch_to`
- `net_route_*`
- `net_prepare_candidate_iface`

**验收**
- [x] 启动后仍能正确选中首个可用链路
- [x] 活跃链路 metric 为最高优先级
- [x] 其它链路 metric 被正确降级

---

### 任务 5：新增恢复状态机模块

**文件**
- 新建：`e:\Product\TurMassLink2026\tdaemon\tdaemon_recovery.sh`
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)
- 修改：[tdaemon.conf](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.conf)

- [x] 为每个接口建立恢复状态：失败计数、恢复等级、上次恢复时间、冷却窗口
- [x] 定义统一恢复层级：
  - `L1` 轻量恢复
  - `L2` 软件重置
  - `L3` GPIO reset（仅 4G）
  - `L4` GPIO power-cycle（仅 4G）
- [x] `eth0` 仅支持 `L1/L2`
- [x] `wlan0` 仅支持 `L1/L2`
- [x] `usb0` 支持 `L1/L2/L3/L4`
- [x] 恢复失败后再执行 metric 降级与链路切换
- [x] 恢复成功后将接口状态重置回初始层级

**建议新增配置**
- `RECOVERY_LIGHT_THRESHOLD`
- `RECOVERY_SOFT_THRESHOLD`
- `RECOVERY_GPIO_COOLDOWN_S`
- `MODEM_POWER_OFF_HOLD_MS`
- `MODEM_POWER_ON_WAIT_S`
- `MODEM_RESET_PULSE_MS`

**验收**
- [x] 同一接口不会在短时间反复打 GPIO
- [x] 软件恢复成功时不会继续升级到硬件恢复
- [x] 当前链路恢复失败后仍能正常切网

---

### 任务 6：新增 GPIO 适配层

**文件**
- 新建：`e:\Product\TurMassLink2026\tdaemon\tdaemon_gpio.sh`
- 修改：[tdaemon.conf](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.conf)

- [x] 封装 libgpiod 调用，不在主逻辑中直接写 `gpioset/gpioget`
- [x] 移除 Wi‑Fi GPIO 依赖，Wi‑Fi 仅保留软件恢复
- [x] 定义 4G reset 接口
- [x] 定义 4G power off / power on / power-cycle 接口
- [x] 加入动作日志：接口、动作、chip、line、持续时间、原因
- [x] 为板级差异预留 GPIO 配置项，不把 line 编号写死在业务逻辑里
- [x] 明确 4G reset 为高电平有效、4G 供电为低电平关机高电平开机

**建议函数**
- `gpio_modem_reset_pulse`
- `gpio_modem_power_off`
- `gpio_modem_power_on`
- `gpio_modem_power_cycle`

**验收**
- [x] libgpiod 不可用时，函数返回失败并记录日志，不导致主循环崩溃
- [x] GPIO 动作可单独在板子上手工验证

---

### 任务 7：把恢复状态机接入主循环

**文件**
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)
- 修改：`/usr/bin/tdaemon_net.sh`
- 修改：`/usr/bin/tdaemon_recovery.sh`

- [x] `net_tick` 中把“探测失败即切换”改为“先恢复，再切换”
- [x] 当前链路失败达到阈值时，先调用接口恢复流程
- [x] 仅在恢复失败后才执行 metric 降级
- [x] 切换到新链路成功后，对旧链路进入后台恢复窗口
- [x] 保证 `NETWORK_READY` 的定义不变，避免影响进程守护门控

**验收**
- [x] 当前链路故障时，能看到分层恢复日志
- [x] 成功恢复时，不会无谓切网
- [x] 恢复失败时，仍能切到其它链路

---

### 任务 8：收敛主脚本为调度器

**文件**
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)

- [x] 主脚本只保留：
  - 环境初始化
  - 配置入口
  - source 各模块
  - 主循环调度
  - trap/exit 清理
- [x] 删除已经迁移到各模块的函数定义
- [x] 保持 `tdaemon --check-config`、`-c`、`-v`、`-h` 行为不变
- [x] 保持 SysV init 调用入口不变

**验收**
- [x] 主脚本长度显著下降
- [x] 阅读主脚本时能清楚看到“初始化 -> 网络 tick -> 进程 tick -> 兜底重启”主流程

---

### 任务 9：更新测试与板级验证清单

**文件**
- 修改：[tdaemon_test_suite.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon_test_suite.sh)
- 可选新增：`e:\Product\TurMassLink2026\tdaemon\tdaemon_gpio_test.sh`

- [ ] 增加恢复层级相关测试场景
- [ ] 增加 metric 幂等测试
- [ ] 增加 4G reset / power-cycle 测试
- [ ] 增加“恢复成功但不切网”“恢复失败才切网”的验证项
- [ ] 增加无网超时后整机重启验证项

**板级验证建议**
- [ ] 拔网线模拟 eth0 断链
- [ ] 禁止 DHCP 模拟无 IP
- [ ] 人工关闭 Wi‑Fi/4G 模块服务模拟软件异常
- [ ] 人工触发 GPIO reset/power-cycle 验证硬件恢复时间窗
- [ ] 检查日志、状态文件、路由表、进程状态

---

### 任务 10：为后续 USB 升级能力预留接口

**文件**
- 修改：`e:\Product\TurMassLink2026\tdaemon\tdaemon_common.sh`
- 修改：[tdaemon.sh](file:///e:/Product/TurMassLink2026/tdaemon/tdaemon.sh)

- [ ] 先不实现 U 盘检测逻辑
- [ ] 只在主循环里预留低频扩展 tick 位点，例如 `usb_tick`
- [ ] 在日志与配置命名上避免把守护进程写死为“只做网络”
- [ ] 为后续 `tdaemon_usb.sh` 预留模块加载位置

**验收**
- [ ] 当前版本不引入 USB 功能回归
- [ ] 后续可无痛挂接 U 盘检测与升级逻辑

---

**实施顺序**
- **第一阶段**
  - 任务 0、1、2、3、4、8
  - 目标：先拆文件，行为保持等价
- **第二阶段**
  - 任务 5、6、7
  - 目标：接入恢复状态机和 GPIO 恢复
- **第三阶段**
  - 任务 9、10
  - 目标：补验证与后续 USB 扩展位

**完成判据**
- [x] `tdaemon.sh` 成为轻量调度器
- [x] 三网切换仍可用，metric 逻辑不回退
- [x] 4G 支持 reset + power-cycle 恢复
- [x] 所有 GPIO 动作都有冷却和日志
- [x] 无网长期异常仍保持整机重启兜底
- [ ] 为 USB U 盘升级检测预留稳定扩展点

**建议的首个实施切口**
- 先从 **任务 2 + 任务 3 + 任务 4** 开始，只做“拆模块不改行为”，这是最稳的第一刀。  
- 等主脚本瘦下来，再把恢复状态机和 GPIO 接进去，风险最小。  
- 第一阶段完成后，单独补一次“部署清单检查”，确认 `tdaemon.sh` 与新增模块在目标板 `/usr/bin/` 中齐套存在。  

