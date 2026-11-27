# SVD 自适应收敛修改与验证

日期：2026-09-24。用户要求：保持现有开方精度；按“每轮重置旋转标志、弱相关跳过、整轮无旋转提前结束、最大轮数及未收敛状态”修改。

后续更新：用户已将默认最大轮数指定为6，当前主入口默认fixed。本文保留最初10轮测试及当时配置，当前结果见 [Fixed六轮同源TX比对记录](Fixed六轮同源TX比对记录.md)。

本次已修改正式 `Cls_svd.h` 并完成矩阵及集成验证。**开方文件完全未修改，配置仍为22位。自适应控制通过验证，但它不等同于 MATLAB 数值一致性已经通过。**

## 1. 修改范围

| 文件 | 修改 |
|---|---|
| [../Cls_svd.h](../Cls_svd.h) | 可配置轮数/门限；相关性判断；跳过弱相关与零能量列；按整轮旋转标志提前停止；状态及统计；零开方结果保护 |
| [../Cls_tx_beamforming.h](../Cls_tx_beamforming.h) | 输出每次 TX SVD 的轮数、有效旋转、跳过次数及收敛状态，便于集成验证 |
| [test_adaptive_svd.cpp](test_adaptive_svd.cpp) | 单位矩阵、弱相关、轮数上限、复用/复位、零矩阵、配置检查及定点实例测试 |
| [replay_adaptive_svd.cpp](replay_adaptive_svd.cpp) | 用同一批104个真实 TX 矩阵分别重放 fixed 和 double SVD |
| [validate_adaptive_svd.py](validate_adaptive_svd.py) | 矩阵重放、原 MATLAB TX 输入集成对比和证据保存 |

未修改 `Cls_sqrt.h`、任何原有 SVD `fix_set()` 字长或量化配置、近似开方调用、伪逆门限、TX 内部 `Cls_svd<fxp>` 类型。也没有把诊断副本中的 `std::sqrt()` 合入。

`Cls_sqrt.h` 修改前后 SHA-256 均为：

```text
72C93C3137BF2E8C4A40BB3EEDA2241840556340F7C94B0CCCA390B2049D6C55
```

## 2. 配置和判据

`SVD_CFG` 新增有默认值的配置；原调用点只设置 `ant_num` 时也能安全使用默认值。

```cpp
SVD_CFG cfg;
cfg.ant_num = 8;
cfg.max_sweeps = 10;            // 上限，范围1～64
cfg.correlation_tol_shift = 8;  // 相对相关门限 tau=2^-8
cfg.absolute_tol_shift = 24;    // 归一化域绝对相关门限 eta=2^-24
svd.configure(cfg);
```

对当前两列已有的列能量及互相关统计：

```text
alpha = m_wk
beta  = m_wl
gamma = m_xiu

skip = |gamma|² <= max(2^(-2*correlation_tol_shift) * alpha * beta,
                      2^(-2*absolute_tol_shift))
```

列能量为零时也跳过。门限作用于 SVD 内部归一化、按当前模板类型量化后的统计量，不是外部 AH 原始幅度。绝对门限用于避免在极低能量方向继续追逐低于可用精度的相关量。

这两项门限是本次实现初值，不是已验收的 RTL 最优值。门限越宽，可能更早停止，但伪逆精度可能下降；门限越严，近似开方和定点量化可能使某些矩阵长期达不到条件。不能通过放宽门限直接宣称算法精度改善。

检查复用当前每对列已计算的能量、互相关，比较平方量，不新增开方或除法。RTL 可以用扩展位宽乘法、移位和比较实现对应判断；**本次 C_model 采用 double 承载这些比较乘积，RTL 的乘积位宽、舍入、溢出和时序仍需另行定义及验证**。

## 3. 一轮内及轮间控制

1. 每轮开始，清零 `m_rotated_this_sweep` 和 `m_blocked_this_sweep`。
2. 仍按原7组、每组4对的列对调度，逐对检查门限。
3. 满足跳过条件，保留 A、V 的对应两列，不执行后续开方/除法/旋转。
4. 实际执行旋转时置位轮标志，并累计有效旋转次数。
5. 完整一轮没有旋转，且没有精度阻塞，标记 `converged=true` 后结束。
6. 若相关性检查未通过，但后续原精度开方结果为零，保留两列并记录精度阻塞；整轮无法继续时以 `precision_limited=true` 结束，不冒充收敛。
7. 达到最大轮数且最后一轮仍有旋转，则标记 `hit_max_sweeps=true`、`converged=false`。

必须完整一轮无旋转才能确认：如果某轮较早检查的一对列已通过，但后来其他旋转改变了这些列，不能据此直接认定最终矩阵收敛。因此统计轮数包含最后一轮确认检查。

每次 `go()` 先重置矩阵迭代状态，保证连续处理不同矩阵不复用上一矩阵的 V；状态统计在下一次 `go()` 才替换。显式 `reset()` 保留上次结果状态，便于调用方在原复位流程后读取。

## 4. 对外状态

新增公开 `m_status`：

| 字段 | 含义 |
|---|---|
| `sweeps` | 实际执行轮数，包含最后一轮无旋转检查 |
| `rotations` | 实际执行的列对旋转数 |
| `skipped_pairs` | 跳过的列对数 |
| `rotations_per_sweep` | 每轮有效旋转数，最多64项 |
| `converged` | 满足当前相关门限；不保证权重/伪逆误差达到业务标准 |
| `hit_max_sweeps` | 达到上限仍未确认收敛 |
| `precision_limited` | 一轮不能旋转但被有限精度阻塞 |
| `precision_limit_encounters` | 相关检查未通过但开方为零的次数 |

未收敛输出明确日志：

```text
SVD_NOT_CONVERGED sweeps=10 rotations=... hit_max=1 precision_limited=0
```

TX 调用侧输出示例（本次真实日志）：

```text
TX_SVD sweeps=5 rotations=99 skipped=41 converged=1 hit_max=0 precision_limited=0 per_sweep=28,28,28,15,0
```

本次保留原调用链“继续输出当前估计”的行为，未增加业务层重试、改变正则化参数或丢弃用户。调用方可根据状态决定是否接受结果；当前验证报告将未收敛如实计入，不将其算作数值验收通过。

## 5. 回归检查

修改前，真实 `Cls_svd<double>` 对单位矩阵的重构平方残差为 `7.05877`，测试失败，反映原零相关旋转会破坏列。修改后此测试通过，且为1轮、0次有效旋转、28次跳过。

同一测试还通过了：

- double 和 fxp 的正交矩阵提前结束。
- 弱相关列在指定门限下不执行旋转。
- 强相关矩阵限制1轮时，明确 `hit_max_sweeps=1`。
- 每个调用 `rotations + skipped_pairs = 28 * sweeps`。
- 新调用清除旧未收敛状态；显式 reset 保留上次状态。
- 零矩阵不错误旋转；非法最大轮数拒绝。

可执行验证：

```powershell
Set-Location 'D:\CodexWorkSpace\K2301Cmodel\c_model'
& 'C:\Program Files\mingw64\bin\g++.exe' -std=c++11 -O1 -Wno-write-strings .\testbench_GWRX_TX\test_adaptive_svd.cpp -o .\validation\test_adaptive_svd.exe
& .\validation\test_adaptive_svd.exe
python .\testbench_GWRX_TX\validate_adaptive_svd.py
```

## 6. 同一批104个矩阵的实测

使用此前已捕获的 TX 求逆矩阵，开方同为现有22位，门限及上限使用默认值：

| 指标 | fixed SVD | double SVD |
|---|---:|---:|
| 矩阵数 | 104 | 104 |
| 满足门限、提前结束 | 61 | 104 |
| 达到10轮上限仍未收敛 | 43 | 0 |
| 精度阻塞结束 | 0 | 0 |
| 实际轮数范围 | 5～10 | 5～7 |
| 平均轮数 | 7.3654 | 6.0673 |
| 平均有效旋转 | 107.6346 | 100.0000 |
| 最大有效旋转 | 159 | 120 |
| 平均列对检查数 | 206.2308 | 169.8846 |
| SVD矩阵重构残差中位数 | 约2.1539% | 约2.1462% |

轮数分布：fixed 为5轮31个、6轮29个、7轮1个、10轮43个；double 为5轮21个、6轮55个、7轮28个。

计算量解读：

- 无条件10轮为280次列对旋转，当前 fixed 平均有效旋转减少约61.56%，double减少约64.29%。
- 原无条件3轮为84次旋转，当前 fixed 平均反而增加约28.14%，double增加约19.05%。**不能说比原3轮整体更省计算。**
- 被跳过的列对仍需计算能量与互相关，且保留原读写调度；上述节省指旋转后半段，不是实测RTL周期或面积节省。
- 最大轮数仍决定最坏执行时间；支持严格限制为3轮，但当前数据不能据此保证收敛。可通过状态明确哪些矩阵超过预算。

double 全部满足相关门限仍有约2%的重构残差，说明保持现有近似开方后，旋转正交性损失不会单靠自适应停止消失。`converged` 必须理解为控制判据满足，不是与 MATLAB 精确一致。

## 7. 当前 TX 集成结果

沿用正式外层 float，内部 TX SVD 仍是 `Cls_svd<fxp>`，没有擅自切换类型。导入上一轮 MATLAB 的同一批104用户 TX 输入，逐值及载荷检查通过。

- TX 内部104次SVD：61次满足门限，43次达到上限，日志均能对应。
- 权重误差中位数：此前约34.2356%，本次 **1.1417%**。
- 权重最大误差仍约 **133.1335%**，不能认定所有用户通过。
- 8天线最终TX相对L2误差：约 **20.62%～35.01%**，仍不一致。
- 输出长度仍为26,497采样/天线；本次没有更改输入、发送用户集合、载荷或输出缩放。

| 天线 | 本次 TX 相对 L2 误差 |
|---|---:|
| 1 | 22.5628% |
| 2 | 35.0150% |
| 3 | 23.9583% |
| 4 | 29.9132% |
| 5 | 21.6881% |
| 6 | 20.6244% |
| 7 | 27.4290% |
| 8 | 27.7061% |

## 8. 证据

- [本次完整结果 JSON](../validation/adaptive_svd_20260924_161930/result.json)：配置、开方哈希、源码哈希、命令、计数及数值结果。
- [fixed 104矩阵逐项状态](../validation/adaptive_svd_20260924_161930/fixed_status.csv)。
- [double 104矩阵逐项状态](../validation/adaptive_svd_20260924_161930/float_status.csv)。
- [TX 集成日志](../validation/adaptive_svd_20260924_161930/run.log)。
- [TX 八天线输出](../validation/adaptive_svd_20260924_161930/cmodel/GWTXModule/case95/tx_frame0.txt)。
- [TX 权重](../validation/adaptive_svd_20260924_161930/cmodel/GWTXModule/case95/weights_frame0.txt)。
- 原文件备份：`validation/baseline_before_adaptive_svd_20260924/`。

交付边界：完成了用户要求的控制修改、当前精度下的真实矩阵重放与集成测试。未提高开方精度，未修改RTL，未做硬件时序/面积验收，未将当前门限视为最终规格。下一步应结合RTL预算与权重误差要求决定门限、上限及未收敛处理策略，而不是隐藏43个未收敛结果。
