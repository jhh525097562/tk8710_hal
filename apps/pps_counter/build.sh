#!/bin/sh
set -eu
: "${SDK_ROOT:=/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0}"
cd "$(dirname "$0")"
mkdir -p build_arm
"$SDK_ROOT/buildroot/output/rockchip_hd_rk3506b_iot_emmc/host/bin/arm-buildroot-linux-gnueabihf-gcc" \
    -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 \
    -std=c11 -O2 -Wall -Wextra -Werror pps_counter.c -lgpiod -o build_arm/pps_counter
