#!/bin/sh
LOG=/var/log/wifi_ap.log
exec 2>>"$LOG"

fail() {
	echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >&2
	exit 1
}

kill_proc_by_iface() {
	name="$1"
	iface="$2"
	for p in /proc/[0-9]*; do
		[ -r "$p/cmdline" ] || continue
		cmdline=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
		echo "$cmdline" | grep -Eq "(^|[[:space:]])$name([[:space:]]|$)" || continue
		echo "$cmdline" | grep -Eq "(^|[[:space:]])$iface([[:space:]]|$)|-i[[:space:]]*$iface|-i$iface" || continue
		kill -TERM "${p##*/}" >/dev/null 2>&1 || true
		sleep 1
		kill -KILL "${p##*/}" >/dev/null 2>&1 || true
	done
}

stop_ap_stack() {
	if command -v pkill >/dev/null 2>&1; then
		pkill -x hostapd >/dev/null 2>&1 || true
		pkill -x udhcpd >/dev/null 2>&1 || true
	else
		killall hostapd >/dev/null 2>&1 || true
		killall udhcpd >/dev/null 2>&1 || true
	fi
	rm -f /var/run/hostapd/wlan0 >/dev/null 2>&1 || true
	rm -f /var/run/hostapd.ap.conf >/dev/null 2>&1 || true
	ifconfig wlan0 down >/dev/null 2>&1 || true
}

if [ "$1" = "stop" ]; then
	stop_ap_stack
	exit 0
fi

kill_proc_by_iface wpa_supplicant wlan0
kill_proc_by_iface udhcpc wlan0
stop_ap_stack

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

CONFIG_FILE=/etc/config.json
AP_SSID=RK3506
if [ -f "$CONFIG_FILE" ]; then
	GATEWAY_ID=$(tr -d '\r\n' < "$CONFIG_FILE" | sed -n 's/.*"gateway_id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
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
