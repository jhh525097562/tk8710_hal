from __future__ import annotations

import json
import threading
from dataclasses import asdict
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, Iterable

from .models import CaseResult, RunSummary, Verdict


def now_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def create_run_dir(root: Path) -> tuple[str, Path]:
    run_id = datetime.now().strftime("PAYLOAD_%Y%m%d_%H%M%S_%f")[:-3]
    path = root / "results" / datetime.now().strftime("%Y-%m-%d") / run_id
    path.mkdir(parents=True, exist_ok=False)
    return run_id, path


class RunLogger:
    def __init__(self, directory: Path, sink=None):
        self.directory = directory
        self.sink = sink or (lambda source, text: None)
        self.lock = threading.Lock()
        self.events = (directory / "events.jsonl").open("a", encoding="utf-8")
        self.files: Dict[str, Any] = {}

    def log(self, source: str, text: str, **data: Any) -> None:
        timestamp = now_iso()
        event = {"timestamp": timestamp, "source": source, "message": text, **data}
        with self.lock:
            self.events.write(json.dumps(event, ensure_ascii=False, default=str) + "\n")
            self.events.flush()
            safe = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in source)
            stream = self.files.get(safe)
            if stream is None:
                stream = (self.directory / f"{safe}.log").open("a", encoding="utf-8")
                self.files[safe] = stream
            stream.write(f"[{timestamp}] {text}\n"); stream.flush()
        self.sink(source, text)

    def serial_sink(self, port: str, direction: str, line: str) -> None:
        self.log(f"serial_{port}", f"{direction.upper()} {line}")

    def mqtt_sink(self, topic: str, payload: bytes) -> None:
        text = payload.decode("utf-8", "replace")
        self.log("mqtt", f"{topic} {text}")

    def close(self) -> None:
        for stream in self.files.values(): stream.close()
        self.events.close()


def write_summary(summary: RunSummary, directory: Path) -> None:
    with (directory / "summary.json").open("w", encoding="utf-8") as stream:
        json.dump(summary.as_dict(), stream, ensure_ascii=False, indent=2, default=str)


def update_markdown_report(template: Path, destination: Path,
                           results: Iterable[CaseResult]) -> None:
    text = template.read_text(encoding="utf-8")
    mapping = {result.case_id: result for result in results}
    lines = text.splitlines()
    in_section = False
    for index, line in enumerate(lines):
        if line.startswith("### 3.6 "):
            in_section = True; continue
        if in_section and line.startswith("### "):
            break
        if not in_section or not line.startswith("| RF-"):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 7 or cells[0] not in mapping: continue
        result = mapping[cells[0]]
        evidence = "；".join(item.message.replace("|", "\\|").replace("\n", " ")
                            for item in result.evidence[:4])
        cells[-2] = result.summary + (("；" + evidence) if evidence else "")
        cells[-1] = result.verdict.value if result.verdict in (Verdict.PASS, Verdict.FAILED) else ""
        lines[index] = "| " + " | ".join(cells) + " |"
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_excel(summary: RunSummary, destination: Path) -> None:
    try:
        from openpyxl import Workbook
    except ImportError:
        csv_path = destination.with_suffix(".csv")
        rows = ["编号,测试项,判定,结果"]
        rows.extend(f'{c.case_id},{c.name},{c.verdict.value},"{c.summary.replace(chr(34), chr(34)*2)}"'
                    for c in summary.cases)
        csv_path.write_text("\ufeff" + "\n".join(rows), encoding="utf-8")
        return
    book = Workbook()
    sheet = book.active
    sheet.title = "3.6测试结果"
    sheet.append(["编号", "测试项", "判定", "开始时间", "结束时间", "结果"])
    for case in summary.cases:
        sheet.append([case.case_id, case.name, case.verdict.value,
                      case.started_at, case.ended_at, case.summary])
    for column, width in zip("ABCDEF", (12, 30, 12, 24, 24, 80)):
        sheet.column_dimensions[column].width = width
    book.save(destination)
