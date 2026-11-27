"""Generate one MATLAB frame and compare the actual C++ RX and eight-antenna TX.

Exit 0 means exact comparison passes; exit 1 means a completed comparison found
differences; exit 2 means the workflow could not complete. No tolerance is invented.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np

from run_smoke import digest
from tx_comparison_plots import generate_tx_plots


def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False), encoding="utf-8")


def stage_hooks(reference, directory):
    directory.mkdir()
    hooks = (
        ("1.1GWRxCode/GWPHYRx.m",
         b"[GWRxUserInfOut,GWRxUserNum,LostNum,TempUserID,ANoise_REG]",
         b"[GWRxUserInfOut,GWRxUserNum,LostNum,TempUserID,ANoise_REG,SigDownSampleRx]"),
        ("1.2GWTxCode/PHYGWTx.m",
         b"[GW_Tx_MutiAntennaSignal_o,ModeParaDown,PowerTxdBm]",
         b"[GW_Tx_MutiAntennaSignal_o,ModeParaDown,PowerTxdBm,GW_Tx_MutiAntennaSignal,BeamformerMatrix,GWTxSignal]"),
        ("UpLinkChannelModel.m", b"parfor it=1:TxUserNum", b"parfor (it=1:TxUserNum,0)"),
    )
    hashes = {}
    for relative, old, new in hooks:
        source = reference / relative
        content = source.read_bytes()
        if content.count(old) != 1:
            raise ValueError(f"Unexpected reference signature: {source}")
        staged = directory / source.name
        # Mechanical instrumentation: preserve encoding and every algorithm line.
        instrumented = content.replace(old, new, 1)
        if source.name == "PHYGWTx.m":
            loop = b"parfor ii=1:DownlinkUserNum"
            if instrumented.count(loop) != 1:
                raise ValueError("Unexpected gateway TX loop")
            instrumented = instrumented.replace(loop,b"parfor (ii=1:DownlinkUserNum,0)",1)
        staged.write_bytes(instrumented)
        hashes[relative] = {"original": digest(source), "instrumented": digest(staged)}
    return hashes


def load_iq(path, antennas=None):
    values = np.loadtxt(path, ndmin=2)
    if values.shape[1] % 2 or (antennas is not None and values.shape[1] != antennas * 2):
        raise ValueError(f"Invalid I/Q layout: {path}: {values.shape}")
    if not np.isfinite(values).all():
        raise ValueError(f"Nonfinite I/Q: {path}")
    return values[:, 0::2] + 1j * values[:, 1::2]


def compare_iq(reference, actual):
    same_shape = reference.shape == actual.shape
    n = min(len(reference), len(actual))
    if n == 0 or reference.shape[1] != actual.shape[1]:
        raise ValueError("Empty or incompatible waveforms")
    error = actual[:n] - reference[:n]
    power = np.sum(np.abs(reference[:n]) ** 2, axis=0)
    per_antenna = []
    for ant in range(reference.shape[1]):
        mismatches = np.flatnonzero(error[:, ant] != 0)
        per_antenna.append({
            "antenna": ant + 1, "compared_samples": n,
            "exact_mismatch_samples": int(len(mismatches)),
            "first_mismatch_sample_zero_based": int(mismatches[0]) if len(mismatches) else None,
            "max_complex_absolute_error": float(np.max(np.abs(error[:, ant]))),
            "complex_rmse": float(np.sqrt(np.mean(np.abs(error[:, ant]) ** 2))),
            "relative_l2_error": float(np.linalg.norm(error[:, ant]) / np.sqrt(power[ant])) if power[ant] else None,
        })
    return {"reference_shape": list(reference.shape), "actual_shape": list(actual.shape),
            "same_shape": same_shape, "exact_equal": bool(same_shape and np.array_equal(reference, actual)),
            "comparison": "same sample indices, no fitted gain, phase rotation, resampling or delay search",
            "per_antenna": per_antenna}


def run_logged(command, cwd, logfile, timeout):
    with logfile.open("w", encoding="utf-8") as stream:
        start = time.perf_counter()
        done = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, timeout=timeout)
    if done.returncode:
        raise RuntimeError(f"Process exit {done.returncode}: {logfile}\n" +
                           logfile.read_text(encoding="utf-8", errors="replace")[-2500:])
    return time.perf_counter() - start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=Path(
        "D:/Users/HJH/Desktop/HJH/K2301Cmodel/TKG810MatlabPlatform"))
    parser.add_argument("--matlab", type=Path, default=Path("D:/Matlab2021a/bin/matlab.exe"))
    parser.add_argument("--compiler", type=Path, default=Path("C:/Program Files/mingw64/bin/g++.exe"))
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--numeric-mode", choices=("float", "fixed"), default="fixed")
    parser.add_argument("--reuse-generated", type=Path,
                        help="Explicitly reuse an existing MATLAB output folder for C++ diagnostics")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    run_dir = project / "validation" / ("matlab_compare_" + args.numeric_mode + "_" + stamp)
    run_dir.mkdir(parents=True)
    result = {"status": "incomplete", "reference_root": str(args.reference.resolve()),
              "run_dir": str(run_dir), "frames": 1, "numeric_mode": args.numeric_mode}
    print("RUN_DIR=" + str(run_dir), flush=True)
    try:
        if args.reuse_generated:
            matlab_dir = args.reuse_generated.resolve()
            result["reused_matlab_data"] = str(matlab_dir)
        else:
            matlab_dir = run_dir / "matlab"
            matlab_dir.mkdir()
            hooks_dir = run_dir / "matlab_hooks"
            result["hooks"] = stage_hooks(args.reference, hooks_dir)
            result["matlab_source_sha256"] = {
                str(path.relative_to(args.reference)): digest(path)
                for path in args.reference.rglob("*.m")
            }
            result["case_workbook_sha256"] = digest(args.reference / "TestCaseListNew3.xlsx")
            result["driver_sha256"] = digest(project / "testbench_GWRX_TX/matlab_one_frame.m")
            def quote(path):
                return "'" + path.as_posix().replace("'", "''") + "'"
            expression = ("addpath(" + quote(project / "testbench_GWRX_TX") + ");matlab_one_frame(" +
                          ",".join(quote(path.resolve()) for path in (args.reference, matlab_dir, hooks_dir)) + ");")
            command = [str(args.matlab), "-batch", expression]
            result["matlab_command"] = command
            print("MATLAB: generating one frame and reference RX/TX", flush=True)
            result["matlab_seconds"] = run_logged(command, matlab_dir, run_dir / "matlab.log", args.timeout)
            for relative, hashes in result["hooks"].items():
                if digest(args.reference / relative) != hashes["original"]:
                    raise AssertionError("Original MATLAB function changed during validation")
        reference = json.loads((matlab_dir / "matlab_results.json").read_text(encoding="utf-8"))
        parameters = (matlab_dir / "cmodel_args.txt").read_text().split()
        if len(parameters) != 91:
            raise AssertionError("MATLAB did not export 91 case parameters")
        result["matlab_metadata"] = reference
        input_file = matlab_dir / "Cmodel_DataIn/case95/UD_InData0.txt"
        result["input_sha256"] = digest(input_file)
        result["tx_bits_sha256"] = digest(matlab_dir / "tx_bits.txt")
        output = run_dir / "cmodel"
        for module in ("LostRate", "UDModule_Data", "BFModule_Data",
                       "K2201SyncModule_Data", "K2201DemodModule_Data", "GWTXModule"):
            (output / module / "case95").mkdir(parents=True)
        exe = run_dir / "GWRXTX_compare.exe"
        source = project / "testbench_GWRX_TX/GWRXTX_testbench.cpp"
        build = [str(args.compiler), "-std=c++11", "-O1", "-g", "-Wformat=2",
                 "-Wno-write-strings", "-DDUMP_MATLAB_tx_beamforming",
                 "-Wl,--stack,67108864", str(source), "-o", str(exe)]
        if args.numeric_mode == "fixed":
            build.insert(1, "-DCMODEL_FIXED_POINT")
        else:
            build.insert(1, "-DCMODEL_FLOAT_POINT")
        result["build_command"] = build
        result["cmodel_source_sha256"] = {str(path.relative_to(project)): digest(path)
            for path in [*project.glob("*.h"), source, project / "testbench_GWRX_TX/testbench_options.h"]}
        print("MATLAB_OK; building C++", flush=True)
        run_logged(build, output, run_dir / "build.log", args.timeout)
        command = [str(exe), *parameters, "--input-root", str(matlab_dir),
                   "--output-root", str(output), "--frames", "1",
                   "--tx-bits-file", str(matlab_dir / "tx_bits.txt")]
        result["cmodel_command"] = command
        result["cmodel_seconds"] = run_logged(command, output, run_dir / "cmodel.log", args.timeout)
        print((run_dir / "cmodel.log").read_text()[-1600:], flush=True)
        rx_data = np.loadtxt(output / "K2201DemodModule_Data/case95/rx_frame0.txt", ndmin=2)
        payload = np.asarray(reference["rx_payload"], dtype=np.int64).ravel()
        if reference["rx_user_count"] != 1 or len(rx_data) != 1:
            raise AssertionError("This fixture requires exactly one received user in both implementations")
        c_bytes = rx_data[0, 20:].astype(np.int64)
        payload_size = len(payload)
        # C++ exports the full PHY bytes; MATLAB excludes the two trailer bytes.
        if len(c_bytes) != payload_size + 2:
            raise AssertionError("Unexpected RX PHY/payload byte layout")
        log = (run_dir / "cmodel.log").read_text()
        if "NUMERIC_MODE=" + args.numeric_mode not in log:
            raise AssertionError("Executable numeric mode does not match the requested mode")
        result["rx"] = {"payload_equal": bool(np.array_equal(c_bytes[:payload_size], payload)),
                        "matlab_payload": payload.tolist(), "cmodel_phy_bytes": c_bytes.tolist(),
                        "payload_bytes": payload_size, "trailer_bytes_excluded": 2,
                        "completed_crc_clean": "segments=1 crc_errors=0" in log,
                        "matlab_frequency": reference["rx_frequency"], "cmodel_frequency": float(rx_data[0, 2]),
                        "frequency_error": float(rx_data[0, 2] - reference["rx_frequency"])}
        result["rx"]["pilot_power"] = {
            "matlab": reference["rx_pilot_power"], "cmodel": float(rx_data[0, 3]),
            "absolute_error": float(abs(rx_data[0, 3] - reference["rx_pilot_power"]))}
        ah_reference = np.asarray(reference["rx_ah_i"]).ravel() + 1j*np.asarray(reference["rx_ah_q"]).ravel()
        ah_actual = rx_data[:, 4:20:2] + 1j*rx_data[:, 5:20:2]
        result["rx_ah"] = compare_iq(ah_reference.reshape(1,8), ah_actual)
        tx_reference = load_iq(matlab_dir / "matlab_tx_8ant_baseband.txt", 8)
        tx_actual = load_iq(output / "GWTXModule/case95/tx_frame0.txt", 8)
        result["tx_raw"] = compare_iq(tx_reference, tx_actual)
        result["tx_canonical"] = compare_iq(tx_reference, tx_actual * reference["cmodel_tx_scale"])
        result["tx_canonical"]["cmodel_scale"] = reference["cmodel_tx_scale"]
        result["tx_weights"] = compare_iq(load_iq(matlab_dir / "matlab_tx_weights.txt",8),
                                         load_iq(output / "GWTXModule/case95/weights_frame0.txt",8))
        restored_channel = np.loadtxt(output / "GWTXModule/case95/h_anoise.txt", ndmin=2)
        # The legacy fixed-point assignment saturates; fix_set(double, ...) is a no-op.
        limit = 2**11 - 2**-12
        result["tx_channel_saturation"] = {
            "applicable": args.numeric_mode == "fixed",
            "format": "signed wl=24 iwl=12 FXP_SAT_SYM" if args.numeric_mode == "fixed" else "double",
            "component_limit": limit if args.numeric_mode == "fixed" else None,
            "dump_significant_digits": 6,
            "components_at_limit_with_dump_rounding": int(np.count_nonzero(np.isclose(np.abs(restored_channel), limit, rtol=0, atol=0.005))) if args.numeric_mode == "fixed" else None,
            "channel_iq": restored_channel.tolist()}
        result["waveform_sha256"] = {
            "matlab_baseband": digest(matlab_dir / "matlab_tx_8ant_baseband.txt"),
            "matlab_rf": digest(matlab_dir / "matlab_tx_8ant_rf.txt"),
            "cmodel": digest(output / "GWTXModule/case95/tx_frame0.txt")}
        exact = result["rx"]["payload_equal"] and result["rx"]["completed_crc_clean"] and result["tx_canonical"]["exact_equal"]
        result["plots"] = generate_tx_plots(matlab_dir / "matlab_tx_8ant_baseband.txt",
            output / "GWTXModule/case95/tx_frame0.txt",run_dir,mode=args.numeric_mode,
            scale=reference["cmodel_tx_scale"],sample_rate=reference["tx_baseband_rate"],context="MATLAB 单帧收发对比")
        result["status"] = "exact_match" if exact else "completed_with_differences"
        print("RX payload equal:", result["rx"]["payload_equal"], flush=True)
        print("TX canonical exact equal:", result["tx_canonical"]["exact_equal"], flush=True)
        for row in result["tx_canonical"]["per_antenna"]:
            print(row, flush=True)
        return 0 if exact else 1
    except Exception as exc:
        result.update(status="workflow_failed", error=str(exc))
        print("ERROR: " + str(exc), file=sys.stderr, flush=True)
        return 2
    finally:
        save_json(run_dir / "comparison.json", result)


if __name__ == "__main__":
    sys.exit(main())
