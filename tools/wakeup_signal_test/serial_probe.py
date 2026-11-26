"""Discover serial terminals by sending AT+RST; no scan on import."""
import base64
import time


def has_reset_log(data):
    text = data.decode("utf-8", errors="replace")
    return any(any(char.isprintable() and not char.isspace() and char != "\ufffd"
                   for char in line) for line in text.replace("AT+RST", "").splitlines())


def detect(baudrate=115200, timeout=3, log=print, cancelled=lambda: False,
           ports=None, open_port=None):
    if ports is None or open_port is None:
        import serial
        from serial.tools import list_ports
        ports = list_ports.comports() if ports is None else ports
        open_port = serial.Serial if open_port is None else open_port
    matches, evidence = [], []
    for port in ports:
        if cancelled():
            break
        name = port.device
        raw = bytearray()
        record = {"port": name, "description": getattr(port, "description", "")}
        try:
            log(f"探测 {name}：发送 AT+RST，等待 {timeout:g} 秒复位日志")
            with open_port(name, baudrate, timeout=.1, write_timeout=2) as device:
                device.reset_input_buffer()
                device.write(b"AT+RST\r\n")
                device.flush()
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline and not cancelled():
                    raw.extend(device.read(4096))
                    if len(raw) > 1024 * 1024:
                        raise RuntimeError("复位日志超过1 MiB，请检查串口输出")
            if cancelled():
                break
            record.update(matched=has_reset_log(raw),
                          raw_base64=base64.b64encode(raw).decode("ascii"),
                          text=raw.decode("utf-8", errors="replace"))
            if record["matched"]:
                matches.append(name)
                log(f"{name} 有复位日志：\n{record['text']}")
            else:
                log(f"{name} 未检测到复位日志")
        except Exception as exc:
            record.update(matched=False, error=str(exc))
            log(f"{name} 探测失败：{exc}")
        evidence.append(record)
    return matches, evidence
