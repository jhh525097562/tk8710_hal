#!/bin/bash
# Synchronize maintained release modules, not the SDK's build entrypoints.
set -euo pipefail

dry_run=0
if [[ "${1:-}" == --dry-run ]]; then
    dry_run=1
    shift
fi
if [[ "${1:-}" == --help ]]; then
    echo "Usage: bash sync_apps.sh [--dry-run] [source_apps [sdk_root]]"
    exit 0
fi
[[ $# -le 2 ]] || { echo "Too many arguments" >&2; exit 2; }
source_apps=${1:-${RK3506_APP_SOURCE:-/mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app}}
sdk=${2:-${SDK_ROOT:-/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0}}
fail() { echo "App sync failed: $*" >&2; exit 1; }

command -v rsync >/dev/null || fail "rsync is required"
[[ -d "$source_apps" ]] || fail "source unavailable: $source_apps"
source_apps=$(realpath "$source_apps")
sdk=$(realpath -e "$sdk")
[[ -f "$sdk/build.sh" ]] || fail "not an SDK root: $sdk"
destination="$sdk/external/rk3506app"
[[ -d "$destination" && ! -L "$destination" ]] || fail "invalid destination: $destination"
[[ "$(realpath "$destination")" == "$destination" ]] || fail "destination must stay inside SDK"

if [[ "$source_apps" == "$destination" ]]; then
    echo "App sync: using SDK sources explicitly: $destination"
    exit 0
fi
case "$source_apps/" in "$destination/"*) fail "source is inside destination" ;; esac
case "$destination/" in "$source_apps/"*) fail "destination is inside source" ;; esac

modules=(Discovery SMP ipcweb light log mqtt tdaemon webserver common third_party frp watchdog swupdattool tk8710app)
# Check the whole source before changing any destination. Missing mounts must
# never silently compile an old SDK copy.
for module in "${modules[@]}"; do
    [[ -d "$source_apps/$module" && ! -L "$source_apps/$module" ]] || fail "missing module: $module"
    [[ ! -L "$destination/$module" ]] || fail "destination module is a symlink: $module"
    [[ ! -e "$destination/$module" || -d "$destination/$module" ]] || fail "destination module is not a directory: $module"
    unexpected=$(find "$source_apps/$module" ! -type f ! -type d -print -quit)
    [[ -z "$unexpected" ]] || fail "unsupported source entry: $unexpected"
done
for required in mqtt/src/main.c webserver/src/main.c tk8710app/tk8710_gw; do
    [[ -f "$source_apps/$required" ]] || fail "missing source file: $required"
done

filters=(
    --exclude='.git/' --exclude='.trae/' --exclude='.codex/'
    --exclude='build/' --exclude='build_*/' --exclude='bin/'
    --exclude='backup/' --exclude='backups/' --exclude='docs/'
    --exclude='logs/' --exclude='remote_logs/' --exclude='__pycache__/'
    --exclude='*.log' --exclude='*.o' --exclude='*.pyc' --exclude='core'
    --exclude='*.bak' --exclude='*.bak.*' --exclude='*.rar'
    --exclude='*.zip' --exclude='*.swu' --exclude='*.img'
    --exclude='web_resources.h'
)
echo "App sync source: $source_apps"
echo "App sync destination: $destination"
if (( dry_run )); then
    for module in "${modules[@]}"; do
        rsync -rcn --delete-delay --itemize-changes "${filters[@]}" \
            "$source_apps/$module/" "$destination/$module/"
    done
    echo "App sync preview complete; no files changed."
    exit 0
fi

mkdir -p "$sdk/output"
backup=$(mktemp -d "$sdk/output/app-sync-backup-$(date +%Y%m%d-%H%M%S)-XXXXXX")
echo "App sync backup: $backup"
printf 'source=%s\ndestination=%s\n' "$source_apps" "$destination" > "$backup/paths.txt"
for module in "${modules[@]}"; do
    mkdir -p "$destination/$module"
    # Checksums catch same-size edits with unchanged Windows timestamps. Deleted
    # source files are removed too, with their old copies kept in the backup.
    rsync -rct --omit-dir-times --delete-delay --backup \
        --backup-dir="$backup/files/$module" --itemize-changes "${filters[@]}" \
        "$source_apps/$module/" "$destination/$module/" >> "$backup/changes.txt"
    remaining=$(rsync -rcn --delete-delay --itemize-changes "${filters[@]}" \
        "$source_apps/$module/" "$destination/$module/")
    [[ -z "$remaining" ]] || fail "verification differs for $module; see $backup"
done
echo "App sync verified: ${#modules[@]} modules; changes: $backup/changes.txt"
sha256sum "$source_apps/mqtt/src/main.c" "$destination/mqtt/src/main.c"
sha256sum "$source_apps/tk8710app/tk8710_gw" "$destination/tk8710app/tk8710_gw"
