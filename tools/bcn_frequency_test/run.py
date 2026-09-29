"""One-command terminal capture and gateway-B frequency sweep orchestration."""
from __future__ import annotations

import argparse
import codecs
import getpass
import hashlib
import json
import shlex
import stat
import sys
import time
import uuid
from datetime import datetime
from pathlib import Path

from analyze import analyze_records, load_records, write_report
from protocol import Config, parse_rx
from serial_device import Journal, Terminal, discover

ROOT = Path(__file__).resolve().parents[2]


def ask(label, default, convert=str):
    while True:
        value = input(f"{label} [{default}]: ").strip() or str(default)
        try:
            return convert(value)
        except ValueError:
            print("输入格式错误，请重新输入。")


def configuration(args):
    if args.config:
        config = Config(**json.loads(args.config.read_text(encoding="utf-8-sig")))
    else:
        config = Config()
        for field, label in (("host", "网关 B IP"), ("primary_channel", "终端主信道号"),
                             ("secondary_channel", "终端次信道号"), ("rate", "测试速率"),
                             ("base_hz", "网关 A 固定中心频率 Hz"),
                             ("bits_a", "网关 A bcnbits"), ("bits_b", "网关 B bcnbits"),
                             ("tdd_count", "NS 配置的 TDD 数量"),
                             ("ul_blocks", "NS 上行包块数"), ("dl_blocks", "NS 下行包块数"),
                             ("start_hz", "起始频差 Hz"), ("end_hz", "结束频差 Hz"),
                             ("step_hz", "频差步进 Hz"), ("hold_seconds", "每点有效运行秒数"),
                             ("baudrate", "终端串口波特率")):
            setattr(config, field, ask(label, getattr(config, field), str if field == "host" else int))
    if args.serial_port:
        config.serial_port = args.serial_port
    if args.binary:
        config.binary = str(args.binary.resolve())
    return config.validate()


def remote_exec(client, command, timeout=20):
    stdin, stdout, stderr = client.exec_command(command, timeout=timeout)
    try:
        stdin.close()
        text = stdout.read().decode("utf-8", errors="replace")
        error = stderr.read().decode("utf-8", errors="replace")
        code = stdout.channel.recv_exit_status()
        if code:
            raise RuntimeError(f"网关命令失败 ({code}): {error or text}")
        return text
    finally:
        stdout.close()
        stderr.close()


def download_tree(sftp, remote, local):
    local.mkdir(parents=True, exist_ok=True)
    for entry in sftp.listdir_attr(remote):
        if entry.filename in (".", "..") or "/" in entry.filename or "\\" in entry.filename:
            continue
        child = remote+"/"+entry.filename
        destination = local/entry.filename
        if stat.S_ISDIR(entry.st_mode):
            download_tree(sftp, child, destination)
        elif stat.S_ISREG(entry.st_mode):
            sftp.get(child, str(destination))
            if destination.stat().st_size != entry.st_size:
                raise RuntimeError(f"下载长度不一致: {child}")


def preflight(client):
    # Only inspect; never kill another test or change a system service implicitly.
    remote_exec(client, "for name in tk8710_gw tk8710_gw_freq_test TestTRMmain tk8710_gw_slave; do "
                "if pidof \"$name\" >/dev/null 2>&1; then "
                "echo \"Hardware in use: $name. Stop its supervisor before testing.\" >&2; exit 1; fi; done")


def upload(client, config, directory):
    binary = Path(config.binary)
    if not binary.is_absolute():
        binary = ROOT/binary
    if not binary.is_file():
        raise FileNotFoundError(f"未找到测试程序: {binary}")
    remote_exec(client, "mkdir -p "+shlex.quote(directory+"/lib"))
    with client.open_sftp() as sftp:
        sftp.put(str(binary), directory+"/tk8710_gw_freq_test")
        sftp.chmod(directory+"/tk8710_gw_freq_test", 0o755)
        # Match the binary's IPC ABI; leave other dependencies to the board SDK image.
        library = ROOT/"lib/libipc_smp.so"
        sftp.put(str(library), directory+"/lib/libipc_smp.so")
    remote_exec(client, "if [ -f /userdata/TxDC/txadc.txt ]; then mkdir -p "+
                shlex.quote(directory+"/TxDC")+" && cp /userdata/TxDC/txadc.txt "+
                shlex.quote(directory+"/TxDC/txadc.txt")+"; fi")
    return hashlib.sha256(binary.read_bytes()).hexdigest()


def monitor(channel, terminal, journal, gateway_log, config):
    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    pending = ""
    done = False
    completed = []
    active_index = None
    started = time.monotonic()
    deadline = started+len(config.offsets())*(config.ready_timeout+config.hold_seconds+30)+30
    try:
        while True:
            terminal.check()
            if time.monotonic() > deadline:
                raise TimeoutError("网关测试超出总时间上限")
            if channel.recv_ready():
                data = channel.recv(65536)
                decoded = decoder.decode(data)
                gateway_log.write(decoded)
                gateway_log.flush()
                pending += decoded
                while "\n" in pending:
                    line, pending = pending.split("\n", 1)
                    if line.startswith("FREQ_EVENT "):
                        event = json.loads(line[len("FREQ_EVENT "):])
                        journal.emit("gateway_event", **event)
                        if event["event"] in ("POINT_START", "POINT_END"):
                            index = event["index"]
                            offsets = config.offsets()
                            if (type(index) is not int or not 0 <= index < len(offsets) or
                                event["offset_hz"] != offsets[index] or event["status"] != 0):
                                raise RuntimeError(f"网关频点事件与计划不符: {event}")
                        if event["event"] == "POINT_CONFIG":
                            print(f"频差 {event['offset_hz']:+d} Hz：正在配置并等待 GPS/广播就绪")
                        elif event["event"] == "POINT_RETRY":
                            print(f"频差 {event['offset_hz']:+d} Hz：首帧 PPS 校验未通过，启动重试 {event['status']}")
                        elif event["event"] == "POINT_START":
                            if active_index is not None or event["index"] != len(completed):
                                raise RuntimeError("网关频点开始事件顺序错误")
                            active_index = event["index"]
                            print(f"频差 {event['offset_hz']:+d} Hz：GPS/广播就绪，开始计时")
                        elif event["event"] == "POINT_END":
                            if active_index != event["index"]:
                                raise RuntimeError("网关频点结束事件没有对应开始事件")
                            if event["elapsed_ms"] < config.hold_seconds*1000:
                                raise RuntimeError("网关有效运行时长不足")
                            completed.append(event["index"])
                            active_index = None
                            print(f"频差 {event['offset_hz']:+d} Hz：完成 {event['elapsed_ms']/1000:.3f} 秒")
                        elif event["event"] in ("POINT_FAILED", "SWEEP_FAILED"):
                            raise RuntimeError(f"网关测试失败: {event}")
                        elif event["event"] == "SWEEP_DONE":
                            done = event.get("status") == 0
                if len(pending) > 1048576:
                    raise RuntimeError("网关输出连续 1 MiB 无换行")
            elif channel.exit_status_ready():
                break
            else:
                time.sleep(.02)
        code = channel.recv_exit_status()
        if code or not done or completed != list(range(len(config.offsets()))):
            raise RuntimeError(f"测试未完整完成: exit={code}, done={done}, completed={completed}")
    finally:
        tail = decoder.decode(b"", final=True)
        if tail:
            gateway_log.write(tail)


def stop_remote(client, channel, directory, journal):
    if channel is None:
        return
    if not channel.exit_status_ready():
        try:
            channel.sendall(b"STOP\n")
        except Exception as exc:
            journal.emit("cleanup_warning", text=f"发送停止命令失败: {exc}")
        # Address only the exact executable uploaded for this run, using its /proc exe link.
        # SIGTERM also interrupts GPS startup, which may not be reading stdin yet.
        executable = shlex.quote(directory+"/tk8710_gw_freq_test")
        command = ("for proc in /proc/[0-9]*; do if [ \"$(readlink \"$proc/exe\")\" = "+executable+
                   " ]; then kill -TERM \"${proc##*/}\"; fi; done")
        remote_exec(client, command)
        deadline = time.monotonic()+15
        while not channel.exit_status_ready() and time.monotonic() < deadline:
            if channel.recv_ready():
                journal.emit("gateway_cleanup_output", text=channel.recv(65536).decode("utf-8", errors="replace"))
            time.sleep(.1)
        if not channel.exit_status_ready():
            raise RuntimeError("网关测试进程尚未确认退出；请查看远端进程，未发送 SIGKILL")
    channel.close()


def run(config, output, password=None):
    import paramiko
    output.mkdir(parents=True, exist_ok=False)
    (output/"config.json").write_text(json.dumps(config.as_dict(), ensure_ascii=False, indent=2), encoding="utf-8")
    journal = Journal(output/"journal.jsonl")
    client = paramiko.SSHClient()
    terminal = channel = None
    remote_dir = "/userdata/bcn_freq_test/"+datetime.now().strftime("%Y%m%d_%H%M%S_")+uuid.uuid4().hex[:12]
    remote_created = False
    status = {"completed": False, "remote_dir": remote_dir}
    try:
        client.load_system_host_keys()
        if config.known_hosts:
            client.load_host_keys(config.known_hosts)
        client.set_missing_host_key_policy(paramiko.RejectPolicy())
        client.connect(config.host, port=config.ssh_port, username=config.username,
                       password=password, timeout=15, auth_timeout=15, banner_timeout=15)
        preflight(client)
        status["binary_sha256"] = upload(client, config, remote_dir)
        remote_created = True
        port = discover(config.baudrate) if config.serial_port == "auto" else config.serial_port
        journal.emit("terminal_selected", port=port, baudrate=config.baudrate)
        print(f"终端串口：{port}，开始配置并入网")
        terminal = Terminal(port, config.baudrate, journal)
        terminal.configure_and_join(config)
        baseline_start = journal.emit("baseline_start", bits=config.bits_a)
        deadline = time.monotonic()+config.baseline_seconds
        while time.monotonic() < deadline:
            terminal.check()
            time.sleep(.05)
        baseline_end = journal.emit("baseline_end", bits=config.bits_a)
        with terminal.condition:
            baseline_rows = [parse_rx(record["text"]) for _, record in terminal.records
                             if baseline_start["time_ms"] <= record["time_ms"] <= baseline_end["time_ms"]]
        baseline_rows = [row for row in baseline_rows if row]
        if not baseline_rows or any(row["bcnbits"] != config.bits_a for row in baseline_rows):
            raise RuntimeError("网关 A 单独运行基线未收到广播或 bcnbits 不符；检查 A 配置与终端接收")
        # Recheck because a service supervisor may have restarted the ordinary gateway.
        preflight(client)
        arguments = [remote_dir+"/tk8710_gw_freq_test", *config.gateway_args(),
                     "--work-dir", remote_dir, "--require-controller"]
        command = ("cd "+shlex.quote(remote_dir)+" && export LD_LIBRARY_PATH="+
                   shlex.quote(remote_dir+"/lib:/userdata/lib")+" && exec "+shlex.join(arguments))
        channel = client.get_transport().open_session(timeout=15)
        channel.set_combine_stderr(True)
        channel.exec_command(command)
        with (output/"gateway.log").open("w", encoding="utf-8") as gateway_log:
            monitor(channel, terminal, journal, gateway_log, config)
        status["completed"] = True
    except BaseException as exc:
        status["error"] = f"{type(exc).__name__}: {exc}"
        journal.emit("run_error", text=status["error"])
    finally:
        try:
            stop_remote(client, channel, remote_dir, journal)
        except Exception as exc:
            status["cleanup_error"] = str(exc)
            status["completed"] = False
        if terminal is not None:
            try:
                terminal.close()
            except Exception as exc:
                status["terminal_cleanup_error"] = str(exc)
                status["completed"] = False
        if remote_created:
            try:
                with client.open_sftp() as sftp:
                    download_tree(sftp, remote_dir, output/"remote")
            except Exception as exc:
                status["download_error"] = str(exc)
                status["completed"] = False
        client.close()
        journal.close()
        (output/"run_status.json").write_text(json.dumps(status, ensure_ascii=False, indent=2), encoding="utf-8")
        result = analyze_records(list(load_records(output/"journal.jsonl")), config.tdd_count,
                                 config.bits_a, config.bits_b, config.boundary_guard_ms, config.frame_period_ms)
        result["summary"]["run_status"] = status
        print("分析报告：", write_report(result, output/"analysis"))
    if not status["completed"]:
        raise RuntimeError(json.dumps(status, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--serial-port")
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--password", action="store_true", help="交互输入 SSH 密码，不写入文件")
    parser.add_argument("--dry-run", action="store_true", help="只检查并显示计划，不连接设备")
    args = parser.parse_args()
    config = configuration(args)
    if args.dry_run:
        print(json.dumps(dict(config=config.as_dict(), offsets=config.offsets(), args=config.gateway_args()), ensure_ascii=False, indent=2))
        return
    password = getpass.getpass("SSH 密码：") if args.password else None
    output = args.output or ROOT/"remote_logs/bcn_frequency"/datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    run(config, output, password)


if __name__ == "__main__":
    try:
        main()
    except (Exception, KeyboardInterrupt) as exc:
        print(f"测试未完成：{exc}", file=sys.stderr)
        sys.exit(1)
