#!/bin/sh 
LOG=/var/log/wifi_sta.log
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

if command -v pkill >/dev/null 2>&1; then
	pkill -x hostapd >/dev/null 2>&1 || true
	pkill -x udhcpd >/dev/null 2>&1 || true
else
	killall hostapd >/dev/null 2>&1 || true
	killall udhcpd >/dev/null 2>&1 || true
fi

kill_proc_by_iface udhcpc wlan0

if ! command -v wpa_supplicant >/dev/null 2>&1; then
	fail "wpa_supplicant不存在"
fi
if ! command -v wpa_cli >/dev/null 2>&1; then
	fail "wpa_cli不存在"
fi
if ! command -v udhcpc >/dev/null 2>&1; then
	fail "udhcpc不存在"
fi

if [ ! -d /sys/class/net/wlan0 ]; then
	fail "wlan0不存在"
fi
if [ ! -f /etc/wpa_supplicant.conf ]; then
	fail "wpa_supplicant.conf不存在"
fi
mkdir -p /var/run/wpa_supplicant >/dev/null 2>&1 || true
ifconfig wlan0 up >/dev/null 2>&1 || true
WPA_LOG=/var/log/wpa_supplicant.debug
rm -f "$WPA_LOG" >/dev/null 2>&1 || true

start_wpa() {
	wpa_supplicant -B -D "$1" -i wlan0 -c /etc/wpa_supplicant.conf -P /var/run/wpa_supplicant.wlan0.pid -f "$WPA_LOG" >/dev/null 2>&1
}

if ! start_wpa "nl80211"; then
	if ! start_wpa "wext"; then
		fail "wpa_supplicant启动失败"
	fi
fi

SSID=$(awk -F\" '/^[[:space:]]*ssid=/{print $2;exit}' /etc/wpa_supplicant.conf)
if [ -z "$SSID" ]; then
	fail "wpa_supplicant.conf缺少SSID"
fi

wpa_cli -i wlan0 scan >/dev/null 2>&1 || fail "扫描失败"
sleep 3
if ! wpa_cli -i wlan0 scan_results 2>/dev/null | awk -v s="$SSID" '$0 ~ s {f=1} END{exit !f}'; then
	fail "未扫描到SSID:$SSID"
fi

wpa_cli -i wlan0 reassociate >/dev/null 2>&1 || fail "关联失败"

udhcpc -i wlan0 -q -t 5 -T 3 -n >/dev/null 2>&1 || fail "DHCP失败"

printf "nameserver 223.5.5.5\nnameserver 180.76.76.76\n" > /etc/resolv.conf 2>/dev/null || fail "写入resolv.conf失败"

exit 0
