#!/bin/sh
# J23 NET test only: gpiochip1 line24, high=external ON, low=D8 ON.
set -eu
DAEMON=/usr/bin/led_agent
original=$(pidof led_agent || true)
case "$original" in ''|*' '*) echo 'Need exactly one running led_agent' >&2; exit 1;; esac
[ "$(readlink /proc/$original/exe)" = "$DAEMON" ] || exit 1
command -v gpioset >/dev/null
mkdir /var/run/net-blink-test.lock || exit 1
restore=0
guarded=0
cleanup() {
    trap - EXIT INT TERM HUP
    if [ "$guarded" = 1 ]; then
        /usr/bin/led_supervisor.sh resume "$$"
        n=0
        while ! pidof led_agent >/dev/null 2>&1 && [ "$n" -lt 8 ]; do sleep 1; n=$((n+1)); done
    elif [ "$restore" = 1 ] && ! pidof led_agent >/dev/null 2>&1; then
        start-stop-daemon -S -q -m -b -p /var/run/led_agent.pid -x "$DAEMON"
        sleep 1
    fi
    rmdir /var/run/net-blink-test.lock
    echo "RESTORED led_agent=$(pidof led_agent || true)"
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
echo 'PREPARE: testing begins in 5 seconds'
sleep 5
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
echo 'SLOW: 2 seconds ON / 2 seconds OFF, 6 cycles (24s)'
i=0
while [ "$i" -lt 6 ]; do
    echo "SLOW cycle=$i HIGH external=ON D8=OFF"
    gpioset -m time -s 2 gpiochip1 24=1
    echo "SLOW cycle=$i LOW external=OFF D8=ON"
    gpioset -m time -s 2 gpiochip1 24=0
    i=$((i+1))
done
echo 'GAP: external OFF / D8 ON for 5 seconds'
gpioset -m time -s 5 gpiochip1 24=0
echo 'FAST: 200ms ON / 200ms OFF, 50 cycles (about 20s)'
i=0
while [ "$i" -lt 50 ]; do
    gpioset -m time -u 200000 gpiochip1 24=1
    gpioset -m time -u 200000 gpiochip1 24=0
    i=$((i+1))
done
echo 'DONE: resuming normal LED control'
