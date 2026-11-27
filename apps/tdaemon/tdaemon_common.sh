#!/bin/sh

trim() {
    echo "$1" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

to_upper() {
    echo "$1" | tr '[:lower:]' '[:upper:]'
}

basename_simple() {
    local p="$1"
    p=${p%% *}
    echo "${p##*/}"
}

now_ts() {
    date "+%Y-%m-%d %H:%M:%S"
}

get_config_fingerprint() {
    local p="$1"
    local digest
    [ -n "$p" ] && [ -f "$p" ] && [ -r "$p" ] || return 1
    if command -v sha256sum >/dev/null 2>&1; then
        digest=$(sha256sum < "$p" 2>/dev/null) || return 1
        [ -n "$digest" ] || return 1
        printf 'sha256:%s\n' "${digest%% *}"
        return 0
    fi
    if command -v cksum >/dev/null 2>&1; then
        digest=$(cksum < "$p" 2>/dev/null) || return 1
        [ -n "$digest" ] || return 1
        printf 'cksum:%s\n' "$digest"
        return 0
    fi
    echo "No checksum tool available for config monitoring" >&2
    return 1
}

log_write() {
    local level="$1"
    local msg="$2"
    local ts
    ts=$(now_ts)

    if [ -f "$DAEMON_LOG_FILE" ]; then
        local size
        size=$(wc -c < "$DAEMON_LOG_FILE" 2>/dev/null || echo 0)
        if [ "$size" -gt 1048576 ]; then
            tail -n 1000 "$DAEMON_LOG_FILE" > "${DAEMON_LOG_FILE}.tmp" 2>/dev/null
            mv "${DAEMON_LOG_FILE}.tmp" "$DAEMON_LOG_FILE" 2>/dev/null
            echo "$ts [WARN] Log file rotated (size exceeded 1MB)" >> "$DAEMON_LOG_FILE"
        fi
    fi

    echo "$ts [$level] $msg" >> "$DAEMON_LOG_FILE"
    if [ -t 1 ]; then
        echo "$ts [$level] $msg"
    fi
}

log_level_num() {
    case "$(to_upper "$1")" in
        DEBUG) echo 10 ;;
        INFO) echo 20 ;;
        WARN) echo 30 ;;
        ERROR) echo 40 ;;
        *) echo 20 ;;
    esac
}

log() {
    local level="$1"
    local msg="$2"
    local need have
    need=$(log_level_num "$DAEMON_LOG_LEVEL")
    have=$(log_level_num "$level")
    if [ "$have" -ge "$need" ]; then
        log_write "$(to_upper "$level")" "$msg"
    fi
}

die_cfg() {
    local line_no="$1"
    local reason="$2"
    echo "line $line_no: $reason" >&2
    return 1
}

cfg_reset_defaults() {
    PROCESS_ENTRIES=""
    NET_TARGETS=""
    NET_DISABLED=""
    NET_CHECK_INTERVAL=""
    NET_FAIL_THRESHOLD=""
    NET_LOSS_THRESHOLD=""
    NET_LATENCY_THRESHOLD_MS=""
    NET_PING_COUNT=""
    NET_DNS_PRIMARY=""
    NET_DNS_SECONDARY=""
    NET_RESOLV_CONF=""
    NET_ROUTE_METRIC=""
    NET_RECOVERY_LIGHT_THRESHOLD=1
    NET_RECOVERY_SOFT_THRESHOLD=2
    NET_RECOVERY_GPIO_COOLDOWN_S=100
    NET_MODEM_RESET_CHIP=""
    NET_MODEM_RESET_LINE=""
    NET_MODEM_RESET_ACTIVE_VALUE=1
    NET_MODEM_RESET_PULSE_MS=1000
    NET_MODEM_POWER_CHIP=""
    NET_MODEM_POWER_LINE=""
    NET_MODEM_POWER_ACTIVE_VALUE=1
    NET_MODEM_POWER_OFF_HOLD_MS=3000
    NET_MODEM_POWER_ON_WAIT_S=15
    NET_MODEM_ENUM_WAIT_S=20

    DAEMON_SCAN_INTERVAL=30
    DAEMON_RESTART_TIMEOUT=3
    DAEMON_LOG_LEVEL="INFO"
    DAEMON_PID_FILE="/var/run/tdaemon_job.pid"
    DAEMON_LOG_FILE="/root/daemon_guard.log"
    DAEMON_AP_MAX_UP_TIME="$AP_MAX_UP_TIME"
    DAEMON_NO_READY_REBOOT_TIMEOUT="$NET_NO_READY_REBOOT_TIMEOUT"
}

cfg_validate_ip_or_host() {
    local v="$1"
    echo "$v" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9\.\-]*[A-Za-z0-9]$|^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$'
}

cfg_parse_file() {
    local cfg_path="$1"
    local mode="$2"

    [ -z "$cfg_path" ] && cfg_path="/etc/tdaemon.conf"
    [ ! -f "$cfg_path" ] && { echo "config not found: $cfg_path" >&2; return 2; }

    cfg_reset_defaults

    local section=""
    local line_no=0
    local seen_process=0
    local seen_network=0
    local seen_daemon=0

    while IFS= read -r raw_line || [ -n "$raw_line" ]; do
        line_no=$((line_no + 1))
        local line
        line=$(echo "$raw_line" | sed 's/[[:space:]]*[#;].*$//')
        line=$(trim "$line")
        [ -z "$line" ] && continue

        case "$line" in
            \[*\])
                section="${line#[}"
                section="${section%]}"
                section=$(to_upper "$(trim "$section")")
                case "$section" in
                    PROCESS) seen_process=1 ;;
                    NETWORK) seen_network=1 ;;
                    DAEMON) seen_daemon=1 ;;
                    *) die_cfg "$line_no" "未知段[$section]"; return 1 ;;
                esac
                continue
                ;;
        esac

        echo "$line" | grep -q '=' || { die_cfg "$line_no" "缺少=号"; return 1; }
        local key val
        key=$(trim "${line%%=*}")
        val=$(trim "${line#*=}")
        key=$(to_upper "$key")
        [ -z "$key" ] && { die_cfg "$line_no" "空key"; return 1; }

        case "$section" in
            PROCESS)
                case "$key" in
                    PROC)
                        [ -z "$val" ] && { die_cfg "$line_no" "proc为空"; return 1; }
                        echo "$val" | grep -q '^/' || { die_cfg "$line_no" "proc必须是绝对路径开头(/)"; return 1; }
                        PROCESS_ENTRIES="${PROCESS_ENTRIES}${PROCESS_ENTRIES:+
}${val}"
                        ;;
                    *)
                        die_cfg "$line_no" "[process]仅支持键proc"; return 1
                        ;;
                esac
                ;;
            NETWORK)
                case "$key" in
                    TARGETS)
                        [ -z "$val" ] && { die_cfg "$line_no" "targets为空"; return 1; }
                        NET_TARGETS="$val"
                        ;;
                    DISABLED)
                        NET_DISABLED="$val"
                        ;;
                    CHECK_INTERVAL)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "check_interval需为整数秒"; return 1; }
                        NET_CHECK_INTERVAL="$val"
                        ;;
                    FAIL_THRESHOLD)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "fail_threshold需为整数"; return 1; }
                        NET_FAIL_THRESHOLD="$val"
                        ;;
                    LOSS_THRESHOLD)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "loss_threshold需为整数(%)"; return 1; }
                        NET_LOSS_THRESHOLD="$val"
                        ;;
                    LATENCY_THRESHOLD_MS)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "latency_threshold_ms需为整数(ms)"; return 1; }
                        NET_LATENCY_THRESHOLD_MS="$val"
                        ;;
                    PING_COUNT)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "ping_count需为正整数"; return 1; }
                        NET_PING_COUNT="$val"
                        ;;
                    DNS_PRIMARY)
                        cfg_validate_ip_or_host "$val" || { die_cfg "$line_no" "dns_primary格式不合法"; return 1; }
                        NET_DNS_PRIMARY="$val"
                        ;;
                    DNS_SECONDARY)
                        cfg_validate_ip_or_host "$val" || { die_cfg "$line_no" "dns_secondary格式不合法"; return 1; }
                        NET_DNS_SECONDARY="$val"
                        ;;
                    RESOLV_CONF)
                        echo "$val" | grep -q '^/' || { die_cfg "$line_no" "resolv_conf必须是绝对路径"; return 1; }
                        NET_RESOLV_CONF="$val"
                        ;;
                    ROUTE_METRIC)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "route_metric需为整数"; return 1; }
                        NET_ROUTE_METRIC="$val"
                        ;;
                    RECOVERY_LIGHT_THRESHOLD)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "recovery_light_threshold需为正整数"; return 1; }
                        NET_RECOVERY_LIGHT_THRESHOLD="$val"
                        ;;
                    RECOVERY_SOFT_THRESHOLD)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "recovery_soft_threshold需为正整数"; return 1; }
                        NET_RECOVERY_SOFT_THRESHOLD="$val"
                        ;;
                    RECOVERY_GPIO_COOLDOWN_S)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "recovery_gpio_cooldown_s需为整数秒"; return 1; }
                        NET_RECOVERY_GPIO_COOLDOWN_S="$val"
                        ;;
                    MODEM_RESET_CHIP)
                        [ -n "$val" ] || { die_cfg "$line_no" "modem_reset_chip不能为空"; return 1; }
                        NET_MODEM_RESET_CHIP="$val"
                        ;;
                    MODEM_RESET_LINE)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "modem_reset_line需为非负整数"; return 1; }
                        NET_MODEM_RESET_LINE="$val"
                        ;;
                    MODEM_RESET_ACTIVE_VALUE)
                        echo "$val" | grep -Eq '^[01]$' || { die_cfg "$line_no" "modem_reset_active_value需为0或1"; return 1; }
                        NET_MODEM_RESET_ACTIVE_VALUE="$val"
                        ;;
                    MODEM_RESET_PULSE_MS)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "modem_reset_pulse_ms需为正整数"; return 1; }
                        NET_MODEM_RESET_PULSE_MS="$val"
                        ;;
                    MODEM_POWER_CHIP)
                        [ -n "$val" ] || { die_cfg "$line_no" "modem_power_chip不能为空"; return 1; }
                        NET_MODEM_POWER_CHIP="$val"
                        ;;
                    MODEM_POWER_LINE)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "modem_power_line需为非负整数"; return 1; }
                        NET_MODEM_POWER_LINE="$val"
                        ;;
                    MODEM_POWER_ACTIVE_VALUE)
                        echo "$val" | grep -Eq '^[01]$' || { die_cfg "$line_no" "modem_power_active_value需为0或1"; return 1; }
                        NET_MODEM_POWER_ACTIVE_VALUE="$val"
                        ;;
                    MODEM_POWER_OFF_HOLD_MS)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "modem_power_off_hold_ms需为正整数"; return 1; }
                        NET_MODEM_POWER_OFF_HOLD_MS="$val"
                        ;;
                    MODEM_POWER_ON_WAIT_S)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "modem_power_on_wait_s需为正整数"; return 1; }
                        NET_MODEM_POWER_ON_WAIT_S="$val"
                        ;;
                    MODEM_ENUM_WAIT_S)
                        echo "$val" | grep -Eq '^[1-9][0-9]*$' || { die_cfg "$line_no" "modem_enum_wait_s需为正整数"; return 1; }
                        NET_MODEM_ENUM_WAIT_S="$val"
                        ;;
                    *)
                        die_cfg "$line_no" "[network]不支持的键$key"; return 1
                        ;;
                esac
                ;;
            DAEMON)
                case "$key" in
                    SCAN_INTERVAL)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "scan_interval需为整数秒"; return 1; }
                        DAEMON_SCAN_INTERVAL="$val"
                        ;;
                    RESTART_TIMEOUT)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "restart_timeout需为整数秒"; return 1; }
                        DAEMON_RESTART_TIMEOUT="$val"
                        ;;
                    LOG_LEVEL)
                        case "$(to_upper "$val")" in DEBUG|INFO|WARN|ERROR) ;; *) die_cfg "$line_no" "log_level取值DEBUG/INFO/WARN/ERROR"; return 1 ;; esac
                        DAEMON_LOG_LEVEL="$(to_upper "$val")"
                        ;;
                    PID_FILE)
                        echo "$val" | grep -q '^/' || { die_cfg "$line_no" "pid_file必须是绝对路径"; return 1; }
                        DAEMON_PID_FILE="$val"
                        ;;
                    LOG_FILE)
                        echo "$val" | grep -q '^/' || { die_cfg "$line_no" "log_file必须是绝对路径"; return 1; }
                        DAEMON_LOG_FILE="$val"
                        ;;
                    NO_READY_REBOOT_TIMEOUT)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "no_ready_reboot_timeout需为整数秒"; return 1; }
                        DAEMON_NO_READY_REBOOT_TIMEOUT="$val"
                        ;;
                    AP_MAX_UP_TIME)
                        echo "$val" | grep -Eq '^[0-9]+$' || { die_cfg "$line_no" "ap_max_up_time需为整数秒"; return 1; }
                        DAEMON_AP_MAX_UP_TIME="$val"
                        ;;
                    *)
                        die_cfg "$line_no" "[daemon]不支持的键$key"; return 1
                        ;;
                esac
                ;;
            *)
                die_cfg "$line_no" "未指定段，行不属于[process]/[network]/[daemon]"; return 1
                ;;
        esac
    done < "$cfg_path"

    [ $seen_process -eq 1 ] || { echo "missing section: [process]" >&2; return 3; }
    [ $seen_network -eq 1 ] || { echo "missing section: [network]" >&2; return 3; }
    [ $seen_daemon -eq 1 ] || { echo "missing section: [daemon]" >&2; return 3; }

    [ -n "$PROCESS_ENTRIES" ] || { echo "missing [process] proc entries" >&2; return 3; }

    [ -n "$NET_TARGETS" ] || { echo "missing [network] targets" >&2; return 3; }
    [ -n "$NET_CHECK_INTERVAL" ] || { die_cfg 0 "missing [network] check_interval"; return 1; }
    [ -n "$NET_FAIL_THRESHOLD" ] || { die_cfg 0 "missing [network] fail_threshold"; return 1; }
    [ -n "$NET_LOSS_THRESHOLD" ] || { die_cfg 0 "missing [network] loss_threshold"; return 1; }
    [ -n "$NET_LATENCY_THRESHOLD_MS" ] || { die_cfg 0 "missing [network] latency_threshold_ms"; return 1; }
    [ -n "$NET_PING_COUNT" ] || { die_cfg 0 "missing [network] ping_count"; return 1; }
    [ -n "$NET_DNS_PRIMARY" ] || { die_cfg 0 "missing [network] dns_primary"; return 1; }
    [ -n "$NET_DNS_SECONDARY" ] || { die_cfg 0 "missing [network] dns_secondary"; return 1; }
    [ -n "$NET_RESOLV_CONF" ] || { die_cfg 0 "missing [network] resolv_conf"; return 1; }
    [ -n "$NET_ROUTE_METRIC" ] || { die_cfg 0 "missing [network] route_metric"; return 1; }

    [ -n "$DAEMON_SCAN_INTERVAL" ] || { die_cfg 0 "missing [daemon] scan_interval"; return 1; }
    [ -n "$DAEMON_RESTART_TIMEOUT" ] || { die_cfg 0 "missing [daemon] restart_timeout"; return 1; }
    [ -n "$DAEMON_LOG_LEVEL" ] || { die_cfg 0 "missing [daemon] log_level"; return 1; }
    [ -n "$DAEMON_PID_FILE" ] || { die_cfg 0 "missing [daemon] pid_file"; return 1; }
    [ -n "$DAEMON_LOG_FILE" ] || { die_cfg 0 "missing [daemon] log_file"; return 1; }
    [ -n "$DAEMON_AP_MAX_UP_TIME" ] || { die_cfg 0 "missing [daemon] ap_max_up_time"; return 1; }
    [ -n "$DAEMON_NO_READY_REBOOT_TIMEOUT" ] || { die_cfg 0 "missing [daemon] no_ready_reboot_timeout"; return 1; }

    local targets_count
    targets_count=$(echo "$NET_TARGETS" | tr ',' ' ' | awk '{print NF}')
    [ "$targets_count" -ge 3 ] || { echo "[network] targets需至少3个公网地址" >&2; return 3; }

    local t
    for t in $(echo "$NET_TARGETS" | tr ',' ' '); do
        cfg_validate_ip_or_host "$t" || { echo "invalid target: $t" >&2; return 3; }
    done

    return 0
}

cfg_apply_new() {
    ACTIVE_PROCESS_ENTRIES="$PROCESS_ENTRIES"
    ACTIVE_NET_TARGETS="$NET_TARGETS"
    ACTIVE_NET_DISABLED="$NET_DISABLED"
    ACTIVE_NET_CHECK_INTERVAL="$NET_CHECK_INTERVAL"
    ACTIVE_NET_FAIL_THRESHOLD="$NET_FAIL_THRESHOLD"
    ACTIVE_NET_LOSS_THRESHOLD="$NET_LOSS_THRESHOLD"
    ACTIVE_NET_LATENCY_THRESHOLD_MS="$NET_LATENCY_THRESHOLD_MS"
    ACTIVE_NET_PING_COUNT="$NET_PING_COUNT"
    ACTIVE_NET_DNS_PRIMARY="$NET_DNS_PRIMARY"
    ACTIVE_NET_DNS_SECONDARY="$NET_DNS_SECONDARY"
    ACTIVE_NET_RESOLV_CONF="$NET_RESOLV_CONF"
    ACTIVE_NET_ROUTE_METRIC="$NET_ROUTE_METRIC"
    ACTIVE_NET_RECOVERY_LIGHT_THRESHOLD="$NET_RECOVERY_LIGHT_THRESHOLD"
    ACTIVE_NET_RECOVERY_SOFT_THRESHOLD="$NET_RECOVERY_SOFT_THRESHOLD"
    ACTIVE_NET_RECOVERY_GPIO_COOLDOWN_S="$NET_RECOVERY_GPIO_COOLDOWN_S"
    ACTIVE_NET_MODEM_RESET_CHIP="$NET_MODEM_RESET_CHIP"
    ACTIVE_NET_MODEM_RESET_LINE="$NET_MODEM_RESET_LINE"
    ACTIVE_NET_MODEM_RESET_ACTIVE_VALUE="$NET_MODEM_RESET_ACTIVE_VALUE"
    ACTIVE_NET_MODEM_RESET_PULSE_MS="$NET_MODEM_RESET_PULSE_MS"
    ACTIVE_NET_MODEM_POWER_CHIP="$NET_MODEM_POWER_CHIP"
    ACTIVE_NET_MODEM_POWER_LINE="$NET_MODEM_POWER_LINE"
    ACTIVE_NET_MODEM_POWER_ACTIVE_VALUE="$NET_MODEM_POWER_ACTIVE_VALUE"
    ACTIVE_NET_MODEM_POWER_OFF_HOLD_MS="$NET_MODEM_POWER_OFF_HOLD_MS"
    ACTIVE_NET_MODEM_POWER_ON_WAIT_S="$NET_MODEM_POWER_ON_WAIT_S"
    ACTIVE_NET_MODEM_ENUM_WAIT_S="$NET_MODEM_ENUM_WAIT_S"

    ACTIVE_SCAN_INTERVAL="$DAEMON_SCAN_INTERVAL"
    ACTIVE_RESTART_TIMEOUT="$DAEMON_RESTART_TIMEOUT"
    ACTIVE_AP_MAX_UP_TIME="$DAEMON_AP_MAX_UP_TIME"
    ACTIVE_NO_READY_REBOOT_TIMEOUT="$DAEMON_NO_READY_REBOOT_TIMEOUT"
    DAEMON_LOG_LEVEL="$DAEMON_LOG_LEVEL"
    DAEMON_PID_FILE="$DAEMON_PID_FILE"
    DAEMON_LOG_FILE="$DAEMON_LOG_FILE"
}

cfg_reload_if_changed() {
    local cfg_path="$1"
    local mt
    mt=$(get_config_fingerprint "$cfg_path")
    [ -z "$mt" ] && return 0
    if [ -z "$ACTIVE_CFG_FINGERPRINT" ]; then
        ACTIVE_CFG_FINGERPRINT="$mt"
        return 0
    fi
    if [ "$mt" != "$ACTIVE_CFG_FINGERPRINT" ]; then
        local old_log="$DAEMON_LOG_FILE"
        if cfg_parse_file "$cfg_path" "reload"; then
            cfg_apply_new
            ACTIVE_CFG_FINGERPRINT="$mt"
            log INFO "配置热加载成功：$cfg_path"
        else
            DAEMON_LOG_FILE="$old_log"
            log ERROR "配置热加载失败，继续使用旧配置：$cfg_path"
        fi
    fi
}

pidfile_write() {
    local pf="$1"
    mkdir -p "$(dirname "$pf")" 2>/dev/null || true
    echo "$$" > "$pf" 2>/dev/null || true
}

pidfile_remove() {
    local pf="$1"
    [ -n "$pf" ] && rm -f "$pf" 2>/dev/null || true
}

lock_acquire() {
    local lock="$1"
    if mkdir "$lock" 2>/dev/null; then
        echo "$$" > "$lock/pid" 2>/dev/null || true
        return 0
    fi
    if [ -f "$lock/pid" ]; then
        local old_pid
        old_pid=$(cat "$lock/pid" 2>/dev/null)
        if [ -n "$old_pid" ] && [ -d "/proc/$old_pid" ]; then
            return 1
        fi
    fi
    log WARN "发现僵尸锁（PID可能已死），尝试接管..."
    rm -rf "$lock" 2>/dev/null || true
    if mkdir "$lock" 2>/dev/null; then
        echo "$$" > "$lock/pid" 2>/dev/null || true
        log INFO "接管锁成功"
        return 0
    fi
    return 1
}

lock_release() {
    local lock="$1"
    [ -n "$lock" ] || return 0
    rm -f "$lock/pid" 2>/dev/null || true
    rmdir "$lock" 2>/dev/null || true
}
