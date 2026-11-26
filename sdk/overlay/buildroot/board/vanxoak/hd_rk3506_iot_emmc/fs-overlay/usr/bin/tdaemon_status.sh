#!/bin/sh
# Local network snapshot observer. No routing, GPIO or link mutations here.
# /proc/uptime is CLOCK_BOOTTIME (includes suspend); JSON names it explicitly.
NETWORK_STATUS_ROOT=${NETWORK_STATUS_ROOT:-/run/led}

network_status_now() {
    awk '{printf "%.0f\n", $1 * 1000}' /proc/uptime
}

network_status_identity() {
    case "$1" in ''|*[!0-9]*) return 1;; esac
    awk '{sub(/^.*\) /, ""); if ($1 != "Z") print $20}' "/proc/$1/stat" 2>/dev/null
}

network_status_allowed() {
    local iface token disabled out=""
    disabled=$(printf '%s' "${ACTIVE_NET_DISABLED:-}" | tr ',' ' ')
    for iface in eth0 usb0 wlan0; do
        local skip=0
        for token in $disabled; do
            case "$iface:$token" in eth0:ETH|eth0:eth0|usb0:4G|usb0:usb0|wlan0:WiFi|wlan0:wlan0) skip=1;; esac
        done
        [ "$skip" = 1 ] || out="${out}${out:+,}$iface"
    done
    printf '%s\n' "$out"
}

network_status_progress() {
    [ -n "${NETWORK_STATUS_DIR:-}" ] || return 0
    local now allowed tmp
    now=$(network_status_now) || return 1
    allowed=$(network_status_allowed)
    tmp=$(mktemp "$NETWORK_STATUS_DIR/progress.XXXXXX") || return 1
    printf '%s %s\n' "$now" "${allowed:-none}" > "$tmp" &&
        mv -f "$tmp" "$NETWORK_STATUS_DIR/progress"
}

network_status_event() {
    [ -n "${NETWORK_STATUS_DIR:-}" ] || return 0
    local kind="$1" iface="${2:-none}" why="$3" now tmp
    local old_op old_iface old_why old_since result="unknown" result_iface="none" checked=0
    case "$kind" in unknown|checking|up|switching|down|stopped) ;; *) return 1;; esac
    case "$iface" in eth0|usb0|wlan0|none|'') ;; *) return 1;; esac
    case "$why" in ''|*[!a-z0-9_]*) return 1;; esac
    now=$(network_status_now) || return 1
    if [ -r "$NETWORK_STATUS_DIR/control" ]; then
        read -r old_op old_iface old_why old_since result result_iface checked < "$NETWORK_STATUS_DIR/control" || return 1
    fi
    network_status_progress || return 1
    case "$kind" in
        up) result=up; result_iface=${iface:-none}; checked=$now;;
        down) result=down; result_iface=none; checked=$now;;
        switching|unknown|stopped) result=unknown; result_iface=none;;
    esac
    tmp=$(mktemp "$NETWORK_STATUS_DIR/control.XXXXXX") || return 1
    printf '%s %s %s %s %s %s %s\n' "$kind" "${iface:-none}" "$why" "$now" "$result" "$result_iface" "$checked" > "$tmp" &&
        mv -f "$tmp" "$NETWORK_STATUS_DIR/control"
}

network_status_link() {
    local iface="$1" carrier
    NETWORK_STATUS_LINK_UP=false
    NETWORK_STATUS_IPV4_READY=false
    [ -d "/sys/class/net/$iface" ] || return 1
    carrier=$(cat "/sys/class/net/$iface/carrier" 2>/dev/null) || return 1
    [ "$carrier" = 1 ] || return 1
    NETWORK_STATUS_LINK_UP=true
    ip -4 addr show dev "$iface" 2>/dev/null | grep -q 'inet ' || return 1
    NETWORK_STATUS_IPV4_READY=true
    return 0
}

# Separate decision function permits isolated tests with a fake clock/link.
network_status_render() {
    local parent="$1" start="$2" boot="$3" now="$4" dir="$5"
    local op iface why op_since result result_iface checked progress allowed
    read -r op iface why op_since result result_iface checked < "$dir/control" || return 1
    read -r progress allowed < "$dir/progress" || return 1
    case "$op_since:$checked:$progress:$now" in *[!0-9:]*) return 1;; esac
    case "$op" in unknown|checking|up|switching|down|stopped) ;; *) return 1;; esac
    case "$why" in ''|*[!a-z0-9_]*) return 1;; esac
    case "$iface:$result_iface" in *[!a-z0-9_:]*) return 1;; esac
    local state="$op" reason="$why" active=none link=false ipv4=false ready=false
    local max_age=45000 probe_limit=60000
    case "$allowed" in *[!a-z0-9_,]*) return 1;; esac
    if [ "$now" -lt "$progress" ] || [ "$now" -lt "$checked" ] || [ "$now" -lt "$op_since" ]; then
        state=unknown; reason=invalid_timestamp
    elif [ "$op" = stopped ]; then
        state=unknown; reason=publisher_stopped
    elif [ "$((now - progress))" -gt "$max_age" ]; then
        state=unknown; reason=publisher_progress_stale
    elif { [ "$op" = checking ] || [ "$op" = switching ]; } && [ "$((now - op_since))" -gt "$probe_limit" ]; then
        state=unknown; reason=probe_timeout
    elif { [ "$op" = up ] || [ "$op" = checking ]; } && [ "$result" = up ] && [ "$((now - checked))" -le "$max_age" ]; then
        case ",$allowed," in
            *",$result_iface,"*)
                active=$result_iface
                if network_status_link "$result_iface"; then
                    if [ "$checked" -le "${NETWORK_STATUS_INVALIDATED_CHECKED:-0}" ]; then
                        state=checking; reason=awaiting_fresh_probe
                    else
                        state=up; ready=true; reason=last_probe_valid
                    fi
                else
                    state=switching; reason=active_link_or_ip_lost
                    NETWORK_STATUS_INVALIDATED_CHECKED=$checked
                fi
                link=$NETWORK_STATUS_LINK_UP; ipv4=$NETWORK_STATUS_IPV4_READY;;
            *) state=switching; reason=active_interface_disabled
               NETWORK_STATUS_INVALIDATED_CHECKED=$checked;;
        esac
    elif { [ "$op" = down ] || [ "$op" = checking ]; } && [ "$result" = down ] && [ "$((now - checked))" -le "$max_age" ]; then
        state=down; reason=no_qualified_interface
    elif [ "$op" = up ] || [ "$op" = down ]; then
        state=unknown; reason=probe_result_stale
    fi
    local allowed_json="" token
    for token in $(printf '%s' "$allowed" | tr ',' ' '); do
        case "$token" in eth0|usb0|wlan0) allowed_json="${allowed_json}${allowed_json:+,}\"$token\"";; esac
    done
    # Publisher identity belongs to tdaemon; heartbeat writer is its child.
    printf '{"schema":1,"pid":%s,"start_ticks":%s,"boot_id":"%s","clock":"boottime","state":"%s","reason":"%s","ready":%s,"active_iface":"%s","allowed_ifaces":[%s],"link_up":%s,"ipv4_ready":%s,"probe_state":"%s","probe_iface":"%s","updated_boottime_ms":%s,"checked_boottime_ms":%s,"publisher_progress_boottime_ms":%s,"probe_since_boottime_ms":%s}\n' \
        "$parent" "$start" "$boot" "$state" "$reason" "$ready" "$active" "$allowed_json" "$link" "$ipv4" "$op" "$iface" "$now" "$checked" "$progress" "$op_since"
}

network_status_loop() {
    # tdaemon enables xtrace globally; avoid logging every 1s observation step.
    set +x
    local parent="$1" start="$2" boot="$3" dir="$4" now
    trap 'exit 0' TERM INT HUP
    while [ "$(network_status_identity "$parent")" = "$start" ] &&
          [ "$(cat "$NETWORK_STATUS_ROOT/network_status.owner" 2>/dev/null)" = "$parent $start" ]; do
        now=$(network_status_now) || break
        if network_status_render "$parent" "$start" "$boot" "$now" "$dir" > "$dir/output.tmp"; then
            chmod 644 "$dir/output.tmp"
            # Recheck owner immediately before publication; never publish after a normal stop.
            if [ "$(cat "$NETWORK_STATUS_ROOT/network_status.owner" 2>/dev/null)" = "$parent $start" ]; then
                mv -f "$dir/output.tmp" "$NETWORK_STATUS_ROOT/network_status.json"
            fi
        fi
        sleep 1
    done
}

network_status_start() {
    local start boot tmp
    start=$(network_status_identity "$$")
    boot=$(cat /proc/sys/kernel/random/boot_id) || return 1
    case "$start" in ''|*[!0-9]*) return 1;; esac
    mkdir -p "$NETWORK_STATUS_ROOT" || return 1
    NETWORK_STATUS_DIR=$(mktemp -d "$NETWORK_STATUS_ROOT/network-observer.XXXXXX") || return 1
    network_status_progress || return 1
    network_status_event unknown none process_start || return 1
    tmp=$(mktemp "$NETWORK_STATUS_ROOT/network-owner.XXXXXX") || return 1
    printf '%s %s\n' "$$" "$start" > "$tmp"
    mv -f "$tmp" "$NETWORK_STATUS_ROOT/network_status.owner" || return 1
    network_status_loop "$$" "$start" "$boot" "$NETWORK_STATUS_DIR" &
    NETWORK_STATUS_WORKER=$!
}

network_status_stop() {
    [ -n "${NETWORK_STATUS_DIR:-}" ] || return 0
    network_status_event stopped none publisher_stopped || true
    # Worker terminates itself when the exact parent identity disappears.
    # Leave snapshots for diagnostics; readers must reject a dead owner PID.
}
