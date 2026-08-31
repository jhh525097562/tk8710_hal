from __future__ import annotations

import argparse
import subprocess
import sys
import unittest
from pathlib import Path

from payload_tester.config import load_config
from payload_tester.gui import run_gui
from payload_tester.runner import PayloadTestRunner


ROOT = Path(__file__).resolve().parent


def _configure_console_output() -> None:
    # Windows GBK控制台无法编码串口乱码解码产生的U+FFFD。日志文件仍保留
    # UTF-8原文，命令行显示使用替换策略，不能让显示异常中止硬件预检。
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="replace")


def main() -> int:
    _configure_console_output()
    parser = argparse.ArgumentParser(description="载荷基带软件3.6自动测试工具")
    parser.add_argument("--config", help="JSON配置文件")
    parser.add_argument("--run-all", action="store_true", help="命令行运行配置中的全部用例")
    parser.add_argument("--cases", help="逗号分隔用例，例如RF-01,RF-02")
    parser.add_argument("--simulate", action="store_true", help="不连接硬件，验证完整编排和报告")
    parser.add_argument("--selftest", action="store_true", help="运行Python和C桥接自测")
    args = parser.parse_args()
    if args.selftest:
        suite = unittest.defaultTestLoader.discover(str(ROOT / "tests"))
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        bridge = ROOT / "bridge" / "jtool_spi_bridge.exe"
        bridge_ok = bridge.exists() and subprocess.run([str(bridge), "selftest"], cwd=bridge.parent).returncode == 0
        return 0 if result.wasSuccessful() and bridge_ok else 1
    config = load_config(args.config)
    if args.cases: config.test.selected_cases = [item.strip().upper() for item in args.cases.split(",") if item.strip()]
    if args.run_all or args.cases or args.simulate:
        runner = PayloadTestRunner(config, ROOT, lambda source, text: print(f"[{source}] {text}"), args.simulate)
        result = runner.run()
        print(f"结果目录: {runner.run_dir}")
        return 1 if any(case.verdict.value == "FAILED" for case in result.cases) else 0
    run_gui(config, ROOT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
