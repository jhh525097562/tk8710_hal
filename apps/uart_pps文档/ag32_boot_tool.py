#!/usr/bin/env python3
import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

try:
    import serial
except ImportError as exc:
    print("ERROR: missing dependency 'pyserial'. Install with: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2) from exc


BOOT_PROTO_VERSION = 1
BOOT_FRAME_SOF = 0x55AA
BOOT_IMAGE_MAGIC = 0x50554741

CMD_HELLO = 0x01
CMD_INFO = 0x02
CMD_START = 0x10
CMD_DATA = 0x11
CMD_END = 0x12
CMD_ABORT = 0x13
CMD_REBOOT = 0x14
CMD_ACK = 0x7E
CMD_NACK = 0x7F

IMAGE_TYPE_APP = 1
IMAGE_TYPE_LOGIC = 2
SUPPORT_IMAGE_APP = 1 << 0
SUPPORT_IMAGE_LOGIC = 1 << 1

ERR_NAMES = {
    0x01: "FRAME_CRC",
    0x02: "UNSUPPORTED_CMD",
    0x03: "HEADER",
    0x04: "RANGE",
    0x05: "LENGTH",
    0x06: "FLASH_ERASE",
    0x07: "FLASH_WRITE",
    0x08: "SEQ",
    0x09: "IMAGE_CRC",
    0x0A: "BAD_STATE",
    0x0B: "IMAGE_TYPE",
    0x0C: "TIMEOUT",
}


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, seq: int, payload: bytes = b"") -> bytes:
    header = struct.pack("<HBBHH", BOOT_FRAME_SOF, cmd, 0, seq & 0xFFFF, len(payload))
    crc = crc16_ccitt(header[2:] + payload)
    return header + payload + struct.pack("<H", crc)


def build_image_header(
    image: bytes,
    image_version: int,
    chunk_size: int,
    target_addr: int,
    image_type: int,
) -> bytes:
    image_crc32 = zlib.crc32(image) & 0xFFFFFFFF
    header_len = 40
    header_wo_crc = struct.pack(
        "<IHHIIIIIII",
        BOOT_IMAGE_MAGIC,
        BOOT_PROTO_VERSION,
        header_len,
        image_type,
        target_addr,
        len(image),
        image_crc32,
        image_version,
        chunk_size,
        0,
    )
    header_crc32 = zlib.crc32(header_wo_crc) & 0xFFFFFFFF
    return header_wo_crc + struct.pack("<I", header_crc32)


class BootError(RuntimeError):
    pass


class Ag32BootPort:
    def __init__(self, port: str, baudrate: int, read_timeout: float, verbose: bool):
        self.verbose = verbose
        self._log_tail = bytearray()
        self._rx = bytearray()
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=read_timeout,
            write_timeout=1.0,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
        )

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()

    def write_text(self, text: str) -> None:
        data = text.encode("ascii")
        if self.verbose:
            print(f"[text-tx] {text.rstrip()}")
        self.ser.write(data)
        self.ser.flush()

    def send_frame(self, cmd: int, seq: int, payload: bytes = b"") -> None:
        frame = build_frame(cmd, seq, payload)
        if self.verbose:
            print(f"[bin-tx] cmd=0x{cmd:02X} seq={seq} len={len(payload)}")
        self.ser.write(frame)
        self.ser.flush()

    def _emit_log_bytes(self, data: bytes) -> None:
        if not data:
            return
        self._log_tail.extend(data)
        while b"\n" in self._log_tail:
            line, _, rest = self._log_tail.partition(b"\n")
            self._log_tail = bytearray(rest)
            text = line.decode("ascii", errors="ignore").strip("\r")
            if text:
                print(f"[device] {text}")

    def _extract_frame(self):
        while True:
            idx = self._rx.find(b"\xAA\x55")
            if idx < 0:
                if self._rx:
                    keep = 1 if self._rx[-1] == 0xAA else 0
                    flush_len = len(self._rx) - keep
                    if flush_len > 0:
                        self._emit_log_bytes(bytes(self._rx[:flush_len]))
                        del self._rx[:flush_len]
                return None
            if idx > 0:
                self._emit_log_bytes(bytes(self._rx[:idx]))
                del self._rx[:idx]
            if len(self._rx) < 10:
                return None
            payload_len = struct.unpack_from("<H", self._rx, 6)[0]
            total_len = 8 + payload_len + 2
            if len(self._rx) < total_len:
                return None
            frame = bytes(self._rx[:total_len])
            del self._rx[:total_len]
            recv_crc = struct.unpack_from("<H", frame, 8 + payload_len)[0]
            calc_crc = crc16_ccitt(frame[2:8 + payload_len])
            if recv_crc != calc_crc:
                raise BootError(f"frame CRC mismatch: recv=0x{recv_crc:04X} calc=0x{calc_crc:04X}")
            cmd = frame[2]
            seq = struct.unpack_from("<H", frame, 4)[0]
            payload = frame[8:8 + payload_len]
            return cmd, seq, payload

    def read_frame(self, timeout_s: float):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                self._rx.extend(chunk)
                frame = self._extract_frame()
                if frame is not None:
                    if self.verbose:
                        cmd, seq, payload = frame
                        print(f"[bin-rx] cmd=0x{cmd:02X} seq={seq} len={len(payload)}")
                    return frame
        raise BootError(f"timeout waiting for boot response ({timeout_s:.1f}s)")

    def request(self, cmd: int, seq: int, payload: bytes = b"", timeout_s: float = 2.0):
        self.send_frame(cmd, seq, payload)
        while True:
            rx_cmd, rx_seq, rx_payload = self.read_frame(timeout_s)
            if rx_cmd == CMD_ACK:
                return ("ACK", rx_seq, rx_payload)
            if rx_cmd == CMD_NACK:
                return ("NACK", rx_seq, rx_payload)

    def enter_boot(self, settle_s: float) -> None:
        self.reset_input_buffer()
        self.write_text("ENTER BOOT\n")
        time.sleep(settle_s)

    def reset_input_buffer(self) -> None:
        self.ser.reset_input_buffer()
        self._rx.clear()
        self._log_tail.clear()


def decode_ack(payload: bytes):
    if len(payload) != 12:
        raise BootError(f"unexpected ACK payload length: {len(payload)}")
    status, reserved, ack_seq, value0, value1 = struct.unpack("<BBHII", payload)
    return {
        "status": status,
        "reserved": reserved,
        "ack_seq": ack_seq,
        "value0": value0,
        "value1": value1,
    }


def decode_nack(payload: bytes):
    if len(payload) != 12:
        raise BootError(f"unexpected NACK payload length: {len(payload)}")
    error_code, reserved, nack_seq, detail0, detail1 = struct.unpack("<BBHII", payload)
    return {
        "error_code": error_code,
        "error_name": ERR_NAMES.get(error_code, "UNKNOWN"),
        "reserved": reserved,
        "nack_seq": nack_seq,
        "detail0": detail0,
        "detail1": detail1,
    }


def decode_info(payload: bytes):
    if len(payload) != 36:
        raise BootError(f"unexpected INFO payload length: {len(payload)}")
    keys = [
        "boot_version",
        "proto_version",
        "support_image_mask",
        "app_start",
        "app_size",
        "logic_start",
        "logic_size",
        "page_size",
        "max_chunk_size",
    ]
    values = struct.unpack("<IIIIIIIII", payload)
    return dict(zip(keys, values))


def expect_ack(result):
    tag, seq, payload = result
    if tag == "NACK":
        nack = decode_nack(payload)
        raise BootError(
            "boot returned NACK: "
            f"seq={seq} error={nack['error_name']} "
            f"detail0=0x{nack['detail0']:08X} detail1=0x{nack['detail1']:08X}"
        )
    return decode_ack(payload)


def cmd_hello(port: Ag32BootPort, args) -> None:
    if args.enter_boot:
        port.enter_boot(args.settle)
    ack = expect_ack(port.request(CMD_HELLO, 1, timeout_s=args.timeout))
    print(
        "HELLO OK "
        f"proto={ack['value0']} "
        f"flags=0x{ack['value1']:08X}"
    )


def cmd_info(port: Ag32BootPort, args) -> None:
    if args.enter_boot:
        port.enter_boot(args.settle)
    tag, seq, payload = port.request(CMD_INFO, 2, timeout_s=args.timeout)
    if tag == "NACK":
        nack = decode_nack(payload)
        raise BootError(f"INFO NACK: {nack}")
    info = decode_info(payload)
    print("INFO OK")
    for key in [
        "boot_version",
        "proto_version",
        "support_image_mask",
        "app_start",
        "app_size",
        "logic_start",
        "logic_size",
        "page_size",
        "max_chunk_size",
    ]:
        print(f"  {key} = 0x{info[key]:08X}")


def cmd_reboot(port: Ag32BootPort, args) -> None:
    if args.enter_boot:
        port.enter_boot(args.settle)
    expect_ack(port.request(CMD_REBOOT, 3, timeout_s=args.timeout))
    print("REBOOT OK")


def cmd_upgrade_image(port: Ag32BootPort, args, image_type: int, image_label: str, support_mask: int) -> None:
    image_path = Path(args.bin).resolve()
    image = image_path.read_bytes()
    if not image:
        raise BootError("empty image file")

    if args.enter_boot:
        port.enter_boot(args.settle)
    hello = expect_ack(port.request(CMD_HELLO, 1, timeout_s=args.timeout))
    if hello["value0"] != BOOT_PROTO_VERSION:
        raise BootError(f"protocol mismatch: device={hello['value0']} host={BOOT_PROTO_VERSION}")

    tag, seq, payload = port.request(CMD_INFO, 2, timeout_s=args.timeout)
    if tag == "NACK":
        raise BootError(f"INFO failed: {decode_nack(payload)}")
    info = decode_info(payload)

    if (info["support_image_mask"] & support_mask) == 0:
        raise BootError(
            f"device does not support {image_label}: "
            f"support_image_mask=0x{info['support_image_mask']:08X}"
        )

    chunk_size = args.chunk_size or info["max_chunk_size"]
    if chunk_size <= 0 or chunk_size > info["max_chunk_size"]:
        raise BootError(f"invalid chunk size: {chunk_size}, device max={info['max_chunk_size']}")

    header = build_image_header(
        image=image,
        image_version=args.image_version,
        chunk_size=chunk_size,
        target_addr=args.target_addr or info[args.target_key],
        image_type=image_type,
    )

    target_addr = args.target_addr or info[args.target_key]
    image_limit = info[args.size_key]
    if args.target_addr is not None and args.target_addr != info[args.target_key]:
        raise BootError(
            f"invalid {image_label} target: 0x{args.target_addr:08X}, "
            f"expected 0x{info[args.target_key]:08X}"
        )
    if len(image) > image_limit:
        raise BootError(
            f"{image_label} image too large: {len(image)} bytes, "
            f"partition limit={image_limit}"
        )

    print(f"UPGRADE {image_label} image={image_path}")
    print(f"  size      = {len(image)} bytes")
    print(f"  target    = 0x{target_addr:08X}")
    print(f"  chunk     = {chunk_size} bytes")
    print(f"  version   = 0x{args.image_version:08X}")

    start_ack = expect_ack(port.request(CMD_START, 3, header, timeout_s=max(args.timeout, 5.0)))
    print(
        "START OK "
        f"image_size={start_ack['value0']} "
        f"chunk_size={start_ack['value1']}"
    )

    total_chunks = (len(image) + chunk_size - 1) // chunk_size
    for chunk_index in range(total_chunks):
        offset = chunk_index * chunk_size
        chunk = image[offset:offset + chunk_size]
        payload = struct.pack("<IHH", offset, len(chunk), 0) + chunk
        ack = expect_ack(port.request(CMD_DATA, chunk_index, payload, timeout_s=max(args.timeout, 5.0)))
        print(
            f"DATA {chunk_index + 1}/{total_chunks} "
            f"written={ack['value0']}/{ack['value1']}"
        )

    end_ack = expect_ack(port.request(CMD_END, 4, timeout_s=max(args.timeout, 5.0)))
    print(
        "END OK "
        f"image_crc32=0x{end_ack['value0']:08X} "
        f"image_size={end_ack['value1']}"
    )

    if args.no_reboot:
        print("UPGRADE OK (staying in boot)")
        return

    expect_ack(port.request(CMD_REBOOT, 5, timeout_s=args.timeout))
    print("UPGRADE OK (reboot requested)")


def cmd_upgrade_app(port: Ag32BootPort, args) -> None:
    args.target_key = "app_start"
    args.size_key = "app_size"
    cmd_upgrade_image(port, args, IMAGE_TYPE_APP, "APP", SUPPORT_IMAGE_APP)


def cmd_upgrade_cpld(port: Ag32BootPort, args) -> None:
    args.target_key = "logic_start"
    args.size_key = "logic_size"
    cmd_upgrade_image(port, args, IMAGE_TYPE_LOGIC, "CPLD", SUPPORT_IMAGE_LOGIC)


def make_parser():
    parser = argparse.ArgumentParser(description="AG32 BOOT serial upgrade tool")
    parser.add_argument("--port", required=True, help="serial port, e.g. COM3 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="serial baudrate")
    parser.add_argument("--timeout", type=float, default=2.0, help="command timeout in seconds")
    parser.add_argument("--settle", type=float, default=1.2, help="wait time after ENTER BOOT")
    parser.add_argument("--verbose", action="store_true", help="show binary TX/RX details")

    subparsers = parser.add_subparsers(dest="command", required=True)

    hello = subparsers.add_parser("hello", help="send HELLO")
    hello.add_argument("--enter-boot", action="store_true", help="send ENTER BOOT before HELLO")
    hello.set_defaults(func=cmd_hello)

    info = subparsers.add_parser("info", help="send INFO")
    info.add_argument("--enter-boot", action="store_true", help="send ENTER BOOT before INFO")
    info.set_defaults(func=cmd_info)

    reboot = subparsers.add_parser("reboot", help="send REBOOT")
    reboot.add_argument("--enter-boot", action="store_true", help="send ENTER BOOT before REBOOT")
    reboot.set_defaults(func=cmd_reboot)

    upgrade = subparsers.add_parser("upgrade-app", help="upgrade APP image")
    upgrade.add_argument("--bin", required=True, help="APP binary path")
    upgrade.add_argument("--image-version", type=lambda value: int(value, 0), required=True,
                         help="image version, e.g. 0x20260805")
    upgrade.add_argument("--chunk-size", type=int, default=0, help="chunk size override")
    upgrade.add_argument("--target-addr", type=lambda value: int(value, 0), default=None,
                         help="optional target address; must match BOOT partition")
    upgrade.add_argument("--enter-boot", action="store_true", help="send ENTER BOOT before upgrade")
    upgrade.add_argument("--no-reboot", action="store_true", help="finish upgrade without reboot")
    upgrade.set_defaults(func=cmd_upgrade_app)

    cpld = subparsers.add_parser("upgrade-cpld", aliases=["upgrade-logic"],
                                 help="upgrade CPLD/LOGIC image")
    cpld.add_argument("--bin", required=True, help="CPLD logic binary path")
    cpld.add_argument("--image-version", type=lambda value: int(value, 0), required=True,
                      help="image version, e.g. 0x0100")
    cpld.add_argument("--chunk-size", type=int, default=0, help="chunk size override")
    cpld.add_argument("--target-addr", type=lambda value: int(value, 0), default=None,
                      help="optional target address; must match BOOT partition")
    cpld.add_argument("--enter-boot", action="store_true", help="send ENTER BOOT before upgrade")
    cpld.add_argument("--no-reboot", action="store_true", help="finish upgrade without reboot")
    cpld.set_defaults(func=cmd_upgrade_cpld)

    return parser


def main() -> int:
    parser = make_parser()
    args = parser.parse_args()
    port = Ag32BootPort(args.port, args.baud, 0.2, args.verbose)
    try:
        args.func(port, args)
        return 0
    except BootError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        port.close()


if __name__ == "__main__":
    raise SystemExit(main())
