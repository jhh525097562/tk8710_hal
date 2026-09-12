"""Rejudge saved case-6 evidence offline; back up verdicts before replacement."""
import argparse
import copy
import json
import shutil
from datetime import datetime
from pathlib import Path

from sync_network_test import SlotCombination, evaluate_packet, evaluate_combination, _write_reports


def totals(records):
    return {str(case): {
        "combinations": len(rows),
        "passed_combinations": sum(bool(r["passed"]) for r in rows),
        "passed_packets": sum(r["passed_packet_count"] for r in rows),
        "expected_packets": sum(r["expected_packet_count"] for r in rows),
    } for case in (4, 5, 6)
        for rows in [[r for r in records if r["combination"]["case"] == case]]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    root = args.run_dir.resolve()
    read = lambda p: json.loads(p.read_text(encoding="utf-8"))
    original = read(root / "summary.json")["records"]
    records = copy.deepcopy(original)
    gateways = [g["gw_id"].upper() for g in read(root / "config.redacted.json")["gateways"]]
    changes = {}
    changed_packets = 0
    for record in records:
        if record["combination"]["case"] != 6:
            continue
        combo = SlotCombination(**record["combination"])
        result_path = root / combo.key / "result.json"
        assert read(result_path) == record, f"summary/result disagreement: {combo.key}"
        for packet in record["packets"]:
            evidence_path = root / combo.key / f"packet_{packet['index']:02d}" / "evidence.json"
            assert read(evidence_path) == packet, f"packet disagreement: {evidence_path}"
            verdict = evaluate_packet(6, combo, gateways, packet["uplinks"],
                                      packet["terminal"], packet["gateway_windows"])
            expected = copy.deepcopy(packet["verdict"])
            expected["reasons"] = [r for r in expected["reasons"]
                if r != "received_rate_is_not_configured_lowest_rate"]
            expected["passed"] = not expected["reasons"]
            assert verdict == expected, f"unrelated verdict change: {evidence_path}"
            changed_packets += verdict != packet["verdict"]
            packet["verdict"] = verdict
            changes[evidence_path] = packet
        record.update(evaluate_combination(record["packets"],
            record["expected_packet_count"], record["success_threshold"]))
        if record.get("error"):
            record["passed"] = False
        changes[result_path] = record

    backup = root / ("rejudge_backup_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
    backup.mkdir(exist_ok=False)
    for path in [*changes, *(root / name for name in ("summary.json", "summary.csv", "report.md"))]:
        destination = backup / path.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    for path, value in changes.items():
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
    _write_reports(root, records)
    audit = {"rule": "case6: equal received rates; all other checks retained",
             "before": totals(original), "after": totals(records),
             "changed_packet_verdicts": changed_packets, "backup": str(backup)}
    (root / "case6_rejudge.json").write_text(
        json.dumps(audit, ensure_ascii=False, indent=2), encoding="utf-8")
    report = root / "report.md"
    report.write_text(report.read_text(encoding="utf-8") +
        "\n用例6已按两台网关接收速率一致重判，不要求最低速率；其他条件不变。"
        "本次为离线重判，未重新运行硬件测试。详见 case6_rejudge.json。\n",
        encoding="utf-8")
    assert read(root / "summary.json")["records"] == records
    assert [r for r in original if r["combination"]["case"] != 6] == [
        r for r in records if r["combination"]["case"] != 6]
    for path, value in changes.items():
        assert read(path) == value
    print(json.dumps(audit, indent=2))


if __name__ == "__main__":
    main()
