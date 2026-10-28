import socketserver
import sys
import threading
import time
import unittest
from collections import deque
from io import StringIO
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock


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
        try:
            self.handle_commands()
        except ConnectionError:
            pass

    def handle_commands(self):
        self.wfile.write(b"OK RF_CAL_SERVER 1\n")
        self.wfile.flush()
        while True:
            line = self.rfile.readline()
            if not line:
                return
            command = line.decode("ascii").strip().split()
            if command == ["PING"]:
                response = "OK PONG"
            elif command == ["SLOW"]:
                time.sleep(0.3)
                response = "OK SLOW"
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
            elif len(command) == 3 and command[0] == "WAKEUP":
                response = f"OK WAKEUP mode={int(command[1])} duration_ms={int(command[2])}"
            elif command == ["LOG"]:
                response = (
                    "OK LOG seq=7 RX user=7 freq=125Hz rssi=-55 snr=18 "
                    "total=100 lost=3"
                )
            elif command == ["SUBSCRIBE", "LOG"]:
                self.wfile.write(b"OK SUBSCRIBE LOG\n")
                self.wfile.write(
                    b"EVENT LOG seq=8 RX lost: crc_ok=0 crc_err=1 total=101 lost=4\n"
                )
                self.wfile.flush()
                continue
            elif command == ["UNSUBSCRIBE", "LOG"]:
                response = "OK UNSUBSCRIBE LOG"
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
        self.assertEqual(extract_log_sequence("EVENT LOG seq=18 RX lost"), 18)
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
        FakeCalibrationHandler.registers = {0x08C8: 0x12345678}
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

    def test_register_read_survives_idle_longer_than_timeout(self):
        time.sleep(self.client.timeout + 0.2)
        self.assertTrue(self.client.connected)
        self.assertEqual(self.client.read_register(0x08C8), 0x12345678)

    def test_close_unblocks_idle_receiver(self):
        receiver = self.client._receiver
        started = time.monotonic()
        self.client.close()
        receiver.join(timeout=0.5)
        self.assertFalse(receiver.is_alive())
        self.assertLess(time.monotonic() - started, 0.8)

    def test_reconnect_after_idle_can_read_register(self):
        old_receiver = self.client._receiver
        self.client.connect("127.0.0.1", self.server.server_address[1])
        old_receiver.join(timeout=0.5)
        self.assertFalse(old_receiver.is_alive())
        time.sleep(self.client.timeout + 0.2)
        self.assertEqual(self.client.read_register(0x08C8), 0x12345678)

    def test_command_timeout_disconnects_and_reconnect_has_no_stale_reply(self):
        self.client.timeout = 0.05
        with self.assertRaisesRegex(TimeoutError, "等待服务端响应超时"):
            self.client.command("SLOW")
        self.assertFalse(self.client.connected)
        self.client.timeout = 1.0
        self.client.connect("127.0.0.1", self.server.server_address[1])
        self.assertEqual(self.client.ping(), "OK PONG")
        self.assertEqual(self.client.read_register(0x08C8), 0x12345678)

    def test_server_error_becomes_exception(self):
        with self.assertRaisesRegex(RuntimeError, "ERR INVALID_COMMAND"):
            self.client.command("UNKNOWN")

    def test_send_wakeup_and_boundaries(self):
        for mode, duration in ((2, 5000), (1, 0), (3, 0xFFFFFF)):
            with self.subTest(mode=mode, duration=duration):
                self.assertEqual(
                    self.client.send_wakeup(mode, duration),
                    f"OK WAKEUP mode={mode} duration_ms={duration}",
                )

    def test_invalid_wakeup_is_rejected_before_sending(self):
        self.client.command = Mock()
        for mode, duration in ((0, 1), (4, 1), (1, -1), (1, 0x1000000)):
            with self.subTest(mode=mode, duration=duration), self.assertRaises(ValueError):
                self.client.send_wakeup(mode, duration)
        self.client.command.assert_not_called()

    def test_wakeup_server_failure_is_reported(self):
        self.client.command = Mock(side_effect=RuntimeError("ERR WAKEUP_FAILED 7"))
        with self.assertRaisesRegex(RuntimeError, "ERR WAKEUP_FAILED 7"):
            self.client.send_wakeup(2, 5000)

    def test_subscribed_log_is_delivered_without_polling(self):
        event = threading.Event()
        received = []

        def on_log(response):
            received.append(response)
            event.set()

        self.client.set_log_callback(on_log)
        self.assertEqual(self.client.subscribe_log(), "OK SUBSCRIBE LOG")
        self.assertTrue(event.wait(1.0))
        self.assertEqual(
            received,
            ["EVENT LOG seq=8 RX lost: crc_ok=0 crc_err=1 total=101 lost=4"],
        )
        self.assertEqual(self.client.unsubscribe_log(), "OK UNSUBSCRIBE LOG")


class ServiceArgumentTests(unittest.TestCase):
    def test_wakeup_arguments_only_apply_to_mode_three(self):
        executable = REPO_ROOT / "build_jtool" / "Test8710RFTest.exe"
        arguments = build_service_arguments(
            executable, 6, 509100000, 42, 126, 3, "127.0.0.1", 12879, 3, 1234,
        )
        self.assertEqual(arguments[1:6], ["6", "509100000", "42", "126", "3"])
        self.assertEqual(arguments[-4:], ["--wakeup-mode", "3", "--wakeup-ms", "1234"])
        with self.assertRaises(ValueError):
            build_service_arguments(
                executable, 6, 509100000, 42, 126, 3, "127.0.0.1", 12879, 4, 1234,
            )

    def test_wakeup_button_captures_parameters_before_worker_runs(self):
        app = object.__new__(RfCalibrationApp)
        app.wakeup_mode_var = SimpleNamespace(get=Mock(return_value="3"))
        app.wakeup_ms_var = SimpleNamespace(get=Mock(return_value="1200"))
        app._run_async = Mock()
        app._log = Mock()
        client = SimpleNamespace(send_wakeup=Mock(return_value="OK WAKEUP"))
        app._require_client = lambda: client

        app._send_wakeup()
        _, task, on_success = app._run_async.call_args.args
        app.wakeup_mode_var.get.side_effect = AssertionError("Tk read in worker")
        app.wakeup_ms_var.get.side_effect = AssertionError("Tk read in worker")
        on_success(task())
        client.send_wakeup.assert_called_once_with(3, 1200)
        app._log.assert_called_once_with("OK WAKEUP")

    def test_wakeup_button_rejects_invalid_duration(self):
        app = object.__new__(RfCalibrationApp)
        app.wakeup_mode_var = SimpleNamespace(get=lambda: "2")
        app.wakeup_ms_var = SimpleNamespace(get=lambda: "16777216")
        app._run_async = Mock()
        app._log = Mock()
        app._send_wakeup()
        app._run_async.assert_not_called()
        app._log.assert_called_once()

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
