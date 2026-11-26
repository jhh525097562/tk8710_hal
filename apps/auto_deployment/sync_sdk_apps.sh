#!/bin/bash
# Keep the historical entrypoint on the same synchronization policy.
set -euo pipefail
exec bash "$(dirname "$0")/../sync_apps.sh" "$@"
