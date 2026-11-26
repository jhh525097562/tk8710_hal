import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location("wakeup_run", Path(__file__).with_name("run.py"))
run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run)


class Clock:
    def __init__(self):
        self.now = 0

    def sleep(self, seconds):
        self.now += seconds


class Tests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(Path(__file__).with_name("config.example.json").read_text())
        self.config["ids"] = list(range(256))
        self.case = {"case": "m3_id166_r1", "mode": 3, "id": 166, "repeat": 1}

    def test_all_768_unique_cases(self):
        run.validate(self.config)
        plan = list(run.cases(self.config))
        self.assertEqual(len(plan), 768)
        self.assertEqual(len({c["case"] for c in plan}), 768)
        self.assertEqual(plan[-1]["id"], 255)

    def test_input_boundaries(self):
        for key, value in [("ids", [256]), ("ids", [-1]), ("ids", [1, 1]),
                           ("modes", [0]), ("duration_ms", 0), ("duration_ms", 0x1000000),
                           ("frequency_hz", 0), ("serial_timeout", 0), ("rate_mode", 12),
                           ("timer_wakeup_ms", 0), ("timer_wakeup_ms", 1000),
                           ("timer_guard_ms", 5000), ("recovery_timeout", "bad")]:
            config = copy.deepcopy(self.config)
            config[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                run.validate(config)

    def terminal_for_lines(self, lines):
        terminal = object.__new__(run.WakeTerminal)
        terminal.check = Mock()
        terminal.condition = threading.Condition()
        terminal.sequence = 10
        terminal.journal = Mock()
        terminal.device = Mock()
        terminal.wait = Mock(return_value=lines)
        return terminal

    def test_freq_payload_and_ok_required(self):
        for lines, expected in [(["+FREQ:471200000,471200000", "AT_OK"], True),
                                (["AT+FREQ?", "AT_OK"], False), (["j", "AT_OK"], False),
                                (["+FREQ:471200000", "AT_ERROR"], False)]:
            with self.subTest(lines=lines):
                terminal = self.terminal_for_lines(lines)
                self.assertEqual(terminal.awake(.7), expected)
                terminal.device.write.assert_called_with(b"AT+FREQ?\r\n")

    def test_query_timeout_is_not_success(self):
        terminal = self.terminal_for_lines([])
        terminal.wait.side_effect = TimeoutError()
        self.assertFalse(terminal.awake(.7))

    def test_sleep_commands_and_negative_probes(self):
        terminal = Mock()
        terminal.awake.side_effect = [True, False, False]
        with patch.object(run.time, "sleep"):
            run.prepare_sleep(terminal, self.config, self.case)
        self.assertEqual([c.args[0] for c in terminal.command.call_args_list],
                         ["AT+WORKMODE=21", "AT+WAKEUPCFG=8,5000;9,166,1000,492300000,3",
                          "AT+ENTERSLEEP"])

    def test_terminal_not_sleeping_aborts(self):
        terminal = Mock()
        terminal.awake.side_effect = [True, True]
        with patch.object(run.time, "sleep"), self.assertRaisesRegex(RuntimeError, "仍能查询"):
            run.prepare_sleep(terminal, self.config, self.case)

    def test_early_response_waits_full_tx_window(self):
        clock = Clock()
        clock.now = .6
        tx, terminal = Mock(), Mock()
        tx.send.side_effect = lambda case: clock.now
        terminal.awake.return_value = True
        tx.finish_signal.side_effect = lambda: self.assertGreaterEqual(clock.now, 5.6)
        with patch.object(run, "prepare_sleep", return_value=0.0), \
                patch.object(run, "wait_terminal_ready"), \
                patch.object(run.time, "monotonic", lambda: clock.now), \
                patch.object(run.time, "sleep", clock.sleep):
            result = run.run_attempt(tx, terminal, self.config, self.case)
        self.assertEqual(result["status"], "PASS")
        tx.finish_signal.assert_called_once()
        tx.stop_signal.assert_not_called()

    def test_timer_response_does_not_count_as_wireless_pass(self):
        clock = Clock()
        clock.now = .6
        tx, terminal = Mock(), Mock()
        tx.send.side_effect = lambda case: clock.now
        def awake(timeout):
            clock.sleep(timeout)
            return clock.now >= 5
        terminal.awake.side_effect = awake
        stopped = []
        tx.stop_signal.side_effect = lambda: stopped.append(clock.now)
        with patch.object(run, "prepare_sleep", return_value=0.0), \
                patch.object(run.time, "monotonic", lambda: clock.now), \
                patch.object(run.time, "sleep", clock.sleep):
            result = run.run_attempt(tx, terminal, self.config, self.case)
        self.assertEqual(result["status"], "WAKE_TIMEOUT")
        self.assertIsNone(result["query_latency_s"])
        self.assertAlmostEqual(stopped[0], 4.8)
        self.assertGreaterEqual(clock.now, 5)
        tx.finish_signal.assert_not_called()

    def test_response_crossing_cutoff_is_not_pass(self):
        clock = Clock()
        clock.now = 4.7
        tx, terminal = Mock(), Mock()
        tx.send.side_effect = lambda case: clock.now
        terminal.awake.side_effect = lambda timeout: (clock.sleep(.3) or True)
        with patch.object(run, "prepare_sleep", return_value=0.0), \
                patch.object(run, "wait_terminal_ready"), \
                patch.object(run.time, "monotonic", lambda: clock.now), \
                patch.object(run.time, "sleep", clock.sleep):
            result = run.run_attempt(tx, terminal, self.config, self.case)
        self.assertEqual(result["status"], "WAKE_TIMEOUT")

    def test_three_failed_attempts_then_final_timeout(self):
        tx, terminal = Mock(), Mock()
        def failed(t, term, config, case):
            return {**case, "status": "WAKE_TIMEOUT", "query_latency_s": None}
        with patch.object(run, "wait_terminal_ready"), \
                patch.object(run, "run_attempt", side_effect=failed) as attempt:
            result = run.run_case(tx, terminal, self.config, self.case)
        self.assertEqual(result["status"], "WAKE_TIMEOUT")
        self.assertEqual(result["attempt_count"], 3)
        self.assertEqual(attempt.call_count, 3)
        self.assertEqual([r["attempt"] for r in result["attempts"]], [1,2,3])

    def test_retry_success_stops_after_second_attempt(self):
        tx, terminal = Mock(), Mock()
        def outcome(t, term, config, case):
            return {**case, "status": "PASS" if case["attempt"] == 2 else "WAKE_TIMEOUT",
                    "query_latency_s": .3 if case["attempt"] == 2 else None}
        with patch.object(run, "wait_terminal_ready"), \
                patch.object(run, "run_attempt", side_effect=outcome) as attempt:
            result = run.run_case(tx, terminal, self.config, self.case)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["attempt_count"], 2)
        self.assertEqual(attempt.call_count, 2)

    def test_three_failures_advance_next_id_and_persist_all_attempts(self):
        config = copy.deepcopy(self.config)
        config.update(modes=[3], ids=[47,48], serial_port="COM_TEST_NO_IO")
        tx, client, terminal = Mock(), Mock(), Mock()
        def transmitter(c, remote, cfg, journal):
            tx.journal = journal
            return tx
        def outcome(t, term, cfg, case):
            return {**case, "status": "WAKE_TIMEOUT" if case["id"] == 47 else "PASS",
                    "query_latency_s": None if case["id"] == 47 else .3}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"config.json"
            path.write_text(json.dumps(config))
            output = Path(directory)/"result"
            output.mkdir()
            snapshot = {**config, "local_bin": str((path.parent/config["local_bin"]).resolve())}
            (output/"config.json").write_text(json.dumps(snapshot))
            with patch.object(run, "connect", return_value=(client,"/remote")), \
                    patch.object(run, "WakeTerminal", return_value=terminal), \
                    patch.object(run, "Transmitter", side_effect=transmitter), \
                    patch.object(run, "download_logs"), patch.object(run, "wait_terminal_ready"), \
                    patch.object(run, "run_attempt", side_effect=outcome) as attempt:
                self.assertEqual(run.main(["--config",str(path),"--resume",str(output)]),1)
            self.assertEqual([c.args[3]["id"] for c in attempt.call_args_list], [47,47,47,48])
            rows = [json.loads(line) for line in (output/"results.jsonl").read_text().splitlines()]
            self.assertEqual([r["attempt_count"] for r in rows], [3,1])
            import csv
            with (output/"attempts.csv").open(encoding="utf-8-sig") as file:
                self.assertEqual(len(list(csv.DictReader(file))),4)
            tx.start.assert_called_once()
            tx.stop.assert_called_once()

    def test_missing_timer_recovery_stops_without_manual_prompt(self):
        clock = Clock()
        terminal = Mock()
        terminal.awake.side_effect = lambda timeout: (clock.sleep(timeout) and False)
        with patch.object(run.time, "monotonic", lambda: clock.now), \
                patch.object(run.time, "sleep", clock.sleep), self.assertRaises(RuntimeError):
            run.wait_terminal_ready(terminal, self.config)
        self.assertLessEqual(clock.now, 13.01)

    def test_stop_timeout_is_fatal_and_channel_closed(self):
        tx = run.Transmitter(Mock(), "/test", self.config, Mock())
        tx.channel = Mock()
        channel = tx.channel
        channel.exit_status_ready.return_value = False
        clock = Clock()
        with patch.object(run.time, "monotonic", lambda: clock.now), \
                patch.object(run.time, "sleep", clock.sleep), self.assertRaises(RuntimeError):
            tx.stop()
        channel.close.assert_called_once()
        self.assertIsNone(tx.channel)

    def test_ssh_input_and_success_marker(self):
        tx = run.Transmitter(Mock(), "/remote", self.config, Mock())
        tx.channel = Mock()
        tx.wait = Mock()
        tx.send(self.case)
        self.assertEqual([call.args[0] for call in tx.channel.sendall.call_args_list],
                         [b"u\n", b"3 5000 166\n"])
        self.assertEqual(tx.wait.call_args.args[0], "WAKEUP_STARTED ")

    def test_connect_copies_board_calibration(self):
        import paramiko
        config = copy.deepcopy(self.config)
        client, journal = Mock(), Mock()
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "Test8710RFTxTone"
            binary.write_bytes(b"\x7fELF-test-stub")
            config["local_bin"] = str(binary)
            sftp = Mock()
            client.open_sftp.return_value.__enter__ = Mock(return_value=sftp)
            client.open_sftp.return_value.__exit__ = Mock(return_value=None)
            def remote(c, command):
                if command.startswith("sha256sum"):
                    return ("calibration-hash" if "/TxDC/" in command else
                            hashlib.sha256(binary.read_bytes()).hexdigest()) + "  file\n"
                return ""
            with patch.object(paramiko, "SSHClient", return_value=client), \
                    patch.object(run, "remote_exec", side_effect=remote) as execute:
                connected, path = run.connect(config, Path(directory), journal)
            commands = [c.args[1] for c in execute.call_args_list]
            self.assertTrue(any("cp /userdata/TxDC/txadc.txt" in command for command in commands))
            self.assertTrue(any("TxDC/txadc.txt" in command and "sha256sum" in command for command in commands))
            self.assertEqual(connected, client)
            self.assertTrue(path.startswith("/userdata/wakeup_test_"))

    def test_ssh_timeout_identifies_target_and_stage(self):
        import paramiko
        client, journal = Mock(), Mock()
        client.connect.side_effect = TimeoutError("timed out")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory)/"Test8710RFTxTone"
            binary.write_bytes(b"\x7fELF-test")
            config = {**self.config, "local_bin":str(binary)}
            with patch.object(paramiko, "SSHClient", return_value=client), \
                    self.assertRaisesRegex(RuntimeError, "SSH连接超时.*192.168.100.78:22"):
                run.connect(config,Path(directory),journal)
            client.close.assert_called_once()
            self.assertEqual(journal.emit.call_args.args[0],"prepare_failed")
            self.assertEqual(journal.emit.call_args.kwargs["stage"],"SSH连接")

    def test_split_utf8_ssh_output_and_exit(self):
        tx = run.Transmitter(Mock(), "/remote", self.config, Mock())
        tx.channel = Mock()
        data = "唤醒信号发送配置成功:\n".encode()
        tx.channel.recv.side_effect = [data[:2], data[2:5], data[5:], b""]
        tx._read()
        self.assertIn("唤醒信号发送配置成功:", tx.wait("唤醒信号发送配置成功:", 0, 1))
        with self.assertRaises(RuntimeError):
            tx.wait("不存在的提示", 0, 1)

    def test_campaign_persists_timeout_then_resume_skips_it(self):
        config = copy.deepcopy(self.config)
        config.update(modes=[3], ids=[166, 167], serial_port="COM_TEST_NO_IO")
        terminal, client = Mock(), Mock()
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.json"
            config_path.write_text(json.dumps(config))
            output = Path(directory) / "result"
            output.mkdir()
            snapshot = copy.deepcopy(config)
            snapshot["local_bin"] = str((config_path.parent / config["local_bin"]).resolve())
            (output / "config.json").write_text(json.dumps(snapshot))
            tx = Mock()
            def fake_case(transmitter, term, cfg, case):
                return {**case, "status": "WAKE_TIMEOUT" if case["id"] == 166 else "PASS"}
            with patch.object(run, "connect", return_value=(client, "/remote")), \
                    patch.object(run, "WakeTerminal", return_value=terminal), \
                    patch.object(run, "Transmitter", return_value=tx), \
                    patch.object(run, "download_logs"), \
                    patch.object(run, "run_case", side_effect=fake_case) as case_runner:
                self.assertEqual(run.main(["--config", str(config_path), "--resume", str(output)]), 1)
                self.assertEqual(case_runner.call_count, 2)
                tx.start.assert_called_once()
                tx.stop.assert_called_once()
                case_runner.reset_mock()
                self.assertEqual(run.main(["--config", str(config_path), "--resume", str(output)]), 1)
                case_runner.assert_not_called()
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["counts"], {"PASS": 1, "WAKE_TIMEOUT": 1, "ERROR": 0})
            self.assertTrue((output / "results.csv").exists())

    def test_two_ids_wait_driver_completion_without_p_or_q(self):
        tx = run.Transmitter(Mock(), "/remote", self.config, Mock())
        tx.channel = Mock()
        tx.wait = Mock(return_value="WAKEUP_FINISHED state=3 result=0 stopped=1 elapsed_us=5001000\n")
        for wid in (31, 32):
            tx.send({**self.case, "id": wid})
            tx.finish_signal()
            tx.stop_signal()  # Completed operation needs no cancel.
        self.assertEqual([c.args[0] for c in tx.channel.sendall.call_args_list],
                         [b"u\n", b"3 5000 31\n", b"u\n", b"3 5000 32\n"])

    def test_stop_signal_failure_blocks_next_id(self):
        tx = run.Transmitter(Mock(), "/remote", self.config, Mock())
        tx.channel = Mock()
        tx.transmitting = True
        tx.completion_start = 0
        tx.wait = Mock(side_effect=TimeoutError("no stop acknowledgment"))
        with self.assertRaises(TimeoutError):
            tx.stop_signal()
        with self.assertRaises(RuntimeError):
            tx.send(self.case)


if __name__ == "__main__":
    unittest.main()
