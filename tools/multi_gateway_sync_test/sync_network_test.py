#!/usr/bin/env python3
"""Two-gateway GPS/PPS synchronization network hardware test runner.

The hardware-facing dependencies (paho-mqtt, pyserial and paramiko) are
imported lazily so selection generation and parser unit tests run offline.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import random
import re
import shlex
import threading
import time
import traceback
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from queue import Empty, Queue
from typing import Any, Iterable


RATE_TO_NS = {5: 0, 6: 1, 7: 2, 8: 3, 9: 4, 10: 5, 11: 6, 18: 7}
RATE_MAX_BLOCKS = {5: 10, 6: 10, 7: 10, 8: 10,
                   9: 16, 10: 16, 11: 16, 18: 10}
WAN_BLOCK_BYTES = {5: 26, 6: 26, 7: 26, 8: 26,
                   9: 26, 10: 26, 11: 26, 18: 40}
WAN_UPLINK_OVERHEAD_BYTES = 15
WAN_DOWNLINK_OVERHEAD_BYTES = 15
BODY_US = {5: 131072, 6: 65536, 7: 32768, 8: 16384,
           9: 8192, 10: 4096, 11: 2048, 18: 2048}
# Ground-WAN fixed cost: BCN + BRD(2 blocks) + UL/DL one-body terms,
# including the current per-slot gaps in src/trm/trm_slot.c.
BASE_US = {5: 1098143, 6: 569120, 7: 290077, 8: 147692,
           9: 77026, 10: 42454, 11: 26318, 18: 26318}
DEFAULT_MULTI_RATE_GROUPS = ((5, 6, 7, 8), (9, 10, 11, 18),
                             (6, 7, 8), (10, 11, 18),
                             (7, 8), (11, 18))
RX_RE = re.compile(
    r"RX users - rateMode=(?P<rate>\d+), systemFrame=(?P<system>\d+), "
    r"superFrame=(?P<super>\d+), userCount=(?P<count>\d+)"
    r"(?:, firstRssi=(?P<rssi>-?\d+))?"
)
RATE_CONFIG_RE = re.compile(
    r"Rate\[(?P<index>\d+)\]: mode=(?P<rate>\d+), brdBlocks=(?P<brd>\d+), "
    r"ulBlocks=(?P<ul>\d+), dlBlocks=(?P<dl>\d+)"
)
TX_RE = re.compile(r"Sent (?P<sent>\d+)/(?P<total>\d+) users")
USER_RSSI_RE = re.compile(
    r"用户\[0\]:.*?rssi=(?P<rssi>-?\d+).*?payload_len=(?P<payload_len>\d+)"
)


@dataclass(frozen=True)
class SlotCombination:
    case: int
    key: str
    super_frame_num: int
    rates: tuple[int, ...]
    ul_blocks: tuple[int, ...]
    dl_blocks: tuple[int, ...]
    frame_period_us: int
    frame_count: int

    def gateway_rate_cfgs(self) -> list[dict[str, int]]:
        return [
            {
                "rate_mode": RATE_TO_NS[rate],
                # The NS API accepts MAC payload byte lengths, while the
                # gateway receives already-converted PHY block counts.
                # Pick the largest payload that still occupies exactly the
                # requested number of blocks after the 15-byte WAN overhead.
                "uplink_len": (WAN_BLOCK_BYTES[rate] * self.ul_blocks[index] -
                               WAN_UPLINK_OVERHEAD_BYTES),
                "downlink_len": (WAN_BLOCK_BYTES[rate] * self.dl_blocks[index] -
                                 WAN_DOWNLINK_OVERHEAD_BYTES),
            }
            for index, rate in enumerate(self.rates)
        ]


def solve_period(total_raw_us: int, super_frame_num: int) -> tuple[int, int] | None:
    best: tuple[int, int, int] | None = None
    for multiplier in range(1, 11):
        total_us = multiplier * 1_000_000
        for frame_count in range(1, 65):
            if total_us % frame_count or frame_count % super_frame_num:
                continue
            period = total_us // frame_count
            if period < total_raw_us:
                continue
            gap = period - total_raw_us
            if best is None or gap < best[0]:
                best = (gap, period, frame_count)
    return None if best is None else (best[1], best[2])


def _single_from_row(case: int, index: int, row: dict[str, str]) -> SlotCombination:
    rate = int(row["rate_mode"])
    return SlotCombination(
        case=case,
        key=f"C{case}-R{rate}-{index:03d}",
        super_frame_num=int(row["super_frame_num"]),
        rates=(rate,),
        ul_blocks=(int(row["ul_blocks"]),),
        dl_blocks=(int(row["dl_blocks"]),),
        frame_period_us=int(row["frame_period_us"]),
        frame_count=int(row["frame_count"]),
    )


def sample_single_rate_cases(csv_path: Path, case: int, sample_count: int,
                             seed: int) -> list[SlotCombination]:
    if case not in (4, 5):
        raise ValueError("single-rate sampler only supports case 4 or 5")
    grouped: dict[int, list[dict[str, str]]] = {rate: [] for rate in RATE_TO_NS}
    with csv_path.open("r", encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            super_frame = int(row["super_frame_num"])
            rate = int(row["rate_mode"])
            max_blocks = RATE_MAX_BLOCKS[rate]
            legal_blocks = (int(row["ul_blocks"]) <= max_blocks and
                            int(row["dl_blocks"]) <= max_blocks)
            if legal_blocks and ((case == 4 and super_frame == 1) or
                                 (case == 5 and super_frame > 1)):
                grouped[rate].append(row)

    result: list[SlotCombination] = []
    for rate in RATE_TO_NS:
        candidates = grouped[rate]
        if len(candidates) < sample_count:
            raise ValueError(f"case {case} rate {rate} only has {len(candidates)} candidates")
        rng = random.Random(seed + case * 1000 + rate)
        chosen = rng.sample(candidates, sample_count)
        result.extend(_single_from_row(case, index, row)
                      for index, row in enumerate(chosen, start=1))
    return result


def _multi_raw_period(rates: tuple[int, ...], ul: tuple[int, ...],
                      dl: tuple[int, ...]) -> int:
    return sum(BASE_US[rate] + 2 * BODY_US[rate] * (ul[index] + dl[index])
               for index, rate in enumerate(rates))


def sample_multi_rate_cases(groups: Iterable[Iterable[int]], sample_count: int,
                            seed: int) -> list[SlotCombination]:
    result: list[SlotCombination] = []
    for group_index, values in enumerate(groups, start=1):
        rates = tuple(int(value) for value in values)
        if not 2 <= len(rates) <= 4 or any(rate not in RATE_TO_NS for rate in rates):
            raise ValueError(f"invalid multi-rate group: {rates}")
        rng = random.Random(seed + 6000 + group_index)
        selected: dict[tuple[tuple[int, ...], tuple[int, ...]], tuple[int, int]] = {}
        attempts = 0
        while len(selected) < sample_count and attempts < sample_count * 10000:
            attempts += 1
            ul = tuple(rng.randint(1, RATE_MAX_BLOCKS[rate]) for rate in rates)
            dl = tuple(rng.randint(1, RATE_MAX_BLOCKS[rate]) for rate in rates)
            identity = (ul, dl)
            if identity in selected:
                continue
            solution = solve_period(_multi_raw_period(rates, ul, dl), 1)
            if solution is not None:
                selected[identity] = solution
        if len(selected) < sample_count:
            raise ValueError(f"cannot generate {sample_count} valid combinations for {rates}")
        for index, ((ul, dl), (period, frame_count)) in enumerate(selected.items(), start=1):
            result.append(SlotCombination(
                case=6,
                key=f"C6-G{group_index}-{index:03d}",
                super_frame_num=1,
                rates=rates,
                ul_blocks=ul,
                dl_blocks=dl,
                frame_period_us=period,
                frame_count=frame_count,
            ))
    return result


def build_selection(repo_root: Path, cases: set[int], count: int,
                    seed: int, groups: Iterable[Iterable[int]],
                    rates: set[int] | None = None) -> list[SlotCombination]:
    source = repo_root / "docs" / "trm_slot_analysis" / "ground_wan_brd2_full_results.csv"
    combinations: list[SlotCombination] = []
    if 4 in cases:
        combinations.extend(sample_single_rate_cases(source, 4, count, seed))
    if 5 in cases:
        combinations.extend(sample_single_rate_cases(source, 5, count, seed))
    if 6 in cases:
        combinations.extend(sample_multi_rate_cases(groups, count, seed))
    if rates is not None:
        combinations = [item for item in combinations
                        if item.case == 6 or item.rates[0] in rates]
    return combinations


class MqttPlatform:
    def __init__(self, config: dict[str, Any], transcript):
        try:
            import paho.mqtt.client as mqtt
        except ImportError as exc:
            raise RuntimeError("missing dependency: pip install paho-mqtt") from exc
        try:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,
                                      client_id=f"sync-net-{os.getpid()}")
        except (AttributeError, TypeError):
            self.client = mqtt.Client(client_id=f"sync-net-{os.getpid()}")
        self.config = config
        self.transcript = transcript
        self.responses: Queue[dict[str, Any]] = Queue()
        self.uplinks: Queue[dict[str, Any]] = Queue()
        self.connected = threading.Event()
        self._request_id = int(time.time()) % 1_000_000
        password = _secret(config, "password", "password_env")
        if config.get("username"):
            self.client.username_pw_set(config["username"], password)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        self.client.connect(config["host"], int(config["port"]), keepalive=30)
        self.client.loop_start()
        if not self.connected.wait(float(config.get("timeout_seconds", 10))):
            self.client.loop_stop()
            self.client.disconnect()
            raise TimeoutError("MQTT connect or subscribe timeout")

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()

    def _on_connect(self, client, _userdata, _flags, rc, *_extra):
        if rc != 0:
            return
        subscribe_result = client.subscribe(self.config["topic_up"], qos=1)
        result_code = subscribe_result[0] if isinstance(subscribe_result, tuple) else 0
        if result_code == 0:
            self.connected.set()

    def _on_disconnect(self, *_args):
        self.connected.clear()

    def _on_message(self, _client, _userdata, message):
        raw = message.payload.decode("utf-8", errors="replace")
        self.transcript("MQTT_RX", raw)
        try:
            payload = json.loads(raw)
        except json.JSONDecodeError:
            return
        if payload.get("req_opt") == "push_uplink" and isinstance(payload.get("req_body"), dict):
            body = dict(payload["req_body"])
            body["received_at"] = datetime.now().isoformat(timespec="milliseconds")
            self.uplinks.put(body)
        elif "rsp_code" in payload:
            self.responses.put(payload)

    def request(self, operation: str, body: Any, timeout: float | None = None) -> dict[str, Any]:
        self._request_id += 1
        request = {"req_id": self._request_id, "req_opt": operation, "req_body": body}
        raw = json.dumps(request, ensure_ascii=False, separators=(",", ":"))
        self.transcript("MQTT_TX", raw)
        publish_result = self.client.publish(
            self.config["topic_down"], raw, qos=1, retain=False)
        if hasattr(publish_result, "rc") and publish_result.rc != 0:
            raise RuntimeError(f"MQTT publish failed: operation={operation}, rc={publish_result.rc}")
        deadline = time.monotonic() + float(timeout or self.config.get("timeout_seconds", 10))
        deferred = []
        try:
            while time.monotonic() < deadline:
                try:
                    response = self.responses.get(timeout=max(0.05, deadline - time.monotonic()))
                except Empty:
                    break
                if response.get("req_id") == request["req_id"] and response.get("req_opt") == operation:
                    codes = response.get("rsp_code", -1)
                    codes = codes if isinstance(codes, list) else [codes]
                    if not codes or any(code != 0 for code in codes):
                        raise RuntimeError(f"NS rejected {operation}: {response}")
                    return response
                deferred.append(response)
        finally:
            for response in deferred:
                self.responses.put(response)
        raise TimeoutError(f"NS response timeout: {operation}")

    def replace_gateways(self, gateways: list[dict[str, Any]]) -> dict[str, Any]:
        ids = [item["gw_id"] for item in gateways]
        before = self.request("get_gateway", {"gw_ids": ids})
        if before.get("rsp_body"):
            self.request("delete_gateway", {"gw_ids": ids})
        added = self.request("add_gateway", gateways)
        after = self.request("get_gateway", {"gw_ids": ids})
        actual_rows = {row.get("gw_id"): row for row in after.get("rsp_body", [])
                       if isinstance(row, dict) and row.get("gw_id")}
        if set(actual_rows) != set(ids):
            raise RuntimeError(
                f"gateway readback mismatch: expected={ids}, actual={sorted(actual_rows)}")
        for expected in gateways:
            actual = actual_rows[expected["gw_id"]]
            for field in ("freq_major", "freq_minor", "nwk_num", "tdd_num", "rate_num"):
                if int(actual.get(field, -1)) != int(expected[field]):
                    raise RuntimeError(
                        f"gateway {expected['gw_id']} readback {field} mismatch: "
                        f"expected={expected[field]}, actual={actual.get(field)}")
            actual_rates = actual.get("rate_cfgs")
            if actual_rates != expected["rate_cfgs"]:
                raise RuntimeError(
                    f"gateway {expected['gw_id']} rate_cfgs mismatch: "
                    f"expected={expected['rate_cfgs']}, actual={actual_rates}")
        return {"before": before, "add": added, "after": after}

    def replace_terminal(self, terminal: dict[str, Any]) -> dict[str, Any]:
        dev_eui = terminal["dev_eui"]
        before = self.request("get_terminal", {"dev_euis": [dev_eui]})
        if before.get("rsp_body"):
            self.request("delete_terminal", {"dev_euis": [dev_eui]})
        added = self.request("add_terminal", [terminal])
        after = self.request("get_terminal", {"dev_euis": [dev_eui]})
        rows = [row for row in after.get("rsp_body", []) if isinstance(row, dict)]
        if len(rows) != 1 or str(rows[0].get("dev_eui", "")).upper() != dev_eui.upper():
            raise RuntimeError(f"terminal readback mismatch: expected={dev_eui}, actual={rows}")
        actual = rows[0]
        actual_type = actual.get("dev_type", actual.get("dev_mode", -1))
        for expected_field, actual_value in (
                ("dev_type", actual_type),
                ("security_mode", actual.get("security_mode", -1))):
            if int(actual_value) != int(terminal[expected_field]):
                raise RuntimeError(
                    f"terminal {dev_eui} readback {expected_field} mismatch: "
                    f"expected={terminal[expected_field]}, actual={actual_value}")
        if str(actual.get("related_id", "")).upper() != str(terminal.get("related_id", "")).upper():
            raise RuntimeError(
                f"terminal {dev_eui} readback related_id mismatch: "
                f"expected={terminal.get('related_id', '')}, actual={actual.get('related_id', '')}")
        return {"before": before, "add": added, "after": after}

    def clear_uplinks(self):
        while True:
            try:
                self.uplinks.get_nowait()
            except Empty:
                return

    def collect_packet_uplinks(self, dev_eui: str, payload_hex: str,
                               gateway_ids: set[str], timeout: float,
                               post_first_wait: float = 1.0) -> dict[str, dict[str, Any]]:
        found: dict[str, dict[str, Any]] = {}
        deadline = time.monotonic() + timeout
        first_deadline: float | None = None
        while time.monotonic() < deadline and set(found) != gateway_ids:
            if first_deadline is not None and time.monotonic() >= first_deadline:
                break
            effective_deadline = min(deadline, first_deadline) if first_deadline else deadline
            try:
                item = self.uplinks.get(
                    timeout=max(0.05, effective_deadline - time.monotonic()))
            except Empty:
                break
            if str(item.get("dev_eui", "")).upper() != dev_eui.upper():
                continue
            if str(item.get("data", "")).upper() != payload_hex.upper():
                continue
            gateway_id = str(item.get("gw_id", "")).upper()
            if gateway_id in gateway_ids:
                found[gateway_id] = item
                if first_deadline is None:
                    first_deadline = time.monotonic() + max(0.0, post_first_wait)
        return found


class Terminal:
    def __init__(self, config: dict[str, Any], transcript):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("missing dependency: pip install pyserial") from exc
        self.serial_module = serial
        self.config = config
        self.transcript = transcript
        self.port = config["port"]
        self.handle = None

    def open(self):
        self.handle = self.serial_module.Serial(
            self.port, int(self.config.get("baudrate", 115200)), timeout=0.2,
            write_timeout=1)
        self.handle.reset_input_buffer()
        self.handle.reset_output_buffer()

    def close(self):
        if self.handle is not None and self.handle.is_open:
            self.handle.close()

    def _read(self, duration: float) -> list[str]:
        deadline = time.monotonic() + duration
        lines = []
        while time.monotonic() < deadline:
            raw = self.handle.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                lines.append(line)
                self.transcript("TERM_RX", line)
        return lines

    def _read_until(self, duration: float, stop_tokens: tuple[str, ...]) -> list[str]:
        deadline = time.monotonic() + duration
        lines = []
        matched = False
        while time.monotonic() < deadline and not matched:
            raw = self.handle.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            lines.append(line)
            self.transcript("TERM_RX", line)
            matched = any(token in line for token in stop_tokens)
        if matched:
            lines.extend(self._read(0.5))
        return lines

    def command(self, command: str, timeout: float = 2,
                expected: str | None = "AT_OK") -> list[str]:
        self.transcript("TERM_TX", command)
        self.handle.write((command + "\r\n").encode("ascii"))
        self.handle.flush()
        stop_tokens = (expected,) if expected is not None else ()
        lines = self._read_until(timeout, stop_tokens + ("AT_ERR", "AT_ERROR", "AT_PARAM_ERROR"))
        if any(token in line for line in lines for token in ("AT_ERR", "AT_ERROR", "AT_PARAM_ERROR")):
            raise RuntimeError(f"terminal rejected {command}: {lines}")
        if expected is not None and not any(expected in line for line in lines):
            raise TimeoutError(f"terminal response missing {expected}: {command}: {lines}")
        return lines

    def configure_and_join(self, rate: int):
        attempts = int(self.config.get("configuration_attempts", 3))
        if attempts < 1:
            raise ValueError("terminal configuration_attempts must be at least 1")
        last_error: Exception | None = None
        for attempt in range(1, attempts + 1):
            try:
                return self._configure_and_join_once(rate)
            except (OSError, RuntimeError, TimeoutError) as exc:
                last_error = exc
                self.transcript(
                    "TERM_RETRY",
                    f"configuration attempt {attempt}/{attempts} failed: "
                    f"{type(exc).__name__}: {exc}")
                if attempt < attempts:
                    time.sleep(1)
                    self.close()
                    self.open()
        assert last_error is not None
        raise last_error

    def _configure_and_join_once(self, rate: int):
        dev_eui = str(self.config["dev_eui"])
        if not re.fullmatch(r"[0-9A-Fa-f]{16}", dev_eui):
            raise ValueError("terminal dev_eui must contain exactly 16 hexadecimal digits")
        if self.config.get("reset_before_combination", True):
            self.command("AT+RST", timeout=2, expected="AT_OK")
            time.sleep(float(self.config.get("reset_wait_seconds", 3)))
        self.command(f"AT+DEVEUI={dev_eui}")
        self.command(f"AT+FREQCFG={self.config['freq_major']},{self.config['freq_minor']}")
        self.command(terminal_frequency_slot_command(self.config))
        devmode = 2 if int(self.config.get("dev_mode", 0)) == 0 else 3
        self.command(f"AT+DEVMODE={devmode}")
        key = _secret(self.config, "root_key", "root_key_env")
        if not key:
            raise ValueError("terminal root_key or root_key_env is required")
        if not re.fullmatch(r"[0-9A-Fa-f]{32}", key):
            raise ValueError("terminal root_key must contain exactly 32 hexadecimal digits")
        self.command(f"AT+SEC={int(self.config.get('security_mode', 0))},{key}")
        self.command(f"AT+RATE={rate}")
        for power_command in terminal_power_commands(self.config):
            self.command(power_command)
        self.command("AT+SENDPOL=0,0")
        self.command("AT+PRINTMODE=MAC,1")
        self.transcript("TERM_TX", "AT+JOIN=0")
        self.handle.write(b"AT+JOIN=0\r\n")
        self.handle.flush()
        lines = self._read_until(float(self.config.get("join_timeout_seconds", 120)),
                                 ("+NWKINFO:4", "AT_ERROR", "AT_ERR"))
        if not any("+NWKINFO:4" in line for line in lines):
            raise TimeoutError("terminal join did not reach +NWKINFO:4")

    def send_confirmed(self, payload_hex: str) -> dict[str, Any]:
        command = f"AT+SENDB=0,1,0,1,{payload_hex}"
        self._read(0.2)
        self.transcript("TERM_TX", command)
        self.handle.write((command + "\r\n").encode("ascii"))
        self.handle.flush()
        lines = self._read_until(float(self.config.get("ack_timeout_seconds", 120)),
                                 ("+TXSTATUS:7", "+TXSTATUS:8", "+TXSTATUS:9",
                                  "AT_ERROR", "AT_ERR"))
        return {
            "lines": lines,
            "txstatus7": any("+TXSTATUS:7" in line for line in lines),
            "txstatus8": any("+TXSTATUS:8" in line for line in lines),
            "txstatus9": any("+TXSTATUS:9" in line for line in lines),
            "rx_data": any("RX_DATA" in line for line in lines),
        }


class GatewayLogs:
    def __init__(self, config: dict[str, Any], output_dir: Path, transcript):
        try:
            import paramiko
        except ImportError as exc:
            raise RuntimeError("missing dependency: pip install paramiko") from exc
        self.paramiko = paramiko
        self.config = config
        self.output_dir = output_dir
        self.transcript = transcript
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.client.connect(config["host"], port=int(config.get("ssh_port", 22)),
                            username=config.get("ssh_user", "root"),
                            password=_secret(config, "ssh_password", "ssh_password_env"),
                            timeout=float(config.get("ssh_timeout_seconds", 10)))
        self.sftp = self.client.open_sftp()
        self.managed_pid: str | None = None
        self.remote_run_dir: str | None = None
        self.managed_launch_count = 0
        self.managed_stdout_paths: list[str] = []

    def close(self):
        if self.config.get("log_mode", "attach") == "manage" and self.managed_pid:
            self.stop_managed()
        if self.remote_run_dir:
            local_dir = self.output_dir / "gateway_stdout"
            local_dir.mkdir(parents=True, exist_ok=True)
            filenames = [path.rsplit("/", 1)[-1] for path in self.managed_stdout_paths]
            filenames.extend(
                f"gateway{'.restart_' + str(index).zfill(2) if index else ''}.pid"
                for index in range(self.managed_launch_count))
            for filename in filenames:
                try:
                    self.sftp.get(f"{self.remote_run_dir}/{filename}",
                                  str(local_dir / f"{self.config['name']}_{filename}"))
                except IOError:
                    pass
        self.sftp.close()
        self.client.close()

    def _exec(self, command: str) -> str:
        self.transcript(f"SSH_{self.config['name']}", command)
        _stdin, stdout, stderr = self.client.exec_command(command)
        status = stdout.channel.recv_exit_status()
        output = stdout.read().decode("utf-8", errors="replace")
        error = stderr.read().decode("utf-8", errors="replace")
        if status != 0:
            raise RuntimeError(f"SSH command failed ({status}): {command}: {error}")
        return output.strip()

    def start_managed(self, run_id: str):
        if self.config.get("log_mode", "attach") != "manage":
            return
        stop_command = self.config.get("stop_command")
        start_command = self.config.get("start_command")
        if not stop_command or not start_command:
            raise ValueError("managed gateway requires stop_command and start_command")
        self._exec(stop_command)
        process_name = str(self.config.get("process_name", "tk8710_gw"))
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", process_name):
            raise ValueError("gateway process_name contains unsupported characters")
        quoted_process_name = shlex.quote(process_name)
        self._exec(
            f"count=0; while pidof {quoted_process_name} >/dev/null 2>&1 && "
            "[ $count -lt 15 ]; do sleep 1; count=$((count + 1)); done; "
            f"if pidof {quoted_process_name} >/dev/null 2>&1; then "
            f"echo '{process_name} did not stop' >&2; exit 1; fi; sleep 2"
        )
        remote_dir = f"/userdata/sync_network_{run_id}_{self.config['name']}"
        self.remote_run_dir = remote_dir
        self._exec(f"mkdir -p {remote_dir}")
        self._launch_managed(start_command)

    def _launch_managed(self, start_command: str):
        if not self.remote_run_dir:
            raise RuntimeError("managed gateway run directory is not initialized")
        suffix = ("" if self.managed_launch_count == 0 else
                  f".restart_{self.managed_launch_count:02d}")
        pid_file = f"{self.remote_run_dir}/gateway{suffix}.pid"
        stdout_path = f"{self.remote_run_dir}/stdout{suffix}.log"
        launcher = f"exec {start_command} > {stdout_path} 2>&1"
        command = (
            f"start-stop-daemon -S -b -m -p {pid_file} -x /bin/sh -- "
            f"-c {shlex.quote(launcher)} && cat {pid_file}"
        )
        self.managed_pid = self._exec(command).splitlines()[-1]
        self.managed_stdout_paths.append(stdout_path)
        self.managed_launch_count += 1
        time.sleep(float(self.config.get("gateway_start_wait_seconds", 5)))
        self._exec(
            f"if ! kill -0 {int(self.managed_pid)} 2>/dev/null; then "
            f"tail -n 80 {stdout_path} >&2; exit 1; fi"
        )

    def stop_managed(self):
        if self.config.get("log_mode", "attach") != "manage":
            raise RuntimeError("gateway interruption requires log_mode=manage")
        if not self.managed_pid:
            raise RuntimeError("managed gateway process is not running")
        pid = int(self.managed_pid)
        self._exec(
            f"kill -TERM {pid} 2>/dev/null || true; count=0; "
            f"while kill -0 {pid} 2>/dev/null && [ $count -lt 10 ]; do "
            "sleep 1; count=$((count + 1)); done; "
            f"if kill -0 {pid} 2>/dev/null; then exit 1; fi")
        self.managed_pid = None

    def restart_managed(self):
        if self.config.get("log_mode", "attach") != "manage":
            raise RuntimeError("gateway interruption requires log_mode=manage")
        if self.managed_pid:
            raise RuntimeError("managed gateway process is already running")
        start_command = self.config.get("start_command")
        if not start_command:
            raise ValueError("managed gateway requires start_command")
        self._launch_managed(start_command)

    def mark(self) -> dict[str, int]:
        result = {}
        remote_dir = self.config.get("trm_log_dir", "/userdata/8710log")
        try:
            for entry in self.sftp.listdir_attr(remote_dir):
                if entry.filename.startswith("trm_log_") and entry.filename.endswith(".log"):
                    result[f"{remote_dir}/{entry.filename}"] = entry.st_size
        except IOError:
            pass
        if self.remote_run_dir:
            for stdout_path in self.managed_stdout_paths:
                try:
                    result[stdout_path] = self.sftp.stat(stdout_path).st_size
                except IOError:
                    pass
        return result

    def collect_since(self, mark: dict[str, int], packet_dir: Path) -> str:
        packet_dir.mkdir(parents=True, exist_ok=True)
        remote_dir = self.config.get("trm_log_dir", "/userdata/8710log")
        chunks = []
        try:
            entries = self.sftp.listdir_attr(remote_dir)
        except IOError as exc:
            raise RuntimeError(f"cannot list gateway TRM logs: {remote_dir}") from exc
        for entry in entries:
            if not (entry.filename.startswith("trm_log_") and entry.filename.endswith(".log")):
                continue
            remote_path = f"{remote_dir}/{entry.filename}"
            offset = min(mark.get(remote_path, 0), entry.st_size)
            with self.sftp.open(remote_path, "rb") as source:
                source.seek(offset)
                data = source.read()
            if data:
                local_path = packet_dir / f"{self.config['name']}_{entry.filename}"
                local_path.write_bytes(data)
                chunks.append(data.decode("utf-8", errors="replace"))
        if self.remote_run_dir:
            for stdout_path in self.managed_stdout_paths:
                suffix = stdout_path.rsplit("/", 1)[-1]
                local_name = f"{self.config['name']}_{suffix}"
                try:
                    size = self.sftp.stat(stdout_path).st_size
                    offset = min(mark.get(stdout_path, 0), size)
                    with self.sftp.open(stdout_path, "rb") as source:
                        source.seek(offset)
                        data = source.read()
                    if data:
                        local_path = packet_dir / local_name
                        local_path.write_bytes(data)
                        chunks.append(data.decode("utf-8", errors="replace"))
                except IOError:
                    pass
        return "\n".join(chunks)


def parse_gateway_window(text: str) -> dict[str, Any]:
    receives = [{name: int(value) for name, value in match.groupdict().items()
                 if value is not None}
                for match in RX_RE.finditer(text)]
    sends = [{name: int(value) for name, value in match.groupdict().items()}
             for match in TX_RE.finditer(text)]
    user_rows = [{name: int(value) for name, value in match.groupdict().items()}
                 for match in USER_RSSI_RE.finditer(text)]
    rx_log_rssi = receives[-1].get("rssi") if receives else None
    return {"receives": receives, "sends": sends,
            "user_rows": user_rows,
            "last_rssi": rx_log_rssi if rx_log_rssi is not None else
                         (user_rows[-1]["rssi"] if user_rows else None),
            "sent_ack": any(item["sent"] > 0 for item in sends)}


def parse_applied_rate_config(text: str, rate_count: int) -> list[dict[str, int]]:
    matches = [{name: int(value) for name, value in match.groupdict().items()}
               for match in RATE_CONFIG_RE.finditer(text)]
    for start in range(len(matches) - rate_count, -1, -1):
        candidate = matches[start:start + rate_count]
        if [item["index"] for item in candidate] == list(range(rate_count)):
            return candidate
    return []


def validate_applied_rate_config(combination: SlotCombination,
                                 parsed: list[dict[str, int]]) -> None:
    if len(parsed) != len(combination.rates):
        raise RuntimeError("gateway applied rate configuration log is missing")
    for index, item in enumerate(parsed):
        expected = (combination.rates[index], combination.ul_blocks[index],
                    combination.dl_blocks[index])
        actual = (item["rate"], item["ul"], item["dl"])
        if actual != expected:
            raise RuntimeError(
                f"gateway applied rate configuration mismatch at {index}: "
                f"expected rate/ul/dl={expected}, actual={actual}"
            )


def evaluate_packet(case: int, combination: SlotCombination, gateway_ids: list[str],
                    uplinks: dict[str, dict[str, Any]], terminal_result: dict[str, Any],
                    gateway_windows: dict[str, dict[str, Any]]) -> dict[str, Any]:
    reasons = []
    if not uplinks:
        reasons.append("mqtt_uplink_missing")
    rssi = {}
    for gateway in gateway_ids:
        log_rssi = gateway_windows.get(gateway, {}).get("last_rssi")
        mqtt_rssi = uplinks.get(gateway, {}).get("rssi")
        if log_rssi is not None:
            rssi[gateway] = int(log_rssi)
        elif mqtt_rssi is not None:
            rssi[gateway] = int(mqtt_rssi)
    strongest = None
    if len(rssi) == len(gateway_ids):
        maximum = max(rssi.values())
        winners = [gateway for gateway, value in rssi.items() if value == maximum]
        if len(winners) == 1:
            strongest = winners[0]
        else:
            reasons.append("strongest_gateway_rssi_tie")
    else:
        reasons.append("gateway_rssi_missing")
    if not terminal_result.get("txstatus7") or not terminal_result.get("rx_data"):
        reasons.append("terminal_ack_missing")
    senders = [gateway for gateway, window in gateway_windows.items() if window.get("sent_ack")]
    if strongest is not None and senders != [strongest]:
        reasons.append("ack_sender_is_not_unique_strongest_gateway")

    selected_rx = {}
    for gateway in gateway_ids:
        receives = gateway_windows.get(gateway, {}).get("receives", [])
        if not receives:
            reasons.append(f"{gateway}_trm_rx_missing")
        else:
            selected_rx[gateway] = receives[-1]
    if case == 5 and len(selected_rx) == 2:
        values = {item["super"] for item in selected_rx.values()}
        if len(values) != 1:
            reasons.append("gateway_superframe_mismatch")
    if case == 6 and len(selected_rx) == 2:
        values = {item["rate"] for item in selected_rx.values()}
        if len(values) != 1:
            reasons.append("gateway_rate_mismatch")
    return {
        "passed": not reasons,
        "reasons": reasons,
        "rssi": rssi,
        "strongest_gateway": strongest,
        "ack_senders": senders,
        "selected_rx": selected_rx,
    }


def evaluate_combination(packets: list[dict[str, Any]], expected_packet_count: int,
                         success_threshold: float) -> dict[str, Any]:
    if expected_packet_count <= 0:
        raise ValueError("packets_per_combination must be greater than zero")
    if not 0.0 <= success_threshold < 1.0:
        raise ValueError("combination_success_threshold must be in range [0, 1)")
    passed_packet_count = sum(
        bool(packet.get("verdict", {}).get("passed")) for packet in packets)
    success_rate = passed_packet_count / expected_packet_count
    return {
        "expected_packet_count": expected_packet_count,
        "attempted_packet_count": len(packets),
        "passed_packet_count": passed_packet_count,
        "success_rate": success_rate,
        "success_threshold": success_threshold,
        "passed": (len(packets) == expected_packet_count and
                   success_rate >= success_threshold),
    }


def make_payload(case: int, combination_index: int, packet_index: int,
                 byte_count: int) -> str:
    prefix = bytes((0xA5, 0x5A, case & 0xFF, (combination_index >> 8) & 0xFF,
                    combination_index & 0xFF, packet_index & 0xFF))
    if byte_count < len(prefix):
        raise ValueError("payload_bytes must be at least 6")
    return (prefix + bytes([packet_index & 0xFF]) * (byte_count - len(prefix))).hex().upper()


def _secret(config: dict[str, Any], value_key: str, env_key: str) -> str:
    environment_name = config.get(env_key)
    if environment_name:
        value = os.environ.get(environment_name)
        if value is None:
            raise ValueError(f"environment variable is not set: {environment_name}")
        return value
    return str(config.get(value_key, ""))


def _redact_value(value: Any, key: str = "") -> Any:
    if key.lower() in {"password", "root_key", "ssh_password"} and value:
        return "***"
    if isinstance(value, dict):
        return {item_key: _redact_value(item_value, item_key)
                for item_key, item_value in value.items()}
    if isinstance(value, list):
        return [_redact_value(item) for item in value]
    if isinstance(value, tuple):
        return tuple(_redact_value(item) for item in value)
    if isinstance(value, str):
        return re.sub(r"(AT\+SEC=\d+,)[0-9A-Fa-f]+", r"\1***", value)
    return value


def _write_json(path: Path, payload: Any):
    path.write_text(json.dumps(_redact_value(payload), ensure_ascii=False, indent=2),
                    encoding="utf-8")


def load_config(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def terminal_power_commands(config: dict[str, Any]) -> list[str]:
    power_control = int(config.get("power_control", 0))
    tx_power = int(config.get("tx_power", 15))
    if power_control not in (0, 1):
        raise ValueError("terminal power_control must be 0 or 1")
    if not 0 <= tx_power <= 15:
        raise ValueError("terminal tx_power must be in range 0..15")
    return [f"AT+PWRCTRL={power_control}", f"AT+TXP={tx_power}"]


def terminal_frequency_slot_command(config: dict[str, Any]) -> str:
    values = config.get("fixed_frequency_slot")
    if not isinstance(values, list) or len(values) != 4:
        raise ValueError("terminal fixed_frequency_slot must contain four integers")
    if any(isinstance(value, bool) or not isinstance(value, int) for value in values):
        raise ValueError("terminal fixed_frequency_slot must contain four integers")
    if any(value <= 0 for value in values[:3]) or values[3] not in (0, 1):
        raise ValueError("terminal fixed_frequency_slot values are invalid")
    return "AT+FREQSLOT=" + ",".join(str(value) for value in values)


def gateway_body(gateway: dict[str, Any], combination: SlotCombination,
                 common: dict[str, Any]) -> dict[str, Any]:
    return {
        "gw_id": gateway["gw_id"].upper(),
        "freq_major": int(common["freq_major"]),
        "freq_minor": int(common["freq_minor"]),
        "nwk_num": int(gateway["nwk_num"]),
        "tdd_num": combination.super_frame_num,
        "rate_num": len(combination.rates),
        "rate_cfgs": combination.gateway_rate_cfgs(),
        "description": f"GPS sync auto {combination.key}",
    }


def run_hardware(config: dict[str, Any], combinations: list[SlotCombination],
                 run_dir: Path) -> dict[str, Any]:
    transcript_lock = threading.Lock()
    transcript_path = run_dir / "events.log"

    def transcript(source: str, message: str):
        safe_message = _redact_value(message)
        safe_message = re.sub(r'("root_key"\s*:\s*")[^"]*(")', r'\1***\2', safe_message)
        line = (f"[{datetime.now().isoformat(timespec='milliseconds')}] "
                f"[{source}] {safe_message}\n")
        with transcript_lock:
            with transcript_path.open("a", encoding="utf-8") as target:
                target.write(line)

    platform = None
    terminal = None
    gateway_collectors = []
    results = []
    run_id = run_dir.name
    try:
        platform = MqttPlatform(config["mqtt"], transcript)
        terminal = Terminal(config["terminal"], transcript)
        for item in config["gateways"]:
            gateway_collectors.append(GatewayLogs(item, run_dir, transcript))
        for collector in gateway_collectors:
            collector.start_managed(run_id)
        terminal.open()
        gateway_ids = [item["gw_id"].upper() for item in config["gateways"]]
        terminal_body = {
            "dev_eui": config["terminal"]["dev_eui"].upper(),
            "dev_type": int(config["terminal"].get("dev_mode", 0)),
            "security_mode": int(config["terminal"].get("security_mode", 0)),
            "root_key": _secret(config["terminal"], "root_key", "root_key_env"),
            "related_id": config["terminal"].get("related_id", ""),
            "description": "GPS synchronization network test terminal",
        }
        for combination_index, combination in enumerate(combinations, start=1):
            print(f"[{combination_index}/{len(combinations)}] start {combination.key} "
                  f"rates={combination.rates} tdd={combination.super_frame_num}", flush=True)
            combo_dir = run_dir / combination.key
            combo_dir.mkdir(parents=True, exist_ok=True)
            packet_count = int(config["execution"].get("packets_per_combination", 10))
            success_threshold = float(
                config["execution"].get("combination_success_threshold", 0.70))
            record: dict[str, Any] = {"combination": asdict(combination), "packets": []}
            try:
                config_marks = {collector.config["gw_id"].upper(): collector.mark()
                                for collector in gateway_collectors}
                bodies = [gateway_body(item, combination, config["radio"])
                          for item in config["gateways"]]
                record["gateway_configuration"] = platform.replace_gateways(bodies)
                time.sleep(float(config["timing"].get("gps_sync_wait_seconds", 30)))
                applied = {}
                config_log_dir = combo_dir / "configuration"
                for collector in gateway_collectors:
                    gateway_id = collector.config["gw_id"].upper()
                    text = collector.collect_since(config_marks[gateway_id], config_log_dir)
                    parsed = parse_applied_rate_config(text, len(combination.rates))
                    if not parsed:
                        current_log_dir = config_log_dir / "current"
                        text = collector.collect_since({}, current_log_dir)
                        parsed = parse_applied_rate_config(text, len(combination.rates))
                    validate_applied_rate_config(combination, parsed)
                    applied[gateway_id] = parsed
                record["applied_rate_configuration"] = applied
                record["terminal_configuration"] = platform.replace_terminal(terminal_body)
                terminal.configure_and_join(combination.rates[0])
                for packet_index in range(1, packet_count + 1):
                    payload = make_payload(combination.case, combination_index, packet_index,
                                           int(config["execution"].get("payload_bytes", 30)))
                    platform.clear_uplinks()
                    marks = {collector.config["gw_id"].upper(): collector.mark()
                             for collector in gateway_collectors}
                    terminal_result = terminal.send_confirmed(payload)
                    uplinks = platform.collect_packet_uplinks(
                        terminal_body["dev_eui"], payload, set(gateway_ids),
                        float(config["timing"].get("mqtt_uplink_wait_seconds", 10)),
                        float(config["timing"].get("mqtt_post_first_wait_seconds", 1)))
                    time.sleep(float(config["timing"].get("log_settle_seconds", 2)))
                    windows = {}
                    packet_dir = combo_dir / f"packet_{packet_index:02d}"
                    for collector in gateway_collectors:
                        gateway_id = collector.config["gw_id"].upper()
                        text = collector.collect_since(marks[gateway_id], packet_dir)
                        windows[gateway_id] = parse_gateway_window(text)
                    verdict = evaluate_packet(combination.case, combination, gateway_ids,
                                              uplinks, terminal_result, windows)
                    packet = {"index": packet_index, "payload": payload,
                              "terminal": terminal_result, "uplinks": uplinks,
                              "gateway_windows": windows, "verdict": verdict}
                    record["packets"].append(packet)
                    _write_json(packet_dir / "evidence.json", packet)
                    print(f"[{combination_index}/{len(combinations)}] {combination.key} "
                          f"packet={packet_index}/{packet_count} "
                          f"{'PASS' if verdict['passed'] else 'FAIL'}", flush=True)
                record.update(evaluate_combination(
                    record["packets"], packet_count, success_threshold))
            except Exception as exc:
                record.update(evaluate_combination(
                    record["packets"], packet_count, success_threshold))
                record["passed"] = False
                record["error"] = f"{type(exc).__name__}: {exc}"
                record["traceback"] = traceback.format_exc()
                transcript("ERROR", record["error"])
            results.append(record)
            print(f"[{combination_index}/{len(combinations)}] finish {combination.key} "
                  f"{'PASS' if record['passed'] else 'FAIL'}", flush=True)
            _write_json(combo_dir / "result.json", record)
            _write_reports(run_dir, results)
            if not record["passed"] and not config["execution"].get("continue_on_failure", True):
                break
    finally:
        if terminal is not None:
            terminal.close()
        for collector in reversed(gateway_collectors):
            try:
                collector.close()
            except Exception as exc:
                transcript("CLEANUP_ERROR", f"{type(exc).__name__}: {exc}")
        if platform is not None:
            platform.close()
    return {"passed": bool(results) and all(item["passed"] for item in results), "results": results}


def _write_reports(run_dir: Path, records: list[dict[str, Any]]):
    _write_json(run_dir / "summary.json", {
        "total": len(records),
        "passed": sum(bool(item.get("passed")) for item in records),
        "failed": sum(not bool(item.get("passed")) for item in records),
        "records": records,
    })
    with (run_dir / "summary.csv").open("w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=(
            "case", "key", "rates", "super_frame_num", "ul_blocks", "dl_blocks",
            "attempted_packet_count", "expected_packet_count", "passed_packet_count",
            "success_rate", "success_threshold", "passed", "error"))
        writer.writeheader()
        for item in records:
            combo = item["combination"]
            writer.writerow({
                "case": combo["case"], "key": combo["key"],
                "rates": "+".join(map(str, combo["rates"])),
                "super_frame_num": combo["super_frame_num"],
                "ul_blocks": "+".join(map(str, combo["ul_blocks"])),
                "dl_blocks": "+".join(map(str, combo["dl_blocks"])),
                "attempted_packet_count": item.get("attempted_packet_count", 0),
                "expected_packet_count": item.get("expected_packet_count", 0),
                "passed_packet_count": item.get("passed_packet_count", 0),
                "success_rate": f"{float(item.get('success_rate', 0.0)):.2%}",
                "success_threshold": f"{float(item.get('success_threshold', 0.70)):.2%}",
                "passed": item.get("passed", False), "error": item.get("error", ""),
            })
    lines = ["# 多网关 GPS 同步自动测试结果", "",
             f"- 已执行组合：{len(records)}",
             f"- 通过：{sum(bool(item.get('passed')) for item in records)}",
             f"- 失败：{sum(not bool(item.get('passed')) for item in records)}", "",
             "| 用例 | 组合 | 速率 | tdd_num | 包通过 | 成功率 | 结果 |",
             "|---:|---|---|---:|---:|---:|---|"]
    for item in records:
        combo = item["combination"]
        lines.append(f"| {combo['case']} | {combo['key']} | {'/'.join(map(str, combo['rates']))} | "
                     f"{combo['super_frame_num']} | {item.get('passed_packet_count', 0)}/"
                     f"{item.get('expected_packet_count', 0)} | "
                     f"{float(item.get('success_rate', 0.0)):.2%} | "
                     f"{'通过' if item.get('passed') else '失败'} |")
    (run_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--cases", default="4,5,6")
    parser.add_argument("--seed", type=int, default=20260908)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--rates", default="",
                        help="comma-separated physical rates for case 4/5, for example 8")
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--output-root", type=Path, default=Path("sync_network_results"))
    args = parser.parse_args()

    config = load_config(args.config)
    cases = {int(value.strip()) for value in args.cases.split(",") if value.strip()}
    if not cases or not cases <= {4, 5, 6}:
        parser.error("--cases must contain only 4,5,6")
    rates = ({int(value.strip()) for value in args.rates.split(",") if value.strip()}
             if args.rates else None)
    if rates is not None and (not rates or not rates <= set(RATE_TO_NS)):
        parser.error("--rates must contain only 5,6,7,8,9,10,11,18")
    repo_root = Path(__file__).resolve().parents[2]
    groups = config.get("case6_rate_groups", DEFAULT_MULTI_RATE_GROUPS)
    selection = build_selection(repo_root, cases, args.sample_count, args.seed, groups, rates)
    run_dir = args.output_root / datetime.now().strftime("sync_network_%Y%m%d_%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=False)
    _write_json(run_dir / "selection.json", {
        "seed": args.seed, "sample_count": args.sample_count,
        "rates": sorted(rates) if rates is not None else None,
        "cases": sorted(cases), "combinations": [asdict(item) for item in selection],
    })
    sanitized = json.loads(json.dumps(config))
    for section in (sanitized.get("mqtt", {}), sanitized.get("terminal", {})):
        for key in ("password", "root_key"):
            if section.get(key):
                section[key] = "***"
    for gateway in sanitized.get("gateways", []):
        if gateway.get("ssh_password"):
            gateway["ssh_password"] = "***"
    _write_json(run_dir / "config.redacted.json", sanitized)
    if args.generate_only:
        print(f"generated {len(selection)} combinations: {run_dir / 'selection.json'}")
        return 0
    result = run_hardware(config, selection, run_dir)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
