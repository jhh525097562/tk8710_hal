#!/bin/sh
# External J23 lamps: RUN=line23, NET=line24, high=ON.
# Use libgpiod v1 gpioset. No network, MQTT, TK8710 or watchdog changes.
set -eu
mode=${1:-all}
case "$mode" in run|net|all) ;; *) echo "Usage: sh $0 [run|net|all]" >&2; exit 2;; esac
DAEMON=/usr/bin/led_agent
original=$(pidof led_agent || true)
case "$original" in ''|*' '*) echo 'Need exactly one running led_agent' >&2; exit 1;; esac
[ "$(readlink /proc/$original/exe)" = "$DAEMON" ] || exit 1
command -v gpioset >/dev/null
command -v start-stop-daemon >/dev/null
# Share the lock with the earlier NET blink test.
mkdir /var/run/net-blink-test.lock || { echo 'Another LED test is running'; exit 1; }
restore=0
guarded=0
child=
cleanup() {
    result=$?
    trap - EXIT INT TERM HUP
    if [ -n "$child" ]; then
        kill -TERM "$child" 2>/dev/null || true
        wait "$child" 2>/dev/null || true
    fi
    if [ "$restore" = 1 ]; then
        if [ "$guarded" = 1 ]; then
            /usr/bin/led_supervisor.sh resume "$$" || result=1
            n=0
            while ! pidof led_agent >/dev/null 2>&1 && [ "$n" -lt 8 ]; do sleep 1; n=$((n+1)); done
        elif ! pidof led_agent >/dev/null 2>&1; then
            start-stop-daemon -S -q -m -b -p /var/run/led_agent.pid -x "$DAEMON" || result=1
            sleep 1
        fi
        if pidof led_agent >/dev/null 2>&1; then
            echo "RESTORED led_agent=$(pidof led_agent)"
        else
            echo 'ERROR: restore failed; run /etc/init.d/S37led_agent start' >&2
            result=1
        fi
    fi
    rmdir /var/run/net-blink-test.lock || true
    exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
hold() {
    gpioset -m time -s "$1" gpiochip1 "23=$2" "24=$3" &
    child=$!
    wait "$child"
    child=
}
echo 'Preparing: normal LED control will pause; external active-high lamps only.'
restore=1
if [ -x /usr/bin/led_supervisor.sh ]; then
    /usr/bin/led_supervisor.sh pause "$$"
    guarded=1
fi
kill -TERM "$original"
n=0
while [ -e "/proc/$original/exe" ]; do
    n=$((n+1)); [ "$n" -lt 6 ] || { echo 'led_agent did not exit'; exit 1; }
    sleep 1
done
echo 'BOTH external lamps OFF for 3s (D8 is opposite).'
hold 3 0 0
test_lamp() {
    name=$1
    run_value=$2
    net_value=$3
    echo "$name ONLY: steady ON for 10s. Identify its position and color."
    hold 10 "$run_value" "$net_value"
    echo "$name ONLY: 2s OFF / 2s ON, three cycles; the other external lamp stays OFF."
    i=0
    while [ "$i" -lt 3 ]; do
        echo "$name OFF"
        hold 2 0 0
        echo "$name ON"
        hold 2 "$run_value" "$net_value"
        i=$((i+1))
    done
    echo 'BOTH external lamps OFF for 3s.'
    hold 3 0 0
}
case "$mode" in run|all) test_lamp RUN 1 0;; esac
case "$mode" in net|all) test_lamp NET 0 1;; esac
echo 'DONE: restoring normal LED control.'
