#!/bin/sh

proc_is_running() {
    local cmdline="$1"
    local name
    name=$(basename_simple "$cmdline")
    if pidof "$name" >/dev/null 2>&1; then
        return 0
    fi
    if [ -d "/proc" ]; then
        local expected
        expected=$(echo "$cmdline" | tr -s ' ')
        local p
        for p in /proc/[0-9]*; do
            [ -r "$p/cmdline" ] || continue
            local pcmd
            pcmd=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null | tr -s ' ')
            pcmd=$(trim "$pcmd")
            [ -z "$pcmd" ] && continue
            if [ "$pcmd" = "$expected" ]; then
                return 0
            fi
        done
    fi
    return 1
}

proc_restart_and_wait() {
    local cmdline="$1"
    local name
    name=$(basename_simple "$cmdline")

    log WARN "进程异常：$name 不在运行，触发重启"
    set -f
    set -- $cmdline
    set +f
    "$@" >/dev/null 2>&1 &
    local t=0
    while [ $t -lt "$ACTIVE_RESTART_TIMEOUT" ]; do
        if pidof "$name" >/dev/null 2>&1; then
            log INFO "进程重启成功：$name"
            return 0
        fi
        sleep 1
        t=$((t + 1))
    done
    log ERROR "进程重启失败：$name (超时${ACTIVE_RESTART_TIMEOUT}s)"
    return 1
}

proc_tick() {
    local cmdline
    while IFS= read -r cmdline; do
        [ -z "$cmdline" ] && continue
        local name
        name=$(basename_simple "$cmdline")

        local gated=0
        case "$name" in
            mqtt_bridge|tk8710_gw) gated=1 ;;
        esac

        if proc_is_running "$cmdline"; then
            continue
        fi

        if [ $gated -eq 1 ] && [ "$NETWORK_READY" -ne 1 ]; then
            log ERROR "启动阻断：网络未就绪，禁止启动 $name"
            continue
        fi

        proc_restart_and_wait "$cmdline" || true
    done <<EOF
$ACTIVE_PROCESS_ENTRIES
EOF
}
