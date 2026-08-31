from __future__ import annotations

import re
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import Any, Callable, Dict, Iterable, List, Optional, Tuple

from .protocol import parse_app_tm, parse_fpga_tm

TMS570_BANNERS = ("Satellite payload control ready", "TK8710 TMS570 satellite payload start")
TERMINAL_BANNERS = ("MAC AT CMD!", "TurMass")


def natural_port_key(name: str) -> Tuple[str, int]:
    match = re.match(r"^(.*?)(\d+)$", name.upper())
    return (match.group(1), int(match.group(2))) if match else (name.upper(), -1)


class SerialError(RuntimeError):
    pass


class SerialEndpoint:
    def __init__(self, port: str, baudrate: int = 115200,
                 line_sink: Optional[Callable[[str, str, str], None]] = None,
                 serial_factory: Optional[Callable[..., Any]] = None):
        self.port = port
        self.baudrate = baudrate
        self.line_sink = line_sink or (lambda port, direction, line: None)
        self.serial_factory = serial_factory or self._default_factory
        self.serial: Any = None
        self.lock = threading.RLock()

    @staticmethod
    def _default_factory(**kwargs: Any) -> Any:
        import serial
        return serial.Serial(**kwargs)

    def open(self) -> None:
        if self.serial and getattr(self.serial, "is_open", True): return
        self.serial = self.serial_factory(port=self.port, baudrate=self.baudrate,
                                          bytesize=8, parity="N", stopbits=1,
                                          timeout=0.1, write_timeout=1,
                                          xonxoff=False, rtscts=False, dsrdtr=False)

    def close(self) -> None:
        if self.serial:
            try: self.serial.close()
            finally: self.serial = None

    def write(self, command: str) -> None:
        self.open()
        if self.baudrate >= 500000:
            self._wait_rx_quiet(quiet_s=0.05, max_wait_s=2.0)
        self.line_sink(self.port, "tx", command)
        payload = (command + "\r\n").encode("ascii")
        if self.baudrate >= 500000:
            # 570控制台轮询SCILIN接收。1 Mbps下一次写完整命令只需约
            # 0.1 ms，容易与阻塞式INFO日志输出重叠并造成RX溢出。
            for byte in payload:
                self.serial.write(bytes((byte,)))
                if hasattr(self.serial, "flush"): self.serial.flush()
                time.sleep(0.001)
        else:
            self.serial.write(payload)
        if hasattr(self.serial, "flush"): self.serial.flush()

    def _wait_rx_quiet(self, quiet_s: float, max_wait_s: float) -> None:
        deadline = time.monotonic() + max_wait_s
        quiet_since = time.monotonic()
        pending = bytearray()
        while time.monotonic() < deadline:
            waiting = getattr(self.serial, "in_waiting", 0)
            raw = self.serial.read(max(1, min(4096, waiting)))
            if raw:
                pending.extend(raw if isinstance(raw, bytes) else raw.encode())
                quiet_since = time.monotonic()
            elif time.monotonic() - quiet_since >= quiet_s:
                break
            else:
                time.sleep(0.005)
        if pending:
            text = pending.decode("utf-8", "replace")
            for line in text.replace("\r", "\n").split("\n"):
                if line.strip(): self.line_sink(self.port, "rx", line.strip())

    def collect(self, duration: float, markers: Iterable[str] = (),
                any_markers: Iterable[str] = ()) -> str:
        self.open()
        deadline = time.monotonic() + duration
        data = bytearray()
        markers = tuple(markers)
        any_markers = tuple(any_markers)
        while time.monotonic() < deadline:
            waiting = getattr(self.serial, "in_waiting", 0)
            raw = self.serial.read(max(1, min(4096, waiting)))
            if raw:
                data.extend(raw if isinstance(raw, bytes) else raw.encode())
                text = data.decode("utf-8", "replace")
                if markers and all(marker in text for marker in markers): break
                if any_markers and any(marker in text for marker in any_markers): break
            else:
                time.sleep(0.01)
        text = data.decode("utf-8", "replace")
        for line in text.replace("\r", "\n").split("\n"):
            if line.strip(): self.line_sink(self.port, "rx", line.strip())
        return text

    def command(self, command: str, timeout: float = 3.0,
                markers: Iterable[str] = (), any_markers: Iterable[str] = ()) -> str:
        with self.lock:
            self.write(command)
            return self.collect(timeout, markers, any_markers)


@dataclass
class DiscoveredPorts:
    tms570: str = ""
    terminals: List[str] = None  # type: ignore[assignment]
    unknown: List[str] = None  # type: ignore[assignment]

    def __post_init__(self) -> None:
        self.terminals = self.terminals or []
        self.unknown = self.unknown or []


class PortDiscovery:
    def __init__(self, tms570_baudrate: int = 1000000,
                 terminal_baudrate: int = 115200,
                 endpoint_factory: Callable[..., SerialEndpoint] = SerialEndpoint,
                 line_sink: Optional[Callable[[str, str, str], None]] = None,
                 probe_timeout_s: float = 15.0):
        self.tms570_baudrate = tms570_baudrate
        self.terminal_baudrate = terminal_baudrate
        self.endpoint_factory = endpoint_factory
        self.line_sink = line_sink
        self.probe_timeout_s = probe_timeout_s

    @staticmethod
    def available_ports() -> List[str]:
        from serial.tools import list_ports
        return sorted((port.device for port in list_ports.comports()), key=natural_port_key)

    def discover(self, ports: Optional[List[str]] = None, preferred_terminal: str = "COM14") -> DiscoveredPorts:
        candidates = ports or self.available_ports()
        result = DiscoveredPorts()
        tms_endpoints = {
            p: self.endpoint_factory(p, self.tms570_baudrate, self.line_sink)
            for p in candidates
        }
        try:
            # 先识别570，绝不在这个阶段发送AT+RST。
            for port in candidates:
                endpoint = tms_endpoints[port]
                passive = endpoint.collect(0.4)
                if any(marker in passive for marker in TMS570_BANNERS):
                    result.tms570 = port; break
                reply = endpoint.command("AT+FPGATM", self.probe_timeout_s)
                if ("FPGA_TM rxFrames=" in reply and "FPGA_PARAM mode=" in reply and
                        ("DT head=" in reply or "dmaRxSum=" in reply)):
                    result.tms570 = port; break
        finally:
            for endpoint in tms_endpoints.values(): endpoint.close()

        terminal_candidates = [p for p in candidates if p != result.tms570]
        terminal_endpoints = {
            p: self.endpoint_factory(p, self.terminal_baudrate, self.line_sink)
            for p in terminal_candidates
        }
        try:
            for port in terminal_candidates:
                reply = terminal_endpoints[port].command("AT+RST", 5.0)
                if any(marker in reply for marker in TERMINAL_BANNERS): result.terminals.append(port)
                else: result.unknown.append(port)
        finally:
            for endpoint in terminal_endpoints.values(): endpoint.close()
        if preferred_terminal in result.terminals:
            result.terminals.remove(preferred_terminal)
            result.terminals.insert(0, preferred_terminal)
        return result


class Tms570Console:
    def __init__(self, endpoint: SerialEndpoint):
        self.endpoint = endpoint

    def wait_rc(self, command: int, timeout: float = 5.0) -> str:
        marker = f"RC OK CMD_{command:02X}"
        text = self.endpoint.collect(timeout)
        if marker not in text:
            raise TimeoutError(f"570未输出{marker}")
        return text

    def fpga_tm(self, timeout: float = 15.0, attempts: int = 3) -> Dict[str, Any]:
        last_text = ""
        for _ in range(attempts):
            last_text = self.endpoint.command("AT+FPGATM", timeout,
                                              ("FPGA_TM rxFrames=", "dmaRxSum=", "OK"))
            values = parse_fpga_tm(last_text)
            if all(key in values for key in ("rx_frames", "mode", "dt_pending")):
                values["raw"] = last_text
                return values
        raise TimeoutError(f"570未返回完整AT+FPGATM，最后响应={last_text.strip()}")

    def app_tm(self, timeout: float = 15.0, attempts: int = 3) -> Dict[str, Any]:
        last_text = ""
        for _ in range(attempts):
            last_text = self.endpoint.command("AT+TM", timeout, ("SWEEP gen=", "OK"))
            values = parse_app_tm(last_text)
            if all(key in values for key in ("uptime_ms", "mode", "sweep_generation")):
                values["raw"] = last_text
                return values
        raise TimeoutError(f"570未返回完整AT+TM，最后响应={last_text.strip()}")

    def clear_data_transfer(self, timeout: float = 15.0, attempts: int = 3) -> str:
        transcripts = []
        for _ in range(attempts):
            text = self.endpoint.command("AT+DTCLEAR", timeout,
                                         any_markers=("DTCLEAR OK", "ERROR"))
            transcripts.append(text)
            if "DTCLEAR OK" in text and "ERROR" not in text:
                return "".join(transcripts)
        raise SerialError(f"清理历史数传缓存失败: {''.join(transcripts).strip()}")

    def close(self) -> None:
        self.endpoint.close()


class TerminalConsole:
    def __init__(self, endpoint: SerialEndpoint):
        self.endpoint = endpoint

    def configure_and_join(self, terminal_rate: int, frequency_hz: int = 477800000,
                           join_timeout_s: float = 60.0) -> str:
        transcript = ""
        for command in (f"AT+FREQ={frequency_hz}", f"AT+RATE={terminal_rate}"):
            reply = self.endpoint.command(command, 3.0,
                                          any_markers=("AT_OK", "AT_PARAM_ERROR", "AT_ERROR"))
            transcript += reply
            if "AT_PARAM_ERROR" in reply or "AT_ERROR" in reply:
                raise SerialError(f"终端配置失败: {command}")
        self.endpoint.write("AT+JOIN=0")
        deadline = time.monotonic() + join_timeout_s
        while time.monotonic() < deadline:
            reply = self.endpoint.collect(min(3.0, max(0.1, deadline - time.monotonic())),
                                          any_markers=("+NWKINFO:4",))
            transcript += reply
            if "+NWKINFO:4" in transcript: return transcript
            self.endpoint.write("AT+NWKINFO?")
        raise TimeoutError(f"{self.endpoint.port} 入网超时")

    def dev_eui(self) -> str:
        reply = self.endpoint.command("AT+DEVEUI?", 3.0)
        match = re.search(r"\+DEVEUI:([0-9A-Fa-f]{16})", reply)
        if not match:
            raise SerialError(f"{self.endpoint.port}无法读取DevEUI")
        return match.group(1).upper()

    def send_ack(self, payload: str, port: int = 2, timeout: float = 20.0) -> str:
        self.endpoint.write(f"AT+SENDB=1,{port},{payload}")
        reply = self.endpoint.collect(timeout,
                                      any_markers=("+TXSTATUS:7", "+TXSTATUS:8", "+TXSTATUS:9"))
        if "+TXSTATUS:7" not in reply:
            raise TimeoutError(f"{self.endpoint.port} 未收到+TXSTATUS:7")
        return reply

    def close(self) -> None:
        self.endpoint.close()


def parallel_call(consoles: List[TerminalConsole], action: Callable[[TerminalConsole], Any]) -> List[Any]:
    with ThreadPoolExecutor(max_workers=len(consoles) or 1) as pool:
        futures = [pool.submit(action, console) for console in consoles]
        return [future.result() for future in futures]
