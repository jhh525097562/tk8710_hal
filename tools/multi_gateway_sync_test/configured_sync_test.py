#!/usr/bin/env python3
"""Run one configured two-gateway synchronization and broadcast stability test."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import threading
import time
import traceback
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from statistics import fmean
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
    solve_period,
    validate_applied_rate_config,
)


RX_TDD_RE = re.compile(
    r"^RX_TDD:\s*(?P<tdd>-?\d+),(?P<rssi>-?\d+),(?P<snr>-?\d+),"
    r"(?P<cfo>-?\d+)(?:,(?P<bcnbits>-?\d+))?\s*$"
)
GATEWAY_RX_RE = re.compile(
    r"RX users - rateMode=(?P<rate>\d+), systemFrame=(?P<system>\d+), "
    r"superFrame=(?P<super>\d+), userCount=(?P<count>\d+)"
    r"(?:, firstRssi=(?P<rssi>-?\d+))?"
)
GATEWAY_TX_RE = re.compile(
    r"Sent (?P<sent>\d+)/(?P<total>\d+) users"
    r"(?:.*?systemFrame=(?P<system>\d+))?"
)


@dataclass(frozen=True)
class ConfiguredTestSpec:
    rate: int
    tdd_num: int
    uplink_length: int
    downlink_length: int
    packets: int
    uplink_blocks: int
    downlink_blocks: int
    frame_period_us: int
    frame_count: int
    pps_period_seconds: int

    def combination(self) -> SlotCombination:
        return SlotCombination(
            case=5,
            key=(f"CFG-R{self.rate}-SF{self.tdd_num}-"
                 f"UL{self.uplink_length}-DL{self.downlink_length}"),
            super_frame_num=self.tdd_num,
            rates=(self.rate,),
            ul_blocks=(self.uplink_blocks,),
            dl_blocks=(self.downlink_blocks,),
            frame_period_us=self.frame_period_us,
            frame_count=self.frame_count,
        )


@dataclass(frozen=True)
class BroadcastSample:
    observed_at: str
    packet_index: int | None
    tdd: int
    rssi: int
    snr: int
    cfo: int
    bcnbits: int | None
    raw: str


class BroadcastCollector:
    def __init__(self):
        self.enabled = False
        self.packet_index: int | None = None
        self.samples: list[BroadcastSample] = []
        self.malformed: list[dict[str, Any]] = []

    def observe(self, source: str, message: str):
        if not self.enabled or source != "TERM_RX" or not message.startswith("RX_TDD:"):
            return
        try:
            self.samples.append(parse_rx_tdd(message, self.packet_index))
        except ValueError as exc:
            self.malformed.append({
                "observed_at": datetime.now().isoformat(timespec="milliseconds"),
                "packet_index": self.packet_index,
                "raw": message,
                "error": str(exc),
            })


def payload_length_to_blocks(rate: int, length: int, overhead: int) -> int:
    if rate not in WAN_BLOCK_BYTES:
        raise ValueError(f"unsupported rate: {rate}")
    if length <= 0:
        raise ValueError("payload length must be greater than zero")
    return math.ceil((length + overhead) / WAN_BLOCK_BYTES[rate])


def build_test_spec(rate: int, tdd_num: int, uplink_length: int,
                    downlink_length: int, packets: int) -> ConfiguredTestSpec:
    if rate not in RATE_TO_NS:
        raise ValueError("rate must be one of 5,6,7,8,9,10,11,18")
    if not 1 <= tdd_num <= 64:
        raise ValueError("tdd_num must be in range 1..64")
    if packets <= 0:
        raise ValueError("packets must be greater than zero")
    ul_blocks = payload_length_to_blocks(
        rate, uplink_length, WAN_UPLINK_OVERHEAD_BYTES)
    dl_blocks = payload_length_to_blocks(
        rate, downlink_length, WAN_DOWNLINK_OVERHEAD_BYTES)
    maximum = RATE_MAX_BLOCKS[rate]
    if ul_blocks > maximum or dl_blocks > maximum:
        raise ValueError(
            f"rate {rate} supports at most {maximum} blocks: "
            f"calculated ul={ul_blocks}, dl={dl_blocks}")
    raw_period_us = (BASE_US[rate] +
                     2 * BODY_US[rate] * (ul_blocks + dl_blocks))
    solution = solve_period(raw_period_us, tdd_num)
    if solution is None:
        raise ValueError(
            f"no integer-second PPS solution: rate={rate}, tdd_num={tdd_num}, "
            f"ul_blocks={ul_blocks}, dl_blocks={dl_blocks}")
    frame_period_us, frame_count = solution
    total_us = frame_period_us * frame_count
    if total_us % 1_000_000:
        raise ValueError("calculated PPS period is not an integer number of seconds")
    return ConfiguredTestSpec(
        rate=rate,
        tdd_num=tdd_num,
        uplink_length=uplink_length,
        downlink_length=downlink_length,
        packets=packets,
        uplink_blocks=ul_blocks,
        downlink_blocks=dl_blocks,
        frame_period_us=frame_period_us,
        frame_count=frame_count,
        pps_period_seconds=total_us // 1_000_000,
    )


def parse_rx_tdd(line: str, packet_index: int | None = None,
                 observed_at: str | None = None) -> BroadcastSample:
    match = RX_TDD_RE.fullmatch(line.strip())
    if match is None:
        raise ValueError(f"invalid RX_TDD line: {line}")
    values = {name: int(value) if value is not None else None
              for name, value in match.groupdict().items()}
    return BroadcastSample(
        observed_at=observed_at or datetime.now().isoformat(timespec="milliseconds"),
        packet_index=packet_index,
        tdd=int(values["tdd"]),
        rssi=int(values["rssi"]),
        snr=int(values["snr"]),
        cfo=int(values["cfo"]),
        bcnbits=values["bcnbits"],
        raw=line.strip(),
    )


def _numeric_stats(values: list[int]) -> dict[str, float | int | None]:
    if not values:
        return {"minimum": None, "maximum": None, "average": None}
    return {
        "minimum": min(values),
        "maximum": max(values),
        "average": fmean(values),
    }


def analyze_broadcast_samples(samples: list[BroadcastSample], tdd_num: int,
                              cfo_jump_threshold: int,
                              expected_bcnbits: int | None = None,
                              malformed_count: int = 0) -> dict[str, Any]:
    if tdd_num <= 0:
        raise ValueError("tdd_num must be greater than zero")
    if cfo_jump_threshold < 0:
        raise ValueError("cfo_jump_threshold must not be negative")
    grouped: dict[int, list[BroadcastSample]] = {}
    missing_bcnbits = []
    invalid_tdd = []
    unexpected_bcnbits = []
    expected_mismatches = []
    transitions = []
    previous_global: BroadcastSample | None = None
    for sample in samples:
        if not 1 <= sample.tdd <= tdd_num:
            invalid_tdd.append(asdict(sample))
        if sample.bcnbits is None:
            missing_bcnbits.append(asdict(sample))
        else:
            grouped.setdefault(sample.bcnbits, []).append(sample)
            if sample.bcnbits not in (1, 2):
                unexpected_bcnbits.append(asdict(sample))
            if expected_bcnbits is not None and sample.bcnbits != expected_bcnbits:
                expected_mismatches.append(asdict(sample))
            if (previous_global is not None and previous_global.bcnbits is not None and
                    previous_global.bcnbits != sample.bcnbits):
                transitions.append({
                    "observed_at": sample.observed_at,
                    "previous": previous_global.bcnbits,
                    "current": sample.bcnbits,
                    "previous_tdd": previous_global.tdd,
                    "current_tdd": sample.tdd,
                })
            previous_global = sample

    per_bcn = {}
    tdd_jumps = []
    cfo_jumps = []
    for bcnbits, group in sorted(grouped.items()):
        group_tdd_jumps = []
        group_cfo_jumps = []
        max_cfo_delta = 0
        previous: BroadcastSample | None = None
        for sample in group:
            if previous is not None:
                expected_tdd = previous.tdd % tdd_num + 1
                if sample.tdd != expected_tdd:
                    jump = {
                        "bcnbits": bcnbits,
                        "observed_at": sample.observed_at,
                        "previous": previous.tdd,
                        "expected": expected_tdd,
                        "current": sample.tdd,
                    }
                    group_tdd_jumps.append(jump)
                    tdd_jumps.append(jump)
                delta = abs(sample.cfo - previous.cfo)
                max_cfo_delta = max(max_cfo_delta, delta)
                if delta > cfo_jump_threshold:
                    jump = {
                        "bcnbits": bcnbits,
                        "observed_at": sample.observed_at,
                        "previous": previous.cfo,
                        "current": sample.cfo,
                        "delta": delta,
                        "threshold": cfo_jump_threshold,
                    }
                    group_cfo_jumps.append(jump)
                    cfo_jumps.append(jump)
            previous = sample
        per_bcn[str(bcnbits)] = {
            "sample_count": len(group),
            "tdd": _numeric_stats([item.tdd for item in group]),
            "rssi": _numeric_stats([item.rssi for item in group]),
            "snr": _numeric_stats([item.snr for item in group]),
            "cfo": _numeric_stats([item.cfo for item in group]),
            "maximum_adjacent_cfo_delta": max_cfo_delta,
            "tdd_jump_count": len(group_tdd_jumps),
            "cfo_jump_count": len(group_cfo_jumps),
        }

    failure_reasons = []
    if not samples:
        failure_reasons.append("broadcast_samples_missing")
    if malformed_count:
        failure_reasons.append("malformed_rx_tdd")
    if missing_bcnbits:
        failure_reasons.append("rx_tdd_bcnbits_missing")
    if invalid_tdd:
        failure_reasons.append("broadcast_tdd_out_of_range")
    if unexpected_bcnbits:
        failure_reasons.append("unexpected_bcnbits")
    if expected_mismatches:
        failure_reasons.append("expected_bcnbits_mismatch")
    if tdd_jumps:
        failure_reasons.append("broadcast_tdd_jump")
    if cfo_jumps:
        failure_reasons.append("broadcast_cfo_jump")
    return {
        "passed": not failure_reasons,
        "failure_reasons": failure_reasons,
        "sample_count": len(samples),
        "malformed_count": malformed_count,
        "missing_bcnbits_count": len(missing_bcnbits),
        "invalid_tdd_count": len(invalid_tdd),
        "unexpected_bcnbits_count": len(unexpected_bcnbits),
        "expected_bcnbits": expected_bcnbits,
        "expected_bcnbits_mismatch_count": len(expected_mismatches),
        "bcnbits_transition_count": len(transitions),
        "bcnbits_transitions": transitions,
        "tdd_jump_count": len(tdd_jumps),
        "tdd_jumps": tdd_jumps,
        "cfo_jump_threshold": cfo_jump_threshold,
        "cfo_jump_count": len(cfo_jumps),
        "cfo_jumps": cfo_jumps,
        "per_bcnbits": per_bcn,
    }


def sanitize_description(value: str) -> str:
    result = re.sub(r'[<>:"/\\|?*\x00-\x1f]+', "_", value.strip())
    result = re.sub(r"\s+", "_", result).strip(" ._")
    if not result:
        raise ValueError("location description must contain a usable character")
    return result[:64]


def infer_expected_bcnbits(description: str) -> int | None:
    normalized = description.lower().replace("_", "").replace("-", "")
    if "网关04" in normalized or "gw04" in normalized:
        return 1
    if "网关05" in normalized or "gw05" in normalized:
        return 2
    return None


def configured_gateway_body(gateway: dict[str, Any], spec: ConfiguredTestSpec,
                            common: dict[str, Any], description: str) -> dict[str, Any]:
    return {
        "gw_id": gateway["gw_id"].upper(),
        "freq_major": int(common["freq_major"]),
        "freq_minor": int(common["freq_minor"]),
        "nwk_num": int(gateway["nwk_num"]),
        "tdd_num": spec.tdd_num,
        "rate_num": 1,
        "rate_cfgs": [{
            "rate_mode": RATE_TO_NS[spec.rate],
            "uplink_len": spec.uplink_length,
            "downlink_len": spec.downlink_length,
        }],
        "description": f"GPS sync configured {description}",
    }


def parse_gateway_statistics(text: str) -> dict[str, Any]:
    receives = []
    seen_receives = set()
    for match in GATEWAY_RX_RE.finditer(text):
        item = {name: int(value) for name, value in match.groupdict().items()
                if value is not None}
        identity = tuple(sorted(item.items()))
        if identity not in seen_receives:
            seen_receives.add(identity)
            receives.append(item)
    sends = []
    seen_sends = set()
    for match in GATEWAY_TX_RE.finditer(text):
        item = {name: int(value) for name, value in match.groupdict().items()
                if value is not None}
        identity = tuple(sorted(item.items()))
        if identity not in seen_sends:
            seen_sends.add(identity)
            sends.append(item)
    return {
        "receives": receives,
        "sends": sends,
        "last_rssi": receives[-1].get("rssi") if receives else None,
        "sent_ack": any(item["sent"] > 0 for item in sends),
    }


def _terminal_send_succeeded(result: dict[str, Any]) -> bool:
    return bool(result.get("txstatus7") and result.get("rx_data"))


def _gateway_summary(packets: list[dict[str, Any]], gateway_ids: list[str]) -> dict[str, Any]:
    summary = {}
    for gateway_id in gateway_ids:
        packet_windows = [attempt["gateway_windows"].get(gateway_id, {})
                          for packet in packets for attempt in packet["attempts"]]
        summary[gateway_id] = {
            "logical_packets_received": sum(
                any(attempt["gateway_windows"].get(gateway_id, {}).get("receives", [])
                    for attempt in packet["attempts"])
                for packet in packets),
            "attempts_with_receive": sum(bool(window.get("receives"))
                                         for window in packet_windows),
            "receive_event_count": sum(len(window.get("receives", []))
                                       for window in packet_windows),
            "attempts_with_ack_send": sum(bool(window.get("sent_ack"))
                                          for window in packet_windows),
            "ack_send_event_count": sum(len(window.get("sends", []))
                                        for window in packet_windows),
        }
    return summary


def _write_csv_reports(run_dir: Path, result: dict[str, Any]):
    with (run_dir / "packet_summary.csv").open(
            "w", encoding="utf-8-sig", newline="") as target:
        fields = ["packet", "attempts", "passed", "reasons", "terminal_success",
                  "gw04_rx", "gw04_ack", "gw05_rx", "gw05_ack"]
        writer = csv.DictWriter(target, fieldnames=fields)
        writer.writeheader()
        gateway_ids = result["gateway_ids"]
        for packet in result["packets"]:
            final = packet["attempts"][-1] if packet["attempts"] else None
            windows = final["gateway_windows"] if final is not None else {}
            first_window = windows.get(gateway_ids[0], {})
            second_window = windows.get(gateway_ids[1], {})
            writer.writerow({
                "packet": packet["index"],
                "attempts": len(packet["attempts"]),
                "passed": final["verdict"]["passed"] if final else False,
                "reasons": (",".join(final["verdict"]["reasons"])
                            if final else "attempt_not_started"),
                "terminal_success": (_terminal_send_succeeded(final["terminal"])
                                     if final else False),
                "gw04_rx": len(first_window.get("receives", [])),
                "gw04_ack": len(first_window.get("sends", [])),
                "gw05_rx": len(second_window.get("receives", [])),
                "gw05_ack": len(second_window.get("sends", [])),
            })
    with (run_dir / "broadcast_samples.csv").open(
            "w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=BroadcastSample.__dataclass_fields__)
        writer.writeheader()
        writer.writerows(result["broadcast_samples"])

    anomaly_fields = ["type", "bcnbits", "observed_at", "previous",
                      "expected", "current", "delta", "threshold", "raw"]
    with (run_dir / "broadcast_anomalies.csv").open(
            "w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=anomaly_fields)
        writer.writeheader()
        analysis = result["broadcast_analysis"]
        for kind, key in (("tdd_jump", "tdd_jumps"),
                          ("cfo_jump", "cfo_jumps"),
                          ("bcnbits_transition", "bcnbits_transitions")):
            for item in analysis[key]:
                row = {"type": kind}
                row.update({name: item.get(name) for name in anomaly_fields
                            if name != "type"})
                writer.writerow(row)
        for item in result["malformed_rx_tdd"]:
            row = {"type": "malformed_rx_tdd", "observed_at": item["observed_at"],
                   "raw": item["raw"]}
            writer.writerow(row)


def _format_stat(value: float | int | None) -> str:
    if value is None:
        return "-"
    if isinstance(value, float):
        return f"{value:.2f}"
    return str(value)


def _write_markdown_report(run_dir: Path, result: dict[str, Any]):
    spec = result["spec"]
    data = result["data_summary"]
    broadcast = result["broadcast_analysis"]
    lines = [
        "# 双网关指定配置同步测试结果",
        "",
        f"- 位置说明：{result['location_description']}",
        f"- 测试时间：{result['started_at']} 至 {result['finished_at']}",
        f"- 配置：模式 {spec['rate']}，tdd_num={spec['tdd_num']}，"
        f"上行长度={spec['uplink_length']}，下行长度={spec['downlink_length']}",
        f"- 时隙换算：UL={spec['uplink_blocks']}块，DL={spec['downlink_blocks']}块，"
        f"framePeriod={spec['frame_period_us']} us，frameCount={spec['frame_count']}，"
        f"PPS周期={spec['pps_period_seconds']} s",
        f"- 数据包：{data['passed_packet_count']}/{data['expected_packet_count']} 通过，"
        f"成功率 {data['success_rate']:.2%}",
        f"- 广播稳定性：{'通过' if broadcast['passed'] else '失败'}",
        f"- 最终结果：{'通过' if result['passed'] else '失败'}",
        "",
        "## 双网关收发统计",
        "",
        "| 网关 | 逻辑包收到 | 接收尝试 | 接收事件 | ACK发送尝试 | ACK发送事件 |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    names = {item["gw_id"].upper(): item["name"] for item in result["gateway_configs"]}
    for gateway_id, summary in result["gateway_summary"].items():
        lines.append(
            f"| {names.get(gateway_id, gateway_id)} | "
            f"{summary['logical_packets_received']}/{spec['packets']} | "
            f"{summary['attempts_with_receive']} | {summary['receive_event_count']} | "
            f"{summary['attempts_with_ack_send']} | {summary['ack_send_event_count']} |")
    lines.extend([
        "",
        "## 广播统计",
        "",
        f"共记录 {broadcast['sample_count']} 条 RX_TDD，格式错误 "
        f"{broadcast['malformed_count']} 条，缺少 bcnbits "
        f"{broadcast['missing_bcnbits_count']} 条。",
        f"bcnbits 切换 {broadcast['bcnbits_transition_count']} 次；TDD 跳变 "
        f"{broadcast['tdd_jump_count']} 次；CFO 跳变 {broadcast['cfo_jump_count']} 次"
        f"（阈值 {broadcast['cfo_jump_threshold']}）。",
        "",
        "| bcnbits | 样本 | RSSI min/avg/max | SNR min/avg/max | CFO min/avg/max | 最大相邻CFO差 | TDD跳变 | CFO跳变 |",
        "|---:|---:|---|---|---|---:|---:|---:|",
    ])
    for bcnbits, item in broadcast["per_bcnbits"].items():
        def triple(name: str) -> str:
            values = item[name]
            return "/".join(_format_stat(values[key])
                            for key in ("minimum", "average", "maximum"))
        lines.append(
            f"| {bcnbits} | {item['sample_count']} | {triple('rssi')} | "
            f"{triple('snr')} | {triple('cfo')} | "
            f"{item['maximum_adjacent_cfo_delta']} | {item['tdd_jump_count']} | "
            f"{item['cfo_jump_count']} |")
    lines.extend(["", "## 异常", ""])
    if broadcast["failure_reasons"]:
        lines.extend(f"- {reason}" for reason in broadcast["failure_reasons"])
    else:
        lines.append("- 未检测到广播异常。")
    failed_packets = [packet for packet in result["packets"]
                      if (not packet["attempts"] or
                          not packet["attempts"][-1]["verdict"]["passed"])]
    for packet in failed_packets:
        reasons = (",".join(packet["attempts"][-1]["verdict"]["reasons"])
                   if packet["attempts"] else "attempt_not_started")
        lines.append(f"- 数据包 {packet['index']}：{reasons}")
    if result["error"]:
        lines.append(f"- 运行错误：{result['error']}")
    (run_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_hardware(config: dict[str, Any], spec: ConfiguredTestSpec, run_dir: Path,
                 location_description: str, expected_bcnbits: int | None,
                 cfo_jump_threshold: int, max_send_attempts: int,
                 broadcast_tail_seconds: float) -> dict[str, Any]:
    if len(config.get("gateways", [])) != 2:
        raise ValueError("configured synchronization test requires exactly two gateways")
    transcript_lock = threading.Lock()
    transcript_path = run_dir / "events.log"
    broadcasts = BroadcastCollector()

    def transcript(source: str, message: str):
        broadcasts.observe(source, message)
        safe_message = _redact_value(message)
        safe_message = re.sub(
            r'("root_key"\s*:\s*")[^"]*(")', r"\1***\2", safe_message)
        line = (f"[{datetime.now().isoformat(timespec='milliseconds')}] "
                f"[{source}] {safe_message}\n")
        with transcript_lock:
            with transcript_path.open("a", encoding="utf-8") as target:
                target.write(line)

    platform = None
    terminal = None
    gateway_collectors = []
    packets = []
    started_at = datetime.now().isoformat(timespec="seconds")
    gateway_ids = [item["gw_id"].upper() for item in config["gateways"]]
    combination = spec.combination()
    applied = {}
    gateway_configuration = None
    terminal_configuration = None
    run_error = None
    try:
        platform = MqttPlatform(config["mqtt"], transcript)
        terminal = Terminal(config["terminal"], transcript)
        gateway_collectors = [GatewayLogs(item, run_dir, transcript)
                              for item in config["gateways"]]
        for collector in gateway_collectors:
            collector.start_managed(run_dir.name)
        terminal.open()
        config_marks = {collector.config["gw_id"].upper(): collector.mark()
                        for collector in gateway_collectors}
        bodies = [configured_gateway_body(item, spec, config["radio"],
                                           location_description)
                  for item in config["gateways"]]
        gateway_configuration = platform.replace_gateways(bodies)
        time.sleep(float(config["timing"].get("gps_sync_wait_seconds", 30)))
        config_log_dir = run_dir / "configuration"
        for collector in gateway_collectors:
            gateway_id = collector.config["gw_id"].upper()
            text = collector.collect_since(config_marks[gateway_id], config_log_dir)
            parsed = parse_applied_rate_config(text, 1)
            if not parsed:
                text = collector.collect_since({}, config_log_dir / "current")
                parsed = parse_applied_rate_config(text, 1)
            validate_applied_rate_config(combination, parsed)
            applied[gateway_id] = parsed
        terminal_body = {
            "dev_eui": config["terminal"]["dev_eui"].upper(),
            "dev_type": int(config["terminal"].get("dev_mode", 0)),
            "security_mode": int(config["terminal"].get("security_mode", 0)),
            "root_key": _secret(config["terminal"], "root_key", "root_key_env"),
            "related_id": config["terminal"].get("related_id", ""),
            "description": f"Configured sync test {location_description}",
        }
        terminal_configuration = platform.replace_terminal(terminal_body)
        terminal.configure_and_join(spec.rate)
        broadcasts.enabled = True
        for packet_index in range(1, spec.packets + 1):
            payload = make_payload(
                7, 1, packet_index,
                int(config.get("execution", {}).get("payload_bytes", 30)))
            packet = {"index": packet_index, "payload": payload, "attempts": []}
            broadcasts.packet_index = packet_index
            for attempt_index in range(1, max_send_attempts + 1):
                platform.clear_uplinks()
                marks = {collector.config["gw_id"].upper(): collector.mark()
                         for collector in gateway_collectors}
                terminal_result = terminal.send_confirmed(payload)
                uplinks = platform.collect_packet_uplinks(
                    terminal_body["dev_eui"], payload, set(gateway_ids),
                    float(config["timing"].get("mqtt_uplink_wait_seconds", 10)),
                    float(config["timing"].get("mqtt_post_first_wait_seconds", 1)))
                time.sleep(float(config["timing"].get("log_settle_seconds", 2)))
                attempt_dir = (run_dir / f"packet_{packet_index:02d}" /
                               f"attempt_{attempt_index}")
                windows = {}
                for collector in gateway_collectors:
                    gateway_id = collector.config["gw_id"].upper()
                    text = collector.collect_since(marks[gateway_id], attempt_dir)
                    windows[gateway_id] = parse_gateway_statistics(text)
                verdict = evaluate_packet(
                    5, combination, gateway_ids, uplinks, terminal_result, windows)
                rates = {item["rate"] for window in windows.values()
                         for item in window["receives"]}
                if rates and rates != {spec.rate}:
                    verdict["reasons"].append("received_rate_mismatch")
                    verdict["passed"] = False
                attempt = {
                    "index": attempt_index,
                    "terminal": terminal_result,
                    "uplinks": uplinks,
                    "gateway_windows": windows,
                    "verdict": verdict,
                }
                packet["attempts"].append(attempt)
                _write_json(attempt_dir / "evidence.json", attempt)
                print(
                    f"packet={packet_index}/{spec.packets} attempt="
                    f"{attempt_index}/{max_send_attempts} "
                    f"{'PASS' if verdict['passed'] else 'FAIL'}",
                    flush=True)
                if _terminal_send_succeeded(terminal_result):
                    break
            packets.append(packet)
        broadcasts.packet_index = None
        terminal._read(broadcast_tail_seconds)
        broadcasts.enabled = False
    except Exception as exc:
        run_error = f"{type(exc).__name__}: {exc}"
        transcript("ERROR", run_error)
        traceback_text = traceback.format_exc()
        (run_dir / "traceback.log").write_text(traceback_text, encoding="utf-8")
    finally:
        broadcasts.enabled = False
        if terminal is not None:
            terminal.close()
        for collector in reversed(gateway_collectors):
            try:
                collector.close()
            except Exception as exc:
                transcript("CLEANUP_ERROR", f"{type(exc).__name__}: {exc}")
        if platform is not None:
            platform.close()

    final_packets = [{"verdict": packet["attempts"][-1]["verdict"]}
                     for packet in packets if packet["attempts"]]
    success_threshold = float(
        config.get("execution", {}).get("combination_success_threshold", 0.70))
    data_summary = evaluate_combination(
        final_packets, spec.packets, success_threshold)
    broadcast_analysis = analyze_broadcast_samples(
        broadcasts.samples, spec.tdd_num, cfo_jump_threshold,
        expected_bcnbits, len(broadcasts.malformed))
    result = {
        "started_at": started_at,
        "finished_at": datetime.now().isoformat(timespec="seconds"),
        "location_description": location_description,
        "expected_bcnbits": expected_bcnbits,
        "spec": asdict(spec),
        "gateway_ids": gateway_ids,
        "gateway_configs": config["gateways"],
        "gateway_configuration": gateway_configuration,
        "applied_rate_configuration": applied,
        "terminal_configuration": terminal_configuration,
        "packets": packets,
        "data_summary": data_summary,
        "gateway_summary": _gateway_summary(packets, gateway_ids),
        "broadcast_samples": [asdict(item) for item in broadcasts.samples],
        "malformed_rx_tdd": broadcasts.malformed,
        "broadcast_analysis": broadcast_analysis,
        "error": run_error,
        "passed": bool(not run_error and data_summary["passed"] and
                       broadcast_analysis["passed"]),
    }
    _write_json(run_dir / "summary.json", result)
    _write_csv_reports(run_dir, result)
    _write_markdown_report(run_dir, result)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--rate", type=int, required=True)
    parser.add_argument("--tdd-num", type=int, required=True)
    parser.add_argument("--uplink-length", type=int, required=True,
                        help="NS MAC payload length in bytes")
    parser.add_argument("--downlink-length", type=int, required=True,
                        help="NS MAC payload length in bytes")
    parser.add_argument("--packets", type=int, required=True,
                        help="number of logical confirmed packets")
    parser.add_argument("--location-description", required=True)
    parser.add_argument("--expected-bcnbits", type=int, choices=(1, 2))
    parser.add_argument("--cfo-jump-threshold", type=int, default=500)
    parser.add_argument("--max-send-attempts", type=int, default=3)
    parser.add_argument("--broadcast-tail-seconds", type=float, default=2.0)
    parser.add_argument(
        "--output-root", type=Path,
        default=Path("sync_network_results") / "configured_sync_test")
    args = parser.parse_args()
    if args.max_send_attempts < 1:
        parser.error("--max-send-attempts must be at least 1")
    if args.broadcast_tail_seconds < 0:
        parser.error("--broadcast-tail-seconds must not be negative")
    try:
        spec = build_test_spec(
            args.rate, args.tdd_num, args.uplink_length,
            args.downlink_length, args.packets)
        description_slug = sanitize_description(args.location_description)
    except ValueError as exc:
        parser.error(str(exc))
    expected_bcnbits = args.expected_bcnbits
    if expected_bcnbits is None:
        expected_bcnbits = infer_expected_bcnbits(args.location_description)
    config = load_config(args.config)
    run_dir = (args.output_root /
               f"configured_{description_slug}_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
    run_dir.mkdir(parents=True, exist_ok=False)
    _write_json(run_dir / "requested_test.json", {
        "location_description": args.location_description,
        "expected_bcnbits": expected_bcnbits,
        "cfo_jump_threshold": args.cfo_jump_threshold,
        "max_send_attempts": args.max_send_attempts,
        "broadcast_tail_seconds": args.broadcast_tail_seconds,
        "spec": asdict(spec),
    })
    _write_json(run_dir / "config.redacted.json", json.loads(json.dumps(config)))
    result = run_hardware(
        config, spec, run_dir, args.location_description, expected_bcnbits,
        args.cfo_jump_threshold, args.max_send_attempts,
        args.broadcast_tail_seconds)
    print(f"result={'PASS' if result['passed'] else 'FAIL'}: {run_dir / 'report.md'}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
