# TMS570LS3137 HDK 开发资料整理与上手指南

版本：V2.1<br>
整理日期：2026-09-21<br>
实物依据：用户照片中的 TI Hercules HDK，芯片丝印 `TMS570LS3137...ZWT...`，板卡丝印 `ASSY 4197282 REV-E`  
目标平台：TMDS570LS31HDK / TMS570LS3137ZWT  
推荐工具链：CCS Classic/Eclipse 12.x + HALCoGen v04.07.01

## 1. 快速结论

这块板不是 `TMS570LC4357 / LAUNCHXL2-570LC43`，而是 TI 官网销售的 `TMDS570LS31HDK`，核心 MCU 是 `TMS570LS3137ZWT`。后续资料、工程、HALCoGen 设备选择和 SPI 引脚都应按 `TMS570LS31x/21x HDK` 路线处理。

先跑通建议路线：

1. 使用外部 `+5 V` 到 `+12 V` 电源给 HDK 的 `P1` 圆孔供电，优先按用户指南推荐使用 `+12 V`、中心正极电源。
2. 用板载 `J7 Mini-B USB` 连接 PC；该接口提供 `XDS100v2 JTAG` 和 SCI 虚拟串口。
3. 开发环境优先使用 `CCS 12.x Classic/Eclipse + HALCoGen v04.07.01`，不要把首次跑通卡在 CCS 20.5.1 Theia 的 Project Wizard。
4. HALCoGen 设备选择 `TMS570LS3137ZWT`，示例优先参考本机目录 `D:\ti\Hercules\HALCoGen\v04.07.01\examples\TMS570LS31x_21x`。
5. 第一阶段先跑 `RTI Blinky` 或 NHET LED，第二阶段跑 SCI 串口，第三阶段再做 SPI/MibSPI。

必须优先阅读的资料：

| 优先级 | 资料 | 用途 |
| --- | --- | --- |
| P0 | [TMDS570LS31HDK 产品页](https://www.ti.com/tool/TMDS570LS31HDK) | HDK 官方入口、设计文件、用户指南、推荐软件 |
| P0 | [TMS570LS31x HDK User's Guide, SPNU509C](https://www.ti.com/lit/pdf/SPNU509) | 板卡电源、接口、JTAG、SCI、扩展口、LED、DIP、跳线 |
| P0 | [TMS570LS3137 产品页](https://www.ti.com/product/TMS570LS3137) | 芯片官方入口、datasheet、TRM、Errata |
| P0 | [TMS570LS3137 Datasheet, SPNS162C](https://www.ti.com/lit/gpn/tms570ls3137) | 芯片特性、封装、引脚、电气参数 |
| P0 | [TMS570LS31x/21x TRM, SPNU499C](https://www.ti.com/lit/ug/spnu499c/spnu499c.pdf) | 外设寄存器、MibSPI/SPI、VIM、DMA、Clock、System Control |
| P0 | [HALCoGen](https://www.ti.com/tool/HALCOGEN) | Hercules 初始化代码和外设驱动生成 |
| P0 | [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) | 编译、下载、调试 IDE |

关键事实：

- `TMDS570LS31HDK` 是基于 `TMS570LS3137` 的 Hercules HDK，官方页面说明板上带 RJ45 10/100 Ethernet、两个 CAN transceiver、板载 XDS100v2 JTAG、外设 IO 引出。
- `TMS570LS3137` 是 Arm Cortex-R4F，最高 180 MHz，3 MB Flash，256 KB RAM，支持 EMAC、FlexRay、3 个 DCAN、3 个 MibSPI、2 个标准 SPI、LIN/SCI、I2C、N2HET、MibADC。
- HDK 用户指南列出 `J9` 引出 `SPI1 / SPI5 / ADC`，`J10` 引出 `SPI2 / EMIF / ECLK`，`J11` 引出 `SPI3 / GIO / NHET / DCAN / LIN`。
- 板上有 `J15 SD card`，用户指南框图标注为 `SPI2 / SD Slot`，因此做 SPI 实验时要注意 SPI2 可能与 SD 卡电路相关。
- 对《妙月卫星载荷概要设计方案 - v0.4》而言，HDK 适合作为 TMS570L 控制软件和双 CAN 的地面原型平台，但不能直接作为完整载荷硬件或飞行件：RS422/PPS 差分接口、39.6 V 至 48.6 V 电源、片外 64 MB Flash、6 路模拟量调理、独立看门狗、主备切换和 8 通道射频均需外接电路或定制载荷板。

## 2. 官方资料索引

### 2.0 本地已下载资料

以下文件已下载到当前资料目录，后续优先打开本地文件：

| 类别 | 本地文件 |
| --- | --- |
| HDK 用户指南 | `01_TI官方文档/板卡资料/SPNU509C_TMS570LS31x_HDK_User_Guide.pdf` |
| HDK 设计文件 | `02_TMS570LS31HDK设计文件/SPNR034_TMS570LS31x_HDK_Design_Files.zip` |
| 芯片数据手册 | `01_TI官方文档/芯片资料/SPNS162C_TMS570LS3137_Data_Manual.pdf` |
| TRM 技术参考手册 | `01_TI官方文档/芯片资料/SPNU499C_TMS570LS31x_21x_TRM.pdf` |
| Errata Rev C | `01_TI官方文档/芯片资料/SPNZ195_TMS570LS31x_21x_Errata_RevC.pdf` |
| Errata Rev D | `01_TI官方文档/芯片资料/SPNZ222_TMS570LS31x_21x_Errata_RevD.pdf` |

### 2.1 板卡资料

| 资料 | 链接 | 建议阅读方式 |
| --- | --- | --- |
| TMDS570LS31HDK 产品页 | <https://www.ti.com/tool/TMDS570LS31HDK> | 官方入口，确认当前设计文件和文档 |
| HDK User's Guide | <https://www.ti.com/lit/pdf/SPNU509> | 先看供电、JTAG、SCI、扩展口、DIP、跳线 |
| HDK Design Files | 产品页 Design files 中的 `SPNR034.ZIP` | 查原理图、BOM、PCB；做外接线和 SPI 时必须查 |
| Hercules Safety MCU Demos | 产品页 Related design resources | 初次板卡自检和演示 |

板卡资料优先确认：

- `P1` 电源输入，用户指南写明 HDK 支持 `+5 V` 到约 `+12 V` 输入，典型推荐 `+12 V` 外部电源。
- `J7` Mini-B USB 是板载 `XDS100v2` 接口，同时 FT2232H 第二通道提供 SCI 虚拟串口。
- `J4` 是 20-pin ARM JTAG；插入外部 JTAG 后板载 XDS100v2 JTAG 会被禁用，`DS1` 指示 ARM JTAG 插入。
- `S2` DIP 默认全 `OFF`；用户指南说明 Ethernet 使用时需 `S2:4 ON` 且其它相关开关关闭，USB Host/Device 与 Ethernet/PinMux 有互斥点。
- `J8` 选择 ADC 供电选项，`J13` 控制板载 SDRAM。

### 2.2 芯片资料

| 资料 | 链接 | 用途 |
| --- | --- | --- |
| TMS570LS3137 产品页 | <https://www.ti.com/product/TMS570LS3137> | 芯片资料总入口 |
| Datasheet / Data Manual | <https://www.ti.com/lit/gpn/tms570ls3137> | 特性、封装、引脚、电气参数、模块数量 |
| TRM / Technical Reference Manual | <https://www.ti.com/lit/ug/spnu499c/spnu499c.pdf> | 系统架构、寄存器、外设编程模型 |
| Silicon Errata Rev C | <https://www.ti.com/lit/pdf/spnz195> | 如果芯片是 Silicon Revision C，按此复核 |
| Silicon Errata Rev D | <https://www.ti.com/lit/pdf/spnz222> | 如果芯片是 Silicon Revision D，按此复核 |

阅读原则：

- HDK 接线和跳线以 `SPNU509C + SPNR034.ZIP 原理图` 为准。
- 芯片能力和引脚复用以 `SPNS162C Datasheet + SPNU499C TRM` 为准。
- 已知硬件/硅版本限制以 Errata 为准；先确认芯片丝印和硅版本，再选对应 Errata。

### 2.3 本机已安装资源

当前本机可用资源：

```text
D:\ti\Hercules\HALCoGen\v04.07.01\drivers\TMS570LS3137ZWT
D:\ti\Hercules\HALCoGen\v04.07.01\examples\TMS570LS31x_21x
D:\ti\ccs1281\ccs\ccs_base\common\targetdb\devices\tms570ls3137.xml
D:\ti\ccs1281\ccs\ccs_base\arm\include\TMS570LS313xFlashLnk.cmd
D:\ti\ccs1281\ccs\tools\compiler\ti-cgt-arm_20.2.7.LTS
```

示例优先看：

| 示例文件 | 用途 |
| --- | --- |
| `example_rtiBlinky.c` | LED/定时器基础验证 |
| `example_sci_uart_9600.c` | XDS100v2 SCI 虚拟串口验证 |
| `example_spi_Master_Slave.c` | 标准 SPI 主从基础参考 |
| `example_mibspiDma.c` | MibSPI + DMA 参考 |
| `example_mibspi_trigger_tick.c` | MibSPI 触发/周期传输参考 |
| `example_canCommunication.c` | 板载 CAN 接口参考 |
| `example_EMAC_Loopback_TxRx.c` | Ethernet/EMAC 参考 |

## 3. 编译环境搭建

### 3.1 推荐工具链

推荐组合：

- `CCS 12.x Classic/Eclipse`：用于首次创建、编译、下载、调试 TMS570LS3137 HALCoGen 工程。
- `HALCoGen v04.07.01`：选择 `TMS570LS3137ZWT`，生成启动、系统、驱动代码。
- `TI ARM Compiler Tools` legacy CGT：HALCoGen 示例和旧 Hercules 工程通常按 `armcl` 时代的 TI ARM CGT 验证。

不推荐把首次跑通放在 CCS `20.5.1.12__1.11.1` Theia Project Wizard 上：

- 当前安装中 Theia Project Wizard 可能找不到 `TMS570LS3137` / `TMS570LS31x`。
- 当前 CCS 20.5.1 安装目录里只确认看到 TI Arm Clang `tiarmclang.exe`，未确认有 legacy `armcl.exe`。
- 若目标是快速把 HDK 下载运行，先用 CCS 12.x Classic/Eclipse 更稳。

### 3.2 创建 CCS 工程

在 CCS 12.x Classic/Eclipse 中：

1. `File -> New -> CCS Project`。
2. Target 选择 `TMS570LS3137` 或 `TMS570LS313x` 系列项。
3. Connection 选择板载 `Texas Instruments XDS100v2 USB Debug Probe`；如果使用外部仿真器，则选实际 XDS 型号。
4. Project name 使用英文，例如 `tms570ls3137_hdk_base`。
5. Output type 选择 `Executable`。
6. Template 选择 `Empty Project`。
7. Linker command file 使用 `TMS570LS313xFlashLnk.cmd`；当前本机路径为 `D:\ti\ccs1281\ccs\ccs_base\arm\include\TMS570LS313xFlashLnk.cmd`。HALCoGen 生成目录中的 `sys_link.cmd` 也可作为工程链接脚本来源。
8. Finish 后确认工程在 Project Explorer 中。

如果使用 CCS 20.5.1：

- 不要继续在 Device 下拉里硬找 `TMS570LS3137`。
- 可用 `File -> Open Folder` 打开 HALCoGen 工程目录做编辑。
- 是否能直接构建取决于是否能补齐 legacy TI ARM CGT 或完成 TI Arm Clang 迁移；首次上板不建议走这条路线。

### 3.3 创建 HALCoGen 工程

1. 打开 HALCoGen。
2. `File -> New -> Project`。
3. Device 选择 `TMS570LS3137ZWT`。
4. Project name 建议与 CCS 工程同名，例如 `tms570ls3137_hdk_base`。
5. Location 指向 CCS 工程目录。
6. 取消 `Create directory for project`，让 HALCoGen 直接把 `source`、`include`、配置文件生成到 CCS 工程目录。
7. 先不改外设，直接 `File -> Generate Code`。
8. 回到 CCS，刷新工程，确认生成了 `source`、`include`。

### 3.4 CCS 编译配置

在 CCS 工程属性中确认：

- Include path 加入：

```text
${PROJECT_ROOT}/include
```

- 编译器选 legacy TI ARM Compiler，输出格式按 HALCoGen/CCS 工程默认设置。
- Linker 使用 `TMS570LS313xFlashLnk.cmd` 或 HALCoGen 生成的 `sys_link.cmd`，不要同时加入多个内存布局互相冲突的 `.cmd` 文件。
- 若工程提示找不到 HALCoGen 头文件，先检查 `include` 路径，不要先改源码。
- 若出现 endian、ABI、VFP 相关报错，优先使用 CCS 12.x 自带的 TMS570LS3137 默认工程设置。

### 3.5 下载与调试

1. HDK 先接 `P1` 外部电源，确认 `DS2/DS3/DS4/DS5` 等电源 LED 状态。
2. 用 `J7 Mini-B USB` 连接 PC。
3. CCS Debug 配置选择 `XDS100v2 + TMS570LS3137`。
4. 编译后 `Run -> Debug` 下载。
5. 若使用外部 JTAG，确认 `J4/J19` 插入后板载 XDS100v2 JTAG 会被禁用，`DS1` 会指示外部 JTAG 插入。

## 4. HDK 板卡资源速查

| 资源 | 板上标号 | 说明 |
| --- | --- | --- |
| MCU | U? / 主芯片 | `TMS570LS3137ZWT`，337-ball BGA |
| 电源输入 | `P1` | `+5 V` 到约 `+12 V`，典型按用户指南使用 `+12 V` |
| 板载调试 | `J7` | Mini-B USB，XDS100v2 JTAG + SCI VCP |
| 外部 JTAG | `J4` | 20-pin ARM JTAG |
| ETM Trace | `J19` | 30x2 MIPI ETM |
| Ethernet | `J1` | RJ45，DP83640 PHY |
| CAN1/CAN2 | `J2/J3` | 3-pin 螺丝端子 |
| SD 卡 | `J15` | SD card slot，按框图与 SPI2 相关 |
| 扩展口 P1 | `J9` | SPI1、SPI5、ADC |
| 扩展口 P2 | `J10` | SPI2、EMIF、ECLK |
| 扩展口 P3 | `J11` | SPI3、GIO、NHET、DCAN、LIN |
| 用户 LED | D3/D4/D5/D6/D7/D8/LED1/LED2 | 由 NHET1 信号控制 |
| DIP | `S2` | 默认全 OFF；Ethernet/USB/PinMux 注意互斥 |
| 跳线 | `J8/J13` | ADC 电压选择、SDRAM 使能 |

## 5. SPI/MibSPI 配置使用

### 5.1 先选接口

TMS570LS3137 有 3 个 MibSPI 和 2 个标准 SPI。HDK 用户指南明确给出扩展口分布：

| 接口 | HDK 连接位置 | 适合用途 |
| --- | --- | --- |
| MibSPI1 | `J9` P1，SPI1 引出 | 普通外接 SPI 外设、先做示波器验证 |
| MibSPI5 | `J9` P1，SPI5 引出 | 多通道 SIMO/SOMI 或 MibSPI 实验 |
| SPI2 | `J10` P2，并与 SD Slot 框图相关 | SD 卡/板载相关电路或外接 SPI2 验证 |
| MibSPI3 | `J11` P3，SPI3 引出 | 另一路外接 SPI/MibSPI 实验 |

建议首个外设实验优先用 `J9` 上的 `MibSPI1`，因为它在用户指南中连续给出 CLK、SIMO、SOMI、CS、ENA 引脚，便于逻辑分析仪接线。

### 5.2 J9 / MibSPI1 关键信号

按 HDK 用户指南 `Expansion Connector P1 (J9, Left, BottomView)`：

| 信号 | MCU ball | J9 引脚 |
| --- | --- | --- |
| MibSPI1ENA | G19 | 5 |
| MibSPI1CLK | F18 | 6 |
| MibSPI1CS[1] | F3 | 7 |
| MibSPI1CS[0] | R2 | 8 |
| MibSPI1CS[3] | J3 | 9 |
| MibSPI1CS[2] | G3 | 10 |
| MibSPI1SIMO | F19 | 11 |
| MibSPI1SOMI | G18 | 12 |
| GND | - | 13 / 14 |

接线注意：

- HDK 扩展口是 3.3 V 逻辑；外设必须兼容 3.3 V，不能直接接 5 V SPI。
- 优先接 `CLK/SIMO/SOMI/CS0/GND`，`ENA` 暂时不用。
- 表格中 J9 是 Bottom View，实际从板正面接线时要结合用户指南图和原理图确认方向。
- 做真实外设前先用 internal loopback 或示波器验证波形。

### 5.3 J10 / SPI2 关键信号

按 HDK 用户指南 `Expansion Connector P2 (J10, Right, BottomView)`：

| 信号 | MCU ball | J10 引脚 |
| --- | --- | --- |
| SPI2_SOMI | D2 | 59 |
| SPI2_SIMO | D1 | 61 |
| SPI2_CS1 | D3 | 62 |
| SPI2_CS0 | N3 | 63 |
| SPI2_CLK | E2 | 64 |

注意：

- 用户指南框图把 SD Slot 标到 `SPI2`，所以使用 SPI2 前必须查 `SPNR034.ZIP` 原理图，确认是否与 `J15 SD card` 或其它板载电路共用。
- 如果只是学习 SPI，优先用 J9/MibSPI1，避免一开始遇到板载 SD 卡共线干扰。

### 5.4 J11 / MibSPI3 关键信号

按 HDK 用户指南 `Expansion Connector P3 (J11, Bottom One, TopView)`：

| 信号 | MCU ball | J11 引脚 |
| --- | --- | --- |
| MibSPI3CS[3] | C3 | 71 |
| MibSPI3CS[2] | B2 | 72 |
| MibSPI3SIMO | W8 | 73 |
| MibSPI3SOMI | V8 | 74 |
| MibSPI3CS[1] | V5 | 75 |
| MibSPI3CS[0] | V10 | 76 |
| MibSPI3ENA | W9 | 77 |
| MibSPI3CLK | V9 | 78 |
| EXP_12V / GND | - | 79 / 80 |

注意 J11 是 TopView，与 J9/J10 的视图方向不同，接线前必须对照用户指南和板上丝印。

### 5.5 HALCoGen 中的 SPI/MibSPI 配置流程

建议从 MibSPI1 轮询方式开始：

1. `Driver Enable`
   - 使能 `MibSPI1`。
2. `PINMUX`
   - 选择 `MibSPI1CLK`、`MibSPI1SIMO`、`MibSPI1SOMI`、`MibSPI1CS[0]`。
   - 暂时不要启用不需要的复用功能。
3. `MibSPI1 -> Global / General`
   - 选择 Master。
   - 第一版先开 internal loopback，验证软件链路。
4. `MibSPI1 -> Data Format`
   - 先用 8-bit word。
   - CPOL/CPHA 按外设 datasheet 设置；无外设时先用 Mode 0。
   - 波特率先低速，例如 100 kHz 到 1 MHz。
5. `MibSPI1 -> Transfer Group`
   - 配置 TG0，buffer 长度先设 4 到 8 个 word。
   - 使用软件触发、one-shot、非 DMA。
6. 生成代码后，在 `mibspi.h` / `mibspi.c` 中确认实际 API 名称。

典型调用顺序：

```c
#include "sys_common.h"
#include "system.h"
#include "mibspi.h"

void app_mibspi1_polling_test(void)
{
    uint16 tx_data[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint16 rx_data[4] = {0U};

    mibspiInit();

    mibspiSetData(mibspiREG1, 0U, tx_data);
    mibspiTransfer(mibspiREG1, 0U);

    while (mibspiIsTransferComplete(mibspiREG1, 0U) == 0U)
    {
    }

    mibspiGetData(mibspiREG1, 0U, rx_data);
}
```

实际工程需确认：

- `mibspiREG1` 是否对应 HALCoGen 里启用的 MibSPI1。
- TG 编号是否是 `0U`。
- buffer 长度、word 长度和数组长度一致。
- 关闭 loopback 后再接 J9 实际引脚测波形。

### 5.6 验证顺序

1. 只生成 HALCoGen 空工程，确认可编译、可下载。
2. 跑 `example_rtiBlinky.c` 思路，确认用户 LED 可控。
3. 跑 SCI 9600 串口，确认 J7 的虚拟串口可用。
4. MibSPI1 internal loopback，确认收发数组一致。
5. J9 上测 `MibSPI1CLK/SIMO/CS0` 波形。
6. 接外部 SPI 设备，先读固定 ID 或状态寄存器。
7. 再尝试 MibSPI DMA、trigger tick 或高速传输。

## 6. CAN/DCAN 开发使用

### 6.1 能力、接口与边界

`TMS570LS3137` 集成 3 个 DCAN 控制器，每个控制器有 64 个 message object，支持经典 CAN 2.0A/2.0B，最高 1 Mbit/s。它不支持 CAN FD；如果星上接口协议要求 CAN FD、超过 8 字节的数据段或超过 1 Mbit/s，则当前 MCU 方案不满足。

HDK 对其中两路配置了板载物理层：

| 逻辑接口 | 板载收发器 | 连接器 | 用途建议 |
| --- | --- | --- | --- |
| DCAN1 | `U21 / SN65HVDA541-Q1` | `J2` | 对应方案中的 CAN A |
| DCAN2 | `U22 / SN65HVDA541-Q1` | `J3` | 对应方案中的 CAN B |
| DCAN3 | 无板载收发器 | `J11 pin 11/12` 仅引出 `CAN3RX/CAN3TX` 逻辑信号 | 需外接 CAN 收发器后才能接总线 |

J2、J3 均为 3 针螺丝端子，按 HDK 用户指南和 Rev-E 原理图：

| 引脚 | 信号 |
| --- | --- |
| pin 1 | CANH |
| pin 2 | GND |
| pin 3 | CANL |

原理图中的关键电气事实：

- U21/U22 的总线侧各带一组分裂终端：`62 ohm + 62 ohm`，两电阻中点通过 `4.7 nF` 接地，因此每个 J2/J3 自带约 `124 ohm` 的 CANH-CANL 终端。
- 收发器 `STB` 默认由 `10 kohm` 下拉，板卡上电后处于正常收发模式；上拉位置标为 `NOP`。
- J2/J3 不隔离。原理图上未见隔离器、TVS 或共模电感，不能把该接口直接等同于满足星载 EMC、浪涌、地电位差或单点故障要求的最终接口。
- CAN1、CAN2 是两条独立总线，不能为了“冗余”而把 J2 与 J3 长期并联到同一条星上总线。方案要求的是两路同时监听、同一时刻仅一路有有效业务。

终端与接线规则：

1. 总线只在物理两端各保留一个约 `120 ohm` 终端，支线尽量短。
2. HDK 已自带约 `124 ohm` 终端，不要再在同一 HDK 端重复并联 `120 ohm`。
3. 断电测量时，单独一个 J2 或 J3 的 CANH-CANL 应约为 `124 ohm`；HDK 与另一个带 `120 ohm` 终端的节点连成总线后应约为 `60 ohm` 至 `62 ohm`。
4. 实验室自环可将 `J2-1` 接 `J3-1`、`J2-2` 接 `J3-2`、`J2-3` 接 `J3-3`。此时 J2/J3 两端各有约 `124 ohm`，总线等效约 `62 ohm`，不要再加第三个终端。
5. 与外部设备连接时先断电确认 CANH、CANL、GND 和终端阻值，再上电；不要用 MCU 的 `CANxTX/CANxRX` 逻辑脚直接连接 CANH/CANL。

### 6.2 当前工程的 CAN 状态

当前工程“生成了驱动”不等于“CAN 业务已经可用”。静态检查结果如下：

| 检查项 | 当前状态 | 结论 |
| --- | --- | --- |
| `source/can.c`、`include/can.h` | 已生成，CAN1/2/3 初始化代码存在 | 驱动源码可用于继续开发 |
| 位速率 | CAN1/2/3 均为 `500 kbit/s`，采样点 `75%`，VCLK1 为 `80 MHz` | 只是当前 HALCoGen 值，尚未由星上 CAN 接口协议确认 |
| Message Box | CAN1/2/3 的 64 个 message object 当前全部未启用 | 不能正常接收或发送业务帧 |
| VIM | CAN1 high-level channel 16、CAN2 high-level channel 35 当前均未 Enable | 中断接收尚不可用 |
| 应用初始化 | `source/sys_main.c` 未调用 `canInit()` | APP 运行时没有启用 CAN |
| 通知函数 | `canMessageNotification()`、`canErrorNotification()`、`canStatusChangeNotification()` 为空 | 无接收分发和错误记录 |

因此，当前可以确认的是“HDK 硬件具备双 CAN 原型条件，HALCoGen 已生成基础驱动”；不能确认“当前固件已经实现 CAN A/B 遥控、广播和遥测”。

### 6.3 HALCoGen 配置步骤

在修改前先从星务/综电接口协议确认位速率、11/29 位标识符、各帧 ID、DLC、周期、超时、字节序和应用层校验。不能把本工程当前的 `500 kbit/s` 或示例 ID 当作最终协议。

建议配置流程：

1. `Enable Drivers`
   - 启用 `CAN` driver。
   - 保留 `CAN1` 和 `CAN2`；CAN3 只有在外接第三颗收发器时才作为总线接口使用。
2. `CAN -> CAN1 / CAN2`
   - 两路使用相同且与综电一致的 bit rate、sample point 和 SJW。
   - 第一轮板间测试可暂用当前 `500 kbit/s / 75%`，但测试记录必须标为实验值。
3. `Message Box`
   - CAN1、CAN2 分别配置相同的一组 RX message object，用于遥控、广播、遥测请求。
   - 分别配置 TX message object，用于遥测应答或状态上报。
   - 按协议选择 Standard ID 或 Extended ID，设置准确的 ID mask 和 DLC；不要默认开放全部 ID。
   - RX message object 启用 receive interrupt；TX complete 是否启用中断取决于发送队列设计。
4. `VIM`
   - 启用 CAN1 high-level interrupt，当前设备映射为 VIM channel 16。
   - 启用 CAN2 high-level interrupt，当前设备映射为 VIM channel 35。
   - 使用 IRQ；只有明确的安全分析要求时才改为 FIQ。
5. `Generate Code`
   - 生成后检查 `can.c` 中确实出现 message object 初始化，而不只是 CAN 控制器位时序初始化。
   - 把业务代码放在应用文件或 HALCoGen `USER CODE` 区，避免下次生成覆盖。

一个不绑定最终 ID 的 message object 规划示例：

| Message Box | CAN1 | CAN2 | 说明 |
| --- | --- | --- | --- |
| MB1 | RX 遥控 | RX 遥控 | 两路 ID/mask/DLC 相同 |
| MB2 | RX 广播 | RX 广播 | 两路同时监听 |
| MB3 | RX 遥测请求 | RX 遥测请求 | 记录请求来源通道 |
| MB4 | TX 遥测应答 | TX 遥测应答 | 只在当前有效通道发送 |

表中的帧 ID 必须来自后续定稿的《设备遥控遥测数传上注接口协议》，本文不自行分配。

### 6.4 最小轮询收发验证

先使用 CAN1 发、CAN2 收的同板物理总线测试，验证控制器、收发器、连接器和终端。HALCoGen 中需先把 CAN1 MB1 配成 TX、CAN2 MB1 配成 RX，两者 ID 和 DLC 均相同，然后按 6.1 节连接 J2/J3。

```c
#include "can.h"

static int CanPollingSelfTest(void)
{
    uint8 txData[8] = {0x54U, 0x4DU, 0x53U, 0x35U,
                       0x37U, 0x30U, 0x01U, 0x02U};
    uint8 rxData[8] = {0U};
    uint32 timeout = 1000000U;
    uint32 i;

    canInit();

    if (canTransmit(canREG1, canMESSAGE_BOX1, txData) == 0U)
    {
        return -1;
    }

    while ((canIsRxMessageArrived(canREG2, canMESSAGE_BOX1) == 0U) &&
           (timeout > 0U))
    {
        timeout--;
    }
    if ((timeout == 0U) ||
        (canGetData(canREG2, canMESSAGE_BOX1, rxData) == 0U))
    {
        return -2;
    }

    for (i = 0U; i < 8U; i++)
    {
        if (rxData[i] != txData[i])
        {
            return -3;
        }
    }
    return 0;
}
```

该测试不是内部 loopback：必须实际经过 DCAN1 TX、U21、J2/J3 总线、U22 和 DCAN2 RX。反向再做一次 CAN2 TX -> CAN1 RX，才能覆盖两路收发方向。测试代码需要超时，不能使用无限等待，否则接线错误或总线无 ACK 时会把主循环永久卡死。

### 6.5 中断接收与 CAN A/B 业务结构

生产业务建议采用“中断搬运 + 主循环解析”，不要在 `canMessageNotification()` 内执行协议解析、Flash 擦写、RS422 发送或长时间日志输出。

中断层至少记录：

- 来源控制器：`canREG1` 为 CAN A，`canREG2` 为 CAN B。
- message box、帧 ID、DLC、8 字节数据、接收时间戳。
- 每通道 RX/TX 计数、溢出计数、最后有效帧时间。
- error warning、error passive、bus-off 和恢复次数。

主循环层建议按以下状态处理：

1. CAN1、CAN2 始终保持独立接收，不因一路成为当前有效通道而停止监听另一路。
2. 协议校验通过后才更新“最近有效 CAN 通道”；物理层有帧不等于业务帧有效。
3. 遥测应答默认回到请求帧来源通道。无请求的主动上报走哪一路、双路同时收到相同帧如何去重、切换超时和 bus-off 后的恢复策略，必须由接口协议明确。
4. 一路 bus-off 时只隔离和恢复故障通道，另一通道继续工作；不要无条件同时复位 CAN1、CAN2。
5. 对接收队列设置容量和溢出遥测，避免连续广播占满队列后阻塞遥控或遥测请求。

当前 HALCoGen 配置中 automatic bus-on 已关闭、bus-on timer 为 `0`，错误/status 中断也未启用。正式开发时需要明确采用自动恢复还是应用控制恢复，并对恢复次数和最近错误状态做遥测，不能只清错误标志。

### 6.6 CAN 验证顺序与通过标准

| 阶段 | 方法 | 通过标准 | 不能证明的内容 |
| --- | --- | --- | --- |
| 静态配置 | 检查 `.dil`、`can.c`、VIM 和 message object | 两路参数一致且与接口协议一致；RX/TX box 已生成；中断已启用 | 总线物理收发 |
| 单控制器内部 loopback | 分别测试 CAN1、CAN2 | 各发送至少 1000 帧，数据、ID、DLC 全部一致，零超时 | 收发器、端子和线缆 |
| HDK 同板 J2-J3 | 两路三线直连，正反向发送 | CAN1 -> CAN2、CAN2 -> CAN1 均零错误；断电终端约 `62 ohm` | 外部综电兼容性 |
| 外部 CAN 分析仪 | 分别接 J2/J3 | 位速率、采样点、ID、DLC、周期和字节序与协议一致 | 双路冗余切换 |
| CAN A/B 并行 | 两个独立总线源同时运行 | 两路均可收；有效通道判定、应答路由、重复帧策略符合协议 | 星上 EMC/环境适应性 |
| 故障注入 | 断线、CANH/CANL 反接、无 ACK、bus-off、恢复 | 故障仅影响对应通道；计数/遥测正确；恢复策略可重复 | 辐照、热真空、振动、长期寿命 |

板测记录至少保存：固件版本、HALCoGen 配置、bit rate/sample point、帧 ID 表、接线图、终端阻值、CAN 分析仪日志、错误计数和故障恢复结果。

## 7. 《妙月卫星载荷概要设计方案 - v0.4》硬件支持性评估

### 7.1 总体结论

结论分三级：

- **直接支持**：HDK 板载器件和连接器可以完成地面功能验证。
- **条件支持**：TMS570LS3137 芯片资源足够，但 HDK 必须增加外接接口板、调理板或存储器。
- **不支持完整目标**：HDK 只能做控制器原型，不能单板实现方案要求，也不能作为飞行硬件结论。

当前 HDK **可以支持 TMS570L 主控软件、双经典 CAN、片内 Flash、片内 RAM、板载 SDRAM、部分 ADC 和定时/看门狗功能的实验室开发**。它**不能直接接入 39.6 V 至 48.6 V 一次电源，也不具备板载 RS422/PPS 差分物理层、64 MB 外部 Flash、载荷模拟量调理、独立外部看门狗、主备切换、TK8710 和 8 通道射频硬件**。因此它适合原理样机和软件联调，不等于完整方案硬件可用。

### 7.2 逐项适配矩阵

| v0.4 需求 | HDK/当前工程现状 | 结论 | 需要补充 |
| --- | --- | --- | --- |
| TMS570L，180 MHz | 板载 TMS570LS3137，最高 180 MHz | 直接支持 | 仍需做最坏负载、ISR 延迟和 RAM/栈预算 |
| CAN A/B 两路，同时监听 | J2/J3 各有 SN65HVDA541-Q1 和约 124 ohm 终端；MCU 有 3 个 DCAN | 硬件原型直接支持 | 当前 APP 未初始化 CAN、message box/VIM 未启用；需实现双通道路由和错误恢复 |
| CAN 自定义遥控/广播/遥测 | 仅有经典 CAN 2.0B，最大 1 Mbit/s、单帧最多 8 字节 | 条件支持 | 定稿 bit rate、ID、DLC、分包、超时、CRC、去重和切换规则；若要求 CAN FD 则不支持 |
| 1 路 RS422 双向上注/数传 | HDK 无 RS422 收发器和方案要求的 DB9；LIN 可配置为 SCI/UART，J11 引出 LINRX/LINTX 逻辑信号 | 条件支持 | 外接 3.3 V 逻辑侧全双工 RS422 收发器、端接/保护和 DB9；确认波特率、4 线定义及流控 |
| 1 路 RS422 差分 PPS | HDK 无 RS422 接收器；N2HET 有输入捕获能力并在 J11 引出 | 条件支持 | 外接 RS422 接收与保护，接 N2HET 捕获脚；按方案在下降沿锁存时间戳并验证抖动/丢脉冲 |
| 6 路电压/温度模拟量 | J9 引出多路 12 位 ADC，通道数量足够 | 条件支持 | 每路分压、缓冲、抗混叠、钳位、参考地和标定；39.6 V 至 48.6 V 母线绝不能直连 ADC |
| 256 KB 片内 RAM + 1 MB 片外 RAM | MCU 有 256 KB；HDK 板载 8 MB SDRAM，当前工程已配置 EMIF/SDRAM | 容量支持但器件类型不一致 | 确认最终要求是 SDRAM 还是异步 SRAM，并验证时序、ECC/奇偶校验需求和掉电行为 |
| 3 MB 程序 Flash + 64 KB 数据 Flash | TMS570LS3137 芯片资源匹配 | 芯片直接支持 | v0.4 的 Bootloader/APP A/APP B/默认区仍需按真实 sector 和链接脚本落地 |
| Bootloader + A/B + 默认稳定版本 | 当前 APP 链接区从 `0x00020040` 延伸到 Flash 末端，未按 v0.4 拆为 A/B/默认三个 896 KB 区 | 当前工程不支持该布局 | 重做 bootloader/app 链接脚本、向量表、镜像头、CRC/MIC、跳转和回滚测试 |
| 64 MB 片外 Flash 数传缓存 | HDK 无板载 64 MB SPI NOR；当前代码按 64 MB 建模并有 MibSPI5 Flash 驱动 | 条件支持，当前无实物闭环 | 外接实际 Flash，确认容量/地址模式/擦除粒度/寿命/掉电恢复，读取 JEDEC ID 后做全容量边界测试 |
| 硬件看门狗 | MCU 有内部 DWWD；当前代码还会翻转 GIOA0 作为外部 WDIA，但 HDK 未确认接有对应外部看门狗器件，且现有记录显示 J11/GIOA0 输入路径曾异常 | 条件支持 | 明确内部 DWWD 是否满足要求；若需独立监督器，增加器件并实测 WDIA 波形、停止喂狗和复位结果 |
| 39.6 V 至 48.6 V 一次电源 | HDK P1 仅接受约 5 V 至 12 V | 不可直接支持 | 使用满足功率、浪涌、反接、EMI 和隔离要求的 48 V 前端/DC-DC；严禁母线直接接 P1 |
| 主备控制器/电源/射频切换 | 单块 HDK 是单控制器开发板；v0.4 又规定主备切换由外部设备完成 | 单板不支持 | 至少两套控制器/载荷链路和外部切换控制，验证冷备/热备、上电次序和故障隔离 |
| TK8710 与 8 通道射频、8 路 SMA | HDK 不含 TK8710、射频收发链或 SMA；当前工程已有 MibSPI/TK8710 软件接口 | HDK 单板不支持 | 外接 TK8710/FPGA/射频板，完成电平、复位、中断、时钟、供电和真实空口验证 |
| 星载机械、电磁、热真空、振动、辐照 | HDK 是评估板，接口裸露且 CAN 非隔离 | 不支持飞行件要求 | 设计定制载荷板并完成器件等级、降额、EMC、热、振动、辐照和可靠性验证 |

### 7.3 推荐的地面原型组成

使用当前 HDK 验证 v0.4 时，至少需要以下外接硬件：

1. 两路独立 CAN 分析仪或综电模拟器，分别连接 J2/CAN A 与 J3/CAN B。
2. 一块全双工 RS422 接口板，提供上注 RX 和数传 TX，并带明确的端接、保护和 DB9 定义。
3. 一路独立 RS422 PPS 接收器，输出 3.3 V 逻辑到 N2HET 捕获输入。
4. 一块 6 路模拟量调理板，把温度、电压和电流信号限制到 ADC 允许范围。
5. 一颗真实 64 MB SPI NOR Flash 模块；其 MibSPI 资源不能与 TK8710 或其它外设冲突。
6. 外部看门狗/电源监控器，以及能观测复位原因和 WDIA 波形的示波器或逻辑分析仪。
7. TK8710/FPGA 与 8 通道射频板；若只接数字 loopback，只能算数字链路验证，不能算真实射频通过。
8. 独立实验室电源给 HDK 的 P1 提供 5 V 至 12 V。48 V 母线验证必须先经过专用电源前端，不在 HDK 上直接进行。

### 7.4 建议的分阶段验收

1. **HDK 单板阶段**：LED/SCI、SDRAM、片内 Flash、ADC 基础、内部 DWWD、CAN1/2 internal loopback。
2. **接口板阶段**：J2/J3 双总线、RS422 上注/数传、RS422-PPS 捕获、模拟量调理、外部看门狗、64 MB Flash。
3. **载荷联调阶段**：TK8710 与 8 通道射频、综电 CAN A/B、X 波段 RS422、48 V 电源前端和主备切换。
4. **工程样机阶段**：掉电/上电时序、bus-off、PPS 丢失、Flash 掉电恢复、看门狗复位、升级回滚和长稳测试。
5. **环境与准飞行阶段**：EMC、热真空、振动、辐照、降额和可靠性验证。

只有第 2 阶段完成，才能说“接口功能样机支持 v0.4”；只有定制载荷板完成第 3 至第 5 阶段，才能评估整机或飞行适用性。HDK 上的编译成功、内部 loopback 或数字接口联调均不能替代这些结论。

### 7.5 方案仍需定稿的硬件接口参数

在原理图冻结或 CAN/RS422 固件定版前，至少需要补齐：

- CAN A/B 的 bit rate、sample point 容差、标准/扩展 ID、完整 ID 表、DLC、周期、超时、应用层 CRC、重复帧和通道切换规则。
- CAN 是否要求隔离、共模范围、ESD/浪涌等级、终端位置、线缆阻抗和连接器针脚。
- RS422 上注/数传的波特率、数据位/校验/停止位、全双工四线定义、端接和流控。
- PPS 的差分电气指标、脉宽、下降沿抖动、超时阈值，以及 UTC 数据帧与 PPS 的配对规则。
- 6 路模拟量的源范围、传感器类型、精度、采样率、滤波、开短路诊断和标定方法。
- 片外 1 MB RAM 的器件类型与带宽；64 MB Flash 的具体型号、供电、接口、擦写寿命和掉电策略。
- 外部看门狗的窗口、容差、复位脉宽和上电默认状态。
- 48 V 电源前端的峰值/平均功耗、浪涌、欠压/过压、EMI、隔离与主备切换边界。

## 8. 推荐实践顺序

### 阶段 1：板卡上电和识别

- 接 `P1` 外部电源。
- 接 `J7 Mini-B USB`。
- Windows 设备管理器确认 XDS100v2 和虚拟 COM 口。
- CCS 能连接目标并停在 `main()`。

### 阶段 2：LED / RTI

目标：用 `RTI Compare 0` 每 1 秒产生一次中断，在 `rtiNotification()` 中翻转 NHET1 输出，先验证 D5 / NHET1[0] 闪烁。该步骤基于 HALCoGen 自带示例 `D:\ti\Hercules\HALCoGen\v04.07.01\examples\TMS570LS31x_21x\example_rtiBlinky.c`。

LED 映射先按官方资料确认：

| LED | 信号 | 说明 |
| --- | --- | --- |
| D2 | JTAG TDI | 蓝色 JTAG 指示灯，不是用户 HET LED |
| D3 | NHET1[17] | 用户可编程 LED |
| D4 | NHET1[31] | 用户可编程 LED |
| D5 | NHET1[0] | 本阶段默认翻转目标 |
| D6 | NHET1[25] | 用户可编程 LED |
| D7 | NHET1[18] | 用户可编程 LED |
| D8 | NHET1[29] | 用户可编程 LED |
| LED1 | NHET1[27] | 用户可编程 LED |
| LED2 | NHET1[5] | 用户可编程 LED |

实测备注：如果看到 `D2` 闪烁，通常表示 JTAG TDI 活动或调试链路动作，不能作为 RTI/NHET 程序已成功的验收依据。本阶段验收应看 D5，或者用断点确认 `rtiNotification()` 进入，再测 NHET1[0]。

操作步骤：

1. 确认阶段 1 已完成：
   - HDK 已从 `P1` 外部供电。
   - `J7 Mini-B USB` 已连接 PC。
   - CCS 能连接目标并停在 `main()`。
   - 当前工程是 `TMS570LS3137ZWT` HALCoGen 工程，且能空工程编译通过。
2. 打开 HALCoGen 工程。
3. 进入 `TMS570LSxx / RM4 -> Enable Drivers`：
   - 勾选 `RTI` driver。
   - 勾选 `GIO` driver。
   - 保留系统启动相关默认项。
   - 其它暂时不用的外设先不勾选，避免干扰。
4. 配置 VIM：
   - 进入 `TMS570LSxx / RM4 -> VIM Channel 0-31`。
   - 找到 `VIM Channel 2`。
   - 将 Channel 2 映射到 `RTI Compare 0 interrupt`。
   - 勾选 Enable。
   - 类型选择 `IRQ`，不要选 FIQ。
5. 配置 RTI：
   - 进入 `RTI -> RTI1 Compare`。
   - `Compare 0 Period` 填 `1000.000 ms`。
   - 保持 Compare 0 使用 Counter Block 0。
6. 确认 HET/GIO 输出：
   - 本示例把 `hetPORT1` 作为 GIO 端口使用。
   - 代码会调用 `gioSetDirection(hetPORT1, 0xFFFFFFFF)`，把 NHET1 全部设为输出。
   - 第一版只翻转 bit0，也就是 `NHET1[0]`；用户指南中 D5 对应 NHET1[0]，因此预期 D5 每秒翻转。
7. `File -> Generate Code`。
8. 回到 CCS，刷新工程。
9. 在 `sys_main.c` 的用户代码区加入头文件：

```c
/* USER CODE BEGIN (1) */
#include "rti.h"
#include "het.h"
#include "gio.h"
/* USER CODE END */
```

10. 在 `main()` 的用户代码区加入初始化和主循环：

```c
/* USER CODE BEGIN (3) */
rtiInit();

gioSetDirection(hetPORT1, 0xFFFFFFFFU);

rtiEnableNotification(rtiNOTIFICATION_COMPARE0);

_enable_IRQ();

rtiStartCounter(rtiCOUNTER_BLOCK0);

while (1)
{
}
/* USER CODE END */
```

11. 在 `notification.c` 中找到 HALCoGen 生成的 `rtiNotification(uint32 notification)`，只在函数内部用户代码区加入翻转逻辑，不要在 `sys_main.c` 里再定义一个同名函数：

```c
/* USER CODE BEGIN (9) */
if (notification == rtiNOTIFICATION_COMPARE0)
{
    gioSetPort(hetPORT1, gioGetPort(hetPORT1) ^ 0x00000001U);
}
/* USER CODE END */
```

12. 编译并下载运行。

验收标准：

- CCS 编译无 error。
- 下载后程序持续运行，不进入 abort 或复位。
- 板上 D5 以约 1 秒节拍翻转。
- 如果只看到 D2 闪烁，不算阶段 2 通过；D2 是 JTAG TDI 指示灯，常见于调试器访问目标时闪烁。
- 如果 D5 不明显，先用断点确认 `rtiNotification()` 是否进入，再用示波器或万用表测 NHET1[0] 对应 LED 网络。

常见问题：

| 现象 | 优先检查 |
| --- | --- |
| 编译提示 `rtiNotification` 重定义 | 只保留 `notification.c` 中 HALCoGen 生成的函数，在其用户代码区写逻辑；不要从示例文件完整复制第二个函数 |
| 编译找不到 `rti.h` / `het.h` / `gio.h` | 工程 include path 是否包含 `${PROJECT_ROOT}/include` |
| 不进中断 | VIM Channel 2 是否启用、是否映射 RTI Compare 0、是否选 IRQ、是否调用 `_enable_IRQ()` |
| 只看到 D2 闪烁 | D2 是 JTAG TDI 蓝色指示灯，说明调试链路有活动；继续检查 D5/NHET1[0] |
| LED 不闪但中断进入 | `gioSetPort()` 翻转位是否正确，先只测 `0x00000001U`；再对照 HDK 用户指南和原理图确认 LED 映射 |
| LED 常亮或常灭 | 可能是 LED 有效电平与预期相反，先看是否每秒变化，而不是只看亮灭方向 |

`rtiNotification()` 断点不进入时，按下面顺序排查：

1. 先确认代码确实执行到 RTI 初始化链路：
   - 在 `rtiInit()`、`rtiEnableNotification(rtiNOTIFICATION_COMPARE0)`、`_enable_interrupt_()`、`rtiStartCounter(rtiCOUNTER_BLOCK0)` 分别下断点。
   - 单步确认 4 个函数都执行到，且没有提前停在异常、abort 或 `while(1)` 前。
2. 确认 `rtiStartCounter()` 后 RTI 计数器在跑：
   - 在 CCS Registers 视图打开 RTI 寄存器，观察 `rtiREG1->CNT[0].FRCx` 是否持续增加。
   - 如果 `FRCx` 不变，优先检查 `rtiStartCounter(rtiCOUNTER_BLOCK0)` 是否执行、RTI clock 是否被 HALCoGen 使能、工程是否重新 Generate Code。
3. 确认 Compare0 事件是否产生：
   - 运行一段时间后暂停，查看 `rtiREG1->INTFLAG` 的 bit0 是否置位。
   - 如果 bit0 会置位但 `rtiNotification()` 不进，说明 RTI 比较事件有了，但中断没有通过 VIM/CPU IRQ。
   - 如果 bit0 一直不置位，检查 `RTI -> RTI1 Compare` 中 `Compare 0 Period`、Compare 0 source、Counter Block 0 配置。
4. 确认 RTI Compare0 中断使能：
   - 执行 `rtiEnableNotification(rtiNOTIFICATION_COMPARE0)` 后，查看 `rtiREG1->SETINTENA` 或 RTI interrupt enable 状态中 Compare0 是否打开。
   - 注意 `rtiEnableNotification()` 内部会先清 `INTFLAG`，再写 `SETINTENA`。
5. 确认 VIM 通道：
   - HALCoGen 中 `VIM Channel 0-31` 必须启用 Channel 2。
   - Channel 2 必须是 `RTI Compare 0`。
   - Channel 2 必须映射为 `IRQ`。
   - 修改 VIM 后必须重新 `Generate Code`，并刷新/重编译 CCS 工程。
6. 确认 CPU 全局 IRQ：
   - `_enable_IRQ()` 必须执行。
   - 如果一直单步调试，尝试正常 `Resume` 运行，不要一直停在断点附近。
   - 如需确认，可在 `_enable_IRQ()` 后查看 CPSR 的 I bit 是否被清除。
7. 确认断点位置：
   - 如果断点下在 `sys_main.c` 中自己复制的 `rtiNotification()`，但工程实际链接的是 `notification.c` 中的弱定义/生成函数，断点可能下错位置。
   - 推荐只保留 `notification.c` 中 HALCoGen 生成的 `rtiNotification()`，并把断点下在该函数第一行。
8. 快速隔离法：
   - 先不依赖中断，在 `while(1)` 中临时加入软件延时并翻转 `hetPORT1` bit0，确认 LED/GIO 输出链路可用。
   - 如果轮询翻转有效而 RTI 中断无效，问题集中在 RTI/VIM/IRQ；如果轮询也无效，问题集中在 HET/GIO/LED 映射或 PinMux。

临时轮询翻转代码如下，只用于隔离问题，验证后再恢复 RTI 中断版本：

```c
gioSetDirection(hetPORT1, 0xFFFFFFFFU);

while (1)
{
    volatile uint32 delay;

    gioSetPort(hetPORT1, gioGetPort(hetPORT1) ^ 0x00000001U);

    for (delay = 0U; delay < 5000000U; delay++)
    {
    }
}
```

可选扩展：

- 将翻转掩码从 `0x00000001U` 改为 `0x00020001U`，同时翻转 `NHET1[0]` 和 `NHET1[17]`，用于观察 D5 与 D3。具体 LED 位号仍以 `SPNU509C` 和 `tms570ls3_hdk_schematics_reve.pdf` 为准。
- 将 `Compare 0 Period` 改为 `500.000 ms` 或 `200.000 ms`，确认闪烁频率随 RTI 配置变化。
- 阶段 2 完成后，保留该工程作为 `tms570ls3137_hdk_rti_led_YYYYMMDD` 基线工程，不要直接在同一工程上做 SPI 复杂实验。

### 阶段 3：SCI 串口

- 使用 J7 的 XDS100v2 第二通道虚拟 COM 口。
- 先按 `example_sci_uart_9600.c` 验证发送字符串。
- 当前工程 `D:\ti_workspace\tms570ls3137_halcogen_base1` 已加入阶段 3 最小发送程序：
  - 保留阶段 2 的 RTI/D5 闪灯，用来确认程序仍在运行。
  - J7 虚拟 COM 对应板卡设计文件中的 `USB_LINTX` / `USB_LINRX`，实际走 `LIN_TX` / `LIN_RX`，因此代码使用 `scilinREG`，按 SCI/UART 兼容模式初始化。
  - 串口参数：`9600 baud`、`8 data bits`、`No parity`、`2 stop bits`。
  - 预期串口输出：

```text
TMS570LS3137 HDK stage 3 SCI UART start
stage 3: SCI UART 9600 8N2 OK
stage 3: SCI UART 9600 8N2 OK
...
```

如果串口终端没有输出，优先检查：

1. Windows 设备管理器中是否出现 XDS100v2 的虚拟 COM 口。
2. 串口终端是否打开的是 J7 对应的第二通道 COM 口，而不是其他 USB 转串口。
3. 串口参数是否为 `9600 8N2`。
4. D5 是否仍在闪烁；如果 D5 不闪，先回到阶段 2 排查下载/运行/RTI。
5. 若 D5 闪但串口无输出，再复核原理图中的 `USB_LINTX` / `USB_LINRX` 到 `LIN_TX` / `LIN_RX` 路径，以及 HALCoGen 中 SCILIN/LIN 引脚复用状态。

### 阶段 4：MibSPI1

目标：在当前工程 `D:\ti_workspace\tms570ls3137_halcogen_base1` 中，把 `J9/MibSPI1` 做成 8710 默认 SPI 通道。

本阶段固定配置：

| 项目 | 配置 |
| --- | --- |
| SPI 模块 | `MibSPI1`，对应 HDK `J9` |
| 工作模式 | Master |
| SPI Mode | 按 8710 标准 SPI `CPOL=1`、`CPHA=1` 配置；注意 TMS570 MibSPI 的 phase 位与常见 CPHA 命名映射不同，当前代码 `SPIFMT0.PHASE=0` |
| 字长 | 8-bit |
| 位序 | MSB first |
| 速率 | 16 MHz。当前 `VCLK1=80 MHz`，`PRESCALE=4`，所以 `80 MHz / (4 + 1) = 16 MHz` |
| 片选 | `MibSPI1CS[0]`，低有效，默认接 `J9` |
| CS 控制策略 | 手动控制 CS0。长 SPI 事务内部按 MibSPI RAM 能力分块，当前每块最多 127 byte，但整个 8710 命令期间保持 CS 低电平 |
| PAD_IRQ | 当前改用 `J11 pin 21 / GIOA3`，上升沿中断；`J11 pin 20 / GIOA0` 实测 3.3 V 输入无 DIN 变化，暂不作为默认 IRQ |
| 可选复位 | `J11 pin 22 / GIOA2`，当前默认配置为输出高电平，作为可选硬件复位控制脚 |

默认接线定义：

| 8710 信号 | HDK 信号 | 说明 |
| --- | --- | --- |
| `SCLK` | `J9 MibSPI1CLK` | SPI 时钟空闲高；逻辑分析仪按 8710 配置 `CPOL=1`、`CPHA=1` 解码 |
| `MOSI` | `J9 MibSPI1SIMO` | TMS570 输出到 8710 |
| `MISO` | `J9 MibSPI1SOMI` | 8710 输出到 TMS570 |
| `CS_N` | `J9 MibSPI1CS[0]` | 本工程手动拉低/释放 |
| `PAD_IRQ` | `J11 pin 21 / GIOA3` | 上升沿触发，进入 `gioNotification()` 后计数；这是当前实测可读的默认接线 |
| `RESET_N` 可选 | `J11 pin 22 / GIOA2` | 当前先保持高电平 |
| `GND` | 任意 HDK GND | 8710 外设必须和 HDK 共地 |

注意：`GIOA0` 是 MCU 3.3 V 逻辑输入，不是 12 V 容忍输入。8710 或外部设备如果输出 12 V IRQ，必须先经过分压、电平转换、光耦或比较器整形到 3.3 V，再接入 `J11 pin 20 / GIOA0`。不要将 12 V 直接接到 GIOA0。

板级路径补充：按 `SPNR034` 设计文件网表，`J11-20` 在 `GIOA0` 网，MCU `U1-A5` 在 `m0034` 网，中间通过 `U44/SN74CBTLV3257RGYR` 总线开关/复用器连接。`SN74CBTLV3257` 是 4 路 2:1 FET bus switch，`OE` 禁用或 `S` 选择错误时，J11 上有 3.3 V 也不一定能被 MCU `GIOA0` 读到。当前实测结果也是 `J11-20/GIOA0` 输入 3.3 V 时 `A0=0`，而 `J11-21/GIOA3` 输入 3.3 V 时 `A3` 可随电平变化，因此阶段 4 默认将 `PAD_IRQ` 改接 `J11-21/GIOA3`。

注意：上表只定义本阶段默认走线，实际焊接或杜邦线接线前仍要按 `SPNU509C_TMS570LS31x_HDK_User_Guide.pdf`、`SPNR034` 原理图和 8710 模块资料复核 J9/J11 的物理针脚、电平和方向。

当前工程已完成的代码点：

1. `source/sys_main.c`
   - `stage4Mibspi1Init()` 调用 `mibspiInit()` 后覆盖 MibSPI1 的运行配置为 16 MHz、8710 标准 `CPOL=1/CPHA=1`、8-bit word、MSB first。代码中 `STAGE4_MIBSPI1_FMT0_VALUE` 使用 `clock polarity bit=1`、`clock phase bit=0`，这是 TMS570 MibSPI 对标准 CPHA=1 的映射。
   - `MibSPI1CS[0]` 被切到 MibSPI1 端口的 GIO 模式，使用 `PC5` 拉低、`PC4` 释放，不再依赖 MibSPI 硬件自动片选。
   - `stage4Mibspi1Transfer8710Command()` 是后续 8710 命令收发入口。它会在命令开始前拉低 CS0，命令结束后释放 CS0，中间分块传输时不释放 CS。
   - 当前实现已改成 MibSPI RAM ping-pong：Bank A 使用 `RAM[0..63] / TG0`，Bank B 使用 `RAM[64..127] / TG7`。下一块启动后再回填刚空出来的 bank，用于压缩普通软件分块造成的块间空隙。
   - 每个 TX RAM entry 的内部 chip-select 字段使用 `CS_0`；同一 bank 内，除最后一个 entry 外均设置 `CSHOLD=1`。外部 CS0 仍由 GIO 手动拉低/释放。
   - `stage4Mibspi1Transfer512Test()` 每秒左右发送一次 512 byte 测试帧，用于在示波器或逻辑分析仪上确认 `CS/CLK/SIMO` 波形、分块传输和长事务 CS 保持。
   - 512 byte 测试帧数据模式：前 256 byte 为 `00 01 02 ... FE FF`，后 256 byte 为 `FF FE FD ... 01 00`。
   - 当前 ping-pong 分块方式为 `64 byte * 8 bank`。整个 512 byte 测试帧期间 CS0 应保持低电平，中间 bank 切换间隙不应释放 CS。
2. `source/notification.c`
   - `gioNotification(gioPORTA, 3)` 中递增 `g_stage4PadIrqCount`。
   - 调试 8710 的 `PAD_IRQ` 时，可在 `gioNotification()` 内下断点，或 Watch `g_stage4PadIrqCount` 是否随上升沿增加。
3. GIO/VIM 处理
   - 当前 HALCoGen 生成配置里 VIM channel 9 没有启用，运行时代码用 `vimChannelMap(9, 9, stage4GioHighLevelInterrupt)` 和 `vimEnableInterrupt(9, SYS_IRQ)` 补齐。
   - `GIOA3` 配成 single edge + rising edge，并走 high level GIO interrupt handler。

下载后串口期望输出：

```text
TMS570LS3137 HDK stage 4 MibSPI1 start
MibSPI1 master CPOL1 CPHA1 8-bit MSB first 16MHz, manual CS0 low active
stage 4: 512-byte SPI test sent on J9
stage 4: 512-byte SPI test sent on J9
...
```

示波器/逻辑分析仪验收步骤：

1. 先不接 8710，只接探头到 `J9 MibSPI1CS[0]`、`J9 MibSPI1CLK`、`J9 MibSPI1SIMO` 和 GND。
2. 下载并运行当前工程。
3. 确认串口出现阶段 4 输出，D5 仍按 RTI 节拍闪烁。
4. 观察 CS0 是否周期性低脉冲，CLK 是否只在 CS0 低电平期间输出。
5. 测量 CLK 频率应约为 16 MHz。
6. 确认 SIMO 输出 512 byte 测试帧：`00 01 02 ... FE FF FF FE FD ... 01 00`，MSB first。
7. 重点观察 CS0：512 byte 期间应一直为低，只允许在整帧结束后释放；分块间可能有很短空隙，但 CS 不应跳高。
8. 接入 8710 后，再观察 `PAD_IRQ -> GIOA3` 上升沿是否让 `g_stage4PadIrqCount` 增加。

`PAD_IRQ/GIOA3` 中断排查：

1. 先不要接 12 V；用 0/3.3 V 方波或手动跳线测试 `GIOA3/J11-21`。
2. 当前固件每轮会打印 `GIOA DIN`、`A[7:0]`、`A0/A1/A2/A3`、`GIO FLG`、`ENASET` 和 `g_stage4PadIrqCount`。
3. 如果 `J11 pin 21` 输入 3.3 V 时 `A3` 仍为 0，先用万用表或示波器直接量 `J11-21` 对 HDK GND 是否真到 3.3 V，再看 `A[7:0]` 是否有相邻 bit 变化，用来判断是否接反 J11 行列或针脚方向。
4. 当前 `J11-20/GIOA0` 已记录为异常路径：3.3 V 输入时 `A0` 无变化，暂不建议继续用作 8710 `PAD_IRQ`，除非后续确认 `U44/SN74CBTLV3257` 复用状态或硬件损伤情况。
5. 如果 `A3` 能变为 1，但 `FLG/count` 不增加，再查 GIO 上升沿配置、VIM channel 9 和 `stage4GioHighLevelInterrupt()`。
6. 如果 `FLG` 置位但 `count` 不增加，说明 GIO 模块已捕获边沿但 VIM/ISR 路径未进；在 `stage4GioHighLevelInterrupt()` 下断点继续查。

波形排查记录：

- 如果放大后发现每 8-bit byte 之间都有明显 CLK 空窗，优先检查 TX RAM entry 的 `CSHOLD` 和内部 chip-select 字段。只用 GIO 手动 CS0，但 TX RAM control 写 `CS_NONE` 时，MibSPI 内部可能仍按每个 word 结束一次事务处理。
- 当前修正方式：同一 bank 内前 63 个 entry 设置 `CSHOLD=1`，最后一个 entry 设置 `CSHOLD=0`；TX RAM control 的 CS 字段使用 `CS_0`，物理 CS0 管脚仍保持 GIO 模式手动控制。
- 如果 8-bit 模式下修正 `CSHOLD/CS_0` 后仍然每 byte 有空窗，说明这是 MibSPI 硬件/时序装载的 word-to-word gap，不是片选配置问题。
- 16-bit packed word 曾用于验证 word-to-word gap：`00 01` 打包为 `0x0001` 时，逻辑分析仪仍可按 8-bit 解码。验证完成后已切回 8-bit word，最终应按 8710 配置 `CPOL=1`、`CPHA=1` 验证 `00 01 02 03...` 数据顺序。
- 曾出现 `CPHA=0` 解码正常、`CPHA=1` 解码异常，是因为当时 TMS570 `SPIFMT0.PHASE` 位设置为 1；该位和标准 CPHA 命名不是同向映射。现已改为 `SPIFMT0.PHASE=0`，用于匹配 8710 的标准 CPHA=1。
- 如果 8710 要求任意 byte 之间都没有可见 CLK 空窗，软件 ping-pong 不能满足，应评估 DMA 或其他 SPI 控制方式。
- Bank A/B 拼接处的空窗来自 CPU 轮询完成标志后再启动下一组传输，和 byte 内部空窗不是同一个问题；软件 ping-pong 只能压缩，不能保证零间隔。

后续接真实 8710 协议时，只替换探测事务的数据内容，不要直接改回 HALCoGen 的 `mibspiSetData()` + `mibspiTransfer()` 单次调用模式；否则长命令跨块时 CS 保持低电平这个约束容易被破坏。

### 阶段 5：板载资源扩展

- CAN：J2/J3。
- Ethernet：J1，注意 S2 DIP 的 Ethernet 配置。
- SD 卡：J15，重点查 SPI2 连接。
- EMIF/SDRAM：J13 和 EMIF 配置。

## 9. 后续资料归档建议

建议目录改成：

```text
TMS570资料/
  TMS570LS3137_HDK开发资料整理.md
  01_TI官方文档/
    SPNU509C_HDK_User_Guide.pdf
    SPNU499C_TRM.pdf
    SPNS162C_TMS570LS3137_Data_Manual.pdf
    SPNZ195_Errata_RevC.pdf
    SPNZ222_Errata_RevD.pdf
  02_TMS570LS31HDK设计文件/
    SPNR034_HDK_Design_Files/
  03_CCS12_HALCoGen环境/
    install_notes/
    screenshots/
  04_HALCoGen示例/
    rti_blinky/
    sci_uart/
    mibspi1_loopback/
    mibspi_dma/
  05_SPI_MibSPI实验/
    wiring/
    logic_analyzer/
    test_records/
```

命名建议：

- 工程目录名包含板卡和外设，例如 `tms570ls3137_hdk_mibspi1_polling_20260601`。
- 每次实验记录：供电、USB/JTAG、HALCoGen 设备名、CCS 版本、编译器版本、接线、波形、结论。
- SPI 相关记录必须写清楚是 `J9/MibSPI1`、`J10/SPI2` 还是 `J11/MibSPI3`。

## 10. 本文限制

- 本文依据用户照片和 TI 官方 `TMDS570LS31HDK` 资料修订，尚未对实物板进行上电、JTAG 连接或波形实测。
- 照片只能确认板卡大类和芯片丝印；精确硬件修订差异仍以 `SPNR034.ZIP` 设计文件和板上丝印为准。
- SPI/MibSPI 引脚表来自 `SPNU509C` 用户指南；实际接线前必须再用原理图确认连接器方向、共线器件和电平。
- CAN 章节依据 `SPNU509C`、Rev-E 原理图、TMS570LS3137 数据手册和当前 HALCoGen/源码静态检查；尚未在本次更新中进行 J2/J3 实物收发、CAN 分析仪、bus-off 或故障注入测试。
- v0.4 硬件适配结论仅用于开发板原型选型，不是载荷板原理图评审、接口控制文件符合性或飞行鉴定结论。
