#!/usr/bin/env python3
"""Run the 40-sample cases 4/5/6 campaign, then a fixed case-5 long run."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


def _write_status(path: Path, payload: dict[str, Any]) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def build_phase1_command(args: argparse.Namespace, script_dir: Path) -> list[str]:
    return [
        sys.executable,
        str(script_dir / "run_cases_5_6.py"),
        "--config", str(args.config),
        "--cases", "4,5,6",
        "--seed", str(args.seed),
        "--sample-count", str(args.sample_count),
        "--output-root", str(args.session_dir / "phase1_cases_4_5_6"),
    ]


def build_phase2_command(args: argparse.Namespace, script_dir: Path) -> list[str]:
    return [
        sys.executable,
        str(script_dir / "fixed_config_sync_test.py"),
        "--config", str(args.config),
        "--rate", str(args.rate),
        "--tdd-num", str(args.tdd_num),
        "--uplink-length", str(args.uplink_length),
        "--downlink-length", str(args.downlink_length),
        "--packet-count", str(args.packet_count),
        "--max-send-attempts", str(args.max_send_attempts),
        "--cfo-jump-threshold", str(args.cfo_jump_threshold),
        "--location-description", args.location_description,
        "--output-root", str(args.session_dir / "phase2_case5_rate8_long_run"),
    ]


def _run_phase(status_path: Path, status: dict[str, Any], name: str,
               command: list[str], log_path: Path) -> int:
    record: dict[str, Any] = {
        "name": name,
        "status": "running",
        "started_at": datetime.now().isoformat(timespec="seconds"),
        "command": command,
        "log": str(log_path),
    }
    status["phases"].append(record)
    _write_status(status_path, status)
    print(f"{name} started; log={log_path}", flush=True)
    try:
        with log_path.open("a", encoding="utf-8") as target:
            exit_code = subprocess.call(
                command, stdout=target, stderr=subprocess.STDOUT)
    except Exception as exc:
        exit_code = 125
        record["error"] = f"{type(exc).__name__}: {exc}"
    record.update({
        "status": "finished",
        "finished_at": datetime.now().isoformat(timespec="seconds"),
        "exit_code": exit_code,
    })
    _write_status(status_path, status)
    print(f"{name} finished; exit_code={exit_code}", flush=True)
    return exit_code


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260909)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--rate", type=int, default=8)
    parser.add_argument("--tdd-num", type=int, default=20)
    parser.add_argument("--uplink-length", type=int, default=50)
    parser.add_argument("--downlink-length", type=int, default=50)
    parser.add_argument("--packet-count", type=int, default=1000)
    parser.add_argument("--max-send-attempts", type=int, default=3)
    parser.add_argument("--cfo-jump-threshold", type=int, default=500)
    parser.add_argument(
        "--location-description", default="重叠覆盖区域-用例5模式8-1000帧")
    parser.add_argument(
        "--output-root", type=Path,
        default=Path("sync_network_results") / "full40_then_case5_rate8_1000")
    args = parser.parse_args()
    if args.sample_count <= 0 or args.packet_count <= 0:
        parser.error("--sample-count and --packet-count must be greater than zero")
    return args


def main() -> int:
    args = parse_args()
    args.config = args.config.resolve()
    args.output_root = args.output_root.resolve()
    args.session_dir = (
        args.output_root / datetime.now().strftime("full_campaign_%Y%m%d_%H%M%S"))
    args.session_dir.mkdir(parents=True, exist_ok=False)
    status_path = args.session_dir / "campaign_status.json"
    status: dict[str, Any] = {
        "status": "running",
        "started_at": datetime.now().isoformat(timespec="seconds"),
        "session_dir": str(args.session_dir),
        "phases": [],
    }
    _write_status(status_path, status)

    script_dir = Path(__file__).resolve().parent
    exit_codes = [
        _run_phase(status_path, status, "cases_4_5_6_sample40",
                   build_phase1_command(args, script_dir),
                   args.session_dir / "phase1.log"),
        _run_phase(status_path, status, "case5_rate8_1000_packets",
                   build_phase2_command(args, script_dir),
                   args.session_dir / "phase2.log"),
    ]
    status.update({
        "status": "finished",
        "finished_at": datetime.now().isoformat(timespec="seconds"),
        "exit_code": 0 if all(code == 0 for code in exit_codes) else 1,
    })
    _write_status(status_path, status)
    return int(status["exit_code"])


if __name__ == "__main__":
    raise SystemExit(main())
