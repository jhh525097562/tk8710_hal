# TMS570 模式3速率容量测试工具

本目录生成独立TMS570测试固件，用于验证遥控协议速率0、1、2映射到
TK8710 Driver速率6、7、8后，16用户及128用户的连续收发处理能力。

测试需要TMS570与TK8710硬件，不连接真实终端、NS或网关。它验证Driver、
SPI、中断、用户Buffer和回调处理容量，不代表真实RF空口或终端并发验收。

## 测试专用Driver接口

RX确定性数据通过内部测试接口注入。接口仅在定义
`TK8710_DRIVER_TEST_RX_INJECTION` 时存在：

- `TK8710TestRxInjectionEnable()`
- `TK8710TestRxInjectionSetUser()`
- `TK8710TestRxInjectionDisable()`

`build.ps1` 仅为本工具重新编译 `tk8710_irq.c`，并排除生产Driver IRQ对象。
生产构建不定义该宏，因此不导出这些符号，也不改变生产运行路径或公共API。

每个用户payload包含用户ID、帧序号、Driver速率和确定性校验数据。每帧均检查
唯一用户数量、CRC状态、ID、序号、速率和完整payload。

## 构建

```powershell
# 默认：6个组合，每项10帧、单帧超时5秒
.\tools\rate_mode_capacity_test\build.ps1

# 单项稳定性测试
.\tools\rate_mode_capacity_test\build.ps1 -Rate 1 -Users 128 -Frames 100

# Host统计逻辑测试
.\tools\rate_mode_capacity_test\run-host-tests.ps1
```

独立产物为 `build-tms570\rate_mode_capacity_test.out`。烧录全片前需与当前
Bootloader合并，不能用单独OUT覆盖Bootloader区域。

## 通过条件

- 每项连续完成配置帧数。
- 每帧观察到精确的16或128个不同用户。
- RX CRC错误、缺失、重复、越界和payload错误均为0。
- TX提交和完成数量等于预期，发送失败和遗漏均为0。
- Driver错误、超时和SPI错误均为0。
- 六项全部PASS时，`g_capacityTestExitCode` 为0；任一失败为1。

仅编译或Host测试通过不能作为实板结论，必须保留UART的六项汇总日志。
