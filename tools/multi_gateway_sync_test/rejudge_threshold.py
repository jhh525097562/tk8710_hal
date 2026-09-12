"""Update combination thresholds offline, retaining packet verdicts and backups."""
import argparse
import copy
import json
import shutil
from datetime import datetime

from pathlib import Path
from sync_network_test import evaluate_combination, _write_reports
from rejudge_case6 import totals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir", type=Path)
    args = parser.parse_args()
    root = args.run_dir.resolve()
    read = lambda p: json.loads(p.read_text(encoding="utf-8"))
    original = read(root / "summary.json")["records"]
    records = copy.deepcopy(original)
    for record in records:
        assert record["expected_packet_count"] == 10
        assert read(root / record["combination"]["key"] / "result.json") == record
        record.update(evaluate_combination(record["packets"], 10, 0.70))
        if record.get("error"):
            record["passed"] = False
    backup = root / ("threshold_backup_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
    backup.mkdir(exist_ok=False)
    paths = [root / name for name in ("summary.json", "summary.csv", "report.md")]
    paths.extend(root / r["combination"]["key"] / "result.json" for r in records)
    for path in paths:
        destination = backup / path.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    for record in records:
        path = root / record["combination"]["key"] / "result.json"
        path.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
    _write_reports(root, records)
    report = root / "report.md"
    report.write_text(report.read_text(encoding="utf-8") +
        "\n组合通过条件：10包完成，成功至少7包（≥70%）；运行错误仍判失败。\n"
        "用例6速率条件：两台网关接收速率相同，不要求最低速率；其他逐包条件不变。\n"
        "本次为离线重判，未重新运行硬件测试。详见 threshold_rejudge.json。\n",
        encoding="utf-8")
    audit = {"before": totals(original), "after": totals(records), "backup": str(backup),
             "rule": "completed 10 packets, >=7 successful; retain runtime failures",
             "changed_combinations": [r["combination"]["key"] for old, r in zip(original, records)
                                      if old["passed"] != r["passed"]]}
    (root / "threshold_rejudge.json").write_text(
        json.dumps(audit, ensure_ascii=False, indent=2), encoding="utf-8")
    assert read(root / "summary.json")["records"] == records
    for old, record in zip(original, records):
        assert old["packets"] == record["packets"]
        assert read(root / record["combination"]["key"] / "result.json") == record
    print(json.dumps(audit, indent=2))


if __name__ == "__main__":
    main()
