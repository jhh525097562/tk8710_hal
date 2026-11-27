"""Build the selected RX/TX entry and validate one isolated case95 frame.

This checks a real RX decode followed by a finite, nonzero TX waveform.
It is not a MATLAB/RTL equivalence or packet-loss acceptance test.
"""
import argparse
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def check_rejected_inputs(exe, row, run_dir):
    negative = run_dir / "negative_checks"
    negative.mkdir()
    for module in ("LostRate", "UDModule_Data", "BFModule_Data",
                   "K2201SyncModule_Data", "K2201DemodModule_Data", "GWTXModule"):
        (negative / module / "case95").mkdir(parents=True)
    missing_input = [*row, "--input-root", str(negative / "no_input"),
                     "--output-root", str(negative), "--frames", "1"]
    bad_integer = row.copy()
    bad_integer[2] = "3.5"
    checks = {}
    for label, params, expected, marker in (
            ("missing_arguments", [], 2, "Usage:"),
            ("fractional_fft_index", bad_integer, 2, "Invalid integer parameter 3"),
            ("missing_input", missing_input, 3, "Cannot open input:")):
        completed = subprocess.run([str(exe), *params], cwd=negative, capture_output=True,
                                   stdin=subprocess.DEVNULL, text=True, timeout=30)
        text = completed.stdout + completed.stderr
        (negative / (label + ".log")).write_text(text, encoding="utf-8")
        checks[label] = {"returncode": completed.returncode,
                         "passed": completed.returncode == expected and marker in text}
        if not checks[label]["passed"]:
            raise AssertionError(f"{label}: expected exit {expected}; got {completed.returncode}: {text}")
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-root", type=Path,
                        default=Path("D:/data/K2301/K2301PHY_CaseList"))
    parser.add_argument("--compiler", type=Path,
                        default=Path("C:/Program Files/mingw64/bin/g++.exe"))
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--numeric-mode", choices=("float", "fixed"), default="fixed")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    run_dir = project / "validation" / ("smoke_case95_" + stamp)
    run_dir.mkdir(parents=True, exist_ok=False)
    result = {"status": "incomplete", "run_dir": str(run_dir), "numeric_mode": args.numeric_mode,
              "scope": "one frame, RX capacity 1, zero TX test payload; no MATLAB/RTL comparison"}
    print("RUN_DIR=" + str(run_dir), flush=True)
    try:
        case_table = project / "testbench_GWRX_TX/GWRXCaseList.csv"
        row = next(line.split() for line in case_table.read_text().splitlines()
                   if line.split() and line.split()[0] == "95")
        if len(row) != 91:
            raise ValueError(f"case95 requires 91 values, got {len(row)}")
        row[14] = "1"  # RX capacity only; preserve the original waveform and other fields.
        input_file = args.input_root.resolve() / "Cmodel_DataIn/case95/UD_InData0.txt"
        if not input_file.is_file():
            raise FileNotFoundError(input_file)
        result["input"] = {"path": str(input_file), "sha256": digest(input_file)}
        input_values = input_file.read_text(encoding="ascii").split()
        expected_values = 8 * (1024 << int(row[2])) * int(row[3]) * 2
        if len(input_values) != expected_values or not all(math.isfinite(float(v)) for v in input_values):
            raise ValueError("Input frame length or numeric content is invalid")
        result["input"]["scalar_count"] = len(input_values)
        del input_values
        for module in ("LostRate", "UDModule_Data", "BFModule_Data",
                       "K2201SyncModule_Data", "K2201DemodModule_Data", "GWTXModule"):
            (run_dir / module / "case95").mkdir(parents=True)
        exe = run_dir / "GWRXTX_smoke.exe"
        source = project / "testbench_GWRX_TX/GWRXTX_testbench.cpp"
        build = [str(args.compiler), "-std=c++11", "-O1", "-g", "-Wformat=2",
                 "-Wno-write-strings", "-Wl,--stack,67108864", str(source), "-o", str(exe)]
        if args.numeric_mode == "fixed":
            build.insert(1, "-DCMODEL_FIXED_POINT")
        else:
            build.insert(1, "-DCMODEL_FLOAT_POINT")
        result["build_command"] = build
        result["source_sha256"] = {
            str(path.relative_to(project)): digest(path)
            for path in list(project.glob("*.h")) +
            [source, project / "testbench_GWRX_TX/testbench_options.h", Path(__file__).resolve()]
        }
        with (run_dir / "build.log").open("w", encoding="utf-8") as log:
            subprocess.run(build, cwd=run_dir, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=args.timeout)
        result["build_passed"] = True
        result["executable_sha256"] = digest(exe)
        result["negative_checks"] = check_rejected_inputs(exe, row, run_dir)
        command = [str(exe), *row, "--input-root", str(args.input_root.resolve()),
                   "--output-root", str(run_dir), "--frames", "1"]
        result["run_command"] = command
        result["working_directory"] = str(run_dir)
        (run_dir / "command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
        print("BUILD_OK; running 1 frame with RX capacity 1", flush=True)
        start = time.perf_counter()
        with (run_dir / "run.log").open("w", encoding="utf-8") as log:
            completed = subprocess.run(command, cwd=run_dir, stdout=log, stderr=subprocess.STDOUT,
                                       stdin=subprocess.DEVNULL, timeout=args.timeout)
        result["runtime_seconds"] = time.perf_counter() - start
        result["returncode"] = completed.returncode
        log_text = (run_dir / "run.log").read_text(encoding="utf-8", errors="replace")
        print(log_text[-5000:], flush=True)
        if completed.returncode != 0:
            raise RuntimeError(f"RX/TX process exited with {completed.returncode}")
        if "NUMERIC_MODE=" + args.numeric_mode not in log_text:
            raise AssertionError("Executable numeric mode does not match the requested mode")
        rx = re.findall(r"FreamIndex:(\d+) RxUserNum:(\d+)", log_text)
        tx = re.findall(r"TX_FRAME frame=(\d+) users=(\d+) samples=(\d+) energy=([^\s]+)", log_text)
        if rx != [("0", "1")] or len(tx) != 1 or tx[0][:2] != ("0", "1"):
            raise AssertionError(f"Expected one decoded and transmitted user: rx={rx}, tx={tx}")
        if not re.search(r"RX_DECODE frame=0 .*segments=1 crc_errors=0 bytes=[1-9]\d*", log_text):
            raise AssertionError("No completed CRC-clean RX decode")
        if "RxFinished" not in log_text:
            raise AssertionError("Missing completion marker")
        waveform = run_dir / "GWTXModule/case95/tx_frame0.txt"
        samples, energy = 0, 0.0
        with waveform.open(encoding="ascii") as stream:
            for line in stream:
                values = [float(value) for value in line.split()]
                if len(values) != 16 or not all(math.isfinite(value) for value in values):
                    raise AssertionError(f"Invalid waveform row {samples}")
                energy += sum(value * value for value in values)
                samples += 1
        if samples != int(tx[0][2]) or not math.isfinite(energy) or energy <= 0:
            raise AssertionError("TX waveform is empty, truncated or zero")
        result.update(status="passed", rx_users=1, tx_users=1, tx_samples_per_antenna=samples,
                      tx_energy=energy, waveform_sha256=digest(waveform))
        print(f"PASS: RX=1, TX=1, samples/antenna={samples}, energy={energy:.12g}")
        return 0
    except Exception as exc:
        result.update(status="failed", error=str(exc))
        print(f"FAIL: {exc}; see {run_dir}", file=sys.stderr)
        return 1
    finally:
        (run_dir / "result.json").write_text(
            json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
