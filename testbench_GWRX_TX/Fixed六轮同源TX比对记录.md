# Fixed 模式、默认最多6轮的同源 TX 比对

日期：2026-09-24。用户要求：SVD 默认最大轮数由10改为6，C_model 切回 fixed，并使用与 MATLAB 相同的 TX 输入重新比较。

本次[8天线I/Q局部总图](../validation/adaptive_svd_fixed_6_20260924_162930/tx_plots_20260924_164302_145889/all_antennas_zoom.png)及[完整图像索引（含各天线详图）](../validation/adaptive_svd_fixed_6_20260924_162930/tx_plots_20260924_164302_145889/图像索引.md)已补充。后续每次对比自动出图，见[自动生成说明](对比图自动生成说明.md)。

## 1. 已生效的配置

- `Cls_svd.h`：`SVD_CFG::max_sweeps` 默认值 **6**。
- 当前主入口 `testbench_GWRX_TX/GWRXTX_testbench.cpp` 默认定义 `Fix`，RX/TX 主要模板及 TX 末级乘加使用 `fxp`。
- `-DCMODEL_FLOAT_POINT` 可显式选择原 float/double 分支，`-DCMODEL_FIXED_POINT` 显式选择定点；同时指定会产生编译错误。
- `run_smoke.py`、`run_matlab_compare.py` 默认改为 fixed；已有明确用于 float 对照的脚本增加显式编译宏，避免随主入口默认值变化而悄悄切换模式。
- 自适应门限保持：相对相关门限 `2^-8`，归一化绝对相关门限 `2^-24`；一整轮无旋转提前结束，达到6轮仍有旋转则标记未收敛。
- **自定义开方完全未改，仍为22位**。`Cls_sqrt.h` SHA-256 保持 `72C93C3137BF2E8C4A40BB3EEDA2241840556340F7C94B0CCCA390B2049D6C55`。

`fixed` 指使用现有 fxp 模型；底层仍可能使用 double 保存数值和模拟定点操作，不等同于已经通过 RTL bitmatch。

## 2. 同一份 MATLAB TX 输入

继续复用上一轮从 MATLAB 保存帧导出的 **104 用户 TX 输入**。它来自128个终端上行的单帧，取 MATLAB 实际成功接收并用于 TX 的104个用户。

输入目录：`validation/imported_tx_float_20260924_152146_236522/matlab_tx/`。

读取相同的 `matlab_tx_inputs.txt`、`tx_bits.txt`、`tx_user_ids.txt`、`cmodel_args.txt`，包括用户集合/顺序、AH、噪声功率、频率、导频功率、TX 功率、校准和完整 PHY bit 序列。实际使用的参数与载荷分别回写后逐值核对，通过。内部进入 fxp 运算后产生的量化是本次被测内容，不应误称为输入文件不一致。

两边 mode14、单 block、CRC 开启、总功率150、125 ksample/s、8天线，每路26,497个复数采样。比对仅还原 C++ 既有的 `2^23` 缩放；没有增益拟合、相位旋转、时移或裁剪。

MATLAB 参考波形沿用原始模型结果，本次没有改成 MATLAB 定点仿真，也没有重新生成参考波形。

## 3. 实测结果

最终目录：[adaptive_svd_fixed_6_20260924_162930](../validation/adaptive_svd_fixed_6_20260924_162930/)。

日志明确包含：

```text
NUMERIC_MODE=fixed
TX_INPUT_SOURCE=MATLAB users=104
TX_FRAME frame=0 users=104 samples=26497 energy=5.73404e-08
RxFinished
```

| 项目 | 本次结果 |
|---|---:|
| TX 用户数 | 104，与 MATLAB 相同 |
| 实际 TX SVD 次数 | 104 |
| 满足门限 | 44 |
| 达到6轮上限，仍未收敛 | 60 |
| 平均轮数 | 5.8077 |
| 平均有效旋转次数 | 103.5288 |
| 权重相对 L2 误差中位数 | 1.2636% |
| 权重相对 L2 最大误差 | 133.4639% |
| C++ 执行时间 | 31.82秒，不含编译，仅作本机记录 |

| 天线 | TX 相对 L2 误差 | 复数 RMSE |
|---|---:|---:|
| 1 | 43.6502% | 1.946554 |
| 2 | 36.3162% | 1.638561 |
| 3 | 37.6847% | 1.582656 |
| 4 | 38.5106% | 1.342537 |
| 5 | 27.3839% | 1.140352 |
| 6 | 30.2006% | 1.476384 |
| 7 | 36.3114% | 1.512791 |
| 8 | 39.4288% | 1.864581 |

**结论：fixed 模式、6轮上限和同源输入均已验证生效；TX 权重及波形仍有明显误差，不能判为 MATLAB 一致性通过。** 44次“收敛”表示满足当前相关性门限，不是数值精度验收。其余60次的当前估计仍输出用于对比，未隐藏或剔除。

本次还处理了 RX，但 TX 明确覆盖为 MATLAB 导出的输入，因此 TX 不依赖 C++ RX 成功用户数。旧日志的 `LostRate` 仍采用mode4硬编码分母96，不属于本次TX判定依据。

## 4. 辅助矩阵重放与范围说明

同时用**此前 float TX 捕获的同一批104个求逆输入矩阵**重放6轮上限：

| 重放类型 | 满足门限 | 达到上限 | 平均轮数 |
|---|---:|---:|---:|
| fxp | 60 | 44 | 5.7019 |
| double | 76 | 28 | 5.7981 |

这与上面的实际 fixed TX 集成“44/60”不是同一个测试口径：集成中 AH 恢复、归一化和矩阵构造也走 fxp，会改变输入到 SVD 的矩阵。不能混用两组统计。

没有提高门限来减少未收敛数量，也没有恢复到10轮。运行检查逐次断言 `sweeps<=6`，并核对 `rotations+skipped=28*sweeps`。

## 5. 复现

```powershell
Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'
python .\testbench_GWRX_TX\validate_adaptive_svd.py --numeric-mode fixed
```

该命令重新构建、重放矩阵并执行同源 TX 比较，生成独立结果目录。脚本退出0表示流程和配置检查完成；数值是否一致请看 `result.json` 的 `tx.exact_equal`，本次为false，状态为 `completed_with_measured_accuracy_limits`。

若要在当前6轮上限下额外跑 float，可用同一命令的 `--numeric-mode float`；那是另一次实验，本报告没有声称已执行。

## 6. 证据和检查

- [完整 result.json](../validation/adaptive_svd_fixed_6_20260924_162930/result.json)：模式、配置、源码哈希、命令、轮数、权重和波形指标。
- [实际运行日志](../validation/adaptive_svd_fixed_6_20260924_162930/run.log)：包含104条 `TX_SVD` 状态。
- [TX 八天线波形](../validation/adaptive_svd_fixed_6_20260924_162930/cmodel/GWTXModule/case95/tx_frame0.txt)。
- [TX 权重](../validation/adaptive_svd_fixed_6_20260924_162930/cmodel/GWTXModule/case95/weights_frame0.txt)。
- [实际 TX 输入回读](../validation/adaptive_svd_fixed_6_20260924_162930/cmodel/GWTXModule/case95/tx_inputs_used.txt)。
- [实际 TX bit 回读](../validation/adaptive_svd_fixed_6_20260924_162930/cmodel/GWTXModule/case95/tx_bits_used.txt)。
- [fxp 辅助矩阵状态](../validation/adaptive_svd_fixed_6_20260924_162930/fixed_status.csv)；[double 辅助矩阵状态](../validation/adaptive_svd_fixed_6_20260924_162930/float_status.csv)。

SVD 回归测试新增“默认上限必须为6”检查，并通过正交矩阵、弱相关、轮数上限、重复调用状态、非法配置及 fixed/double 基本测试。受影响 Python 运行脚本语法检查通过。测试后源码哈希及开方文件未改变检查通过。

本轮改动前备份：`validation/baseline_before_fixed6_20260924/`。历史10轮结果保留，不作为当前默认配置的结果。未进行 RTL、硬件或所有模式的验证。
