from __future__ import annotations

import json
import queue
import subprocess
import threading
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from .protocol import RecordDecoder, TelemetryAssembler, build_remote_control


class BridgeError(RuntimeError):
    pass


class SpiBridge:
    def __init__(self, executable: str | Path, event_sink: Optional[Callable[[Dict[str, Any]], None]] = None):
        self.executable = Path(executable).resolve()
        self.event_sink = event_sink or (lambda event: None)
        self.process: Optional[subprocess.Popen[str]] = None
        self._queue: queue.Queue[Dict[str, Any]] = queue.Queue()
        self._reader: Optional[threading.Thread] = None
        self._request_id = 0
        self._lock = threading.Lock()
        self.telemetry = TelemetryAssembler()

    def start(self) -> None:
        if self.process and self.process.poll() is None:
            return
        if not self.executable.exists():
            raise BridgeError(f"SPI桥接程序不存在: {self.executable}")
        self.process = subprocess.Popen(
            [str(self.executable)], cwd=str(self.executable.parent),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace", bufsize=1,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        self._reader = threading.Thread(target=self._read_loop, name="spi-bridge-reader", daemon=True)
        self._reader.start()
        ready = self._queue.get(timeout=5)
        if ready.get("event") != "ready":
            raise BridgeError(f"SPI桥接启动失败: {ready}")

    def _read_loop(self) -> None:
        assert self.process and self.process.stdout
        for line in self.process.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                message = json.loads(line)
            except ValueError:
                message = {"event": "bridge_text", "text": line}
            self._queue.put(message)

    def request(self, op: str, timeout: float = 10.0, **kwargs: Any) -> Dict[str, Any]:
        self.start()
        assert self.process and self.process.stdin
        with self._lock:
            self._request_id += 1
            request_id = self._request_id
            payload = {"id": request_id, "op": op, **kwargs}
            self.process.stdin.write(json.dumps(payload, separators=(",", ":")) + "\n")
            self.process.stdin.flush()
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                try:
                    message = self._queue.get(timeout=min(0.25, max(0.01, deadline - time.monotonic())))
                except queue.Empty:
                    if self.process.poll() is not None:
                        raise BridgeError(f"SPI桥接异常退出: {self.process.returncode}")
                    continue
                if message.get("id") == request_id:
                    if not message.get("ok"):
                        raise BridgeError(str(message.get("error", message)))
                    return message
                self.event_sink(message)
            raise TimeoutError(f"SPI桥接请求超时: {op}")

    def scan(self) -> Dict[str, Any]:
        return self.request("scan")

    def open(self, spi1_sn: str, spi2_sn: str) -> Dict[str, Any]:
        return self.request("open", spi1_sn=spi1_sn, spi2_sn=spi2_sn)

    def spi1_transfer(self, tx: bytes) -> bytes:
        response = self.request("spi1", tx_hex=tx.hex(), timeout=15)
        return bytes.fromhex(response["rx_hex"])

    def send_remote_control(self, command: int, value: Any = None) -> Dict[str, Any]:
        frame = build_remote_control(command, value)
        tx = frame + bytes(128 - len(frame))
        page = self.spi1_transfer(tx)
        telemetry = self.telemetry.feed(page)
        return {"command": command, "frame_hex": frame.hex().upper(), "telemetry": telemetry}

    def collect_telemetry(self, count: int = 2, timeout: float = 10.0) -> List[Dict[str, Any]]:
        result: List[Dict[str, Any]] = []
        deadline = time.monotonic() + timeout
        while len(result) < count and time.monotonic() < deadline:
            page = self.spi1_transfer(bytes(128))
            decoded = self.telemetry.feed(page)
            if decoded is not None:
                result.append(decoded)
            time.sleep(0.02)
        if len(result) < count:
            raise TimeoutError(f"遥测超时: 期望{count}帧，实际{len(result)}帧")
        return result

    def capture_data_transfer(self, max_frames: int = 4096, idle_timeout_ms: int = 2000,
                              timeout: float = 3600.0) -> Dict[str, Any]:
        frames: List[bytes] = []
        previous_sink = self.event_sink

        def sink(event: Dict[str, Any]) -> None:
            if event.get("event") == "dt_frame":
                frames.append(bytes.fromhex(event["frame_hex"]))
            previous_sink(event)

        self.event_sink = sink
        try:
            response = self.request("spi2_capture", timeout=timeout, max_frames=max_frames,
                                    idle_timeout_ms=idle_timeout_ms, arm_delay_ms=100)
        finally:
            self.event_sink = previous_sink
        decoder = RecordDecoder()
        records = []
        for frame in frames:
            records.extend(decoder.feed_frame(frame))
        return {"response": response, "frames": frames, "records": records,
                "partial_record_bytes": len(decoder.pending)}

    def close(self) -> None:
        process = self.process
        if not process:
            return
        if process.poll() is None:
            try:
                self.request("quit", timeout=3)
            except Exception:
                process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
        self.process = None

    def __enter__(self) -> "SpiBridge":
        self.start()
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()
