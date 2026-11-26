#!/bin/sh
# Independent LED exit supervision; never sends signals to any other service.
set -u
ROOT=${LED_SUPERVISOR_ROOT:-/run/led}
AGENT=${LED_AGENT_BIN:-/usr/bin/led_agent}
SELF=${LED_SUPERVISOR_BIN:-/usr/bin/led_supervisor.sh}
POLL=${LED_POLL_SECONDS:-1}
RETRY=${LED_RETRY_SECONDS:-5}
LOG=${LED_SUPERVISOR_LOG:-/var/log/led_supervisor.log}
PIDFILE=${LED_AGENT_PIDFILE:-/var/run/led_agent.pid}

identity() {
    case "$1" in ''|*[!0-9]*) return 1;; esac
    awk '{sub(/^.*\) /, ""); if ($1 != "Z" && $1 != "X") print $20}' "/proc/$1/stat" 2>/dev/null
}
now() { cut -d. -f1 /proc/uptime; }
log() {
    if [ -f "$LOG" ] && [ "$(wc -c < "$LOG")" -gt 262144 ]; then mv -f "$LOG" "$LOG.1"; fi
    printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG"
}
agent_pid() {
    local p exe
    for p in $(pidof "${AGENT##*/}" 2>/dev/null); do
        exe=$(readlink "/proc/$p/exe" 2>/dev/null)
        case "$exe" in "$AGENT"|"$AGENT (deleted)")
            [ -n "$(identity "$p")" ] && { echo "$p"; return 0; };;
        esac
    done
    return 1
}
supervisor_pid() {
    local p start
    [ -r "$ROOT/supervisor.pid" ] || return 1
    read -r p start < "$ROOT/supervisor.pid" || return 1
    [ -n "$start" ] && [ "$(identity "$p")" = "$start" ] || return 1
    tr '\0' '\n' < "/proc/$p/cmdline" | grep -Fxq "$SELF" || return 1
    echo "$p"
}
lease_active() {
    local p start until current
    [ -r "$ROOT/test.lease" ] || return 1
    read -r p start until < "$ROOT/test.lease" || return 1
    case "$p:$start:$until" in *[!0-9:]*|::*|*:) return 1;; esac
    current=$(now)
    if [ "$(identity "$p")" = "$start" ] && [ "$current" -lt "$until" ]; then return 0; fi
    rm -f "$ROOT/test.lease"
    log 'Test lease expired or owner exited; resuming LED supervision'
    return 1
}
control_lock() { exec 8>"$ROOT/supervisor.control"; flock -x 8; }
control_unlock() { flock -u 8; exec 8>&-; }

mkdir -p "$ROOT" || exit 1
case "${1:-}" in
run)
    exec 9>"$ROOT/supervisor.lock"
    flock -n 9 || exit 0
    printf '%s %s\n' "$$" "$(identity "$$")" > "$ROOT/supervisor.pid"
    trap 'exit 0' TERM INT HUP
    next=0
    log "Supervisor started pid=$$"
    while :; do
        control_lock
        if [ -e "$ROOT/supervisor.disabled" ]; then control_unlock; exit 0; fi
        if ! lease_active; then
            p=$(agent_pid || true)
            if [ -z "$p" ] && [ "$(now)" -ge "$next" ]; then
                if [ -x "$AGENT" ]; then
                    "$AGENT" </dev/null >/dev/null 2>&1 8>&- 9>&- &
                    p=$!
                    printf '%s\n' "$p" > "$ROOT/agent.pid"
                    log "LED launch pid=$p"
                else
                    log "LED executable missing: $AGENT"
                fi
                next=$(($(now) + RETRY))
            fi
            if [ -n "$p" ] && [ "$(cat "$PIDFILE" 2>/dev/null)" != "$p" ]; then
                printf '%s\n' "$p" > "$PIDFILE"
            fi
        fi
        control_unlock
        sleep "$POLL" & wait $! || true
    done
    ;;
start)
    control_lock
    rm -f "$ROOT/supervisor.disabled"
    if supervisor_pid >/dev/null; then control_unlock; echo 'LED supervisor already running'; exit 0; fi
    start-stop-daemon -S -q -b -x "$SELF" -- run
    rc=$?
    control_unlock
    [ "$rc" = 0 ] || exit "$rc"
    sleep 1
    supervisor_pid >/dev/null || { echo 'LED supervisor failed to start' >&2; exit 1; }
    echo 'LED supervisor started'
    ;;
stop)
    control_lock
    : > "$ROOT/supervisor.disabled"
    control_unlock
    p=$(supervisor_pid || true)
    [ -z "$p" ] || kill -TERM "$p" 2>/dev/null || true
    # Verify executable identity immediately before terminating LED only.
    p=$(agent_pid || true)
    [ -z "$p" ] || kill -TERM "$p" 2>/dev/null || true
    n=0
    while agent_pid >/dev/null || supervisor_pid >/dev/null; do
        n=$((n+1)); [ "$n" -lt 7 ] || { echo 'LED stop incomplete' >&2; exit 1; }
        sleep 1
    done
    rm -f "$PIDFILE"
    echo 'LED supervisor and agent stopped'
    ;;
pause)
    owner=${2:-}
    start=$(identity "$owner")
    [ -n "$start" ] || { echo 'Invalid test owner' >&2; exit 1; }
    control_lock
    if ! supervisor_pid >/dev/null || [ -e "$ROOT/supervisor.disabled" ]; then
        control_unlock; echo 'LED supervisor is not active' >&2; exit 1
    fi
    if lease_active; then control_unlock; echo 'Another test holds the lease' >&2; exit 1; fi
    printf '%s %s %s\n' "$owner" "$start" "$(($(now)+120))" > "$ROOT/test.lease.tmp"
    mv -f "$ROOT/test.lease.tmp" "$ROOT/test.lease"
    control_unlock
    echo 'LED auto-restart paused for this test owner (max 120s)'
    ;;
resume)
    owner=${2:-}; start=$(identity "$owner")
    control_lock
    if [ -r "$ROOT/test.lease" ]; then
        read -r p expected until < "$ROOT/test.lease"
        if [ "$p" != "$owner" ] || [ "$expected" != "$start" ]; then
            control_unlock; echo 'Lease belongs to another process' >&2; exit 1
        fi
        rm -f "$ROOT/test.lease"
    fi
    control_unlock
    echo 'LED auto-restart resumed'
    ;;
status)
    printf 'supervisor=%s led_agent=%s\n' "$(supervisor_pid || true)" "$(agent_pid || true)"
    [ ! -e "$ROOT/supervisor.disabled" ] || echo 'administratively stopped'
    [ ! -f "$ROOT/test.lease" ] || echo 'test lease present (validated by supervisor)'
    ;;
*) echo "Usage: $0 {start|stop|run|status|pause OWNER_PID|resume OWNER_PID}" >&2; exit 2;;
esac
