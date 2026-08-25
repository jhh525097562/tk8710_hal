# TMS570 Bootloader 编译与烧录说明

> 更新日期：2026-08-13  
> 工程：`tms570ls3137_halcogen_base_570RAM_add`

## 1. 当前结论

当前 Bootloader 和应用已经能够分别编译，并能生成无地址重叠的 JTAG 合并烧录镜像。

| 区域 | 地址/实际范围 | 说明 |
|---|---:|---|
| Bootloader 向量 | `0x00000000..0x0000001F` | 上电复位向量 |
| Bootloader 程序 | 实际到 `0x000066C7` | 链接器最多允许到 `0x0000FFFF` |
| 保留区 | `0x00010000..0x0001FFFF` | 当前未使用 |
| 应用状态区 | `0x00020000..0x0002001F` | magic、入口、长度、版本等 32 字节 |
| 应用向量 | `0x00020020..0x0002003F` | `resetEntry=0x00020020` |
| 应用程序 | 从 `0x00020040` 开始，当前镜像最高到约 `0x00052277` | 与 Bootloader 无重叠 |
| Bank7 Sector1 | `0xF0204000..0xF0207FFF` | 升级/回滚标志、复位次数 |
| Bank7 Sector2 | `0xF0208000..0xF020BFFF` | FPGA 参数持久化 |

应用状态区的入口必须指向应用复位向量 `resetEntry`，不能直接调用 `main()`。否则会绕过应用的 `_c_int00`、栈初始化、`.bss` 清零、`.data` 拷贝和 HALCoGen 启动流程。

## 2. 工具要求

- TI ARM CGT：`ti-cgt-arm_20.2.7.LTS`
- Hercules F021 Flash API：`02.01.01`
- CCS/DSLite：当前脚本可自动查找 CCS 12.8.1 中的 `DSLite.exe`
- 仿真器配置：`targetConfigs/TMS570LS3137.ccxml`

工具不在默认路径时，使用脚本参数显式指定 `-CgtRoot`、`-F021Root` 或 `-UniFlashRoot`。

## 3. 编译

在工程根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-bootloader.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\build-tms570.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\scripts\build-combined-image.ps1
```

输出文件：

| 文件 | 用途 |
|---|---|
| `bootloader/Debug/tms570ls3137_bootloader.out` | 仅烧 Bootloader |
| `Debug/tms570ls3137_halcogen_base_570RAM.out` | 仅烧应用 |
| `combined/tms570_boot_app_flash.hex` | 首次生产/调试推荐，Bootloader 与应用一起烧录 |
| `combined/boot.hex`、`combined/app.hex` | 地址检查和问题定位 |

`build-combined-image.ps1` 会解析 Intel HEX 地址，若 Bootloader 和应用存在重叠会直接失败。`*_flash.hex` 会去除链接器生成的 ECC 地址记录，由 DSLite 在程序装载时按目标配置处理 ECC。

## 4. JTAG 烧录

### 4.1 检查连接，不写 Flash

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -ConnectionCheck
```

### 4.2 首次烧录 Bootloader 与应用

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -Image Combined -DataTransferSpi2SlaveTest
```

该命令默认依次执行：Bootloader 构建、保留 TK8710 功能且启用 SPI2 从机数传测试的 App 构建、合并镜像构建、烧录/校验/运行。脚本强制使用 `FlashEraseSelection=1`，即只擦除所选镜像需要的程序 Flash sector，避免默认的 `Entire Flash` 擦除 Bank7 参数区。

脚本会自动兼容两种工具入口：Standalone UniFlash 的 `dslite.bat --config ... --flash image`，以及 CCS DebugServer 的 `DSLite.exe flash --config ... --flash image`。当前机器未发现 `F:\ti\uniflash_9.6.0\dslite.bat`，实际验证使用的是 `D:\ti\ccs1281\ccs\ccs_base\DebugServer\bin\DSLite.exe`。

若只想烧录已经生成的镜像，增加 `-SkipBuild`；若烧录后需要保持调试停止状态，增加 `-NoRun`。

`-FpgaSelfTest` 只用于脱离真实 TK8710 的 FPGA 遥控遥测协议自测。该宏会跳过 TK8710 SPI reset/version read，并禁止主循环中的 `TK8710Tms570PollIrq()`、运行看门狗和载荷业务处理；验证真实 TK8710 时不能使用。

不带任何测试开关时会按 CCS Debug 配置构建。当前 Debug 配置本身包含 `DATA_TRANSFER_SPI2_SLAVE_TEST`；明确传入该开关则强制完整直编译，并保证这个宏生效。

### 4.3 只更新应用

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -Image App
```

只更新 Bootloader：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -Image Boot
```

只校验、不写入：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\flash-tms570.ps1 -Image Combined -VerifyOnly
```

## 5. CAN Bootloader 当前参数

- 传输接口：DCAN1
- 波特率：500 kbit/s
- 应用下载起始地址：`0x00020020`
- 状态区地址：`0x00020000`
- Bootloader 调试串口：SCI/LIN，115200 bit/s

Bootloader 必须直接初始化 `scilinREG`。HALCoGen 的 `sciInit()` 初始化的是另一个 `sciREG` 外设，不能用于当前载荷板的 SCI/LIN 控制台。串口发送增加了超时保护，即使控制台硬件异常也不能阻塞 Bootloader 跳转应用。

CAN 下载数据必须从 `0x00020020` 开始，不能把 `combined/app.hex` 中 `0x00020000` 的 `.app_status` 当作普通应用数据直接发送。Bootloader 擦除 `0x20000` 所在 sector、从 `0x20020` 顺序写应用，全部成功后再单独写 `0x20000` 的 32 字节有效状态。

## 6. 仍未闭环的在线升级项

当前代码可以完成构建和 JTAG 合并烧录，但不能据此判定 CAN 在线升级已经可用：

1. Bootloader 已读取 Bank7 Sector1 的 `1`（升级）和 `2`（回滚）标志，并在复位后停留在 CAN 更新/恢复模式。成功下载后上位机应发送 RUN 跳入新应用，由新应用清除标志；若只发送 RESET，标志仍存在，Bootloader 会继续等待 CAN。
2. 当前没有 A/B 应用分区或黄金镜像，`rollback=2` 目前仅进入 CAN 恢复模式，不能自动切换到旧镜像。
3. 缺少与当前 CAN 包协议匹配的上位机下载工具及掉电、丢包、错误地址、错误长度测试。
4. 普通 IRQ/FIQ 可继续使用低地址 Bootloader 向量中的 VIM 跳转指令；若要求应用拥有独立的 abort/SVC/undef 异常处理，还需设计 POM 向量重映射或 Bootloader 异常转发。

因此当前推荐验证顺序是：先完成 JTAG 合并镜像上板启动，再补 boot flag 判定与 CAN 上位机，最后做在线升级和掉电恢复测试。

## 7. 上板验收

1. 擦写前先备份 Bank7 参数；执行 Combined 烧录后确认参数未被整片擦除。
2. 上电应先看到 Bootloader 的 SCI 启动输出，随后进入应用并出现应用启动日志。
3. 读取 `0x00020000`：首字为 `0x5A5A5A5A`，第二字为 `0x00020020`。
4. 正常复位 20 次，确认每次都能进入应用且无 ESM/abort/reset loop。
5. 分别验证 TK8710 IRQ、RTI、SPI 和 FPGA 遥控，确认 Bootloader 低地址向量未影响应用中断。
6. 下发升级/回滚命令后，确认复位进入 CAN Bootloader；完成下载后发送 RUN，确认新应用启动并清除 boot flag。
