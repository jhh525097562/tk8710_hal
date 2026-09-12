import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from configured_sync_test import (
    BroadcastSample,
    _gateway_summary,
    _write_csv_reports,
    analyze_broadcast_samples,
    build_test_spec,
    configured_gateway_body,
    infer_expected_bcnbits,
    parse_gateway_statistics,
    parse_rx_tdd,
    sanitize_description,
)


class ConfiguredSyncTest(unittest.TestCase):
    def test_rate8_sf20_length50_spec(self):
        spec = build_test_spec(8, 20, 50, 50, 20)
        self.assertEqual(3, spec.uplink_blocks)
        self.assertEqual(3, spec.downlink_blocks)
        self.assertEqual(350_000, spec.frame_period_us)
        self.assertEqual(20, spec.frame_count)
        self.assertEqual(7, spec.pps_period_seconds)

    def test_gateway_body_uses_requested_ns_lengths(self):
        spec = build_test_spec(8, 20, 50, 50, 20)
        body = configured_gateway_body(
            {"gw_id": "04", "nwk_num": 2}, spec,
            {"freq_major": 7, "freq_minor": 6}, "overlap")
        self.assertEqual(20, body["tdd_num"])
        self.assertEqual(
            [{"rate_mode": 3, "uplink_len": 50, "downlink_len": 50}],
            body["rate_cfgs"])

    def test_parse_five_and_four_field_rx_tdd(self):
        current = parse_rx_tdd("RX_TDD:20,-83,12,-1710,2", 4, "now")
        self.assertEqual((20, -83, 12, -1710, 2),
                         (current.tdd, current.rssi, current.snr,
                          current.cfo, current.bcnbits))
        legacy = parse_rx_tdd("RX_TDD:1,-87,14,-1710")
        self.assertIsNone(legacy.bcnbits)
        with self.assertRaisesRegex(ValueError, "invalid RX_TDD"):
            parse_rx_tdd("RX_TDD:1,-87,14,bad,2")

    def test_tdd_wrap_is_not_a_jump(self):
        samples = [parse_rx_tdd(f"RX_TDD:{tdd},-80,14,-1710,2")
                   for tdd in (17, 18, 19, 20, 1)]
        result = analyze_broadcast_samples(samples, 20, 500)
        self.assertTrue(result["passed"], result)
        self.assertEqual(0, result["tdd_jump_count"])
        self.assertEqual(0, result["cfo_jump_count"])

    def test_tdd_and_cfo_are_checked_per_bcnbits(self):
        rows = (
            "RX_TDD:1,-80,14,-100,1",
            "RX_TDD:1,-90,10,-200,2",
            "RX_TDD:2,-81,14,-120,1",
            "RX_TDD:3,-91,10,-900,2",
        )
        result = analyze_broadcast_samples(
            [parse_rx_tdd(row) for row in rows], 20, 500)
        self.assertEqual(1, result["tdd_jump_count"])
        self.assertEqual(1, result["cfo_jump_count"])
        self.assertEqual(3, result["bcnbits_transition_count"])

    def test_expected_bcnbits_only_applies_to_near_gateway_location(self):
        samples = [parse_rx_tdd("RX_TDD:1,-80,14,-100,1"),
                   parse_rx_tdd("RX_TDD:1,-90,10,-200,2")]
        overlap = analyze_broadcast_samples(samples, 20, 500)
        self.assertNotIn("expected_bcnbits_mismatch", overlap["failure_reasons"])
        near = analyze_broadcast_samples(samples, 20, 500, expected_bcnbits=1)
        self.assertIn("expected_bcnbits_mismatch", near["failure_reasons"])
        self.assertEqual(1, infer_expected_bcnbits("靠近网关04"))
        self.assertEqual(2, infer_expected_bcnbits("near-gw05"))
        self.assertIsNone(infer_expected_bcnbits("重叠覆盖区域"))

    def test_gateway_log_duplicates_are_removed(self):
        line = ("TRM: RX users - rateMode=8, systemFrame=1, superFrame=2, "
                "userCount=1, firstRssi=-80\n")
        send = "TRM: Sent 1/1 users with fixed power=34, systemFrame=2\n"
        parsed = parse_gateway_statistics(line + send + line + send)
        self.assertEqual(1, len(parsed["receives"]))
        self.assertEqual(1, len(parsed["sends"]))

    def test_location_description_is_safe_for_result_directory(self):
        self.assertEqual("靠近网关04_第一次", sanitize_description(" 靠近网关04/第一次 "))
        with self.assertRaisesRegex(ValueError, "usable"):
            sanitize_description("<>:/*")

    def test_gateway_summary_counts_receive_from_any_retry(self):
        packets = [{"attempts": [
            {"gateway_windows": {
                "04": {"receives": [{"rate": 8}], "sends": [],
                       "sent_ack": False},
            }},
            {"gateway_windows": {
                "04": {"receives": [], "sends": [], "sent_ack": False},
            }},
        ]}]
        result = _gateway_summary(packets, ["04"])
        self.assertEqual(1, result["04"]["logical_packets_received"])
        self.assertEqual(1, result["04"]["attempts_with_receive"])

    def test_csv_report_accepts_packet_without_attempt(self):
        analysis = analyze_broadcast_samples([], 20, 500)
        result = {
            "gateway_ids": ["04", "05"],
            "packets": [{"index": 1, "attempts": []}],
            "broadcast_samples": [],
            "broadcast_analysis": analysis,
            "malformed_rx_tdd": [],
        }
        with TemporaryDirectory() as directory:
            output = Path(directory)
            _write_csv_reports(output, result)
            packet_csv = (output / "packet_summary.csv").read_text(
                encoding="utf-8-sig")
            self.assertIn("attempt_not_started", packet_csv)
            self.assertTrue((output / "broadcast_anomalies.csv").exists())


if __name__ == "__main__":
    unittest.main()
