"""SSH transmitter + TK8620 serial wakeup campaign. No hardware access on import."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import sys
import threading
import time
import uuid
from datetime import datetime

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bcn_frequency_test.serial_device import Journal, Terminal

STOP_FILE = None


def check_cancel():
    if STOP_FILE is not None and STOP_FILE.exists():
        raise KeyboardInterrupt("用户点击停止")


class WakeTerminal(Terminal):
    def awake(self, timeout):
        """Require both a FREQ payload and command completion after this query."""
        self.check()
        with self.condition:
            cursor = self.sequence
        self.journal.emit("command", text="AT+FREQ?")
        self.device.write(b"AT+FREQ?\r\n")
        self.device.flush()
        try:
            lines = self.wait(cursor, lambda line: line.strip() == "AT_OK" or
                              "ERROR" in line, timeout)
        except TimeoutError:
            return False
        return any(re.fullmatch(r"\+FREQ:\s*\d+(?:\s*,\s*\d+)*\s*", line.strip())
                   for line in lines) and lines[-1].strip() == "AT_OK"


def validate(config):
    for key in ("host", "serial_port", "local_bin"):
        if not isinstance(config.get(key), str) or not config[key]:
            raise ValueError(f"需要配置 {key}")
    for key in ("username", "password_env"):
        if not isinstance(config.get(key), str) or not config[key]:
            raise ValueError(f"需要配置 {key}")
    for key, low, high in (("frequency_hz", 1, 0xffffffff), ("duration_ms", 1, 0xffffff),
                           ("receiver_duration_ms", 1, 0xffffff), ("repeats", 1, 10000),
                           ("baudrate", 1, 4000000), ("rate_mode", 5, 18),
                           ("tx_gain", 0, 255), ("ssh_port", 1, 65535),
                           ("wakeup_cfg_first", 0, 0xffffffff)):
        value = config.get(key)
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f"{key} 必须为 {low}~{high} 的整数")
    if config["rate_mode"] not in (5, 6, 7, 8, 9, 10, 11, 18):
        raise ValueError("不支持的 rate_mode")
    for key, low, high in (("modes", 1, 3), ("ids", 0, 255)):
        values = config.get(key)
        if not isinstance(values, list) or not values or any(
                type(v) is not int or not low <= v <= high for v in values):
            raise ValueError(f"{key} 必须为 {low}~{high} 的非空整数列表")
        if len(set(values)) != len(values):
            raise ValueError(f"{key} 不允许重复，重复测量请使用 repeats")
    for key in ("serial_timeout", "poll_interval", "sleep_settle", "wake_grace",
                "startup_timeout", "stop_timeout"):
        if type(config.get(key)) not in (int, float) or not 0 < config[key] <= 300:
            raise ValueError(f"{key} 必须大于0且不超过300秒")
    if type(config.get("timer_wakeup_ms")) is not int or not 1000 <= config["timer_wakeup_ms"] <= 0xffffff:
        raise ValueError("timer_wakeup_ms 必须为1000～16777215毫秒；旧配置请补充定时唤醒周期")
    if type(config.get("timer_guard_ms")) is not int or not 0 <= config["timer_guard_ms"] < config["timer_wakeup_ms"]:
        raise ValueError("timer_guard_ms 必须非负且小于定时唤醒周期")
    if type(config.get("recovery_timeout")) not in (int, float) or not 0 < config["recovery_timeout"] <= 300:
        raise ValueError("recovery_timeout 必须大于0且不超过300秒")
    window = (config["timer_wakeup_ms"] - config["timer_guard_ms"]) / 1000
    if window <= config["sleep_settle"] + 2 * min(.2, config["serial_timeout"]) + .5:
        raise ValueError("定时唤醒周期过短，无法完成休眠确认和无线测试；请增大周期或减少等待/保护时间")


def cases(config):
    for mode in config["modes"]:
        for wid in config["ids"]:
            for repeat in range(1, config["repeats"] + 1):
                yield {"case": f"m{mode}_id{wid}_r{repeat}", "mode": mode,
                       "id": wid, "repeat": repeat}


def wait_terminal_ready(terminal, config, deadline=None):
    if deadline is None:
        deadline = time.monotonic() + config["timer_wakeup_ms"] / 1000 + config["recovery_timeout"]
    while time.monotonic() < deadline:
        check_cancel()
        if terminal.awake(min(config["serial_timeout"], max(.01, deadline-time.monotonic()))):
            terminal.journal.emit("terminal_ready")
            return
        time.sleep(min(config["poll_interval"], max(0, deadline-time.monotonic())))
    raise RuntimeError("定时唤醒等待结束后终端仍无有效AT响应，已保存并停止；请检查终端/串口/固件")


def prepare_sleep(terminal, config, case):
    terminal.command("AT+WORKMODE=21")
    terminal.command(f"AT+WAKEUPCFG=8,{config['timer_wakeup_ms']};{config['wakeup_cfg_first']},{case['id']},"
                     f"{config['receiver_duration_ms']},{config['frequency_hz']},{case['mode']}")
    if not terminal.awake(config["serial_timeout"]):
        raise RuntimeError("休眠前 AT+FREQ? 无有效响应，不能开始测试")
    # Anchor before sending ENTERSLEEP: conservatively include ACK latency.
    sleep_started = time.monotonic()
    terminal.command("AT+ENTERSLEEP")
    time.sleep(config["sleep_settle"])
    # Two negative probes distinguish a continuously responsive terminal from sleep.
    for _ in range(2):
        if terminal.awake(min(.2, config["serial_timeout"])):
            raise RuntimeError("AT+ENTERSLEEP 后仍能查询，不能判为有效休眠测试")
    return sleep_started


class Transmitter:
    def __init__(self, client, remote_dir, config, journal):
        self.client, self.remote_dir, self.config, self.journal = client, remote_dir, config, journal
        self.channel = None
        self.thread = None
        self.condition = threading.Condition()
        self.buffer = ""
        self.base = 0
        self.closed = False
        self.error = None
        self.transmitting = False

    def _read(self):
        import codecs
        decoder = codecs.getincrementaldecoder("utf-8")("replace")
        try:
            while True:
                data = self.channel.recv(16384)
                if not data:
                    break
                text = decoder.decode(data)
                self.journal.emit("ssh_output", text=text)
                with self.condition:
                    self.buffer += text
                    if len(self.buffer) > 1024 * 1024:
                        remove = len(self.buffer) - 1024 * 1024
                        self.base += remove
                        self.buffer = self.buffer[remove:]
                    self.condition.notify_all()
        except Exception as exc:
            self.error = exc
        finally:
            with self.condition:
                self.closed = True
                self.condition.notify_all()

    def mark(self):
        with self.condition:
            return self.base + len(self.buffer)

    def wait(self, marker, start, timeout):
        deadline = time.monotonic() + timeout
        with self.condition:
            while True:
                check_cancel()
                if start < self.base:
                    raise RuntimeError("SSH 等待窗口超出缓存，请检查日志")
                text = self.buffer[start - self.base:]
                if marker in text:
                    return text
                if self.error or self.closed:
                    raise RuntimeError("SSH 进程意外结束") from self.error
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(f"SSH 等待 {marker!r} 超时")
                self.condition.wait(min(remaining, .2))

    def start(self):
        args = [self.remote_dir + "/Test8710RFTxTone", str(self.config["rate_mode"]),
                str(self.config["frequency_hz"]), str(self.config["tx_gain"])]
        command = "cd " + shlex.quote(self.remote_dir) + " && exec " + shlex.join(args)
        self.journal.emit("ssh_start", command=command)
        self.channel = self.client.get_transport().open_session(timeout=15)
        self.channel.get_pty()
        self.channel.exec_command(command)
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()
        self.wait("TK8710> ", 0, self.config["startup_timeout"])
        self.wait("WAKEUP_SESSION_V2:", 0, 1)

    def send(self, case):
        if self.transmitting:
            raise RuntimeError("上一项发射尚未停止")
        start = self.mark()
        self.channel.sendall(b"u\n")
        self.wait("Enter wakeup mode", start, 5)
        start = self.mark()
        self.completion_start = start
        self.transmitting = True
        self.channel.sendall(f"{case['mode']} {self.config['duration_ms']} {case['id']}\n".encode())
        started = time.monotonic()
        self.wait("WAKEUP_STARTED ", start, 10)
        self.journal.emit("tx_configured", **case)
        return started

    def _completion(self, require_done):
        if not self.transmitting:
            return
        text = self.wait("WAKEUP_FINISHED ", self.completion_start, self.config["stop_timeout"])
        offset = self.completion_start + text.index("WAKEUP_FINISHED ")
        text = self.wait("\n", offset, self.config["stop_timeout"])
        match = re.search(r"WAKEUP_FINISHED state=(\d+) result=(-?\d+) stopped=(\d+) elapsed_us=(\d+)", text)
        if not match or int(match[2]) != 0 or int(match[3]) != 1 or int(match[1]) not in (3, 4):
            raise RuntimeError(f"Driver唤醒停止失败: {text}")
        if require_done and int(match[1]) != 3:
            raise RuntimeError(f"Driver唤醒未正常完成: {text}")
        self.transmitting = False
        self.journal.emit("tx_stopped", state=int(match[1]), elapsed_us=int(match[4]))

    def finish_signal(self):
        """Driver stops on the first S0 at/after the deadline; never send p here."""
        self._completion(require_done=True)

    def stop_signal(self):
        if self.transmitting:
            self.channel.sendall(b"p\n")
            self._completion(require_done=False)

    def stop(self):
        if self.channel is None:
            return
        try:
            if not self.channel.exit_status_ready():
                self.channel.sendall(b"q\n")
                deadline = time.monotonic() + self.config["stop_timeout"]
                while not self.channel.exit_status_ready() and time.monotonic() < deadline:
                    time.sleep(.1)
            if not self.channel.exit_status_ready():
                raise RuntimeError("发射进程未正常退出，停止整个测试，请人工确认发射已停止")
            code = self.channel.recv_exit_status()
            if code != 0:
                raise RuntimeError(f"发射进程退出码 {code}")
            self.journal.emit("ssh_exit", code=code)
        finally:
            self.channel.close()
            if self.thread:
                self.thread.join(timeout=3)
            self.channel = None


def run_attempt(tx, terminal, config, case):
    sleep_started = prepare_sleep(terminal, config, case)
    timer_deadline = sleep_started + config["timer_wakeup_ms"] / 1000
    radio_deadline = timer_deadline - config["timer_guard_ms"] / 1000
    if time.monotonic() >= radio_deadline:
        raise RuntimeError("休眠确认耗尽无线判定窗口，未启动发射；请增大定时周期")
    started = tx.send(case)
    send_end = started + config["duration_ms"] / 1000
    deadline = min(radio_deadline, send_end + config["wake_grace"])
    latency = None
    while time.monotonic() < deadline:
        check_cancel()
        # A query that starts before but completes at/after the cutoff is NOT PASS.
        response = terminal.awake(min(config["serial_timeout"], max(.01, deadline-time.monotonic())))
        observed = time.monotonic()
        if response and observed < deadline:
            latency = observed - started
            break
        if observed >= send_end:
            tx.finish_signal()
        time.sleep(min(config["poll_interval"], max(0, deadline-time.monotonic())))
    if latency is not None:
        while time.monotonic() < send_end:
            check_cancel()
            terminal.check()
            time.sleep(min(.2, send_end-time.monotonic()))
        tx.finish_signal()
    else:
        # Finish/abort TX before waiting for the timer or sending any next config.
        if time.monotonic() < send_end:
            tx.stop_signal()
        else:
            tx.finish_signal()
    wait_terminal_ready(terminal, config,
                        max(timer_deadline, time.monotonic()) + config["recovery_timeout"])
    return {**case, "status": "PASS" if latency is not None else "WAKE_TIMEOUT",
            "query_latency_s": latency, "timer_wakeup_ms": config["timer_wakeup_ms"],
            "reason": "response_before_timer_guard" if latency is not None else
                      "no_wireless_response_before_timer_guard",
            "sleep_started_monotonic": sleep_started, "radio_deadline_monotonic": radio_deadline}


def run_case(tx, terminal, config, case):
    attempts = []
    for attempt in range(1, 4):
        wait_terminal_ready(terminal, config)
        print(f"{case['case']}: 尝试 {attempt}/3，定时唤醒 {config['timer_wakeup_ms']} ms", flush=True)
        tx.journal.emit("attempt_start", **case, attempt=attempt)
        row = run_attempt(tx, terminal, config, {**case, "attempt": attempt})
        attempts.append(row)
        tx.journal.emit("attempt_result", **row)
        print(f"{case['case']}: 尝试 {attempt}/3 -> {row['status']}", flush=True)
        if row["status"] == "PASS":
            break
    return {**case, "status": attempts[-1]["status"],
            "query_latency_s": attempts[-1]["query_latency_s"],
            "timer_wakeup_ms": config["timer_wakeup_ms"],
            "attempt_count": len(attempts), "attempts": attempts}



def remote_exec(client, command):
    _, out, err = client.exec_command(command, timeout=15)
    text, errors = out.read().decode(), err.read().decode()
    if out.channel.recv_exit_status() != 0:
        raise RuntimeError(errors or text or "远端命令失败")
    return text


def connect(config, output, journal):
    import paramiko
    from ssh_hosts import configure, fingerprint
    binary = Path(config["local_bin"]).resolve()
    if binary.read_bytes()[:4] != b"\x7fELF":
        raise ValueError("local_bin 需要是编译好的 RK3506 ELF 程序，不能是 .c")
    client = paramiko.SSHClient()
    stage = "SSH连接"
    def progress(name):
        nonlocal stage
        stage = name
        journal.emit("prepare_stage", stage=stage, host=config["host"], port=config["ssh_port"])
        print(f"准备：{stage} [{config['host']}:{config['ssh_port']}]", flush=True)
    try:
        progress("SSH连接")
        host_file = config.get("known_hosts") or Path(__file__).with_name("known_hosts.local")
        configure(client, host_file, journal, log=lambda text: print(text, flush=True))
        try:
            client.connect(config["host"], port=config["ssh_port"], username=config["username"],
                           password=os.getenv(config["password_env"]), timeout=15,
                           auth_timeout=15, banner_timeout=15)
        except paramiko.BadHostKeyException as exc:
            journal.emit("ssh_host_key_changed", host=config["host"], port=config["ssh_port"],
                         expected=fingerprint(exc.expected_key), received=fingerprint(exc.key))
            raise RuntimeError(f"SSH主机指纹变化：{config['host']}:{config['ssh_port']}，"
                               f"已登记 {fingerprint(exc.expected_key)}，"
                               f"当前 {fingerprint(exc.key)}。请确认是否更换/重刷网关；"
                               "工具未覆盖原记录。") from exc
        check_cancel()
        progress("检查网关硬件占用")
        remote_exec(client, "for n in Test8710RFTxTone Test8710RFTest Test8710Wakeup "
                    "TestTRMmain tk8710_gw tk8710_gw_slave tk8710_gw_freq_test; do "
                    "if pidof \"$n\" >/dev/null; then echo \"Hardware busy: $n\" >&2; "
                    "exit 1; fi; done")
        directory = "/userdata/wakeup_test_" + uuid.uuid4().hex
        progress("创建运行目录及复制校准文件")
        remote_exec(client, "mkdir " + shlex.quote(directory))
        # Driver resolves TxDC/txadc.txt from cwd. Preserve this board's own
        # calibration when running from an isolated directory.
        txadc_source = config.get("remote_txadc", "/userdata/TxDC/txadc.txt")
        if not isinstance(txadc_source, str) or not txadc_source.startswith("/"):
            raise ValueError("remote_txadc 必须是板端校准文件的绝对路径")
        remote_exec(client, "test -r " + shlex.quote(txadc_source)
                    + " && mkdir " + shlex.quote(directory + "/TxDC")
                    + " && cp " + shlex.quote(txadc_source) + " "
                    + shlex.quote(directory + "/TxDC/txadc.txt"))
        txadc_hash = remote_exec(client, "sha256sum "
                                + shlex.quote(directory + "/TxDC/txadc.txt")).split()[0]
        journal.emit("calibration_copied", source=txadc_source,
                     remote=directory + "/TxDC/txadc.txt", sha256=txadc_hash)
        progress("SFTP上传测试程序")
        with client.open_sftp() as sftp:
            sftp.put(str(binary), directory + "/Test8710RFTxTone")
            sftp.chmod(directory + "/Test8710RFTxTone", 0o755)
        digest = hashlib.sha256(binary.read_bytes()).hexdigest()
        progress("核对远端程序SHA256")
        actual = remote_exec(client, "sha256sum " + shlex.quote(directory + "/Test8710RFTxTone"))
        if actual.split()[0] != digest:
            raise RuntimeError("远端二进制 SHA256 校验失败")
        journal.emit("binary_uploaded", local=str(binary), remote_dir=directory, sha256=digest)
        print(f"上传：{directory}/Test8710RFTxTone\nSHA256：{digest}")
        return client, directory
    except BaseException as exc:
        client.close()
        journal.emit("prepare_failed", stage=stage, host=config["host"], port=config["ssh_port"],
                     exception=type(exc).__name__, error=str(exc))
        if isinstance(exc, TimeoutError):
            raise RuntimeError(f"{stage}超时：{config['host']}:{config['ssh_port']}。"
                               "请检查界面SSH地址、网关IP/电源、网络连接及SSH服务。") from exc
        raise


def download_logs(client, remote, output):
    with client.open_sftp() as sftp:
        def walk(source, target):
            import stat
            target.mkdir(parents=True, exist_ok=True)
            for item in sftp.listdir_attr(source):
                if item.filename in (".", "..") or "/" in item.filename or "\\" in item.filename:
                    raise RuntimeError("无效远端文件名")
                if stat.S_ISDIR(item.st_mode):
                    walk(source + "/" + item.filename, target / item.filename)
                elif stat.S_ISREG(item.st_mode) and item.filename != "Test8710RFTxTone":
                    sftp.get(source + "/" + item.filename, str(target / item.filename))
        walk(remote, output / "remote_logs" / remote.rsplit("/", 1)[-1])


def main(argv=None):
    global STOP_FILE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--plan", action="store_true", help="校验并打印计划，不连接硬件")
    parser.add_argument("--resume", type=Path, help="原结果目录，跳过已完成的 PASS/WAKE_TIMEOUT 项")
    parser.add_argument("--stop-file", type=Path, help="GUI停止请求文件")
    args = parser.parse_args(argv)
    STOP_FILE = args.stop_file
    config = json.loads(args.config.read_text(encoding="utf-8-sig"))
    if config.get("ids") == "all":
        config["ids"] = list(range(256))
    validate(config)
    binary = Path(config["local_bin"])
    if not binary.is_absolute():
        config["local_bin"] = str((args.config.resolve().parent / binary).resolve())
    plan = list(cases(config))
    if args.plan:
        preview = {**config, "ids": {"count": len(config["ids"]),
                                     "first": config["ids"][0], "last": config["ids"][-1]}}
        print(json.dumps({"count": len(plan), "first": plan[0], "last": plan[-1],
                          "config": preview}, ensure_ascii=False, indent=2))
        return 0
    output = args.resume or Path(__file__).parent / "results" / (
        datetime.now().strftime("%Y%m%d_%H%M%S_") + uuid.uuid4().hex[:8])
    if not args.resume:
        output.mkdir(parents=True, exist_ok=False)
        (output / "config.json").write_text(json.dumps(config, indent=2), encoding="utf-8")
    elif json.loads((output / "config.json").read_text(encoding="utf-8")) != config:
        raise ValueError("恢复运行的配置与原运行不一致")
    done = set()
    results = output / "results.jsonl"
    if results.exists():
        for line in results.read_text(encoding="utf-8").splitlines():
            row = json.loads(line)
            if row["status"] in ("PASS", "WAKE_TIMEOUT"):
                done.add(row["case"])
    journal = Journal(output / ("journal_" + uuid.uuid4().hex[:8] + ".jsonl"))
    client = terminal = tx = None
    remote = None
    failed = False
    try:
        client, remote = connect(config, output, journal)
        port = config["serial_port"]
        if port == "auto":
            from serial_probe import detect
            matches, evidence = detect(config["baudrate"], log=lambda text: print(text, flush=True),
                cancelled=lambda: STOP_FILE is not None and STOP_FILE.exists())
            journal.emit("serial_probe", evidence=evidence)
            check_cancel()
            if len(matches) != 1:
                raise RuntimeError(f"自动检测得到 {matches}，请在界面选择唯一终端串口")
            port = matches[0]
        journal.emit("terminal_selected", port=port, baudrate=config["baudrate"])
        terminal = WakeTerminal(port, config["baudrate"], journal)
        for case in plan:
            check_cancel()
            if case["case"] in done:
                continue
            print(f"开始 {case['case']}")
            journal.emit("case_start", **case)
            row = {**case, "status": "ERROR"}
            try:
                if tx is None:
                    tx = Transmitter(client, remote, config, journal)
                    tx.start()
                row = run_case(tx, terminal, config, case)
            except BaseException as exc:
                row["error"] = str(exc) or type(exc).__name__
                raise
            finally:
                try:
                    if tx is not None:
                        tx.stop_signal()
                except Exception as exc:
                    row.update(status="ERROR", error=str(exc))
                    raise
                finally:
                    row["time"] = datetime.now().astimezone().isoformat()
                    with results.open("a", encoding="utf-8") as file:
                        file.write(json.dumps(row, ensure_ascii=False) + "\n")
                    journal.emit("case_result", **row)
                    print(f"{case['case']}: {row['status']}")
    except (Exception, KeyboardInterrupt) as exc:
        failed = True
        journal.emit("run_stopped", reason=str(exc) or "KeyboardInterrupt")
        print(f"已停止：{exc}\n恢复命令：python {Path(__file__).name} "
              f"--config {args.config} --resume {output}", file=sys.stderr)
    finally:
        for action in ([lambda: tx.stop()] if tx is not None else []) + (
                [lambda: terminal.close()] if terminal else []) + (
                [lambda: download_logs(client, remote, output)] if client and remote else []):
            try:
                action()
            except Exception as exc:
                failed = True
                journal.emit("cleanup_error", error=str(exc))
                print(f"收尾失败：{exc}", file=sys.stderr)
        if client:
            client.close()
        journal.close()
        STOP_FILE = None
        rows = [json.loads(line) for line in results.read_text(encoding="utf-8").splitlines()] if results.exists() else []
        with (output / "results.csv").open("w", encoding="utf-8-sig", newline="") as file:
            writer = csv.DictWriter(file, ["case", "mode", "id", "repeat", "status",
                                           "query_latency_s", "attempt_count", "timer_wakeup_ms", "error", "time"],
                                    extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)
        attempt_rows = []
        for path in sorted(output.glob("journal_*.jsonl"), key=lambda p: p.stat().st_mtime):
            for line in path.read_text(encoding="utf-8").splitlines():
                record = json.loads(line)
                if record["kind"] == "attempt_result":
                    attempt_rows.append(record)
        with (output / "attempts.csv").open("w", encoding="utf-8-sig", newline="") as file:
            writer = csv.DictWriter(file, ["case", "mode", "id", "repeat", "attempt", "status",
                                           "query_latency_s", "timer_wakeup_ms", "reason", "utc"],
                                    extrasaction="ignore")
            writer.writeheader()
            writer.writerows(attempt_rows)
        latest = {row["case"]: row for row in rows}
        summary = {"planned": len(plan), "recorded": len(latest), "output": str(output),
                   "counts": {status: sum(r["status"] == status for r in latest.values())
                              for status in ("PASS", "WAKE_TIMEOUT", "ERROR")}, "stopped": failed, "attempts": sum(r.get("attempt_count", 1) for r in latest.values())}
        (output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print(json.dumps(summary, ensure_ascii=False))
    return 2 if failed else (1 if summary["counts"]["WAKE_TIMEOUT"] else 0)


if __name__ == "__main__":
    raise SystemExit(main())
