# RK3506 APP 部署依赖清单

本文基于当前 ARM 构建产物的 `readelf -d` 结果整理，用于部署到 RK3506/RK3506B 设备时核对动态库。

## 构建产物

| 程序 | 构建路径 | 类型 |
| --- | --- | --- |
| `lan_discoveryd` | `app/Discovery/build_arm/lan_discoveryd` | ARM 32-bit PIE executable |
| `mqtt_bridge` | `app/mqtt/build_arm/mqtt_bridge` | ARM 32-bit PIE executable |
| `web_server` | `app/webserver/build_arm/web_server` | ARM 32-bit PIE executable |
| `led_agent` | `app/light/build_arm/led_agent` | ARM 32-bit PIE executable |
| `tk8710_gw` | `tk8710app/tk8710_gw` | ARM 32-bit PIE executable；由SDK统一预置入口部署 |
| `log_daemon` | `app/log/bin/log_daemon` | ARM 32-bit PIE executable |

## 程序直接依赖

| 程序 | 直接依赖库 |
| --- | --- |
| `lan_discoveryd` | `libm.so.6`, `libjson-c.so.5`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `mqtt_bridge` | `libm.so.6`, `libgpiod.so.2`, `libmosquitto.so.1`, `libjson-c.so.5`, `libipc_smp.so`, `libmbedcrypto.so.7`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `web_server` | `libm.so.6`, `libgpiod.so.2`, `libjson-c.so.5`, `libmongoose.so`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `led_agent` | `libm.so.6`, `libgpiod.so.2`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `tk8710_gw` | `libm.so.6`, `libgpiod.so.2`, `libipc_smp.so`, `libc.so.6`, `ld-linux-armhf.so.3` |
| `log_daemon` | `libc.so.6`, `ld-linux-armhf.so.3` |

## 应随应用部署的本地库

这些库不在当前 SDK sysroot 的通用系统库范围内，建议随应用一起部署，或加入目标 rootfs 的标准动态库搜索路径。

| 库 | 仓库来源 | SONAME / 说明 | 使用者 |
| --- | --- | --- | --- |
| `libjson-c.so.5` | `app/third_party/arm/lib/libjson-c.so.5.3.0` | SONAME: `libjson-c.so.5` | `lan_discoveryd`, `mqtt_bridge`, `web_server` |
| `libmosquitto.so.1` | `app/third_party/arm/lib/libmosquitto.so.1` | SONAME: `libmosquitto.so.1` | `mqtt_bridge` |
| `libmbedcrypto.so.7` | `app/third_party/arm/lib/libmbedcrypto.so.7` | SONAME: `libmbedcrypto.so.7` | `mqtt_bridge` |
| `libmongoose.so` | `app/third_party/arm/lib/libmongoose.so` | 无显式 SONAME，文件名即运行期依赖名 | `web_server` |
| `libipc_smp.so` | `app/mqtt/inc/libipc_smp.so` 或 `app/spi/inc/libipc_smp.so` | SONAME: `libipc_smp.so` | `mqtt_bridge`, `tk8710_gw` |

## 需要目标系统提供的库

当前 SDK sysroot 中已找到以下库。部署时应确认目标设备 rootfs 中也存在相同 ABI 的库。

| 库 | SDK sysroot 路径 | 来源类型 |
| --- | --- | --- |
| `libgpiod.so.2` | `usr/lib/libgpiod.so.2` | 系统库 |
| `libssl.so.3` | `usr/lib/libssl.so.3` | `libmosquitto.so.1` 二级依赖 |
| `libcrypto.so.3` | `usr/lib/libcrypto.so.3` | `libmosquitto.so.1` / `libssl.so.3` 二级依赖 |
| `libz.so.1` | `usr/lib/libz.so.1` | `libmbedcrypto.so.7` 二级依赖 |
| `libatomic.so.1` | `usr/lib/libatomic.so.1` | `libssl.so.3` / `libcrypto.so.3` 二级依赖 |
| `libm.so.6` | `usr/lib/libm.so.6` | libc 运行库 |
| `libc.so.6` | `usr/lib/libc.so.6` | libc 运行库 |
| `ld-linux-armhf.so.3` | `usr/lib/ld-linux-armhf.so.3` | ARM hard-float loader |

## 建议部署集合

如果目标 rootfs 已包含 `libgpiod/libssl/libcrypto/libz/libatomic/libm/libc/ld-linux-armhf`，应用侧至少部署：

```text
libjson-c.so.5
libmosquitto.so.1
libmbedcrypto.so.7
libmongoose.so
libipc_smp.so
```

如果目标 rootfs 不包含 OpenSSL、zlib 或 libatomic，还需从匹配 SDK/rootfs 中补充：

```text
libssl.so.3
libcrypto.so.3
libz.so.1
libatomic.so.1
```

## 验证命令

在构建机上查看依赖：

```sh
readelf -d app/mqtt/build_arm/mqtt_bridge | grep NEEDED
readelf -d app/webserver/build_arm/web_server | grep NEEDED
readelf -d app/Discovery/build_arm/lan_discoveryd | grep NEEDED
```

在设备上验证动态库是否可解析：

```sh
ldd ./mqtt_bridge
ldd ./web_server
ldd ./lan_discoveryd
```

如设备没有 `ldd`，可直接运行程序并检查是否出现 `error while loading shared libraries`。
