# 三应用 SWU 升级使用说明

## 1. 适用范围

本目录实现 RK3506 网关的 MQTT、Web、TK8710 应用升级。制作端运行于 Ubuntu WSL，安装端使用板上 SWUpdate 2023.12。

| 包内文件 | 安装用途 |
|---|---|
| sw-description | 指定 stable/copy1 集合、文件和安装脚本 |
| install.sh | 由 app_upgrade.sh 加版本、SHA256清单及空间要求生成 |
| mqtt_bridge | 替换 /userdata/mqtt_bridge |
| web_server | 替换 /userdata/web_server |
| tk8710_gw | 替换 /userdata/tk8710_gw |
| metadata | ARM JSON-C辅助程序，保留原版本JSON键值，新增/更新 app_bundle_version |

包不会覆盖整个 userdata，不更新内核、Bootloader、rootfs或分区表。FRP、网络、/etc/config.json、GPS、RF校准和日志文件不在覆盖清单中。依赖库不随本包替换，制作时检查SDK target库依赖，安装前还通过板上的动态加载器检查。

`app_bundle_version` 是这次三个应用的发布组合版本；不会伪造 `comm_version`、`app_version` 或 TK8710 自报版本。这些组件版本仍由各自已有机制维护。

## 2. 首次启用所需基础固件

旧板仅有 swupdate 和旧 Web 不够。基础固件还需提供：

- `/usr/bin/app_upgrade.sh`：失败/开机恢复工具。
- `/etc/init.d/S98app_upgrade_recover`：在 S99 守护启动前恢复未完成事务。
- `/usr/bin/tdaemon.sh`、`tdaemon_proc.sh`：识别 `/var/run/app-upgrade-maintenance`，暂停网络/进程检查及拉起。
- 新版 `/userdata/web_server`：私有临时文件目录、校验后安装、上传冻结、失败恢复及成功重启。
- `/etc/init.d/S80swupdate`：不再启动厂商8080升级服务和`swupdate-progress -r`，SWUpdate由8090的Web按需调用。该板二进制的socket路径是编译时固定的，TMPDIR不能隔离socket。

这些文件已经接入 SDK 的 `external/rk3506app/build_apps.sh --stage`，分别写入 Buildroot overlay 和 userdata 预置目录。以后重新制作基础固件时先运行 --stage，再编译 rootfs/制作升级镜像。只替换 userdata.img 不会更新 rootfs 内的恢复脚本。

本次测试板已通过SSH安装上述基础能力并保留备份；日常制作的三App SWU不再包含/覆盖这些基础脚本。基础升级协议若再次变化，先更新基础固件再分发对应App包。

## 3. 一键制作

在 WSL 中运行 SDK 内的脚本：

```sh
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
bash external/rk3506app/swupdattool/gateway/make_app_swu.sh 20260915.7
```

此命令重新编译并预置 SDK 内 MQTT/Web 等App，然后生成三App包。`build_apps.sh --stage`会先从`D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\tk8710app`自动预置完整TK8710应用目录；SWU仍只选择其中的`tk8710_gw`，不会包含测试程序和class3数据。完整目录需要通过userdata镜像/完整update.img部署。

若已单独交叉编译出最新 TK8710，可以显式提供它：

```sh
bash external/rk3506app/swupdattool/gateway/make_app_swu.sh 20260915.7 \
  /mnt/d/other-build/tk8710_gw
```

两个例子是二选一；同名发布包存在时会拒绝覆盖，选择新的发布版本即可。

输出：

```text
output/app-updates/gateway-apps-20260915.5.swu
output/app-updates/gateway-apps-20260915.5.manifest.json
```

manifest记录每个ARM文件和整包SHA256、依赖、大小与版本。若只想对已预置程序打包，不重新编译：

```sh
python3 external/rk3506app/swupdattool/gateway/build_swu.py \
  --sdk "$PWD" --version 20260915.7 \
  --output output/app-updates/gateway-apps-20260915.7.swu
```

复制到Windows后在浏览器中选择：

```sh
mkdir -p /mnt/d/RK3506_Flash
cp output/app-updates/gateway-apps-20260915.7.swu /mnt/d/RK3506_Flash/
sha256sum output/app-updates/gateway-apps-20260915.7.swu \
  /mnt/d/RK3506_Flash/gateway-apps-20260915.7.swu
```

两行SHA256须一致。这是 `.swu`，不是RKDevTool使用的 `update.img`。

## 4. 网页升级步骤

1. 打开 `http://<当前网关IP>:8090`，使用原Web账号登录。
2. 进入“系统”，在固件升级区选择新制作的 `.swu`。
3. 点击升级，等待上传和安装完成。前端按512KiB分块；安装期间再次上传返回409，避免覆盖安装文件。
4. 后端先调用目标 swupdate `-c` 校验，再正式安装。任何校验失败显示error，不安装、不重启。
5. 安装成功显示done，约3秒后请求整机重启。启动耗时另计，不是3秒后保证可访问。
6. DHCP场景可能更换IP。用网关发现工具按GatewayId找回设备；本次 `.62` 重启后变为 `.227`，不是配置被覆盖。
7. 重新登录，检查Web、MQTT、TK8710及原业务配置。SSH进一步核对下面的证据。

```sh
cat /etc/device_version.json
cat /userdata/.app-upgrade/transaction/phase
sha256sum /userdata/mqtt_bridge /userdata/web_server /userdata/tk8710_gw
pidof mqtt_bridge web_server tk8710_gw
tail -80 /userdata/.app-upgrade/last-swupdate.log
```

版本应匹配包的manifest，phase应为`committed`，三个程序哈希应逐一匹配。进程存在还不等于NS业务验收通过，需另看连接/配置/无线收发结果。

Web API沿用原入口：POST `/upload?file=image.swu&offset=N`、POST `/upload/start`、GET `/api/upgrade_status`。无需NS下发接口参与。安装进度仍为状态提示，并非真实写入百分比。

## 5. 内部时序与回滚

```text
上传到 /tmp/image.swu
  → 冻结为 /tmp/image.installing.swu
  → 检查没有其他swupdate/进度监听器 → 创建 /tmp/web-swupdate-XXXXXX 临时目录
  → swupdate -c → 正式 swupdate
  → preinst：板型、目录、空间、事务锁检查
  → 全部文件写入 transaction/incoming
  → postinst：SHA256及动态库检查
  → 备份旧文件 → prepared → 暂停守护 → 正常停止MQTT/TK8710
  → applying → 同文件系统逐文件rename → 更新版本JSON
  → committed → 恢复守护 → Web保存日志并重启
```

Web进程保留旧inode继续提供升级状态，不在安装脚本中杀死自己。不要调用 `S99tdaemon_job stop`，其旧实现会连Web一起强杀。FRP不在本包中替换，保留远程连接。

运行中失败：安装脚本恢复备份；Web在swupdate非零退出后再次做幂等恢复。升级锁带owner，第二个被拒绝的Web升级不能清除第一个任务的锁或回滚它。不要同时从8080、U盘或命令行安装其他类型固件。

开机恢复：S98遇到`prepared/applying`恢复旧三App和旧版本JSON；`committed`保留新版本；恢复失败保留维护标记，避免守护拉起混合版本。每次成功/已回滚事务在下次升级前归档到`history-XXXXXX`，不会自动删除备份。

故障排查目录：

```text
/userdata/.app-upgrade/transaction/phase
/userdata/.app-upgrade/transaction/backup/
/userdata/.app-upgrade/transaction/incoming/
/userdata/.app-upgrade/history-*/
/userdata/.app-upgrade/last-swupdate.log
/tmp/swupdate.log
```

手动恢复仅用于确认没有升级进程在运行、正在维护窗口的情况：

```sh
/usr/bin/app_upgrade.sh recover
```

它恢复未完成事务，不会把已提交的版本自动降级。若需降级，应对旧的匹配程序重新制作明确版本的SWU并按正常流程安装。

## 6. 验证与边界

本地回归：

```sh
python3 external/rk3506app/swupdattool/gateway/test_transaction.py
```

覆盖成功、哈希损坏、中途失败、SIGKILL后恢复，以及并发任务锁保护。测试根目录为临时隔离目录，不触碰真实userdata。

`validate_board.py`提供实验室SSH/Web验证；`prepare`会安装基础文件并短暂重启Web/守护，`install`会安装并重启指定板子，不能当作只读检查使用。需要Python paramiko/requests和环境变量 `BOARD_PASSWORD`，密码不会写入报告。`verify`允许改用DHCP后的新地址，仍核对原配置哈希和boot_id变化。

板端结果见[验证报告](验证报告.md)。本次没有物理断电试验，SIGKILL恢复不能替代硬件断电验证；没有升级失败后的业务健康自动降级；没有实现SWU签名信任链。SHA256是完整性校验，不是来源认证，仅使用可信来源的包。
