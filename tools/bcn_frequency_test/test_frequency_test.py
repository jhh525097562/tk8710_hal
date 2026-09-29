from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from analyze import analyze_records, load_records, make_windows
from protocol import Config, infer_gap, parse_rx

ROOT = Path(__file__).resolve().parents[2]


def terminal(time_ms, text):
    return dict(kind="terminal", time_ms=time_ms, text=text)


def event(time_ms, name, index=0, offset=100, status=0):
    return dict(kind="gateway_event", time_ms=time_ms, event=name, index=index,
                offset_hz=offset, status=status)


class FrequencyTests(unittest.TestCase):
    def test_historical_log_regression(self):
        path = Path(__file__).parent/"fixtures/terminal_sample.txt"
        result = analyze_records(list(load_records(path)))
        summary = result["summary"]
        self.assertEqual(summary["samples"], 178)
        self.assertEqual(summary["bcnbits_errors"], 0)
        self.assertEqual(summary["cfo_changes"], 83)
        self.assertEqual(summary["bcnbits_changes"], 79)
        self.assertEqual(summary["cfo_change_same_bits"], 4)
        self.assertEqual(summary["missing_records_estimate"], 25)
        self.assertAlmostEqual(summary["frame_period_ms"], 350, delta=1)
        self.assertEqual(summary["uncertain_intervals"], 0)
        joined = summary["joined_window_including_same_receive_block"]
        self.assertEqual((joined["samples"], joined["missing_records_estimate"]), (176, 22))
        self.assertEqual(summary["rejoined"], 0)

    def test_signed_offsets_and_exact_endpoint(self):
        config = Config(host="test").validate()
        self.assertEqual(len(config.offsets()), 21)
        self.assertEqual(config.offsets()[0], -1000)
        config.start_hz, config.end_hz, config.step_hz = 1000, -1000, -100
        self.assertEqual(config.validate().offsets()[-1], -1000)
        config.step_hz = -300
        with self.assertRaises(ValueError):
            config.validate()

    def test_invalid_options(self):
        for overrides in ({"step_hz": 0}, {"step_hz": -100}, {"bits_b": 0},
                          {"rate": 2}, {"base_hz": 0}, {"hold_seconds": True},
                          {"frame_period_ms": float("nan")}, {"tdd_count": 0}):
            with self.subTest(overrides=overrides), self.assertRaises(ValueError):
                Config(host="test", **overrides).validate()

    def test_gap_wrap_full_cycle_and_duplicate(self):
        a = dict(tdd=10, time_ms=0)
        self.assertEqual(infer_gap(a, dict(tdd=1, time_ms=350), 10, 350), (0, "consistent"))
        self.assertEqual(infer_gap(a, dict(tdd=2, time_ms=700), 10, 350), (1, "consistent"))
        self.assertEqual(infer_gap(a, dict(tdd=10, time_ms=3500), 10, 350), (9, "consistent"))
        self.assertIsNone(infer_gap(a, dict(tdd=10, time_ms=30), 10, 350)[0])
        self.assertIsNone(infer_gap(a, dict(tdd=8, time_ms=350), 10, 350)[0])

    def test_bcnbits_error_and_malformed(self):
        result = analyze_records([terminal(0, "RX_TDD:1,-80,9,-2000,0"),
                                  terminal(350, "RX_TDD:2,-81,8,-1900,7"),
                                  terminal(700, "RX_TDD:3,-80,9,garbage,1")], period_ms=350)
        self.assertEqual(result["summary"]["bcnbits_errors"], 1)
        self.assertEqual(result["summary"]["malformed_count"], 1)
        self.assertIsNone(parse_rx("RX_TDD:1,2,3,4"))

    def test_point_guard_and_failure_never_count_as_valid(self):
        records = [event(0, "POINT_START"), terminal(100, "RX_TDD:1,-80,9,0,0"),
                   terminal(1000, "RX_TDD:2,-80,9,0,0"), event(2000, "POINT_END"),
                   event(3000, "POINT_START", 1), terminal(4000, "RX_TDD:3,-80,9,500,1"),
                   event(5000, "POINT_FAILED", 1, status=1)]
        result = analyze_records(records)
        self.assertEqual(result["summary"]["samples"], 1)
        self.assertEqual(result["summary"]["raw_samples"], 3)
        self.assertFalse(result["summary"]["points"][1]["valid"])

    def test_interrupted_point_keeps_data_outside_valid_statistics(self):
        result = analyze_records([event(0, "POINT_START"), terminal(1000, "RX_TDD:1,-80,9,100,1")])
        self.assertEqual(result["summary"]["samples"], 0)
        self.assertEqual(result["summary"]["raw_samples"], 1)

    def test_rejoin_breaks_missing_inference_and_records_duration(self):
        records = [terminal(0, "+NWKINFO:4"), terminal(350, "RX_TDD:9,-80,9,0,0"),
                   terminal(400, "+NWKINFO:6"), terminal(1000, "+NWKINFO:2"),
                   terminal(1400, "+NWKINFO:4"), terminal(1750, "RX_TDD:2,-80,9,100,1")]
        result = analyze_records(records, period_ms=350)
        self.assertEqual(result["summary"]["rejoined"], 1)
        self.assertEqual(result["summary"]["disconnect_events"], 1)
        self.assertEqual(result["transitions"], [])
        self.assertEqual([e for e in result["events"] if e["kind"] == "rejoined"][0]["outage_ms"], 1000)

    def test_repeated_join_state_not_counted_as_rejoin(self):
        result = analyze_records([terminal(0, "+NWKINFO:4"), terminal(10, "+NWKINFO:4")])
        self.assertEqual(result["summary"]["joins"], 1)

    def test_midnight_and_receive_block_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"log.txt"
            path.write_text("[23:59:59.800]RX_TDD:10,-80,9,0,0\n+NWKINFO:4\n"
                            "[00:00:00.150]RX_TDD:1,-80,9,0,0\n", encoding="utf-8")
            records = list(load_records(path))
        self.assertEqual(records[0]["time_ms"], records[1]["time_ms"])
        self.assertEqual(records[2]["time_ms"]-records[0]["time_ms"], 350)

    def test_zero_offset_overlapping_cfo_is_not_called_match(self):
        records = [event(0, "POINT_START", offset=0)]
        for i in range(6):
            records.append(terminal(1000+i*350, f"RX_TDD:{i+1},-80,9,0,{i%2}"))
        records.append(event(4000, "POINT_END", offset=0))
        result = analyze_records(records, period_ms=350)
        self.assertEqual(result["summary"]["association"], "overlapping_groups")

    def test_empty_completed_point_reports_full_silence_unknown_loss(self):
        result = analyze_records([event(0, "POINT_START"), event(60000, "POINT_END")])
        point = result["summary"]["points"][0]
        self.assertIsNone(point["missing_records_estimate"])
        self.assertEqual(point["max_no_log_ms"], 59000)
        self.assertEqual(point["reception_coverage"], "no_samples")

    def test_a_baseline_and_per_point_drift(self):
        records = [dict(kind="baseline_start", time_ms=0), terminal(100, "RX_TDD:1,-80,9,1000,0"),
                   dict(kind="baseline_end", time_ms=500), event(1000, "POINT_START"),
                   terminal(2000, "RX_TDD:2,-80,9,1100,0"), terminal(2350, "RX_TDD:3,-80,9,1400,1"),
                   event(3000, "POINT_END")]
        result = analyze_records(records)
        self.assertEqual(result["summary"]["baseline_a"]["cfo_median"], 1000)
        self.assertEqual(result["summary"]["points"][0]["a_drift_from_baseline_hz"], 100)

    def test_usb_quantized_intervals_do_not_bias_period_to_median(self):
        # Six 344 ms and four 359 ms intervals: mean 350 ms, median 344 ms.
        records = [terminal(0, "RX_TDD:1,-80,9,0,0")]
        stamp = 0
        for index, delta in enumerate([344]*6+[359]*4):
            stamp += delta
            records.append(terminal(stamp, f"RX_TDD:{(index+1)%10+1},-80,9,0,0"))
        result = analyze_records(records)
        self.assertEqual(result["summary"]["frame_period_ms"], 350)
        self.assertEqual(result["summary"]["missing_records_estimate"], 0)


if __name__ == "__main__":
    unittest.main()
