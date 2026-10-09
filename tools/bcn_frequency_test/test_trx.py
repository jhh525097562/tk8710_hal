import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from analyze import analyze_records, load_records, write_report
from protocol import Config
from serial_device import Terminal
from trx import parse_trx


def line(text, stamp=None):
    return dict(kind="terminal", time_ms=stamp, text=text)


def event(name, stamp, index=0, status=0):
    return dict(kind="gateway_event", event=name, time_ms=stamp, index=index, offset_hz=index*100, status=status)


class TrxTests(unittest.TestCase):
    def test_attachment_slot3_failures_are_not_broadcast_failures(self):
        records = list(load_records(Path(__file__).parent/"fixtures/terminal_trx_sample.txt"))
        result = analyze_records(records)
        stats = result["summary"]["trx"]
        self.assertEqual((stats["bcn"]["success"], stats["bcn"]["failure"]), (29, 0))
        self.assertEqual((stats["broadcast"]["success"], stats["broadcast"]["failure"]), (30, 0))
        self.assertEqual(stats["other_slots"]["failure"], 28)
        self.assertEqual(result["summary"]["samples"], 30)
        self.assertIsNone(result["summary"]["missing_records_estimate"])
        self.assertEqual(result["summary"]["frame_period_ms"], 0)
        self.assertTrue(all(r["time_ms"] is None for r in result["trx_records"]))

    def test_hex_bcnbits_failure_bytes_and_minimal_failure(self):
        parsed = parse_trx("[12:00:00.123]收←◆+bcn :80 - 0 | s,snr: 14,cfo:-2198,rssi:-36,data:1f")
        self.assertEqual(parsed["bcnbits"], 31)
        self.assertEqual(parsed["cfo_hz"], -2198)
        self.assertIsNone(parse_trx("+bcn:81 - 0 | f,data:ff")["bcnbits"])
        self.assertFalse(parse_trx("+bcn:82 - 0 | f")["success"])
        self.assertIsNone(parse_trx("+bcn:82 - 0 | unknown"))

    def test_broadcast_failure_keeps_successful_bcn_without_rx_tdd(self):
        result = analyze_records([
            line("+bcn:1 - 0 | s,snr:9,cfo:500,rssi:-70,data:01", 0),
            line("+data:1 - 1 | f,pkt:2,crc:03,snr:-2,rssi:-101,data(0,0):.", 10),
            line("+data:1 - 3 | f", 20),
            line("+bcn:2 - 0 | f,snr:0,cfo:999,rssi:-110,data:ff", 300)])
        stats = result["summary"]["trx"]
        self.assertEqual(result["summary"]["samples"], 0)
        self.assertEqual(stats["broadcast"]["failure"], 1)
        self.assertEqual(stats["broadcast"]["failure_ratio"], 1)
        self.assertEqual(stats["bcn"]["failure_ratio"], .5)
        self.assertEqual(stats["bcn"]["bcnbits_samples"], 1)
        self.assertEqual(stats["bcn"]["bcnbits_errors"], 0)
        self.assertEqual(stats["bcn"]["cfo_bcnbits_distribution"], [dict(cfo_hz=500, bcnbits=1, count=1)])

    def test_rx_tdd_does_not_double_count_and_success_outside_bits_is_error(self):
        result = analyze_records([line("+bcn:1 - 0 | s,data:02"),
                                  line("+data:1 - 1 | s,pkt:2,crc:00,data(1,52):AA."),
                                  line("RX_TDD:1,-80,9,0,2")])
        stats = result["summary"]["trx"]
        self.assertEqual(stats["bcn"]["bcnbits_errors"], 1)
        self.assertEqual(stats["broadcast"]["records"], 1)
        self.assertEqual(result["summary"]["bcnbits_errors"], 1)

    def test_guard_failed_points_and_unknown_old_log_coverage(self):
        result = analyze_records([event("POINT_START", 0),
            line("+bcn:1 - 0 | f", 100), line("+bcn:2 - 0 | f", 1000),
            line("+data:2 - 1 | f", 1100), event("POINT_END", 2000),
            event("POINT_START", 3000, 1), line("+bcn:3 - 0 | f", 4000),
            event("POINT_FAILED", 5000, 1, 1)])
        self.assertEqual(result["summary"]["trx"]["bcn"]["failure"], 1)
        self.assertEqual(result["summary"]["points"][0]["broadcast_failure"], 1)
        self.assertEqual(result["summary"]["points"][1]["bcn_records"], 0)
        old = analyze_records([line("RX_TDD:1,-80,9,0,0", 100)])
        self.assertIsNone(old["summary"]["trx"]["bcn"]["failure_ratio"])

    def test_malformed_and_missing_success_fields_not_fabricated(self):
        result = analyze_records([line("+bcn:1 - 0 | ?"), line("+data:2 - 1 | garbled"),
                                  line("+bcn:3 - 0 | s,data:XX")])
        self.assertEqual(result["summary"]["trx_malformed_count"], 2)
        self.assertEqual(result["summary"]["trx"]["bcn"]["successful_missing_bcnbits"], 1)

    def test_report_exports_explicit_failures_and_no_timestamp_logs(self):
        result = analyze_records([line("+bcn:1 - 0 | f"), line("+data:1 - 1 | f")])
        with tempfile.TemporaryDirectory() as temp, patch.dict("sys.modules", {"matplotlib": None}):
            report = write_report(result, temp)
            self.assertIn("BCN", report.read_text(encoding="utf-8"))
            self.assertIn("+bcn", (Path(temp)/"bcn_failures.csv").read_text(encoding="utf-8-sig"))
            self.assertIn("+data", (Path(temp)/"broadcast_failures.csv").read_text(encoding="utf-8-sig"))
            self.assertIn("+bcn", (Path(temp)/"terminal_raw.log").read_text(encoding="utf-8"))

    def test_configure_enables_trx_after_mac_before_join(self):
        terminal = Terminal.__new__(Terminal)
        sent = []
        terminal.command = lambda cmd, **kwargs: sent.append(cmd) or 0
        terminal.wait = lambda *args: []
        terminal.configure_and_join(Config(host="test"))
        self.assertEqual(sent[sent.index("AT+PRINTMODE=MAC,1")+1], "AT+PRINTMODE=TRX,1")
        self.assertEqual(sent.count("AT+JOIN=0"), 1)


if __name__ == "__main__":
    unittest.main()
