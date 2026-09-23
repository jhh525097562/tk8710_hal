#!/bin/sh

net_priority_to_list() {
    echo "eth0 usb0 wlan0"
}

net_disabled_to_set() {
    echo "$1" | tr ',' ' ' | tr -s ' '
}

net_led_write_state() {
    local status="$1"
    local led_dir="/run/led"
    local state_file="$led_dir/net.state"
    local tmp_file="$led_dir/net.tmp"
    local ts_s ts_ms

    [ -n "$status" ] || return 0

    mkdir -p "$led_dir" 2>/dev/null || {
        log WARN "NET灯状态写入失败：无法创建目录 $led_dir"
        return 1
    }
    ts_s=$(date +%s 2>/dev/null)
    [ -n "$ts_s" ] || ts_s=0
    ts_ms=$((ts_s * 1000))

    {
        echo "STATUS=$status"
        echo "TS_MS=$ts_ms"
    } > "$tmp_file" 2>/dev/null || {
        log WARN "NET灯状态写入失败：无法写临时文件 $tmp_file"
        return 1
    }

    mv "$tmp_file" "$state_file" 2>/dev/null || {
        rm -f "$tmp_file" 2>/dev/null || true
        log WARN "NET灯状态写入失败：无法替换状态文件 $state_file"
        return 1
    }
    return 0
}

net_metric_priority_offset() {
    case "$1" in
        eth0) echo 0 ;;
        usb0) echo 10 ;;
        wlan0) echo 20 ;;
        *) echo 30 ;;
    esac
}

net_metric_standby_value() {
    local iface="$1"
    local offset
    offset=$(net_metric_priority_offset "$iface")
    echo $((ACTIVE_NET_ROUTE_METRIC + offset + 10))
}

net_route_has_default() {
    local iface="$1"
    ip route show default dev "$iface" 2>/dev/null | grep -q "^default"
}

net_route_get_default_metric() {
    local iface="$1"
    ip route show default dev "$iface" 2>/dev/null | awk '
        {
            for (i = 1; i <= NF; i++) {
                if ($i == "metric") {
                    print $(i + 1)
                    found = 1
                    exit
                }
            }
        }
        END {
            if (NR > 0 && !found) {
                print 0
            }
        }
    ' | head -n1
}

net_route_get_default_gw() {
    local iface="$1"
    local gw
    gw=$(ip route show default dev "$iface" 2>/dev/null | awk '
        {
            for (i = 1; i <= NF; i++) {
                if ($i == "via") {
                    print $(i + 1)
                    exit
                }
            }
        }
    ' | head -n1)
    if [ -n "$gw" ]; then
        echo "$gw"
        return 0
    fi
    ip route show dev "$iface" 2>/dev/null | awk '
        {
            for (i = 1; i <= NF; i++) {
                if ($i == "via") {
                    print $(i + 1)
                    exit
                }
            }
        }
    ' | head -n1
}

net_route_get_highest_metric_except() {
    local skip_iface="$1"
    ip route show default 2>/dev/null | awk -v skip_iface="$skip_iface" '
        BEGIN {
            max_metric = ""
        }
        /^default/ {
            dev = ""
            metric = 0
            for (i = 1; i <= NF; i++) {
                if ($i == "dev" && (i + 1) <= NF) {
                    dev = $(i + 1)
                }
                if ($i == "metric" && (i + 1) <= NF) {
                    metric = $(i + 1)
                }
            }
            if (dev != "" && dev != skip_iface) {
                if (max_metric == "" || metric > max_metric) {
                    max_metric = metric
                }
            }
        }
        END {
            if (max_metric != "") {
                print max_metric
            }
        }
    ' | head -n1
}

net_route_with_lock() {
    local lock_file="/var/run/tdaemon_route.lock"
    local lock_dir="${lock_file}.dir"
    local rc
    mkdir -p "$(dirname "$lock_file")" 2>/dev/null || true
    if command -v flock >/dev/null 2>&1; then
        exec 9>"$lock_file" || return 1
        flock -x 9 || {
            exec 9>&-
            return 1
        }
        "$@"
        rc=$?
        flock -u 9 >/dev/null 2>&1 || true
        exec 9>&-
        return $rc
    fi
    lock_acquire "$lock_dir" || return 1
    "$@"
    rc=$?
    lock_release "$lock_dir"
    return $rc
}

net_route_replace_default_core() {
    local iface="$1"
    local metric="$2"
    local gw
    gw=$(net_route_get_default_gw "$iface")
    while net_route_has_default "$iface"; do
        ip route del default dev "$iface" >/dev/null 2>&1 || break
    done
    if [ -n "$gw" ]; then
        ip route add default via "$gw" dev "$iface" metric "$metric" >/dev/null 2>&1
        return $?
    fi
    ip route add default dev "$iface" metric "$metric" >/dev/null 2>&1
}

net_route_update_metric() {
    local iface="$1"
    local new_metric="$2"
    local reason="$3"
    local old_metric
    if [ ! -d "/sys/class/net/$iface" ]; then
        log WARN "Metric调整跳过：iface=$iface reason=$reason cause=interface_absent"
        return 1
    fi
    if ! net_route_has_default "$iface"; then
        log WARN "Metric调整跳过：iface=$iface reason=$reason cause=no_default_route"
        return 1
    fi
    old_metric=$(net_route_get_default_metric "$iface")
    [ -z "$old_metric" ] && old_metric="0"
    if [ "$old_metric" = "$new_metric" ]; then
        log INFO "Metric保持不变：iface=$iface reason=$reason old_metric=$old_metric new_metric=$new_metric"
        return 0
    fi
    if net_route_with_lock net_route_replace_default_core "$iface" "$new_metric"; then
        log INFO "Metric调整完成：iface=$iface reason=$reason old_metric=$old_metric new_metric=$new_metric"
        return 0
    fi
    log ERROR "Metric调整失败：iface=$iface reason=$reason old_metric=$old_metric new_metric=$new_metric"
    return 1
}

net_route_demote_iface() {
    local iface="$1"
    local reason="$2"
    local ref_metric
    local new_metric
    ref_metric=$(net_route_get_highest_metric_except "$iface")
    if [ -z "$ref_metric" ]; then
        ref_metric="$ACTIVE_NET_ROUTE_METRIC"
    fi
    new_metric=$((ref_metric + 10))
    net_route_update_metric "$iface" "$new_metric" "$reason"
}

net_route_apply_standby_metrics() {
    local active_iface="$1"
    local reason="$2"
    local iface
    local standby_metric
    for iface in $(net_priority_to_list); do
        [ "$iface" = "$active_iface" ] && continue
        if ! net_route_has_default "$iface"; then
            continue
        fi
        if net_is_disabled "$iface"; then
            net_route_demote_iface "$iface" "$reason-接口禁用保持低优先级" || true
            continue
        fi
        standby_metric=$(net_metric_standby_value "$iface")
        net_route_update_metric "$iface" "$standby_metric" "$reason" || true
    done
}

net_route_activate_iface() {
    local iface="$1"
    local reason="$2"
    net_route_update_metric "$iface" "$ACTIVE_NET_ROUTE_METRIC" "$reason" || return 1
    net_route_apply_standby_metrics "$iface" "$reason" || true
    return 0
}

net_run_with_timeout() {
    local timeout_s="$1"
    shift
    "$@" >/dev/null 2>&1 &
    local cmd_pid=$!
    (
        sleep "$timeout_s"
        if kill -0 "$cmd_pid" 2>/dev/null; then
            kill -TERM "$cmd_pid" 2>/dev/null || true
            sleep "$NET_FORCE_STOP_WAIT"
            kill -KILL "$cmd_pid" 2>/dev/null || true
        fi
    ) &
    local guard_pid=$!

    wait "$cmd_pid"
    local rc=$?

    kill -TERM "$guard_pid" 2>/dev/null || true
    wait "$guard_pid" 2>/dev/null || true
    return $rc
}

net_iface_has_ipv4() {
    local iface="$1"
    ip -4 addr show dev "$iface" 2>/dev/null | grep -q "inet "
}

net_iface_has_dhcp_client() {
    local iface="$1"
    local p
    for p in /proc/[0-9]*; do
        [ -r "$p/cmdline" ] || continue
        local c
        c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
        echo "$c" | grep -Eq "(udhcpc|dhclient|dhcpcd)" || continue
        echo "$c" | grep -Eq "(^|[[:space:]])$iface([[:space:]]|$)|-i[[:space:]]*$iface|-i$iface" && return 0
    done
    return 1
}

net_iface_link_up() {
    local iface="$1"
    if [ ! -d "/sys/class/net/$iface" ]; then
        return 1
    fi

    if [ -r "/sys/class/net/$iface/carrier" ]; then
        [ "$(cat "/sys/class/net/$iface/carrier" 2>/dev/null)" = "1" ] && return 0
    fi

    if [ -r "/sys/class/net/$iface/operstate" ]; then
        local st
        st=$(cat "/sys/class/net/$iface/operstate" 2>/dev/null)
        case "$st" in
            up|unknown) return 0 ;;
        esac
    fi

    ip link show "$iface" 2>/dev/null | grep -qi "state UP"
}

net_wait_link_up() {
    local iface="$1"
    local timeout_s="$2"
    local t=0
    while [ "$t" -lt "$timeout_s" ]; do
        if net_iface_link_up "$iface"; then
            return 0
        fi
        sleep 1
        t=$((t + 1))
    done
    return 1
}

net_check_dhcp_ready() {
    local iface="$1"
    case "$iface" in
        usb0) net_iface_has_ipv4 "$iface"; return $? ;;
    esac

    net_iface_has_ipv4 "$iface" || return 1
    ip route show dev "$iface" 2>/dev/null | grep -q "proto dhcp" && return 0
    ip route show default dev "$iface" 2>/dev/null | grep -q "^default" && return 0
    return 1
}

net_stop_dhcp_for_iface() {
    local iface="$1"
    local p
    for p in /proc/[0-9]*; do
        [ -r "$p/cmdline" ] || continue
        local c pid
        c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
        echo "$c" | grep -Eq "(udhcpc|dhclient|dhcpcd)" || continue
        echo "$c" | grep -Eq "(^|[[:space:]])$iface([[:space:]]|$)|-i[[:space:]]*$iface|-i$iface" || continue
        pid=${p##*/}
        kill -TERM "$pid" 2>/dev/null || true
        sleep 1
        kill -KILL "$pid" 2>/dev/null || true
    done
}

net_stop_wpasupplicant_for_iface() {
    local iface="$1"
    if command -v wpa_cli >/dev/null 2>&1; then
        wpa_cli -i "$iface" terminate >/dev/null 2>&1 || true
    fi
    local p
    for p in /proc/[0-9]*; do
        [ -r "$p/cmdline" ] || continue
        local c pid
        c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
        echo "$c" | grep -q "wpa_supplicant" || continue
        echo "$c" | grep -Eq "(^|[[:space:]])$iface([[:space:]]|$)|-i[[:space:]]*$iface|-i$iface" || continue
        pid=${p##*/}
        kill -TERM "$pid" 2>/dev/null || true
        sleep 1
        kill -KILL "$pid" 2>/dev/null || true
    done
}

net_ifup_iface() {
    local iface="$1"
    if command -v ifup >/dev/null 2>&1; then
        if [ "$iface" = "eth0" ]; then
            net_run_with_timeout "$NET_IFACE_OP_TIMEOUT" ifup eth0
            return $?
        fi
        net_run_with_timeout "$NET_IFACE_OP_TIMEOUT" ifup "$iface"
        return $?
    fi
    ip link set "$iface" up >/dev/null 2>&1
}

net_ifdown_iface() {
    local iface="$1"
    if [ "$iface" = "wlan0" ]; then
        net_stop_dhcp_for_iface "$iface"
        net_stop_wpasupplicant_for_iface "$iface"
        return 0
    fi
    if command -v ifdown >/dev/null 2>&1; then
        if [ "$iface" = "eth0" ]; then
            net_run_with_timeout "$NET_IFACE_OP_TIMEOUT" ifdown eth0
            return $?
        fi
        net_run_with_timeout "$NET_IFACE_OP_TIMEOUT" ifdown "$iface"
        return $?
    fi
    ip link set "$iface" down >/dev/null 2>&1
}

net_release_iface() {
    local iface="$1"
    local failed=0

    net_ifdown_iface "$iface"
    if [ $? -ne 0 ]; then
        failed=1
        log WARN "接口关闭超时或失败：$iface，执行强制释放"
    fi

    net_stop_dhcp_for_iface "$iface"
    if [ "$iface" = "wlan0" ]; then
        net_stop_wpasupplicant_for_iface "$iface"
    fi

    ip route del default dev "$iface" >/dev/null 2>&1 || true
    ip route flush dev "$iface" >/dev/null 2>&1 || true
    ip addr flush dev "$iface" >/dev/null 2>&1 || true
    ip link set "$iface" down >/dev/null 2>&1 || failed=1

    if [ $failed -ne 0 ]; then
        log WARN "接口释放结果：$iface 存在错误，已执行强制关闭流程"
        return 1
    fi
    log INFO "接口释放完成：$iface"
    return 0
}

net_check_iface_qualified() {
    local iface="$1"
    local targets="$2"
    local count="$3"

    if ! net_iface_link_up "$iface"; then
        log WARN "达标检测失败：$iface 物理链路未UP"
        return 1
    fi
    if ! net_check_dhcp_ready "$iface"; then
        log WARN "达标检测失败：$iface DHCP/IP状态不达标"
        return 1
    fi
    if ! net_probe_quality "$iface" "$targets" "$count"; then
        log WARN "达标检测失败：$iface 延迟/丢包不达标"
        return 1
    fi
    log INFO "达标检测通过：$iface"
    return 0
}

wlan_ap_has_clients() {
    if ! command -v hostapd_cli >/dev/null 2>&1; then
        return 1
    fi
    hostapd_cli -i wlan0 list_sta 2>/dev/null | grep -Eq '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}'
}

wlan_ap_auto_stop_worker() {
    if command -v timeout >/dev/null 2>&1; then
        timeout "$ACTIVE_AP_MAX_UP_TIME" sh -c "sleep $ACTIVE_AP_MAX_UP_TIME" >/dev/null 2>&1 || true
    else
        sleep "$ACTIVE_AP_MAX_UP_TIME"
    fi

    while net_wlan0_is_ap_mode; do
        if wlan_ap_has_clients; then
            log INFO "AP延时关闭：检测到客户端在线，延后关闭"
            sleep "$WLAN_AP_CLIENT_POLL_INTERVAL"
            continue
        fi
        if [ -f "$SCRIPT_DIR/switch_to_ap.sh" ]; then
            log INFO "AP关闭：执行 $SCRIPT_DIR/switch_to_ap.sh stop"
            sh "$SCRIPT_DIR/switch_to_ap.sh" stop >/dev/null 2>&1 || true
        else
            log WARN "AP关闭：未找到 $SCRIPT_DIR/switch_to_ap.sh"
        fi
        if net_wlan0_is_ap_mode && [ -f "$SCRIPT_DIR/switch_to_sta.sh" ]; then
            log INFO "AP关闭回退：执行 $SCRIPT_DIR/switch_to_sta.sh"
            sh "$SCRIPT_DIR/switch_to_sta.sh" >/dev/null 2>&1 || true
        fi
        if net_wlan0_is_ap_mode; then
            log ERROR "AP延时关闭失败：超时后仍处于AP模式"
        else
            log INFO "AP延时关闭：无客户端，已关闭AP"
        fi
        break
    done
}

wlan_bootstrap_ap() {
    if [ ! -d "/sys/class/net/wlan0" ]; then
        return 0
    fi
    if [ ! -f "$SCRIPT_DIR/switch_to_ap.sh" ]; then
        log ERROR "wlan0 AP初始化失败：未找到 $SCRIPT_DIR/switch_to_ap.sh"
        return 1
    fi

    log INFO "AP开启：执行 $SCRIPT_DIR/switch_to_ap.sh"
    sh "$SCRIPT_DIR/switch_to_ap.sh" >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        log ERROR "wlan0 AP初始化失败：switch_to_ap.sh 执行失败"
        return 1
    fi
    log INFO "AP开启成功：wlan0 AP初始化成功"
    wlan_ap_auto_stop_worker &
    return 0
}

net_prepare_candidate_iface() {
    local iface="$1"
    local targets="$2"
    local count="$3"

    if [ "$iface" = "wlan0" ]; then
        if [ ! -f "$SCRIPT_DIR/switch_to_sta.sh" ]; then
            log ERROR "候选准备失败：未找到 $SCRIPT_DIR/switch_to_sta.sh"
            return 1
        fi
        sh "$SCRIPT_DIR/switch_to_sta.sh" >/dev/null 2>&1
        if [ $? -ne 0 ]; then
            log WARN "候选准备失败：wlan0 切换STA模式失败"
            return 1
        fi
    else
        net_ifup_iface "$iface"
        if [ $? -ne 0 ]; then
            log WARN "候选准备失败：ifup $iface 失败"
            return 1
        fi
    fi

    if ! net_wait_link_up "$iface" "$NET_IFACE_LINK_WAIT_TIMEOUT"; then
        log WARN "候选准备失败：$iface 链路未在超时内UP"
        return 1
    fi

    if ! net_check_iface_qualified "$iface" "$targets" "$count"; then
        return 1
    fi
    return 0
}

net_reboot_due_to_no_network() {
    log ERROR "网络连续不可用超过${ACTIVE_NO_READY_REBOOT_TIMEOUT}s，执行设备重启"
    sync >/dev/null 2>&1 || true
    if command -v reboot >/dev/null 2>&1; then
        reboot -f >/dev/null 2>&1 || reboot >/dev/null 2>&1 || true
        return 0
    fi
    if [ -x /sbin/reboot ]; then
        /sbin/reboot -f >/dev/null 2>&1 || /sbin/reboot >/dev/null 2>&1 || true
        return 0
    fi
    if command -v busybox >/dev/null 2>&1; then
        busybox reboot -f >/dev/null 2>&1 || busybox reboot >/dev/null 2>&1 || true
        return 0
    fi
    return 1
}

net_wlan0_is_ap_mode() {
    if [ ! -d "/sys/class/net/wlan0" ]; then
        return 1
    fi
    if command -v hostapd_cli >/dev/null 2>&1; then
        hostapd_cli -i wlan0 status 2>/dev/null | grep -q "^state=" && return 0
    fi
    if pidof hostapd >/dev/null 2>&1; then
        local p
        for p in /proc/[0-9]*; do
            [ -r "$p/cmdline" ] || continue
            local c
            c=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)
            echo "$c" | grep -q "hostapd" || continue
            echo "$c" | grep -q "wlan0" && return 0
        done
    fi
    return 1
}

net_is_disabled() {
    local iface="$1"
    if [ "$iface" = "wlan0" ] && net_wlan0_is_ap_mode; then
        return 0
    fi
    local dis
    for dis in $(net_disabled_to_set "$ACTIVE_NET_DISABLED"); do
        [ "$iface" = "$dis" ] && return 0
    done
    return 1
}

net_detect_presence() {
    local iface="$1"
    if ip link show "$iface" >/dev/null 2>&1; then
        if ip link show "$iface" | grep -qi "state UP"; then
            return 0
        fi
        return 2
    fi
    return 3
}

ping_parse_loss_pct() {
    local out="$1"
    echo "$out" | grep -Eo "[0-9]+% packet loss" | head -n1 | grep -Eo "^[0-9]+" | head -n1
}

ping_parse_avg_ms() {
    local out="$1"
    local rtt
    rtt=$(echo "$out" | grep -E "rtt min/avg/max/mdev|round-trip min/avg/max" | tail -n1)
    if echo "$rtt" | grep -q "mdev"; then
        echo "$rtt" | awk -F'[=/ ]+' '{print $(NF-3)}'
    else
        echo "$rtt" | awk -F'[=/ ]+' '{print $(NF-1)}'
    fi
}

ping_parse_jitter_ms() {
    local out="$1"
    local rtt
    rtt=$(echo "$out" | grep -E "rtt min/avg/max/mdev|round-trip min/avg/max" | tail -n1)
    if echo "$rtt" | grep -q "mdev"; then
        echo "$rtt" | awk -F'[=/ ]+' '{print $(NF)}'
    else
        local avg max
        avg=$(echo "$rtt" | awk -F'[=/ ]+' '{print $(NF-1)}')
        max=$(echo "$rtt" | awk -F'[=/ ]+' '{print $(NF)}')
        echo "$max $avg" | awk '{printf("%.3f", $1-$2)}'
    fi
}

net_probe_quality() {
    local iface="$1"
    local targets="$2"
    local count="$3"
    local ok=1

    local t
    for t in $targets; do
        local out rc loss avg jitter
        out=$(ping -I "$iface" -c "$count" -W 1 "$t" 2>&1)
        rc=$?
        if [ $rc -ne 0 ]; then
            log WARN "L3探测失败：$iface -> $t (ping rc=$rc)"
            continue
        fi
        loss=$(ping_parse_loss_pct "$out")
        [ -z "$loss" ] && loss=100
        avg=$(ping_parse_avg_ms "$out")
        [ -z "$avg" ] && avg=99999
        jitter=$(ping_parse_jitter_ms "$out")
        [ -z "$jitter" ] && jitter=0
        log INFO "L3探测：$iface -> $t 丢包:${loss}% 平均延迟:${avg}ms 抖动:${jitter}ms"
        local avg_i=${avg%.*}
        if [ "$loss" -le "$ACTIVE_NET_LOSS_THRESHOLD" ] && [ "$avg_i" -le "$ACTIVE_NET_LATENCY_THRESHOLD_MS" ]; then
            ok=0
            break
        fi
    done
    return $ok
}

net_switch_to() {
    local iface="$1"
    local reason="${2:-切换活跃链路}"
    local previous_iface="$CURRENT_IFACE"
    if ! net_route_activate_iface "$iface" "$reason"; then
        log ERROR "切换失败：写默认路由Metric失败 iface=$iface"
        return 1
    fi
    dns_ensure_config || return 1
    CURRENT_IFACE="$iface"
    echo "$iface" > /var/run/tdaemon_job.state
    recovery_iface_reset_state "$iface" "切换成为活跃链路" || true
    if [ -n "$previous_iface" ] && [ "$previous_iface" != "$iface" ]; then
        recovery_iface_mark_background "$previous_iface" "切换到$iface"
    fi
    log WARN "切换完成：当前活跃网络=$CURRENT_IFACE"
    net_led_write_state "L3_OK" || true
    return 0
}

net_select_best_iface() {
    local ifaces
    ifaces=$(net_priority_to_list)
    local targets
    targets=$(echo "$ACTIVE_NET_TARGETS" | tr ',' ' ')

    local iface
    for iface in $ifaces; do
        if net_is_disabled "$iface"; then
            log INFO "候选跳过：$iface 被禁用"
            continue
        fi

        net_detect_presence "$iface"
        local prc=$?
        if [ $prc -ne 0 ]; then
            case "$prc" in
                3)
                    log INFO "候选跳过：$iface 不存在"
                    continue
                    ;;
                2) log INFO "候选检测：$iface 存在但未UP，进入预启用流程" ;;
                *) log INFO "候选检测：$iface 状态未知，尝试预启用流程" ;;
            esac
        fi

        if net_prepare_candidate_iface "$iface" "$targets" "$ACTIVE_NET_PING_COUNT"; then
            echo "$iface"
            return 0
        fi
        log INFO "候选不可用：$iface 预启用或达标检测失败"
    done
    echo ""
    return 1
}

net_tick() {
    local targets
    targets=$(echo "$ACTIVE_NET_TARGETS" | tr ',' ' ')

    if [ -z "$CURRENT_IFACE" ]; then
        local best
        best=$(net_select_best_iface)
        if [ -n "$best" ]; then
            net_switch_to "$best"
        else
            NETWORK_READY=0
            log ERROR "网络闸门关闭：无可用链路"
            net_led_write_state "L3_DOWN" || true
        fi
        return 0
    fi

    if net_is_disabled "$CURRENT_IFACE"; then
        log WARN "当前接口被禁用，触发切换：$CURRENT_IFACE"
        net_led_write_state "RECOVERING" || true
        net_route_demote_iface "$CURRENT_IFACE" "接口被禁用触发降级" || true
        CURRENT_IFACE=""
        NETWORK_READY=0
        local best
        best=$(net_select_best_iface)
        if [ -n "$best" ]; then
            net_switch_to "$best" "禁用接口后切换"
        else
            net_led_write_state "L3_DOWN" || true
        fi
        return 0
    fi

    if ! net_check_iface_qualified "$CURRENT_IFACE" "$targets" "$ACTIVE_NET_PING_COUNT"; then
        eval "fc=\${fail_count_${CURRENT_IFACE}:-0}"
        fc=$((fc + 1))
        eval "fail_count_${CURRENT_IFACE}=$fc"
        log WARN "达标检测失败计数：iface=$CURRENT_IFACE fc=$fc/${ACTIVE_NET_FAIL_THRESHOLD}"
        if [ "$fc" -lt "$ACTIVE_NET_FAIL_THRESHOLD" ]; then
            NETWORK_READY=0
            net_led_write_state "RECOVERING" || true
            return 0
        fi

        net_led_write_state "RECOVERING" || true
        if recovery_attempt_iface "$CURRENT_IFACE" "$ACTIVE_NET_TARGETS" "$ACTIVE_NET_PING_COUNT" "连续探测失败"; then
            eval "fail_count_${CURRENT_IFACE}=0"
            net_route_activate_iface "$CURRENT_IFACE" "恢复成功后保持活跃" || true
            NETWORK_READY=1
            net_led_write_state "L3_OK" || true
            return 0
        fi

        log WARN "当前接口恢复失败，触发Metric降级并切换：$CURRENT_IFACE"
        net_route_demote_iface "$CURRENT_IFACE" "连续探测失败触发降级" || true
        eval "fail_count_${CURRENT_IFACE}=0"
        CURRENT_IFACE=""
        NETWORK_READY=0
        local best
        best=$(net_select_best_iface)
        if [ -n "$best" ]; then
            net_switch_to "$best" "故障降级后切换"
        else
            net_led_write_state "L3_DOWN" || true
        fi
        return 0
    fi
    eval "fail_count_${CURRENT_IFACE}=0"
    recovery_iface_reset_state "$CURRENT_IFACE" "链路检测通过" || true
    net_route_activate_iface "$CURRENT_IFACE" "链路恢复或保持可用" || true
    NETWORK_READY=1
    net_led_write_state "L3_OK" || true
    return 0
}

net_init_selection() {
    local best
    best=$(net_select_best_iface)
    if [ -n "$best" ]; then
        log INFO "初始化选路：发现高优先级接口 $best，直接设为活跃"
        net_switch_to "$best"
        return $?
    fi
    return 1
}
