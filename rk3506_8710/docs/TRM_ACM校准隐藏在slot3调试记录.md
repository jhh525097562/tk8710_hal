# TRM ACM校准隐藏在slot3调试记录

## 1. 背景和目标

外部触发 TRM ACM 校准后，网关需要在一个超帧的最后一帧执行校准。校准时机选择在 slot2 结束，也就是 slot3 开始的位置。ACM 校准会中断原本硬件时隙运行，因此校准完成后需要补偿等待，并在原本 slot3 中断附近重新启动时隙运行。

目标是让终端侧无法感知网关执行过一次 ACM 校准，终端仍按原来的周期和网关对齐通信。

当前 TRM 侧调度策略：

- 外部命令调用 `TRM_RequestAcmCalibration()`。
- TRM 保存 pending 请求，不立即执行。
- 在 `TRM_OnDriverSlotEndAdapter()` 收到最后一帧的 S2 中断时执行 ACM。
- ACM 结束后根据 slot3 窗口剩余时间等待。
- 到达目标时间后调用 `TK8710FastStartTrigger()` 重新启动硬件时隙。
- TRM 软件上补一个虚拟 S3 完成，保持帧号推进一致。

## 2. 已实现的关键方案

### 2.1 TRM ACM请求接口

新增 TRM 侧请求接口，用于外部命令只提交校准请求：

```c
int TRM_RequestAcmCalibration(const TRM_AcmCalibRequest* request);
```

请求参数包括：

- `calibCount`：连续校准次数，默认 5。
- `snrThreshold`：SNR 门限，默认 32。
- `restartAdvanceUs`：相对 slot3 结束提前启动的时间。
- `guardUs`：slot3 剩余时间保护门限。

### 2.2 ACM执行位置

校准执行位置放在：

- `TRM_GetSuperFramePosition() == g_trmMaxFrameCount`
- 当前中断为 S2 结束

也就是超帧最后一帧的 slot2 结束位置。

### 2.3 快速start接口

新增 TK8710 快速 start 路径，将启动拆成两步：

```c
int TK8710FastStartPrepare(uint8_t workType, uint8_t workMode);
int TK8710FastStartTrigger(uint8_t workType);
```

其中：

- `TK8710FastStartPrepare()` 在等待 slot3 结束前执行，提前恢复必要寄存器。
- `TK8710FastStartTrigger()` 只做最终触发，减少临界点耗时。

实测 `triggerCost` 大多在 70us 到 90us 范围内。

### 2.4 测试命令

在 `TestTRMmain` 中增加 `k/K` 操作，用于运行时触发 TRM ACM 校准请求。

## 3. restartAdvanceUs调试过程

早期使用 `restartAdvanceUs=500us`，日志显示：

```text
elapsed=106307 us wait=110553 us restartAdvance=500 us
targetOffset=216860 us triggerLate=0 us triggerCost=73 us
```

随后调整为 `100us`：

```text
elapsed=107743 us wait=109517 us restartAdvance=100 us
targetOffset=217260 us triggerLate=0 us triggerCost=76 us
```

再调整为 `80us`：

```text
elapsed=106754 us wait=110526 us restartAdvance=80 us
targetOffset=217280 us triggerLate=0 us triggerCost=78 us
```

结论：

- `triggerLate=0us` 说明等待逻辑能命中目标时间。
- 最终触发写寄存器耗时约等于 `triggerCost`。
- 当 slot3 较短且硬件 slot 周期未变化时，`restartAdvanceUs` 取 80us 左右能让触发点贴近 slot3 结束。
- 后续 slot3 变长问题不是由 `restartAdvanceUs` 引起。

## 4. slot3变长后的异常现象

slot3 未加长时，校准后周期正常：

```text
slot3 window=217360 us
elapsed=106238 us wait=110922 us restartAdvance=200 us
s0PeriodBefore=688423 us
s0DeltaToStart=651988 us
```

slot3 加长后，校准前可以正常接收终端上行，但校准后后续周期异常：

```text
slot3 window=348432 us
elapsed=106368 us wait=241864 us restartAdvance=200 us
s0PeriodBefore=819480 us
s0DeltaToStart=783074 us
```

从这个阶段的日志看：

- fast start 启动点仍然接近原 slot3 结束位置。
- `s0DeltaToStart` 与 `s0PeriodBefore` 的差值约为 slot0 长度。
- 因此启动时刻本身基本对齐。

但是后续 S0 周期监控显示：

```text
ACM post S0[1]: period=819613 us before=819512 us diff=101 us
ACM post S0[2]: period=688412 us before=819512 us diff=-131100 us
ACM post S0[3]: period=688409 us before=819512 us diff=-131103 us
ACM post S0[4]: period=688432 us before=819512 us diff=-131080 us
```

结论：

- 校准后的第一个 S0 周期仍接近加长后的 819ms。
- 从第二个 S0 周期开始，周期回退到 688ms。
- 回退量约为 131ms。

## 5. slot寄存器排查

为确认是不是 slot 相关寄存器被 ACM 修改，增加了寄存器快照日志：

- `before-calib`
- `after-calib`
- `after-fast-prepare`
- `after-fast-trigger`
- `post-s0-1`
- `post-s0-2`

读取并打印以下寄存器：

- `mac_config_0`
- `mac_config_1`
- `init_1`
- `init_2`
- `init_3`
- `init_4`
- `init_18`
- `0x9810`
- `0x9814`
- `obv_1`
- `obv_2`
- `obv_3`
- `obv_4`

关键日志：

```text
before-calib:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,348432

after-calib:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,217360

after-fast-prepare:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,217360

after-fast-trigger:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,217360

post-s0-1:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,217360

post-s0-2:
mac1=0x00280016(s2=22,s3=40,crc=0)
init4=0x00004D10(da3=19728)
obv=36340,217360,217360,217360
```

## 6. 最终定位结论

### 6.1 不是配置寄存器被软件改掉

从快照看：

- `mac_config_1.s3_cfg` 一直为 40。
- `init_4.da3_m` 一直为 19728。
- `mac_config_0`、`init_1`、`init_2`、`init_3`、`init_18`、`0x9810`、`0x9814` 都没有变化。

因此，ACM 校准流程没有通过软件显式改掉 slot 配置寄存器。

### 6.2 实际变化的是obv_4

真正发生变化的是 `obv_4.s3_len`：

```text
before-calib: obv_4.s3_len = 348432
after-calib:  obv_4.s3_len = 217360
```

周期变化量与 `obv_4` 变化完全对应：

```text
348432 - 217360 = 131072 us
819453 - 688425 = 131028 us
```

两者误差约 44us，属于中断和调度抖动量级。

### 6.3 高概率触发点是TK8710SpiReset(1)

`tk8710_acm_calibrate()` 中的主要步骤包括：

- 写 `init_10`
- 写 `0x9478`
- 写 ACM 控制寄存器
- 修改 `irq_ctrl0.acm_irq_mask`
- 循环内调用 `TK8710SpiReset(1)`
- 触发 ACM
- 读取 SNR 和 ACM 因子

这些步骤里没有直接写 slot 配置寄存器。结合 `obv_4` 在 `after-calib` 已经变短，最可疑的是：

```c
TK8710SpiReset(1);
```

虽然该接口注释为“仅复位状态机”，但实测表现说明它会让硬件内部 slot 时长计算状态或影子状态回退，导致 `obv_4.s3_len` 从加长后的 348432us 变回 217360us。

## 7. 当前建议修复方向

在 `TK8710Ctrl(TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY)` 返回后，`TK8710FastStartPrepare()` 前，重新刷新 slot 相关配置寄存器，让硬件重新计算 `obv_1~obv_4`。

建议刷新寄存器包括：

- `mac_config_0`
- `mac_config_1`
- `init_1`
- `init_2`
- `init_3`
- `init_4`
- `init_18`
- `0x9810`
- `0x9814`，仅 mode10 需要特殊值

刷新后立即读取 `obv_4` 验证：

- 如果 `obv_4.s3_len` 恢复为 348432us，再进入等待和 fast trigger。
- 如果仍为 217360us，说明还缺少触发硬件重新装载 slot 时序的步骤，需要继续查硬件手册或启动触发依赖。

这个刷新动作放在 ACM 校准完成后的等待窗口内，不放在最终 `TK8710FastStartTrigger()` 临界路径上，因此不会明显影响 slot3 结束点对齐。

## 8. 后续验证建议

修复后重点观察以下日志：

```text
after-calib
after-slot-refresh
after-fast-trigger
ACM hidden in slot3
ACM post S0[1]
ACM post S0[2]
ACM post S0[3]
```

期望结果：

- `after-calib` 可能仍显示 `obv_4=217360`。
- `after-slot-refresh` 应恢复为 `obv_4=348432`。
- `post S0[1]`、`post S0[2]`、`post S0[3]` 周期都应保持约 819ms。
- `diff` 应保持在几十到几百微秒以内，而不是 `-131ms`。

## 9. 当前修复尝试

根据上述定位，已在 TRM ACM 流程中增加一次 slot 配置刷新：

```text
TK8710Ctrl(TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY)
TRM_RefreshAcmSlotConfig()
TRM_ReadAcmSlot3LenUs()
TK8710FastStartPrepare()
TK8710FastStartTrigger()
```

实现要点：

- ACM 执行前先备份一份 `slotCfg_t`。
- ACM 返回后用备份配置调用 `TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG)`。
- 刷新动作在等待 slot3 结束前完成，不放在最终 trigger 临界路径。
- 刷新后读取 `obv_4.s3_len`，输出 `TRM: ACM slot refreshed` 摘要。
- `waitUs` 改为根据刷新完成后的当前时间计算，只影响日志准确性，实际等待仍使用绝对目标时间 `restartTargetUs`。

下一轮板端验证重点：

- `afterCalib` 如果仍显示 `217360`，说明 ACM/reset 仍会让硬件计算值回退。
- `afterRefresh` 期望恢复为 `348432`。
- 如果 `afterRefresh` 恢复正常，继续看是否没有 `ACM post S0 period mismatch`。
- 如果 `afterRefresh` 仍是 217360，则说明仅重写 slot 配置寄存器不足以触发硬件重新计算 slot3 时长，需要继续确认是否还需要额外 reset/start/load 类触发。

## 10. 日志收敛

确认修复生效后，TRM 侧临时诊断日志已收敛：

- 移除正常路径中的 `ACM slot regs[...]` 全量寄存器快照。
- 保留 `TRM: ACM slot refreshed` 摘要，用于确认 `afterCalib` 和 `afterRefresh` 的 slot3 时长。
- 保留 `TRM: ACM hidden in slot3` 汇总日志，用于观察 `elapsed`、`wait`、`triggerLate`、`triggerCost` 等关键时序。
- post-S0 周期监控仅在偏差超过 5000us 时输出 WARN，正常周期不再连续打印。
- Driver 层 `Slot time lengths` 的 CONFIG WARN 保留，用于确认 slot 配置刷新后的 S0/S1/S2/S3 时长。

收敛后的正常日志期望：

```text
TRM: ACM calibration starts ...
Slot time lengths - S0:..., S1:..., S2:..., S3:...
TRM: ACM slot refreshed: before=... afterCalib=... afterRefresh=...
TRM: ACM hidden in slot3: ...
```

如果后续再次出现周期异常，应重点看：

- `ACM slot refresh mismatch`
- `ACM post S0 period mismatch`
- `afterRefresh` 是否没有恢复到预期 slot3 时长

## 11. 编译命令

RK3506 编译命令：

```bash
wsl -d Ubuntu-22.04 -- bash -lc 'cd /mnt/d/CodexWorkSpace/tk8710_0418 && source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup && chmod +x ./cmake/build_rk3506.sh && ./cmake/build_rk3506.sh'
```

当前诊断代码已通过 RK3506 编译，生成 `build_rk3506/TestTRMmain`。
