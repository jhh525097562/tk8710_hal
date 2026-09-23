# FRP加入SDK与userdata预置

本地来源：../frp，包含frpc、frpc.toml、start_frpc.sh。
SDK源码/资源目录：external/rk3506app/frp。
固件预置目录：device/rockchip/common/extra-parts/userdata/normal/frp，烧录userdata分区后对应板端/userdata/frp。

frpc为静态ARM EABI5程序，直接预置，无需在SDK重新编译。frpc和start_frpc.sh权限755，frpc.toml权限600。三份文件逐一SHA256一致。未复制运行时生成配置或日志。

启动脚本默认从/etc/config.json读取gateway_id（16位字母、数字、下划线或连字符），替换配置模板中的DEVICE_ID_STRING，生成/userdata/frp/frpc.generated.toml，日志在/var/log/frpc.log。

## 后续同步和构建

sync_sdk_apps.sh的同步清单已加入frp。
SDK external/rk3506app/stage_frp.sh可单独预置FRP，覆盖前备份现有预置目录。
build_apps.sh --stage会自动调用stage_frp.sh，保持后续预置流程一致。

```sh
# SDK根目录：仅更新FRP预置文件
bash external/rk3506app/stage_frp.sh
# 如需生成包含FRP的升级镜像
./build.sh extra-parts
./build.sh updateimg
```

本次完成文件同步、预置、权限和语法/哈希检查，未重建镜像、未部署实板、未启动FRP，也未新增tdaemon自动启动项。是否开机建立隧道需按部署需求另行启用；当前需求为文件预置。
