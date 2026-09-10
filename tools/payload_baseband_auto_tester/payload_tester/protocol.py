from __future__ import annotations

import math
import re
import struct
from dataclasses import dataclass, field
from typing import Any, Dict, Iterable, List, Optional, Tuple

RC_LEN = 10
SPI1_PAGE_LEN = 128
TM_LEN = 144
DT_FRAME_LEN = 512
DT_DATA_LEN = 502
DT_SYNC = 0x1ACFFC1D


def checksum8(data: bytes, start: int, end: int) -> int:
    return sum(data[start:end]) & 0xFF


def build_remote_control(command: int, value: Any = None) -> bytes:
    if not 1 <= command <= 0x0E:
        raise ValueError("遥控命令必须为0x01..0x0E")
    frame = bytearray(RC_LEN)
    frame[0:3] = bytes((0x76, 0x25, command))
    if command in (0x01, 0x02, 0x03, 0x04, 0x06):
        frame[3] = int(value) & 0xFF
    elif command == 0x09:
        if value is None or int(value) not in (0, 1):
            raise ValueError("CMD_09 data transfer switch must be 0 or 1")
        frame[3] = int(value)
    elif command in (0x05, 0x0D):
        frame[3:7] = int(value).to_bytes(4, "big")
    elif command == 0x0A:
        address, register_value = value
        frame[3:5] = int(address).to_bytes(2, "big")
        frame[5:9] = int(register_value).to_bytes(4, "big")
    elif command == 0x0B:
        if isinstance(value, tuple):
            device, address = value
        else:
            device, address = 0, value
        frame[3:5] = int(device).to_bytes(2, "big")
        frame[5:7] = int(address).to_bytes(2, "big")
    elif command == 0x0E:
        antenna, i_dc, q_dc = value
        frame[3] = int(antenna) & 0xFF
        frame[4:6] = (int(i_dc) & 0xFFFF).to_bytes(2, "big")
        frame[6:8] = (int(q_dc) & 0xFFFF).to_bytes(2, "big")
    elif command in (0x08, 0x0C) and value is not None:
        raw = bytes(value)
        frame[3:3 + min(6, len(raw))] = raw[:6]
    frame[9] = checksum8(frame, 2, 9)
    return bytes(frame)


def spi1_page_for_command(command: int, value: Any = None) -> bytes:
    return build_remote_control(command, value) + bytes(SPI1_PAGE_LEN - RC_LEN)


def validate_telemetry(frame: bytes) -> bool:
    return len(frame) == TM_LEN and frame[:2] == b"\xEB\x90" and checksum8(frame, 2, 143) == frame[143]


def _s16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "big", signed=True)


def parse_telemetry(frame: bytes) -> Dict[str, Any]:
    if not validate_telemetry(frame):
        raise ValueError("遥测帧头、长度或checksum错误")
    return {
        "noise_dbm_hz": [_s16(frame, i) / 128.0 for i in range(2, 18, 2)],
        "mode": frame[20], "rate": frame[21], "slot_config": frame[22],
        "tx_power": frame[23], "frequency_hz": int.from_bytes(frame[24:28], "big"),
        "rf_mask": frame[28], "version": f"{frame[29]}.{frame[30]}.{frame[31]}",
        "uptime_s": int.from_bytes(frame[32:36], "big"),
        "rx_count": int.from_bytes(frame[36:40], "big"),
        "tx_count": int.from_bytes(frame[40:44], "big"),
        "reset_count": frame[44], "reset_type": frame[45],
        "reg_device": int.from_bytes(frame[63:65], "big"),
        "reg_address": int.from_bytes(frame[65:67], "big"),
        "reg_value": int.from_bytes(frame[67:71], "big"),
        "acm": [
            {"i": int.from_bytes(frame[71 + i * 8:75 + i * 8], "big", signed=True),
             "q": int.from_bytes(frame[75 + i * 8:79 + i * 8], "big", signed=True)}
            for i in range(8)
        ],
        "raw_hex": frame.hex().upper(),
    }


class TelemetryAssembler:
    def __init__(self) -> None:
        self.page0: Optional[bytes] = None

    def feed(self, page: bytes) -> Optional[Dict[str, Any]]:
        if len(page) != SPI1_PAGE_LEN:
            raise ValueError("SPI1页必须为128字节")
        if page[:2] == b"\xEB\x90":
            self.page0 = page
            return None
        if self.page0 is None:
            return None
        frame = self.page0 + page[:TM_LEN - SPI1_PAGE_LEN]
        self.page0 = None
        return parse_telemetry(frame) if validate_telemetry(frame) else None


def dt_checksum(frame: bytes) -> int:
    if len(frame) != DT_FRAME_LEN:
        raise ValueError("数传帧必须为512字节")
    return sum(int.from_bytes(frame[i:i + 2], "big") for i in range(0, 510, 2)) & 0xFFFF


def parse_dt_frame(frame: bytes) -> Tuple[int, bytes]:
    if len(frame) != DT_FRAME_LEN or int.from_bytes(frame[:4], "big") != DT_SYNC:
        raise ValueError("数传帧同步头或长度错误")
    length = int.from_bytes(frame[4:6], "big")
    if length > DT_DATA_LEN:
        raise ValueError("数传有效长度越界")
    if dt_checksum(frame) != int.from_bytes(frame[510:512], "big"):
        raise ValueError("数传帧checksum错误")
    return int.from_bytes(frame[6:8], "big"), frame[8:8 + length]


@dataclass
class DataRecord:
    timestamp: int
    format: int
    type: int
    payload: bytes
    decoded: Dict[str, Any] = field(default_factory=dict)


def decode_record_payload(record_type: int, payload: bytes) -> Dict[str, Any]:
    if record_type == 0x01:
        return {"text": payload.decode("utf-8", "replace")}
    if record_type == 0x02 and len(payload) >= 96:
        return {
            "version": payload[0], "rate": payload[1],
            "rssi": int.from_bytes(payload[2:4], "big", signed=True), "snr": payload[4],
            "data_len": int.from_bytes(payload[6:8], "big"),
            "frame_no": int.from_bytes(payload[8:12], "big"),
            "user_id": int.from_bytes(payload[12:16], "big"),
            "freq_offset": int.from_bytes(payload[16:20], "big", signed=True),
            "frequency_hz": int.from_bytes(payload[20:24], "big"),
            "pilot_power": int.from_bytes(payload[24:32], "big"),
            "ah": [int.from_bytes(payload[32 + i * 4:36 + i * 4], "big") for i in range(16)],
        }
    if record_type == 0x03 and len(payload) >= 20:
        length = int.from_bytes(payload[16:18], "big")
        return {
            "version": payload[0], "rate": payload[1], "antenna": payload[2],
            "sample_format": payload[3], "generation": int.from_bytes(payload[4:8], "big"),
            "bytes_per_antenna": int.from_bytes(payload[8:12], "big"),
            "offset": int.from_bytes(payload[12:16], "big"), "length": length,
            "data": payload[20:20 + length],
        }
    if record_type == 0x04 and len(payload) >= 32:
        point_count = int.from_bytes(payload[28:30], "big")
        points = []
        for i in range(point_count):
            offset = 32 + i * 36
            if offset + 36 > len(payload): break
            noise = list(struct.unpack(">8f", payload[offset + 4:offset + 36]))
            points.append({"frequency_hz": int.from_bytes(payload[offset:offset + 4], "big"),
                           "noise_dbm_hz": noise})
        return {
            "version": payload[0], "sweep_mode": payload[1], "rate": payload[2],
            "antenna_count": payload[3], "generation": int.from_bytes(payload[4:8], "big"),
            "start_frequency_hz": int.from_bytes(payload[8:12], "big"),
            "end_frequency_hz": int.from_bytes(payload[12:16], "big"),
            "step_frequency_hz": int.from_bytes(payload[16:20], "big"),
            "total_points": int.from_bytes(payload[20:24], "big"),
            "start_index": int.from_bytes(payload[24:28], "big"),
            "point_count": point_count, "points": points,
        }
    return {"raw_hex": payload.hex().upper()}


class RecordDecoder:
    def __init__(self) -> None:
        self.pending = bytearray()
        self.last_seq: Optional[int] = None

    def feed_frame(self, frame: bytes) -> List[DataRecord]:
        seq, payload = parse_dt_frame(frame)
        if self.last_seq is not None and seq != ((self.last_seq + 1) & 0xFFFF):
            raise ValueError(f"数传包序号不连续: {self.last_seq}->{seq}")
        self.last_seq = seq
        self.pending.extend(payload)
        records: List[DataRecord] = []
        while len(self.pending) >= 8:
            length = int.from_bytes(self.pending[6:8], "big")
            total = 8 + length
            if len(self.pending) < total: break
            timestamp = int.from_bytes(self.pending[:4], "big")
            record_format, record_type = self.pending[4], self.pending[5]
            body = bytes(self.pending[8:total])
            del self.pending[:total]
            records.append(DataRecord(timestamp, record_format, record_type, body,
                                      decode_record_payload(record_type, body)))
        return records


def validate_capture(records: Iterable[DataRecord],
                     minimum_generation: int = 0) -> Dict[str, Any]:
    chunks = [r.decoded for r in records if r.type == 0x03]
    generations = sorted({int(c["generation"]) for c in chunks
                          if c.get("generation") is not None and
                          int(c["generation"]) >= minimum_generation})
    for generation in generations:
        group = [c for c in chunks if c.get("generation") == generation]
        antennas = sorted({c["antenna"] for c in group})
        if antennas != list(range(8)): continue
        valid = True
        for antenna in antennas:
            parts = sorted((c for c in group if c["antenna"] == antenna), key=lambda c: c["offset"])
            expected = 0
            total = parts[0]["bytes_per_antenna"]
            for part in parts:
                if part["offset"] != expected or part["length"] != len(part["data"]): valid = False; break
                expected += part["length"]
            if expected != total: valid = False
        if valid:
            return {"valid": True, "generation": generation, "antennas": 8,
                    "bytes_per_antenna": group[0]["bytes_per_antenna"]}
    return {"valid": False,
            "reason": f"没有重组出generation>={minimum_generation}的8天线完整采数"}


def validate_sweep(records: Iterable[DataRecord], required_generations: int = 2,
                   minimum_generation: int = 0) -> Dict[str, Any]:
    chunks = [r.decoded for r in records if r.type == 0x04]
    complete = []
    generations = sorted({int(c["generation"]) for c in chunks
                          if c.get("generation") is not None and
                          int(c["generation"]) >= minimum_generation})
    for generation in generations:
        group = sorted((c for c in chunks if c.get("generation") == generation),
                       key=lambda c: c["start_index"])
        points: List[Dict[str, Any]] = []
        expected_index = 0
        valid = True
        for chunk in group:
            if chunk["start_index"] != expected_index or chunk["antenna_count"] != 8:
                valid = False; break
            points.extend(chunk["points"])
            expected_index += chunk["point_count"]
        if not group or expected_index != group[0]["total_points"]: valid = False
        total_points = group[0]["total_points"] if group else 0
        start_frequency = group[0]["start_frequency_hz"] if group else 0
        end_frequency = group[0]["end_frequency_hz"] if group else 0
        step_frequency = group[0]["step_frequency_hz"] if group else 0
        expected_freqs = [start_frequency + step_frequency * i
                          for i in range(total_points)]
        if expected_freqs and expected_freqs[-1] != end_frequency:
            valid = False
        if valid and [p["frequency_hz"] for p in points] == expected_freqs and all(
                len(p["noise_dbm_hz"]) == 8 and all(math.isfinite(x) for x in p["noise_dbm_hz"])
                for p in points):
            complete.append(generation)
    return {"valid": len(complete) >= required_generations, "generations": complete,
            "required_generations": required_generations,
            "minimum_generation": minimum_generation}


_FIELD_PATTERNS = {
    "rx_frames": r"FPGA_TM rxFrames=(\d+)", "rx_errors": r"rxErrors=(\d+)",
    "last_cmd": r"lastCmd=0x([0-9A-Fa-f]+)", "physical_rx": r"physicalRx=(\d+)",
    "mode": r"FPGA_PARAM mode=(\d+)", "rate": r"rate=(\d+)",
    "frequency_hz": r"freqHz=(\d+)", "rf_mask": r"rfMask=(\d+)",
    "reset_count": r"resetCount=(\d+)", "dt_pending": r"pending=(\d+)",
    "dt_active": r"active=(\d+)", "dt_starts": r"starts=(\d+)",
    "dt_built": r"built=(\d+)", "dt_sent": r"sent=(\d+)", "dt_send_error": r"sendErr=(\d+)",
}


def parse_fpga_tm(text: str) -> Dict[str, int]:
    result: Dict[str, int] = {}
    for key, pattern in _FIELD_PATTERNS.items():
        match = re.search(pattern, text)
        if match: result[key] = int(match.group(1), 16 if key == "last_cmd" else 10)
    frame_match = re.search(r"lastPhysicalRx=([0-9A-Fa-f ]+)", text)
    if frame_match: result["last_physical_rx_hex"] = frame_match.group(1).replace(" ", "").upper()  # type: ignore[assignment]
    return result


def parse_app_tm(text: str) -> Dict[str, Any]:
    patterns = {
        "uptime_ms": r"uptimeMs=(\d+)", "mode": r"\bmode=(\d+)",
        "spi_error_count": r"spiErr=(\d+)", "acm_completed": r"ACM pending=\d+ running=\d+ completed=(\d+)",
        "acm_generation": r"ACM_RESULT valid=\d+ gen=(\d+)",
        "acm_valid_mask": r"antMask=0x([0-9A-Fa-f]+)",
        "capture_generation": r"CAP state=\d+ gen=(\d+)",
        "capture_valid_mask": r"CAP state=.*?valid=0x([0-9A-Fa-f]+)",
        "capture_bytes": r"CAP state=.*?bytes=(\d+)", "capture_errors": r"CAP state=.*?errors=(\d+)",
        "sweep_generation": r"SWEEP gen=(\d+)", "sweep_complete": r"SWEEP gen=.*?complete=(\d+)",
        "sweep_points": r"SWEEP gen=.*?points=(\d+)/(\d+)",
    }
    result: Dict[str, Any] = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, text, re.S)
        if match:
            base = 16 if "mask" in key else 10
            result[key] = tuple(int(v, base) for v in match.groups()) if len(match.groups()) > 1 else int(match.group(1), base)
    factors = re.findall(r"ACM_FACTOR ant=(\d+) I=0x([0-9A-Fa-f]+) Q=0x([0-9A-Fa-f]+)", text)
    if factors: result["acm_factors"] = [{"antenna": int(a), "i": int(i, 16), "q": int(q, 16)} for a, i, q in factors]
    return result
