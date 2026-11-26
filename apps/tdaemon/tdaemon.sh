#!/bin/sh
###
 # @Author: 359327604 359327604@qq.com
 # @Date: 2026-03-11 16:21:08
 # @LastEditors: 359327604 359327604@qq.com
 # @LastEditTime: 2026-04-07 16:57:25
 # @FilePath: \tdaemon\tdaemon.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 

exec 2>>/root/tdaemon_debug_full.log
set -x
TDAEMON_VERSION="1.0.0"
SCRIPT_DIR="/usr/bin"
TDAEMON_DEBUG_LOG_FILE="/root/tdaemon_debug_full.log"
TDAEMON_DEBUG_LOG_MAX_BYTES=5242880
TDAEMON_DEBUG_LOG_TAIL_LINES=5000
TDAEMON_DEBUG_LOG_CHECK_INTERVAL=10

NET_IFACE_OP_TIMEOUT=10
NET_IFACE_LINK_WAIT_TIMEOUT=15
NET_FORCE_STOP_WAIT=2
WLAN_AP_CLIENT_POLL_INTERVAL=5

tdaemon_resolve_module_dir() {
    if [ -n "$TDAEMON_MODULE_DIR" ] && [ -d "$TDAEMON_MODULE_DIR" ]; then
        echo "$TDAEMON_MODULE_DIR"
        return 0
    fi
    if [ -n "$0" ]; then
        local script_dir
        script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
        if [ -n "$script_dir" ] && [ -d "$script_dir" ]; then
            echo "$script_dir"
            return 0
        fi
    fi
    echo "$SCRIPT_DIR"
}

tdaemon_source_required() {
    local module_dir="$1"
    local module_name="$2"
    local module_path="$module_dir/$module_name"
    [ -f "$module_path" ] || module_path="$SCRIPT_DIR/$module_name"
    [ -f "$module_path" ] || {
        echo "required module not found: $module_name" >&2
        exit 1
    }
    . "$module_path"
}

tdaemon_rotate_debug_log_if_needed() {
    [ -f "$TDAEMON_DEBUG_LOG_FILE" ] || return 0
    local size
    size=$(wc -c < "$TDAEMON_DEBUG_LOG_FILE" 2>/dev/null || echo 0)
    [ "$size" -gt "$TDAEMON_DEBUG_LOG_MAX_BYTES" ] || return 0

    local tmp_file
    tmp_file="${TDAEMON_DEBUG_LOG_FILE}.tmp"
    tail -n "$TDAEMON_DEBUG_LOG_TAIL_LINES" "$TDAEMON_DEBUG_LOG_FILE" > "$tmp_file" 2>/dev/null || return 1
    cat "$tmp_file" > "$TDAEMON_DEBUG_LOG_FILE" 2>/dev/null || {
        rm -f "$tmp_file" 2>/dev/null || true
        return 1
    }
    rm -f "$tmp_file" 2>/dev/null || true
    echo "$(date '+%Y-%m-%d %H:%M:%S') [DEBUG] debug log truncated to last ${TDAEMON_DEBUG_LOG_TAIL_LINES} lines at ${TDAEMON_DEBUG_LOG_MAX_BYTES} bytes" >&2
    return 0
}

TDAEMON_MODULE_DIR=$(tdaemon_resolve_module_dir)
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_common.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_gpio.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_net.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_status.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_dns.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_recovery.sh"
tdaemon_source_required "$TDAEMON_MODULE_DIR" "tdaemon_proc.sh"

tdaemon_run() {
    local cfg_path="$1"
    local lock_dir="/var/run/tdaemon_job.lock"
    cfg_reset_defaults

    if ! lock_acquire "$lock_dir"; then
        local old_pid=""
        [ -f "$lock_dir/pid" ] && old_pid=$(cat "$lock_dir/pid" 2>/dev/null)
        if [ -n "$old_pid" ] && [ -d "/proc/$old_pid" ]; then
            log ERROR "已有实例运行 pid=$old_pid"
        else
            log ERROR "已有实例运行"
        fi
        return 0
    fi

    if ! cfg_parse_file "$cfg_path" "run"; then
        echo "配置解析失败，无法启动：$cfg_path" >&2
        lock_release "$lock_dir"
        return 1
    fi
    cfg_apply_new
    dns_ensure_config || true
    ACTIVE_CFG_PATH="$cfg_path"
    ACTIVE_CFG_FINGERPRINT=$(get_config_fingerprint "$cfg_path")

    pidfile_write "$DAEMON_PID_FILE"
    trap 'network_status_stop; log WARN "收到退出信号，清理PID文件并退出"; pidfile_remove "$DAEMON_PID_FILE"; lock_release "$lock_dir"; exit 0' TERM INT

    log INFO "tdaemon启动：version=$TDAEMON_VERSION cfg=$cfg_path pidfile=$DAEMON_PID_FILE"

    NETWORK_READY=0
    CURRENT_IFACE=""
    network_status_start || log WARN "网络状态JSON观察器启动失败"

    ### Enhancement begin ###
    wlan_bootstrap_ap || true
    ### Enhancement end ###

    net_init_selection

    local tick=0
    local next_net=0
    local next_proc=0
    local next_debug_log_check=0
    local no_net_ready_seconds=0
    local no_net_reboot_requested=0

    # 初始化 config.json 的内容指纹
    local config_json_path="/etc/config.json"
    local config_json_fingerprint
    config_json_fingerprint=$(get_config_fingerprint "$config_json_path")

    while true; do
        network_status_progress || true
        if [ -e /var/run/app-upgrade-maintenance ]; then
            network_status_event unknown none maintenance || true
            sleep 1; continue
        fi
        tick=$((tick + 1))
        cfg_reload_if_changed "$ACTIVE_CFG_PATH"
        dns_ensure_config || true

        # 检查 config.json 是否变更
        local new_fingerprint
        new_fingerprint=$(get_config_fingerprint "$config_json_path")
        if [ -n "$new_fingerprint" ] && [ "$new_fingerprint" != "$config_json_fingerprint" ]; then
            log INFO "检测到 $config_json_path 变更，重启相关进程..."
            killall -9 mqtt_bridge tk8710_gw 2>/dev/null
            proc_reload_frp_config
            config_json_fingerprint="$new_fingerprint"
            # 下一次 proc_tick 会自动把它们拉起来
            next_proc=$((tick + 1)) 
        fi

        if [ $tick -ge $next_net ]; then
            net_tick
            next_net=$((tick + ACTIVE_NET_CHECK_INTERVAL))
        fi

        if [ $tick -ge $next_proc ]; then
            proc_tick
            next_proc=$((tick + ACTIVE_SCAN_INTERVAL))
        fi

        if [ $tick -ge $next_debug_log_check ]; then
            tdaemon_rotate_debug_log_if_needed || true
            next_debug_log_check=$((tick + TDAEMON_DEBUG_LOG_CHECK_INTERVAL))
        fi

        ### Enhancement begin ###
        if [ "$NETWORK_READY" -eq 1 ]; then
            no_net_ready_seconds=0
            no_net_reboot_requested=0
        else
            no_net_ready_seconds=$((no_net_ready_seconds + 1))
            if [ "$ACTIVE_NO_READY_REBOOT_TIMEOUT" -gt 0 ] && [ "$no_net_ready_seconds" -ge "$ACTIVE_NO_READY_REBOOT_TIMEOUT" ] && [ "$no_net_reboot_requested" -eq 0 ]; then
                no_net_reboot_requested=1
                net_reboot_due_to_no_network || true
            fi
        fi
        ### Enhancement end ###

        sleep 1
    done
}

tdaemon_check_config() {
    local cfg_path="$1"
    if cfg_parse_file "$cfg_path" "check"; then
        return 0
    fi
    return 1
}

tdaemon_usage() {
    cat <<EOF
tdaemon $TDAEMON_VERSION
Usage:
  tdaemon --check-config [-c <config>]
  tdaemon [-c <config>]
Options:
  -c, --config        Config file path (default: /etc/tdaemon.conf)
  --check-config      Validate configuration, return 0 if ok
  -h, --help          Show help
  -v, --version       Show version
EOF
}

tdaemon_main() {
    local cfg_path="/etc/tdaemon.conf"
    [ -f "./tdaemon.conf" ] && cfg_path="./tdaemon.conf"

    local mode="run"
    while [ $# -gt 0 ]; do
        case "$1" in
            --check-config) mode="check"; shift ;;
            -c|--config) cfg_path="$2"; shift 2 ;;
            -h|--help) tdaemon_usage; return 0 ;;
            -v|--version) echo "$TDAEMON_VERSION"; return 0 ;;
            *) echo "Unknown arg: $1" >&2; tdaemon_usage; return 2 ;;
        esac
    done

    if [ "$mode" = "check" ]; then
        tdaemon_check_config "$cfg_path"
        return $?
    fi
    tdaemon_run "$cfg_path"
}

# check if script is sourced
if (return 0 2>/dev/null); then
    :
else
    tdaemon_main "$@"
fi
