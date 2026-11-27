# Float 模式单帧收发复测

日期：2026-09-23。目标：将当前 c_model 切换到 float 模式，用上一次 MATLAB 生成的同一帧重新验证。

最新状态：float 相位回绕已正式修复，并完成直接使用 MATLAB 全部 TX 输入的 104 用户测试，见 [同源 TX 输入与回绕修复对比](../validation/imported_tx_float_20260924_152146_236522/同源TX输入与回绕修复对比.md)。本文数值为修复前基线。

后续 128 用户扩展结果见 [128 用户单帧收发对比报告](../validation/multiuser_float_20260924_150540_506860/128用户收发对比报告.md)。本文保留原单用户基线，不能用于解释 128 用户的接收比例或 TX 总误差。

2026-09-24 原因定位已完成：float 分支丢失隐式 `FXP_WRAP` 相位回绕，超范围相位进入上采样后产生 40 组、共 280 个局部异常点。独立 C++ 诊断副本在插值输入归一后消除了这些大偏差；正式代码未修改。详见 [TX 局部相位异常定位](TX局部相位异常定位.md)。

2026-09-24 图形复核：[8 天线 I/Q 总图、各天线详图及近零点分析](../validation/matlab_compare_float_20260923_180530_159111/tx_plots_20260924_143655/图像与指标说明.md)。每路 280 个点（1.0567%）贡献约 99.97% 的误差能量，其余区间 L2 误差约 0.26%；复数包络基本恒定，较大偏差并非近零分母造成。是否影响下行接收尚需另行解调验证。本次仅分析已有数据，未修改算法。

结论：已切换、重新编译并完成复测。**RX 有效载荷、CRC 和频率与 MATLAB 一致；TX 权重误差显著降低，但最终 8 天线波形仍不一致。** 本次没有调整算法公式或设置数值容限来判定通过。

## 1. 模式配置与复现

当前 `testbench_GWRX_TX/GWRXTX_testbench.cpp` 默认不定义 `Fix`，执行原有 `<double>` 分支；工程中的 float 模式实际使用双精度 `double`，不是单精度 `float`。

TX 最终乘法/加法临时变量原本硬编码为 `Complex<fxp>`，本次改为随模式选择的 `Complex<ModelScalar>`，避免顶层浮点而最终输出继续定点量化。原有固定 `2^23` 输出缩放保持不变。

使用刚才生成的同一帧复现本次 float 测试：

```powershell
Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'
python .\testbench_GWRX_TX\run_matlab_compare.py --numeric-mode float --reuse-generated .\validation\matlab_compare_20260923_175248_040281\matlab
```

不带 `--reuse-generated` 时仍会重新运行 MATLAB 生成一帧。两个运行脚本 `run_matlab_compare.py`、`run_smoke.py` 均支持 `--numeric-mode float/fixed`，默认 `float`。

复现定点基线：

```powershell
python .\testbench_GWRX_TX\run_matlab_compare.py --numeric-mode fixed --reuse-generated .\validation\matlab_compare_20260923_175248_040281\matlab
```

`fixed` 构建显式传入 `-DCMODEL_FIXED_POINT`。可执行文件打印 `NUMERIC_MODE=float (double)` 或 `NUMERIC_MODE=fixed`，脚本核对实际日志并在 `comparison.json` 记录模式，防止运行了错误版本。

## 2. 输入与测试边界

- 复用上次 MATLAB 已生成的 case95 派生单用户一帧，包括完全相同的 RX 输入、91 个参数、TX bit 文件及 MATLAB 参考结果。
- 输入 SHA-256：`9bd169d19305998bba3ce98b8a21b5a740318f0b4d914e33795a056ec71cdbc7`。
- RX mode4、TX mode14；8 天线；每边 TX 基带均为 125 ksample/s、26,497 个采样/天线。
- 比较时只还原已有的 `2^23` 固定输出缩放；无拟合增益、相位旋转、时移或裁剪。
- C++ 实际运行约 5.97 秒，不含编译；不是性能验收结果。

本次沿用工程已有的 float 分支，并非对全库进行“纯浮点重写”：

- `Cls_rx_data_phy2_1.h` 内 Viterbi 的 `m_state_metrics` 仍是固定声明的 `fxp`。
- `Cls_tx_beamforming.h` 内的 SVD 仍固定实例化为 `Cls_svd<fxp>`；本次仅一个用户，TX 走单用户波束分支，没有执行这个 SVD 求逆路径。
- 算法中的近似开方、常量、索引/舍入逻辑仍保留，切换数据类型不等于与 MATLAB 浮点函数逐操作一致。
- UD 内另有固定类型辅助成员 `m_tx`，未找到其实际调用；不能仅按类模板标签声称所有内部对象均为 double。

## 3. RX 结果

| 项目 | MATLAB | float c_model | 结论 |
|---|---:|---:|---|
| 成功用户 | 1 | 1 | 一致 |
| CRC 错误数 | 0 | 0，已完成 1 分段 | 一致 |
| 有效载荷 | 26 字节 | 26 字节 | 逐字节相同 |
| 频率 | -10025.0244140625 | -10025.0244140625 | 完全相同；定点基线差 0.0009765625 Hz |
| 导频功率 | 3947674.7245195163 | 3947753.0118419533 | 差 78.28732243692502；定点基线差 34025.10360548366 |
| AH 各路相对误差 | — | 约 0.0147%～0.0759% | 显著降低，但不完全相同 |

因此“RX 一致”只适用于上述用户数、CRC、有效载荷和频率，不包括全部内部数值。

## 4. TX 结果与定点基线对比

恢复信道的 float 导出值能够超过 ±2048，例如第 5 天线 Q 约为 -3539.39；此前对应定点分量约为 -2048，原限幅在这一浮点赋值处不再发生。

各路 TX 权重相对误差从定点的约 **10.33%～30.71%** 降至 float 的约 **0.0046%～0.0424%**。但最终波形仍存在较大局部差异：

| 天线 | 定点 TX 相对 L2 误差 | float TX 相对 L2 误差 | float 复数 RMSE | float 最大复数绝对误差 |
|---|---:|---:|---:|---:|
| 1 | 15.2766% | 15.5431% | 0.419173 | 5.392793 |
| 2 | 10.3778% | 15.5472% | 0.712325 | 9.164311 |
| 3 | 12.2258% | 15.5441% | 0.717514 | 9.231060 |
| 4 | 11.9900% | 15.5453% | 0.844656 | 10.866798 |
| 5 | 30.6343% | 15.5467% | 1.063125 | 13.677480 |
| 6 | 15.7228% | 15.5464% | 0.621191 | 7.991844 |
| 7 | 14.8650% | 15.5424% | 0.414029 | 5.326611 |
| 8 | 15.4607% | 15.5470% | 0.161620 | 2.079293 |

两边形状相同，但所有天线均未精确相等；比较脚本返回 1，状态为 `completed_with_differences`，C++ 算法进程自身正常返回 0。

补充定位：对每路计算 `abs(C*2^23-M)/abs(M)`，均有 280/26,497 个采样点超过 1%；其余点误差较小。第 1 路首批较大误差索引为 753～759、881～887、1009～1015（从 0 计数）。1% 只是用于描述误差分布的诊断阈值，不是通过标准。

这表明消除信道限幅后，仍有下游波形差异。优先检查调制相位、插值及频移边界；目前尚未通过逐级对比证明具体根因，本轮没有修改这些算法。

## 5. 证据与兼容性检查

- [float comparison.json](../validation/matlab_compare_float_20260923_180530_159111/comparison.json)：输入/源码哈希、完整命令、模式、逐天线误差。
- [float cmodel.log](../validation/matlab_compare_float_20260923_180530_159111/cmodel.log)：`NUMERIC_MODE=float (double)`、`Float_Sim`、实际 RX/TX 完成日志。
- [float TX 八天线输出](../validation/matlab_compare_float_20260923_180530_159111/cmodel/GWTXModule/case95/tx_frame0.txt)。
- [float TX 权重](../validation/matlab_compare_float_20260923_180530_159111/cmodel/GWTXModule/case95/weights_frame0.txt)。
- [float RX 结果](../validation/matlab_compare_float_20260923_180530_159111/cmodel/K2201DemodModule_Data/case95/rx_frame0.txt)。
- [本次 fixed 兼容性复测](../validation/matlab_compare_fixed_20260923_180559_600515/comparison.json)：重新切回定点后，TX 波形 SHA-256 与上次定点基线完全相同。
- [上次 MATLAB 参考数据](../validation/matlab_compare_20260923_175248_040281/matlab/)：本次只读复用，未重新生成或修改。

float TX 波形 SHA-256：`cfd2a57783635fa548236dbacf2d582e0642ca0d2b5124d707b70dc0fab25b64`。

fixed TX 波形 SHA-256：`3ddc3964e913ba7427d1368ba279e2c7d81a1684957009959af56014266c586f`，与之前一致。

修改文件仅为主入口、对比脚本和 smoke 脚本；原文件备份在 `validation/baseline_before_float_20260923/`。未修改 MATLAB、输入、参考结果或算法头文件。本次不覆盖多用户 SVD、其他模式、RF 上采样、RTL 或硬件验证。
