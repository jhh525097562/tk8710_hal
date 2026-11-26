import json
from pathlib import Path
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gui
import serial_probe


class ProbeTests(unittest.TestCase):
    def test_reset_echo_alone_is_not_log(self):
        self.assertFalse(serial_probe.has_reset_log(b"AT+RST\r\n"))
        self.assertFalse(serial_probe.has_reset_log(b"\xff\xfe\n"))
        self.assertTrue(serial_probe.has_reset_log(b"BOOT TK8620\r\nAT_OK\r\n"))

    def test_multiple_ports_and_busy_port(self):
        now = [0.0]
        devices = []
        def factory(name, *args, **kwargs):
            if name == "BUSY":
                raise OSError("busy")
            device = Mock()
            device.__enter__ = Mock(return_value=device)
            device.__exit__ = Mock(return_value=None)
            def read(count):
                now[0] += .2
                return b"AT+RST\r\n" if name == "ECHO" else b"BOOT\r\n"
            device.read.side_effect = read
            devices.append(device)
            return device
        ports = [SimpleNamespace(device=n) for n in ("ONE", "ECHO", "BUSY", "TWO")]
        with patch.object(serial_probe.time, "monotonic", lambda: now[0]):
            matches, evidence = serial_probe.detect(115200, .3, log=Mock(), ports=ports, open_port=factory)
        self.assertEqual(matches, ["ONE", "TWO"])
        self.assertEqual(len(evidence), 4)
        for device in devices:
            device.write.assert_called_once_with(b"AT+RST\r\n")
            device.__exit__.assert_called_once()

    def test_cancel_before_scan_does_not_open_port(self):
        factory = Mock()
        matches, _ = serial_probe.detect(cancelled=lambda: True,
            ports=[SimpleNamespace(device="NO_IO")], open_port=factory)
        self.assertEqual(matches, [])
        factory.assert_not_called()

    def test_gui_id_ranges(self):
        self.assertEqual(gui.parse_numbers("0,31-33,166,255"), [0,31,32,33,166,255])
        with self.assertRaises(ValueError):
            gui.parse_numbers("0-256")

    def test_hidden_password_and_defaults(self):
        config = json.loads(Path(__file__).with_name("config.example.json").read_text())
        self.assertEqual(config["host"], "192.168.100.78")
        self.assertEqual(config["ssh_port"], 22)
        self.assertEqual(config["serial_port"], "auto")
        self.assertEqual(config["tx_gain"], 46)
        self.assertNotIn("password", config)

    def test_stop_file_interrupts_main_flow(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stop.request"
            with patch.object(gui.run, "STOP_FILE", path):
                gui.run.check_cancel()
                path.touch()
                with self.assertRaises(KeyboardInterrupt):
                    gui.run.check_cancel()

    def test_timer_default_and_no_manual_recovery(self):
        config = json.loads(Path(__file__).with_name("config.example.json").read_text())
        self.assertEqual(config["timer_wakeup_ms"], 5000)
        self.assertEqual(config["frequency_hz"], 492300000)
        self.assertFalse(hasattr(gui.run, "recover"))


if __name__ == "__main__":
    unittest.main()
