"""Single-reader serial transport: commands and unsolicited MAC logs coexist."""
from __future__ import annotations

import base64
import collections
import json
import re
import threading
import time
from datetime import datetime, timezone
from pathlib import Path


class Journal:
    def __init__(self, path):
        self.lock = threading.Lock()
        self.file = Path(path).open("w", encoding="utf-8")

    def emit(self, kind, **fields):
        with self.lock:
            record = dict(kind=kind, time_ms=time.monotonic_ns()/1e6,
                          utc=datetime.now(timezone.utc).isoformat(), **fields)
            self.file.write(json.dumps(record, ensure_ascii=False)+"\n")
            self.file.flush()
            return record

    def close(self):
        self.file.close()


def discover(baudrate):
    import serial
    from serial.tools import list_ports
    matches = []
    for port in list_ports.comports():
        try:
            with serial.Serial(port.device, baudrate, timeout=.2, write_timeout=2) as device:
                device.write(b"AT+DEVEUI?\r\n")
                device.flush()
                deadline = time.monotonic()+2
                response = bytearray()
                while time.monotonic() < deadline:
                    response.extend(device.read(4096))
                if re.search(rb"(?:\+)?DEVEUI\s*[:=]\s*[0-9a-fA-F]{16}", response):
                    matches.append(port.device)
        except (OSError, serial.SerialException):
            continue
    if len(matches) != 1:
        raise RuntimeError(f"只读 DEVEUI 探测匹配 {matches}；请通过 --serial-port COMxx 指定终端")
    return matches[0]


class Terminal:
    def __init__(self, port, baudrate, journal):
        import serial
        self.journal = journal
        self.device = serial.Serial(port, baudrate, timeout=.1, write_timeout=2)
        self.condition = threading.Condition()
        self.records = collections.deque(maxlen=20000)
        self.sequence = 0
        self.error = None
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self._read, name="terminal-reader", daemon=True)
        try:
            self.thread.start()
        except Exception:
            self.device.close()
            raise

    def _line(self, raw):
        line = raw.decode("utf-8", errors="replace").rstrip("\r")
        record = self.journal.emit("terminal", text=line)
        with self.condition:
            self.sequence += 1
            self.records.append((self.sequence, record))
            self.condition.notify_all()

    def _read(self):
        pending = bytearray()
        try:
            while not self.stop.is_set():
                data = self.device.read(4096)
                if not data:
                    continue
                self.journal.emit("serial_bytes", base64=base64.b64encode(data).decode("ascii"))
                pending.extend(data)
                while b"\n" in pending:
                    raw, _, tail = pending.partition(b"\n")
                    pending = bytearray(tail)
                    self._line(raw)
                if len(pending) > 65536:
                    raise RuntimeError("串口连续 64 KiB 无换行，原始字节已保存")
        except Exception as exc:
            self.error = exc
            with self.condition:
                self.condition.notify_all()
        finally:
            if pending:
                self.journal.emit("terminal_partial", text=pending.decode("utf-8", errors="replace"))

    def check(self):
        if self.error:
            raise RuntimeError("终端采集失败") from self.error

    def wait(self, start_sequence, predicate, timeout):
        deadline = time.monotonic()+timeout
        cursor = start_sequence
        collected = []
        with self.condition:
            while True:
                self.check()
                if self.records and cursor < self.records[0][0]-1:
                    raise RuntimeError("命令等待窗口超出缓存；原始日志仍保留")
                for sequence, record in self.records:
                    if sequence > cursor:
                        cursor = sequence
                        collected.append(record["text"])
                        if predicate(record["text"]):
                            return collected
                remaining = deadline-time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("等待终端响应超时")
                self.condition.wait(min(.2, remaining))

    def command(self, command, timeout=3):
        self.check()
        with self.condition:
            cursor = self.sequence
        self.journal.emit("command", text=command)
        self.device.write((command+"\r\n").encode("ascii"))
        self.device.flush()
        lines = self.wait(cursor, lambda line: line.strip() == "AT_OK" or
                          bool(re.fullmatch(r"AT_(?:[A-Z_]*ERROR)", line.strip())), timeout)
        if any(re.fullmatch(r"AT_(?:[A-Z_]*ERROR)", line.strip()) for line in lines):
            raise RuntimeError(f"终端拒绝命令 {command}: {lines}")
        return cursor

    def configure_and_join(self, config):
        for command in ("AT+DEVEUI=0000000000000003", "AT+DEVMODE=2", "AT+PRINTMODE=MAC,1",
                        f"AT+FREQCFG={config.primary_channel},{config.secondary_channel}",
                        f"AT+RATE={config.rate}"):
            self.command(command)
        cursor = self.command("AT+JOIN=0", timeout=10)
        self.wait(cursor, lambda line: bool(re.fullmatch(r"\+NWKINFO:4\s*", line)), config.join_timeout)

    def close(self):
        self.stop.set()
        self.thread.join(timeout=3)
        self.device.close()
        if self.thread.is_alive():
            self.thread.join(timeout=3)
        if self.thread.is_alive():
            raise RuntimeError("终端采集线程未退出")
        self.check()
