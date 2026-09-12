#!/usr/bin/env python3
"""Summarize a completed case-4 run and optionally update its result table."""

from __future__ import annotations

import argparse
import csv
import ctypes
import json
import os
import time
from collections import Counter
from pathlib import Path
from typing import Any


RATES = (5, 6, 7, 8, 9, 10, 11, 18)


def _pid_running(pid: int) -> bool:
    if os.name == "nt":
        process = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid)
        if not process:
            return False
        try:
            exit_code = ctypes.c_ulong()
            if not ctypes.windll.kernel32.GetExitCodeProcess(process, ctypes.byref(exit_code)):
                return False
            return exit_code.value == 259
        finally:
            ctypes.windll.kernel32.CloseHandle(process)
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def _read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _period_seconds(combination: dict[str, Any]) -> int:
    total_us = int(combination["frame_period_us"]) * int(combination["frame_count"])
    if total_us % 1_000_000:
        raise ValueError(f"non-integer PPS period: {combination['key']}")
    return total_us // 1_000_000


def _combination_result(item: dict[str, Any], expected_packet_count: int,
                        success_threshold: float) -> dict[str, Any]:
    packets = item.get("packets", [])
    passed_packet_count = sum(
        bool(packet.get("verdict", {}).get("passed")) for packet in packets)
    success_rate = passed_packet_count / expected_packet_count
    return {
        "case": item["combination"]["case"],
        "key": item["combination"]["key"],
        "rates": item["combination"]["rates"],
        "attempted_packet_count": len(packets),
        "expected_packet_count": expected_packet_count,
        "passed_packet_count": passed_packet_count,
        "success_rate": success_rate,
        "success_threshold": success_threshold,
        "passed": (not item.get("error") and len(packets) == expected_packet_count and
                   success_rate >= success_threshold),
        "error": item.get("error", ""),
    }


def summarize(run_dir: Path) -> dict[str, Any]:
    selection = _read_json(run_dir / "selection.json")
    summary = _read_json(run_dir / "summary.json")
    config = _read_json(run_dir / "config.redacted.json")
    packets_per_combination = int(
        config.get("execution", {}).get("packets_per_combination", 10))
    success_threshold = float(
        config.get("execution", {}).get("combination_success_threshold", 0.70))
    if packets_per_combination <= 0:
        raise ValueError("packets_per_combination must be greater than zero")
    if not 0.0 <= success_threshold < 1.0:
        raise ValueError("combination_success_threshold must be in range [0, 1)")
    expected = len(selection["combinations"])
    records = summary.get("records", [])
    combination_results = [
        _combination_result(item, packets_per_combination, success_threshold)
        for item in records
    ]
    results_by_key = {item["key"]: item for item in combination_results}
    rows = []
    for rate in RATES:
        selected = [item for item in records if item["combination"]["rates"] == [rate]]
        configured = [item for item in selected
                      if item.get("applied_rate_configuration")]
        fully_executed = [item for item in selected
                          if len(item.get("packets", [])) == packets_per_combination]
        partial = [item for item in selected
                   if 0 < len(item.get("packets", [])) < packets_per_combination]
        setup_failed = [item for item in selected if item.get("error")]
        packets = [packet for item in selected for packet in item.get("packets", [])]
        rssi_04 = [int(packet["verdict"]["rssi"]["0499999999999999"])
                   for packet in packets
                   if "0499999999999999" in packet["verdict"].get("rssi", {})]
        rssi_05 = [int(packet["verdict"]["rssi"]["0599999999999999"])
                   for packet in packets
                   if "0599999999999999" in packet["verdict"].get("rssi", {})]
        ack_senders = Counter(sender for packet in packets
                              for sender in packet["verdict"].get("ack_senders", []))
        dual_gateway_rx = sum(
            len(packet["verdict"].get("selected_rx", {})) == 2 for packet in packets)
        correct_ack_arbitration = sum(
            packet["verdict"].get("strongest_gateway") is not None and
            packet["verdict"].get("ack_senders") ==
            [packet["verdict"].get("strongest_gateway")] for packet in packets)
        rows.append({
            "rate": rate,
            "combinations": len(selected),
            "configured_combinations": len(configured),
            "fully_executed_combinations": len(fully_executed),
            "partial_combinations": len(partial),
            "setup_failed_combinations": len(setup_failed),
            "passed_combinations": sum(
                bool(results_by_key[item["combination"]["key"]]["passed"])
                for item in selected),
            "expected_packets": len(selected) * packets_per_combination,
            "attempted_packets": len(packets),
            "passed_packets": sum(bool(item["verdict"].get("passed")) for item in packets),
            "dual_gateway_rx_packets": dual_gateway_rx,
            "correct_ack_arbitration_packets": correct_ack_arbitration,
            "terminal_acks": sum(bool(item["terminal"].get("txstatus7")) and
                                 bool(item["terminal"].get("rx_data")) for item in packets),
            "periods_seconds": sorted({_period_seconds(item["combination"])
                                       for item in configured}),
            "gw04_rssi_min": min(rssi_04) if rssi_04 else None,
            "gw04_rssi_max": max(rssi_04) if rssi_04 else None,
            "gw05_rssi_min": min(rssi_05) if rssi_05 else None,
            "gw05_rssi_max": max(rssi_05) if rssi_05 else None,
            "ack_senders": dict(ack_senders),
        })
    return {
        "expected_combinations": expected,
        "records_seen": len(records),
        "run_finished": len(records) == expected,
        "complete": (len(records) == expected and
                     all(len(item.get("packets", [])) == packets_per_combination
                         for item in records)),
        "packets_per_combination": packets_per_combination,
        "combination_success_threshold": success_threshold,
        "configured_combinations": sum(bool(item.get("applied_rate_configuration"))
                                       for item in records),
        "fully_executed_combinations": sum(
            len(item.get("packets", [])) == packets_per_combination for item in records),
        "setup_failed_combinations": sum(bool(item.get("error")) for item in records),
        "passed_combinations": sum(bool(item["passed"]) for item in combination_results),
        "failed_combinations": sum(not bool(item["passed"]) for item in combination_results),
        "rates": rows,
        "combinations": combination_results,
    }


def write_outputs(run_dir: Path, result: dict[str, Any]):
    (run_dir / "case4_rate_summary.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    with (run_dir / "case4_rate_summary.csv").open("w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=result["rates"][0].keys())
        writer.writeheader()
        writer.writerows(result["rates"])
    with (run_dir / "case4_combination_summary.csv").open(
            "w", encoding="utf-8-sig", newline="") as target:
        fieldnames = (
            "case", "key", "rates", "attempted_packet_count", "expected_packet_count",
            "passed_packet_count", "success_rate", "success_threshold", "passed", "error")
        writer = csv.DictWriter(target, fieldnames=fieldnames)
        writer.writeheader()
        for item in result["combinations"]:
            row = dict(item)
            row["rates"] = "+".join(map(str, row["rates"]))
            row["success_rate"] = f"{float(row['success_rate']):.2%}"
            row["success_threshold"] = f"{float(row['success_threshold']):.2%}"
            writer.writerow(row)
    lines = ["# 用例四自动测试速率汇总", "",
             f"- 计划组合：{result['expected_combinations']}",
             f"- 已遍历记录：{result['records_seen']}",
             f"- 运行已结束：{'是' if result['run_finished'] else '否'}",
             f"- 完整业务执行：{'是' if result['complete'] else '否'}",
             f"- 配置成功/完整业务/配置或终端失败："
             f"{result['configured_combinations']}/"
             f"{result['fully_executed_combinations']}/"
             f"{result['setup_failed_combinations']}",
             f"- 组合判定：10包完成且成功率大于等于"
             f"{result['combination_success_threshold']:.0%}",
             f"- 组合通过/失败：{result['passed_combinations']}/{result['failed_combinations']}", "",
             "| 速率 | 配置/完整/失败组合 | 组合通过/总数 | 包通过/已发/应发 | 双网关接收 | 正确ACK仲裁 | 终端ACK | PPS周期(s) | GW1 RSSI | GW2 RSSI |",
             "|---:|---:|---:|---:|---:|---:|---:|---|---|---|"]
    for row in result["rates"]:
        gw1 = (f"{row['gw04_rssi_min']}～{row['gw04_rssi_max']}"
               if row["gw04_rssi_min"] is not None else "无")
        gw2 = (f"{row['gw05_rssi_min']}～{row['gw05_rssi_max']}"
               if row["gw05_rssi_min"] is not None else "无")
        lines.append(
            f"| {row['rate']} | {row['configured_combinations']}/"
            f"{row['fully_executed_combinations']}/{row['setup_failed_combinations']} | "
            f"{row['passed_combinations']}/{row['combinations']} | "
            f"{row['passed_packets']}/{row['attempted_packets']}/"
            f"{row['expected_packets']} | {row['dual_gateway_rx_packets']} | "
            f"{row['correct_ack_arbitration_packets']} | {row['terminal_acks']} | "
            f"{','.join(map(str, row['periods_seconds']))} | {gw1} | {gw2} |")
    (run_dir / "case4_rate_summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def update_document(document: Path, result: dict[str, Any]):
    if not result["run_finished"]:
        raise RuntimeError("refusing to update document before the run finishes")
    text = document.read_text(encoding="utf-8")
    section_start = text.index("### 8.4 结果表")
    section_end = text.index("### 8.5", section_start)
    section = text[section_start:section_end]
    for row in result["rates"]:
        periods = (("/".join(map(str, row["periods_seconds"])) + " s")
                   if row["periods_seconds"] else "未形成有效配置")
        fixed_result = (f"{row['passed_packets']}/{row['attempted_packets']}包通过；"
                        f"应发{row['expected_packets']}包")
        conclusion = (f"固定点{row['passed_combinations']}/{row['combinations']}组通过；"
                      f"完整执行{row['fully_executed_combinations']}组；"
                      f"配置/终端失败{row['setup_failed_combinations']}组")
        bcn_status = "已确认" if row["configured_combinations"] else "未验证"
        replacement = (f"| {row['rate']:>4} | {periods} | {bcn_status} | {bcn_status} | 未执行 | 未执行 | "
                       f"{fixed_result} | 未执行 | 未测 | {conclusion} |")
        prefix = f"| {row['rate']:>4} |"
        candidates = [line for line in section.splitlines() if line.startswith(prefix)]
        if not candidates:
            raise RuntimeError(f"case-4 result row not found for rate {row['rate']}")
        section = section.replace(candidates[0], replacement, 1)
    text = text[:section_start] + section + text[section_end:]
    document.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--wait-pid", type=int)
    parser.add_argument("--document", type=Path)
    args = parser.parse_args()
    if args.wait_pid:
        while _pid_running(args.wait_pid):
            time.sleep(30)
    result = summarize(args.run_dir)
    write_outputs(args.run_dir, result)
    if args.document and result["run_finished"]:
        update_document(args.document, result)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["run_finished"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
