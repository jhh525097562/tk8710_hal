# App 自动同步与 SDK 构建

日期：2026-10-28。

## 解决的问题

Windows 的 `RK3506资料/rk3506/rk3506app` 与 WSL SDK 的
`external/rk3506app` 是两个独立目录。旧的 `build_apps.sh --stage` 只从 SDK
源码编译 MQTT/Web，只有 TK8710 单独从 Windows 复制成品，因此完整编译 SDK
也不能带入尚未同步的 MQTT 心跳改动。

## 默认入口

已将 `apps/sync_apps.sh` 接入 SDK 中的 `build_apps.sh --stage`，在工具链检查和
编译之前执行。源目录缺失、模块不全、同步失败或校验不一致均返回非零，停止构建。

```bash
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0 && \
bash external/rk3506app/build_apps.sh --stage && \
./build.sh buildroot && \
./build.sh extra-parts && \
./build.sh updateimg
```

默认来源：`/mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app`。
同步目标：`/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0/external/rk3506app`。
输出：SDK 的 `output/update/Image/update.img`。
已有完整 SDK 产物时，以上流程更新应用和 rootfs，不重新编译内核/U-Boot。
更改内核、驱动、板级配置时仍按相应 SDK 流程构建。

在本 Git 仓库的 `apps/` 中直接构建时，继续使用仓库自身源码；只有安装到
`SDK/external/rk3506app` 后的 `--stage` 默认读取 Windows 开发目录。
不带 `--stage` 的 App 编译不自动同步、不更新固件预置目录。

## 同步范围及备份

同步模块：`Discovery`、`SMP`、`ipcweb`、`light`、`log`、`mqtt`、`tdaemon`、
`webserver`、`common`、`third_party`、`frp`、`watchdog`、`swupdattool`、`tk8710app`。
包含 Web 页面/资源生成工具、公共头文件和依赖、服务脚本、FRP 配置和 TK8710 成品。
TK8710 HAL 源码不会在此流程中编译。

这些模块以 Windows 源目录为准，按内容校验同步，包含源文件删除。
旧文件在覆盖或删除前保存到 SDK 的
`output/app-sync-backup-<时间>-<随机后缀>/files/<模块>/`。
`changes.txt` 记录变更路径，`paths.txt` 记录来源与目标；备份可能包含现场配置，应留在本地。
若需恢复单个文件，将 `files/` 下对应文件复制回 SDK 应用目录的相同相对路径。

排除 Git/编辑器元数据、build/build_*、bin、docs、日志、备份、对象文件、固件包
和构建时重新生成的 Web `web_resources.h`；
这些被排除的 SDK 目录不会因源侧缺失而删除。拒绝源模块中的符号链接/特殊文件，
拒绝源目标嵌套和目标模块符号链接。构建/预置入口脚本不由模块同步覆盖，避免自我替换。
新增发布模块或修改顶层构建/预置脚本时，需要同时更新对应入口。

## 单独预览、同步和切换来源

```bash
# 在 SDK 根目录预览，不写文件
bash external/rk3506app/sync_apps.sh --dry-run

# 只同步，不编译
bash external/rk3506app/sync_apps.sh

# 指定另一份完整应用树
RK3506_APP_SOURCE=/path/to/apps bash external/rk3506app/build_apps.sh --stage

# 明确只使用当前 SDK 内源码，不从 Windows 同步
RK3506_APP_SOURCE="$PWD/external/rk3506app" \
    bash external/rk3506app/build_apps.sh --stage
```

独立同步支持 `bash sync_apps.sh [--dry-run] [源目录 [SDK根目录]]`。
旧入口 `auto_deployment/sync_sdk_apps.sh` 转发到同一个同步实现。
`TK8710_APP_SOURCE` 如显式设置，则覆盖 TK8710 成品的预置来源；默认跟随本次应用源目录。

## 验证

隔离回归：在 WSL 执行 `python3 apps/tests/test_sync_apps.py -v`。
覆盖内容更新、相同大小/时间戳、删除备份、生成文件排除、预览无写入、模块缺失、
符号链接拒绝、重复同步和显式使用 SDK 源码。
构建后的 GPS 心跳字段可静态检查：

```bash
strings device/rockchip/common/extra-parts/userdata/normal/mqtt_bridge \
    | grep -x 'avgsnr'
```

该检查证明平均 SNR 字段存在于预置程序。`count` 可能被编译器合并为其他字符串的
后缀，不能仅以 `strings` 是否独立列出它判断功能。板端须另外验证程序哈希和实际 MQTT 心跳，
并检查 `/userdata/GPS/GPSinfo.txt` 中的 `count/avgsnr`。打包及静态检查不代表烧录验收。

2026-10-28 实际验证：7项隔离回归通过；已在现用 WSL SDK 运行
`bash external/rk3506app/build_apps.sh --stage`，14个模块同步校验和应用交叉编译/预置成功。
MQTT 源码已包含 `gps_info.c`、`count/avgsnr` 封包；预置程序包含 `avgsnr` 字符串，
且与 `build_sdk/bin/mqtt_bridge` 的 SHA-256 一致：
`3da697904daa982826537e4f8331db420868498bd711ad08806a47aca831f541`。
同步备份为 SDK 的 `output/app-sync-backup-20261028-151946-9E6nqo`，
构建日志为 `output/app-autosync-validation-20261028-1.log`。
编译有既有字符串截断等告警；本次没有重新生成 rootfs/update.img 或执行板端升级。
