# 5 个文件修改前后差异说明

## 对比范围

- 修改前：`538f34e`（`Improve payload tester serial reliability`）
- 修改后：`3306a78`（`Complete TMS570 payload integration and RF validation`）
- 分支：`TMS570ls3137_8710HAL`
- 对比对象：截图所列 5 个文件
- 完整补丁：`docs/compare_5_files_538f34e_to_3306a78.patch`

## Git 差异统计

| 文件 | 状态 | 新增 | 删除 | 主要变化 |
|---|---:|---:|---:|---|
| `tests/adc_telemetry_test.c` | 新增 | 20 | 0 | 验证 ADC 原始值到引脚电压、分压电源电压和热敏电阻温度的换算及边界值 |
| `source/fpga_protocol.c` | 修改 | 194 | 36 | 补充 boot flag 写入回读校验、故障位、自检、v0.5 遥测字段、ADC 遥测及遥控命令行为 |
| `tests/fpga_protocol_v05_test.c` | 新增 | 82 | 0 | 验证 v0.5 遥测字段和 checksum、天线编号转换及保留写寄存器命令拒绝行为 |
| `include/fpga_param_store.h` | 修改 | 3 | 0 | 增加参数损坏检测接口及 boot flag 主机测试注入接口 |
| `source/fpga_param_store.c` | 修改 | 70 | 0 | 实现参数损坏扫描、boot flag 保存失败和回读异常测试注入 |
| **合计** | 5 个文件 | **369** | **36** | 以上统计来自 Git 提交间的稳定差异 |

## 截图计数为何不同

截图显示 `+117/-49`，各文件计数为当时编辑器/代理界面记录的一轮中间编辑操作。该状态没有形成独立 Git 提交，因此 Git 中不存在可唯一寻址的“修改前”版本；后续同一批文件又有补充和修正，最终进入 `3306a78` 的稳定差异为 `+369/-36`。

本目录中的 `.patch` 以已上传 Git 的父提交为修改前版本，以当前提交为修改后版本，可用于审查，也可在相同基线上通过 `git apply` 重放。

## SCI 波特率说明

本次提交间对比中，`source/sci.c` 和 `include/sci.h` 均无差异。也就是说，先前提到的 SCI 波特率 `520 -> 4` 修改没有进入提交 `3306a78`，截图中的 5 个文件也不包含这两个文件。
