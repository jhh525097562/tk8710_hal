"""Run with host mock executable, never with the board executable."""
import json
import os
import subprocess
import sys
import unittest

BINARY = os.environ.get("FREQ_SCHEDULER_MOCK", "/tmp/test_gw_freq_scheduler")
BASE = [BINARY, "--base-hz", "496800000", "--start-hz", "0", "--end-hz", "0",
        "--step-hz", "100", "--hold-seconds", "1", "--settle-seconds", "0"]


def run(extra=(), env=None, **kwargs):
    environment = os.environ.copy()
    environment.update(env or {})
    completed = subprocess.run(BASE+list(extra), env=environment, capture_output=True, text=True,
                               timeout=10, **kwargs)
    events = [json.loads(line[len("FREQ_EVENT "):]) for line in completed.stdout.splitlines()
              if line.startswith("FREQ_EVENT ")]
    return completed, events


@unittest.skipUnless(os.path.isfile(BINARY), "build and set FREQ_SCHEDULER_MOCK first")
class SchedulerTests(unittest.TestCase):
    def test_duration_starts_after_readiness_and_cleans_up(self):
        completed, events = run(env={"MOCK_NS_RATE": "3"})
        self.assertEqual(completed.returncode, 0, completed.stderr)
        configured, started, ended, done = events
        self.assertGreaterEqual(started["monotonic_ms"]-configured["monotonic_ms"], 200)
        self.assertGreaterEqual(ended["elapsed_ms"], 1000)
        self.assertEqual(done["event"], "SWEEP_DONE")
        self.assertIn("MOCK_CLEANUP", completed.stdout)

    def test_mode18_maps_to_ns7(self):
        completed, events = run(["--rate", "18"], env={"MOCK_NS_RATE": "7"})
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_three_points_and_descending_order(self):
        completed, events = run(["--start-hz", "100", "--end-hz", "-100", "--step-hz", "-100"])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual([e["offset_hz"] for e in events if e["event"] == "POINT_END"], [100, 0, -100])

    def test_invalid_options_never_open_hardware(self):
        for arguments in (["--step-hz", "0"], ["--end-hz", "110"], ["--rate", "4"]):
            completed, events = run(arguments)
            self.assertEqual(completed.returncode, 2)
            self.assertNotIn("MOCK_CLEANUP", completed.stdout)

    def test_gps_and_config_failure_no_valid_point(self):
        for variable in ("MOCK_GPS_FAIL", "MOCK_CONFIG_FAIL"):
            completed, events = run(env={variable: "1"})
            self.assertNotEqual(completed.returncode, 0)
            self.assertNotIn("POINT_END", [e["event"] for e in events])
            self.assertEqual(events[-1]["event"], "SWEEP_FAILED")
            self.assertIn("MOCK_CLEANUP", completed.stdout)

    def test_controller_eof_stops_without_counting_point(self):
        completed, events = run(["--require-controller"], input="")
        self.assertNotEqual(completed.returncode, 0)
        self.assertEqual(events[-1]["event"], "SWEEP_FAILED")

    def test_pps_startup_retry_requires_fresh_readiness_and_full_hold(self):
        completed, events = run(env={"MOCK_PPS_STARTUP_ONCE": "1"})
        self.assertEqual(completed.returncode, 0)
        self.assertEqual([e["event"] for e in events],
                         ["POINT_CONFIG", "POINT_RETRY", "POINT_START", "POINT_END", "SWEEP_DONE"])
        self.assertGreaterEqual(events[3]["elapsed_ms"], 1000)

    def test_pps_fault_inside_effective_window_never_retries(self):
        completed, events = run(env={"MOCK_PPS_AFTER_START": "1"})
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("POINT_START", [e["event"] for e in events])
        self.assertNotIn("POINT_RETRY", [e["event"] for e in events])

    def test_pps_startup_retries_are_bounded(self):
        completed, events = run(env={"MOCK_PPS_ALWAYS_FAIL": "1"})
        self.assertNotEqual(completed.returncode, 0)
        self.assertEqual(sum(e["event"] == "POINT_RETRY" for e in events), 2)


if __name__ == "__main__":
    unittest.main()
