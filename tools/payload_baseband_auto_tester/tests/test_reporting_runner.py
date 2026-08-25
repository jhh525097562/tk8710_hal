import json
import tempfile
import unittest
from pathlib import Path

from payload_tester.config import AppConfig
from payload_tester.models import CaseResult, Evidence, Verdict
from payload_tester.reporting import update_markdown_report
from payload_tester.runner import PayloadTestRunner


class ReportingRunnerTests(unittest.TestCase):
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


if __name__ == "__main__": unittest.main()
