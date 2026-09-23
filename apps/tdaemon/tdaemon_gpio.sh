#!/bin/sh

gpio_sleep_ms() {
    local ms="$1"
    awk -v ms="$ms" 'BEGIN { printf "%.3f\n", ms / 1000 }'
}

gpio_inactive_value() {
    local active_value="$1"
    if [ "$active_value" = "1" ]; then
        echo "0"
    else
        echo "1"
    fi
}

gpio_tool_available() {
    command -v gpioset >/dev/null 2>&1
}

gpio_apply_value() {
    local chip="$1"
    local line="$2"
    local value="$3"
    if ! gpio_tool_available; then
        return 1
    fi
    gpioset -m exit "$chip" "$line=$value" >/dev/null 2>&1 && return 0
    gpioset "$chip" "$line=$value" >/dev/null 2>&1
}

gpio_pulse_line() {
    local iface="$1"
    local action="$2"
    local chip="$3"
    local line="$4"
    local active_value="$5"
    local pulse_ms="$6"
    local inactive_value
    [ -n "$chip" ] || return 1
    [ -n "$line" ] || return 1
    inactive_value=$(gpio_inactive_value "$active_value")
    log INFO "GPIO动作开始：iface=$iface action=$action chip=$chip line=$line pulse_ms=$pulse_ms"
    gpio_apply_value "$chip" "$line" "$active_value" || {
        log ERROR "GPIO动作失败：iface=$iface action=$action chip=$chip line=$line step=active"
        return 1
    }
    sleep "$(gpio_sleep_ms "$pulse_ms")"
    gpio_apply_value "$chip" "$line" "$inactive_value" || {
        log ERROR "GPIO动作失败：iface=$iface action=$action chip=$chip line=$line step=inactive"
        return 1
    }
    log INFO "GPIO动作完成：iface=$iface action=$action chip=$chip line=$line pulse_ms=$pulse_ms"
    return 0
}

gpio_modem_reset_pulse() {
    gpio_pulse_line \
        "usb0" \
        "modem_reset" \
        "$ACTIVE_NET_MODEM_RESET_CHIP" \
        "$ACTIVE_NET_MODEM_RESET_LINE" \
        "$ACTIVE_NET_MODEM_RESET_ACTIVE_VALUE" \
        "$ACTIVE_NET_MODEM_RESET_PULSE_MS"
}

gpio_modem_power_off() {
    local inactive_value
    [ -n "$ACTIVE_NET_MODEM_POWER_CHIP" ] || return 1
    [ -n "$ACTIVE_NET_MODEM_POWER_LINE" ] || return 1
    inactive_value=$(gpio_inactive_value "$ACTIVE_NET_MODEM_POWER_ACTIVE_VALUE")
    log INFO "GPIO动作开始：iface=usb0 action=modem_power_off chip=$ACTIVE_NET_MODEM_POWER_CHIP line=$ACTIVE_NET_MODEM_POWER_LINE hold_ms=$ACTIVE_NET_MODEM_POWER_OFF_HOLD_MS"
    gpio_apply_value "$ACTIVE_NET_MODEM_POWER_CHIP" "$ACTIVE_NET_MODEM_POWER_LINE" "$inactive_value" || {
        log ERROR "GPIO动作失败：iface=usb0 action=modem_power_off chip=$ACTIVE_NET_MODEM_POWER_CHIP line=$ACTIVE_NET_MODEM_POWER_LINE"
        return 1
    }
    sleep "$(gpio_sleep_ms "$ACTIVE_NET_MODEM_POWER_OFF_HOLD_MS")"
    log INFO "GPIO动作完成：iface=usb0 action=modem_power_off"
    return 0
}

gpio_modem_power_on() {
    [ -n "$ACTIVE_NET_MODEM_POWER_CHIP" ] || return 1
    [ -n "$ACTIVE_NET_MODEM_POWER_LINE" ] || return 1
    log INFO "GPIO动作开始：iface=usb0 action=modem_power_on chip=$ACTIVE_NET_MODEM_POWER_CHIP line=$ACTIVE_NET_MODEM_POWER_LINE wait_s=$ACTIVE_NET_MODEM_POWER_ON_WAIT_S"
    gpio_apply_value "$ACTIVE_NET_MODEM_POWER_CHIP" "$ACTIVE_NET_MODEM_POWER_LINE" "$ACTIVE_NET_MODEM_POWER_ACTIVE_VALUE" || {
        log ERROR "GPIO动作失败：iface=usb0 action=modem_power_on chip=$ACTIVE_NET_MODEM_POWER_CHIP line=$ACTIVE_NET_MODEM_POWER_LINE"
        return 1
    }
    sleep "$ACTIVE_NET_MODEM_POWER_ON_WAIT_S"
    log INFO "GPIO动作完成：iface=usb0 action=modem_power_on"
    return 0
}

gpio_modem_power_cycle() {
    gpio_modem_power_off || return 1
    gpio_modem_power_on || return 1
    sleep "$ACTIVE_NET_MODEM_ENUM_WAIT_S"
    log INFO "GPIO动作完成：iface=usb0 action=modem_power_cycle enum_wait_s=$ACTIVE_NET_MODEM_ENUM_WAIT_S"
    return 0
}
