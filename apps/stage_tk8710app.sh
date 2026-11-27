#!/bin/bash
# Stage every file from the maintained TK8710 application bundle into userdata.
set -euo pipefail

cd "$(dirname "$0")"
apps=$PWD
sdk=$(realpath ../..)
userdata="$sdk/device/rockchip/common/extra-parts/userdata/normal"
default_source="$apps/tk8710app"
source_dir=${TK8710_APP_SOURCE:-$default_source}

if [[ ! -d "$source_dir" ]]; then
    source_dir="$apps/tk8710app"
    echo "TK8710 Windows source unavailable; using SDK bundle: $source_dir"
fi
[[ -d "$source_dir" ]] || { echo "TK8710 app source missing: $source_dir" >&2; exit 1; }
[[ -f "$source_dir/tk8710_gw" ]] || { echo "Missing tk8710_gw in $source_dir" >&2; exit 1; }
[[ -d "$source_dir/class3" ]] || { echo "Missing class3 in $source_dir" >&2; exit 1; }

if find "$source_dir" -type l -print -quit | grep -q .; then
    echo "Symlinks are not allowed in TK8710 app source" >&2
    exit 1
fi
if find "$source_dir" ! -type f ! -type d -print -quit | grep -q .; then
    echo "Special files are not allowed in TK8710 app source" >&2
    exit 1
fi

tmp=$(mktemp -d "$sdk/output/tk8710-app-stage-XXXXXX")
cleanup() {
    case "$tmp" in
        "$sdk/output"/tk8710-app-stage-*) rm -rf -- "$tmp" ;;
        *) echo "Refusing to remove unexpected temporary path: $tmp" >&2 ;;
    esac
}
trap cleanup EXIT
cp -a "$source_dir/." "$tmp/"

for name in tk8710_gw Test8710Loopback Test8710RFTxTone Test8710RxSensitivity Test8710W_R_Reg; do
    [[ -f "$tmp/$name" ]] || { echo "Missing required ARM executable: $name" >&2; exit 1; }
    file "$tmp/$name" | grep -q 'ELF 32-bit LSB.*ARM' || {
        echo "Not an RK3506 32-bit ARM executable: $name" >&2
        exit 1
    }
done

find "$tmp" -type d -exec chmod 755 {} +
find "$tmp" -type f -exec chmod 644 {} +
chmod 755 "$tmp/tk8710_gw" "$tmp"/Test8710*

backup=$(mktemp -d "$sdk/output/tk8710-userdata-backup-XXXXXX")
while IFS= read -r -d '' entry; do
    name=${entry#"$tmp/"}
    [[ "$name" != */* && "$name" != "." && "$name" != ".." ]] || {
        echo "Invalid top-level TK8710 entry: $name" >&2
        exit 1
    }
    [[ ! -e "$userdata/$name" ]] || cp -a "$userdata/$name" "$backup/"
done < <(find "$tmp" -mindepth 1 -maxdepth 1 -print0)

cp -a "$tmp/." "$userdata/"

file_count=0
while IFS= read -r -d '' relative; do
    relative=${relative#./}
    cmp -s "$tmp/$relative" "$userdata/$relative" || {
        echo "Staged TK8710 file differs: $relative" >&2
        exit 1
    }
    file_count=$((file_count + 1))
done < <(cd "$tmp" && find . -type f -print0)

echo "TK8710 app source: $source_dir"
echo "TK8710 files staged: $file_count"
echo "TK8710 userdata backup: $backup"
sha256sum "$source_dir/tk8710_gw" "$userdata/tk8710_gw"
