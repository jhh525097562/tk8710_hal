#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
apps=$PWD
sdk=${SDK_ROOT:-$(realpath ../..)}
if [[ "${1:-}" == --stage && "$apps" == "$sdk/external/rk3506app" ]]; then
    app_source=${RK3506_APP_SOURCE:-/mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app}
    bash "$apps/sync_apps.sh" "$app_source" "$sdk"
    export TK8710_APP_SOURCE=${TK8710_APP_SOURCE:-"$app_source/tk8710app"}
fi
host="$sdk/buildroot/output/rockchip_hd_rk3506b_iot_emmc/host"
cc="$host/bin/arm-buildroot-linux-gnueabihf-gcc"
cflags=(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64)
sysroot="$host/arm-buildroot-linux-gnueabihf/sysroot"
test -x "$cc"
test -f "$sysroot/usr/lib/libjson-c.so"
stage="$apps/build_sdk"
mkdir -p "$stage/lib" "$stage/bin"
"$cc" "${cflags[@]}" -shared -fPIC -O2 -Icommon/inc -ISMP/ipc/include SMP/ipc/src/ipc_smp.c common/src/file_logger.c -lrt -o "$stage/lib/libipc_smp.so"
for module in Discovery light mqtt webserver; do
    # Separate output avoids importing Windows objects; no legacy auto-SCP recipes.
    sources=("$module"/src/*.c common/src/file_logger.c)
    libs=(-ljson-c -lpthread -lrt -lm)
    case "$module" in
      Discovery) name=lan_discoveryd ;;
      light) name=led_agent; libs+=(-lgpiod) ;;
      mqtt) name=mqtt_bridge; libs+=(-lgpiod -lmosquitto -lmbedcrypto -lipc_smp) ;;
      webserver) name=web_server; python3 webserver/tools/gen_resources.py;
        sources+=(mqtt/src/gateway_telemetry.c)
        # Use the same Mongoose version as the SDK headers; no runtime .so needed.
        # A pristine toolchain may not contain the manually installed archive.
        mongoose="$sysroot/usr/lib/libmongoose.a"
        if [ ! -f "$mongoose" ]; then
            mongoose="$stage/lib/mongoose.o"
            "$cc" "${cflags[@]}" -O2 -Ithird_party/arm/include -c third_party/arm/src/mongoose.c -o "$mongoose"
        fi
        libs+=(-lgpiod "$mongoose") ;;
    esac
    "$cc" "${cflags[@]}" -std=gnu11 -O2 -Wall -I"$module/inc" -Icommon/inc -Imqtt/inc -idirafter third_party/arm/include \
      "${sources[@]}" -L"$stage/lib" -Lthird_party/arm/lib \
      -Wl,-rpath-link,"$sysroot/usr/lib:$sysroot/lib" "${libs[@]}" -o "$stage/bin/$name"
done
"$cc" "${cflags[@]}" -std=gnu11 -O2 -Ilog/include -Icommon/inc log/daemon/log_daemon.c common/src/file_logger.c -lpthread -o "$stage/bin/log_daemon"
"$cc" "${cflags[@]}" -std=gnu11 -O2 -Ilog/include log/src/log_send_cli.c log/src/log_udp.c -lpthread -o "$stage/bin/log_send"
file "$stage/bin/"* "$stage/lib/"*
"$cc" "${cflags[@]}" -std=gnu11 -O2 -Wall -Wextra -Werror tdaemon/tk8710_reset.c -lgpiod -o "$stage/bin/tk8710_reset"
"$cc" "${cflags[@]}" -std=gnu11 -O2 -Wall -Wextra -Werror watchdog/rk_wdt.c -o "$stage/bin/rk_wdt"
if [ "${1:-}" = "--stage" ]; then
    userdata="$sdk/device/rockchip/common/extra-parts/userdata/normal"
    overlay="$sdk/buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay"
    backup="$sdk/output/app-stage-backup-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$backup/bin" "$backup/lib"
    cp -a "$userdata" "$backup/userdata-normal"
    for name in lan_discoveryd led_agent led_supervisor.sh log_daemon log_send tdaemon_proc.sh tdaemon.sh tdaemon_common.sh tdaemon_net.sh tdaemon_status.sh tdaemon_dns.sh; do
        [ ! -f "$overlay/usr/bin/$name" ] || cp -a "$overlay/usr/bin/$name" "$backup/bin/"
    done
    cp -a "$overlay/usr/lib/libipc_smp.so" "$backup/lib/"
    mkdir -p "$backup/init.d"
    mkdir -p "$overlay/usr/sbin"
    install -m755 "$stage/bin/rk_wdt" "$overlay/usr/sbin/"
    install -m755 watchdog/S15rk-watchdog "$overlay/etc/init.d/"
    [ ! -f "$overlay/etc/init.d/S99tdaemon_job" ] || cp -a "$overlay/etc/init.d/S99tdaemon_job" "$backup/init.d/"
    [ ! -f "$overlay/etc/init.d/S37led_agent" ] || cp -a "$overlay/etc/init.d/S37led_agent" "$backup/init.d/"
    [ ! -f "$overlay/usr/bin/tk8710_reset" ] || cp -a "$overlay/usr/bin/tk8710_reset" "$backup/bin/"
    install -m755 "$stage/bin/tk8710_reset" "$overlay/usr/bin/"
    install -m755 tdaemon/S99tdaemon_job "$overlay/etc/init.d/"
    install -m755 "$stage/bin/mqtt_bridge" "$stage/bin/web_server" "$userdata/"
    bash "$apps/stage_tk8710app.sh"
    install -m755 "$stage/bin/lan_discoveryd" "$stage/bin/led_agent" "$stage/bin/log_daemon" "$stage/bin/log_send" "$overlay/usr/bin/"
    install -m755 light/S37led_agent "$overlay/etc/init.d/"
    install -m755 light/led_supervisor.sh "$overlay/usr/bin/"
    install -m755 "$stage/lib/libipc_smp.so" "$overlay/usr/lib/"
    install -m755 tdaemon/tdaemon_proc.sh tdaemon/tdaemon.sh tdaemon/tdaemon_common.sh tdaemon/tdaemon_net.sh tdaemon/tdaemon_status.sh "$overlay/usr/bin/"
    bash "$apps/tdaemon/stage_dns.sh"
    bash "$apps/stage_frp.sh"
    install -m755 swupdattool/gateway/app_upgrade.sh "$overlay/usr/bin/"
    install -m755 swupdattool/gateway/S98app_upgrade_recover "$overlay/etc/init.d/"
    install -m755 swupdattool/gateway/S80swupdate "$overlay/etc/init.d/"
    echo "Backup: $backup"
fi
