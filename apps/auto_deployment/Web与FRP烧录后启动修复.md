# Web与FRP烧录后未启动：SDK修复记录

2026-09-14，本次检查SDK本地预置与rootfs，未连接新烧录板。

## Web

原build_apps.sh添加-Lthird_party/arm/lib并以-lmongoose链接，产生libmongoose.so动态依赖，但rootfs target/usr/lib没有该库。本地third_party为Mongoose7.0，而SDK头文件为7.12，也存在ABI混用风险。

已修改SDK external/rk3506app/build_apps.sh，显式链接sysroot/usr/lib/libmongoose.a。执行--stage重编并更新userdata/web_server。readelf确认没有libmongoose.so依赖；qemu-arm-static -L <Buildroot target> <新web_server> --version返回V1.0.1，退出0。

## FRP

此前只预置文件，没有开机启动项。现已在SDK板型overlay/etc/tdaemon.conf及external/rk3506app/tdaemon/tdaemon.conf加入：

```ini
proc=/userdata/frp/start_frpc.sh
```

保留原tk8710_gw完整参数。脚本语法检查通过；仅证明开机入口已接入，FRP服务端连通、配置认证和端口映射需要板端另行验证。

## 重新生成镜像

```sh
cd /home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0
./build.sh buildroot && ./build.sh extra-parts && ./build.sh updateimg
```

App已在本次重建并stage。后续源码再修改时先运行bash external/rk3506app/build_apps.sh --stage。

输出output/update/Image/update.img。rootfs包含FRP守护项，userdata包含修正版Web和FRP文件；二者都要重建。本次尚未执行上述镜像构建或烧录。
