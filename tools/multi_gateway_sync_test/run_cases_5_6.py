#!/usr/bin/env python3
"""Run selected hardware synchronization cases sequentially."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def _write_status(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def _summarize_run(run_dir: Path | None) -> dict:
    if run_dir is None:
        return {
            "total_combinations": 0,
            "passed_combinations": 0,
            "failed_combinations": 0,
            "combination_success_rate": 0.0,
            "passed_packets": 0,
            "expected_packets": 0,
            "packet_success_rate": 0.0,
        }
    summary_path = run_dir / "summary.json"
    if not summary_path.exists():
        return _summarize_run(None)
    records = json.loads(summary_path.read_text(encoding="utf-8")).get("records", [])
    total = len(records)
    passed = sum(bool(item.get("passed")) for item in records)
    passed_packets = sum(int(item.get("passed_packet_count", 0)) for item in records)
    expected_packets = sum(int(item.get("expected_packet_count", 0)) for item in records)
    return {
        "total_combinations": total,
        "passed_combinations": passed,
        "failed_combinations": total - passed,
        "combination_success_rate": passed / total if total else 0.0,
        "passed_packets": passed_packets,
        "expected_packets": expected_packets,
        "packet_success_rate": passed_packets / expected_packets if expected_packets else 0.0,
    }


def _write_aggregate(session: Path, status: dict) -> None:
    rows = []
    for record in status["runs"]:
        row = {"case": record["case"], **record.get("summary", {})}
        rows.append(row)
    total_combinations = sum(item["total_combinations"] for item in rows)
    passed_combinations = sum(item["passed_combinations"] for item in rows)
    passed_packets = sum(item["passed_packets"] for item in rows)
    expected_packets = sum(item["expected_packets"] for item in rows)
    aggregate = {
        "cases": rows,
        "total_combinations": total_combinations,
        "passed_combinations": passed_combinations,
        "failed_combinations": total_combinations - passed_combinations,
        "combination_success_rate": (
            passed_combinations / total_combinations if total_combinations else 0.0),
        "passed_packets": passed_packets,
        "expected_packets": expected_packets,
        "packet_success_rate": passed_packets / expected_packets if expected_packets else 0.0,
    }
    _write_status(session / "aggregate_summary.json", aggregate)
    lines = [
        "# 多网关同步自动测试汇总", "",
        "组合通过条件：完成10包且成功率大于等于70%。", "",
        "| 用例 | 组合通过/总数 | 组合通过率 | 包通过/应发 | 包成功率 |",
        "|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['case']} | {row['passed_combinations']}/{row['total_combinations']} | "
            f"{row['combination_success_rate']:.2%} | "
            f"{row['passed_packets']}/{row['expected_packets']} | "
            f"{row['packet_success_rate']:.2%} |")
    lines.extend([
        f"| 合计 | {aggregate['passed_combinations']}/{aggregate['total_combinations']} | "
        f"{aggregate['combination_success_rate']:.2%} | "
        f"{aggregate['passed_packets']}/{aggregate['expected_packets']} | "
        f"{aggregate['packet_success_rate']:.2%} |",
        "",
    ])
    (session / "aggregate_summary.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260909)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--cases", default="5,6")
    parser.add_argument("--output-root", type=Path,
                        default=Path("sync_network_results/cases_5_6_sequential"))
    args = parser.parse_args()
    cases = tuple(int(value.strip()) for value in args.cases.split(",") if value.strip())
    if not cases or len(set(cases)) != len(cases) or not set(cases) <= {4, 5, 6}:
        parser.error("--cases must be a unique comma-separated subset of 4,5,6")

    runner = Path(__file__).with_name("sync_network_test.py")
    session = args.output_root / datetime.now().strftime("cases_5_6_%Y%m%d_%H%M%S")
    session.mkdir(parents=True, exist_ok=False)
    status_path = session / "orchestration_status.json"
    status = {"started_at": datetime.now().isoformat(timespec="seconds"), "runs": []}
    _write_status(status_path, status)

    for case in cases:
        case_root = session / f"case{case}"
        command = [
            sys.executable, str(runner),
            "--config", str(args.config),
            "--cases", str(case),
            "--seed", str(args.seed),
            "--sample-count", str(args.sample_count),
            "--output-root", str(case_root),
        ]
        record = {
            "case": case,
            "started_at": datetime.now().isoformat(timespec="seconds"),
            "command": command,
            "status": "running",
        }
        status["runs"].append(record)
        _write_status(status_path, status)
        exit_code = subprocess.call(command)
        run_dirs = sorted(case_root.glob("sync_network_*"))
        run_dir = run_dirs[-1] if run_dirs else None
        record.update({
            "finished_at": datetime.now().isoformat(timespec="seconds"),
            "exit_code": exit_code,
            "run_dir": str(run_dir) if run_dir else None,
            "summary": _summarize_run(run_dir),
            "status": "finished",
        })
        _write_status(status_path, status)

    status["finished_at"] = datetime.now().isoformat(timespec="seconds")
    _write_status(status_path, status)
    _write_aggregate(session, status)
    return 0 if all(item.get("exit_code") == 0 for item in status["runs"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
