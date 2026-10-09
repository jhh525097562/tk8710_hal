import unittest
import io
import stat
import tempfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from run import ensure_mqtt_bridge, download_tree
from test_runner import FakeJournal


class MqttBridgeTests(unittest.TestCase):
    def test_live_bridge_log_download_is_bounded_to_observed_size(self):
        sftp = SimpleNamespace(
            listdir_attr=lambda _: [SimpleNamespace(filename="mqtt_bridge_startup.log",
                                                    st_mode=stat.S_IFREG, st_size=5)],
            open=lambda *_: io.BytesIO(b"ready and still growing"))
        with tempfile.TemporaryDirectory() as directory:
            download_tree(sftp, "/userdata/test", Path(directory))
            self.assertEqual((Path(directory)/"mqtt_bridge_startup.log").read_bytes(), b"ready")

    @patch("run.time.sleep")
    @patch("run.remote_exec", return_value="123 456\n")
    def test_existing_bridge_is_not_restarted(self, remote, sleep):
        result = ensure_mqtt_bridge(object(), "/userdata/test", FakeJournal())
        self.assertEqual(result, dict(action="already_running", pids=[123, 456]))
        self.assertEqual(remote.call_count, 1)
        sleep.assert_not_called()

    @patch("run.time.sleep")
    @patch("run.remote_exec", side_effect=["", "", "321\n", "321\n"])
    def test_absent_bridge_starts_from_userdata_detached_and_checks_liveness(self, remote, sleep):
        journal = FakeJournal()
        result = ensure_mqtt_bridge(object(), "/userdata/test dir", journal)
        command = remote.call_args_list[1].args[1]
        self.assertIn("cd /userdata", command)
        self.assertIn("if pidof mqtt_bridge", command)
        self.assertIn("[ ! -x /userdata/mqtt_bridge ]", command)
        self.assertIn("nohup /userdata/mqtt_bridge", command)
        self.assertIn("'/userdata/test dir/mqtt_bridge_startup.log'", command)
        self.assertIn("2>&1 < /dev/null &", command)
        self.assertEqual(result["pids"], [321])
        self.assertEqual(sleep.call_count, 2)
        self.assertEqual(journal.records[-1]["action"], "running_after_start_request")

    @patch("run.time.sleep")
    @patch("run.remote_exec", side_effect=["", "", "", "321", "321"])
    def test_delayed_start_is_allowed(self, remote, sleep):
        self.assertEqual(ensure_mqtt_bridge(object(), "/userdata/test", FakeJournal())["pids"], [321])

    @patch("run.time.sleep")
    @patch("run.remote_exec", side_effect=["", RuntimeError("missing or not executable")])
    def test_missing_executable_stops_test_without_polling(self, remote, sleep):
        with self.assertRaisesRegex(RuntimeError, "not executable"):
            ensure_mqtt_bridge(object(), "/userdata/test", FakeJournal())
        sleep.assert_not_called()

    @patch("run.time.sleep")
    @patch("run.remote_exec", side_effect=["", "", "321", "", "", "", ""])
    def test_immediate_exit_is_not_success(self, remote, sleep):
        journal = FakeJournal()
        with self.assertRaisesRegex(RuntimeError, "mqtt_bridge_startup.log"):
            ensure_mqtt_bridge(object(), "/userdata/test", journal)
        self.assertEqual(journal.records[-1]["action"], "startup_failed")

    @patch("run.time.sleep")
    @patch("run.remote_exec", side_effect=["", "", "321", "322", "323", "324", "325"])
    def test_restart_loop_is_not_stable_liveness(self, remote, sleep):
        with self.assertRaises(RuntimeError):
            ensure_mqtt_bridge(object(), "/userdata/test", FakeJournal())


if __name__ == "__main__":
    unittest.main()
