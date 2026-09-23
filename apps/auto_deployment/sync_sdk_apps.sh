#!/bin/bash
set -euo pipefail
source_apps=${1:?source app directory required}
sdk=${2:-/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0}
destination="$sdk/external/rk3506app"
backup="$sdk/output/app-sync-backup-$(date +%Y%m%d-%H%M%S)"
test -d "$destination"
mkdir -p "$backup"
cp -a "$destination" "$backup/rk3506app"
overlay="$sdk/buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay"
cp -a "$overlay/etc/tdaemon.conf" "$backup/overlay-tdaemon.conf"
cp -a "$overlay/usr/bin/tdaemon_proc.sh" "$backup/overlay-tdaemon_proc.sh"
for module in Discovery SMP ipcweb light log mqtt tdaemon webserver common third_party frp; do
    test -d "$source_apps/$module"
    mkdir -p "$destination/$module"
    rsync -rc --exclude='.git/' --exclude='.trae/' --exclude='build/' \
      --exclude='build_*/' --exclude='bin/' --exclude='backup/' --exclude='docs/' \
      --exclude='__pycache__/' --exclude='*.log' --exclude='*.o' --exclude='core' \
      "$source_apps/$module/" "$destination/$module/"
done
printf '%s\n' "$backup"
