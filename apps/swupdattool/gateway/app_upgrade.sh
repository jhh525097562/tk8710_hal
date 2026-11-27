#!/bin/sh
# Installed on the base firmware; also embedded in each matching application SWU.
set -eu
ROOT=${APP_UPGRADE_TEST_ROOT:-}
BASE="$ROOT/userdata/.app-upgrade"
TX="$BASE/transaction"
MAINT="$ROOT/var/run/app-upgrade-maintenance"
LOCK="$ROOT/var/run/app-upgrade-lock"
NAMES="mqtt_bridge web_server tk8710_gw"
log() { echo "[app-upgrade] $*"; }
phase() { printf '%s\n' "$1" > "$TX/phase.tmp"; sync; mv "$TX/phase.tmp" "$TX/phase"; sync; }
stop_apps() {
    [ -z "$ROOT" ] || return 0
    for name in mqtt_bridge tk8710_gw; do
        for pid in $(pidof "$name" 2>/dev/null || true); do
            exe=$(readlink "/proc/$pid/exe" 2>/dev/null || true)
            case "$exe" in "/userdata/$name"|"/userdata/$name (deleted)")
                kill -TERM "$pid" 2>/dev/null || true ;;
            esac
        done
    done
    i=0
    while [ "$i" -lt 20 ]; do
        found=0
        for name in mqtt_bridge tk8710_gw; do
            for pid in $(pidof "$name" 2>/dev/null || true); do
                exe=$(readlink "/proc/$pid/exe" 2>/dev/null || true)
                case "$exe" in "/userdata/$name"|"/userdata/$name (deleted)") found=1;; esac
            done
        done
        [ "$found" -eq 1 ] || return 0
        sleep 1; i=$((i + 1))
    done
    log "Applications did not stop gracefully; refusing installation"
    return 1
}
resume_apps() {
    rm -f "$MAINT"
    # Existing daemon starts the configured applications on its next proc tick.
}
recover() {
    # A rejected concurrent updater must never roll back the lock holder.
    # Boot/manual recovery deliberately has no owner and handles abandoned jobs.
    if [ -n "${APP_UPGRADE_OWNER:-}" ] && [ -f "$LOCK/owner" ] &&
       [ "$(cat "$LOCK/owner")" != "$APP_UPGRADE_OWNER" ]; then
        log 'Recovery skipped: transaction belongs to another updater'
        return 0
    fi
    # Keep the daemon paused if any restore step fails (also after a reboot).
    mkdir -p "$ROOT/var/run"
    touch "$MAINT"
    if [ -f "$TX/phase" ]; then
        state=$(cat "$TX/phase")
        case "$state" in
        prepared|applying)
            log "Recovering interrupted transaction ($state)"
            for name in $NAMES; do
                test -f "$TX/backup/$name" || return 1
                cp -p "$TX/backup/$name" "$ROOT/userdata/.$name.restore" || return 1
                mv "$ROOT/userdata/.$name.restore" "$ROOT/userdata/$name" || return 1
            done
            test -f "$TX/backup/device_version.json" || return 1
            cp -p "$TX/backup/device_version.json" "$ROOT/etc/device_version.json.restore" || return 1
            mv "$ROOT/etc/device_version.json.restore" "$ROOT/etc/device_version.json" || return 1
            sync; phase rolled_back ;;
        committed|rolled_back) ;;
        *) log "Unknown transaction state; operator inspection needed"; return 1 ;;
        esac
    fi
    resume_apps
    if [ -d "$LOCK" ]; then rm -f "$LOCK/pid" "$LOCK/owner"; rmdir "$LOCK"; fi
}

case "${1:-}" in
preinst)
    [ -z "$ROOT" ] && {
        [ "$(uname -m)" = armv7l ] || exit 1
        tr '\000' '\n' < /proc/device-tree/compatible | grep -q 'rockchip,rk3506' || exit 1
    }
    command -v sha256sum >/dev/null
    test -f "$ROOT/etc/device_version.json"
    test -d "$ROOT/userdata"
    mkdir -p "$BASE" "$ROOT/var/run"
    mkdir "$LOCK" || { log 'Another upgrade holds the lock'; exit 1; }
    echo $$ > "$LOCK/pid"
    printf '%s\n' "${APP_UPGRADE_OWNER:-manual}" > "$LOCK/owner"
    if [ -d "$TX" ]; then
        if [ -f "$TX/phase" ]; then
            case "$(cat "$TX/phase")" in committed|rolled_back) ;; *) rm -f "$LOCK/pid" "$LOCK/owner"; rmdir "$LOCK"; log 'Unfinished transaction requires recovery'; exit 1;; esac
        fi
        archive=$(mktemp -d "$BASE/history-XXXXXX")
        mv "$TX" "$archive/transaction"
    fi
    mkdir -p "$TX/incoming" "$TX/backup"
    free_kb=$(df -Pk "$ROOT/userdata" | awk 'END {print $4}')
    [ "$free_kb" -ge "${REQUIRED_KB:?}" ] || { recover; log 'Insufficient space'; exit 1; }
    log 'Preflight passed; staging only, no applications stopped yet'
    ;;
postinst)
    test -d "$LOCK"
    trap 'rc=$?; trap - EXIT; if [ "$rc" -ne 0 ]; then recover || true; fi; exit "$rc"' EXIT
    cd "$TX/incoming"
    printf '%s\n' "${PACKAGE_HASHES:?}" | sha256sum -c -
    if [ -z "$ROOT" ]; then
        for name in $NAMES metadata; do /lib/ld-linux-armhf.so.3 --list "./$name" >/dev/null; done
    fi
    # All payloads have arrived before creating rollback state or stopping services.
    for name in $NAMES; do test -f "$ROOT/userdata/$name"; cp -p "$ROOT/userdata/$name" "$TX/backup/$name"; done
    cp -p "$ROOT/etc/device_version.json" "$TX/backup/device_version.json"
    chmod 755 metadata
    ./metadata "$ROOT/etc/device_version.json" "$TX/new-device-version.json" "${PACKAGE_VERSION:?}"
    phase prepared
    touch "$MAINT"; sync
    # Give the daemon an opportunity to leave a network/proc tick already in flight.
    [ -n "$ROOT" ] || sleep 5
    stop_apps
    phase applying
    for name in $NAMES; do
        chmod 755 "$name"
        mv "$name" "$ROOT/userdata/$name"
        sync
        if [ -n "$ROOT" ] && [ "${APP_UPGRADE_FAIL_AFTER:-}" = "$name" ]; then exit 1; fi
        if [ -n "$ROOT" ] && [ "${APP_UPGRADE_CRASH_AFTER:-}" = "$name" ]; then kill -KILL $$; fi
    done
    cp "$TX/new-device-version.json" "$ROOT/etc/device_version.json.upgrade"
    chmod 644 "$ROOT/etc/device_version.json.upgrade"
    sync; mv "$ROOT/etc/device_version.json.upgrade" "$ROOT/etc/device_version.json"; sync
    phase committed
    resume_apps
    rm -f "$LOCK/pid" "$LOCK/owner"; rmdir "$LOCK"
    log "Committed app bundle ${PACKAGE_VERSION}"
    ;;
recover) recover ;;
*) echo 'Usage: app_upgrade.sh preinst|postinst|recover' >&2; exit 2 ;;
esac
