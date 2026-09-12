import csv
import json
import tempfile
import unittest
from pathlib import Path

from sync_network_test import (
    GatewayLogs,
    MqttPlatform,
    SlotCombination,
    Terminal,
    evaluate_combination,
    evaluate_packet,
    make_payload,
    parse_gateway_window,
    parse_applied_rate_config,
    validate_applied_rate_config,
    _redact_value,
    build_selection,
    sample_multi_rate_cases,
    sample_single_rate_cases,
    solve_period,
    terminal_frequency_slot_command,
    terminal_power_commands,
)
from run_cases_5_6 import _summarize_run


class SyncNetworkTest(unittest.TestCase):
    @staticmethod
    def _gateway_config() -> dict:
        return {
            "gw_id": "0499999999999999",
            "freq_major": 509_308_000,
            "freq_minor": 509_308_000,
            "nwk_num": 1,
            "tdd_num": 3,
            "rate_num": 1,
            "rate_cfgs": [{"rate": 5, "ul": 1, "dl": 2}],
        }

    def test_period_solver(self):
        self.assertEqual((1_750_000, 4), solve_period(1_622_431, 1))
        self.assertIsNone(solve_period(11_000_000, 1))

    def test_single_rate_case_filters_and_repeats_seed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "slots.csv"
            with path.open("w", encoding="utf-8", newline="") as target:
                writer = csv.DictWriter(target, fieldnames=(
                    "rate_mode", "super_frame_num", "ul_blocks", "dl_blocks",
                    "frame_period_us", "frame_count"))
                writer.writeheader()
                for rate in (5, 6, 7, 8, 9, 10, 11, 18):
                    for super_frame in (1, 2):
                        for value in range(1, 4):
                            writer.writerow({"rate_mode": rate, "super_frame_num": super_frame,
                                             "ul_blocks": value, "dl_blocks": value,
                                             "frame_period_us": 1_000_000,
                                             "frame_count": super_frame})
            case4_a = sample_single_rate_cases(path, 4, 2, 123)
            case4_b = sample_single_rate_cases(path, 4, 2, 123)
            case5 = sample_single_rate_cases(path, 5, 2, 123)
            self.assertEqual(case4_a, case4_b)
            self.assertTrue(all(item.super_frame_num == 1 for item in case4_a))
            self.assertTrue(all(item.super_frame_num > 1 for item in case5))

    def test_build_selection_filters_single_rate(self):
        repo_root = Path(__file__).resolve().parents[2]
        selected = build_selection(repo_root, {4}, 1, 123, (), {8})
        self.assertEqual(1, len(selected))
        self.assertEqual((8,), selected[0].rates)

    def test_single_rate_selection_obeys_ns_block_limits(self):
        repo_root = Path(__file__).resolve().parents[2]
        selected = build_selection(repo_root, {4}, 40, 20260908, ())
        self.assertEqual(320, len(selected))
        for item in selected:
            maximum = 16 if item.rates[0] in (9, 10, 11) else 10
            self.assertLessEqual(item.ul_blocks[0], maximum)
            self.assertLessEqual(item.dl_blocks[0], maximum)

    def test_gateway_lengths_encode_requested_wan_blocks(self):
        combination = SlotCombination(4, "C4", 1, (5, 9, 18),
                                      (1, 10, 10), (1, 16, 10),
                                      1_000_000, 1)
        self.assertEqual([
            {"rate_mode": 0, "uplink_len": 11, "downlink_len": 11},
            {"rate_mode": 4, "uplink_len": 245, "downlink_len": 401},
            {"rate_mode": 7, "uplink_len": 385, "downlink_len": 385},
        ], combination.gateway_rate_cfgs())

    def test_applied_gateway_rate_config_must_match_selection(self):
        text = (
            "Rate[0]: mode=5, brdBlocks=2, ulBlocks=1, dlBlocks=1\n"
            "Rate[0]: mode=5, brdBlocks=2, ulBlocks=7, dlBlocks=3\n"
        )
        parsed = parse_applied_rate_config(text, 1)
        combination = SlotCombination(4, "C4", 1, (5,), (7,), (3,), 4_000_000, 1)
        validate_applied_rate_config(combination, parsed)
        self.assertEqual(7, parsed[0]["ul"])
        with self.assertRaisesRegex(RuntimeError, "mismatch"):
            validate_applied_rate_config(
                SlotCombination(4, "C4", 1, (5,), (7,), (4,), 4_000_000, 1),
                parsed,
            )

    def test_multi_rate_generation(self):
        selected = sample_multi_rate_cases(((5, 6, 7, 8), (11, 18)), 40, 7)
        self.assertEqual(80, len(selected))
        self.assertEqual(80, len({(item.rates, item.ul_blocks, item.dl_blocks)
                                  for item in selected}))

    def test_parser_and_case5_verdict(self):
        receive = ("[INFO] TRM: RX users - rateMode=5, systemFrame=10, "
                   "superFrame=3, userCount=1, firstRssi=-72\n")
        strong = parse_gateway_window(receive + "[INFO] TRM: Sent 1/1 users with fixed power=31\n")
        weak = parse_gateway_window(receive.replace("firstRssi=-72", "firstRssi=-90"))
        combination = SlotCombination(5, "C5", 20, (5,), (1,), (1,), 1_000_000, 20)
        uplinks = {
            "GW1": {"rssi": -60},
            "GW2": {"rssi": -80},
        }
        verdict = evaluate_packet(5, combination, ["GW1", "GW2"], uplinks,
                                  {"txstatus7": True, "rx_data": True},
                                  {"GW1": strong, "GW2": weak})
        self.assertTrue(verdict["passed"], verdict)
        self.assertEqual(-72, strong["last_rssi"])

    def test_case6_rate_mismatch_fails(self):
        combination = SlotCombination(6, "C6", 1, (7, 8), (1, 1), (1, 1), 1_000_000, 1)
        windows = {
            "GW1": {"receives": [{"rate": 7, "super": 0}], "sent_ack": True},
            "GW2": {"receives": [{"rate": 8, "super": 0}], "sent_ack": False},
        }
        verdict = evaluate_packet(6, combination, ["GW1", "GW2"],
                                  {"GW1": {"rssi": -50}, "GW2": {"rssi": -70}},
                                  {"txstatus7": True, "rx_data": True}, windows)
        self.assertIn("gateway_rate_mismatch", verdict["reasons"])

    def test_case6_same_nonlowest_rate_passes_but_ack_still_required(self):
        combination = SlotCombination(6, "C6", 1, (7, 8), (1, 1), (1, 1), 1_000_000, 1)
        windows = {
            "GW1": {"receives": [{"rate": 8, "super": 0}], "sent_ack": True},
            "GW2": {"receives": [{"rate": 8, "super": 0}], "sent_ack": False},
        }
        for ack in (True, False):
            verdict = evaluate_packet(6, combination, ["GW1", "GW2"],
                {"GW1": {"rssi": -50}, "GW2": {"rssi": -70}},
                {"txstatus7": ack, "rx_data": ack}, windows)
            self.assertEqual(verdict["passed"], ack)
            self.assertEqual(verdict["reasons"], [] if ack else ["terminal_ack_missing"])

    def test_combination_passes_at_seventy_percent(self):
        passed = {"verdict": {"passed": True}}
        failed = {"verdict": {"passed": False}}
        result = evaluate_combination([passed] * 8 + [failed] * 2, 10, 0.70)
        self.assertTrue(result["passed"])
        self.assertEqual(8, result["passed_packet_count"])
        self.assertEqual(0.8, result["success_rate"])

        boundary = evaluate_combination([passed] * 7 + [failed] * 3, 10, 0.70)
        self.assertTrue(boundary["passed"])
        self.assertEqual(0.7, boundary["success_rate"])
        self.assertFalse(evaluate_combination(
            [passed] * 6 + [failed] * 4, 10, 0.70)["passed"])

    def test_incomplete_combination_fails_even_with_high_success_rate(self):
        passed = {"verdict": {"passed": True}}
        result = evaluate_combination([passed] * 9, 10, 0.70)
        self.assertFalse(result["passed"])
        self.assertEqual(0.9, result["success_rate"])

    def test_combination_rejects_invalid_threshold(self):
        with self.assertRaisesRegex(ValueError, "success_threshold"):
            evaluate_combination([], 10, 1.0)

    def test_sequential_summary_reports_combination_and_packet_rates(self):
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            records = [
                {"passed": True, "passed_packet_count": 8,
                 "expected_packet_count": 10},
                {"passed": False, "passed_packet_count": 7,
                 "expected_packet_count": 10},
            ]
            (run_dir / "summary.json").write_text(
                json.dumps({"records": records}), encoding="utf-8")
            result = _summarize_run(run_dir)
            self.assertEqual(1, result["passed_combinations"])
            self.assertEqual(0.5, result["combination_success_rate"])
            self.assertEqual(15, result["passed_packets"])
            self.assertEqual(0.75, result["packet_success_rate"])

    def test_ns_deduplicated_uplink_uses_gateway_log_rssi(self):
        combination = SlotCombination(4, "C4", 1, (8,), (1,), (1,), 1_000_000, 1)
        windows = {
            "GW1": {"receives": [{"rate": 8, "super": 1}],
                    "last_rssi": -110, "sent_ack": False},
            "GW2": {"receives": [{"rate": 8, "super": 1}],
                    "last_rssi": -90, "sent_ack": True},
        }
        verdict = evaluate_packet(4, combination, ["GW1", "GW2"],
                                  {"GW2": {"rssi": -90}},
                                  {"txstatus7": True, "rx_data": True}, windows)
        self.assertTrue(verdict["passed"], verdict)
        self.assertEqual("GW2", verdict["strongest_gateway"])

    def test_payload_is_unique_and_sized(self):
        first = make_payload(4, 1, 1, 30)
        second = make_payload(4, 1, 2, 30)
        self.assertEqual(60, len(first))
        self.assertNotEqual(first, second)

    def test_terminal_disables_power_control_before_setting_max_tx_power(self):
        self.assertEqual(["AT+PWRCTRL=0", "AT+TXP=15"],
                         terminal_power_commands({}))
        with self.assertRaisesRegex(ValueError, "power_control"):
            terminal_power_commands({"power_control": 2})
        with self.assertRaisesRegex(ValueError, "tx_power"):
            terminal_power_commands({"tx_power": 16})

    def test_terminal_uses_fixed_frequency_slot_before_join(self):
        config = {"fixed_frequency_slot": [496800000, 496820000, 496828000, 1]}
        self.assertEqual(
            "AT+FREQSLOT=496800000,496820000,496828000,1",
            terminal_frequency_slot_command(config),
        )
        with self.assertRaisesRegex(ValueError, "four integers"):
            terminal_frequency_slot_command({})
        with self.assertRaisesRegex(ValueError, "values are invalid"):
            terminal_frequency_slot_command({"fixed_frequency_slot": [1, 2, 3, 2]})

        class FakeHandle:
            def __init__(self):
                self.writes = []

            def write(self, data):
                self.writes.append(data.decode("ascii").strip())

            def flush(self):
                pass

        terminal = Terminal.__new__(Terminal)
        terminal.config = {
            **config,
            "dev_eui": "0000000000000003",
            "freq_major": 7,
            "freq_minor": 6,
            "dev_mode": 0,
            "security_mode": 0,
            "root_key": "00112233445566778899AABBCCDDEEFF",
            "power_control": 0,
            "tx_power": 15,
            "reset_before_combination": False,
        }
        terminal.handle = FakeHandle()
        commands = []
        terminal.command = lambda command, **_kwargs: commands.append(command)
        terminal.transcript = lambda *_args: None
        terminal._read_until = lambda *_args: ["+NWKINFO:4"]
        terminal._configure_and_join_once(5)
        self.assertLess(
            commands.index("AT+FREQSLOT=496800000,496820000,496828000,1"),
            commands.index("AT+RATE=5"),
        )
        self.assertLess(
            commands.index("AT+SENDPOL=0,0"),
            commands.index("AT+PRINTMODE=MAC,1"),
        )
        self.assertEqual(1, commands.count("AT+SENDPOL=0,0"))
        self.assertEqual(["AT+JOIN=0"], terminal.handle.writes)

    def test_terminal_send_returns_immediately_on_explicit_send_failure(self):
        class FakeHandle:
            def __init__(self):
                self.writes = []

            def write(self, data):
                self.writes.append(data.decode("ascii").strip())

            def flush(self):
                pass

        terminal = Terminal.__new__(Terminal)
        terminal.config = {"ack_timeout_seconds": 120}
        terminal.handle = FakeHandle()
        terminal.transcript = lambda *_args: None
        terminal._read = lambda *_args: []
        observed_stop_tokens = []

        def read_until(_timeout, stop_tokens):
            observed_stop_tokens.extend(stop_tokens)
            return ["+TXSTATUS:8"]

        terminal._read_until = read_until
        result = terminal.send_confirmed("A55A")
        self.assertIn("+TXSTATUS:8", observed_stop_tokens)
        self.assertTrue(result["txstatus8"])
        self.assertFalse(result["txstatus7"])
        self.assertFalse(result["rx_data"])
        self.assertEqual(["AT+SENDB=0,1,0,1,A55A"], terminal.handle.writes)

    def test_managed_gateway_can_stop_and_restart_with_separate_stdout(self):
        gateway = GatewayLogs.__new__(GatewayLogs)
        gateway.config = {
            "log_mode": "manage",
            "start_command": "/userdata/tk8710_gw --trm-log-level info",
            "gateway_start_wait_seconds": 0,
        }
        gateway.remote_run_dir = "/userdata/test_run"
        gateway.managed_pid = "101"
        gateway.managed_launch_count = 1
        gateway.managed_stdout_paths = ["/userdata/test_run/stdout.log"]
        commands = []

        def execute(command):
            commands.append(command)
            if "start-stop-daemon" in command:
                return "202"
            return ""

        gateway._exec = execute
        gateway.stop_managed()
        self.assertIsNone(gateway.managed_pid)
        gateway.restart_managed()
        self.assertEqual("202", gateway.managed_pid)
        self.assertEqual(2, gateway.managed_launch_count)
        self.assertEqual(
            "/userdata/test_run/stdout.restart_01.log",
            gateway.managed_stdout_paths[-1])
        self.assertTrue(any("kill -TERM 101" in command for command in commands))
        self.assertTrue(any("gateway.restart_01.pid" in command for command in commands))

    def test_terminal_configuration_retries_transient_failure(self):
        class FakeHandle:
            def __init__(self):
                self.input_resets = 0
                self.output_resets = 0

            def reset_input_buffer(self):
                self.input_resets += 1

            def reset_output_buffer(self):
                self.output_resets += 1

        terminal = Terminal.__new__(Terminal)
        terminal.config = {"configuration_attempts": 3}
        terminal.handle = FakeHandle()
        messages = []
        terminal.transcript = lambda source, message: messages.append((source, message))
        reconnects = []
        terminal.close = lambda: reconnects.append("close")
        terminal.open = lambda: reconnects.append("open")
        calls = []

        def configure_once(rate):
            calls.append(rate)
            if len(calls) < 3:
                raise TimeoutError("transient")
            return "joined"

        terminal._configure_and_join_once = configure_once
        self.assertEqual("joined", terminal.configure_and_join(8))
        self.assertEqual([8, 8, 8], calls)
        self.assertEqual(["close", "open", "close", "open"], reconnects)
        self.assertEqual(0, terminal.handle.input_resets)
        self.assertEqual(0, terminal.handle.output_resets)
        self.assertEqual(2, len(messages))

    def test_credentials_are_redacted(self):
        value = _redact_value({"root_key": "001122", "line": "AT+SEC=0,001122"})
        self.assertEqual("***", value["root_key"])
        self.assertEqual("AT+SEC=0,***", value["line"])

    def test_terminal_root_key_is_required(self):
        terminal = Terminal.__new__(Terminal)
        terminal.config = {
            "dev_eui": "0000000000000001",
            "freq_major": 0,
            "freq_minor": 0,
            "fixed_frequency_slot": [1, 1, 1, 0],
            "reset_before_combination": False,
        }
        terminal.command = lambda *_args, **_kwargs: None
        with self.assertRaisesRegex(ValueError, "root_key"):
            terminal._configure_and_join_once(8)

    def test_gateway_configuration_readback_is_strict(self):
        expected = self._gateway_config()
        platform = MqttPlatform.__new__(MqttPlatform)
        responses = iter((
            {"rsp_body": []},
            {"rsp_body": []},
            {"rsp_body": [{**expected, "tdd_num": 4}]},
        ))
        platform.request = lambda _operation, _body: next(responses)
        with self.assertRaisesRegex(RuntimeError, "readback tdd_num mismatch"):
            platform.replace_gateways([expected])

    def test_terminal_readback_accepts_dev_mode_alias(self):
        terminal = {
            "dev_eui": "0000000000000003",
            "dev_type": 0,
            "security_mode": 0,
            "root_key": "00112233445566778899AABBCCDDEEFF",
            "related_id": "",
        }
        platform = MqttPlatform.__new__(MqttPlatform)
        responses = iter((
            {"rsp_body": []},
            {"rsp_body": []},
            {"rsp_body": [{
                "dev_eui": terminal["dev_eui"],
                "dev_mode": 0,
                "security_mode": 0,
                "root_key": "",
                "related_id": "",
            }]},
        ))
        platform.request = lambda _operation, _body: next(responses)
        result = platform.replace_terminal(terminal)
        self.assertEqual(terminal["dev_eui"], result["after"]["rsp_body"][0]["dev_eui"])


if __name__ == "__main__":
    unittest.main()
