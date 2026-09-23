#!/bin/bash


LOG_FILE="/tmp/usb_update.log"

# ===== 日志同时输出到 console + 文件 =====
exec > >(tee -a "$LOG_FILE" /dev/console) 2>&1
# --- 逻辑区 ---
echo "--- USB 升级检测任务启动-0 ---"
sleep 3


echo "--- USB 升级检测任务启动-1 ---"
sleep 3

echo "--- USB 升级检测任务启动-2 ---"
sleep 3
