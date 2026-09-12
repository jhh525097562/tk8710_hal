#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PORT=${1:-/dev/ttyS4}
IMAGE=${2:-${SCRIPT_DIR}/firmware/ag32_gps_pps_app_20260902.bin}
MODE=${3:-upgrade}
TOOL=${SCRIPT_DIR}/ag32_boot_tool.py
IMAGE_VERSION=0x20260902
EXPECTED_SHA256=848ae2b2f9eebce1dcf8ab4733c7a5d7bb7ae3d87243a290ec6cab8dc80a5720
if [ "${MODE}" = "--verify-only" ]; then
    MODE_TAG=verify
else
    MODE_TAG=upgrade
fi
LOG=${SCRIPT_DIR}/upgrade_ag32_app_20260902_${MODE_TAG}_$(date '+%Y%m%d_%H%M%S').log

exec >"${LOG}" 2>&1

echo "AG32 APP upgrade started: $(date '+%Y-%m-%d %H:%M:%S')"
echo "port=${PORT}"
echo "image=${IMAGE}"
echo "image_version=${IMAGE_VERSION}"
echo "mode=${MODE}"

if [ ! -c "${PORT}" ]; then
    echo "ERROR: serial port is not a character device: ${PORT}"
    exit 2
fi
if [ ! -f "${TOOL}" ]; then
    echo "ERROR: upgrade tool not found: ${TOOL}"
    exit 2
fi
if [ ! -s "${IMAGE}" ]; then
    echo "ERROR: APP image is missing or empty: ${IMAGE}"
    exit 2
fi
if ! python3 -c 'import serial' >/dev/null 2>&1; then
    echo "ERROR: Python dependency pyserial is not installed"
    exit 2
fi

ACTUAL_SHA256=$(sha256sum "${IMAGE}" | awk '{print $1}')
echo "image_sha256=${ACTUAL_SHA256}"
if [ "${ACTUAL_SHA256}" != "${EXPECTED_SHA256}" ]; then
    echo "ERROR: APP image SHA-256 mismatch"
    exit 2
fi

if [ "${MODE}" != "--verify-only" ]; then
    echo "[1/3] Entering BOOT and checking capabilities"
    python3 "${TOOL}" --port "${PORT}" info --enter-boot

    echo "[2/3] Programming APP image"
    # INFO leaves the device in BOOT, so do not send ENTER BOOT twice.
    python3 "${TOOL}" --port "${PORT}" upgrade-app \
        --bin "${IMAGE}" \
        --image-version "${IMAGE_VERSION}"
fi

echo "[3/3] Waiting for APP and verifying version/status"
sleep 5
python3 - "${PORT}" <<'PY'
import sys
import time

import serial

port = sys.argv[1]
with serial.Serial(port, 115200, timeout=0.2) as uart:
    for command, expected in (
        ("GET VERSION", "OK APP_VER="),
        ("GET STATUS", "OK STATUS "),
    ):
        for attempt in range(1, 6):
            uart.reset_input_buffer()
            uart.write((command + "\n").encode("ascii"))
            uart.flush()
            deadline = time.monotonic() + 5.0
            while time.monotonic() < deadline:
                line = uart.readline().decode("ascii", errors="replace").strip()
                if not line:
                    continue
                print(line)
                if line.startswith("ERR"):
                    raise SystemExit("ERROR: APP rejected " + command)
                if line.startswith(expected):
                    break
            else:
                print(f"VERIFY retry command={command!r} attempt={attempt}")
                time.sleep(1.0)
                continue
            break
        else:
            raise SystemExit("ERROR: timeout waiting for " + command)
PY

echo "AG32 APP upgrade completed: $(date '+%Y-%m-%d %H:%M:%S')"
