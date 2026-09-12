import argparse
import sys
import unittest
from pathlib import Path

from run_full_campaign import build_phase1_command, build_phase2_command


class FullCampaignCommandTests(unittest.TestCase):
    def setUp(self):
        self.args = argparse.Namespace(
            config=Path("config.local.json"),
            session_dir=Path("results/session"),
            seed=123,
            sample_count=40,
            rate=8,
            tdd_num=20,
            uplink_length=50,
            downlink_length=50,
            packet_count=1000,
            max_send_attempts=3,
            cfo_jump_threshold=500,
            location_description="overlap",
        )
        self.script_dir = Path("tools")

    def test_phase1_runs_all_cases_with_40_samples(self):
        command = build_phase1_command(self.args, self.script_dir)
        self.assertEqual(command[0], sys.executable)
        self.assertEqual(command[command.index("--cases") + 1], "4,5,6")
        self.assertEqual(command[command.index("--sample-count") + 1], "40")

    def test_phase2_runs_fixed_case5_rate8_with_1000_packets(self):
        command = build_phase2_command(self.args, self.script_dir)
        self.assertTrue(command[1].endswith("fixed_config_sync_test.py"))
        expected = {
            "--rate": "8",
            "--tdd-num": "20",
            "--uplink-length": "50",
            "--downlink-length": "50",
            "--packet-count": "1000",
        }
        for option, value in expected.items():
            self.assertEqual(command[command.index(option) + 1], value)


if __name__ == "__main__":
    unittest.main()
