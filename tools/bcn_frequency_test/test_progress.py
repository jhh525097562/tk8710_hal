import io
import json
import unittest
from unittest.mock import patch

from progress import GatewayProgress
from protocol import Config
from run import monitor
from test_runner import FakeChannel, FakeJournal, FakeTerminal, point


class ProgressTests(unittest.TestCase):
    def test_readiness_snapshot_never_starts_effective_timing(self):
        now = [0]
        lines = []
        progress = GatewayProgress(lines.append, lambda: now[0])
        progress.event(point("POINT_CONFIG"))
        progress.status(dict(phase="readiness", gps_mode="EXTERNAL_ACTIVE", ns_received=1,
                             ns_verified=1, broadcast_valid=0, s3_count=15, s3_idle_ms=20, ready=0))
        self.assertIn("等待有效广播载荷", lines[-1])
        progress.status(dict(phase="readiness", gps_mode="EXTERNAL_ACTIVE", ns_received=1,
                             ns_verified=1, broadcast_valid=1, s3_count=15, ready=1))
        self.assertIn("等待稳定期/POINT_START", lines[-1])
        self.assertTrue(progress.waiting)
        progress.event(point("POINT_START"))
        self.assertFalse(progress.waiting)

    def test_silent_gps_phase_has_periodic_heartbeat_and_retries_keep_elapsed(self):
        now = [0]
        lines = []
        progress = GatewayProgress(lines.append, lambda: now[0])
        progress.event(point("POINT_CONFIG"))
        progress.status(dict(phase="gps_acquire"))
        now[0] = 9
        progress.tick()
        self.assertEqual(len(lines), 1)
        now[0] = 10
        progress.tick()
        self.assertEqual(len(lines), 2)
        self.assertIn("GPS/PPS", lines[-1])
        now[0] = 20
        progress.event(point("POINT_RETRY"))
        progress.status(dict(phase="hal_init"))
        self.assertIn("20 秒", lines[-1])

    def test_legacy_zero_broadcast_hint_and_ns_missing(self):
        lines = []
        progress = GatewayProgress(lines.append, lambda: 0)
        progress.event(point("POINT_CONFIG"))
        progress.diagnostic_line("Invalid parameters for downlink1: dataLen=0")
        progress.report()
        self.assertIn("dataLen=0", lines[-1])
        progress.status(dict(phase="readiness", gps_mode="EXTERNAL_ACTIVE", ns_received=0,
                             ns_verified=0, broadcast_valid=0, s3_count=0, ready=0))
        self.assertIn("等待 NS 配置消息", lines[-1])

    def test_status_and_gps_text_cannot_substitute_for_point_start(self):
        channel = FakeChannel([])
        payload = ("GPS/PPS monitor: STATE=RUNNING ALIGN=1\nFREQ_STATUS "+
                   json.dumps(dict(phase="readiness", ready=1))+"\nFREQ_EVENT "+
                   json.dumps(point("SWEEP_DONE", -1))+"\n").encode()
        channel.chunks = [payload]
        journal = FakeJournal()
        with self.assertRaises(RuntimeError):
            monitor(channel, FakeTerminal(), journal, io.StringIO(),
                    Config(host="test", start_hz=0, end_hz=0))
        self.assertEqual(journal.records[0]["phase"], "readiness")

    def test_fragmented_status_is_saved_with_successful_points(self):
        payload = ("FREQ_EVENT "+json.dumps(point("POINT_CONFIG"))+"\nFREQ_STATUS "+
                   json.dumps(dict(phase="pps_align"))+"\n"+
                   "\n".join("FREQ_EVENT "+json.dumps(point(name, -1 if name == "SWEEP_DONE" else 0))
                             for name in ("POINT_START", "POINT_END", "SWEEP_DONE"))+"\n").encode()
        channel = FakeChannel([])
        channel.chunks = [payload[i:i+9] for i in range(0, len(payload), 9)]
        journal = FakeJournal()
        with patch("builtins.print"):
            monitor(channel, FakeTerminal(), journal, io.StringIO(),
                    Config(host="test", start_hz=0, end_hz=0, hold_seconds=1))
        self.assertEqual(sum(r.get("phase") == "pps_align" for r in journal.records), 1)


if __name__ == "__main__":
    unittest.main()
