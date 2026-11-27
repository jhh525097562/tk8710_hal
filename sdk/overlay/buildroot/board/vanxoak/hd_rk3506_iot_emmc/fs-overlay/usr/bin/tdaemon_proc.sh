#!/bin/sh

# The FRP watchdog stays alive and regenerates its config after the child exits.
MQTT_READY_FILE="${MQTT_READY_FILE:-/run/led/mqtt_status.json}"
MQTT_READY_TIMEOUT="${MQTT_READY_TIMEOUT:-15}"

proc_mqtt_ready() {
    local pid status
    pid=$(pidof mqtt_bridge 2>/dev/null | awk '{print $1}')
    [ -n "$pid" ] || return 1
    [ -r "$MQTT_READY_FILE" ] || return 1
    status=$(cat "$MQTT_READY_FILE" 2>/dev/null) || return 1
    echo "$status" | grep -Eq '"pid"[[:space:]]*:[[:space:]]*'"$pid"'([,}])' || return 1
    echo "$status" | grep -Eq '"ready"[[:space:]]*:[[:space:]]*true([,}])'
}

proc_wait_mqtt_ready() {
    local elapsed=0
    while [ "$elapsed" -lt "$MQTT_READY_TIMEOUT" ]; do
        proc_mqtt_ready && return 0
        sleep 1
        elapsed=$((elapsed + 1))
    done
    proc_mqtt_ready
}

proc_reload_frp_config() {
    local frpc_bin="${1:-/userdata/frp/frpc}"
    local pid exe
    for pid in $(pidof frpc 2>/dev/null || true); do
        exe=$(readlink "/proc/$pid/exe" 2>/dev/null || true)
        case "$exe" in
            "$frpc_bin"|"$frpc_bin (deleted)")
                log INFO "配置更新：通知FRP子进程退出并重新加载 pid=$pid"
                kill -TERM "$pid" 2>/dev/null || true
                ;;
        esac
    done
}

proc_cmdline_matches() {
    local expected="$1"
    local actual="$2"

    [ "$actual" = "$expected" ] && return 0
    case "$expected" in
        *.sh|/*/*.sh)
            echo " $actual " | grep -Fq " $expected " && return 0
            ;;
    esac
    return 1
}

proc_is_running() {
    local cmdline="$1"
    local name
    name=$(basename_simple "$cmdline")
    case "$name" in
        *.sh) ;;
        *)
            if pidof "$name" >/dev/null 2>&1; then
                return 0
            fi
            ;;
    esac
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
            if proc_cmdline_matches "$expected" "$pcmd"; then
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
        if proc_is_running "$cmdline"; then
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
    [ ! -e /var/run/app-upgrade-maintenance ] || return 0
    local cmdline
    while IFS= read -r cmdline; do
        [ ! -e /var/run/app-upgrade-maintenance ] || return 0
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

        if [ "$name" = "tk8710_gw" ] && ! proc_wait_mqtt_ready; then
            log WARN "启动阻断：MQTT未完成CONNACK/SUBACK，延后启动 $name"
            continue
        fi

        proc_restart_and_wait "$cmdline" || true
    done <<EOF
$ACTIVE_PROCESS_ENTRIES
EOF
}
