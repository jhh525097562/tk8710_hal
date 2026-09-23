#!/bin/bash
# Run the copy inside SDK/external/rk3506app/swupdattool/gateway/.
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
sdk=$(cd "$here/../../../.." && pwd)
release=${1:-}
case "$release" in ''|*[!A-Za-z0-9_.-]*) echo 'Usage: make_app_swu.sh RELEASE [ARM_TK8710_BINARY]' >&2; exit 2;; esac
[[ ${#release} -le 48 && -f "$sdk/build.sh" && -d "$sdk/external/rk3506app" ]] || {
    echo 'Run this script from its copy inside the SDK; release length must be <=48.' >&2; exit 2;
}
output="$sdk/output/app-updates/gateway-apps-$release.swu"
[[ ! -e "$output" ]] || { echo "Already exists: $output" >&2; exit 2; }
if [[ -n ${2:-} ]]; then
    [[ -f $2 ]] || { echo "TK8710 binary missing: $2" >&2; exit 2; }
    file "$2" | grep -q 'ELF 32-bit LSB.*ARM' || { echo 'Not a 32-bit ARM executable' >&2; exit 2; }
fi
bash "$sdk/external/rk3506app/build_apps.sh" --stage
if [[ -n ${2:-} ]]; then
    stage="$sdk/device/rockchip/common/extra-parts/userdata/normal"
    backup=$(mktemp -d "$sdk/output/tk8710-swu-backup-XXXXXX")
    [[ ! -f "$stage/tk8710_gw" ]] || cp -p "$stage/tk8710_gw" "$backup/"
    install -m755 "$2" "$stage/tk8710_gw"
    echo "Previous TK8710 backup: $backup"
else
    echo 'Using tk8710_gw automatically staged from rk3506app/tk8710app.'
fi
python3 "$here/build_swu.py" --sdk "$sdk" --version "$release" --output "$output"
