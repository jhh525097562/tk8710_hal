#!/bin/sh
set -eu
cd "$(dirname "$0")"
child=
cleanup() {
    rc=$?
    trap - EXIT INT TERM HUP
    if [ -n "$child" ]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; fi
    /usr/bin/led_supervisor.sh resume "$$" || true
    exit "$rc"
}
/usr/bin/led_supervisor.sh pause "$$"
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
p=$(pidof led_agent)
case "$p" in ''|*' '*) exit 1;; esac
[ "$(readlink "/proc/$p/exe")" = /usr/bin/led_agent ] || exit 1
cp -p /usr/bin/led_agent led_agent.old
kill -TERM "$p"
n=0
while [ -e "/proc/$p/exe" ]; do n=$((n+1)); [ "$n" -lt 6 ] || exit 1; sleep 1; done
./led_pattern_test & child=$!
wait "$child"
child=
# Install only after every pattern read-back check has passed.
cp led_agent.new /usr/bin/led_agent.group-new
chmod 755 /usr/bin/led_agent.group-new
mv /usr/bin/led_agent.group-new /usr/bin/led_agent
echo INSTALLED
