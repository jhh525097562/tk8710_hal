"""Exercise the real UART protocol and atomic GPS file writer using a PTY."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="gw-gps-info-") as directory:
    build = Path(directory)
    gps_dir = build / "GPS"
    compiler = shlex.split(os.environ.get("GPS_INFO_TEST_CC", "gcc"))
    runner = shlex.split(os.environ.get("GPS_INFO_TEST_RUNNER", ""))
    command = compiler + ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-pthread",
        "-DPLATFORM_RK3506", "-D_FILE_OFFSET_BITS=64", "-D_TIME_BITS=64",
        f'-DGW_GPS_INFO_DIR="{gps_dir}"',
        f'-DGW_GPS_INFO_TEST_FILE="{gps_dir / "GPSinfo.txt"}"',
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections"]
    command += ["-I" + str(ROOT / p) for p in
        ["inc", "inc/driver", "inc/trm", "port", "src", "src/gateway"]]
    command += [str(ROOT / p) for p in [
        "test/example/test_tk8710_gw_gps_protocol.c", "src/gateway/tk8710_gw_gps.c",
        "src/tk8710_pps_api.c", "src/trm/trm_pps_monitor.c", "src/trm/trm_log.c"]]
    command += ["-lgpiod", "-o", str(build / "gps-test")]
    subprocess.run(command, check=True)
    subprocess.run(runner + [str(build / "gps-test")], check=True, timeout=30)
