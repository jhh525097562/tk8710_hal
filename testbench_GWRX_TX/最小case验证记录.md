# GWRXTX 最小 case 验证记录

日期：2026-09-23。目标：按用户要求，先跑通指定收发入口的一个最小 case，暂不做算法优化。

本文为当时的定点 smoke 记录。后续默认模式已改为 float；复现这里的定点波形需加 `--numeric-mode fixed`，最新结果见 [Float 单帧收发对比记录](Float单帧收发对比记录.md)。

## 1. 已确认的项目基准

- C++ 主入口：`D:\CodexWorkSpace\K2301Cmodel\c_model\testbench_GWRX_TX\GWRXTX_testbench.cpp`。
- MATLAB 参考工程：`D:\Users\HJH\Desktop\HJH\K2301Cmodel\TKG810MatlabPlatform`。这是用户最后明确指定的目录，取代此前提供的 `8700MatlabSimPlatform` 目录。
- 本阶段通过条件：从当前源码构建可执行程序；完成一帧有效接收解调并进入发射链路；产生可检查的有效波形；正常退出。
- 本阶段不要求 MATLAB/RTL 逐 bit 对齐，也没有擅自设定数值误差容限。本报告的通过仅针对上述运行验证。

## 2. 本次结果

最终证据目录：[smoke_case95_20260923_172304_333673](../validation/smoke_case95_20260923_172304_333673/)。

| 检查 | 结果 |
|---|---|
| 当前源码编译与链接 | 通过；GCC 8.1.0，C++11，`-O1 -g -Wformat=2 -Wno-write-strings` |
| Windows 栈预留 | 链接参数 `-Wl,--stack,67108864`，64 MiB；为入口较大的栈对象预留空间 |
| 输入内容检查 | 393,216 个有限数值，正好是 `8192 × 3 × 8 × 2` |
| 接收 | 第 0 帧，成功接收 1 用户；完成 1 分段，CRC 错误数 0，解调字节数 28 |
| 发射 | 1 用户，8 天线，每天线 26,113 个复数采样 |
| 发射波形检查 | 26,113 行，每行 16 个 I/Q 数值；全部有限，总能量大于零 |
| 程序返回值 | 0，输出 `RxFinished` |
| 本次进程耗时 | 9.852097 秒，不含编译；不是性能优化基准 |
| 少参数 | 返回 2，并打印 Usage |
| FFT 档位传入 `3.5` | 返回 2，明确拒绝非法整数参数 |
| 输入帧文件不存在 | 返回 3，打印确切文件路径 |

关键实测输出：

```text
CaseNum:95 Mode:4
RX_DECODE frame=0 candidate=0 segments=1 crc_errors=0 bytes=28
FreamIndex:0 RxUserNum:1
TX_FRAME frame=0 users=1 samples=26113 energy=5.56167e-08
[Fix_Sim]Case:95 TotalRxNum:1 SimNum:1  LostRate:0.9895833333
RxFinished
```

这里的输入仍是现有 case95 多用户数据，不是重新生成的单用户信号。本次仅把接收容量 `GWAllRxUserNum` 从 128 限为 1，并只运行第 0 帧。旧 `LostRate` 仍以 mode4 的 96 用户为分母，因此 `0.9895833333 = 1 - 1/96` **不用于本次丢包率验收**，也不能作为完整 case95 的性能结论。

TX 使用原入口的全零测试载荷，经 CRC/编码/调制产生波形；不是接收到的 28 字节回显，也没有模拟终端对该下行波形进行解调。波形总能量 `5.561666349684913e-08` 是当前模型数值域中的平方和，不是射频功率。

## 3. 一条命令复现

在 PowerShell 中执行：

```powershell
Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'
python .\testbench_GWRX_TX\run_smoke.py --numeric-mode fixed
```

本次使用 Python 3.9.2 和 `C:\Program Files\mingw64\bin\g++.exe`。如位置不同：

```powershell
python .\testbench_GWRX_TX\run_smoke.py --numeric-mode fixed --compiler 'C:\Program Files\mingw64\bin\g++.exe' --input-root 'D:\data\K2301\K2301PHY_CaseList' --timeout 600
```

脚本会读取原 case95 参数行，验证输入、重新构建、执行错误输入检查、运行收发 case 并检查波形。每次创建新的 `validation/smoke_case95_<时间戳>/`；算法工作目录也位于该目录，使固定文件名的模块导出留在本次结果中。不会覆盖原始测试数据或历史参考输出。

脚本成功以 `PASS: RX=1, TX=1, samples/antenna=26113, ...` 结束；任何构建、运行、CRC 完成判据或波形检查失败都会返回非零并在 `result.json` 记录原因。可执行文件本身完成运行并不自动表示该 smoke 通过，最终判定由脚本完成。

## 4. 运行参数已还原

主入口使用 **91 项位置参数**，不是 `case, Iter` 两项。完整原始 case95 配置保存在 [GWRXCaseList.csv](GWRXCaseList.csv)；当前 smoke 的精确命令以字符串数组保存在 [command.json](../validation/smoke_case95_20260923_172304_333673/command.json)。

1～38 项 RX 参数见[项目说明第 6 节](../项目说明与后续优化验证.md)。本次只将第 15 项改为 1；其余位置参数沿用 case95，包括全部 TX 字段。

| 参数位置 | 变量 | case95 值 | 当前入口说明 |
|---|---|---|---|
| 39 | `TxUserNum` | 0 | 实际进入 TX 时由成功接收用户数覆盖 |
| 40 | `TxMode` | 14 | TX mode |
| 41 | `Freq` | 0 | 已解析，实际发送频率使用 RX 检测结果 |
| 42 | `AH[0]` | 0 | 已解析，实际 AH 使用 RX 结果 |
| 43 | `AHPower` | 0 | 按 double 解析；不替代实际导频功率 |
| 44～59 | `CalibrateFactor[0..15]` | 全部 1 | 按每路 I、Q 交织赋给 8 天线，即每路 `1+j`，不是 `1+0j` |
| 60 | `TxPowerFlag` | 1 | 当前功率分配模式 |
| 61 | `RecipracalCaliFlag` | 1 | 启用校准 |
| 62 | `MMSE` | 0 | 本次未覆盖小数正则化值 |
| 63 | `TotalTxPower` | 150 | 模型配置值，不在本次解释为物理功率 |
| 64～66 | `PowerDif1/2/3` | 10、15、20 | 功率分配参数 |
| 67～68 | `AGCLen/ToneLen` | 0、0 | 本次无附加 AGC/tone；`ToneLen` 原入口未实际接线 |
| 69 | `CodeRate` | 0.5 | 编码率 |
| 70 | `CRC_En` | 1 | TX CRC 开关 |
| 71 | `DSSSNUM` | 1 | TX 扩频配置 |
| 72 | `TransRatio` | 0.25 | 修正为 double 读取，避免原 `%d` 将其读成 0 |
| 73 | `TxBlockNum` | 1 | 本次一个发射 block |
| 74～81 | `Fbit[0..7]` | 全部 0 | 原入口保留解析，本次未增加协议行为 |
| 82～89 | `Sbit[0..7]` | 全部 0 | 同上 |
| 90 | `SymNumData` | 192 | 发射数据符号数；与 RX 第 33 项 112 不同 |
| 91 | `UpSampleNum` | 3 | 原入口的上采样配置编码 |

新增可选参数追加在这 91 项之后：

| 选项 | 作用 | 未指定时 |
|---|---|---|
| `--input-root <路径>` | `Cmodel_DataIn` 的上级目录 | 原 Linux 数据根路径 |
| `--output-root <路径>` | 模块输出和日志的根目录 | 原 Linux 数据根路径 |
| `--frames <数量>` | 限制输入帧数，从 0 开始 | 100 |

直接调用可执行文件前需建立各模块的 case 输出目录；推荐使用脚本自动准备这些目录。91 项参数读取增加了有限数值、整数类型和部分维度检查，但不代表所有模式、组合及异常输入已经穷尽验证。

## 5. 为跑通所做的必要修复

修改原文件两处，并新增两个运行支持文件：

| 文件 | 改动及原因 |
|---|---|
| [../Lib_complex.h](../Lib_complex.h) | `add/minus(Float_struct, Float_struct)` 的声明/定义返回类型改为实际返回的 `Float_struct`；解除 GCC 编译错误。未改内部算式 |
| [GWRXTX_testbench.cpp](GWRXTX_testbench.cpp) | Windows `mkdir` 兼容、位置参数检查、可指定输入/输出根和帧数、取消结束时 `system("pause")` |
| 同一入口 | 初始化配置结构和未赋值输入；修复 `%d` 读取 double 及 `TransRatio` 小数丢失 |
| 同一入口 | 先填充 TX BF 配置再 `configure/init`，避免按未初始化用户数分配；补充噪声平方根与逐天线校准系数传递 |
| 同一入口 | RX→TX AH 由错误的连续内存复制改为按 `[天线][用户]` 逐天线取值 |
| 同一入口 | 成功接收必须实际完成预期分段且 CRC 错误数为零；增加输入流失败检测和接收完成日志 |
| 同一入口 | 增加 TX 波形文件及有限值/非零检查，使“进入并完成 TX”有独立证据；修正入口局部数组释放方式 |
| [testbench_options.h](testbench_options.h) | 保存入口专用的选项解析与路径拼接，避免这些逻辑分散到算法库 |
| [run_smoke.py](run_smoke.py) | 最小 case 的构建、隔离运行、结果核验和证据记录 |

这些是构建、初始化、数据传递和测试入口修复，不是 FFT/SVD/BF 算法优化。`Cls_ud.h`、`Cls_GWRxBFModule.h`、`Cls_tx_beamforming.h` 等算法实现本次未修改。

原文件副本保存在 [baseline_before_smoke_20260923](../validation/baseline_before_smoke_20260923/)。本项目仍未初始化 Git；副本仅供比较和恢复本次改动，不替代正式版本管理。

复核范围包括上述改动、输入拒绝路径、当前源码构建和实际收发运行。未借本次修复其他旧入口，也未承诺全库内存安全：其他类析构中的历史 `new[]/free()` 混用等问题仍需后续专项处理。

## 6. MATLAB 参考目录核实结果

当前参考根目录下已确认存在：

| MATLAB 路径（相对参考根目录） | 对应范围 |
|---|---|
| `TKG810MatlabTestCase.m` | 单 case 组织、输入数据/日志目录、收发仿真流程 |
| `TestCaseListParaSet.m`、`TestCaseListNew3.xlsx` | 参数生成及配置表 |
| `TKG810MatlabTestCaseMain_Case92_105.m` | 批量驱动脚本；当前内部实际选择 `TestCaseList=[35]`、`SimNum=100`，文件名不代表实际运行 case95 |
| `1.1GWRxCode/GWPHYRx.m` | RX 流程，含 Cmodel 输入导出/回读片段 |
| `1.1GWRxCode/1.3UserDetection_Module/` | 用户检测规则与相关函数 |
| `1.1GWRxCode/1.4GWRxBF_Module/` | RX BF/SVD 相关处理 |
| `1.2GWTxCode/GW_Tx_Beamforming.m` | TX 波束赋形参考 |

`GWPHYRx.m` 中的 Cmodel 输入根恰为 `D:/data/K2301/K2301PHY_CaseList`，与本次输入位置一致；其中导出和回读片段均由 `if 0` 关闭。因此不能直接启动批量脚本就认为已经回读本次输入或完成了 C++ 对比。

这次只读检查 MATLAB 源码，没有修改/运行 MATLAB 工程，也没有重新生成输入。原数据与这份 MATLAB 快照的逐文件生成关系仍待后续建立。

参考文件身份快照：

| 文件 | SHA-256 |
|---|---|
| `TKG810MatlabTestCaseMain_Case92_105.m` | `1C01EEC7AB7019885FF9C0B1653F5DE044E7A574C1660AEBB83BAA0175F5CAA9` |
| `TKG810MatlabTestCase.m` | `C9CA38CF0F8EC543B2B05DB7E692ECD39BA56DD299DDDE71B9F4D34A223EEE39` |
| `1.1GWRxCode/GWPHYRx.m` | `0D5CC1032EB2BF33BBFB202EF29531C5228A71CA3FE1D39B5AF5286AD9197B8C` |
| `1.2GWTxCode/GW_Tx_Beamforming.m` | `7A88E1C80A686D8E6E19DC0AA115A0CFB0B6A75A20094FF3A2949E2BE5B3A04E` |

## 7. 证据与限制

- [result.json](../validation/smoke_case95_20260923_172304_333673/result.json)：构建/运行命令、工作目录、源码哈希、输入哈希、返回码、负向检查及波形统计。
- [build.log](../validation/smoke_case95_20260923_172304_333673/build.log)：本次所选编译选项下无诊断输出；不是所有告警类别的完整审计。
- [run.log](../validation/smoke_case95_20260923_172304_333673/run.log)：真实接收、发射和退出日志。
- [tx_frame0.txt](../validation/smoke_case95_20260923_172304_333673/GWTXModule/case95/tx_frame0.txt)：每行按 `I1 Q1 ... I8 Q8` 排列的波形。
- [negative_checks](../validation/smoke_case95_20260923_172304_333673/negative_checks/)：三项错误输入的日志。

输入 SHA-256：`2a21bcb5dd74857f084a08f1dcad5fd461b85163a571cc08c467d8428e527bcb`。

输出波形 SHA-256：`bbfdc7adb63dde972eb2592a429ababdd7fd062d6f7b7a08dcec0542d08627e3`。最初跑通版本与补齐参数检查后的最终版本产生相同波形哈希；该事实仅说明这两次 smoke 输出一致。

未验证：全 100 帧、全部接收用户、多用户 TX 分支、其他模式、下行终端解调、MATLAB 数值一致性、RTL bitmatch、RF/硬件表现。TX BF 小数 MMSE 类型、频率选择边界、零信道处理等既有问题未在本次优化，也未被本次单用户 smoke 覆盖。

本轮目标已完成。后续如用户要求扩大验证，优先在这份 MATLAB 参考上建立同一输入和参数的 RX/解调/波形对照，再决定精度门限与算法优化范围。
