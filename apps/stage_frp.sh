#!/bin/bash
set -euo pipefail
apps=$(cd "$(dirname "$0")" && pwd)
sdk=$(realpath "$apps/../..")
destination="$sdk/device/rockchip/common/extra-parts/userdata/normal/frp"
for name in frpc frpc.toml start_frpc.sh; do
    test -f "$apps/frp/$name"
done
if [ -e "$destination" ]; then
    backup=$(mktemp -d "$sdk/output/frp-stage-backup-XXXXXX")
    cp -a "$destination" "$backup/frp"
    echo "Backup: $backup"
fi
mkdir -p "$destination"
chmod 755 "$destination"
install -m755 "$apps/frp/frpc" "$apps/frp/start_frpc.sh" "$destination/"
install -m600 "$apps/frp/frpc.toml" "$destination/"
bash -n "$destination/start_frpc.sh"
for name in frpc frpc.toml start_frpc.sh; do
    cmp "$apps/frp/$name" "$destination/$name"
done
echo "FRP staged: $destination"
