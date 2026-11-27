# RK3506B 网关 SDK 与 App 定制工程

本仓库管理当前RK3506网关App、相对原厂SDK的定制文件、构建入口及开发说明。
原厂SDK基线为 `rk3506_linux6.1_sdk_v1.2.0`，目标板型为 **HD-RK3506B-IOT-EMMC**。
这不是脱离原厂SDK即可独立构建的完整SDK镜像。

## 目录和维护入口

2026-11-26 已整合现有 App 更新：统一灯控及独立守护、网络状态 JSON、
MQTT/GPS 心跳扩展，并保留 MQTT 就绪启动门控。服务停止流程按用户选择
与现用 WSL 一致：按配置强制停止，不清空 IPC，不显式保留 FRP。
验证范围与二进制来源见 [本轮整合与验证](docs/App版本整合与验证_20261126.md)。

- `apps/`：App维护主线。已集成模块以当前SDK副本为准，补充本地独立工具（含PPS计数器）。
- `sdk/baseline/`：原厂子仓库提交、manifest和定制文件/删除清单。
- `sdk/overlay/`：按SDK相对路径保存新增及修改文件。既有文件的修改也保存完整文件，便于审查和维护；删除项在changes.json中。
- `reference/local-app-variants/`：导入时与SDK不同的本地文件，仅用于核对，不参与构建。以后统一修改apps，不维护两份主线。
- `scripts/apply_sdk.py`：在匹配基线的干净SDK副本上检查并恢复定制。
- `docs/migration-inventory.json`：本地差异、排除项、预编译依赖校验清单。

## 获取及恢复

准备同版本原厂SDK及其Git基线，保留原厂下载包/归档。manifest中的内网服务器不保证外部可访问。
推荐在WSL/Linux文件系统中克隆，避免Windows符号链接和权限问题。

```sh
git clone git@github.com:jhh525097562/rk3506b.git
cd rk3506b
python3 scripts/apply_sdk.py /path/to/clean/rk3506_linux6.1_sdk_v1.2.0
python3 scripts/apply_sdk.py /path/to/clean/rk3506_linux6.1_sdk_v1.2.0 --apply
```

恢复脚本拒绝基线不符、已有跟踪文件改动和目标App已存在的工作区。不要对正在开发的SDK强制覆盖。
需要恢复完整原厂子项目时，使用自己保存的SDK包或对应manifest；仓库不包含原厂完整源码历史和交叉工具链。

## 配置及构建

发布副本中的敏感配置已模板化，正式打包前必须填入实际部署参数；签名私钥、系统密码数据库不入库。
保留的公钥与发布签名私钥必须匹配。参见 `docs/迁移与验证说明.md`。

```sh
cd /path/to/clean/rk3506_linux6.1_sdk_v1.2.0
# 按SDK正常方式选择B-IOT-EMMC板型、准备Buildroot host依赖后：
bash external/rk3506app/build_apps.sh --stage
./build.sh
./build.sh extra-parts
./build.sh updateimg
```

SDK 内的 `build_apps.sh --stage` 现在会先从 Windows 开发目录自动同步应用，
备份被覆盖/删除的旧文件并校验内容。已有 SDK 产物时可只重做 Buildroot、
extra-parts 和 updateimg，参见 [App自动同步与构建](docs/App自动同步与构建.md)。
在本仓库 `apps/` 中直接构建时仍使用仓库源码。

`apps/tk8710app/`保存现用ARM程序和测试数据。TK8710 HAL源码继续在独立的
`jhh525097562/tk8710_hal`项目中维护；更新这里的二进制时同步记录来源版本及SHA256。
App源码构建不等于固件打包完成，也不等于板端验证通过。

## Git使用

继续在原App目录和WSL SDK开发时，使用[增量同步脚本](docs/增量同步使用说明.md)先比较、处理冲突，再同步到本仓库。
首次执行 `scripts/同步开发目录.ps1 -Init`，日常默认预览，添加 `-Apply -Select apps/mqtt` 等参数才写入；不会自动提交或推送。

修改主线源码及对应SDK定制文件，检查后提交：

```sh
git status --short
git add <明确要提交的文件>
git diff --cached --stat
git commit -m "说明具体修改"
git push origin main
```

不要提交板端日志、固件镜像、备份、真实密码或签名私钥。必要的小型预编译依赖已纳入仓库；固件发布包单独归档。
