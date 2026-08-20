import socketserver
import sys
import threading
import unittest
from collections import deque
from io import StringIO
from pathlib import Path
from types import SimpleNamespace


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from rf_calibration_gui import (  # noqa: E402
    RfCalibrationApp,
    RfCalibrationClient,
    application_root,
    build_service_arguments,
    extract_log_sequence,
    pack_dc_word,
    parse_dc_component,
    parse_integer,
)


class FakeCalibrationHandler(socketserver.StreamRequestHandler):
    registers = {0x08C8: 0x12345678}

    def handle(self):
        self.wfile.write(b"OK RF_CAL_SERVER 1\n")
        self.wfile.flush()
        while True:
            line = self.rfile.readline()
            if not line:
                return
            command = line.decode("ascii").strip().split()
            if command == ["PING"]:
                response = "OK PONG"
            elif len(command) == 2 and command[0] == "READ":
                address = int(command[1], 0)
                value = self.registers.get(address, 0)
                response = f"OK READ 0x{address:04X} 0x{value:08X}"
            elif len(command) == 3 and command[0] == "WRITE":
                address = int(command[1], 0)
                value = int(command[2], 0)
                self.registers[address] = value
                response = (
                    f"OK WRITE 0x{address:04X} 0x{value:08X} "
                    f"READBACK 0x{value:08X}"
                )
            elif command == ["STATS"]:
                response = (
                    "OK STATS mode=2 total=100 lost=3 period=20 "
                    "period_lost=1 loss=3.00 last_valid=1 last_user=7 "
                    "last_rssi=-55 last_snr=18 last_freq=125"
                )
            elif command == ["LOG"]:
                response = (
                    "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 "
                    "total=100 lost=3"
                )
            elif command == ["QUIT"]:
                self.wfile.write(b"OK BYE\n")
                self.wfile.flush()
                return
            elif command == ["SHUTDOWN"]:
                self.wfile.write(b"OK SHUTDOWN\n")
                self.wfile.flush()
                return
            else:
                response = "ERR INVALID_COMMAND"
            self.wfile.write((response + "\n").encode("ascii"))
            self.wfile.flush()


class FakeCalibrationServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class NumberTests(unittest.TestCase):
    def test_parse_decimal_and_hexadecimal(self):
        self.assertEqual(parse_integer("42", 0, 100, "value"), 42)
        self.assertEqual(parse_integer("0x2A", 0, 100, "value"), 42)

    def test_parse_rejects_invalid_range(self):
        with self.assertRaises(ValueError):
            parse_integer("101", 0, 100, "value")

    def test_pack_dc_word_supports_signed_and_raw_values(self):
        self.assertEqual(pack_dc_word(0x1234, 0x5678), 0x12345678)
        self.assertEqual(pack_dc_word(-1, -2), 0xFFFFFFFE)
        self.assertEqual(parse_dc_component("0xFFFF", "I_DC"), 0xFFFF)

    def test_extract_log_sequence(self):
        self.assertEqual(extract_log_sequence("OK LOG seq=17 RX user=0"), 17)
        self.assertIsNone(extract_log_sequence("OK STATS total=1"))


class ClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        FakeCalibrationHandler.registers = {0x08C8: 0x12345678}
        cls.server = FakeCalibrationServer(("127.0.0.1", 0), FakeCalibrationHandler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2.0)

    def setUp(self):
        self.client = RfCalibrationClient(timeout=1.0)
        self.client.connect("127.0.0.1", self.server.server_address[1])

    def tearDown(self):
        self.client.close()

    def test_ping_read_write_and_quit(self):
        self.assertEqual(self.client.ping(), "OK PONG")
        self.assertEqual(self.client.read_register(0x08C8), 0x12345678)
        self.assertEqual(self.client.write_register(0x08C8, 0x89ABCDEF), 0x89ABCDEF)
        self.assertEqual(self.client.read_register(0x08C8), 0x89ABCDEF)
        self.assertEqual(self.client.quit(), "OK BYE")
        self.assertFalse(self.client.connected)

    def test_stats_and_log_commands(self):
        expected_stats = (
            "OK STATS mode=2 total=100 lost=3 period=20 period_lost=1 "
            "loss=3.00 last_valid=1 last_user=7 last_rssi=-55 last_snr=18 "
            "last_freq=125"
        )
        expected_log = (
            "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 total=100 lost=3"
        )
        self.assertEqual(self.client.get_stats(), expected_stats)
        self.assertEqual(self.client.get_log(), expected_log)

    def test_server_error_becomes_exception(self):
        with self.assertRaisesRegex(RuntimeError, "ERR INVALID_COMMAND"):
            self.client.command("UNKNOWN")


class ServiceArgumentTests(unittest.TestCase):
    def test_application_root_uses_repo_root_when_not_frozen(self):
        self.assertEqual(application_root(), REPO_ROOT)

    def test_application_root_uses_pyinstaller_bundle_dir(self):
        old_frozen = getattr(sys, "frozen", None)
        old_meipass = getattr(sys, "_MEIPASS", None)
        try:
            sys.frozen = True
            sys._MEIPASS = str(REPO_ROOT / "dist" / "_MEI12345")
            self.assertEqual(application_root(), REPO_ROOT / "dist" / "_MEI12345")
        finally:
            if old_frozen is None:
                delattr(sys, "frozen")
            else:
                sys.frozen = old_frozen
            if old_meipass is None:
                delattr(sys, "_MEIPASS")
            else:
                sys._MEIPASS = old_meipass

    def test_build_service_arguments_uses_new_rf_test_cli(self):
        executable = REPO_ROOT / "build_jtool" / "Test8710RFTest.exe"
        self.assertEqual(
            build_service_arguments(
                executable,
                6,
                509100000,
                0x2A,
                0x7E,
                2,
                "127.0.0.1",
                12879,
            ),
            [
                str(executable),
                "6",
                "509100000",
                "42",
                "126",
                "2",
                "--tcp",
                "--bind",
                "127.0.0.1",
                "--port",
                "12879",
            ],
        )

    def test_local_process_output_is_forwarded_and_retained(self):
        app = object.__new__(RfCalibrationApp)
        app._local_output_lines = deque(maxlen=20)
        app._local_output_lock = threading.Lock()
        forwarded = []
        app._log = forwarded.append
        process = SimpleNamespace(stdout=StringIO("first line\nlast error\n"))

        app._stream_local_process_output(process)

        self.assertEqual(forwarded, ["[RFTest] first line", "[RFTest] last error"])
        self.assertEqual(app._local_process_error_detail(), "last error")


if __name__ == "__main__":
    unittest.main()
