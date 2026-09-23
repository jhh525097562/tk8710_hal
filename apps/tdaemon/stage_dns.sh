#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
sdk=$(cd "$here/../../.." && pwd)
overlay="$sdk/buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay"
test -f "$sdk/build.sh"
install -D -m755 "$here/tdaemon_dns.sh" "$overlay/usr/bin/tdaemon_dns.sh"
install -D -m644 "$here/dns-hooks/tdaemon-dns-owner" "$overlay/etc/tdaemon-dns-owner"
install -D -m755 "$here/dns-hooks/default.script" "$overlay/usr/share/udhcpc/default.script"
install -D -m644 "$here/dns-hooks/20-resolv.conf" "$overlay/usr/lib/dhcpcd/dhcpcd-hooks/20-resolv.conf"
