#!/bin/sh
set -eu
module_dir=${1:?module directory required}
. "$module_dir/tdaemon_common.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/tools"
ln -s "$(command -v sha256sum)" "$work/tools/sha256sum"
ln -s "$(command -v cksum)" "$work/tools/cksum"
printf '{"mqtt_port":1883}\n' > "$work/config.json"
cp -p "$work/config.json" "$work/reference"
original_path=$PATH
PATH="$work/tools"
first=$(get_config_fingerprint "$work/config.json")
test "$first" = "$(get_config_fingerprint "$work/config.json")"
printf '{"mqtt_port":1884}\n' > "$work/config.json"
PATH=$original_path touch -r "$work/reference" "$work/config.json"
second=$(get_config_fingerprint "$work/config.json")
test "$first" != "$second"
printf '{"mqtt_port":1883}\n' > "$work/config.json"
test "$first" = "$(get_config_fingerprint "$work/config.json")"
if get_config_fingerprint "$work/missing"; then exit 1; fi

# Exercise the real daemon-config reload entry with no stat/ls/awk installed.
reloads=0
cfg_parse_file() { reloads=$((reloads + 1)); }
cfg_apply_new() { :; }
log() { :; }
DAEMON_LOG_FILE=/dev/null
ACTIVE_CFG_FINGERPRINT=$first
cfg_reload_if_changed "$work/config.json"
test "$reloads" = 0
printf '{"mqtt_port":1885}\n' > "$work/config.json"
cfg_reload_if_changed "$work/config.json"
test "$reloads" = 1
cfg_reload_if_changed "$work/config.json"
test "$reloads" = 1

PATH=$original_path
if [ "${2:-}" = "--basic" ]; then
    echo "PASS BusyBox: changed/unchanged content and reload once"
    exit 0
fi
rm "$work/tools/sha256sum"
PATH="$work/tools"
first=$(get_config_fingerprint "$work/config.json")
case "$first" in cksum:*) ;; *) exit 1;; esac
printf '{"mqtt_port":1886}\n' > "$work/config.json"
test "$first" != "$(get_config_fingerprint "$work/config.json")"
PATH=$original_path
rm "$work/tools/cksum"
PATH="$work/tools"
if get_config_fingerprint "$work/config.json" 2>/dev/null; then exit 1; fi
PATH=$original_path
echo "PASS: no-stat SHA256 detection, preserved mtime, identical content, reload once, missing file, cksum fallback, missing tools"
