# ACM 按 PPS 周期末帧调度

60分钟到期只产生pending请求。外部同步下，执行窗口为PPS周期最后一轮速率循环的
最后一个速率帧的S2结束；本地同步仍使用原有业务超帧末帧条件。

网关在HAL时隙配置成功后、启动之前调用
`TRM_SetAcmPpsSchedule(multiSlotOutput.frameCount, slotCfg.rateCount)`。
`framePeriod`为完整速率循环总时长，`frameCount`为每个PPS周期的循环数。
该参数独立于业务`tdd_num`，不改变业务帧号和超帧结构。

以PPS=7秒、单帧280ms为例，25帧构成一个PPS周期。首次实际S0建立第0帧锚点，
仅在第24号帧的S2末尾执行校准。多速率按S0的rateIndex序列计数，回到rate0才增加
循环计数，末轮的末速率帧才允许校准。速率序列异常请求退出，避免使用错误相位。
独立模计数不依赖累计业务帧号或S0统计计数的溢出；普通S3和校准补偿的虚拟S3均不
额外推进该计数，下一次实际S0继续轮转。

TRM初始化、NS重新配置及新硬件启动前重置调度锚点；ACM内部快速重启不调用重置API。
其他外部同步调用者也必须在启动前设置调度，否则ACM请求保持pending，不在任意帧执行。

在校准前，使用已有S0时间戳和当前时隙长度估算末帧S3结束时间；必须有S0锚点且
剩余时间大于`28000us × calibCount + guardUs + restartAdvanceUs`。不足时保留请求，
继续业务并等待下一周期。若每周期窗口都不足，请求持续等待，需要调整时隙或校准参数。

校准和刷新后，先切换到首速率，再执行FastStartPrepare/Trigger。准备后余量不足guardUs、
触发跨过末帧期限或驱动恢复失败时，请求致命退出；不按正常情况虚拟补一帧，避免掩盖
错过脉冲造成的多秒停机。正常路径只补一个业务S3，等待外部PPS启动。
恢复后的首次S0与预测时间偏差超过5ms时也请求退出。

新增日志：

```text
TRM: ACM PPS schedule reset: cycles=25 rates=1
TRM: ACM PPS-final frame: cycle=25/25 rate=1/1 remaining=... us
TRM: ACM first S0 after PPS restart: error=... us
```

验证：`test/example/test_trm_acm_pps.c`覆盖25帧单速率、40帧跨业务超帧、25轮4速率、
单周期边界、重复周期/虚拟S3、错误速率序列、无配置和时间预算边界。
Linux主机测试及RK3506交叉编译需分别记录。板端仍需验证无超帧7秒/280ms、
有超帧且frameCount大于tdd_num、多速率以及NS重新配置后的首个ACM；同时观察终端BCN、
广播TDD连续性和ACK，而不能仅依据校准valid=1判断业务恢复。

当前硬件接口没有提供独立的PPS输入边沿时间戳，调度基于首次外部启动与连续S0序列。
完整循环的IRQ漏计、长期帧时钟漂移仍需通过板端日志和脉冲测量验证；本改动不宣称
具备实际PPS边沿捕获能力。若触发后完全没有S0，沿用网关原有IRQ停滞检查。
