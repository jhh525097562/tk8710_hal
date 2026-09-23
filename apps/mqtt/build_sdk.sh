#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
SDK_ROOT=${SDK_ROOT:-/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0}
HOST="$SDK_ROOT/buildroot/output/rockchip_hd_rk3506b_iot_emmc/host"
COMPILER="$HOST/bin/arm-buildroot-linux-gnueabihf-gcc"
SYSROOT="$HOST/arm-buildroot-linux-gnueabihf/sysroot"
mkdir -p build_arm
"$COMPILER" --sysroot="$SYSROOT" -std=gnu11 -Wall -Wextra -Werror -O2 -Iinc -I../common/inc \
  -ffunction-sections -fdata-sections tests/test_ns_gps_config.c src/mac_msg_parser.c \
  ../common/src/file_logger.c src/gateway_telemetry.c -Wl,--gc-sections \
  -ljson-c -lmbedcrypto -lpthread -o build_arm/test_ns_gps_config
"$COMPILER" --sysroot="$SYSROOT" -std=gnu11 -Wall -Wextra -O2 -g -Iinc -I../common/inc \
  src/*.c ../common/src/file_logger.c -Linc -Wl,-rpath-link,"$SYSROOT/usr/lib:$SYSROOT/lib" \
  -lmosquitto -ljson-c -lipc_smp -lmbedcrypto -lgpiod -lpthread -lrt -lm \
  -o build_arm/mqtt_bridge
"$COMPILER" --sysroot="$SYSROOT" -std=gnu11 -Wall -Wextra -Werror -O2 -Iinc \
  tests/test_gateway_telemetry.c src/gateway_telemetry.c -ljson-c \
  -o build_arm/test_gateway_telemetry
file build_arm/mqtt_bridge build_arm/test_gateway_telemetry
"$COMPILER" --sysroot="$SYSROOT" -std=gnu11 -Wall -Wextra -Werror -O2 -Iinc \
  tests/test_ns_scan_protocol.c src/ns_scan_protocol.c -ljson-c -lm \
  -o build_arm/test_ns_scan_protocol
"$COMPILER" --sysroot="$SYSROOT" -std=gnu11 -Wall -Wextra -Werror -O2 -Iinc \
  tests/test_ns_scan_manager.c src/ns_scan_manager.c src/ns_scan_protocol.c -ljson-c -lpthread -lm \
  -o build_arm/test_ns_scan_manager
