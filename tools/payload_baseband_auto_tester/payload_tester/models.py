from __future__ import annotations

from dataclasses import asdict, dataclass, field
from enum import Enum
from typing import Any, Dict, List


class Verdict(str, Enum):
    PASS = "PASS"
    FAILED = "FAILED"
    SKIP = "SKIP"
    BLOCKED = "BLOCKED"


@dataclass
class Evidence:
    source: str
    message: str
    data: Dict[str, Any] = field(default_factory=dict)


@dataclass
class CaseResult:
    case_id: str
    name: str
    verdict: Verdict
    summary: str
    started_at: str
    ended_at: str
    evidence: List[Evidence] = field(default_factory=list)

    def as_dict(self) -> Dict[str, Any]:
        value = asdict(self)
        value["verdict"] = self.verdict.value
        return value


@dataclass
class RunSummary:
    run_id: str
    started_at: str
    ended_at: str = ""
    cases: List[CaseResult] = field(default_factory=list)
    environment: Dict[str, Any] = field(default_factory=dict)

    def as_dict(self) -> Dict[str, Any]:
        return {
            "run_id": self.run_id,
            "started_at": self.started_at,
            "ended_at": self.ended_at,
            "environment": self.environment,
            "cases": [case.as_dict() for case in self.cases],
        }
