import json
import tempfile
import unittest
from pathlib import Path

from app import result_exit_code
from payload_tester.config import AppConfig
from payload_tester.models import CaseResult, Evidence, Verdict
from payload_tester.reporting import update_markdown_report
from payload_tester.runner import MissingDependency, PayloadTestRunner


class ReportingRunnerTests(unittest.TestCase):
    def test_capture_store_log_can_span_serial_reads(self):
        class Endpoint:
            def __init__(self):
                self.parts = iter(("SAT APP capture stored: gen=",
                                   "12 bytesPerAntenna=32768\n"))
            def collect(self, duration, markers=(), any_markers=()):
                return next(self.parts)

        class Tms:
            endpoint = Endpoint()

        runner = PayloadTestRunner(AppConfig(), Path("."))
        runner.hardware.tms570 = Tms()
        result = runner._wait_capture_store(11, 1.0)
        self.assertEqual(result["capture_generation"], 12)
        self.assertEqual(result["capture_bytes"], 32768)

    def test_sweep_store_log_can_span_serial_reads(self):
        class Endpoint:
            def __init__(self):
                self.parts = iter((
                    "SAT APP sweep stored: gen=7 points=8\n"
                    "SAT APP sweep stored: gen=8",
                    " points=8\n"))
            def collect(self, duration, markers=(), any_markers=()):
                return next(self.parts)

        class Tms:
            endpoint = Endpoint()

        runner = PayloadTestRunner(AppConfig(), Path("."))
        runner.config.test.sweep_point_count = 8
        runner.hardware.tms570 = Tms()
        result = runner._wait_sweep_stores(6, 2, 1.0)
        self.assertEqual(result["sweep_generations"], [7, 8])

    def test_acm_wait_uses_telemetry_factor_change(self):
        baseline = [{"i": 0x8000 if i == 0 else i, "q": i}
                    for i in range(8)]
        updated = [dict(item) for item in baseline]
        updated[3] = {"i": 0x1234, "q": 0x5678}

        class Spi:
            def collect_telemetry(self, count, timeout):
                return [{"acm": updated}, {"acm": updated}]

        runner = PayloadTestRunner(AppConfig(), Path("."))
        runner.hardware.spi = Spi()
        result = runner._wait_acm_factors_change(baseline, 1.0)
        self.assertTrue(result["changed"])
        self.assertEqual(result["factor_count"], 8)

    def test_acm_publish_log_can_span_serial_reads(self):
        class Endpoint:
            def __init__(self):
                self.parts = iter(("TRM: ACM result published to RAM ",
                                   "bank=1 generation=28 valid=1\n"))
            def collect(self, duration, markers=(), any_markers=()):
                return next(self.parts)

        class Tms:
            endpoint = Endpoint()

        runner = PayloadTestRunner(AppConfig(), Path("."))
        runner.hardware.tms570 = Tms()
        result = runner._wait_acm_publish(27, 1.0)
        self.assertEqual(result, {"generation": 28, "bank": 1, "valid": 1})

    def test_command_line_exit_code_fails_for_failed_or_blocked(self):
        def case(verdict):
            return CaseResult("RF-X", "case", verdict, "summary", "a", "b")

        self.assertEqual(result_exit_code([case(Verdict.PASS), case(Verdict.SKIP)]), 0)
        self.assertEqual(result_exit_code([case(Verdict.BLOCKED)]), 1)
        self.assertEqual(result_exit_code([case(Verdict.FAILED)]), 1)

    def test_preflight_telemetry_distinguishes_missing_clock_from_missing_miso(self):
        class TimeoutSpi:
            def collect_telemetry(self, count, timeout):
                raise TimeoutError("no telemetry")

        class Tms:
            def __init__(self, physical_rx): self.physical_rx = physical_rx
            def fpga_tm(self): return {"physical_rx": self.physical_rx}

        config = AppConfig()
        runner = PayloadTestRunner(config, Path("."))
        runner.hardware.spi = TimeoutSpi()
        runner.hardware.tms570 = Tms(10)
        with self.assertRaisesRegex(MissingDependency, "physicalRx未递增"):
            runner._collect_preflight_telemetry({"physical_rx": 10})

        runner.hardware.tms570 = Tms(11)
        with self.assertRaisesRegex(MissingDependency, "SOMI.*MISO"):
            runner._collect_preflight_telemetry({"physical_rx": 10})

    def test_case_data_isolation_clears_and_verifies_queue(self):
        class Tms:
            def __init__(self):
                self.snapshots = iter((
                    {"dt_pending": 1234, "dt_active": 0},
                    {"dt_pending": 104, "dt_active": 0},
                ))
                self.cleared = False

            def fpga_tm(self, timeout=15.0, attempts=3):
                return next(self.snapshots)

            def clear_data_transfer(self, timeout=15.0, attempts=3):
                self.cleared = True

        runner = PayloadTestRunner(AppConfig(), Path("."))
        tms = Tms()
        runner.hardware.tms570 = tms
        evidence = runner._clear_case_data("RF-02")
        self.assertTrue(tms.cleared)
        self.assertEqual(evidence.data["pending_before"], 1234)
        self.assertEqual(evidence.data["pending_after"], 104)
        self.assertEqual(evidence.data["active_after"], 0)

    def test_data_transfer_is_started_and_stopped_explicitly(self):
        class Spi:
            def __init__(self):
                self.commands = []

            def send_remote_control(self, command, value=None):
                self.commands.append((command, value))
                return {}

            def capture_data_transfer(self, max_frames, idle_timeout_ms,
                                      timeout=3600.0):
                return {"response": {}, "frames": [], "records": [],
                        "partial_record_bytes": 0}

        class Tms:
            def __init__(self):
                self.snapshots = iter((
                    {"dt_starts": 4, "dt_send_error": 0, "dt_active": 0},
                    {"dt_starts": 5, "dt_send_error": 0, "dt_active": 0},
                ))
                self.rc_commands = []

            def fpga_tm(self, timeout=15.0, attempts=3):
                return next(self.snapshots)

            def wait_rc(self, command, timeout=5.0):
                self.rc_commands.append(command)

        with tempfile.TemporaryDirectory() as directory:
            runner = PayloadTestRunner(AppConfig(), Path(directory))
            runner.run_dir = Path(directory)
            spi = Spi()
            tms = Tms()
            runner.hardware.spi = spi
            runner.hardware.tms570 = tms
            runner._start_data_transfer("switch")

            self.assertEqual(spi.commands, [(0x09, 1), (0x09, 0)])
            self.assertEqual(tms.rc_commands, [0x09, 0x09])

    def test_data_transfer_is_stopped_when_capture_fails(self):
        class Spi:
            def __init__(self):
                self.commands = []

            def send_remote_control(self, command, value=None):
                self.commands.append((command, value))

            def capture_data_transfer(self, max_frames, idle_timeout_ms,
                                      timeout=3600.0):
                raise TimeoutError("SPI2 idle timeout")

        class Tms:
            def fpga_tm(self, timeout=15.0, attempts=3):
                return {"dt_starts": 0, "dt_send_error": 0, "dt_active": 0}

            def wait_rc(self, command, timeout=5.0):
                pass

        runner = PayloadTestRunner(AppConfig(), Path("."))
        spi = Spi()
        runner.hardware.spi = spi
        runner.hardware.tms570 = Tms()
        with self.assertRaisesRegex(TimeoutError, "SPI2 idle timeout"):
            runner._start_data_transfer("timeout")
        self.assertEqual(spi.commands, [(0x09, 1), (0x09, 0)])

    def test_markdown_copy_fills_only_result(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / "source.md"; output = root / "output.md"
            source.write_text("### 3.6 测试\n\n| 编号 | 测试项 | 条件 | 步骤 | 预期 | 真实结果 | 判定 |\n| --- | --- | --- | --- | --- | --- | --- |\n| RF-01 | 参数 | 条件 | 步骤 | 预期 | 待填写 |  |\n", encoding="utf-8")
            result = CaseResult("RF-01", "参数", Verdict.PASS, "正确", "a", "b", [Evidence("SPI", "一致")])
            update_markdown_report(source, output, [result])
            self.assertIn("| 正确；一致 | PASS |", output.read_text(encoding="utf-8"))
            self.assertIn("待填写", source.read_text(encoding="utf-8"))

    def test_simulation_creates_all_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "docs").mkdir()
            template = root / "docs" / "report.md"
            template.write_text("### 3.6 测试\n| 编号 | 测试项 | 条件 | 步骤 | 预期 | 真实结果 | 判定 |\n| RF-01 | 参数 | 条件 | 步骤 | 预期 | 待填写 |  |\n", encoding="utf-8")
            config = AppConfig(); config.test.report_template = "docs/report.md"; config.test.selected_cases = ["RF-01", "RF-03"]
            runner = PayloadTestRunner(config, root, simulate=True)
            summary = runner.run()
            self.assertEqual([c.verdict for c in summary.cases], [Verdict.SKIP, Verdict.SKIP])
            self.assertTrue((runner.run_dir / "summary.json").exists())
            self.assertTrue((runner.run_dir / "report.xlsx").exists())
            self.assertTrue(list(runner.run_dir.glob("*.md")))

    def test_preflight_failure_becomes_blocked(self):
        class Broken(PayloadTestRunner):
            def _preflight(self): raise RuntimeError("fixture missing")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); config = AppConfig(); config.test.selected_cases = ["RF-01"]
            runner = Broken(config, root)
            summary = runner.run()
            self.assertEqual(summary.cases[0].verdict, Verdict.BLOCKED)

    def test_runner_pass_and_failed_branches(self):
        class PassRunner(PayloadTestRunner):
            def _preflight(self): pass
            def _case_rf_01(self, started):
                return self._result("RF-01", Verdict.PASS, "fixture pass", started=started)
        class FailRunner(PassRunner):
            def _case_rf_01(self, started): raise AssertionError("fixture fail")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); config = AppConfig(); config.test.selected_cases = ["RF-01"]
            self.assertEqual(PassRunner(config, root).run().cases[0].verdict, Verdict.PASS)
            self.assertEqual(FailRunner(config, root).run().cases[0].verdict, Verdict.FAILED)

    def test_case_timeout_is_enforced(self):
        class SlowRunner(PayloadTestRunner):
            def _preflight(self): pass
            def _case_rf_01(self, started):
                self._case_sleep(0.2)
                return self._result("RF-01", Verdict.PASS, "unexpected", started=started)

        with tempfile.TemporaryDirectory() as directory:
            config = AppConfig()
            config.test.selected_cases = ["RF-01"]
            config.test.case_timeout_s = 0.05
            result = SlowRunner(config, Path(directory)).run().cases[0]
            self.assertEqual(result.verdict, Verdict.FAILED)
            self.assertIn("0.05秒上限", result.summary)

    def test_rf06_checks_modes_a_b_c(self):
        class ModeRunner(PayloadTestRunner):
            def __init__(self):
                super().__init__(AppConfig(), Path("."), simulate=True)
                self.modes = []

            def _send_checked(self, command, value, expected_tm,
                              require_tms=True, verify_active=True):
                self.modes.append((command, value, expected_tm))
                return [Evidence("fixture", f"mode={value}")]

        runner = ModeRunner()
        result = runner._case_rf_06("start")
        self.assertEqual(result.verdict, Verdict.PASS)
        self.assertEqual(runner.modes, [
            (0x01, 1, {"mode": 1}),
            (0x01, 2, {"mode": 2}),
            (0x01, 3, {"mode": 3}),
        ])


if __name__ == "__main__": unittest.main()
