# Driver 异步唤醒发送接口

日期：2026-11-24。决策已按用户确认实施：异步、外部恢复正常工作、
到期后在最近一次发送BCN结束中断（S0）停止。

## 各天线唤醒频偏（2026-11-26增补）

启动时增加配置tx_bcn_7.bcn_ddc_ang，低25位。天线1～8地址为
0x0830、0x1830、0x2830、0x3830、0x4830、0x5830、0x6830、0x7830。
Fs固定为1000000Hz，使用64位整数运算，将负频偏先加Fs再编码：

```text
wrapped_hz = offset_hz < 0 ? 1000000 + offset_hz : offset_hz
value = floor(wrapped_hz * 2^25 / 1000000)
```

正负1kHz示例分别为33554、33520877。负频偏先回绕再取整，不使用有符号除法后取补码，
避免与所给公式差一个最低位。量化分辨率为1000000/2^25 Hz，结果取整数部分。

| 天线 | 地址 | 模式1/2频偏Hz | 模式1/2值 | 模式3频偏Hz | 模式3值 |
|---|---|---|---|---|---|
| 1 | 0x0830 | -26000 | 0x01F2B020 | -52000 | 0x01E56041 |
| 2 | 0x1830 | 0 | 0x00000000 | 0 | 0x00000000 |
| 3 | 0x2830 | -9000 | 0x01FB645A | -18000 | 0x01F6C8B4 |
| 4 | 0x3830 | 0 | 0x00000000 | 0 | 0x00000000 |
| 5 | 0x4830 | 8000 | 0x00041893 | 17000 | 0x0008B439 |
| 6 | 0x5830 | 0 | 0x00000000 | 0 | 0x00000000 |
| 7 | 0x6830 | 25000 | 0x000CCCCC | 50000 | 0x00199999 |
| 8 | 0x7830 | 0 | 0x00000000 | 0 | 0x00000000 |

每次启动均对8路写入，包括频偏为0的天线，以清除前次模式残留。日志BCN_FREQ记录
0开始的天线编号、频偏Hz、地址、写入值和返回码；CFG也已改为记录实际值，
包括当前工作区自定义bcn_ampc写入值。

本次保留当前用户的1/3/5/7路幅度配置、轮换准备/切换调用注释和多天线同时发送设置。
新增频偏测试不等同于先前单天线轮换测试：主机覆盖768种模式/ID×8路频偏、正负1k/零值、
每路写失败在触发RF前回滚，以及原幅度设置保持不变。未测量实际RF频偏。

## 接口

包含 `driver/tk8710_driver_api.h`：

```c
int TK8710SendWakeup(wakeUpMode_e mode, uint32_t duration_ms, uint8_t wakeup_id);
int TK8710CancelWakeup(void);
int TK8710GetWakeupStatus(TK8710WakeupStatus* status);
int TK8710RegisterWakeupCallback(TK8710WakeupCallback callback, void* user_data);
```

模式为1～3，时长为1～16777215毫秒，ID为0～255。传给uint8_t ID参数前，
上层应校验原始输入，避免大于255的整数发生截断。频率不属于此接口参数，
调用前由 `TK8710Init()` 的RF配置设置。必须先成功Init才能启动。
返回0为成功，参数错误为 `TK8710_ERR_PARAM`，忙/尚未初始化为 `TK8710_ERR`；
寄存器或SPI操作错误保留非0返回值。启动返回0只表示异步启动成功。

## 推荐调用顺序

1. 应用层暂停正常业务，确保没有普通中断处理和Init/配置/Start等操作与唤醒切换并发。
2. 用Init配置唤醒频率和射频参数，注册完成回调。
3. 调用 `TK8710SendWakeup(mode, ms, id)`。
4. 应用可以继续做其他工作，或查询状态。到期后的第一个S0中断自动停止。
5. 收到完成事件且 `stopped=1` 后，应用线程用正常频率重新Init，配置正常时隙并Start。

连续测试不同ID时可在上一次停止成功后直接再次SendWakeup，不必退出进程或重新Init。
这只适用于继续发送唤醒信号。恢复普通业务必须重做Init/时隙配置/Start；Driver不恢复
此前的普通寄存器快照，也不自行切回正常频率。

```c
static void OnWakeupFinished(const TK8710WakeupStatus* status, void* user_data)
{
    /* 将整个status复制到应用自己的队列，并唤醒应用线程。
     * status指针只在本次回调内有效；不要在回调内直接Init或关闭IRQ线程。
     */
    AppPostWakeupEvent(status);  /* 应用自定义的非阻塞队列操作 */
}

/* 已在应用线程完成唤醒频率Init后 */
int ret = TK8710RegisterWakeupCallback(OnWakeupFinished, NULL);
if (ret == TK8710_OK) {
    ret = TK8710SendWakeup(TK8710_WAKEUP_MODE_3, 5000, 32);
}
/* 检查ret；随后由应用事件线程接收完成并恢复正常配置。 */
```

回调从GPIO IRQ处理上下文发出，RK3506上为Port的GPIO线程；主动取消时从取消调用线程发出。
回调调用前释放内部临界区，允许查询状态，但应只投递事件，不阻塞、不重入硬件配置。
没有回调时也可通过GetWakeupStatus查询最终状态。旧 `TK8710_CTRL_TYPE_SEND_WAKEUP`
保留原硬件寄存器控制行为，供已有自定义寄存器测试使用，不具备本接口的完成通知。
新业务及本次选中的RFTxTone入口使用 `TK8710SendWakeup`，不通过旧Ctrl路径。
原 `TK8710DriverCallbacks` 结构保持不变，单独注册唤醒回调。

## 时间、状态与完成含义

| 状态 | 含义 |
|---|---|
| IDLE | 未启动或已重新Init |
| SENDING | 已触发发射，尚未到时长 |
| WAIT_BCN | 时长已到，等待第一个S0结束中断 |
| DONE | 到期后的S0中断已执行停止并清理成功 |
| CANCELLED | 外部主动取消，立即停止，不等待S0 |
| ERROR | 配置、中断处理、寄存器切换或停止失败 |

采用 `TK8710GetMonotonicTimeUs()` 计时，从触发发射前一刻开始；S0处理和状态查询
均可识别是否到期。无需上层轮询来驱动停止，也不依赖墙上时间或Python计时。
在S0到达前不会因时长到期直接复位，因此实际发送会多出等待下一个BCN结束的时间，
还包含SPI、GPIO线程调度延时。它是软件观察到的中断边界，不是仪表测量的空口结束时刻。

没有S0中断时，状态保持WAIT_BCN，不发送DONE，不假定RF停止。应用可按自身策略超时报警，
再调用CancelWakeup。Cancel是立即终止的异常/人工操作，不代表正常完整BCN发送完成。

完成时先SPI状态机复位、禁用时隙中断并清理，再更新最终状态、通知一次。
停止使用 `TK8710SpiReset(TK8710_RST_STATE_MACHINE)`，避免现有 `TK8710Reset()`
附带硬件复位脉冲破坏RF配置。任何停止操作失败都不会报告DONE。
`stopped=1`表示复位及中断清理调用均成功；ERROR且stopped=1表示本次发送仍失败，但已清理。
ERROR且stopped=0时不能假定RF已停，需要外部复位/Init恢复。未初始化、忙、参数或启动配置
失败直接以接口非0返回值报告，不发完成回调；异步启动后的终态则只通知一次。
`elapsed_us`为软件计时值，取消/完成后保留，重新Init清为IDLE。

## 配置及普通业务隔离

配置来自当前验证入口，集中放在 `src/driver/tk8710_wakeup.c`：

- `group = ID / 32`，`bcnbits = ID % 32`。
- `0x0814 = 0x00100000 | (bcnbits << 4)`。
- `0x0824`：模式3基础 `0x004D0000`，模式1/2基础 `0x008D0000`，再加group×`0x00100000`。
- `0x0814/0x0818/0x081c/0x0820/0x0824/0x082c` 为每路TX FE独立寄存器。
  启动时对全部8路写入相同唤醒配置，通道偏移为 `0x1000 * ant`，ant=0～7。
  例如物理天线1/2/8对应 `0x0814/0x1814/0x7814`；其余5项按相同规则映射。
  即使Init只使能部分通道，也配置全部8路。MAC的 `0xc000/0xc008/0xc01c/0xc054`
  仍只配置一次，不随天线偏移。DEBUG日志CFG的ant采用0开始的编号。
- `0xc000`：模式1/2/3为 `0x700000/0x800000/0x900000`。
- `0xc054 = group × (模式1 ? 0x800 : 0x400)`。
- 其他固定配置依照当前u分支；每次写入都检查返回值，失败尝试停止回滚。
- 仅使能S0。唤醒模块接管IRQ时，普通BCN轮换、自动下行和普通业务回调均不执行。
- 到期前S0执行 `0xc008=1 → DelayMs(1) → 0xc008=0x80`；到期后的S0直接停止，不再切换。
- 唤醒BCN天线轮换：每次启动从Init使能的第一根天线开始，到期前每累计16次S0结束后切换到下一根，
  按编号递增并循环。轮换集合为 `init_9.ant_en & init_9.rf_sel`；全部使能时为0→1→…→7→0。
  不依赖普通时隙的bcnRotation表或txBcnAntEn=0xFF条件，唤醒接口仍只有模式、时长和ID。
  每次先关闭所有通道的sc_bcn，再使能选中通道的sc_bcn=0x60，sc_da统一设0x40。
  IQ/sign/保留位从启动前寄存器值保留；init_9.tx_bcn_ant_en设置为轮换集合，避免Init
  默认只选天线0导致后续通道被屏蔽。没有可用天线时拒绝启动。
  第16、32、48……次S0才写轮换配置，在 `0xc008=1` 后、延时及 `0xc008=0x80` 前完成，
  写失败按ERROR停止。其余S0保持天线，但仍执行原c008切换。每次启动重新计数；
  到期判断优先，即使到期时正好为第16次S0，也直接停止，不再轮换。
- 普通工作模式下不执行上述切换。停止后继续隔离遗留IRQ，直到外部成功Init恢复正常路径。
- 发送期间禁止普通Init、配置、Start、RF配置、Debug控制等冲突入口。
  原始ReadReg/WriteReg及底层Port调试接口不做全面封锁，上层不得绕过接口并发改写芯片。

## 测试程序和工具

`Test8710RFTxTone` 的u改用新Driver接口；p为Cancel，q先取消再退出。
启动标记变为 `WAKEUP_SESSION_V2`，通知日志为：

```text
WAKEUP_STARTED mode=3 duration_ms=5000 id=32
WAKEUP_FINISHED state=3 result=0 stopped=1 elapsed_us=...
```

Python工具正常流程等待FINISHED/DONE，不在配置时长到期时发送p。
停止按钮、异常收尾才发送p；整轮只启动一个进程。旧V1程序不能与新版工具配套。
外部查询得到PASS与Driver完成是两个判断：工具先检测终端响应，再等待发射完成，
不会因终端提前响应就提前结束Driver发射。

## 验证

主机测试（不访问硬件）：

```bash
gcc -std=c99 -Wall -Wextra -Werror -Iinc -Iport \
    test/DriverTest/support/test_wakeup_driver.c src/driver/tk8710_wakeup.c \
    -o /tmp/test_wakeup_driver
/tmp/test_wakeup_driver
```

覆盖到期前/截止点/非S0/无S0、回调只通知一次且不持锁、取消、复用、启动未Init、
参数边界、768种模式/ID映射、14个启动写入故障点、读失败和停止失败恢复。
Python工具测试命令沿用 `python -m unittest discover -s tools/wakeup_signal_test -p 'test_*.py' -q`。
ARM通过现有 `bash cmake/build_rk3506.sh Test8710RFTxTone` 构建。
此次接口变更不改写旧遍历结果。

2026-11-24板端验证：192.168.100.78，启动参数 `6 472300000 46`，携带板端原TXADC。
模式3的ID31、32各配置200 ms，工具不发送p，同一PID1299自动返回DONE/result=0/stopped=1。
Driver报告elapsed_us分别为206897、208245，符合到期后在下一个S0处理时停止的行为。
最后q退出码0，SSH复查无遗留测试进程，日志ERROR/WARN均0。
这证明板端异步完成及进程复用链路，未测量RF波形，也未重新验证接收端唤醒成功率。

- [板端事件日志](../tools/wakeup_signal_test/results/driver_async_20261124_7b02a87f/journal.jsonl)
- [板端验证汇总](../tools/wakeup_signal_test/results/driver_async_20261124_7b02a87f/summary.json)
- 新二进制SHA256：`f67a3743967368de66db20c6e6e93675b0684b998dbd072987d0ba5033cdb700`。
- 主机C测试通过AddressSanitizer/UndefinedBehaviorSanitizer；Python24项回归测试通过。
- ARM和JTOOL的RFTxTone程序均编译链接通过。ARM测试入口保留已有未使用TCP变量/函数警告。

### 天线轮换增补验证（2026-11-24）

上述f67a...哈希和板端时长数据属于增加天线轮换前版本。
新增轮换的主机测试通过ASan/UBSan：初始天线选择、完整循环两轮、稀疏使能集合、
无可用天线、任意时刻最多一路BCN使能、保留IQ字段、到期不再轮换、
24处启动写入故障、9处轮换写入故障，以及原768种模式/ID映射及异步生命周期。
新版本实际空口天线输出、每根天线功率和接收效果仍需仪表及终端验证。
