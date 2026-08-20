#!/bin/sh

# The RK3506 test binary resolves TXADC calibration data relative to its
# working directory.  The test skill intentionally gives every run an
# isolated directory, so expose the board's existing read-only TXDC data in
# that directory while keeping generated logs and calibration factors isolated.
if [ ! -e TxDC ]; then
    ln -s /userdata/TxDC TxDC || exit 125
fi

exec /userdata/TestTRMmain "$@"
