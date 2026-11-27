#!/bin/sh

recovery_iface_max_level() {
    case "$1" in
        eth0) echo 2 ;;
        wlan0) echo 3 ;;
        usb0) echo 4 ;;
        *) echo 0 ;;
    esac
}

recovery_iface_level_get() {
    local iface="$1"
    eval "echo \${recovery_level_${iface}:-0}"
}

recovery_iface_level_set() {
    local iface="$1"
    local level="$2"
    eval "recovery_level_${iface}=$level"
}

recovery_iface_attempt_count_get() {
    local iface="$1"
    eval "echo \${recovery_attempt_count_${iface}:-0}"
}

recovery_iface_attempt_count_set() {
    local iface="$1"
    local count="$2"
    eval "recovery_attempt_count_${iface}=$count"
}

recovery_iface_reset_state() {
    local iface="$1"
    local reason="$2"
    recovery_iface_attempt_count_set "$iface" 0
    recovery_iface_level_set "$iface" 0
    eval "recovery_last_reason_${iface}=\"\$reason\""
    log INFO "恢复状态重置：iface=$iface reason=$reason"
}

recovery_iface_mark_background() {
    local iface="$1"
    local reason="$2"
    [ -n "$iface" ] || return 0
    eval "recovery_background_ts_${iface}=\$(date +%s)"
    eval "recovery_background_reason_${iface}=\"\$reason\""
    log INFO "接口进入后台恢复窗口：iface=$iface reason=$reason"
}

recovery_iface_gpio_allowed() {
    local iface="$1"
    local now_ts last_ts cooldown
    cooldown="${ACTIVE_NET_RECOVERY_GPIO_COOLDOWN_S:-0}"
    [ "$cooldown" -le 0 ] && return 0
    now_ts=$(date +%s)
    eval "last_ts=\${recovery_last_gpio_ts_${iface}:-0}"
    [ -z "$last_ts" ] && last_ts=0
    if [ $((now_ts - last_ts)) -lt "$cooldown" ]; then
        log WARN "GPIO恢复冷却中：iface=$iface cooldown_s=$cooldown last_ts=$last_ts now_ts=$now_ts"
        return 1
    fi
    return 0
}

recovery_iface_mark_gpio_used() {
    local iface="$1"
    eval "recovery_last_gpio_ts_${iface}=\$(date +%s)"
}

recovery_validate_iface() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    local targets_sp
    targets_sp=$(echo "$targets" | tr ',' ' ')
    net_check_iface_qualified "$iface" "$targets_sp" "$count"
}

recovery_light_attempt() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    log INFO "开始轻量恢复：iface=$iface"
    if [ "$iface" = "wlan0" ]; then
        [ -f "$SCRIPT_DIR/switch_to_sta.sh" ] || return 1
        
        local sta_err
        sta_err=$(sh "$SCRIPT_DIR/switch_to_sta.sh" 2>&1 >/dev/null)
        if [ $? -ne 0 ]; then
            log WARN "wlan0 轻量恢复出错，原因：$sta_err"
            # 移除 return 1，由 net_wait_link_up 兜底
        fi
    else
        net_ifup_iface "$iface" || return 1
    fi
    net_wait_link_up "$iface" "$NET_IFACE_LINK_WAIT_TIMEOUT" || return 1
    recovery_validate_iface "$iface" "$targets" "$count"
}

recovery_soft_attempt() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    log INFO "开始软件重置恢复：iface=$iface"
    net_release_iface "$iface" || true
    recovery_light_attempt "$iface" "$targets" "$count"
}

recovery_gpio_reset_attempt() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    [ "$iface" = "usb0" ] || return 1
    recovery_iface_gpio_allowed "$iface" || return 1
    gpio_modem_reset_pulse || return 1
    recovery_iface_mark_gpio_used "$iface"
    recovery_light_attempt "$iface" "$targets" "$count"
}

recovery_gpio_power_cycle_attempt() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    [ "$iface" = "usb0" ] || return 1
    recovery_iface_gpio_allowed "$iface" || return 1
    gpio_modem_power_cycle || return 1
    recovery_iface_mark_gpio_used "$iface"
    recovery_light_attempt "$iface" "$targets" "$count"
}

recovery_attempt_iface() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    local trigger_reason="$4"
    local current_count next_count light_threshold soft_threshold next_level max_level rc extra_count

    max_level=$(recovery_iface_max_level "$iface")
    [ "$max_level" -gt 0 ] || return 1

    light_threshold="${ACTIVE_NET_RECOVERY_LIGHT_THRESHOLD:-1}"
    soft_threshold="${ACTIVE_NET_RECOVERY_SOFT_THRESHOLD:-2}"
    [ "$soft_threshold" -lt "$light_threshold" ] && soft_threshold="$light_threshold"

    current_count=$(recovery_iface_attempt_count_get "$iface")
    next_count=$((current_count + 1))
    if [ "$next_count" -le "$light_threshold" ]; then
        next_level=1
    elif [ "$next_count" -le "$soft_threshold" ]; then
        next_level=2
    else
        extra_count=$((next_count - soft_threshold))
        if [ "$max_level" -le 2 ]; then
            next_level=2
        elif [ "$max_level" -eq 3 ]; then
            next_level=3
        else
            if [ "$extra_count" -le 1 ]; then
                next_level=3
            else
                next_level=4
            fi
        fi
    fi

    if [ "$next_level" -gt "$max_level" ]; then
        log WARN "恢复层级已耗尽：iface=$iface next_level=$next_level max_level=$max_level reason=$trigger_reason"
        return 1
    fi

    recovery_iface_attempt_count_set "$iface" "$next_count"
    recovery_iface_level_set "$iface" "$next_level"
    log WARN "开始接口恢复：iface=$iface level=$next_level attempt=$next_count reason=$trigger_reason"

    case "$next_level" in
        1) recovery_light_attempt "$iface" "$targets" "$count"; rc=$? ;;
        2) recovery_soft_attempt "$iface" "$targets" "$count"; rc=$? ;;
        3) recovery_gpio_reset_attempt "$iface" "$targets" "$count"; rc=$? ;;
        4) recovery_gpio_power_cycle_attempt "$iface" "$targets" "$count"; rc=$? ;;
        *) rc=1 ;;
    esac

    if [ $rc -eq 0 ]; then
        log INFO "接口恢复成功：iface=$iface level=$next_level attempt=$next_count reason=$trigger_reason"
        recovery_iface_reset_state "$iface" "恢复成功"
        return 0
    fi

    log WARN "接口恢复失败：iface=$iface level=$next_level attempt=$next_count reason=$trigger_reason"
    return 1
}
