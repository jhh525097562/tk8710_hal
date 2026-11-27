# MATLAB 生成一帧数据与 c_model 收发对比

日期：2026-09-23。

本文保留定点基线结果。后续已将主入口默认配置改为 float（`double`），复测结果见 [Float 单帧收发对比记录](Float单帧收发对比记录.md)。复现本文的定点结果需显式指定 `--numeric-mode fixed`。

结论：已经完成“MATLAB 新生成一帧 → 保存参考 RX/TX → C++ 读取同一帧 → 自动对比”的实际运行。**RX 用户数、CRC 和 26 字节有效载荷一致；RX 信道/频率等数值信息并非完全一致；8 天线 TX 波形不一致。** 本次没有修改算法来消除差异，也没有设定一个宽松容限把差异判为通过。

## 1. 一条命令运行

```powershell
Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'
python .\testbench_GWRX_TX\run_matlab_compare.py --numeric-mode fixed
```

默认参考工程：`D:\Users\HJH\Desktop\HJH\K2301Cmodel\TKG810MatlabPlatform`。

本机实测环境：MATLAB R2021a（9.10.0.1602886）、GCC 8.1.0、Python 3.9.2、NumPy 2.0.2。脚本支持 `--reference`、`--matlab`、`--compiler` 和 `--timeout` 指定路径/超时。

每次创建独立的 `validation/matlab_compare_<时间戳>/`。默认每次都运行 MATLAB 重新生成一帧，不会读取旧 smoke 的输入。`--reuse-generated <某次matlab输出目录>` 仅用于显式复用已生成帧进行诊断，结果会记录复用路径。

退出码约定：0 表示 RX 有效载荷/CRC 与 TX 规范化波形的精确比较通过；1 表示流程完成但发现差异；2 表示生成、构建、运行或文件检查失败。本次返回 **1**，不是进程崩溃，也不是一致性通过。

## 2. 数据来源与一帧配置

使用参考工程 `TestCaseListNew3.xlsx` 数值区域第 95 行作为参数起点，经 `TestCaseListParaSet()` 生成配置，派生一个便于检查的单用户一帧用例：

| 项目 | 本次值 |
|---|---|
| 实际终端发送用户数 / 接收容量 | 1 / 1，不是旧多用户输入仅限制输出数量 |
| MATLAB 帧号 / C++ 帧号 | 1 / 0，均只运行一帧 |
| 随机种子 | 1；参考函数内部固定种子的行为保留 |
| 接收模式 / 发射模式 | 4 / 14 |
| 天线 | 8 路全部启用 |
| RF 采样率 / 网关基带采样率 | 1,000,000 / 125,000 sample/s |
| RX FFT / 窗口 / IFFT | 8192 / 3 / 1024 |
| C++ 输入长度 | 每天线 24,576 个复数采样，文件每行 `I1 Q1 ... I8 Q8` |
| SNR 配置 | 30 dB，开启原工程噪声模型 |
| 额外 CFO、频漂、时偏、RF 延迟、周期/单音/镜像干扰 | 本测试关闭 |
| 互易校准 | 关闭，校准系数为 8 路 `1+0j` |
| RX PHY 字节数 / 有效载荷 | 28 / 26 |
| TX PHY 字节数 / 有效载荷 | 48 / 46 |
| TX 功率配置 | 平均分配模式，模型总功率 150；本报告不将其解释为硬件功率 |
| TX 前缀 / 数据符号 / 上采样配置 | 前缀 48 个相位输入点、192 数据符号、上采样编码 3 |
| TX 载荷 | 46 字节确定性非零数据，`mod(17*(0:45)+3,256)`；S/F 均为 0 |

完整 MATLAB 配置和转换后的 91 个 C++ 参数分别保存在 `matlab_results.json`、`cmodel_args.txt` 和 `comparison.json` 中。这里是 case95 派生的验证用例，不是原 case95 的全部用户/100 帧性能测试。

## 3. MATLAB 实际执行与导出

[matlab_one_frame.m](matlab_one_frame.m) 按以下顺序调用原参考模型：

```text
TestCaseListParaSet
  → SimCode_TerminalTxParaSet
  → PHY2_1TerminalTxMainCode
  → UpLinkChannelModel
  → GWPHYRx
      ├→ 导出降采样/输入归一化后的 8 天线数据，供 C++ 读取
      └→ 保存解调用户、CRC、有效载荷、频率、AH、导频功率
  → PHYGWTx（使用 MATLAB 自己的 RX 信道结果）
      ├→ 保存 125 ksample/s 的 8 天线基带波形
      └→ 保存 1 Msample/s 的 8 天线最终上采样波形
```

没有直接调用 `TKG810MatlabTestCase()`，因为该函数内部会把 `DownLinkSimSwitch` 强制置为 0，无法完成本次 TX 参考导出。验证入口显式执行上下行模块，并校验 MATLAB 解调载荷与本次终端发送载荷相同。

为取得内部数据，运行目录中的 `matlab_hooks/` 自动生成 3 个参考函数副本：

- `GWPHYRx.m`：仅增加一个输出参数，返回算法实际使用的 `SigDownSampleRx`。
- `PHYGWTx.m`：增加基带波形、发射权重和逐用户波形输出；单用户 `parfor` 限制为 0 个 worker，在本地执行。
- `UpLinkChannelModel.m`：单用户 `parfor` 同样在本地执行，避免为单帧启动并行池。

这些副本通过检查原始签名后作机械替换，原 MATLAB 目录不被写入，原算法计算语句保留。`comparison.json` 记录参考 `.m` 文件哈希、Excel 哈希和副本哈希。此工程存在终端/GW 两套 `upsampling_new.m`；验证入口在 TX 阶段显式选择网关目录，具体 `which()` 解析结果也已保存。

## 4. C++ 修改与比较边界

[GWRXTX_testbench.cpp](GWRXTX_testbench.cpp) 增加 RX 结果文件和 TX 权重文件，并支持 `--tx-bits-file` 读取 MATLAB 导出的完整 PHY 输入 bit 序列。位顺序为每字节 MSB 优先；尾部预留 S/F 和 CRC 位，由原编码器执行 CRC。省略该选项时保留旧全零测试载荷。

对应关系：

| 比较对象 | MATLAB | C++ | 口径 |
|---|---|---|---|
| 接收数据 | `RxData` 的实际有效部分 | `rx_frame0.txt` 中的 PHY 字节 | 只按已知协议排除 C++ 尾部 2 字节，不按内容寻找/裁剪差异 |
| RX 用户数/CRC | MATLAB RX 结果 | C++ 完成分段后的 CRC 日志 | 两边都必须实际完成解调 |
| 频率、AH、导频功率 | MATLAB RX 结果 | C++ RX 缓存导出 | 报告数值误差，不误称为全部相等 |
| TX 波形 | `matlab_tx_8ant_baseband.txt` | `tx_frame0.txt` | 同为 125 ksample/s、8 天线、相同载荷/前缀/采样顺序 |
| TX 权重 | `matlab_tx_weights.txt` | `weights_frame0.txt` | 用于区分权重差异和波形调制差异 |
| MATLAB 最终上采样输出 | `matlab_tx_8ant_rf.txt` | 当前入口无对应已执行的 C++ RF 上采样输出 | 已保存，不能将其直接与 C++ 基带波形对比 |

C++ 发射矩阵乘法现有代码除以 `1024 * 1024 * 8 = 2^23`。因此同时记录原始数值对比 `tx_raw` 和明确还原这个固定缩放后的 `tx_canonical`。后者用于下表。未拟合增益、未旋转相位、未搜索延迟、未重采样，也未因误差较大而更改参考文件。

当前基带输出长度两边相等，所以比较覆盖全部 26,497 点，没有只截取一部分来判断通过。

## 5. 实测结果

最终完整流程目录：[matlab_compare_20260923_175248_040281](../validation/matlab_compare_20260923_175248_040281/)。MATLAB 生成约 43.76 秒，C++ 执行约 7.43 秒，不含编译；这些时间仅记录本次执行，不是性能优化结论。

### 5.1 RX

| 项目 | MATLAB | C++ | 结论 |
|---|---|---|---|
| 成功用户数 | 1 | 1 | 一致 |
| CRC | 0 错误 | 1 分段完成，0 错误 | 一致 |
| 有效载荷 | 26 字节 | 前 26 字节 | 26/26 字节一致 |
| 检测频率 | -10025.0244140625 | -10025.0234375 | 差值 +0.0009765625 Hz |
| 导频功率 | 3947674.7245195163 | 3981699.828125 | 不同，差值 34025.10360548366 |
| AH | 8 路复数 | 8 路复数 | 不同，各路相对误差约 0.135%～4.627% |

一致的 26 字节有效载荷（十进制）：

```text
0 1 0 77 37 23 47 88 101 137 106 174 52 223 6 170 106 142 35 50 204 246 79 176 223 228
```

MATLAB 的 `RxData` 是较长的预分配数组，只取 `TMTxByteLen - 2*CRCEn = 26` 个实际载荷字节；C++ 导出的完整 28 字节另外带尾部 `192 0`。不能把 MATLAB 的补零区、C++ 的 PHY 尾部当作有效载荷去比较。

### 5.2 TX 八天线

MATLAB 和 C++ 基带输出均为 **8 × 26,497 个复数采样**。另保存 MATLAB 1 Msample/s 的 **8 × 211,976** 个复数采样。

定义：复数 RMSE 为 `sqrt(mean(abs(C - M).^2))`，相对 L2 为 `norm(C - M)/norm(M)`；这里的 `C` 已乘 `2^23`。每路 26,497 点都存在精确数值差异，第一个差异从采样索引 0 开始。

| 天线 | 最大复数绝对误差 | 复数 RMSE | 相对 L2 误差 | 精确相等 |
|---|---:|---:|---:|---|
| 1 | 0.449818 | 0.411985 | 15.2766% | 否 |
| 2 | 0.522721 | 0.475479 | 10.3778% | 否 |
| 3 | 0.608942 | 0.564341 | 12.2258% | 否 |
| 4 | 0.743048 | 0.651481 | 11.9900% | 否 |
| 5 | 2.172027 | 2.094851 | 30.6343% | 否 |
| 6 | 0.663865 | 0.628240 | 15.7228% | 否 |
| 7 | 0.431646 | 0.395986 | 14.8650% | 否 |
| 8 | 0.198116 | 0.160723 | 15.4607% | 否 |

### 5.3 差异已出现的位置

TX 权重在与用户信号相乘之前已经不同，各路权重相对误差约 10.33%～30.71%，与最终波形误差量级接近。

`Cls_tx_beamforming.h` 中 `m_GWRxAH_fxp` 使用有符号 `wl=24, iwl=12, FXP_SAT_SYM`，分量上限为 `2047.999755859375`；信道恢复使用 `AH × 天线噪声平方根`。本次 `h_anoise.txt` 的第 2 路 I、第 3 路 Q、第 4 路 I、第 5 路 Q 分量导出为约 `±2048`，与该饱和边界吻合。

现有这个 debug dump 只输出 6 位有效数字，因此结果中的“4 个边界分量”按导出舍入精度识别。这是定点限幅的直接线索，**不将其表述为已经排除了其他差异来源**；RX AH、噪声估计、归一化、权重量化和调制/插值还需逐级核对。

本轮没有修改 `Cls_tx_beamforming.h`、FFT、SVD 或定点规则。后续若要求修复一致性，应先确认希望浮点模型一致、定点误差受控，还是中间节点 bitmatch，再针对这些差异处理。

## 6. 文件与证据

以下链接均指向最终完整运行：

| 文件 | 内容 |
|---|---|
| [comparison.json](../validation/matlab_compare_20260923_175248_040281/comparison.json) | 实测结论、逐天线误差、输入/源码/输出哈希、完整命令 |
| [matlab.log](../validation/matlab_compare_20260923_175248_040281/matlab.log) | MATLAB 实际生成和参考收发执行日志 |
| [build.log](../validation/matlab_compare_20260923_175248_040281/build.log) | C++ 编译日志，比较目标启用 `DUMP_MATLAB_tx_beamforming` |
| [cmodel.log](../validation/matlab_compare_20260923_175248_040281/cmodel.log) | C++ 实际 RX、TX、结束日志 |
| [UD_InData0.txt](../validation/matlab_compare_20260923_175248_040281/matlab/Cmodel_DataIn/case95/UD_InData0.txt) | 本次 MATLAB 新生成、实际被 C++ 读取的一帧输入 |
| [matlab_results.json](../validation/matlab_compare_20260923_175248_040281/matlab/matlab_results.json) | MATLAB RX 结果、载荷、配置、函数解析位置 |
| [matlab_one_frame.mat](../validation/matlab_compare_20260923_175248_040281/matlab/matlab_one_frame.mat) | 本次完整配置、输入、信道、RX 用户及 TX 波形等工作数据 |
| [MATLAB 基带 8 天线](../validation/matlab_compare_20260923_175248_040281/matlab/matlab_tx_8ant_baseband.txt) | 与 C++ 比较的 125 ksample/s 波形 |
| [MATLAB 最终 8 天线](../validation/matlab_compare_20260923_175248_040281/matlab/matlab_tx_8ant_rf.txt) | 1 Msample/s 波形 |
| [C++ RX 结果](../validation/matlab_compare_20260923_175248_040281/cmodel/K2201DemodModule_Data/case95/rx_frame0.txt) | 每行用户索引、PHY 字节数、频率、导频功率、16 个 AH I/Q 数值、接收字节 |
| [C++ TX 8 天线](../validation/matlab_compare_20260923_175248_040281/cmodel/GWTXModule/case95/tx_frame0.txt) | 原始缩放域的 C++ 波形 |
| [C++ TX 权重](../validation/matlab_compare_20260923_175248_040281/cmodel/GWTXModule/case95/weights_frame0.txt) | 每用户一行，8 天线 I/Q |
| [C++ 恢复信道](../validation/matlab_compare_20260923_175248_040281/cmodel/GWTXModule/case95/h_anoise.txt) | 限幅诊断数据 |

波形文本均按采样行、天线列保存：`I1 Q1 I2 Q2 ... I8 Q8`。MAT 文件中的波形为“天线 × 采样”复数矩阵。

最终输入 SHA-256：`9bd169d19305998bba3ce98b8a21b5a740318f0b4d914e33795a056ec71cdbc7`。

MATLAB 基带波形 SHA-256：`779f25b4f2dd93f1c511df372087c3574ecd96dc985bd10d32561704cda3f428`。

C++ 波形 SHA-256：`3ddc3964e913ba7427d1368ba279e2c7d81a1684957009959af56014266c586f`。

两次独立 MATLAB 生成的输入、MATLAB 基带/RF 波形及 C++ 波形哈希分别一致。重复性通过，不代表 MATLAB 与 C++ 的波形彼此相等。

## 7. 兼容性与尚未覆盖范围

- 原有 `run_smoke.py` 在本次改动后重新运行通过，旧全零 TX 载荷输出哈希仍为 `bbfdc7adb63dde972eb2592a429ababdd7fd062d6f7b7a08dcec0542d08627e3`。
- 原主入口和选项头文件在修改前备份于 `validation/baseline_before_matlab_compare_20260923/`。
- 修改范围为验证入口、数据导出/导入和新对照脚本；MATLAB 参考目录及原历史数据未修改。
- 仅验证一个确定性单用户、一帧、RX mode4 / TX mode14 用例；不覆盖所有模式、多用户干扰、弱信号边界、其他采样率或多帧状态。
- 未进行 C++ RF 上采样一致性、RTL 或板级验证；仅保存 MATLAB RF 输出供后续使用。
- 不以旧硬编码 `LostRate` 日志作为本次验收依据，也不把“RX 有效载荷相同”扩展成所有 RX 内部数值完全相同。
