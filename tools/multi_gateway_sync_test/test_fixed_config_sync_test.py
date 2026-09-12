import unittest

from fixed_config_sync_test import (
    BroadcastSample,
    FixedTestParameters,
    analyze_broadcast,
    build_fixed_combination,
    fixed_gateway_body,
    infer_expected_bcnbits,
    interruption_thresholds,
    packet_phase,
    parse_unique_gateway_window,
    sanitize_description,
    RX_TDD_RE,
)


def _parameters(**overrides):
    values = {
        "rate": 8,
        "tdd_num": 20,
        "uplink_length": 50,
        "downlink_length": 50,
        "packet_count": 20,
        "max_send_attempts": 3,
        "cfo_jump_threshold": 500,
        "location_description": "重叠覆盖区域",
        "expected_bcnbits": None,
    }
    values.update(overrides)
    return FixedTestParameters(**values)


def _sample(index, tdd, cfo, bcnbits, rssi=-80, snr=15):
    return BroadcastSample(
        timestamp=f"2026-09-09T00:00:{index:02d}",
        packet_index=1,
        tdd=tdd,
        rssi=rssi,
        snr=snr,
        cfo=cfo,
        bcnbits=bcnbits,
        raw=f"RX_TDD:{tdd},{rssi},{snr},{cfo},{bcnbits}",
    )


class FixedConfigSyncTest(unittest.TestCase):
    def test_default_example_builds_expected_gateway_blocks_and_period(self):
        combination = build_fixed_combination(_parameters())
        self.assertEqual((8,), combination.rates)
        self.assertEqual(20, combination.super_frame_num)
        self.assertEqual((3,), combination.ul_blocks)
        self.assertEqual((3,), combination.dl_blocks)
        self.assertEqual(350_000, combination.frame_period_us)
        self.assertEqual(20, combination.frame_count)

    def test_gateway_body_uses_exact_command_line_lengths(self):
        parameters = _parameters()
        body = fixed_gateway_body(
            {"gw_id": "04aa", "nwk_num": 2}, parameters,
            {"freq_major": 7, "freq_minor": 6})
        self.assertEqual(20, body["tdd_num"])
        self.assertEqual([{"rate_mode": 3, "uplink_len": 50, "downlink_len": 50}],
                         body["rate_cfgs"])

    def test_nondefault_command_line_parameters_drive_ns_configuration(self):
        parameters = _parameters(
            rate=11, tdd_num=7, uplink_length=80, downlink_length=120,
            packet_count=6)
        combination = build_fixed_combination(parameters)
        body = fixed_gateway_body(
            {"gw_id": "04aa", "nwk_num": 2}, parameters,
            {"freq_major": 7, "freq_minor": 6})
        self.assertEqual((11,), combination.rates)
        self.assertEqual(7, combination.super_frame_num)
        self.assertEqual(7, body["tdd_num"])
        self.assertEqual(
            [{"rate_mode": 6, "uplink_len": 80, "downlink_len": 120}],
            body["rate_cfgs"])

    def test_gateway_interruption_thresholds_and_packet_phases(self):
        self.assertEqual((5, 10), interruption_thresholds(20))
        self.assertEqual("before_interruption", packet_phase(5, 5, 10))
        self.assertEqual("gateway_stopped", packet_phase(6, 5, 10))
        self.assertEqual("gateway_stopped", packet_phase(10, 5, 10))
        self.assertEqual("after_restart", packet_phase(11, 5, 10))

    def test_gateway_interruption_requires_enough_packets(self):
        with self.assertRaisesRegex(ValueError, "at least four"):
            build_fixed_combination(
                _parameters(packet_count=3, interrupt_one_gateway=True))

    def test_rx_tdd_parser_accepts_five_fields_and_marks_legacy_line(self):
        current = RX_TDD_RE.fullmatch("RX_TDD:17,-81,15,-1710,2")
        self.assertIsNotNone(current)
        self.assertEqual("2", current.group("bcnbits"))
        legacy = RX_TDD_RE.fullmatch("RX_TDD:17,-81,15,-1710")
        self.assertIsNotNone(legacy)
        self.assertIsNone(legacy.group("bcnbits"))
        self.assertIsNone(RX_TDD_RE.fullmatch("RX_TDD:17,-81,X,-1710,2"))

    def test_broadcast_analysis_is_independent_per_bcnbits_and_wraps_tdd(self):
        samples = [
            _sample(1, 19, -1710, 1),
            _sample(2, 7, -900, 2),
            _sample(3, 20, -1700, 1),
            _sample(4, 8, -920, 2),
            _sample(5, 1, -1690, 1),
        ]
        result = analyze_broadcast(samples, [], 20, 500, None)
        self.assertTrue(result["passed"], result)
        self.assertEqual(0, result["tdd_jump_count"])
        self.assertEqual(0, result["cfo_jump_count"])
        self.assertEqual(4, result["bcnbits_transition_count"])

    def test_broadcast_analysis_detects_tdd_cfo_and_expected_bcn_changes(self):
        samples = [
            _sample(1, 1, -1000, 1),
            _sample(2, 3, -1800, 1),
            _sample(3, 4, -1810, 2),
        ]
        result = analyze_broadcast(samples, [], 20, 500, 1)
        self.assertFalse(result["passed"])
        self.assertEqual(1, result["tdd_jump_count"])
        self.assertEqual(1, result["cfo_jump_count"])
        self.assertEqual(1, result["expected_bcnbits_mismatch_count"])

    def test_expected_restart_gap_resets_broadcast_continuity(self):
        samples = [
            _sample(1, 5, -1000, 1),
            BroadcastSample(
                timestamp="2026-09-09T00:01:00", packet_index=11,
                tdd=11, rssi=-80, snr=15, cfo=-1800, bcnbits=1,
                raw="RX_TDD:11,-80,15,-1800,1", continuity_epoch=1),
            BroadcastSample(
                timestamp="2026-09-09T00:01:01", packet_index=12,
                tdd=12, rssi=-80, snr=15, cfo=-1810, bcnbits=1,
                raw="RX_TDD:12,-80,15,-1810,1", continuity_epoch=1),
        ]
        result = analyze_broadcast(samples, [], 20, 500, None)
        self.assertTrue(result["passed"], result)
        self.assertEqual(1, result["continuity_reset_count"])
        self.assertEqual(0, result["tdd_jump_count"])
        self.assertEqual(0, result["cfo_jump_count"])
        self.assertEqual(10, result["per_bcnbits"]["1"]["maximum_absolute_cfo_delta"])

    def test_gateway_log_deduplication_preserves_distinct_frames(self):
        first = ("RX users - rateMode=8, systemFrame=10, superFrame=1, "
                 "userCount=1, firstRssi=-80")
        second = first.replace("systemFrame=10", "systemFrame=11")
        window = parse_unique_gateway_window(f"{first}\n{second}\n{first}\n")
        self.assertEqual([10, 11], [item["system"] for item in window["receives"]])

    def test_location_description_and_expected_bcn_helpers(self):
        self.assertEqual("重叠覆盖区域", sanitize_description(" 重叠覆盖区域 "))
        self.assertEqual("靠近网关04_阶段1", sanitize_description("靠近网关04:阶段1"))
        self.assertEqual(1, infer_expected_bcnbits("靠近网关04"))
        self.assertEqual(1, infer_expected_bcnbits("靠近网关04-走廊阶段"))
        self.assertEqual(2, infer_expected_bcnbits("near-gw05"))
        self.assertIsNone(infer_expected_bcnbits("重叠覆盖区域"))


if __name__ == "__main__":
    unittest.main()
