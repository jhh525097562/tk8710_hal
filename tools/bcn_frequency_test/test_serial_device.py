import queue
import tempfile
import threading
import types
import unittest
from pathlib import Path
from unittest.mock import patch

from serial_device import Journal, Terminal


class FakeSerial:
    def __init__(self, *args, **kwargs):
        self.input = queue.Queue()
        self.closed = False

    def read(self, size):
        try:
            item = self.input.get(timeout=.01)
            if isinstance(item, Exception):
                raise item
            return item
        except queue.Empty:
            return b""

    def write(self, data):
        if b"FAIL" in data:
            self.input.put(b"AT_PARAM_ERROR\r\n")
        else:
            for chunk in (b"RX_T", b"DD:1,-80,9,-1954,1\r\n+NWKINFO:4\r\nAT_", b"OK\r\n"):
                self.input.put(chunk)
        return len(data)

    def flush(self):
        pass

    def close(self):
        self.closed = True


class SerialTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.journal = Journal(Path(self.directory.name)/"journal.jsonl")
        self.patch = patch.dict("sys.modules", serial=types.SimpleNamespace(Serial=FakeSerial))
        self.patch.start()
        self.terminal = Terminal("FAKE", 115200, self.journal)

    def tearDown(self):
        try:
            self.terminal.close()
        except RuntimeError:
            pass
        self.journal.close()
        self.patch.stop()
        self.directory.cleanup()

    def test_fragmented_mac_logs_survive_interleaved_command_response(self):
        cursor = self.terminal.command("AT+JOIN=0")
        received = self.terminal.wait(cursor, lambda text: text == "+NWKINFO:4", 1)
        self.assertIn("RX_TDD:1,-80,9,-1954,1", received)
        self.assertIn("+NWKINFO:4", received)
        self.terminal.close()
        self.assertTrue(self.terminal.device.closed)
        self.assertFalse(self.terminal.thread.is_alive())

    def test_parameter_error_is_not_treated_as_ok(self):
        with self.assertRaises(RuntimeError):
            self.terminal.command("AT+FAIL")

    def test_reader_failure_is_reported_on_close(self):
        self.terminal.device.input.put(OSError("serial unplugged"))
        self.terminal.thread.join(timeout=1)
        with self.assertRaises(RuntimeError):
            self.terminal.close()
        self.assertTrue(self.terminal.device.closed)


if __name__ == "__main__":
    unittest.main()
