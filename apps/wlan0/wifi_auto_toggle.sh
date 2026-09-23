#!/bin/sh
###
 # @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @Date: 2026-03-13 09:32:08
 # @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @LastEditTime: 2026-03-13 17:30:07
 # @FilePath: \wlan0\wifi_auto_toggle.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 
LOG=/var/log/wifi_auto.log
exec >>"$LOG" 2>&1

SWITCH_INTERVAL_SECONDS=120
readonly SWITCH_INTERVAL_SECONDS

AP_SCRIPT=/usr/bin/switch_to_ap.sh
STA_SCRIPT=/usr/bin/switch_to_sta.sh
readonly AP_SCRIPT
readonly STA_SCRIPT

LOCK=/var/run/wifi_auto.pid
if command -v flock >/dev/null 2>&1; then
	exec 200>"$LOCK" || exit 1
	if ! flock -n 200; then
		if [ -s "$LOCK" ]; then
			OLD_PID=$(cat "$LOCK" 2>/dev/null)
			if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
				echo "$(date '+%Y-%m-%d %H:%M:%S') 已有实例运行 pid=$OLD_PID" >&2
				exit 0
			fi
		fi
		rm -f "$LOCK" >/dev/null 2>&1 || true
		exec 200>"$LOCK" || exit 1
		flock -n 200 || exit 1
	fi
	echo $$ > "$LOCK"
else
	echo "$(date '+%Y-%m-%d %H:%M:%S') flock不存在，跳过锁" >&2
	echo $$ > "$LOCK" 2>/dev/null || true
fi

echo "$(date '+%Y-%m-%d %H:%M:%S') wifi_auto启动" >&2
if ! "${AP_SCRIPT}"; then
	echo "$(date '+%Y-%m-%d %H:%M:%S') AP启动失败" >&2
	exit 1
fi

AP_START=$(date +%s)

check_clients() {
	if command -v hostapd_cli >/dev/null 2>&1; then
		hostapd_cli -p /var/run/hostapd -i wlan0 list_sta 2>/dev/null | awk 'NF{c++} END{print c+0}'
		return
	fi
	if command -v iw >/dev/null 2>&1; then
		iw dev wlan0 station dump 2>/dev/null | awk '/^Station /{c++} END{print c+0}'
		return
	fi
	echo 1
}

while true; do
	sleep 60
	NOW=$(date +%s)
	ELAPSED=$((NOW - AP_START))
	if [ "$ELAPSED" -ge "$SWITCH_INTERVAL_SECONDS" ]; then
		CLIENTS=$(check_clients)
		if [ "$CLIENTS" -eq 0 ]; then
			START_TS=$(date +%s)
			if "${STA_SCRIPT}"; then
				END_TS=$(date +%s)
				echo "$(date '+%Y-%m-%d %H:%M:%S') AP->STA切换耗时$((END_TS - START_TS))s" >&2
				exit 0
			else
				sleep 3
				if "${STA_SCRIPT}"; then
					END_TS=$(date +%s)
					echo "$(date '+%Y-%m-%d %H:%M:%S') AP->STA切换耗时$((END_TS - START_TS))s" >&2
					exit 0
				else
					echo "$(date '+%Y-%m-%d %H:%M:%S') STA切换失败，回退AP" >&2
					if [ -f /var/log/wifi_sta.log ]; then
						echo "$(date '+%Y-%m-%d %H:%M:%S') STA日志:" >&2
						tail -n 50 /var/log/wifi_sta.log >&2
					fi
					"${AP_SCRIPT}" >/dev/null 2>&1 || true
					AP_START=$(date +%s)
				fi
			fi
		fi
	fi
done
