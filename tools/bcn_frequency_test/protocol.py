"""Pure parsing and experiment configuration; no device I/O."""
from __future__ import annotations

import dataclasses
import math
import re
from dataclasses import dataclass

RX = re.compile(r"RX_TDD:\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*$")
NWK = re.compile(r"\+NWKINFO:(\d+)\s*$")
RF_STEP = 30.517578125


@dataclass
class Config:
    host: str = ""
    username: str = "root"
    ssh_port: int = 22
    serial_port: str = "auto"
    baudrate: int = 115200
    primary_channel: int = 7
    secondary_channel: int = 6
    rate: int = 8
    base_hz: int = 496800000
    start_hz: int = -1000
    end_hz: int = 1000
    step_hz: int = 100
    hold_seconds: int = 60
    tdd_count: int = 10
    ul_blocks: int = 2
    dl_blocks: int = 2
    bits_a: int = 0
    bits_b: int = 1
    rf_gain: int = 42
    ready_timeout: int = 600
    join_timeout: int = 120
    settle_seconds: int = 2
    startup_retries: int = 2
    baseline_seconds: int = 5
    boundary_guard_ms: int = 500
    frame_period_ms: float = 0.0
    known_hosts: str = ""
    binary: str = "build_freq_test/tk8710_gw_freq_test"

    def validate(self):
        for name in ("base_hz", "start_hz", "end_hz", "step_hz", "hold_seconds",
                     "tdd_count", "ul_blocks", "dl_blocks", "rate", "bits_a", "bits_b",
                     "rf_gain", "ready_timeout", "join_timeout", "settle_seconds",
                     "baseline_seconds", "boundary_guard_ms", "ssh_port", "baudrate", "startup_retries",
                     "primary_channel", "secondary_channel"):
            if type(getattr(self, name)) is not int:
                raise ValueError(f"{name} 必须为整数")
        if not self.host.strip() or any(c.isspace() for c in self.host):
            raise ValueError("需要有效的网关 B 主机名或 IP")
        if not 1 <= self.ssh_port <= 65535 or self.baudrate <= 0:
            raise ValueError("SSH 端口或串口波特率无效")
        if not 1 <= self.primary_channel <= 10 or not 1 <= self.secondary_channel <= 8:
            raise ValueError("主信道号必须为 1..10，次信道号为 1..8")
        if self.rate not in (*range(5, 12), 18):
            raise ValueError("速率应为 5..11 或 18")
        for name, low, high in (("tdd_count", 1, 255), ("ul_blocks", 1, 255),
                                ("dl_blocks", 1, 255), ("rf_gain", 0, 255),
                                ("hold_seconds", 1, 86400), ("ready_timeout", 1, 3600),
                                ("join_timeout", 1, 3600), ("settle_seconds", 0, 300),
                                ("baseline_seconds", 1, 300), ("boundary_guard_ms", 0, 10000),
                                ("startup_retries", 0, 5)):
            if not low <= getattr(self, name) <= high:
                raise ValueError(f"{name} 超出 {low}..{high}")
        if self.boundary_guard_ms * 2 >= self.hold_seconds * 1000:
            raise ValueError("边界保护区必须小于每点时长的一半")
        if not 0 <= self.bits_a <= 31 or not 0 <= self.bits_b <= 31 or self.bits_a == self.bits_b:
            raise ValueError("两个 bcnbits 必须不同且在 0..31 内")
        if not math.isfinite(self.frame_period_ms) or self.frame_period_ms < 0:
            raise ValueError("广播周期必须为非负有限数；0 表示从日志推断")
        if any(abs(v) > 1000000 for v in (self.start_hz, self.end_hz, self.step_hz)):
            raise ValueError("频差/步进范围限制为 ±1000000 Hz")
        if not self.step_hz or (self.end_hz-self.start_hz)*self.step_hz < 0:
            raise ValueError("步进方向错误或为 0")
        if (self.end_hz-self.start_hz) % self.step_hz:
            raise ValueError("步进必须能精确到达结束频差")
        points = (self.end_hz-self.start_hz)//self.step_hz+1
        if points > 10000:
            raise ValueError("最多 10000 个频点")
        if any(not 400000000 <= self.base_hz+v <= 510000000 for v in (0, self.start_hz, self.end_hz)):
            raise ValueError("SX1255 测试频率范围限定为 400..510 MHz")
        return self

    def offsets(self):
        return [self.start_hz+i*self.step_hz
                for i in range((self.end_hz-self.start_hz)//self.step_hz+1)]

    def gateway_args(self):
        keys = {"base-hz": self.base_hz, "start-hz": self.start_hz, "end-hz": self.end_hz,
                "step-hz": self.step_hz, "hold-seconds": self.hold_seconds,
                "bcnbits": self.bits_b, "rate": self.rate, "tdd-count": self.tdd_count,
                "ul-blocks": self.ul_blocks, "dl-blocks": self.dl_blocks,
                "rf-gain": self.rf_gain, "ready-timeout": self.ready_timeout,
                "settle-seconds": self.settle_seconds, "startup-retries": self.startup_retries}
        return [part for key, value in keys.items() for part in ("--"+key, str(value))]

    def as_dict(self):
        return dataclasses.asdict(self)


def parse_rx(line):
    match = RX.search(line)
    if not match:
        return None
    return dict(zip(("tdd", "rssi", "snr", "cfo_hz", "bcnbits"), map(int, match.groups())))


def infer_gap(previous, current, tdd_count, period_ms):
    """Return missing count only when sequence and receive timestamps agree.

    A conservative 20% period tolerance handles serial receive jitter. This is
    a timing-consistency tolerance, never a CFO error threshold.
    """
    delta_ms = current["time_ms"]-previous["time_ms"]
    if not (1 <= previous["tdd"] <= tdd_count and 1 <= current["tdd"] <= tdd_count):
        return None, "invalid_tdd"
    if delta_ms <= 0 or period_ms <= 0:
        return None, "unknown_timing"
    steps = int(math.floor(delta_ms/period_ms+0.5))
    if steps < 1 or abs(delta_ms-steps*period_ms) > period_ms*0.2:
        return None, "timing_mismatch"
    if steps % tdd_count != (current["tdd"]-previous["tdd"]) % tdd_count:
        return None, "sequence_mismatch"
    return steps-1, "consistent"
