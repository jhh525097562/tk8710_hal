#!/bin/sh
###
 # @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @Date: 2026-03-13 09:31:54
 # @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @LastEditTime: 2026-03-13 17:01:12
 # @FilePath: \wlan0\switch_to_ap.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 
LOG=/var/log/wifi_ap.log
exec 2>>"$LOG"

fail() {
	echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >&2
	exit 1
}

if command -v pkill >/dev/null 2>&1; then
	pkill -x wpa_supplicant >/dev/null 2>&1 || true
	pkill -x udhcpc >/dev/null 2>&1 || true
	pkill -x hostapd >/dev/null 2>&1 || true
	pkill -x udhcpd >/dev/null 2>&1 || true
else
	killall wpa_supplicant >/dev/null 2>&1 || true
	killall udhcpc >/dev/null 2>&1 || true
	killall hostapd >/dev/null 2>&1 || true
	killall udhcpd >/dev/null 2>&1 || true
fi

if ! command -v hostapd >/dev/null 2>&1; then
	fail "hostapd不存在"
fi
if ! command -v udhcpd >/dev/null 2>&1; then
	fail "udhcpd不存在"
fi
if [ ! -f /etc/hostapd.conf ]; then
	fail "hostapd.conf不存在"
fi
if [ ! -f /etc/udhcpd.conf ]; then
	fail "udhcpd.conf不存在"
fi

CONFIG_FILE=/userdata/config.json
AP_SSID=RK3506
if [ -f "$CONFIG_FILE" ]; then
	GATEWAY_ID=$(awk -F\" '/"gateway_id"[[:space:]]*:/ {print $4; exit}' "$CONFIG_FILE" 2>/dev/null)
	case "$GATEWAY_ID" in
		""|*[!A-Za-z0-9_-]*)
			AP_SSID=RK3506
			;;
		*)
			AP_SSID="$GATEWAY_ID"
			;;
	esac
fi

AP_PASS=$(printf '\x31\x32\x33\x34\x35\x36')
if [ "${#AP_PASS}" -lt 8 ] || [ "${#AP_PASS}" -gt 63 ]; then
	AP_PASS=12345678
fi
readonly AP_PASS

ifconfig wlan0 192.168.4.1 netmask 255.255.255.0 up >/dev/null 2>&1 || fail "设置静态IP失败"

HOSTAPD_LOG=/var/log/hostapd.log
rm -f "$HOSTAPD_LOG" >/dev/null 2>&1 || true
mkdir -p /var/run >/dev/null 2>&1 || true
HOSTAPD_TMP=/var/run/hostapd.ap.conf
sed -e "s/^ssid=.*/ssid=$AP_SSID/" -e "s/^wpa_passphrase=.*/wpa_passphrase=$AP_PASS/" /etc/hostapd.conf > "$HOSTAPD_TMP" 2>/dev/null || fail "生成hostapd配置失败"
mkdir -p /var/run/hostapd >/dev/null 2>&1 || true
if ! grep -q '^ctrl_interface=' "$HOSTAPD_TMP" 2>/dev/null; then
	printf "ctrl_interface=/var/run/hostapd\nctrl_interface_group=0\n" >> "$HOSTAPD_TMP"
fi
hostapd -B -f "$HOSTAPD_LOG" "$HOSTAPD_TMP" >/dev/null 2>&1 || fail "hostapd启动失败"
sleep 1
if [ ! -S /var/run/hostapd/wlan0 ]; then
	echo "$(date '+%Y-%m-%d %H:%M:%S') hostapd控制接口不存在" >&2
fi

udhcpd /etc/udhcpd.conf >/dev/null 2>&1 || fail "udhcpd启动失败"

printf "nameserver 223.5.5.5\nnameserver 180.76.76.76\n" > /etc/resolv.conf 2>/dev/null || fail "写入resolv.conf失败"

exit 0
