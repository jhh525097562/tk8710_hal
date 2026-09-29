"""Offline replay of SSCOM text or a captured journal; no hardware required."""
from __future__ import annotations

import argparse
import collections
import csv
import html
import json
import math
import re
import statistics
from pathlib import Path

from protocol import NWK, RF_STEP, infer_gap, parse_rx


def load_records(path):
    path = Path(path)
    if path.suffix == ".jsonl":
        with path.open(encoding="utf-8") as stream:
            for number, line in enumerate(stream, 1):
                if line.strip():
                    record = json.loads(line)
                    record["source_line"] = number
                    yield record
        return
    current_ms = None
    previous_ms = None
    days = 0
    block = 0
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        match = re.search(r"\[(\d\d):(\d\d):(\d\d)\.(\d{3})\]", line)
        if match:
            h, m, s, ms = map(int, match.groups())
            raw_ms = ((h*60+m)*60+s)*1000+ms
            if previous_ms is not None and raw_ms < previous_ms-12*3600000:
                days += 1
            current_ms = raw_ms+days*86400000
            previous_ms = raw_ms
            block = number
        if current_ms is not None and line.strip():
            yield dict(kind="terminal", time_ms=current_ms, text=line,
                       source_line=number, receive_block=block)


def make_windows(records, guard_ms):
    pending = {}
    windows = []
    for record in records:
        if record["kind"] != "gateway_event":
            continue
        name = record["event"]
        index = record.get("index", -1)
        if name == "POINT_START":
            pending[index] = record
        elif name in ("POINT_END", "POINT_FAILED"):
            start = pending.pop(index, None)
            windows.append(dict(index=index, offset_hz=record["offset_hz"],
                                start_ms=start["time_ms"]+guard_ms if start else None,
                                end_ms=record["time_ms"]-guard_ms,
                                valid=bool(start and name == "POINT_END" and record.get("status") == 0)))
    for index, start in pending.items():
        windows.append(dict(index=index, offset_hz=start["offset_hz"],
                            start_ms=start["time_ms"]+guard_ms, end_ms=None, valid=False))
    return windows


def assign_window(time_ms, windows):
    for window in windows:
        if (window["valid"] and window["start_ms"] <= time_ms <= window["end_ms"]):
            return window["index"], window["offset_hz"]
    return None, None


def period_from_rows(rows, tdd_count):
    # Adjacent sequence increments provide an estimate; at least 3 observations.
    candidates = [b["time_ms"]-a["time_ms"] for a, b in zip(rows, rows[1:])
                  if a["segment"] == b["segment"] and b["time_ms"] > a["time_ms"] and
                  (b["tdd"]-a["tdd"]) % tdd_count == 1 % tdd_count]
    if len(candidates) < 3:
        return 0
    center = statistics.median(candidates)
    # USB serial delivery is often quantized to ~16 ms. Its modal/median
    # interval can be 344 ms for a 350 ms radio cadence; use the mean of
    # central adjacent-sequence intervals, rejecting long gaps/batched bursts.
    central = [value for value in candidates if abs(value-center) <= center*.2]
    return statistics.mean(central) if len(central) >= 3 else center


def summarize(rows, transitions, bits_a, bits_b):
    joint = collections.Counter((r["bcnbits"], r["cfo_hz"]) for r in rows)
    groups = {}
    for bits in (bits_a, bits_b):
        values = [r["cfo_hz"] for r in rows if r["bcnbits"] == bits]
        groups[str(bits)] = dict(count=len(values), cfo_median=statistics.median(values) if values else None,
                                cfo_min=min(values) if values else None, cfo_max=max(values) if values else None)
    both = [groups[str(bits)] for bits in (bits_a, bits_b)]
    if not all(g["count"] >= 3 for g in both):
        association = "insufficient_samples"
    elif both[0]["cfo_max"] < both[1]["cfo_min"] or both[1]["cfo_max"] < both[0]["cfo_min"]:
        association = "separated_observed_groups"
    else:
        association = "overlapping_groups"
    return dict(samples=len(rows), bcnbits_errors=sum(r["bcnbits_error"] for r in rows),
                bcnbits_error_ratio=sum(r["bcnbits_error"] for r in rows)/len(rows) if rows else None,
                cfo_changes=sum(t["cfo_delta_hz"] != 0 for t in transitions),
                bcnbits_changes=sum(t["bcnbits_changed"] for t in transitions),
                cfo_change_same_bits=sum(t["cfo_delta_hz"] != 0 and not t["bcnbits_changed"] for t in transitions),
                missing_records_estimate=sum(t["missing"] or 0 for t in transitions) if len(rows) >= 2 else None,
                missing_count_scope="lower_bound_between_observed_records_only",
                uncertain_intervals=sum(t["missing"] is None for t in transitions),
                joint_distribution=[dict(bcnbits=k[0], cfo_hz=k[1], count=v) for k, v in sorted(joint.items())],
                cfo_groups=groups, association=association)


def analyze_records(records, tdd_count=10, bits_a=0, bits_b=1, guard_ms=500, period_ms=0):
    if not 1 <= tdd_count <= 255 or bits_a == bits_b or not all(0 <= v <= 31 for v in (bits_a, bits_b)):
        raise ValueError("TDD 数量或 bcnbits 参数无效")
    if guard_ms < 0 or not math.isfinite(period_ms) or period_ms < 0:
        raise ValueError("边界保护区及广播周期必须为非负有限数")
    records = sorted(records, key=lambda r: r["time_ms"])
    windows = make_windows(records, guard_ms)
    has_campaign = any(r["kind"] == "gateway_event" for r in records)
    rows, events, malformed, terminal_records = [], [], [], []
    network_epoch = 0
    was_joined = False
    disconnected_at = None
    join_count = 0
    for record in records:
        if record["kind"] != "terminal":
            continue
        text = record["text"]
        timestamp = record["time_ms"]
        record_index, record_offset = assign_window(timestamp, windows)
        terminal_records.append(dict(time_ms=timestamp, text=text, index=record_index,
                                     offset_hz=record_offset))
        rx = parse_rx(text)
        if rx:
            index, offset = assign_window(timestamp, windows)
            rx.update(time_ms=timestamp, source_line=record.get("source_line"), text=text,
                      index=index, offset_hz=offset, bcnbits_error=int(rx["bcnbits"] not in (bits_a, bits_b)),
                      segment=f"{index if has_campaign else 'sample'}:{network_epoch}")
            rows.append(rx)
        elif "RX_TDD:" in text:
            malformed.append(dict(time_ms=timestamp, text=text))
        state_match = NWK.search(text)
        if state_match:
            state = int(state_match.group(1))
            event = dict(time_ms=timestamp, state=state, kind="network_state", text=text)
            if state == 4:
                if not was_joined:
                    join_count += 1
                    event["kind"] = "joined" if join_count == 1 else "rejoined"
                    if disconnected_at is not None:
                        event["outage_ms"] = timestamp-disconnected_at
                    disconnected_at = None
                    was_joined = True
            elif state in (2, 3, 6) and was_joined:
                # 6 is used as disconnected in the existing WAN test suite.
                event["kind"] = "disconnected" if state == 6 else "rejoin_started"
                was_joined = False
                disconnected_at = timestamp
                network_epoch += 1
            index, offset = assign_window(timestamp, windows)
            event.update(index=index, offset_hz=offset)
            events.append(event)
        if "MAC AT CMD!" in text:
            network_epoch += 1
            was_joined = False
            events.append(dict(time_ms=timestamp, kind="terminal_boot", text=text))
    effective_period = period_ms or period_from_rows(rows, tdd_count)
    transitions = []
    for previous, current in zip(rows, rows[1:]):
        if previous["segment"] != current["segment"]:
            continue
        if has_campaign and current["index"] is None:
            continue
        missing, reason = infer_gap(previous, current, tdd_count, effective_period)
        transitions.append(dict(time_ms=current["time_ms"], previous_time_ms=previous["time_ms"],
                                index=current["index"], offset_hz=current["offset_hz"],
                                previous_tdd=previous["tdd"], tdd=current["tdd"],
                                cfo_delta_hz=current["cfo_hz"]-previous["cfo_hz"],
                                bcnbits_changed=int(current["bcnbits"] != previous["bcnbits"]),
                                missing=missing, timing=reason,
                                source_line=current["source_line"]))
    active_rows = [r for r in rows if r["index"] is not None] if has_campaign else rows
    summary = summarize(active_rows, transitions, bits_a, bits_b)
    summary.update(raw_samples=len(rows), malformed_count=len(malformed),
                   startup_retries=sum(r.get("event") == "POINT_RETRY" for r in records if r["kind"] == "gateway_event"),
                   frame_period_ms=effective_period, period_source="configured" if period_ms else "inferred",
                   tdd_count=tdd_count, bits_a=bits_a, bits_b=bits_b,
                   joins=join_count, rejoined=max(0, join_count-1),
                   disconnect_events=sum(e["kind"] == "disconnected" for e in events),
                   windows=windows, points=[])
    baseline_start = next((r["time_ms"] for r in records if r["kind"] == "baseline_start"), None)
    baseline_end = next((r["time_ms"] for r in records if r["kind"] == "baseline_end"), None)
    baseline_rows = ([r for r in rows if baseline_start <= r["time_ms"] <= baseline_end]
                     if baseline_start is not None and baseline_end is not None else [])
    baseline_a = [r for r in baseline_rows if r["bcnbits"] == bits_a]
    baseline_median = statistics.median(r["cfo_hz"] for r in baseline_a) if baseline_a else None
    summary["baseline_a"] = dict(samples=len(baseline_rows), a_samples=len(baseline_a),
                                 unexpected_bcnbits=sum(r["bcnbits"] != bits_a for r in baseline_rows),
                                 cfo_median=baseline_median,
                                 cfo_min=min((r["cfo_hz"] for r in baseline_a), default=None),
                                 cfo_max=max((r["cfo_hz"] for r in baseline_a), default=None),
                                 rssi_median=statistics.median(r["rssi"] for r in baseline_a) if baseline_a else None,
                                 snr_median=statistics.median(r["snr"] for r in baseline_a) if baseline_a else None)
    if not has_campaign:
        joins = [e["time_ms"] for e in events if e["kind"] == "joined"]
        if joins:
            joined_rows = [r for r in rows if r["time_ms"] >= joins[0]]
            joined_transitions = [t for t in transitions if t["previous_time_ms"] >= joins[0]]
            summary["joined_window_including_same_receive_block"] = summarize(joined_rows, joined_transitions, bits_a, bits_b)
    for window in windows:
        selected = [r for r in rows if r["index"] == window["index"]]
        changes = [t for t in transitions if t["index"] == window["index"]]
        result = dict(window, **summarize(selected, changes, bits_a, bits_b))
        result["startup_retries"] = sum(r.get("event") == "POINT_RETRY" and
                                        r.get("index") == window["index"] for r in records
                                        if r["kind"] == "gateway_event")
        a = result["cfo_groups"][str(bits_a)]["cfo_median"]
        b = result["cfo_groups"][str(bits_b)]["cfo_median"]
        result["observed_b_minus_a_hz"] = b-a if a is not None and b is not None else None
        result["offset_abs_residual_hz"] = abs(b-a)-abs(window["offset_hz"]) if a is not None and b is not None else None
        result["a_drift_from_baseline_hz"] = a-baseline_median if a is not None and baseline_median is not None else None
        result["disconnect_events"] = sum(e["kind"] == "disconnected" and e.get("index") == window["index"] for e in events)
        result["rejoin_events"] = sum(e["kind"] == "rejoined" and e.get("index") == window["index"] for e in events)
        result["matching_note"] = "descriptive_only_no_independent_gateway_CFO_reference"
        edges = ([window["start_ms"]]+[r["time_ms"] for r in selected]+[window["end_ms"]]
                 if window["valid"] else [])
        result["max_no_log_ms"] = max((b-a for a, b in zip(edges, edges[1:])), default=None)
        result["reception_coverage"] = "no_samples" if not selected else "observed_samples"
        if not window["valid"]:
            result["reception_coverage"] = "invalid_window"
        summary["points"].append(result)
    # Nearest empirical group is an association diagnostic, not an independent decoder oracle.
    references = {p["index"]: p for p in summary["points"]} if has_campaign else {None: summary}
    for row in rows:
        reference = references.get(row["index"])
        row["cfo_bcnbits_relation"] = "outside_valid_window"
        if row["bcnbits_error"]:
            row["cfo_bcnbits_relation"] = "bcnbits_error"
        elif reference:
            groups = reference["cfo_groups"]
            ga, gb = groups[str(bits_a)], groups[str(bits_b)]
            a, b = ga["cfo_median"], gb["cfo_median"]
            if min(ga["count"], gb["count"]) < 3:
                row["cfo_bcnbits_relation"] = "insufficient_reference_samples"
            elif a == b or (has_campaign and row["offset_hz"] == 0):
                row["cfo_bcnbits_relation"] = "indistinguishable"
            else:
                da, db = abs(row["cfo_hz"]-a), abs(row["cfo_hz"]-b)
                row["distance_to_a_median_hz"], row["distance_to_b_median_hz"] = da, db
                nearest = bits_a if da < db else bits_b if db < da else None
                row["cfo_bcnbits_relation"] = ("indistinguishable" if nearest is None else
                    "consistent_with_observed_groups" if nearest == row["bcnbits"] else "suspected_cross_group")
    summary["cfo_bcnbits_relation_counts"] = dict(collections.Counter(r["cfo_bcnbits_relation"] for r in active_rows))
    return dict(summary=summary, samples=rows, transitions=transitions, events=events,
                malformed=malformed, terminal_records=terminal_records)


def write_csv(path, records):
    with Path(path).open("w", encoding="utf-8-sig", newline="") as stream:
        if records:
            fields = list(dict.fromkeys(k for row in records for k in row))
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerows(records)


def write_report(result, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    summary = result["summary"]
    (output/"summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    for name in ("samples", "transitions", "events", "malformed"):
        write_csv(output/(name+".csv"), result[name])
    write_csv(output/"bcnbits_errors.csv", [r for r in result["samples"] if r["bcnbits_error"]])
    write_csv(output/"cfo_bcnbits_matching.csv", result["samples"])
    write_csv(output/"gaps.csv", [r for r in result["transitions"] if r["missing"] != 0])
    flat_points = [{k: v for k, v in point.items() if not isinstance(v, (dict, list))} for point in summary["points"]]
    write_csv(output/"points.csv", flat_points)
    for point in summary["points"]:
        directory = output/f"point_{point['index']:04d}_{point['offset_hz']:+d}Hz"
        directory.mkdir(exist_ok=True)
        selected = [r for r in result["samples"] if r["index"] == point["index"]]
        write_csv(directory/"terminal.csv", selected)
        log_rows = [r for r in result["terminal_records"] if r["index"] == point["index"]]
        (directory/"terminal.log").write_text("\n".join(f"[{r['time_ms']:.3f}] {r['text']}" for r in log_rows), encoding="utf-8")
    for filename, log_rows in (("terminal_raw.log", result["terminal_records"]),
                               ("transition_terminal.log", [r for r in result["terminal_records"] if r["index"] is None])):
        (output/filename).write_text("\n".join(f"[{r['time_ms']:.3f}] {r['text']}" for r in log_rows), encoding="utf-8")
    chart_html = ""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        rows = result["samples"]
        if rows:
            start = rows[0]["time_ms"]
            figure, axes = plt.subplots(2, 1, figsize=(12, 6), sharex=True, layout="constrained")
            for bits in sorted({r["bcnbits"] for r in rows}):
                selected = [r for r in rows if r["bcnbits"] == bits]
                axes[0].scatter([(r["time_ms"]-start)/1000 for r in selected], [r["cfo_hz"] for r in selected], s=8, label=f"bcnbits={bits}")
            axes[0].legend(); axes[0].set_ylabel("CFO (Hz)")
            axes[1].scatter([(r["time_ms"]-start)/1000 for r in rows], [r["bcnbits"] for r in rows], s=8)
            axes[1].set_ylabel("bcnbits"); axes[1].set_xlabel("PC receive time (seconds)")
            for axis in axes:
                axis.grid(alpha=.2)
                for window in summary["windows"]:
                    if window["start_ms"] is not None:
                        axis.axvline((window["start_ms"]-start)/1000, color="gray", alpha=.3)
            figure.savefig(output/"cfo_bcnbits.png", dpi=140)
            plt.close(figure)
            chart_html = '<img src="cfo_bcnbits.png" style="max-width:100%" alt="CFO and bcnbits">'
    except ImportError:
        chart_html = "<p>未安装 matplotlib，CSV/JSON 分析已完成。</p>"
    title = "同频同步组网频差测试分析"
    body = f"""<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>{title}</title>
<style>body{{font:16px sans-serif;max-width:1100px;margin:30px auto;padding:0 20px}}pre{{white-space:pre-wrap;background:#f3f5f7;padding:20px}}</style>
<h1>{title}</h1><p>有效窗口接收 {summary['samples']} 条；bcnbits 集合外错误 {summary['bcnbits_errors']} 条；
CFO 变化 {summary['cfo_changes']} 次；bcnbits 变化 {summary['bcnbits_changes']} 次。</p>
<p>缺失为日志记录缺失估计，不能直接归因于射频丢包。边界保护区及未完成频点不进入有效窗口统计。
CFO 分组及频差残差为描述统计，没有独立参考时不判定逐帧来源正确性；重叠分组视为无法区分。
未观测到错误不代表全部广播均正确解析。</p>{chart_html}<h2>统计明细</h2>
<pre>{html.escape(json.dumps(summary, ensure_ascii=False, indent=2))}</pre></html>"""
    (output/"report.html").write_text(body, encoding="utf-8")
    return output/"report.html"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tdd-count", type=int, default=10)
    parser.add_argument("--bits-a", type=int, default=0)
    parser.add_argument("--bits-b", type=int, default=1)
    parser.add_argument("--guard-ms", type=int, default=500)
    parser.add_argument("--period-ms", type=float, default=0)
    args = parser.parse_args()
    result = analyze_records(list(load_records(args.log)), args.tdd_count, args.bits_a, args.bits_b, args.guard_ms, args.period_ms)
    print(write_report(result, args.output))


if __name__ == "__main__":
    main()
