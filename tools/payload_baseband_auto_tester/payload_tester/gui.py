from __future__ import annotations

import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from typing import Dict

from .config import AppConfig, save_config
from .runner import CASE_NAMES, PayloadTestRunner
from .serials import PortDiscovery


class TesterGui:
    def __init__(self, root: tk.Tk, config: AppConfig, tool_root: Path):
        self.root, self.config, self.tool_root = root, config, tool_root
        self.runner = None
        self.messages: queue.Queue[tuple[str, str]] = queue.Queue()
        self.case_vars: Dict[str, tk.BooleanVar] = {}
        self.root.title("载荷基带3.6自动测试工具")
        self.root.geometry("1180x780")
        self._build()
        self.root.after(100, self._pump)

    def _build(self) -> None:
        tabs = ttk.Notebook(self.root); tabs.pack(fill="both", expand=True, padx=8, pady=8)
        setup = ttk.Frame(tabs); tests = ttk.Frame(tabs); logs = ttk.Frame(tabs)
        tabs.add(setup, text="硬件与NS"); tabs.add(tests, text="测试矩阵"); tabs.add(logs, text="实时结果")

        self.spi1 = tk.StringVar(value=self.config.spi.spi1_sn)
        self.spi2 = tk.StringVar(value=self.config.spi.spi2_sn)
        self.tms_port = tk.StringVar(value=self.config.serial.tms570_port)
        self.term_ports = tk.StringVar(value=",".join(self.config.serial.terminal_ports))
        self.mqtt_host = tk.StringVar(value=self.config.mqtt.host)
        self.mqtt_user = tk.StringVar(value=self.config.mqtt.username)
        self.mqtt_password = tk.StringVar(value=self.config.mqtt.password)
        self.gateway = tk.StringVar(value=self.config.gateway.gw_id)

        fields = [
            ("SPI1 SN（遥控遥测）", self.spi1), ("SPI2 SN（数传）", self.spi2),
            ("570串口", self.tms_port), ("终端串口（逗号分隔）", self.term_ports),
            ("MQTT服务器", self.mqtt_host), ("MQTT用户名", self.mqtt_user),
            ("MQTT密码", self.mqtt_password), ("网关ID", self.gateway),
        ]
        for row, (label, variable) in enumerate(fields):
            ttk.Label(setup, text=label).grid(row=row, column=0, sticky="e", padx=5, pady=5)
            entry = ttk.Entry(setup, textvariable=variable, width=65,
                              show="*" if label == "MQTT密码" else "")
            entry.grid(row=row, column=1, sticky="ew", padx=5, pady=5)
        setup.columnconfigure(1, weight=1)
        ttk.Button(setup, text="自动识别串口", command=self._scan_ports).grid(row=8, column=0, padx=5, pady=12)
        ttk.Button(setup, text="保存配置", command=self._save).grid(row=8, column=1, sticky="w", padx=5, pady=12)
        ttk.Label(setup, text="串口识别先发送AT+FPGATM确认570，排除570后才对终端发送AT+RST。",
                  foreground="#555").grid(row=9, column=0, columnspan=2, sticky="w", padx=5)

        for index, case_id in enumerate(CASE_NAMES):
            var = tk.BooleanVar(value=case_id in self.config.test.selected_cases)
            self.case_vars[case_id] = var
            ttk.Checkbutton(tests, text=f"{case_id}  {CASE_NAMES[case_id]}", variable=var).grid(
                row=index // 2, column=index % 2, sticky="w", padx=20, pady=8)
        controls = ttk.Frame(tests); controls.grid(row=8, column=0, columnspan=2, sticky="ew", pady=15)
        self.start_button = ttk.Button(controls, text="开始测试", command=self._start)
        self.start_button.pack(side="left", padx=8)
        ttk.Button(controls, text="停止", command=self._stop).pack(side="left", padx=8)
        ttk.Button(controls, text="模拟运行", command=lambda: self._start(simulate=True)).pack(side="left", padx=8)
        self.status = tk.StringVar(value="就绪")
        ttk.Label(controls, textvariable=self.status).pack(side="left", padx=20)

        self.log = tk.Text(logs, wrap="word", font=("Consolas", 10))
        scroll = ttk.Scrollbar(logs, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side="left", fill="both", expand=True); scroll.pack(side="right", fill="y")

    def _apply(self) -> None:
        self.config.spi.spi1_sn = self.spi1.get().strip()
        self.config.spi.spi2_sn = self.spi2.get().strip()
        self.config.serial.tms570_port = self.tms_port.get().strip()
        self.config.serial.terminal_ports = [item.strip() for item in self.term_ports.get().split(",") if item.strip()]
        self.config.mqtt.host = self.mqtt_host.get().strip()
        self.config.mqtt.username = self.mqtt_user.get().strip()
        self.config.mqtt.password = self.mqtt_password.get()
        self.config.gateway.gw_id = self.gateway.get().strip()
        self.config.test.selected_cases = [case for case, value in self.case_vars.items() if value.get()]

    def _save(self) -> None:
        self._apply()
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON", "*.json")],
                                            initialdir=str(self.tool_root))
        if path:
            save_config(self.config, path)
            messagebox.showinfo("保存完成", "密码不会写入配置；请使用PAYLOAD_TEST_MQTT_PASSWORD环境变量。")

    def _scan_ports(self) -> None:
        self.status.set("正在识别串口…")
        def work() -> None:
            try:
                found = PortDiscovery(self.config.serial.baudrate,
                                      line_sink=lambda p, d, l: self.messages.put((f"serial_{p}", f"{d} {l}"))).discover(
                                          preferred_terminal=self.config.serial.preferred_terminal)
                self.root.after(0, lambda: self._show_ports(found.tms570, found.terminals, found.unknown))
            except Exception as exc:
                self.root.after(0, lambda: messagebox.showerror("串口识别失败", str(exc)))
                self.root.after(0, lambda: self.status.set("识别失败"))
        threading.Thread(target=work, daemon=True).start()

    def _show_ports(self, tms: str, terminals: list[str], unknown: list[str]) -> None:
        self.tms_port.set(tms); self.term_ports.set(",".join(terminals))
        self.status.set(f"570={tms or '未找到'}，终端={len(terminals)}，未知={unknown}")

    def _start(self, simulate: bool = False) -> None:
        if self.runner is not None: return
        self._apply()
        if not self.config.test.selected_cases:
            messagebox.showwarning("没有用例", "请至少选择一个测试项"); return
        self.runner = PayloadTestRunner(self.config, self.tool_root,
                                        lambda source, text: self.messages.put((source, text)), simulate)
        self.start_button.configure(state="disabled"); self.status.set("运行中")
        def work() -> None:
            try:
                result = self.runner.run()
                text = f"完成：{result.run_id}，结果目录={self.runner.run_dir}"
            except Exception as exc: text = f"运行异常：{exc}"
            self.messages.put(("GUI", text))
            self.root.after(0, self._finished)
        threading.Thread(target=work, daemon=True).start()

    def _stop(self) -> None:
        if self.runner: self.runner.stop(); self.status.set("正在停止…")

    def _finished(self) -> None:
        self.runner = None; self.start_button.configure(state="normal"); self.status.set("完成")

    def _pump(self) -> None:
        try:
            while True:
                source, text = self.messages.get_nowait()
                self.log.insert("end", f"[{source}] {text}\n"); self.log.see("end")
        except queue.Empty: pass
        self.root.after(100, self._pump)


def run_gui(config: AppConfig, tool_root: Path) -> None:
    root = tk.Tk(); TesterGui(root, config, tool_root); root.mainloop()
