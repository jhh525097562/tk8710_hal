#!/bin/bash

DEVNAME="$1"

# 简单日志（调试用）
echo "[trigger] device: $DEVNAME" > /dev/console

# 关键修复：彻底脱离 udev worker，避免阻塞后续自动挂载流程
if command -v setsid >/dev/null 2>&1; then
    setsid /usr/bin/usb_update.sh "$DEVNAME" </dev/null >/dev/null 2>&1 &
else
    nohup /usr/bin/usb_update.sh "$DEVNAME" </dev/null >/dev/null 2>&1 &
fi

exit 0
