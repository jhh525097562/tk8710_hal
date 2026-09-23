# Web SWU升级流程与制作说明

2026-09-15更新：已按推荐流程实现三App事务升级，接入SDK的源码、Buildroot overlay和userdata预置步骤。已在原192.168.100.62测试板进行Web升级；首次重启后DHCP地址变为192.168.100.227。完整操作见[三应用SWU使用说明](../swupdattool/gateway/README.md)，逐项证据见[验证报告](../swupdattool/gateway/验证报告.md)。

## 1. 两种升级不要混用

| 格式 | 生成/使用方式 | 更新范围 |
|---|---|---|
| Rockchip update.img | SDK build.sh updateimg，RKDevTool等烧录 | 由package-file包含的分区决定 |
| SWUpdate .swu | CPIO归档，Web上传后swupdate安装 | 由包内sw-description及脚本决定 |

将update.img改名为.swu不能使用。当前Web上限256MiB，暂存/tmp；大包还受/tmp空间和内存限制。不建议直接以当前Web在运行系统上覆盖rootfs。

## 2. Web执行步骤

1. 选择.swu，前端检查扩展名。
2. 每512KiB上传一块到POST /upload?file=image.swu&offset=N，body为原始字节。
3. 后端写/tmp/image.swu。offset=0清空旧上传，后续offset必须等于当前文件长度；限制总长度256MiB。
4. 上传完成后POST /upload/start，后端检查基础恢复工具，将上传文件rename为/tmp/image.installing.swu并冻结后续上传。
5. 线程确认没有其他SWUpdate/进度监听进程后，以独立TMPDIR运行 `swupdate -v -c -i /tmp/image.installing.swu -e stable,copy1`，检查成功后去掉-c正式安装。TMPDIR隔离临时文件，但该板二进制socket路径固定，因此基础S80脚本不再启动原8080更新服务及swupdate-progress监听器，由8090 Web统一管理升级。
6. SWUpdate解析sw-description、处理文件和安装脚本。三App包会先暂存、校验、备份并暂停守护，再替换程序、提交版本。
7. 子命令退出0时Web标记done，保存日志至/userdata/.app-upgrade/last-swupdate.log，约3秒后请求reboot；失败标记error。正式安装失败还会调用带事务owner的恢复工具。校验失败不安装、不重启。
8. 页面每秒查询/api/upgrade_status。0～90%为上传比例，92～99%为模拟进度，不是真实分区写入进度。

源码：src/web_server.c的handle_upload、write_upload_chunk、handle_upgrade_start；src/upgrade_manager.c的upgrade_thread_func。

## 3. 历史UART示例包实际内容

目录：../swupdattool/uartpackage/。已读取真实uart_debug_v1.0.1.swu归档，包含：

```text
sw-description
test_uart
update.sh
```

sw-description版本1.0.1；files将test_uart安装至/tmp/test_uart；scripts声明update.sh类型shellscript。

update.sh流程：

- preinst只打印Stopping service，实际停止服务命令被注释。
- postinst执行 `mv /tmp/test_uart /userdata/test_uart`，再chmod755。
- 启动测试程序/服务的命令也被注释。
- 最后exit0。

因此当前包只替换一个UART测试程序，不更新内核、rootfs、MQTT、Web、TK8710、FRP或版本JSON。Web层另行重启整机。

重要：脚本没有set -e，也未检查mv/chmod退出码，前面失败仍可能执行exit0；不能仅凭Update Success输出判断升级真实成功。

## 4. stable,copy1的含义与能力核对

-e stable,copy1是选择软件集合stable与模式copy1，不自动提供A/B分区或回滚。
本SDK parser/parser.c按board+集合+模式、集合+模式、board、根software顺序查找。因此UART包虽然只有根files/scripts，也可被当前程序选中。

构建目录.config显示CONFIG_SHELLSCRIPTHANDLER未启用、SSL_IMPL_NONE；但rootfs中的实际/usr/bin/swupdate通过-v打印注册了raw、rawfile、shellscript、preinstall、postinstall。必须以最终二进制验证，不能把构建配置当作最终镜像能力证明。

已执行rootfs程序的QEMU检查：`swupdate -v -c -i uart_debug_v1.0.1.swu -e stable,copy1`，退出0，识别1个artifact和1个script。该结果仅证明检查模式接受包，不证明postinst成功或断电可恢复。

## 5. 如何重新制作现有UART包

在WSL执行：

```sh
cd /mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app/swupdattool/uartpackage
file test_uart
sh -n update.sh
# 确认sw-description版本和目标路径，再生成新名称，保留旧包
printf '%s\n' sw-description test_uart update.sh | cpio -ov -H crc > uart_debug_new.swu
# 仅列目录和检查，不安装
cpio -it < uart_debug_new.swu
sha256sum uart_debug_new.swu
```

sw-description必须首先进入归档，文件名应与清单逐字一致，不用直接递归打包整个目录。crc是CPIO校验格式，不是加密签名。更新test_uart之前先交叉编译为板端ARM产物。

板端只校验（实际程序支持-c）：

```sh
swupdate -v -c -i /userdata/uart_debug_new.swu -e stable,copy1
```

执行安装是有副作用操作：仅在备份和停止相关程序后执行 `swupdate -v -i <包路径> -e stable,copy1`，或通过Web上传。不需要同时执行两种入口。

## 6. 已实现的三App升级流程

实现位置：`../swupdattool/gateway/`。先前推荐现已实现为 `app_upgrade.sh`、`build_swu.py`、`make_app_swu.sh`、版本JSON辅助程序及开机恢复脚本。当前包固定更新以下三App；FRP和运行库不纳入覆盖。

当前范围与预留范围：

| 内容 | 安装目标 | 注意 |
|---|---|---|
| mqtt_bridge | /userdata/mqtt_bridge | 与TK8710共享协议必须匹配 |
| web_server | /userdata/web_server | 匹配运行库；升级管理进程本身需特殊处理 |
| tk8710_gw | /userdata/tk8710_gw | 与MQTT配套 |
| FRP（本包不更新） | /userdata/frp | 保留现场连接及配置 |
| 依赖库（本包不更新） | 对应lib目录 | 构建检查依赖，板端安装前再验证动态加载 |
| device_version.json | /etc/device_version.json | 仅全部安装成功后更新app_bundle_version，保留其余键 |

保留/etc/config.json、网络配置、GPS/RF/校准文件和用户日志，不用“整个userdata替换”方式实现日常App升级。

实际包结构：sw-description、install.sh、三个ARM程序、metadata辅助程序。每个程序先落入 `/userdata/.app-upgrade/transaction/incoming/`，再由postinst切换到正式路径，不直接写正在运行的可执行文件。

示意清单（仅展示一个文件，其他程序同样显式列出）：

```text
software = {
    version = "网关发布版本";
    stable = {
        copy1 = {
            files = (
                { filename = "mqtt_bridge"; path = "/userdata/.app-upgrade/transaction/incoming/mqtt_bridge"; }
            );
            scripts = (
                { filename = "install.sh"; type = "shellscript"; }
            );
        };
    };
};
```

install.sh已经实现：

1. preinst验证空间、RK3506板型及目录，获取事务锁；全部载荷落地后校验SHA256和动态依赖，再备份旧程序和版本JSON。
2. 设置维护标记，暂停守护自动拉起，正常停止MQTT和TK8710，最多等待20秒；未退出则拒绝安装。FRP不替换也不停止。
3. 不在preinst杀掉发起升级的web_server：它等待swupdate返回并负责状态和重启，否则浏览器失联且可能失去重启动作。Web二进制可采用同文件系统rename切换路径，旧进程继续持有旧inode，重启后加载新版本。
4. 文件全部落地后校验内容、设置权限，再切换正式路径；任何一步失败均非零退出。
5. 如中途失败，恢复已替换文件和守护状态；全部成功才更新版本文件。
6. Web收到成功后重启。S98app_upgrade_recover在开机时恢复prepared/applying事务；committed事务保留新版本。已验证SIGKILL中断恢复，尚未做物理断电实验。

特别不要直接调用当前S99tdaemon_job stop：其停止逻辑会强杀配置列表中的所有服务，可能把发起升级的Web也杀掉。应为升级提供有明确边界的暂停/恢复机制。

制作步骤：先编译并核对ARM架构和动态库 → 生成匹配版本清单/安装脚本 → 检查shell语法 → 按清单顺序生成CPIO → 使用目标SWUpdate -c检查 → 测试板验证安装、失败回滚、重启后版本及配置保留 → 才发布给现场。

SDK内一键制作示例（新发布版本不能与已有文件重名）：

```sh
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
bash external/rk3506app/swupdattool/gateway/make_app_swu.sh 20260915.5
```

默认使用已经预置的tk8710_gw，独立HAL不由此脚本编译；可通过第二参数传入单独编译好的新ARM程序。基础固件安装、可选TK8710路径、Windows取包及验收命令见[详细使用说明](../swupdattool/gateway/README.md)。

## 7. 签名和U盘流程

Web当前没有上传外置.swu.sig或调用openssl验签的代码；即使/etc/public.pem存在，也不能据此认定Web会验签。SWUpdate内置签名是否启用须核对最终二进制及启动配置。

usb_update/usb_update.sh另有整包签名流程：读取同名.swu.sig，用/etc/public.pem执行openssl dgst -sha256 -verify，然后才应安装。但当前该脚本真正的swupdate命令已注释，后续成功判断可能只检查echo等命令的状态，不能当作已经完成升级。

两种签名机制不同：外置整包.sig需要入口验签；SWUpdate原生签名通常围绕归档内描述文件签名，依赖实际构建支持，不能混用。私钥留在打包端，不放进升级包或板端。

## 8. 验收清单

Web上传成功、SWUpdate检查通过、安装脚本退出0、重启后版本正确、MQTT重新连接、TK8710初始化成功、配置/校准保留是不同层次，必须逐层验证。
已完成三App制作、交叉编译、隔离故障回滚测试、板端无效包拒绝和实际Web升级。最终版本、重启后校验及未覆盖边界以[验证报告](../swupdattool/gateway/验证报告.md)为准。未进行物理断电测试，也未重新烧录完整update.img；SDK集成已落到源码和预置输入中。
