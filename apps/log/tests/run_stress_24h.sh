#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

make all
LOG_UDP_ADDR=127.0.0.1 LOG_UDP_PORT=5514 LOG_SEND_THRESHOLD=256 ./bin/stress_test 20 86400
