# RK3506 一键制作包含最新版 App 的升级包

## 1. 用途

本流程把 Windows 中已经交叉编译完成的三个 ARM 程序复制到 RK3506 SDK 的 `userdata` 预置目录，更新 `tdaemon` 启动参数，重新生成 `rootfs.img`、`userdata.img` 和 `update.img`，最后将升级包复制到：

```text
D:\RK3506_Flash\rk3506b_iot_emmc_update.img
```

一键脚本为同目录下的 `制作最新版App升级包.ps1`。

## 2. 固定的源文件

| 程序 | Windows 源文件 | 固件内路径 |
|---|---|---|
| TK8710 网关 | `D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\tk8710app\tk8710_gw` | `/userdata/tk8710_gw` |
| Web 服务 | `D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\webserver\build_arm\web_server` | `/userdata/web_server` |
| MQTT 桥接 | `D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\mqtt\build_arm\mqtt_bridge` | `/userdata/mqtt_bridge` |

脚本不会重新编译这三个 App，只复制上述已经生成的 ARM 可执行文件。运行脚本前，应先分别完成 App 的交叉编译。

## 3. TK8710 的守护启动命令

固件中的 `/etc/tdaemon.conf` 使用以下命令：

```ini
proc=/userdata/tk8710_gw -w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level warn
```

这里必须使用 `/userdata/tk8710_gw`。虽然参数示例中也可能写成 `/tk8710_gw`，但本升级包将程序放在 `/userdata`，根目录下并不存在 `/tk8710_gw`。

参数含义：

- `-w /userdata/`：工作文件保存目录为 `/userdata/`。
- `-g 0x2e`：使用网关地址/标识 `0x2e`。
- `--driver-log-level warn`：驱动日志只输出 WARN 及以上级别。
- `--trm-log-level warn`：TRM 日志只输出 WARN 及以上级别。

## 4. 一键执行

打开 PowerShell，执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\auto_deployment\制作最新版App升级包.ps1"
```

默认使用：

- WSL 发行版：`Ubuntu-22.04`
- SDK：`/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0`
- 最终升级包：`D:\RK3506_Flash\rk3506b_iot_emmc_update.img`

如需指定其他 WSL 或 SDK：

```powershell
.\制作最新版App升级包.ps1 -WslDistro Ubuntu-22.04 -SdkRoot /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
```

仅更新预置文件和配置、不执行耗时构建：

```powershell
.\制作最新版App升级包.ps1 -SkipBuild
```

## 5. 脚本执行内容

1. 检查 WSL、SDK、三个 App 源文件和 SDK 目标目录。
2. 调用 Linux `file` 检查三个程序是否为 ARM ELF，避免把 Windows/x86 程序装入固件。
3. 备份当前 SDK 中的三个旧程序和 `tdaemon.conf`。
4. 将最新版 App 复制到 `device/rockchip/common/extra-parts/userdata/normal/`。
5. 将 `tdaemon.conf` 中的 TK8710 守护命令更新为上述完整命令。
6. 依次执行 `./build.sh buildroot`、`./build.sh extra-parts`、`./build.sh updateimg`。
7. 检查真正的完整镜像 `output/update/Image/update.img`，备份 D 盘旧升级包，再复制新升级包并输出 SHA-256。

> SDK 还会生成 `output/update/update.img` 和 `output/firmware/update.img` 等 Linux 符号链接。它们经由 Windows 的 `\\wsl.localhost` 访问时可能显示为十几字节的小文件，不能当作升级镜像复制。脚本要求候选文件大于 1 MiB，避免误用符号链接或 `update.raw.img`。

备份目录位于：

```text
D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\auto_deployment\backup\yyyyMMdd_HHmmss
```

## 6. 成功判据

脚本最后必须同时显示：

- 三个 App 均为 ARM ELF；
- 三个 SDK 预置文件的 SHA-256；
- `output/update/Image/update.img` 已生成且大小大于 1 MiB；
- `D:\RK3506_Flash\rk3506b_iot_emmc_update.img` 已更新；
- 源升级包与目标升级包 SHA-256 相同。

烧录并启动后，在板端检查：

```sh
grep '^proc=' /etc/tdaemon.conf
ps | grep -E '[t]k8710_gw|[w]eb_server|[m]qtt_bridge'
ls -l /userdata/tk8710_gw /userdata/web_server /userdata/mqtt_bridge
```

TK8710 进程应能看到完整参数。还可执行：

```sh
tr '\0' ' ' </proc/$(pidof tk8710_gw)/cmdline; echo
```

## 7. 注意事项

- `userdata.img` 被包含在完整 `update.img` 中。整包升级可能覆盖板端 `/userdata` 的配置、日志及校准数据，升级前必须备份重要文件。
- 脚本会备份 PC 上旧的目标升级包，但不能替代板端数据备份。
- App 源码修改后，要先单独交叉编译，再运行本脚本；仅运行 SDK 构建不会自动重新编译 Windows 源码目录里的 App。
- 若只想升级一个程序且不希望覆盖 `/userdata`，可通过 SSH/SCP 单独替换，不能直接使用本完整升级包流程。

