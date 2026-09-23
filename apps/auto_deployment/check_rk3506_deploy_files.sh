#!/bin/sh

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

check_file() {
    item_id="$1"
    name="$2"
    base_dir="$3"

    if [ -z "$name" ] || [ -z "$base_dir" ]; then
        echo "[SKIP] $item_id | invalid entry"
        SKIP_COUNT=$((SKIP_COUNT + 1))
        return
    fi

    case "$base_dir" in
        */"$name")
            target_path="$base_dir"
            ;;
        *)
            target_path="$base_dir/$name"
            ;;
    esac

    if [ -e "$target_path" ]; then
        echo "[ OK ] $item_id | $target_path"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "[MISS] $item_id | $target_path"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

skip_item() {
    item_id="$1"
    name="$2"
    reason="$3"

    echo "[SKIP] $item_id | $name | $reason"
    SKIP_COUNT=$((SKIP_COUNT + 1))
}

echo "RK3506 部署文件检查开始"
echo "========================================"

check_file "P1-01" "tdaemon_proc.sh" "/usr/bin"
check_file "P1-02" "tdaemon_net.sh" "/usr/bin"
check_file "P1-03" "tdaemon_common.sh" "/usr/bin"
check_file "P1-04" "tdaemon_gpio.sh" "/usr/bin"
check_file "P1-05" "tdaemon_recovery.sh" "/usr/bin"
check_file "P1-1" "tdaemon.conf" "/etc"
check_file "P1-2" "S99tdaemon_job" "/etc/init.d"
check_file "P1-3" "S35cat1_ecm" "/etc/init.d"
check_file "P1-4" "hostapd.conf" "/etc"
check_file "P1-5" "wpa_supplicant.conf" "/etc"
check_file "P1-6" "interfaces" "/etc/network"
check_file "P1-7" "inittab" "/etc/inittab"
check_file "P1-8" "config.json" "/etc"
check_file "P1-9" "S37led_agent" "/etc/init.d"
check_file "P1-10" "device_version.json" "/etc"
check_file "P1-11" "S38cfg_restore_by_gpio" "/etc/init.d"
check_file "P1-12" "S40bluetoothd" "/etc/init.d"
check_file "P1-13" "S36wifibt-init.sh" "/etc/init.d"
check_file "P1-14" "ntp.conf" "/etc"
check_file "P1-15-1" "private.pem" "/etc"
check_file "P1-15-2" "public.pem" "/etc"
# Traffic statistics are now read by web_server from /proc/net/dev.
# /etc/turbotraffic stores only user-reset baselines; no S39 producer is required.
check_file "P2" "log_daemon" "/usr/bin"
check_file "P2-1" "log_send" "/usr/bin"
check_file "P3" "mqtt_bridge" "/userdata"
check_file "P4" "tk8710_gw" "/userdata"
check_file "P5" "lan_discoveryd" "/usr/bin"
check_file "P6" "web_server" "/userdata"
check_file "P7" "swupdate" "/usr/bin"
check_file "P8-1" "led_agent" "/usr/bin"
check_file "L3" "openssl" "/usr/bin"
check_file "D1" "tdaemon.sh" "/usr/bin"

skip_item "P8" "other" "文档描述为业务集合，不是明确单文件"
skip_item "L1" "mosquitto" "文档描述为移植库，未给出固定文件路径"
skip_item "L2" "ipc_smp" "文档描述为自研库，未给出固定文件路径"

echo "========================================"
echo "检查完成: OK=$PASS_COUNT MISS=$FAIL_COUNT SKIP=$SKIP_COUNT"

if [ "$FAIL_COUNT" -ne 0 ]; then
    exit 1
fi

exit 0
