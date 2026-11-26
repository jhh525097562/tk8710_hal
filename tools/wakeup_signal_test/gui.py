"""Desktop settings and progress UI for the wakeup campaign."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText
import uuid

import run
from serial_probe import detect

HERE = Path(__file__).resolve().parent


def enable_dpi_awareness():
    if os.name == "nt":
        import ctypes
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except OSError:
            pass


def parse_numbers(text):
    values = []
    for part in text.replace("，", ",").split(","):
        if "-" in part:
            first, last = map(int, part.strip().split("-"))
            if not 0 <= first <= last <= 255:
                raise ValueError("范围需要满足 0 ≤ 起点 ≤ 终点 ≤ 255")
            values.extend(range(first, last + 1))
        else:
            values.append(int(part.strip()))
    return list(dict.fromkeys(values))


class App:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue()
        self.process = None
        self.scanning = False
        self.closing = False
        self.cancel_scan = threading.Event()
        self.stop_file = None
        self.active_output = None
        self.fields = {}
        self.buttons = []
        self.defaults = json.loads((HERE / "config.example.json").read_text(encoding="utf-8"))
        settings_file = HERE / "settings.local.json"
        if settings_file.exists():
            try:
                saved = json.loads(settings_file.read_text(encoding="utf-8-sig"))
                for key in ("host", "ssh_port", "username", "serial_port", "baudrate"):
                    if key in saved:
                        self.defaults[key] = saved[key]
            except (OSError, ValueError):
                pass
        private = HERE / "password.local.json"
        password = os.getenv("TK8710_SSH_PASSWORD", "")
        if private.exists():
            password = json.loads(private.read_text(encoding="utf-8-sig")).get("password", password)
        root.title("TK8710 / TK8620 唤醒信号测试")
        height = min(800, root.winfo_screenheight() - 80)
        root.geometry(f"1000x{height}")
        root.minsize(820, 580)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(4, weight=1)

        ssh = ttk.LabelFrame(root, text="发射端 SSH", padding=8)
        ssh.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 4))
        self.entry(ssh, "地址", "host", self.defaults["host"], 0, 0)
        self.entry(ssh, "端口", "ssh_port", self.defaults["ssh_port"], 0, 2, width=7)
        self.entry(ssh, "用户名", "username", self.defaults["username"], 0, 4, width=12)
        self.entry(ssh, "密码", "password", password, 0, 6, width=14, show="*")
        self.entry(ssh, "ARM 程序", "local_bin",
                   str((HERE / self.defaults["local_bin"]).resolve()), 1, 0, span=6)
        self.button(ssh, "选择程序", self.choose_binary, row=1, column=7)

        terminal = ttk.LabelFrame(root, text="接收终端串口", padding=8)
        terminal.grid(row=1, column=0, sticky="ew", padx=10, pady=4)
        ttk.Label(terminal, text="串口").grid(row=0, column=0, padx=4)
        self.fields["serial_port"] = tk.StringVar(value=self.defaults["serial_port"])
        self.port_box = ttk.Combobox(terminal, textvariable=self.fields["serial_port"], width=20)
        self.port_box.grid(row=0, column=1, padx=4)
        self.entry(terminal, "波特率", "baudrate", self.defaults["baudrate"], 0, 2, width=12)
        self.entry(terminal, "探测等待秒", "scan_timeout", 3, 0, 4, width=6)
        self.button(terminal, "自动检测（发送 AT+RST）", self.scan, row=0, column=6)
        ttk.Label(terminal, text="检测会逐个复位可打开的串口设备；多个有日志的端口需手动选择。"
                  ).grid(row=1, column=0, columnspan=7, sticky="w", padx=4, pady=(6, 0))

        test = ttk.LabelFrame(root, text="测试参数", padding=8)
        test.grid(row=2, column=0, sticky="ew", padx=10, pady=4)
        self.entry(test, "模式", "modes", "1,2,3", 0, 0, width=12)
        self.entry(test, "ID", "ids", "0-255", 0, 2, width=24)
        self.entry(test, "频率 Hz", "frequency_hz", self.defaults["frequency_hz"], 0, 4, width=14)
        self.entry(test, "发送 ms", "duration_ms", 5000, 1, 0, width=12)
        self.entry(test, "接收 ms", "receiver_duration_ms", 1000, 1, 2, width=12)
        self.entry(test, "重复次数", "repeats", 1, 1, 4, width=8)
        self.entry(test, "TX增益", "tx_gain", self.defaults["tx_gain"], 2, 0, width=12)
        self.entry(test, "定时唤醒 ms", "timer_wakeup_ms", self.defaults["timer_wakeup_ms"], 2, 2, width=12)
        ttk.Label(test, text="每项最多尝试3次；定时唤醒自动恢复，3次未检测到无线唤醒则继续下一项。"
                  ).grid(row=3, column=0, columnspan=6, sticky="w", padx=4, pady=(6, 0))

        controls = ttk.Frame(root)
        controls.grid(row=3, column=0, sticky="ew", padx=10, pady=6)
        self.button(controls, "开始测试", self.start, row=0, column=0)
        self.button(controls, "从结果目录继续", self.resume, row=0, column=1)
        self.stop_button = ttk.Button(controls, text="停止", command=self.stop, state="disabled")
        self.stop_button.grid(row=0, column=3, padx=4)
        self.status = tk.StringVar(value="待机：先检测终端串口，再开始测试")
        ttk.Label(controls, textvariable=self.status).grid(row=1, column=0, columnspan=4, sticky="w", pady=6)

        self.log = ScrolledText(root, wrap="word", height=12, state="disabled")
        self.log.grid(row=4, column=0, sticky="nsew", padx=10, pady=(0, 10))
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(100, self.poll)

    def entry(self, parent, label, key, value, row, column, span=1, **kwargs):
        ttk.Label(parent, text=label).grid(row=row, column=column, sticky="w", padx=4, pady=4)
        self.fields[key] = tk.StringVar(value=str(value))
        ttk.Entry(parent, textvariable=self.fields[key], **kwargs).grid(
            row=row, column=column+1, columnspan=span, sticky="ew", padx=4, pady=4)
        parent.columnconfigure(column+1, weight=1)

    def button(self, parent, text, command, **grid):
        button = ttk.Button(parent, text=text, command=command)
        button.grid(**grid, padx=4, pady=4)
        self.buttons.append(button)

    def append(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text.rstrip() + "\n")
        if int(self.log.index("end-1c").split(".")[0]) > 4000:
            self.log.delete("1.0", "1000.0")
        self.log.see("end")
        self.log.configure(state="disabled")

    def busy(self):
        return self.scanning or self.process is not None

    def set_buttons(self):
        for button in self.buttons:
            button.configure(state="disabled" if self.busy() else "normal")
        self.stop_button.configure(state="normal" if self.busy() else "disabled")

    def choose_binary(self):
        path = filedialog.askopenfilename(title="选择 RK3506 ARM 可执行文件")
        if path:
            self.fields["local_bin"].set(path)

    def scan(self):
        if self.busy():
            return
        try:
            baudrate = int(self.fields["baudrate"].get())
            timeout = float(self.fields["scan_timeout"].get())
            if not 1 <= baudrate <= 4000000 or not .2 <= timeout <= 30:
                raise ValueError("波特率需为1～4000000，探测等待需为0.2～30秒")
        except ValueError as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        self.scanning = True
        self.cancel_scan.clear()
        self.status.set("正在逐个串口发送 AT+RST，采集复位日志…")
        self.set_buttons()
        def work():
            try:
                matches, evidence = detect(baudrate, timeout,
                    log=lambda text: self.events.put(("log", text)), cancelled=self.cancel_scan.is_set)
                path = HERE / "results" / ("serial_probe_" + uuid.uuid4().hex[:8] + ".json")
                path.parent.mkdir(exist_ok=True)
                path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8")
                self.events.put(("scan_done", (matches, str(path), self.cancel_scan.is_set())))
            except Exception as exc:
                self.events.put(("scan_error", str(exc)))
        threading.Thread(target=work, daemon=True).start()

    def config(self):
        config = dict(self.defaults)
        if not config.get("known_hosts") and (HERE / "known_hosts.local").exists():
            config["known_hosts"] = str(HERE / "known_hosts.local")
        for key in ("host", "username", "serial_port", "local_bin"):
            config[key] = self.fields[key].get().strip()
        for key in ("ssh_port", "baudrate", "frequency_hz", "duration_ms", "receiver_duration_ms", "repeats", "tx_gain", "timer_wakeup_ms"):
            config[key] = int(self.fields[key].get())
        config["modes"] = parse_numbers(self.fields["modes"].get())
        config["ids"] = parse_numbers(self.fields["ids"].get())
        run.validate(config)
        config["local_bin"] = str(Path(config["local_bin"]).resolve())
        with Path(config["local_bin"]).open("rb") as file:
            if file.read(4) != b"\x7fELF":
                raise ValueError("请选择已编译的 ARM ELF 程序，不能选择 .c 文件")
        return config

    def start(self, resume=None):
        if self.busy():
            return
        try:
            config = self.config()
            settings = {key: config[key] for key in ("host", "ssh_port", "username", "serial_port", "baudrate")}
            (HERE / "settings.local.json").write_text(json.dumps(settings, indent=2), encoding="utf-8")
            if resume:
                saved = json.loads((Path(resume)/"config.json").read_text(encoding="utf-8"))
                if saved != config:
                    raise ValueError("界面配置与该结果目录不同，请先按原配置填写")
            runtime = HERE / "results" / "gui_sessions" / uuid.uuid4().hex
            runtime.mkdir(parents=True)
            config_path = runtime / "config.json"
            config_path.write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding="utf-8")
            self.stop_file = runtime / "stop.request"
            env = dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUNBUFFERED="1")
            env[config["password_env"]] = self.fields["password"].get()
            command = [sys.executable, "-u", str(HERE/"run.py"), "--config", str(config_path),
                       "--stop-file", str(self.stop_file)]
            if resume:
                command += ["--resume", str(resume)]
            self.process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", env=env,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            process = self.process
            self.status.set("测试运行中：自动定时恢复，每项最多3次，结果自动保存")
            self.set_buttons()
            def read():
                for line in process.stdout:
                    self.events.put(("output", line))
                code = process.wait()
                process.stdout.close()
                self.events.put(("finished", code))
            threading.Thread(target=read, daemon=True).start()
        except Exception as exc:
            messagebox.showerror("启动失败", str(exc))

    def resume(self):
        path = filedialog.askdirectory(title="选择包含 config.json 的原结果目录")
        if path:
            try:
                saved = json.loads((Path(path)/"config.json").read_text(encoding="utf-8"))
                run.validate(saved)
                self.defaults = saved
                for key in ("host", "ssh_port", "username", "serial_port", "baudrate", "local_bin",
                            "frequency_hz", "duration_ms", "receiver_duration_ms", "repeats"):
                    self.fields[key].set(str(saved[key]))
                self.fields["tx_gain"].set(str(saved["tx_gain"]))
                self.fields["timer_wakeup_ms"].set(str(saved["timer_wakeup_ms"]))
                for key in ("modes", "ids"):
                    self.fields[key].set(",".join(map(str, saved[key])))
                self.start(path)
            except Exception as exc:
                messagebox.showerror("恢复失败", str(exc))

    def stop(self):
        if self.scanning:
            self.cancel_scan.set()
        if self.process is not None:
            self.stop_file.touch()
        self.status.set("正在停止并保存，等待发射进程正常退出…")

    def poll(self):
        try:
            while True:
                kind, data = self.events.get_nowait()
                if kind in ("log", "output"):
                    self.append(data)
                elif kind in ("scan_done", "scan_error"):
                    self.scanning = False
                    if kind == "scan_error":
                        self.status.set("探测失败：" + data)
                    else:
                        matches, path, cancelled = data
                        self.port_box.configure(values=matches)
                        self.append("串口探测记录：" + path)
                        if cancelled:
                            self.status.set("串口检测已取消")
                        elif len(matches) == 1:
                            self.fields["serial_port"].set(matches[0])
                            self.status.set("已检测到终端：" + matches[0])
                        else:
                            self.fields["serial_port"].set("")
                            self.status.set("未找到终端" if not matches else "多个串口有日志，请从列表选择")
                    self.set_buttons()
                elif kind == "finished":
                    self.process = None
                    self.status.set({0: "测试完成", 1: "遍历完成，存在3次尝试均失败的项",
                                     2: "已停止，请查看日志和结果"}.get(data, f"进程异常退出：{data}"))
                    self.set_buttons()
        except queue.Empty:
            pass
        if self.closing and not self.busy():
            self.root.destroy()
            return
        self.root.after(100, self.poll)

    def close(self):
        if self.busy():
            self.closing = True
            self.stop()
        else:
            self.root.destroy()


if __name__ == "__main__":
    enable_dpi_awareness()
    root = tk.Tk()
    App(root)
    root.mainloop()
