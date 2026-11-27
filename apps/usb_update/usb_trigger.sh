#!/bin/bash

DEVNAME="$1"

# 简单日志（调试用）
echo "[trigger] device: $DEVNAME" > /dev/console

# 丢到后台执行（关键点）
/usr/bin/usb_update_1.sh "$DEVNAME" &
