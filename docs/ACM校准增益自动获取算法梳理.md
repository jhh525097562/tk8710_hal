# ACM校准增益自动获取算法梳理

## 1. 文档目的

本文梳理 `src/driver/tk8710_config.c` 中 `tk8710_acm_get_gain()` 的实际代码行为，重点说明：

- 自动增益搜索前的ACM配置；
- 增益扫描范围与重复测量次数；
- 32路SNR的排列方式；
- 最佳`again`和`dgain`的更新条件；
- 天线0关联索引的特殊批量赋值；
- 最终增益合并、寄存器回写和文件保存；
- 当前实现中需要特别注意的边界和潜在风险。

本文只解释当前代码，不修改算法。代码位置以当前工作区版本为准：

- 全局增益数组：`tk8710_config.c:88-92`；
- 增益结果保存：`tk8710_config.c:123-176`；
- 自动增益函数：`tk8710_config.c:186-422`；
- SNR读取函数：`tk8710_config.c:430-546`；
- 完整ACM入口：`tk8710_config.c:1192-1212`；
- 独立调试入口：`tk8710_config.c:1561-1573`。

## 2. 调用关系

自动增益搜索有两个入口。

### 2.1 完整ACM流程

```text
TK8710Ctrl(TK8710_CTRL_TYPE_ACM_START, acmParam)
    |
    +-- tk8710_acm_get_gain()       自动搜索并写入增益
    |
    +-- tk8710_acm_calibrate()      使用搜索后的增益执行正式校准
```

如果自动增益搜索返回非`TK8710_OK`，完整ACM流程立即返回，不再执行正式校准。

### 2.2 独立调试入口

```text
TK8710DebugCtrl(TK8710_DBG_TYPE_ACM_AUTO_GAIN, ...)
    |
    +-- tk8710_acm_get_gain()
```

该入口只执行增益自动获取，不继续读取正式校准因子。

运行时的`TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY`不会再次执行自动增益搜索，而是直接使用当前
已配置增益执行`tk8710_acm_calibrate()`。

## 3. 关键变量和数组

### 3.1 四组全局增益数组

```c
static uint16_t g_acm_dgain0[16];
static uint16_t g_acm_dgain1[16];
static uint16_t g_acm_again0[16];
static uint16_t g_acm_again1[16];
```

| 数组 | 对应SNR组 | 代码日志含义 | 最终写入 |
|---|---|---|---|
| `g_acm_dgain0` | `TX0_SNR` | `SumSNRTx0` | `acm_ctrl9~16` |
| `g_acm_again0` | `RX0_SNR` | `SumSNRRx0` | `acm_ctrl17~24` |
| `g_acm_dgain1` | `TX1_SNR` | `SumSNRTx1` | `acm_ctrl9~16` |
| `g_acm_again1` | `RX1_SNR` | `SumSNRRx1` | `acm_ctrl17~24` |

代码把每组增益保存为16个元素。自动搜索时根据8个天线的SNR更新偶数索引，天线0还会触发
一批特殊索引赋值；奇数索引主要由该特殊逻辑覆盖。

### 3.2 SNR工作数组

| 数组 | 长度 | 用途 |
|---|---:|---|
| `TmpSNR` | 32 | 单次ACM触发得到的32个SNR |
| `SumSNR_1` | 32 | 当前增益档5次SNR的累计值 |
| `SumSNR_0` | 32 | 上一个增益档的累计SNR |
| `MaxSNR` | 32 | 已接受的历史最佳累计SNR |

### 3.3 固定参数

| 参数 | 当前值 | 实际含义 |
|---|---:|---|
| `SNR_THE` | 32 | 传给SNR统计函数的有效门限 |
| `GainStep` | 4 | 每档增益步长 |
| 外层扫描次数 | 15 | `i=1...15` |
| 增益候选值 | 4～60 | `GainStep * i` |
| 每档重复次数 | 5 | 每个候选增益触发5次ACM |
| 每次SNR数量 | 32 | 8天线×4类SNR |
| 总ACM触发次数 | 75 | 15档×5次，不含其他流程 |

## 4. 自动搜索前的准备

### 4.1 清零比较数组

函数首先执行：

```c
memset(SumSNR_0, 0, sizeof(SumSNR_0));
memset(MaxSNR, 0, sizeof(MaxSNR));
```

因此第一档增益4不是与初始增益16的实测结果比较，而是与全0基线比较。

### 4.2 重置全局增益

索引0～14统一初始化为16，索引15初始化为0：

```text
index 0～14：dgain0=dgain1=again0=again1=16
index 15：dgain0=dgain1=again0=again1=0
```

这里的16只是搜索前默认值。代码没有先用增益16触发ACM建立基线。

### 4.3 设置PA/LNA和ACM信号

准备流程如下：

1. 写`mac.init_10 = 1 << 3`，代码注释为关闭PA；
2. 写`0x9478 = 0x10100010`；
3. 写`mac.init_10 = 0x1a`，代码注释为关闭LNA；
4. 向`acm_ctrl1~8`写入16个校准频点位置；
5. 向`acm_ctrl25~32`写入数据通道校准相位；
6. 向`acm_ctrl41~44`写入数据通道Tone使能；
7. 向`acm_ctrl33~40`写入ACM通道校准相位；
8. 向`acm_ctrl45`写入`0x7fff`，使能ACM通道Tone；
9. 将`irq_ctrl0.acm_irq_mask`置0，打开ACM中断。

校准频点数组为：

```text
0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84,
0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0
```

这些是写入ACM硬件的频点位置码，不能仅从本函数推断其对应的绝对射频频率。

## 5. 增益扫描主流程

### 5.1 总体流程图

```text
初始化比较数组和全局增益
        |
配置PA/LNA、校准Tone和ACM中断
        |
        v
候选增益 G = 4, 8, 12, ... , 60
        |
        +-- 清零当前档 SumSNR_1[32]
        |
        +-- 重复5次：
        |      1. again/dgain全部写为G
        |      2. 触发ACM
        |      3. 等待中断并读取32路SNR
        |      4. 累加到SumSNR_1
        |      5. 只复位状态机
        |
        +-- 对32路分别比较并更新最佳增益
        |
        +-- SumSNR_0 = 当前档SumSNR_1
        |
扫描结束
        |
合并dgain与again最优结果
        |
回写acm_ctrl9~24
        |
关闭ACM中断并保存gain.txt
```

### 5.2 候选增益写入

外层循环为：

```c
for (i = 1; i < 16; i++) {
    G = GainStep * i;
}
```

得到候选增益：

| 档位i | 增益G | 档位i | 增益G | 档位i | 增益G |
|---:|---:|---:|---:|---:|---:|
| 1 | 4 | 6 | 24 | 11 | 44 |
| 2 | 8 | 7 | 28 | 12 | 48 |
| 3 | 12 | 8 | 32 | 13 | 52 |
| 4 | 16 | 9 | 36 | 14 | 56 |
| 5 | 20 | 10 | 40 | 15 | 60 |

每次写入的32位寄存器值为：

```text
regVal = G:G:G:G
```

即4个字节全部写相同增益。该值写入8个`acm_ctrl17~24`寄存器和8个
`acm_ctrl9~16`寄存器，因此在测量当前档时，所有again和dgain都使用同一个候选值。

### 5.3 每档重复5次

每个候选增益执行5次：

```text
写增益 -> 触发ACM -> 获取SNR -> 累加SNR -> TK8710SpiReset(1)
```

所以：

```text
SumSNR_1[k] = SNR1[k] + SNR2[k] + SNR3[k] + SNR4[k] + SNR5[k]
```

代码比较的是5次累计值，不是平均值。若5次都有效，则累计值除以5才是平均SNR。

## 6. 32路SNR排列

`tk8710_acm_get_snr()`读取`acm_obv17~24`，解析为4组、每组8个有符号8位SNR：

| `TmpSNR`范围 | SNR组 | 天线索引 |
|---|---|---|
| 0～7 | `SNR_tx0` | 0～7 |
| 8～15 | `SNR_rx0` | 0～7 |
| 16～23 | `SNR_tx1` | 0～7 |
| 24～31 | `SNR_rx1` | 0～7 |

也就是：

```text
TmpSNR[i1 + 8*0] -> TX0，决定dgain0
TmpSNR[i1 + 8*1] -> RX0，决定again0
TmpSNR[i1 + 8*2] -> TX1，决定dgain1
TmpSNR[i1 + 8*3] -> RX1，决定again1
```

`tk8710_acm_get_snr()`还统计满足以下条件的SNR数量：

```c
SNR >= snrThreshold && SNR < 60
```

在自动增益函数中，返回的`SNRNum`没有被使用。也就是说，`SNR_THE=32`只影响日志中的
“有效SNR数量”，不会阻止低于32的SNR参与增益累计和最佳值选择。

## 7. 最佳增益更新条件

每个天线、每类SNR独立执行相同的双条件判断。以`again0`为例：

```c
if (current_sum > previous_sum && current_sum > best_sum + 4) {
    selected_gain = G;
    best_sum = current_sum;
}
```

对应含义：

1. 当前增益档累计SNR必须高于紧邻的上一档；
2. 当前累计SNR还必须比历史已接受最佳值至少高5个整数单位，因为条件使用严格大于
   `MaxSNR + 4`；
3. 两个条件同时满足才更新增益；
4. 无论是否更新，当前累计SNR都会成为下一档的`SumSNR_0`。

### 7.1 “+4”不是单次SNR提高4 dB

`MaxSNR`是5次SNR之和，因此：

```text
累计值提高4，对应平均每次约提高0.8个SNR单位
```

又因为判断是`current_sum > best_sum + 4`，整数情况下实际最小累计提升为5，对应平均每次
约1个SNR单位。不能把该条件直接解释成单次SNR必须提高4 dB。

### 7.2 搜索不会提前停止

即使SNR已经下降，代码仍会继续扫描到增益60。后续若重新上升，并同时满足高于上一档和高于
历史最佳值的条件，仍可再次更新最佳增益。

### 7.3 第一档与0比较

第一档增益4的`SumSNR_0`和`MaxSNR`都是0。只要5次累计SNR大于4，就可能把最佳增益从
初始化值16改为4。代码没有测量增益0或初始化增益16作为搜索前基线。

## 8. 天线0的特殊索引映射

正常情况下，天线`i1`更新数组索引`i1*2`：

| 天线i1 | 正常更新索引 |
|---:|---:|
| 1 | 2 |
| 2 | 4 |
| 3 | 6 |
| 4 | 8 |
| 5 | 10 |
| 6 | 12 |
| 7 | 14 |

当天线索引`i1==0`时，代码不只更新索引0，而是将同一个候选增益批量写入：

```text
0、1、3、5、7、9、11、13
```

该规则同时用于`again0`、`dgain0`、`again1`和`dgain1`。

因此16个增益索引的来源为：

| 增益索引 | 最佳值来源 |
|---:|---|
| 0、1、3、5、7、9、11、13 | 天线0对应SNR |
| 2 | 天线1对应SNR |
| 4 | 天线2对应SNR |
| 6 | 天线3对应SNR |
| 8 | 天线4对应SNR |
| 10 | 天线5对应SNR |
| 12 | 天线6对应SNR |
| 14 | 天线7对应SNR |
| 15 | 固定保持0 |

这不是普通的一天线对应两个连续索引映射。它显然包含参考通道/校准拓扑的特殊约定，必须结合
ACM RTL寄存器定义确认；不能仅根据数组名推断索引就是物理天线号。

## 9. 最终增益合并

扫描结束后，代码不直接保留独立搜索出的again和dgain，而是遍历索引0～14，将二者合并。

### 9.1 普通索引

大多数索引使用整数平均：

```text
Tmp0 = (dgain0[i] + again0[i]) / 2
Tmp1 = (dgain1[i] + again1[i]) / 2
```

然后强制写回：

```text
dgain0[i] = again0[i] = Tmp0
dgain1[i] = again1[i] = Tmp1
```

这意味着搜索阶段得到的RX最优dgain与TX最优again最终被合并为同一个值。除法为整数除法，
奇数和会向下截断。

### 9.2 索引12特殊处理

索引12对应正常映射中的天线6：

```text
Tmp0 = dgain0[12]
Tmp1 = (dgain1[12] + again1[12]) / 2
```

随后仍执行：

```text
dgain0[12] = again0[12] = Tmp0
```

因此索引12的0组完全忽略`again0[12]`搜索结果，以`dgain0[12]`为准。

### 9.3 索引14特殊处理

索引14对应正常映射中的天线7：

```text
Tmp0 = (dgain0[14] + again0[14]) / 2
Tmp1 = dgain1[14]
```

随后执行：

```text
dgain1[14] = again1[14] = Tmp1
```

因此索引14的1组完全忽略`again1[14]`搜索结果，以`dgain1[14]`为准。

### 9.4 索引15

合并循环条件为`i < 15`，所以索引15不参加合并，保持初始化值0。

## 10. 寄存器回写格式

### 10.1 dgain写入`acm_ctrl9~16`

每个寄存器打包4个8位增益：

```text
bit31:24 = dgain1[2*i]
bit23:16 = dgain0[2*i]
bit15:8  = dgain1[2*i+1]
bit7:0   = dgain0[2*i+1]
```

即：

```c
regVal = dgain1_even << 24 |
         dgain0_even << 16 |
         dgain1_odd  << 8  |
         dgain0_odd;
```

### 10.2 again写入`acm_ctrl17~24`

打包顺序完全相同：

```text
bit31:24 = again1[2*i]
bit23:16 = again0[2*i]
bit15:8  = again1[2*i+1]
bit7:0   = again0[2*i+1]
```

由于前一步已经将again和dgain合并为相同值，正常情况下相同索引的again/dgain回写值相同。

## 11. SNR读取过程

每次`tk8710_acm_trig()`后，`tk8710_acm_get_snr()`执行：

1. 轮询`irq_res`的ACM中断位；
2. 最长等待100 ms；
3. 超时后仅打印警告并退出等待循环；
4. 写`irq_ctrl1.acm_irq_clr=1`清除ACM中断；
5. 读取`acm_obv17~24`；
6. 将8个32位寄存器解析成32个`int8_t` SNR；
7. 统计`snrThreshold <= SNR < 60`的数量；
8. 打印每根天线四组SNR并返回有效数量。

需要注意：自动增益函数不检查`tk8710_acm_get_snr()`返回的有效数量，也没有区分中断正常完成和
100 ms超时。即使超时，它仍会继续读取观察寄存器，并把读到的值纳入当前档累计。

## 12. 结果保存

回写寄存器后，函数：

1. 将`irq_ctrl0.acm_irq_mask`置1，关闭ACM中断；
2. 调用`tk8710_save_acm_gain()`；
3. 以追加模式写入`CaliFactor/gain.txt`；
4. 每次记录时间戳和16行增益。

文件格式：

```text
=== YYYY-MM-DD hh:mm:ss ===
index,dgain0,dgain1,again0,again1
0,...
1,...
...
15,...
```

如果创建目录、打开文件、写文件或关闭文件失败，自动增益函数返回错误。也就是说，硬件增益可能
已经回写成功，但仅因结果文件保存失败，调用者仍会收到失败返回值。

## 13. 算法伪代码

```text
function ACM_AUTO_GAIN:
    previous[32] = 0
    best[32] = 0
    all gain arrays[0..14] = 16
    all gain arrays[15] = 0

    configure ACM tones, phases and IRQ

    for gain in [4, 8, 12, ..., 60]:
        current[32] = 0

        repeat 5 times:
            write all hardware again/dgain = gain
            trigger ACM
            read 32 SNR values
            current += SNR
            reset ACM state machine

        for antenna in 0..7:
            for group in [TX0, RX0, TX1, RX1]:
                k = antenna + 8*group
                if current[k] > previous[k]
                   and current[k] > best[k] + 4:
                    update mapped gain index to gain
                    best[k] = current[k]

                previous[k] = current[k]

    for index in 0..14:
        merge dgain and again
        apply special rules for index 12 and 14

    write merged dgain to acm_ctrl9..16
    write merged again to acm_ctrl17..24
    disable ACM IRQ
    append gain result to CaliFactor/gain.txt
```

## 14. 示例

假设某一路`RX0`在连续增益档的5次累计SNR如下：

| 增益 | 当前累计SNR | 上一档累计 | 历史最佳 | 是否更新 |
|---:|---:|---:|---:|---|
| 4 | 120 | 0 | 0 | 是，120>0且120>4 |
| 8 | 135 | 120 | 120 | 是，135>120且135>124 |
| 12 | 137 | 135 | 135 | 否，137不大于139 |
| 16 | 134 | 137 | 135 | 否，未高于上一档 |
| 20 | 142 | 134 | 135 | 是，142>134且142>139 |
| 24 | 140 | 142 | 142 | 否 |

该路最终搜索结果为增益20。之后它还可能与对应again搜索结果做整数平均，所以寄存器最终值不一定
仍是20。

## 15. 当前实现的关键观察点

下表区分“确定的代码行为”和“需要硬件协议确认的设计意图”。

| 项目 | 当前代码行为 | 影响/待确认事项 |
|---|---|---|
| 搜索范围 | 4～60，步长4 | 是否覆盖硬件允许范围需查寄存器定义 |
| 初始值16 | 只写数组，不做基线测量 | 第一档增益4直接与0比较 |
| 每档采样 | 5次SNR求和 | `+4`门槛不是平均SNR提高4 |
| SNR门限32 | 只用于统计和日志 | 低于32的数据仍参与最佳增益选择 |
| ACM超时 | 警告后继续读寄存器 | 旧值或无效值可能进入累计 |
| 最佳条件 | 高于上一档且高于历史最佳+4 | 抑制小波动，但依赖累计值稳定性 |
| 天线0映射 | 批量更新8个非连续索引 | 需与ACM通道拓扑/RTL定义核对 |
| 索引12 | 0组只使用dgain0 | again0搜索结果被丢弃 |
| 索引14 | 1组只使用dgain1 | again1搜索结果被丢弃 |
| 最终合并 | again/dgain整数平均并强制相等 | 独立TX/RX最优点会被改变 |
| 文件保存 | 失败即函数失败 | 硬件可能已更新但API返回错误 |
| PA/LNA恢复 | 本函数末尾没有显式恢复 | 完整调用依赖后续校准流程处理状态 |

## 16. 建议验证数据

若要判断算法是否适合当前耦合器和天线阵列，建议一次运行至少保留：

- 每个候选增益下5次原始32路SNR；
- 每档`SumSNR_1[32]`；
- 每路最终`MaxSNR`和被选候选增益；
- 合并前的4组最优增益数组；
- 合并后的4组最终增益数组；
- `acm_ctrl9~24`回读值；
- 是否发生100 ms中断超时；
- `CaliFactor/gain.txt`对应时间戳；
- 同一工况连续多次运行的增益离散程度。

结合当前天线阵列测试，还应重点观察天线7、天线8参考路径中强弱耦合相差20～30 dB时，各路
最优增益是否大量落在搜索边界4或60，以及弱路径是否在整个扫描范围内都无法达到稳定SNR。

## 17. 总结

当前自动增益算法本质上是一次全范围离散扫描：

```text
15个增益档 × 每档5次ACM × 32路SNR独立比较
```

它为TX0、RX0、TX1、RX1分别寻找累计SNR较高的增益，然后根据特殊索引规则映射到16个元素，
最终再将again和dgain合并并回写硬件。

理解该算法时最容易混淆的三点是：

1. 比较对象是5次SNR之和，不是单次或平均SNR；
2. `SNR_THE=32`不参与最佳增益淘汰，只影响有效数量统计；
3. 搜索出的独立again/dgain不会原样使用，最终还要经过平均和索引12/14特殊处理。
