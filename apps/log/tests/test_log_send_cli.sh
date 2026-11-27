#!/usr/bin/env bash
set -euo pipefail

sleep 0.2

./log_send --proc test_script -l INFO "key=cli_single value=ok"
printf "key=pipe_line1 value=ok\nkey=pipe_line2 value=ok\n" | ./bin/log_send --proc test_pipe -l WARN

sleep 0.6

if ! grep -R "key=cli_single value=ok" ./logs >/dev/null 2>&1; then
  echo "missing cli_single" >&2
  exit 1
fi
if ! grep -R "key=pipe_line1 value=ok" ./logs >/dev/null 2>&1; then
  echo "missing pipe_line1" >&2
  exit 1
fi
if ! grep -R "key=pipe_line2 value=ok" ./logs >/dev/null 2>&1; then
  echo "missing pipe_line2" >&2
  exit 1
fi

echo "ok"

