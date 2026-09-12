#!/usr/bin/env python3
"""Run one configurable two-gateway GPS/PPS synchronization test."""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import re
import threading
import time
import traceback
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from statistics import mean
from typing import Any

from sync_network_test import (
    BASE_US,
    BODY_US,
    RATE_MAX_BLOCKS,
    RATE_TO_NS,
    WAN_BLOCK_BYTES,
    WAN_DOWNLINK_OVERHEAD_BYTES,
    WAN_UPLINK_OVERHEAD_BYTES,
    GatewayLogs,
    MqttPlatform,
    SlotCombination,
    Terminal,
    _redact_value,
    _secret,
    _write_json,
    evaluate_combination,
    evaluate_packet,
    load_config,
    make_payload,
    parse_applied_rate_config,
    parse_gateway_window,
    solve_period,
    validate_applied_rate_config,
)


RX_TDD_RE = re.compile(
    r"^RX_TDD:\s*(?P<tdd>-?\d+)\s*,\s*(?P<rssi>-?\d+)\s*,\s*"
    r"(?P<snr>-?\d+)\s*,\s*(?P<cfo>-?\d+)"
    r"(?:\s*,\s*(?P<bcnbits>-?\d+))?\s*$"
)
INVALID_PATH_CHARS_RE = re.compile(r"[<>:\"/\\|?*\x00-\x1f]")
LOCATION_EXPECTATIONS = {
    1: ("靠近网关04", "靠近gw04", "near-gw04"),
    2: ("靠近网关05", "靠近gw05", "near-gw05"),
}


@dataclass(frozen=True)
class FixedTestParameters:
    rate: int
    tdd_num: int
    uplink_length: int
    downlink_length: int
    packet_count: int
    max_send_attempts: int
    cfo_jump_threshold: int
    location_description: str
    expected_bcnbits: int | None
    interrupt_one_gateway: bool = False
    gateway_interruption_seed: int | None = None


@dataclass(frozen=True)
class BroadcastSample:
    timestamp: str
    packet_index: int | None
    tdd: int
    rssi: int
    snr: int
    cfo: int
    bcnbits: int | None
    raw: str
    continuity_epoch: int = 0


class BroadcastCollector:
    def __init__(self):
        self.samples: list[BroadcastSample] = []
        self.malformed: list[dict[str, Any]] = []
        self.packet_index: int | None = None
        self.continuity_epoch = 0
        self.lock = threading.Lock()

    def set_packet(self, packet_index: int | None):
        with self.lock:
            self.packet_index = packet_index

    def advance_continuity_epoch(self):
        with self.lock:
            self.continuity_epoch += 1

    def observe(self, source: str, message: str, timestamp: str):
        if source != "TERM_RX" or not message.lstrip().startswith("RX_TDD:"):
            return
        match = RX_TDD_RE.fullmatch(message.strip())
        with self.lock:
            if match is None:
                self.malformed.append({"timestamp": timestamp, "raw": message})
                return
            values = {key: int(value) if value is not None else None
                      for key, value in match.groupdict().items()}
            self.samples.append(BroadcastSample(
                timestamp=timestamp,
                packet_index=self.packet_index,
                tdd=int(values["tdd"]),
                rssi=int(values["rssi"]),
                snr=int(values["snr"]),
                cfo=int(values["cfo"]),
                bcnbits=values["bcnbits"],
                raw=message,
                continuity_epoch=self.continuity_epoch,
            ))


def payload_blocks(rate: int, payload_length: int, overhead: int) -> int:
    if rate not in WAN_BLOCK_BYTES:
        raise ValueError(f"unsupported rate: {rate}")
    if payload_length <= 0:
        raise ValueError("payload length must be greater than zero")
    blocks = math.ceil((payload_length + overhead) / WAN_BLOCK_BYTES[rate])
    if blocks > RATE_MAX_BLOCKS[rate]:
        raise ValueError(
            f"rate {rate} payload length {payload_length} requires {blocks} blocks; "
            f"maximum is {RATE_MAX_BLOCKS[rate]}")
    return blocks


def build_fixed_combination(parameters: FixedTestParameters) -> SlotCombination:
    if parameters.tdd_num <= 0:
        raise ValueError("tdd_num must be greater than zero")
    if parameters.packet_count <= 0:
        raise ValueError("packet_count must be greater than zero")
    if parameters.max_send_attempts <= 0:
        raise ValueError("max_send_attempts must be greater than zero")
    if parameters.cfo_jump_threshold < 0:
        raise ValueError("cfo_jump_threshold must not be negative")
    if parameters.interrupt_one_gateway and parameters.packet_count < 4:
        raise ValueError("gateway interruption requires at least four logical packets")
    uplink_blocks = payload_blocks(
        parameters.rate, parameters.uplink_length, WAN_UPLINK_OVERHEAD_BYTES)
    downlink_blocks = payload_blocks(
        parameters.rate, parameters.downlink_length, WAN_DOWNLINK_OVERHEAD_BYTES)
    raw_period = (BASE_US[parameters.rate] +
                  2 * BODY_US[parameters.rate] * (uplink_blocks + downlink_blocks))
    solution = solve_period(raw_period, parameters.tdd_num)
    if solution is None:
        raise ValueError(
            f"no integer-second PPS solution for rate={parameters.rate}, "
            f"tdd_num={parameters.tdd_num}, ul={parameters.uplink_length}, "
            f"dl={parameters.downlink_length}")
    frame_period_us, frame_count = solution
    return SlotCombination(
        case=5,
        key=(f"R{parameters.rate}-TDD{parameters.tdd_num}-"
             f"UL{parameters.uplink_length}-DL{parameters.downlink_length}"),
        super_frame_num=parameters.tdd_num,
        rates=(parameters.rate,),
        ul_blocks=(uplink_blocks,),
        dl_blocks=(downlink_blocks,),
        frame_period_us=frame_period_us,
        frame_count=frame_count,
    )


def fixed_gateway_body(gateway: dict[str, Any], parameters: FixedTestParameters,
                       common: dict[str, Any]) -> dict[str, Any]:
    return {
        "gw_id": gateway["gw_id"].upper(),
        "freq_major": int(common["freq_major"]),
        "freq_minor": int(common["freq_minor"]),
        "nwk_num": int(gateway["nwk_num"]),
        "tdd_num": parameters.tdd_num,
        "rate_num": 1,
        "rate_cfgs": [{
            "rate_mode": RATE_TO_NS[parameters.rate],
            "uplink_len": parameters.uplink_length,
            "downlink_len": parameters.downlink_length,
        }],
        "description": (
            f"GPS fixed sync R{parameters.rate} TDD{parameters.tdd_num} "
            f"UL{parameters.uplink_length} DL{parameters.downlink_length} "
            f"{parameters.location_description}"
        ),
    }


def sanitize_description(value: str) -> str:
    cleaned = INVALID_PATH_CHARS_RE.sub("_", value.strip())
    cleaned = re.sub(r"\s+", "_", cleaned).strip(" ._")
    if not cleaned:
        raise ValueError("location_description must contain a usable character")
    return cleaned[:48]


def infer_expected_bcnbits(description: str) -> int | None:
    normalized = description.strip().lower()
    for bcnbits, aliases in LOCATION_EXPECTATIONS.items():
        if any(alias in normalized for alias in aliases):
            return bcnbits
    return None


def _numeric_summary(values: list[int]) -> dict[str, float | int | None]:
    if not values:
        return {"minimum": None, "maximum": None, "average": None}
    return {
        "minimum": min(values),
        "maximum": max(values),
        "average": mean(values),
    }


def analyze_broadcast(samples: list[BroadcastSample], malformed: list[dict[str, Any]],
                      tdd_num: int, cfo_jump_threshold: int,
                      expected_bcnbits: int | None) -> dict[str, Any]:
    by_bcn: dict[int, list[BroadcastSample]] = {}
    missing_bcnbits = []
    unexpected_bcnbits = []
    expected_mismatches = []
    transitions = []
    previous_with_bcn: BroadcastSample | None = None

    for sample in samples:
        if sample.bcnbits is None:
            missing_bcnbits.append(asdict(sample))
            continue
        by_bcn.setdefault(sample.bcnbits, []).append(sample)
        if sample.bcnbits not in (1, 2):
            unexpected_bcnbits.append(asdict(sample))
        if expected_bcnbits is not None and sample.bcnbits != expected_bcnbits:
            expected_mismatches.append(asdict(sample))
        if previous_with_bcn is not None and previous_with_bcn.bcnbits != sample.bcnbits:
            transitions.append({
                "timestamp": sample.timestamp,
                "from": previous_with_bcn.bcnbits,
                "to": sample.bcnbits,
                "previous_tdd": previous_with_bcn.tdd,
                "current_tdd": sample.tdd,
            })
        previous_with_bcn = sample

    per_bcnbits = {}
    all_tdd_jumps = []
    all_cfo_jumps = []
    continuity_resets = []
    for bcnbits, group in sorted(by_bcn.items()):
        tdd_jumps = []
        cfo_jumps = []
        for previous, current in zip(group, group[1:]):
            if current.continuity_epoch != previous.continuity_epoch:
                continuity_resets.append({
                    "bcnbits": bcnbits,
                    "timestamp": current.timestamp,
                    "from_epoch": previous.continuity_epoch,
                    "to_epoch": current.continuity_epoch,
                    "previous_tdd": previous.tdd,
                    "current_tdd": current.tdd,
                    "previous_cfo": previous.cfo,
                    "current_cfo": current.cfo,
                })
                continue
            expected_tdd = previous.tdd % tdd_num + 1
            if current.tdd != expected_tdd:
                tdd_jumps.append({
                    "bcnbits": bcnbits,
                    "timestamp": current.timestamp,
                    "previous_tdd": previous.tdd,
                    "expected_tdd": expected_tdd,
                    "current_tdd": current.tdd,
                })
            delta = current.cfo - previous.cfo
            if abs(delta) > cfo_jump_threshold:
                cfo_jumps.append({
                    "bcnbits": bcnbits,
                    "timestamp": current.timestamp,
                    "previous_cfo": previous.cfo,
                    "current_cfo": current.cfo,
                    "delta": delta,
                })
        all_tdd_jumps.extend(tdd_jumps)
        all_cfo_jumps.extend(cfo_jumps)
        cfo_deltas = [
            abs(current.cfo - previous.cfo)
            for previous, current in zip(group, group[1:])
            if current.continuity_epoch == previous.continuity_epoch
        ]
        per_bcnbits[str(bcnbits)] = {
            "sample_count": len(group),
            "rssi": _numeric_summary([item.rssi for item in group]),
            "snr": _numeric_summary([item.snr for item in group]),
            "cfo": _numeric_summary([item.cfo for item in group]),
            "maximum_absolute_cfo_delta": max(cfo_deltas) if cfo_deltas else 0,
            "tdd_jump_count": len(tdd_jumps),
            "tdd_jumps": tdd_jumps,
            "cfo_jump_count": len(cfo_jumps),
            "cfo_jumps": cfo_jumps,
        }

    reasons = []
    if not samples:
        reasons.append("broadcast_samples_missing")
    if malformed:
        reasons.append("malformed_rx_tdd")
    if missing_bcnbits:
        reasons.append("rx_tdd_bcnbits_missing")
    if unexpected_bcnbits:
        reasons.append("unexpected_bcnbits")
    if expected_mismatches:
        reasons.append("expected_bcnbits_mismatch")
    if all_tdd_jumps:
        reasons.append("broadcast_tdd_jump")
    if all_cfo_jumps:
        reasons.append("broadcast_cfo_jump")
    return {
        "passed": not reasons,
        "reasons": reasons,
        "sample_count": len(samples),
        "malformed_count": len(malformed),
        "malformed": malformed,
        "missing_bcnbits_count": len(missing_bcnbits),
        "unexpected_bcnbits_count": len(unexpected_bcnbits),
        "expected_bcnbits": expected_bcnbits,
        "expected_bcnbits_mismatch_count": len(expected_mismatches),
        "bcnbits_transition_count": len(transitions),
        "bcnbits_transitions": transitions,
        "tdd_jump_count": len(all_tdd_jumps),
        "cfo_jump_threshold": cfo_jump_threshold,
        "cfo_jump_count": len(all_cfo_jumps),
        "continuity_reset_count": len(continuity_resets),
        "continuity_resets": continuity_resets,
        "per_bcnbits": per_bcnbits,
    }


def deduplicate_log_text(text: str) -> str:
    seen = set()
    lines = []
    for line in text.splitlines():
        normalized = line.strip()
        if normalized and normalized not in seen:
            seen.add(normalized)
            lines.append(line)
    return "\n".join(lines)


def parse_unique_gateway_window(text: str) -> dict[str, Any]:
    return parse_gateway_window(deduplicate_log_text(text))


def summarize_gateways(packets: list[dict[str, Any]], gateway_ids: list[str]) -> dict[str, Any]:
    summary = {}
    for gateway_id in gateway_ids:
        logical_received = 0
        logical_ack_sent = 0
        receive_events = 0
        ack_send_events = 0
        rssi_values = []
        superframes = set()
        for packet in packets:
            packet_received = False
            packet_ack_sent = False
            for attempt in packet.get("attempts", []):
                window = attempt.get("gateway_windows", {}).get(gateway_id, {})
                receives = window.get("receives", [])
                sends = window.get("sends", [])
                packet_received = packet_received or bool(receives)
                packet_ack_sent = packet_ack_sent or bool(window.get("sent_ack"))
                receive_events += len(receives)
                ack_send_events += sum(int(item.get("sent", 0)) > 0 for item in sends)
                rssi_values.extend(int(item["rssi"]) for item in receives if "rssi" in item)
                superframes.update(int(item["super"]) for item in receives if "super" in item)
            logical_received += int(packet_received)
            logical_ack_sent += int(packet_ack_sent)
        summary[gateway_id] = {
            "logical_packets_received": logical_received,
            "receive_events": receive_events,
            "logical_packets_sent_ack": logical_ack_sent,
            "ack_send_events": ack_send_events,
            "rssi": _numeric_summary(rssi_values),
            "observed_superframes": sorted(superframes),
        }
    return summary


def interruption_thresholds(packet_count: int) -> tuple[int, int]:
    if packet_count < 4:
        raise ValueError("gateway interruption requires at least four logical packets")
    stop_after = math.ceil(packet_count * 0.25)
    restart_after = math.ceil(packet_count * 0.50)
    if stop_after >= restart_after:
        raise ValueError("gateway interruption thresholds overlap")
    return stop_after, restart_after


def packet_phase(packet_index: int, stop_after: int, restart_after: int) -> str:
    if packet_index <= stop_after:
        return "before_interruption"
    if packet_index <= restart_after:
        return "gateway_stopped"
    return "after_restart"


def summarize_phases(packets: list[dict[str, Any]]) -> dict[str, dict[str, int | float]]:
    summary: dict[str, dict[str, int | float]] = {}
    for packet in packets:
        phase = packet.get("phase", "normal")
        item = summary.setdefault(phase, {"packet_count": 0, "passed_count": 0,
                                          "success_rate": 0.0})
        item["packet_count"] += 1
        item["passed_count"] += int(packet.get("verdict", {}).get("passed", False))
    for item in summary.values():
        item["success_rate"] = item["passed_count"] / item["packet_count"]
    return summary


def _terminal_body(config: dict[str, Any]) -> dict[str, Any]:
    terminal = config["terminal"]
    return {
        "dev_eui": terminal["dev_eui"].upper(),
        "dev_type": int(terminal.get("dev_mode", 0)),
        "security_mode": int(terminal.get("security_mode", 0)),
        "root_key": _secret(terminal, "root_key", "root_key_env"),
        "related_id": terminal.get("related_id", ""),
        "description": "GPS fixed-configuration synchronization test terminal",
    }


def _write_broadcast_csv(path: Path, samples: list[BroadcastSample]):
    with path.open("w", encoding="utf-8-sig", newline="") as target:
        fields = tuple(BroadcastSample.__dataclass_fields__)
        writer = csv.DictWriter(target, fieldnames=fields)
        writer.writeheader()
        writer.writerows(asdict(sample) for sample in samples)


def _write_packet_csv(path: Path, packets: list[dict[str, Any]], gateway_ids: list[str]):
    fields = ["packet", "attempts", "passed", "failure_reasons", "terminal_success"]
    for gateway_id in gateway_ids:
        fields.extend((f"{gateway_id}_rx", f"{gateway_id}_ack"))
    with path.open("w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=fields)
        writer.writeheader()
        for packet in packets:
            attempts = packet.get("attempts", [])
            final = attempts[-1] if attempts else {}
            row = {
                "packet": packet["index"],
                "attempts": len(attempts),
                "passed": packet.get("verdict", {}).get("passed", False),
                "failure_reasons": ",".join(packet.get("verdict", {}).get("reasons", [])),
                "terminal_success": bool(final.get("terminal", {}).get("txstatus7") and
                                         final.get("terminal", {}).get("rx_data")),
            }
            for gateway_id in gateway_ids:
                window = final.get("gateway_windows", {}).get(gateway_id, {})
                row[f"{gateway_id}_rx"] = len(window.get("receives", []))
                row[f"{gateway_id}_ack"] = int(bool(window.get("sent_ack")))
            writer.writerow(row)


def _write_report(run_dir: Path, result: dict[str, Any]):
    parameters = result["parameters"]
    packet_summary = result["packet_summary"]
    broadcast = result["broadcast_summary"]
    lines = [
        "# 双网关固定配置同步测试结果",
        "",
        f"- 位置说明：{parameters['location_description']}",
        f"- 配置：模式 {parameters['rate']}，tdd_num={parameters['tdd_num']}，"
        f"上行长度={parameters['uplink_length']}，下行长度={parameters['downlink_length']}",
        f"- 逻辑数据包：{packet_summary['attempted_packet_count']}/"
        f"{packet_summary['expected_packet_count']}，通过 {packet_summary['passed_packet_count']}，"
        f"成功率 {packet_summary['success_rate']:.2%}",
        f"- 广播样本：{broadcast['sample_count']}，TDD 跳变 {broadcast['tdd_jump_count']}，"
        f"CFO 跳变 {broadcast['cfo_jump_count']}，BCN 切换 {broadcast['bcnbits_transition_count']}，"
        f"连续性重置 {broadcast['continuity_reset_count']}",
        f"- 总体结果：{'通过' if result.get('passed') else '失败'}",
        "",
        "## 双网关收发统计",
        "",
        "| 网关 | 收到逻辑包 | 接收事件 | 发送ACK逻辑包 | ACK发送事件 | RSSI均值 |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for gateway_id, item in result["gateway_summary"].items():
        average_rssi = item["rssi"]["average"]
        lines.append(
            f"| {gateway_id} | {item['logical_packets_received']} | {item['receive_events']} | "
            f"{item['logical_packets_sent_ack']} | {item['ack_send_events']} | "
            f"{average_rssi if average_rssi is not None else '-'} |")
    interruption = result.get("gateway_interruption")
    if interruption and interruption.get("enabled"):
        lines.extend((
            "",
            "## 网关中断恢复",
            "",
            f"- 随机中断网关：{interruption.get('gateway_id', '-')}",
            f"- 停止时点：第 {interruption['stop_after_packet']} 包完成后；"
            f"结果：{interruption.get('stop_status', 'not_run')}",
            f"- 恢复时点：第 {interruption['restart_after_packet']} 包完成后；"
            f"结果：{interruption.get('restart_status', 'not_run')}",
            "",
            "| 阶段 | 包数 | 严格判定通过 | 通过率 |",
            "|---|---:|---:|---:|",
        ))
        for phase, item in result.get("phase_summary", {}).items():
            lines.append(
                f"| {phase} | {item['packet_count']} | {item['passed_count']} | "
                f"{item['success_rate']:.2%} |")
    lines.extend((
        "",
        "## 广播统计",
        "",
        "| bcnbits | 样本数 | TDD跳变 | CFO跳变 | 最大CFO差值 | RSSI均值 | SNR均值 | CFO均值 |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|",
    ))
    for bcnbits, item in broadcast["per_bcnbits"].items():
        lines.append(
            f"| {bcnbits} | {item['sample_count']} | {item['tdd_jump_count']} | "
            f"{item['cfo_jump_count']} | {item['maximum_absolute_cfo_delta']} | "
            f"{item['rssi']['average']:.2f} | {item['snr']['average']:.2f} | "
            f"{item['cfo']['average']:.2f} |")
    if result.get("failure_reasons"):
        lines.extend(("", "## 失败原因", ""))
        lines.extend(f"- `{reason}`" for reason in result["failure_reasons"])
    lines.extend((
        "",
        "## 判定说明",
        "",
        "- TDD 连续性按相同 bcnbits 独立检查，期望序列为 1..tdd_num 后回到 1。",
        "- CFO 跳变按相同 bcnbits 相邻样本的绝对差值判断。",
        "- 重叠覆盖区域允许 bcnbits 在 1 和 2 之间切换；靠近单个网关时可指定期望 bcnbits。",
        "- 原始 RX_TDD、逐包尝试及网关日志保留在结果目录中，汇总不能替代原始证据。",
    ))
    (run_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_hardware(config: dict[str, Any], parameters: FixedTestParameters,
                 combination: SlotCombination, run_dir: Path) -> dict[str, Any]:
    collector = BroadcastCollector()
    transcript_lock = threading.Lock()
    transcript_path = run_dir / "events.log"
    platform = None
    terminal = None
    gateway_collectors = []
    packets = []
    gateway_ids = [item["gw_id"].upper() for item in config["gateways"]]
    result: dict[str, Any] = {
        "parameters": asdict(parameters),
        "combination": asdict(combination),
        "packets": packets,
    }
    interruption = {"enabled": parameters.interrupt_one_gateway}
    interrupted_collector = None
    stop_after = None
    restart_after = None
    if parameters.interrupt_one_gateway:
        if any(item.get("log_mode", "attach") != "manage"
               for item in config.get("gateways", [])):
            raise ValueError("gateway interruption requires log_mode=manage for both gateways")
        stop_after, restart_after = interruption_thresholds(parameters.packet_count)
        interruption.update({
            "seed": parameters.gateway_interruption_seed,
            "stop_after_packet": stop_after,
            "restart_after_packet": restart_after,
            "stop_status": "not_run",
            "restart_status": "not_run",
        })
    result["gateway_interruption"] = interruption

    def transcript(source: str, message: str):
        timestamp = datetime.now().isoformat(timespec="milliseconds")
        collector.observe(source, message, timestamp)
        safe_message = _redact_value(message)
        safe_message = re.sub(r'("root_key"\s*:\s*")[^"]*(")', r"\1***\2", safe_message)
        with transcript_lock:
            with transcript_path.open("a", encoding="utf-8") as target:
                target.write(f"[{timestamp}] [{source}] {safe_message}\n")

    try:
        if len(config.get("gateways", [])) != 2:
            raise ValueError("fixed synchronization test requires exactly two gateways")
        platform = MqttPlatform(config["mqtt"], transcript)
        terminal = Terminal(config["terminal"], transcript)
        gateway_collectors = [GatewayLogs(item, run_dir, transcript)
                              for item in config["gateways"]]
        for gateway_collector in gateway_collectors:
            gateway_collector.start_managed(run_dir.name)
        if parameters.interrupt_one_gateway:
            chooser = random.Random(parameters.gateway_interruption_seed)
            interrupted_collector = chooser.choice(gateway_collectors)
            interruption["gateway_id"] = interrupted_collector.config["gw_id"].upper()
            interruption["gateway_name"] = interrupted_collector.config["name"]
            transcript(
                "GATEWAY_INTERRUPTION",
                f"selected gateway={interruption['gateway_id']} seed="
                f"{parameters.gateway_interruption_seed}")
        terminal.open()

        config_marks = {item.config["gw_id"].upper(): item.mark()
                        for item in gateway_collectors}
        bodies = [fixed_gateway_body(item, parameters, config["radio"])
                  for item in config["gateways"]]
        result["gateway_configuration"] = platform.replace_gateways(bodies)
        time.sleep(float(config["timing"].get("gps_sync_wait_seconds", 30)))

        applied = {}
        config_log_dir = run_dir / "configuration"
        for gateway_collector in gateway_collectors:
            gateway_id = gateway_collector.config["gw_id"].upper()
            text = gateway_collector.collect_since(config_marks[gateway_id], config_log_dir)
            parsed = parse_applied_rate_config(text, 1)
            if not parsed:
                text = gateway_collector.collect_since({}, config_log_dir / "current")
                parsed = parse_applied_rate_config(text, 1)
            validate_applied_rate_config(combination, parsed)
            applied[gateway_id] = parsed
        result["applied_rate_configuration"] = applied

        terminal_body = _terminal_body(config)
        result["terminal_configuration"] = platform.replace_terminal(terminal_body)
        terminal.configure_and_join(parameters.rate)
        success_threshold = float(
            config["execution"].get("combination_success_threshold", 0.70))

        for packet_index in range(1, parameters.packet_count + 1):
            payload = make_payload(5, 1, packet_index,
                                   int(config["execution"].get("payload_bytes", 30)))
            phase = (packet_phase(packet_index, stop_after, restart_after)
                     if stop_after is not None and restart_after is not None else "normal")
            packet = {"index": packet_index, "payload": payload, "phase": phase,
                      "attempts": []}
            for attempt_index in range(1, parameters.max_send_attempts + 1):
                collector.set_packet(None)
                terminal._read(0.2)
                collector.set_packet(packet_index)
                platform.clear_uplinks()
                marks = {item.config["gw_id"].upper(): item.mark()
                         for item in gateway_collectors}
                terminal_result = terminal.send_confirmed(payload)
                uplinks = platform.collect_packet_uplinks(
                    terminal_body["dev_eui"], payload, set(gateway_ids),
                    float(config["timing"].get("mqtt_uplink_wait_seconds", 10)),
                    float(config["timing"].get("mqtt_post_first_wait_seconds", 1)))
                time.sleep(float(config["timing"].get("log_settle_seconds", 2)))
                attempt_dir = run_dir / f"packet_{packet_index:02d}" / f"attempt_{attempt_index}"
                windows = {}
                for gateway_collector in gateway_collectors:
                    gateway_id = gateway_collector.config["gw_id"].upper()
                    text = gateway_collector.collect_since(marks[gateway_id], attempt_dir)
                    windows[gateway_id] = parse_unique_gateway_window(text)
                verdict = evaluate_packet(
                    5, combination, gateway_ids, uplinks, terminal_result, windows)
                attempt = {
                    "index": attempt_index,
                    "terminal": terminal_result,
                    "uplinks": uplinks,
                    "gateway_windows": windows,
                    "verdict": verdict,
                }
                packet["attempts"].append(attempt)
                _write_json(attempt_dir / "evidence.json", attempt)
                terminal_success = bool(terminal_result.get("txstatus7") and
                                        terminal_result.get("rx_data"))
                if terminal_success:
                    break
                transcript(
                    "PACKET_RETRY",
                    f"packet={packet_index} attempt={attempt_index}/"
                    f"{parameters.max_send_attempts} terminal send failed")
            collector.set_packet(None)
            final_attempt = packet["attempts"][-1]
            packet["verdict"] = final_attempt["verdict"]
            packets.append(packet)
            _write_json(run_dir / f"packet_{packet_index:02d}" / "result.json", packet)
            print(
                f"packet={packet_index}/{parameters.packet_count} "
                f"attempts={len(packet['attempts'])} "
                f"{'PASS' if packet['verdict']['passed'] else 'FAIL'}",
                flush=True,
            )
            if interrupted_collector is not None and packet_index == stop_after:
                interruption["stop_status"] = "in_progress"
                try:
                    interrupted_collector.stop_managed()
                except Exception:
                    interruption["stop_status"] = "failed"
                    raise
                interruption["stop_status"] = "success"
                interruption["stopped_at"] = datetime.now().isoformat(timespec="seconds")
                transcript(
                    "GATEWAY_INTERRUPTION",
                    f"stopped gateway={interruption['gateway_id']} after_packet={packet_index}")
            if interrupted_collector is not None and packet_index == restart_after:
                interruption["restart_status"] = "in_progress"
                try:
                    restart_mark = interrupted_collector.mark()
                    interrupted_collector.restart_managed()
                    restart_log_dir = run_dir / "gateway_restart_configuration"
                    restart_text = interrupted_collector.collect_since(
                        restart_mark, restart_log_dir)
                    restart_applied = parse_applied_rate_config(restart_text, 1)
                    validate_applied_rate_config(combination, restart_applied)
                except Exception:
                    interruption["restart_status"] = "failed"
                    raise
                interruption["restart_status"] = "success"
                interruption["applied_rate_configuration"] = restart_applied
                interruption["restarted_at"] = datetime.now().isoformat(timespec="seconds")
                collector.advance_continuity_epoch()
                transcript(
                    "GATEWAY_INTERRUPTION",
                    f"restarted gateway={interruption['gateway_id']} after_packet={packet_index}")
        terminal._read(float(config["timing"].get("broadcast_tail_seconds", 2)))
        result["packet_summary"] = evaluate_combination(
            packets, parameters.packet_count, success_threshold)
    except Exception as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
        result["traceback"] = traceback.format_exc()
        transcript("ERROR", result["error"])
        result["packet_summary"] = evaluate_combination(
            packets, parameters.packet_count,
            float(config.get("execution", {}).get("combination_success_threshold", 0.70)))
    finally:
        collector.set_packet(None)
        if terminal is not None:
            terminal.close()
        for gateway_collector in reversed(gateway_collectors):
            try:
                gateway_collector.close()
            except Exception as exc:
                transcript("CLEANUP_ERROR", f"{type(exc).__name__}: {exc}")
        if platform is not None:
            platform.close()

    result["gateway_summary"] = summarize_gateways(packets, gateway_ids)
    result["phase_summary"] = summarize_phases(packets)
    result["broadcast_summary"] = analyze_broadcast(
        collector.samples, collector.malformed, parameters.tdd_num,
        parameters.cfo_jump_threshold, parameters.expected_bcnbits)
    failure_reasons = []
    if result.get("error"):
        failure_reasons.append("runtime_error")
    if not result["packet_summary"]["passed"]:
        failure_reasons.append("packet_success_rate_failed")
    failure_reasons.extend(result["broadcast_summary"]["reasons"])
    result["failure_reasons"] = failure_reasons
    result["passed"] = not failure_reasons
    _write_json(run_dir / "summary.json", result)
    _write_broadcast_csv(run_dir / "broadcast_samples.csv", collector.samples)
    _write_packet_csv(run_dir / "packet_summary.csv", packets, gateway_ids)
    _write_report(run_dir, result)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--config", type=Path, required=True, help="local test config JSON")
    parser.add_argument("--rate", type=int, default=8, choices=sorted(RATE_TO_NS),
                        help="terminal and gateway physical rate mode")
    parser.add_argument("--tdd-num", type=int, default=20,
                        help="NS superframe/TDD count")
    parser.add_argument("--uplink-length", type=int, default=50,
                        help="NS uplink MAC payload length in bytes")
    parser.add_argument("--downlink-length", type=int, default=50,
                        help="NS downlink MAC payload length in bytes")
    parser.add_argument("--packet-count", "--count", dest="packet_count",
                        type=int, default=20,
                        help="number of logical confirmed packets")
    parser.add_argument("--max-send-attempts", type=int, default=3,
                        help="maximum attempts for one logical packet")
    parser.add_argument("--cfo-jump-threshold", type=int, default=500,
                        help="maximum adjacent CFO delta in terminal-reported units")
    parser.add_argument("--location-description", required=True,
                        help="site label embedded in the result directory and report")
    parser.add_argument("--expected-bcnbits", type=int, choices=(1, 2),
                        help="expected beacon source; inferred for near-gateway labels")
    parser.add_argument("--interrupt-one-gateway", action="store_true",
                        help="stop a random gateway at 25 percent and restart it at 50 percent")
    parser.add_argument("--gateway-interruption-seed", type=int,
                        help="optional seed for reproducible random gateway selection")
    parser.add_argument(
        "--output-root", type=Path,
        default=Path("sync_network_results") / "fixed_config_sync_test")
    args = parser.parse_args()
    if args.gateway_interruption_seed is not None and not args.interrupt_one_gateway:
        parser.error("--gateway-interruption-seed requires --interrupt-one-gateway")

    expected_bcnbits = args.expected_bcnbits
    if expected_bcnbits is None:
        expected_bcnbits = infer_expected_bcnbits(args.location_description)
    parameters = FixedTestParameters(
        rate=args.rate,
        tdd_num=args.tdd_num,
        uplink_length=args.uplink_length,
        downlink_length=args.downlink_length,
        packet_count=args.packet_count,
        max_send_attempts=args.max_send_attempts,
        cfo_jump_threshold=args.cfo_jump_threshold,
        location_description=args.location_description,
        expected_bcnbits=expected_bcnbits,
        interrupt_one_gateway=args.interrupt_one_gateway,
        gateway_interruption_seed=args.gateway_interruption_seed,
    )
    combination = build_fixed_combination(parameters)
    config = load_config(args.config)
    if (args.interrupt_one_gateway and
            any(item.get("log_mode", "attach") != "manage"
                for item in config.get("gateways", []))):
        parser.error("--interrupt-one-gateway requires log_mode=manage for both gateways")
    description = sanitize_description(parameters.location_description)
    run_name = (
        f"R{parameters.rate}_TDD{parameters.tdd_num}_UL{parameters.uplink_length}_"
        f"DL{parameters.downlink_length}_{description}_"
        f"{datetime.now().strftime('%Y%m%d_%H%M%S')}")
    run_dir = args.output_root / run_name
    run_dir.mkdir(parents=True, exist_ok=False)
    _write_json(run_dir / "parameters.json", {
        "parameters": asdict(parameters),
        "combination": asdict(combination),
    })
    _write_json(run_dir / "config.redacted.json", config)
    result = run_hardware(config, parameters, combination, run_dir)
    print(f"result={'PASS' if result['passed'] else 'FAIL'} report={run_dir / 'report.md'}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
