#!/bin/sh
set -eu

FRP_DIR=${FRP_DIR:-/userdata/frp}
FRPC_BIN=${FRPC_BIN:-$FRP_DIR/frpc}
FRPC_CONFIG=${FRPC_CONFIG:-$FRP_DIR/frpc.toml}
FRPC_RUN_CONFIG=${FRPC_RUN_CONFIG:-$FRP_DIR/frpc.generated.toml}
FRPC_LOG=${FRPC_LOG:-/var/log/frpc.log}
GATEWAY_CONFIG=${GATEWAY_CONFIG:-/etc/config.json}
RESTART_DELAY=${RESTART_DELAY:-5}
LOCK_DIR=${LOCK_DIR:-/var/run/start_frpc.lock}
LOCK_ACQUIRED=0
FRPC_PID=

log_msg() {
    mkdir -p "$(dirname "$FRPC_LOG")"
    echo "$(date '+%Y-%m-%d %H:%M:%S') [start_frpc] $*" >>"$FRPC_LOG"
}

is_valid_device_id() {
    echo "$1" | grep -Eq '^[A-Za-z0-9_-]{16}$'
}

detect_device_id() {
    if [ -n "${DEVICE_ID:-}" ]; then
        echo "$DEVICE_ID"
        return
    fi

    [ -f "$GATEWAY_CONFIG" ] || return
    awk -F'"' '/"gateway_id"[[:space:]]*:/ {print $4; exit}' "$GATEWAY_CONFIG" 2>/dev/null
}

prepare_config() {
    if [ ! -x "$FRPC_BIN" ]; then
        log_msg "missing executable: $FRPC_BIN"
        return 1
    fi
    if [ ! -f "$FRPC_CONFIG" ]; then
        log_msg "missing config: $FRPC_CONFIG"
        return 1
    fi

    device_id_value=$(detect_device_id || true)
    if ! is_valid_device_id "$device_id_value"; then
        log_msg "invalid gateway_id; check $GATEWAY_CONFIG or set DEVICE_ID"
        return 1
    fi

    sed "s/DEVICE_ID_STRING/$device_id_value/g" "$FRPC_CONFIG" >"$FRPC_RUN_CONFIG"
    log_msg "prepared config=$FRPC_RUN_CONFIG device_id=$device_id_value"
    return 0
}

stop_child() {
    if [ -n "$FRPC_PID" ] && [ -d "/proc/$FRPC_PID" ]; then
        log_msg "stopping frpc pid=$FRPC_PID"
        kill "$FRPC_PID" 2>/dev/null || true
        wait "$FRPC_PID" 2>/dev/null || true
    fi
}

stop_existing_frpc() {
    for pid in $(pidof frpc 2>/dev/null || true); do
        exe=$(readlink "/proc/$pid/exe" 2>/dev/null || true)
        if [ "$exe" = "$FRPC_BIN" ]; then
            log_msg "stopping old frpc pid=$pid"
            kill "$pid" 2>/dev/null || true
        fi
    done
}

acquire_lock() {
    if mkdir "$LOCK_DIR" 2>/dev/null; then
        echo $$ >"$LOCK_DIR/pid"
        LOCK_ACQUIRED=1
        return 0
    fi

    old_pid=$(cat "$LOCK_DIR/pid" 2>/dev/null || true)
    if [ -n "$old_pid" ] && [ -d "/proc/$old_pid" ]; then
        log_msg "another start_frpc is running pid=$old_pid"
        exit 0
    fi

    rm -rf "$LOCK_DIR"
    mkdir "$LOCK_DIR"
    echo $$ >"$LOCK_DIR/pid"
    LOCK_ACQUIRED=1
}

cleanup() {
    stop_child
    if [ "$LOCK_ACQUIRED" -eq 1 ]; then
        rm -rf "$LOCK_DIR"
    fi
}

trap 'cleanup; exit 0' TERM INT
trap 'cleanup' EXIT

acquire_lock

log_msg "start_frpc watchdog started"

while true; do
    if prepare_config; then
        stop_existing_frpc
        "$FRPC_BIN" -c "$FRPC_RUN_CONFIG" >>"$FRPC_LOG" 2>&1 &
        FRPC_PID=$!
        log_msg "frpc started pid=$FRPC_PID"
        if wait "$FRPC_PID"; then
            exit_code=0
        else
            exit_code=$?
        fi
        log_msg "frpc exited status=$exit_code; restart after ${RESTART_DELAY}s"
        FRPC_PID=
    fi
    sleep "$RESTART_DELAY"
done
