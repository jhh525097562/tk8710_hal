#!/usr/bin/env python3
"""TK8710 RF DC calibration TCP client and Tkinter test panel."""

import argparse
import os
import re
import socket
import subprocess
import sys
import threading
import time
import traceback
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Callable, Optional


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 12879
DEFAULT_REGISTER = 0x08C8
SERVER_GREETING = "OK RF_CAL_SERVER 1"
MAX_RESPONSE_LENGTH = 1024
AUTO_LOG_REFRESH_MS = 1000
LOG_RESPONSE_RE = re.compile(r"^OK LOG seq=(\d+)\b")
STARTUP_LOG_NAME = "TK8710_RF_Test_GUI_startup.log"


def application_root() -> Path:
    """Return the directory that owns bundled runtime assets."""
    if getattr(sys, "frozen", False):
        bundle_root = getattr(sys, "_MEIPASS", None)
        if bundle_root:
            return Path(bundle_root).resolve()
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[1]


def startup_log_path() -> Path:
    """Return a writable path for failures that happen before the GUI appears."""
    local_app_data = os.environ.get("LOCALAPPDATA")
    base_dir = Path(local_app_data) if local_app_data else Path.home()
    return base_dir / "TK8710_RF_Test_GUI" / STARTUP_LOG_NAME


def run_with_startup_diagnostics() -> int:
    try:
        return main()
    except Exception as exc:
        log_path = startup_log_path()
        try:
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(traceback.format_exc(), encoding="utf-8")
        except OSError:
            pass

        try:
            from tkinter import messagebox

            messagebox.showerror(
                "TK8710 RF Test GUI",
                f"Program startup failed: {exc}\n\nDiagnostic log: {log_path}",
            )
        except Exception:
            pass
        return 1


def extract_log_sequence(response: str) -> Optional[int]:
    match = LOG_RESPONSE_RE.match(response)
    if match is None:
        return None
    return int(match.group(1))


def parse_integer(text: str, minimum: int, maximum: int, name: str) -> int:
    """Parse a decimal or 0x-prefixed integer and enforce its range."""
    value_text = text.strip()
    if not value_text:
        raise ValueError(f"{name}不能为空")
    try:
        value = int(value_text, 0)
    except ValueError as exc:
        raise ValueError(f"{name}不是有效的十进制或0x十六进制整数: {text}") from exc
    if value < minimum or value > maximum:
        raise ValueError(f"{name}范围应为{minimum}~{maximum}，当前值为{value}")
    return value


def parse_dc_component(text: str, name: str) -> int:
    """Parse a signed 16-bit value or its unsigned 16-bit hexadecimal form."""
    return parse_integer(text, -0x8000, 0xFFFF, name)


def pack_dc_word(i_dc: int, q_dc: int) -> int:
    """Pack I_DC into bits 31:16 and Q_DC into bits 15:0."""
    if not -0x8000 <= i_dc <= 0xFFFF:
        raise ValueError("I_DC超出16位范围")
    if not -0x8000 <= q_dc <= 0xFFFF:
        raise ValueError("Q_DC超出16位范围")
    return ((i_dc & 0xFFFF) << 16) | (q_dc & 0xFFFF)


def build_service_arguments(
    executable: Path,
    mode: int,
    frequency: int,
    tx_gain: int,
    rx_gain: int,
    test_select: int,
    host: str,
    port: int,
):
    return [
        str(executable),
        str(mode),
        str(frequency),
        str(tx_gain),
        str(rx_gain),
        str(test_select),
        "--tcp",
        "--bind",
        host,
        "--port",
        str(port),
    ]


class RfCalibrationClient:
    """Line-based client for the Test8710RFTxTone RF calibration server."""

    def __init__(self, timeout: float = 3.0) -> None:
        self.timeout = timeout
        self._socket: Optional[socket.socket] = None
        self._reader = None
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self._socket is not None

    def connect(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT) -> str:
        self.close()
        sock = socket.create_connection((host, port), timeout=self.timeout)
        sock.settimeout(self.timeout)
        reader = sock.makefile("rb")
        self._socket = sock
        self._reader = reader
        try:
            greeting = self._readline()
            if greeting != SERVER_GREETING:
                raise RuntimeError(f"服务端握手响应异常: {greeting}")
            return greeting
        except Exception:
            self.close()
            raise

    def close(self) -> None:
        reader = self._reader
        sock = self._socket
        self._reader = None
        self._socket = None
        if reader is not None:
            try:
                reader.close()
            except OSError:
                pass
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def _readline(self) -> str:
        if self._reader is None:
            raise RuntimeError("尚未连接TCP服务")
        line = self._reader.readline(MAX_RESPONSE_LENGTH + 1)
        if not line:
            raise ConnectionError("TCP连接已由服务端关闭")
        if len(line) > MAX_RESPONSE_LENGTH or not line.endswith(b"\n"):
            raise RuntimeError("服务端响应过长或缺少换行符")
        return line.rstrip(b"\r\n").decode("utf-8", errors="replace")

    def command(self, command: str) -> str:
        with self._lock:
            if self._socket is None:
                raise RuntimeError("尚未连接TCP服务")
            self._socket.sendall((command + "\n").encode("ascii"))
            response = self._readline()
            if response.startswith("ERR "):
                raise RuntimeError(response)
            if not response.startswith("OK "):
                raise RuntimeError(f"未知服务端响应: {response}")
            return response

    def ping(self) -> str:
        response = self.command("PING")
        if response != "OK PONG":
            raise RuntimeError(f"PING响应异常: {response}")
        return response

    def get_stats(self) -> str:
        return self.command("STATS")

    def get_log(self) -> str:
        return self.command("LOG")

    def read_register(self, address: int) -> int:
        response = self.command(f"READ 0x{address:04X}")
        match = re.fullmatch(r"OK READ 0x[0-9A-Fa-f]+ 0x([0-9A-Fa-f]{1,8})", response)
        if match is None:
            raise RuntimeError(f"READ响应格式异常: {response}")
        return int(match.group(1), 16)

    def write_register(self, address: int, value: int) -> int:
        response = self.command(f"WRITE 0x{address:04X} 0x{value:08X}")
        match = re.fullmatch(
            r"OK WRITE 0x[0-9A-Fa-f]+ 0x[0-9A-Fa-f]{1,8} "
            r"READBACK 0x([0-9A-Fa-f]{1,8})",
            response,
        )
        if match is None:
            raise RuntimeError(f"WRITE响应格式异常: {response}")
        readback = int(match.group(1), 16)
        if readback != value:
            raise RuntimeError(
                f"写后回读不一致: 写入0x{value:08X}，回读0x{readback:08X}"
            )
        return readback

    def quit(self) -> str:
        try:
            return self.command("QUIT")
        finally:
            self.close()

    def shutdown(self) -> str:
        try:
            return self.command("SHUTDOWN")
        finally:
            self.close()


class RfCalibrationApp:
    """Tkinter front end for interactive calibration and connectivity tests."""

    def __init__(self, root) -> None:
        import tkinter as tk
        from tkinter import ttk

        self.root = root
        self.tk = tk
        self.ttk = ttk
        self.client: Optional[RfCalibrationClient] = None
        self.local_process: Optional[subprocess.Popen] = None
        self._operation_lock = threading.Lock()
        self._auto_log_after_id = None
        self._auto_log_in_flight = False
        self._last_auto_log_seq: Optional[int] = None
        self._last_auto_log_response: Optional[str] = None
        self._local_output_lines = deque(maxlen=20)
        self._local_output_lock = threading.Lock()
        self._closing = False

        default_exe = application_root() / "build_jtool" / "Test8710RFTest.exe"

        root.title("TK8710 射频测试工具")
        root.geometry("940x740")
        root.minsize(820, 650)

        self.host_var = tk.StringVar(value=DEFAULT_HOST)
        self.port_var = tk.StringVar(value=str(DEFAULT_PORT))
        self.status_var = tk.StringVar(value="未连接")
        self.exe_var = tk.StringVar(value=str(default_exe))
        self.mode_var = tk.StringVar(value="6")
        self.frequency_var = tk.StringVar(value="509100000")
        self.gain_var = tk.StringVar(value="42")
        self.rx_gain_var = tk.StringVar(value="0x7e")
        self.test_select_var = tk.StringVar(value="0")
        self.auto_log_var = tk.IntVar(value=1)
        self.address_var = tk.StringVar(value=f"0x{DEFAULT_REGISTER:04X}")
        self.value_var = tk.StringVar(value="0x00000000")
        self.i_dc_var = tk.StringVar(value="0")
        self.q_dc_var = tk.StringVar(value="0")

        container = ttk.Frame(root, padding=10)
        container.pack(fill="both", expand=True)
        self._build_connection_frame(container)
        self._build_service_frame(container)
        self._build_register_frame(container)
        self._build_dc_frame(container)
        self._build_log_frame(container)
        root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_connection_frame(self, parent) -> None:
        frame = self.ttk.LabelFrame(parent, text="TCP连接", padding=8)
        frame.pack(fill="x", pady=(0, 8))
        self.ttk.Label(frame, text="IP").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(frame, textvariable=self.host_var, width=18).grid(
            row=0, column=1, padx=(5, 12)
        )
        self.ttk.Label(frame, text="端口").grid(row=0, column=2, sticky="w")
        self.ttk.Entry(frame, textvariable=self.port_var, width=8).grid(
            row=0, column=3, padx=(5, 12)
        )
        self.ttk.Button(frame, text="连接", command=self._connect).grid(row=0, column=4, padx=3)
        self.ttk.Button(frame, text="断开", command=self._disconnect).grid(row=0, column=5, padx=3)
        self.ttk.Button(frame, text="PING", command=self._ping).grid(row=0, column=6, padx=3)
        self.ttk.Label(frame, textvariable=self.status_var).grid(
            row=0, column=7, padx=(15, 0), sticky="w"
        )

    def _build_service_frame(self, parent) -> None:
        frame = self.ttk.LabelFrame(parent, text="本机服务（可选）", padding=8)
        frame.pack(fill="x", pady=(0, 8))
        self.ttk.Label(frame, text="程序").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(frame, textvariable=self.exe_var).grid(
            row=0, column=1, columnspan=5, padx=5, sticky="ew"
        )
        self.ttk.Button(frame, text="选择...", command=self._browse_executable).grid(
            row=0, column=6, padx=3
        )
        labels = (
            ("速率模式", self.mode_var),
            ("频率Hz", self.frequency_var),
            ("TX gain", self.gain_var),
            ("RX gain", self.rx_gain_var),
            ("测试模式", self.test_select_var),
        )
        for index, (label, variable) in enumerate(labels):
            row = 1 + index // 3
            column = (index % 3) * 2
            self.ttk.Label(frame, text=label).grid(row=row, column=column, pady=(8, 0), sticky="w")
            if variable is self.test_select_var:
                self.ttk.Combobox(
                    frame,
                    textvariable=variable,
                    values=("0", "1", "2"),
                    state="readonly",
                    width=12,
                ).grid(row=row, column=column + 1, padx=(5, 12), pady=(8, 0), sticky="w")
            else:
                self.ttk.Entry(frame, textvariable=variable, width=14).grid(
                    row=row, column=column + 1, padx=(5, 12), pady=(8, 0), sticky="w"
                )
        self.ttk.Button(frame, text="启动并连接", command=self._start_local_service).grid(
            row=3, column=0, columnspan=2, pady=(8, 0), sticky="w"
        )
        self.ttk.Button(frame, text="停止服务", command=self._stop_service).grid(
            row=3, column=2, columnspan=2, pady=(8, 0), sticky="w"
        )
        frame.columnconfigure(1, weight=1)

    def _build_register_frame(self, parent) -> None:
        frame = self.ttk.LabelFrame(parent, text="寄存器读写", padding=8)
        frame.pack(fill="x", pady=(0, 8))
        self.ttk.Label(frame, text="地址").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(frame, textvariable=self.address_var, width=14).grid(
            row=0, column=1, padx=(5, 15)
        )
        self.ttk.Label(frame, text="32位数值").grid(row=0, column=2, sticky="w")
        self.ttk.Entry(frame, textvariable=self.value_var, width=18).grid(
            row=0, column=3, padx=(5, 15)
        )
        self.ttk.Button(frame, text="读取", command=self._read_register).grid(row=0, column=4, padx=3)
        self.ttk.Button(frame, text="写入并回读", command=self._write_register).grid(
            row=0, column=5, padx=3
        )

    def _build_dc_frame(self, parent) -> None:
        frame = self.ttk.LabelFrame(parent, text="I/Q直流补偿组字", padding=8)
        frame.pack(fill="x", pady=(0, 8))
        self.ttk.Label(frame, text="I_DC [31:16]").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(frame, textvariable=self.i_dc_var, width=14).grid(
            row=0, column=1, padx=(5, 15)
        )
        self.ttk.Label(frame, text="Q_DC [15:0]").grid(row=0, column=2, sticky="w")
        self.ttk.Entry(frame, textvariable=self.q_dc_var, width=14).grid(
            row=0, column=3, padx=(5, 15)
        )
        self.ttk.Button(frame, text="生成寄存器值", command=self._generate_dc_word).grid(
            row=0, column=4, padx=3
        )
        self.ttk.Button(frame, text="生成并写入", command=self._generate_and_write).grid(
            row=0, column=5, padx=3
        )
        self.ttk.Label(
            frame,
            text="支持-32768~32767有符号输入，也支持0x0000~0xFFFF原始16位值。",
        ).grid(row=1, column=0, columnspan=6, pady=(7, 0), sticky="w")

    def _build_log_frame(self, parent) -> None:
        frame = self.ttk.LabelFrame(parent, text="操作日志", padding=8)
        frame.pack(fill="both", expand=True)
        toolbar = self.ttk.Frame(frame)
        toolbar.pack(fill="x", pady=(0, 6))
        self.ttk.Button(toolbar, text="刷新统计", command=self._get_stats).pack(
            side="left", padx=(0, 6)
        )
        self.ttk.Button(toolbar, text="刷新日志", command=self._get_log).pack(
            side="left", padx=(0, 6)
        )
        self.ttk.Checkbutton(
            toolbar,
            text="自动刷新日志",
            variable=self.auto_log_var,
            command=self._toggle_auto_log_refresh,
        ).pack(side="left", padx=(8, 6))
        self.log_text = self.tk.Text(frame, height=15, wrap="word", state="disabled")
        scrollbar = self.ttk.Scrollbar(frame, orient="vertical", command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        self.log_text.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        self._log("页面已就绪。可先启动本机服务，也可直接连接远端服务。")

    def _log(self, message: str) -> None:
        def append() -> None:
            timestamp = datetime.now().strftime("%H:%M:%S")
            self.log_text.configure(state="normal")
            self.log_text.insert("end", f"[{timestamp}] {message}\n")
            self.log_text.see("end")
            self.log_text.configure(state="disabled")

        if threading.current_thread() is threading.main_thread():
            append()
        else:
            self.root.after(0, append)

    def _set_status(self, value: str) -> None:
        self.root.after(0, lambda: self.status_var.set(value))

    def _stream_local_process_output(self, process: subprocess.Popen) -> None:
        if process.stdout is None:
            return
        for raw_line in process.stdout:
            line = raw_line.rstrip("\r\n")
            if not line:
                continue
            with self._local_output_lock:
                self._local_output_lines.append(line)
            self._log(f"[RFTest] {line}")

    def _local_process_error_detail(self) -> Optional[str]:
        with self._local_output_lock:
            if not self._local_output_lines:
                return None
            return self._local_output_lines[-1]

    def _run_async(
        self,
        description: str,
        task: Callable[[], object],
        on_success: Optional[Callable[[object], None]] = None,
    ) -> None:
        if not self._operation_lock.acquire(blocking=False):
            self._log("已有操作正在执行，请稍候。")
            return

        def worker() -> None:
            try:
                result = task()
                if on_success is not None:
                    self.root.after(0, lambda: on_success(result))
            except Exception as exc:
                self._log(f"{description}失败：{exc}")
            finally:
                self._operation_lock.release()

        threading.Thread(target=worker, daemon=True).start()

    def _auto_log_enabled(self) -> bool:
        return (
            not self._closing
            and bool(self.auto_log_var.get())
            and self.client is not None
            and self.client.connected
        )

    def _reset_auto_log_state(self) -> None:
        self._last_auto_log_seq = None
        self._last_auto_log_response = None

    def _record_auto_log_response(self, response: str) -> bool:
        sequence = extract_log_sequence(response)
        if sequence is not None:
            if self._last_auto_log_seq == sequence:
                return False
            self._last_auto_log_seq = sequence
        elif self._last_auto_log_response == response:
            return False

        self._last_auto_log_response = response
        return True

    def _toggle_auto_log_refresh(self) -> None:
        if self.auto_log_var.get():
            self._schedule_auto_log_refresh()
        else:
            self._cancel_auto_log_refresh()

    def _schedule_auto_log_refresh(self) -> None:
        if self._auto_log_after_id is not None or not self._auto_log_enabled():
            return
        self._auto_log_after_id = self.root.after(
            AUTO_LOG_REFRESH_MS,
            self._auto_log_refresh_tick,
        )

    def _cancel_auto_log_refresh(self) -> None:
        after_id = self._auto_log_after_id
        self._auto_log_after_id = None
        if after_id is not None:
            try:
                self.root.after_cancel(after_id)
            except Exception:
                pass

    def _auto_log_refresh_tick(self) -> None:
        self._auto_log_after_id = None
        if not self._auto_log_enabled():
            return
        if self._auto_log_in_flight:
            self._schedule_auto_log_refresh()
            return

        client = self.client
        if client is None:
            return

        self._auto_log_in_flight = True

        def worker() -> None:
            response = None
            error = None
            try:
                response = client.get_log()
            except Exception as exc:
                error = str(exc)
            try:
                self.root.after(
                    0,
                    lambda response=response, error=error: self._complete_auto_log_refresh(
                        response,
                        error,
                    ),
                )
            except RuntimeError:
                pass

        threading.Thread(target=worker, daemon=True).start()

    def _complete_auto_log_refresh(
        self,
        response: Optional[str],
        error: Optional[str],
    ) -> None:
        self._auto_log_in_flight = False
        if self._closing:
            return
        if error is not None:
            if self.auto_log_var.get():
                self.auto_log_var.set(0)
                self._log(f"自动刷新日志已停止：{error}")
            return
        if response is not None and self._record_auto_log_response(response):
            self._log(response)
        self._schedule_auto_log_refresh()

    def _connection_parameters(self):
        host = self.host_var.get().strip()
        if not host:
            raise ValueError("IP不能为空")
        port = parse_integer(self.port_var.get(), 1, 65535, "端口")
        return host, port

    def _replace_client(self, client: Optional[RfCalibrationClient]) -> None:
        old_client = self.client
        self.client = client
        self._reset_auto_log_state()
        if old_client is not None and old_client is not client:
            old_client.close()
        if client is None:
            self.root.after(0, self._cancel_auto_log_refresh)
        else:
            self.root.after(0, self._schedule_auto_log_refresh)

    def _require_client(self) -> RfCalibrationClient:
        if self.client is None or not self.client.connected:
            raise RuntimeError("请先连接TCP服务")
        return self.client

    def _connect(self) -> None:
        try:
            host, port = self._connection_parameters()
        except ValueError as exc:
            self._log(str(exc))
            return

        def task() -> object:
            client = RfCalibrationClient()
            greeting = client.connect(host, port)
            self._replace_client(client)
            self._set_status(f"已连接 {host}:{port}")
            self._log(greeting)
            return greeting

        self._run_async("连接", task)

    def _disconnect(self) -> None:
        self._cancel_auto_log_refresh()
        self._reset_auto_log_state()
        client = self.client
        self.client = None
        if client is not None:
            client.close()
        self.status_var.set("未连接")
        self._log("TCP连接已断开。")

    def _ping(self) -> None:
        def task() -> object:
            response = self._require_client().ping()
            self._log(response)
            return response

        self._run_async("PING", task)

    def _get_stats(self) -> None:
        def task() -> object:
            response = self._require_client().get_stats()
            self._log(response)
            return response

        self._run_async("刷新统计", task)

    def _get_log(self) -> None:
        def task() -> object:
            response = self._require_client().get_log()
            self._record_auto_log_response(response)
            self._log(response)
            return response

        self._run_async("刷新日志", task)

    def _read_register(self) -> None:
        try:
            address = parse_integer(self.address_var.get(), 0, 0xFFFF, "寄存器地址")
        except ValueError as exc:
            self._log(str(exc))
            return

        def task() -> object:
            value = self._require_client().read_register(address)
            self._log(f"READ 0x{address:04X} -> 0x{value:08X}")
            return value

        def success(result: object) -> None:
            self.value_var.set(f"0x{int(result):08X}")

        self._run_async("读取寄存器", task, success)

    def _write_register(self) -> None:
        try:
            address = parse_integer(self.address_var.get(), 0, 0xFFFF, "寄存器地址")
            value = parse_integer(self.value_var.get(), 0, 0xFFFFFFFF, "寄存器值")
        except ValueError as exc:
            self._log(str(exc))
            return

        def task() -> object:
            readback = self._require_client().write_register(address, value)
            self._log(f"WRITE 0x{address:04X} = 0x{value:08X}，回读一致")
            return readback

        self._run_async("写入寄存器", task)

    def _get_dc_word(self) -> int:
        i_dc = parse_dc_component(self.i_dc_var.get(), "I_DC")
        q_dc = parse_dc_component(self.q_dc_var.get(), "Q_DC")
        return pack_dc_word(i_dc, q_dc)

    def _generate_dc_word(self) -> None:
        try:
            value = self._get_dc_word()
        except ValueError as exc:
            self._log(str(exc))
            return
        self.value_var.set(f"0x{value:08X}")
        self._log(f"I/Q组字结果：0x{value:08X}")

    def _generate_and_write(self) -> None:
        try:
            value = self._get_dc_word()
        except ValueError as exc:
            self._log(str(exc))
            return
        self.value_var.set(f"0x{value:08X}")
        self._write_register()

    def _browse_executable(self) -> None:
        from tkinter import filedialog

        path = filedialog.askopenfilename(
            title="选择Test8710RFTest.exe",
            filetypes=(("Windows executable", "*.exe"), ("All files", "*.*")),
        )
        if path:
            self.exe_var.set(path)

    def _start_local_service(self) -> None:
        try:
            host, port = self._connection_parameters()
            executable = Path(self.exe_var.get().strip()).expanduser().resolve()
            mode = parse_integer(self.mode_var.get(), 0, 0xFFFFFFFF, "模式")
            frequency = parse_integer(self.frequency_var.get(), 1, 0xFFFFFFFF, "频率")
            tx_gain = parse_integer(self.gain_var.get(), 0, 0xFF, "TX gain")
            rx_gain = parse_integer(self.rx_gain_var.get(), 0, 0xFF, "RX gain")
            test_select = parse_integer(self.test_select_var.get(), 0, 2, "测试模式")
        except (ValueError, OSError) as exc:
            self._log(str(exc))
            return
        if not executable.is_file():
            self._log(f"程序不存在：{executable}")
            return
        if self.local_process is not None and self.local_process.poll() is None:
            self._log("本页面启动的服务仍在运行。")
            return

        arguments = build_service_arguments(
            executable,
            mode,
            frequency,
            tx_gain,
            rx_gain,
            test_select,
            host,
            port,
        )

        def task() -> object:
            creation_flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0
            with self._local_output_lock:
                self._local_output_lines.clear()
            self.local_process = subprocess.Popen(
                arguments,
                cwd=str(executable.parent),
                creationflags=creation_flags,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
            )
            output_thread = threading.Thread(
                target=self._stream_local_process_output,
                args=(self.local_process,),
                daemon=True,
            )
            output_thread.start()
            self._log(f"已启动本机程序，PID={self.local_process.pid}，等待TCP服务就绪...")
            deadline = time.monotonic() + 30.0
            last_error: Optional[Exception] = None
            while time.monotonic() < deadline:
                if self.local_process.poll() is not None:
                    output_thread.join(timeout=1.0)
                    detail = self._local_process_error_detail()
                    message = f"本机程序已退出，退出码={self.local_process.returncode}"
                    if detail is not None:
                        message += f"；最后输出：{detail}"
                    raise RuntimeError(message)
                client = RfCalibrationClient(timeout=1.0)
                try:
                    greeting = client.connect(host, port)
                    self._replace_client(client)
                    self._set_status(f"已连接 {host}:{port}")
                    self._log(greeting)
                    return greeting
                except (OSError, RuntimeError) as exc:
                    last_error = exc
                    client.close()
                    time.sleep(0.5)
            raise TimeoutError(f"30秒内TCP服务未就绪，最后错误：{last_error}")

        self._run_async("启动本机服务", task)

    def _stop_service(self) -> None:
        try:
            host, port = self._connection_parameters()
        except ValueError as exc:
            self._log(str(exc))
            return

        def task() -> object:
            client = self.client
            if client is None or not client.connected:
                client = RfCalibrationClient()
                client.connect(host, port)
            response = client.shutdown()
            if client is self.client:
                self.client = None
                self._reset_auto_log_state()
                self.root.after(0, self._cancel_auto_log_refresh)
            self._set_status("未连接")
            self._log(response)
            return response

        self._run_async("停止服务", task)

    def _on_close(self) -> None:
        self._closing = True
        self._cancel_auto_log_refresh()
        if self.client is not None:
            self.client.close()
            self.client = None
        self.root.destroy()


def main() -> int:
    parser = argparse.ArgumentParser(description="TK8710射频测试页面")
    parser.add_argument("--host", default=DEFAULT_HOST, help="默认TCP服务IP")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="默认TCP服务端口")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port范围应为1~65535")

    try:
        import tkinter as tk
    except ImportError as exc:
        print(f"当前Python未安装Tkinter: {exc}", file=sys.stderr)
        return 1

    root = tk.Tk()
    app = RfCalibrationApp(root)
    app.host_var.set(args.host)
    app.port_var.set(str(args.port))
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(run_with_startup_diagnostics())
