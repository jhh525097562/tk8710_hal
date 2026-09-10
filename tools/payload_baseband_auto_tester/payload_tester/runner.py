from __future__ import annotations

import json
import re
import threading
import time
import traceback
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

from .config import AppConfig
from .models import CaseResult, Evidence, RunSummary, Verdict
from .mqtt_ns import NsMqtt
from .protocol import DataRecord, validate_capture, validate_sweep
from .reporting import (RunLogger, create_run_dir, now_iso, update_markdown_report,
                        write_excel, write_summary)
from .serials import (PortDiscovery, SerialEndpoint, TerminalConsole, Tms570Console,
                      parallel_call)
from .spi_bridge import SpiBridge


CASE_NAMES = {
    "RF-01": "参数配置", "RF-02": "底噪检测", "RF-03": "单tone模式",
    "RF-04": "ACM校准", "RF-05": "信号采集", "RF-06": "模式A/B/C",
    "RF-07": "单用户/速率0双向收发", "RF-08": "单用户/速率1双向收发",
    "RF-09": "单用户/速率2双向收发", "RF-10": "16用户/速率0双向收发",
    "RF-11": "16用户/速率1双向收发", "RF-12": "16用户/速率2双向收发",
    "RF-13": "稳定性测试（16用户/速率2）",
}

RATE_MAP = {0: (6, 1), 1: (7, 2), 2: (8, 3)}


class MissingDependency(RuntimeError):
    pass


@dataclass
class Hardware:
    spi: Optional[SpiBridge] = None
    tms570: Optional[Tms570Console] = None
    terminals: List[TerminalConsole] = field(default_factory=list)
    dev_euis: List[str] = field(default_factory=list)
    mqtt: Optional[NsMqtt] = None
    discovered: Dict[str, Any] = field(default_factory=dict)


class PayloadTestRunner:
    def __init__(self, config: AppConfig, tool_root: Path,
                 output_sink: Optional[Callable[[str, str], None]] = None,
                 simulate: bool = False):
        self.config = config
        self.root = tool_root.resolve()
        self.output_sink = output_sink or (lambda source, text: None)
        self.simulate = simulate
        self.stop_event = threading.Event()
        self.hardware = Hardware()
        self.logger: Optional[RunLogger] = None
        self.summary: Optional[RunSummary] = None
        self.run_dir: Optional[Path] = None
        self._case_deadline: Optional[float] = None

    def stop(self) -> None:
        self.stop_event.set()

    def _remaining_case_time(self, requested: float) -> float:
        """Limit a blocking operation to the active case's remaining budget."""
        if self._case_deadline is None:
            return requested
        remaining = self._case_deadline - time.monotonic()
        if remaining <= 0.0:
            raise TimeoutError(
                f"用例执行超过{self.config.test.case_timeout_s:g}秒上限")
        return min(requested, max(0.05, remaining))

    def _case_sleep(self, duration: float) -> None:
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            remaining = self._remaining_case_time(deadline - time.monotonic())
            time.sleep(min(0.2, remaining))

    def _fpga_tm(self, total_timeout: float = 15.0) -> Dict[str, Any]:
        """Query 570 status with short retries inside one fixed time budget."""
        tms = self._need_tms()
        budget = self._remaining_case_time(total_timeout)
        attempts = 3
        return tms.fpga_tm(max(0.5, budget / attempts), attempts)

    def _log(self, source: str, text: str, **data: Any) -> None:
        if self.logger: self.logger.log(source, text, **data)
        else: self.output_sink(source, text)

    def _bridge_event(self, event: Dict[str, Any]) -> None:
        clean = dict(event)
        if "frame_hex" in clean: clean["frame_hex"] = f"<{len(clean['frame_hex']) // 2} bytes>"
        self._log("spi_bridge", json.dumps(clean, ensure_ascii=False))

    def _collect_preflight_telemetry(self, baseline: Dict[str, Any]) -> List[Dict[str, Any]]:
        spi = self.hardware.spi
        if spi is None:
            raise MissingDependency("SPI桥接未初始化")
        try:
            return spi.collect_telemetry(2, 12)
        except TimeoutError as exc:
            tms = self.hardware.tms570
            if tms is None:
                raise
            try:
                after = tms.fpga_tm()
            except Exception:
                raise exc
            before_rx = int(baseline.get("physical_rx", 0))
            after_rx = int(after.get("physical_rx", 0))
            if after_rx == before_rx:
                raise MissingDependency(
                    "SPI1遥测超时且570 physicalRx未递增：检查SPI1适配器SN、CS、CLK、MOSI和共地"
                ) from exc
            raise MissingDependency(
                f"SPI1遥测超时，但570 physicalRx已从{before_rx}增至{after_rx}："
                "写入链路正常，检查TMS570 SOMI到JTool MISO返回线及3.3V电平"
            ) from exc

    def _preflight(self) -> None:
        if self.simulate:
            self.hardware.discovered = {"simulation": True, "tms570": "SIM570", "terminals": ["SIMTERM"]}
            return
        discovery = PortDiscovery(self.config.serial.tms570_baudrate,
                                  self.config.serial.terminal_baudrate,
                                  line_sink=self.logger.serial_sink if self.logger else None,
                                  probe_timeout_s=self.config.serial.probe_timeout_s)
        business_cases = {"RF-07", "RF-08", "RF-09", "RF-10", "RF-11", "RF-12", "RF-13"}
        need_terminals = any(case in business_cases for case in self.config.test.selected_cases)
        if self.config.serial.tms570_port or self.config.serial.terminal_ports:
            tms_port = self.config.serial.tms570_port
            terminal_ports = list(self.config.serial.terminal_ports) if need_terminals else []
            unknown: List[str] = []
        else:
            found = discovery.discover(preferred_terminal=self.config.serial.preferred_terminal,
                                       discover_terminals=need_terminals)
            tms_port, terminal_ports, unknown = found.tms570, found.terminals, found.unknown
        self.hardware.discovered = {"tms570": tms_port, "terminals": terminal_ports, "unknown": unknown}
        self._log("preflight", f"串口识别: 570={tms_port or '未找到'} terminals={terminal_ports} unknown={unknown}")
        baseline: Dict[str, Any] = {}
        if tms_port:
            self.hardware.tms570 = Tms570Console(SerialEndpoint(tms_port, self.config.serial.tms570_baudrate,
                                                                self.logger.serial_sink if self.logger else None))
            baseline = self.hardware.tms570.fpga_tm()
            self._log("preflight", "570串口AT+FPGATM正常", snapshot=baseline)
        for port in terminal_ports:
            console = TerminalConsole(SerialEndpoint(port, self.config.serial.terminal_baudrate,
                                                      self.logger.serial_sink if self.logger else None))
            self.hardware.terminals.append(console)
            try: self.hardware.dev_euis.append(console.dev_eui())
            except Exception as exc:
                self.hardware.dev_euis.append("")
                self._log("preflight", f"{port} DevEUI读取失败: {exc}")

        executable = (self.root / self.config.spi.bridge_exe).resolve()
        self.hardware.spi = SpiBridge(executable, self._bridge_event)
        scan = self.hardware.spi.scan()
        raw = str(scan.get("raw", ""))
        for sn in (self.config.spi.spi1_sn, self.config.spi.spi2_sn):
            if sn.lower() not in raw.lower():
                raise MissingDependency(f"JTool扫描结果中没有SN {sn}: {raw}")
        self.hardware.spi.open(self.config.spi.spi1_sn, self.config.spi.spi2_sn)
        telemetry = self._collect_preflight_telemetry(baseline)
        self._log("preflight", "SPI1双页遥测正常", telemetry=telemetry[-1])

        if any(case in self.config.test.selected_cases for case in
               ("RF-07", "RF-08", "RF-09", "RF-10", "RF-11", "RF-12", "RF-13")):
            try:
                self.hardware.mqtt = NsMqtt(self.config.mqtt, self.logger.mqtt_sink if self.logger else None)
                self.hardware.mqtt.connect()
                self._log("preflight", "MQTT连接正常")
            except Exception as exc:
                self.hardware.mqtt = None
                self._log("preflight", f"MQTT不可用，业务用例将BLOCKED: {exc}")

        # 隔离启动前的旧记录，确保后续generation均属于本次运行。
        if self.hardware.tms570:
            snapshot = self.hardware.tms570.fpga_tm()
            if snapshot.get("dt_pending", 0) > 0:
                if snapshot.get("mode") in (0, 4, 5, 6):
                    self._log("preflight", f"先将持续产数模式{snapshot['mode']}切回模式3")
                    self._exit_busy_mode()
                pending = snapshot["dt_pending"]
                archive_limit = self.config.test.preexisting_archive_limit_bytes
                if pending <= archive_limit:
                    self._log("preflight", f"归档历史数传 pending={pending}")
                    self._start_data_transfer("preexisting")
                else:
                    self._log("preflight", f"历史数传 pending={pending} 超过归档上限{archive_limit}，直接清理")
                    self.hardware.tms570.clear_data_transfer()
                    cleared = self.hardware.tms570.fpga_tm()
                    if cleared.get("dt_active") != 0:
                        raise AssertionError(f"历史数传清理后状态异常: {cleared}")
                    self._log("preflight", "历史数传缓存已清理并确认发送空闲；"
                              "状态查询前新到记录允许pending重新增长", snapshot=cleared)

    def _cleanup(self) -> None:
        for terminal in self.hardware.terminals:
            terminal.close()
        if self.hardware.tms570: self.hardware.tms570.close()
        if self.hardware.mqtt: self.hardware.mqtt.close()
        if self.hardware.spi: self.hardware.spi.close()

    def run(self, selected_cases: Optional[Sequence[str]] = None) -> RunSummary:
        selected = list(selected_cases or self.config.test.selected_cases)
        run_id, self.run_dir = create_run_dir(self.root)
        self.logger = RunLogger(self.run_dir, self.output_sink)
        self.summary = RunSummary(run_id, now_iso())
        self._log("runner", f"测试开始: {run_id}")
        preflight_error: Optional[Exception] = None
        try:
            try: self._preflight()
            except Exception as exc:
                preflight_error = exc
                self._log("preflight", f"预检失败: {exc}")
            self.summary.environment = self.hardware.discovered
            for case_id in selected:
                if self.stop_event.is_set(): break
                if preflight_error and not self.simulate:
                    self.summary.cases.append(self._result(case_id, Verdict.BLOCKED,
                                                           f"预检失败：{preflight_error}"))
                    continue
                self.summary.cases.append(self._run_case(case_id))
        finally:
            self._cleanup()
            self.summary.ended_at = now_iso()
            self._write_outputs()
            self._log("runner", "测试结束")
            self.logger.close()
        return self.summary

    def _write_outputs(self) -> None:
        assert self.summary and self.run_dir
        write_summary(self.summary, self.run_dir)
        write_excel(self.summary, self.run_dir / "report.xlsx")
        template = (self.root / self.config.test.report_template).resolve()
        if template.exists():
            destination = self.run_dir / f"载荷基带软件测试报告_v0.1_{self.summary.run_id}.md"
            update_markdown_report(template, destination, self.summary.cases)
        config_data = self.config.to_dict()
        (self.run_dir / "config.json").write_text(json.dumps(config_data, ensure_ascii=False, indent=2),
                                                   encoding="utf-8")

    def _result(self, case_id: str, verdict: Verdict, summary: str,
                evidence: Optional[List[Evidence]] = None,
                started: Optional[str] = None) -> CaseResult:
        return CaseResult(case_id, CASE_NAMES.get(case_id, case_id), verdict, summary,
                          started or now_iso(), now_iso(), evidence or [])

    def _run_case(self, case_id: str) -> CaseResult:
        started = now_iso()
        self._log(case_id, f"开始 {CASE_NAMES.get(case_id, case_id)}")
        if self.simulate: return self._simulate_case(case_id, started)
        handler = getattr(self, f"_case_{case_id.replace('-', '_').lower()}", None)
        if not handler: return self._result(case_id, Verdict.BLOCKED, "用例尚未实现", started=started)
        self._case_deadline = time.monotonic() + self.config.test.case_timeout_s
        try:
            result = handler(started)
        except MissingDependency as exc:
            result = self._result(case_id, Verdict.BLOCKED, str(exc), started=started)
        except Exception as exc:
            self._log(case_id, "异常堆栈", traceback=traceback.format_exc())
            if case_id in ("RF-02", "RF-03", "RF-04", "RF-05"):
                try:
                    # Recovery is a safety action outside the case verdict
                    # budget; give it a separate short deadline.
                    self._case_deadline = time.monotonic() + 15.0
                    self._exit_busy_mode()
                    self._log(case_id, "异常后已恢复到工作模式3")
                except Exception as recovery_exc:
                    self._log(case_id, f"异常后恢复工作模式3失败: {recovery_exc}")
            result = self._result(case_id, Verdict.FAILED, str(exc), started=started)
        finally:
            self._case_deadline = None
        self._log(case_id, f"{result.verdict.value}: {result.summary}")
        return result

    def _simulate_case(self, case_id: str, started: str) -> CaseResult:
        return self._result(case_id, Verdict.SKIP,
                            "模拟运行仅验证工具编排和报告，不代表实板测试通过",
                            [Evidence("simulation", "未连接真实SPI、串口、射频和NS业务链路")], started)

    def _need_spi(self) -> SpiBridge:
        if not self.hardware.spi: raise MissingDependency("缺少SPI1/SPI2 JTool")
        return self.hardware.spi

    def _need_tms(self) -> Tms570Console:
        if not self.hardware.tms570: raise MissingDependency("缺少570调试串口")
        return self.hardware.tms570

    def _need_mqtt(self) -> NsMqtt:
        if not self.hardware.mqtt: raise MissingDependency("MQTT/NS未连接")
        return self.hardware.mqtt

    def _collect_matching_telemetry(self, expected: Dict[str, Any], count: int = 2,
                                    timeout: float = 20.0) -> List[Dict[str, Any]]:
        spi = self._need_spi()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        matched: List[Dict[str, Any]] = []
        last: Dict[str, Any] = {}
        while len(matched) < count and time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            try:
                last = spi.collect_telemetry(
                    1, self._remaining_case_time(min(5.0, max(0.5, remaining))))[0]
            except TimeoutError:
                continue
            if all(last.get(key) == value for key, value in expected.items()):
                matched.append(last)
            else:
                matched.clear()
        if len(matched) < count:
            actual = {key: last.get(key) for key in expected}
            raise TimeoutError(f"遥测参数未稳定为{expected}，最后值={actual}")
        return matched

    def _send_checked(self, command: int, value: Any, expected_tm: Dict[str, Any],
                      require_tms: bool = True, verify_active: bool = True) -> List[Evidence]:
        spi = self._need_spi()
        tms = self._need_tms() if require_tms else self.hardware.tms570
        evidence: List[Evidence] = []
        mode_transition = command == 0x01
        try:
            before = (self._fpga_tm(5.0)
                      if (tms and mode_transition) else
                      (self._fpga_tm(15.0) if tms else {}))
        except Exception as exc:
            if require_tms and not mode_transition: raise
            evidence.append(Evidence("570_UART", f"模式忙态下基线读取失败，继续用RC日志和SPI遥测验证：{exc}"))
            before = {}
        sent: Dict[str, Any] = {}
        if tms:
            last_error: Optional[Exception] = None
            for attempt in range(1, 4):
                self._remaining_case_time(0.1)
                sent = spi.send_remote_control(command, value)
                rc_seen = True
                rc_timeout = 60.0 if command == 0x01 else 5.0
                try: tms.wait_rc(command, self._remaining_case_time(rc_timeout))
                except TimeoutError: rc_seen = False
                try:
                    busy_target = mode_transition and value in (0, 5, 6)
                    after = (self._fpga_tm(5.0)
                             if busy_target else self._fpga_tm(30.0))
                except TimeoutError as exc:
                    if mode_transition and rc_seen:
                        evidence.append(Evidence(
                            "570_UART", f"RC OK CMD_{command:02X}；目标模式持续运算，状态查询未响应",
                            {"warning": str(exc)}))
                        evidence.append(Evidence("SPI1", f"下发CMD_{command:02X}",
                                                 {"frame": sent["frame_hex"], "attempt": attempt}))
                        break
                    raise
                try:
                    if before and after.get("rx_frames", 0) <= before.get("rx_frames", -1):
                        raise AssertionError("rxFrames未递增")
                    if before and after.get("rx_errors", 0) != before.get("rx_errors", 0):
                        raise AssertionError("遥控导致rxErrors增加")
                    if after.get("last_cmd") != command:
                        raise AssertionError(f"lastCmd不一致: {after.get('last_cmd')} != {command}")
                    if after.get("last_physical_rx_hex") != sent["frame_hex"]:
                        raise AssertionError("lastPhysicalRx与下发遥控不一致")
                    for key, expected in expected_tm.items():
                        if after.get(key) != expected:
                            raise AssertionError(f"570状态{key}={after.get(key)}，期望{expected}")
                except AssertionError as exc:
                    last_error = exc
                    if attempt == 3:
                        raise
                    self._log("remote_control", f"CMD_{command:02X}第{attempt}次未形成有效帧，重发: {exc}")
                    self._case_sleep(0.2)
                    continue
                message = (f"RC OK CMD_{command:02X}且遥控帧计数递增" if rc_seen else
                           f"CMD_{command:02X}执行日志延迟；状态快照确认命令已执行")
                evidence.append(Evidence("570_UART", message,
                                         {k: after.get(k) for k in
                                          ("rx_frames", "physical_rx", "rx_errors", "last_cmd")}))
                evidence.append(Evidence("SPI1", f"下发CMD_{command:02X}",
                                         {"frame": sent["frame_hex"], "attempt": attempt}))
                break
            else:
                if require_tms and last_error: raise last_error
        else:
            sent = spi.send_remote_control(command, value)
            evidence.append(Evidence("SPI1", f"下发CMD_{command:02X}", {"frame": sent["frame_hex"]}))
        if verify_active:
            self._collect_matching_telemetry(expected_tm, 2, 20.0)
            evidence.append(Evidence("SPI1_TM", "连续两帧遥测checksum及参数一致", expected_tm))
        else:
            evidence.append(Evidence("570_UART", "待应用参数回读一致，等待启动模式后检查SPI遥测", expected_tm))
        return evidence

    def _exit_busy_mode(self) -> List[Evidence]:
        spi = self._need_spi()
        tms = self._need_tms()
        sent = spi.send_remote_control(0x01, 3)
        tms.wait_rc(0x01, self._remaining_case_time(60.0))
        self._collect_matching_telemetry({"mode": 3}, 2, 20.0)
        return [
            Evidence("SPI1", "下发CMD_01立即退出持续工作模式",
                     {"frame": sent["frame_hex"], "mode": 3}),
            Evidence("570_UART+SPI1_TM", "RC OK且连续两帧遥测确认模式3"),
        ]

    def _wait_app(self, predicate: Callable[[Dict[str, Any]], bool], timeout: float,
                  description: str) -> Dict[str, Any]:
        tms = self._need_tms()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        last: Dict[str, Any] = {}
        while time.monotonic() < deadline:
            if self.stop_event.is_set(): raise RuntimeError("用户停止")
            last = tms.app_tm(self._remaining_case_time(15.0), 1)
            if predicate(last): return last
            self._case_sleep(min(5.0, max(0.1, deadline - time.monotonic())))
        raise TimeoutError(f"等待{description}超时，最后状态={last}")

    def _wait_capture_store(self, baseline_generation: int, timeout: float) -> Dict[str, Any]:
        tms = self._need_tms()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        last_text = ""
        transcript = ""
        while time.monotonic() < deadline:
            if self.stop_event.is_set():
                raise RuntimeError("用户停止")
            last_text = tms.endpoint.collect(
                self._remaining_case_time(
                    min(10.0, max(0.1, deadline - time.monotonic()))),
                any_markers=("capture stored:",),
            )
            transcript += last_text
            matches = re.findall(
                r"capture stored:\s*gen=(\d+)\s+bytesPerAntenna=(\d+)",
                transcript
            )
            for generation_text, bytes_text in matches:
                generation = int(generation_text)
                if generation > baseline_generation:
                    return {
                        "capture_generation": generation,
                        "capture_bytes": int(bytes_text),
                        "raw": last_text,
                    }
        raise TimeoutError(
            f"等待采数完成日志超时，最后输出={transcript[-1000:].strip()}")

    def _wait_sweep_stores(self, baseline_generation: int, count: int,
                           timeout: float) -> Dict[str, Any]:
        """Observe sweep completion passively while mode 0 owns the CPU."""
        tms = self._need_tms()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        generations: List[int] = []
        transcript = ""
        while time.monotonic() < deadline:
            text = tms.endpoint.collect(
                self._remaining_case_time(
                    min(10.0, max(0.1, deadline - time.monotonic()))),
                any_markers=("SAT APP sweep stored:",))
            transcript += text
            for generation_text, points_text in re.findall(
                    r"SAT APP sweep stored:\s*gen=(\d+)\s+points=(\d+)",
                    transcript):
                generation = int(generation_text)
                if (int(points_text) == self.config.test.sweep_point_count and
                        generation > baseline_generation and
                        generation not in generations):
                    generations.append(generation)
            if len(generations) >= count:
                return {"sweep_generations": generations[:count],
                        "completed_rounds": len(generations),
                        "last_generation": generations[count - 1]}
        raise TimeoutError(
            f"等待{count}轮扫频保存超时，已观察generation={generations}，"
            f"最后输出={transcript[-1000:].strip()}")

    def _wait_acm_factors_change(self, baseline: List[Dict[str, int]],
                                 timeout: float) -> Dict[str, Any]:
        """Detect a new valid calibration through SPI telemetry.

        The driver-side success line is INFO-level and may be filtered even
        while TRM INFO is enabled.  The 144-byte telemetry is the stable public
        interface and changes atomically when a new factor bank is published.
        """
        spi = self._need_spi()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        baseline_values = tuple((item["i"], item["q"]) for item in baseline)
        last_values = baseline_values
        while time.monotonic() < deadline:
            frames = spi.collect_telemetry(
                2, self._remaining_case_time(
                    min(5.0, max(0.1, deadline - time.monotonic()))))
            for frame in frames:
                values = tuple((item["i"], item["q"])
                               for item in frame["acm"])
                last_values = values
                if (values != baseline_values and any(i != 0 or q != 0
                                                       for i, q in values)):
                    return {"changed": True,
                            "factor_count": len(values)}
            self._case_sleep(0.5)
        raise TimeoutError(
            f"120秒内SPI遥测ACM因子未更新，baseline={baseline_values}，"
            f"last={last_values}")

    def _wait_acm_publish(self, baseline_generation: int,
                          timeout: float) -> Dict[str, Any]:
        """Wait for the TRM RAM-bank publication event, across UART chunks."""
        tms = self._need_tms()
        deadline = time.monotonic() + self._remaining_case_time(timeout)
        transcript = ""
        while time.monotonic() < deadline:
            transcript += tms.endpoint.collect(
                self._remaining_case_time(
                    min(10.0, max(0.1, deadline - time.monotonic()))),
                any_markers=("ACM result published to RAM",))
            matches = re.findall(
                r"ACM result published to RAM bank=(\d+) generation=(\d+) valid=1",
                transcript)
            for bank_text, generation_text in matches:
                generation = int(generation_text)
                if generation > baseline_generation:
                    return {"generation": generation,
                            "bank": int(bank_text), "valid": 1}
        raise TimeoutError(
            f"120秒内未观察到新的ACM RAM bank发布，baselineGen={baseline_generation}，"
            f"最后输出={transcript[-1000:].strip()}")

    def _archive_transfer(self, tag: str, transfer: Dict[str, Any]) -> None:
        assert self.run_dir
        frames: List[bytes] = transfer["frames"]
        with (self.run_dir / f"data_transfer_{tag}.bin").open("wb") as stream:
            for frame in frames: stream.write(frame)
        records = []
        for record in transfer["records"]:
            decoded = dict(record.decoded)
            if isinstance(decoded.get("data"), bytes): decoded["data"] = f"<{len(decoded['data'])} bytes>"
            records.append({"timestamp": record.timestamp, "format": record.format,
                            "type": record.type, "length": len(record.payload), "decoded": decoded})
        (self.run_dir / f"data_transfer_{tag}.json").write_text(
            json.dumps(records, ensure_ascii=False, indent=2, default=str), encoding="utf-8")

    def _clear_case_data(self, case_id: str) -> Evidence:
        """Clear records left by earlier cases and prove the queue is idle."""
        tms = self._need_tms()
        before = self._fpga_tm(15.0)
        if before.get("dt_active", 0) != 0:
            raise AssertionError(f"{case_id}开始前数传仍在运行: {before}")
        clear_budget = self._remaining_case_time(12.0)
        tms.clear_data_transfer(max(1.0, clear_budget / 3.0), 3)
        after = self._fpga_tm(15.0)
        if after.get("dt_active") != 0:
            raise AssertionError(f"{case_id}清理数传缓存后状态异常: {after}")
        # C模式下清理完成和状态查询之间可能刚好收到新用户；这些记录属于
        # 清理后的本用例时间窗，不能因为pending重新变为非0就误判清理失败。
        self._log(case_id, "用例数据隔离完成: "
                  f"pending={before.get('dt_pending', 0)} -> {after.get('dt_pending', 0)}")
        return Evidence("570_UART", "用例开始前清空并复核数传缓存",
                        {"pending_before": before.get("dt_pending", 0),
                         "pending_after": after.get("dt_pending", 0),
                         "active_after": after.get("dt_active", 0)})

    def _start_data_transfer(self, tag: str) -> Dict[str, Any]:
        spi = self._need_spi()
        tms = self.hardware.tms570
        before = self._fpga_tm(15.0) if tms else {}
        spi.send_remote_control(0x09, 1)
        try:
            if tms: tms.wait_rc(0x09, self._remaining_case_time(5.0))
            transfer = spi.capture_data_transfer(self.config.spi.spi2_max_frames,
                                                 self.config.spi.spi2_idle_timeout_ms,
                                                 timeout=self._remaining_case_time(120.0))
        finally:
            # CMD_09=1 keeps the firmware in continuous-transfer mode so that
            # records appended during the session are included. Always close
            # the session after SPI2 becomes idle or capture fails.
            spi.send_remote_control(0x09, 0)
            if tms: tms.wait_rc(0x09, self._remaining_case_time(5.0))
        self._archive_transfer(tag, transfer)
        if tms:
            after = self._fpga_tm(15.0)
            if after.get("dt_starts", 0) <= before.get("dt_starts", -1):
                raise AssertionError("数传starts未递增")
            if after.get("dt_send_error", 0) != before.get("dt_send_error", 0):
                raise AssertionError("数传sendErr增加")
            if after.get("dt_active", 1) != 0:
                raise AssertionError("数传结束后active未清零")
        return transfer

    def _set_common(self, rate: int, mode: int = 3, require_tms: bool = True) -> List[Evidence]:
        """Apply the common payload configuration within one evidence window.

        Checking AT+FPGATM and two telemetry frames after every individual
        command consumed most of the per-case budget.  The FPGA already counts
        every physical remote-control frame, so one before/after snapshot plus
        a final two-frame telemetry check provides the same evidence without
        four serial round trips.
        """
        spi = self._need_spi()
        tms = self.hardware.tms570
        evidence: List[Evidence] = []
        before = (self._fpga_tm(15.0)
                  if require_tms and tms else {})
        sent: List[Dict[str, Any]] = []
        for command, value in (
                (0x05, self.config.test.frequency_hz),
                (0x02, rate),
                (0x06, self.config.test.rf_mask),
                (0x01, mode)):
            sent.append(spi.send_remote_control(command, value))
            # The FPGA presents one physical RC mailbox to the 570.  Leave a
            # full processing interval between frames so a later command cannot
            # overwrite an earlier one while INFO logging occupies the main loop.
            self._case_sleep(1.0)
        active = {"mode": mode, "rate": rate,
                  "frequency_hz": self.config.test.frequency_hz,
                  "rf_mask": self.config.test.rf_mask}
        if require_tms:
            if not tms:
                raise RuntimeError("该用例要求570调试串口证据")
            # The last RC acknowledgement proves that the ordered batch reached
            # CMD_01. Busy modes can stop answering AT commands immediately
            # afterwards, therefore their state is proved by SPI telemetry.
            tms.wait_rc(0x01, self._remaining_case_time(15.0))
            if mode not in (0, 5, 6):
                after = self._fpga_tm(15.0)
                if after.get("rx_frames", 0) < before.get("rx_frames", 0) + len(sent):
                    raise AssertionError(
                        f"批量遥控计数不足: before={before.get('rx_frames')} "
                        f"after={after.get('rx_frames')} sent={len(sent)}")
                if after.get("rx_errors", 0) != before.get("rx_errors", 0):
                    raise AssertionError("批量遥控期间rxErrors增加")
                if after.get("last_cmd") != 0x01:
                    raise AssertionError(f"批量遥控lastCmd异常: {after.get('last_cmd')}")
                evidence.append(Evidence(
                    "570_UART", "四条公共配置遥控均被接收且无新增错误",
                    {"rx_before": before.get("rx_frames"),
                     "rx_after": after.get("rx_frames"),
                     "rx_errors": after.get("rx_errors")}))
            else:
                evidence.append(Evidence(
                    "570_UART", "收到最终RC OK；持续工作模式由SPI遥测和业务日志确认",
                    {"mode": mode, "command_count": len(sent)}))
        self._collect_matching_telemetry(active, 2, 15.0)
        evidence.append(Evidence(
            "SPI1_TM", "连续两帧遥测校验及公共配置一致", active))
        evidence.append(Evidence(
            "SPI1", "批量下发频率、速率、RF掩码和工作模式",
            {"frames": [item["frame_hex"] for item in sent]}))
        return evidence

    def _case_rf_01(self, started: str) -> CaseResult:
        spi = self._need_spi()
        tms = self._need_tms()
        evidence: List[Evidence] = []
        before = self._fpga_tm(15.0)
        sent_frames: List[str] = []

        def send(command: int, value: Any) -> None:
            sent_frames.append(spi.send_remote_control(command, value)["frame_hex"])
            self._case_sleep(1.0)

        # Frequency and RF mask are common to every combination and only need
        # to be sent once. Each rate/mode checkpoint uses actual SPI telemetry;
        # one UART counter snapshot at the end proves all RC frames were seen.
        send(0x05, self.config.test.frequency_hz)
        send(0x06, self.config.test.rf_mask)
        for rate in (0, 1, 2):
            send(0x02, rate)
            send(0x01, 3)
            expected = {"mode": 3, "rate": rate,
                        "frequency_hz": self.config.test.frequency_hz,
                        "rf_mask": self.config.test.rf_mask}
            self._collect_matching_telemetry(expected, 2, 12.0)
            evidence.append(Evidence("SPI1_TM", f"速率{rate}参数生效", expected))
        for mode in (0, 4, 5, 6):
            send(0x01, mode)
            self._collect_matching_telemetry({"mode": mode}, 2, 12.0)
            evidence.append(Evidence("SPI1_TM", f"工作模式{mode}生效", {"mode": mode}))
            send(0x01, 3)
            self._collect_matching_telemetry({"mode": 3}, 2, 12.0)
        after = self._fpga_tm(15.0)
        if after.get("rx_frames", 0) < before.get("rx_frames", 0) + len(sent_frames):
            raise AssertionError(
                f"RF-01遥控计数不足: before={before.get('rx_frames')} "
                f"after={after.get('rx_frames')} sent={len(sent_frames)}")
        if after.get("rx_errors", 0) != before.get("rx_errors", 0):
            raise AssertionError("RF-01遍历期间rxErrors增加")
        evidence.append(Evidence(
            "570_UART", "批量遥控接收计数与错误计数正确",
            {"rx_before": before.get("rx_frames"),
             "rx_after": after.get("rx_frames"),
             "sent": len(sent_frames), "rx_errors": after.get("rx_errors")}))
        evidence.append(self._clear_case_data("RF-01"))
        return self._result("RF-01", Verdict.PASS, "模式0/3/4/5/6、速率0/1/2、频率及RF掩码配置一致",
                            evidence[-8:], started)

    def _case_rf_02(self, started: str) -> CaseResult:
        tms = self._need_tms()
        isolation = self._clear_case_data("RF-02")
        baseline = tms.app_tm(self._remaining_case_time(15.0), 1).get(
            "sweep_generation", 0)
        evidence = [isolation] + self._set_common(0, 0, True)
        state = self._wait_sweep_stores(
            baseline, 2, self.config.test.sweep_timeout_s)
        evidence.append(Evidence(
            "570_UART",
            f"被动观察到两轮{self.config.test.sweep_point_count}点扫频结果已保存",
            state))
        evidence += self._exit_busy_mode()
        transfer = self._start_data_transfer("RF02")
        validation = validate_sweep(
            transfer["records"], 2, state["sweep_generations"][0])
        if not validation["valid"]: raise AssertionError(str(validation))
        evidence.append(Evidence(
            "SPI2",
            f"重组两轮{self.config.test.sweep_point_count}点0x04底噪记录",
            validation))
        return self._result("RF-02", Verdict.PASS,
                            f"固定504～508MHz、{self.config.test.sweep_point_count}点循环扫频及两轮背景噪声数传正确",
                            evidence, started)

    def _case_rf_03(self, started: str) -> CaseResult:
        evidence: List[Evidence] = []
        try:
            evidence += self._set_common(0, 4, True)
            evidence += self._exit_busy_mode()
        except Exception as exc:
            evidence.append(Evidence("precheck", f"模式4预检查异常：{exc}"))
        return self._result("RF-03", Verdict.SKIP, "已检查模式4入口；缺少频谱仪，不能判定tone频率和功率",
                            evidence, started)

    def _case_rf_04(self, started: str) -> CaseResult:
        tms = self._need_tms()
        before = tms.app_tm(self._remaining_case_time(15.0), 1)
        baseline_gen = before.get("acm_generation", 0)
        baseline_count = before.get("acm_completed", 0)
        evidence = self._set_common(0, 5, True)
        publish = self._wait_acm_publish(
            baseline_gen, self.config.test.acm_timeout_s)
        evidence.append(Evidence(
            "570_UART", "ACM有效结果已发布到内部RAM双bank", publish))
        evidence += self._exit_busy_mode()
        state = tms.app_tm(self._remaining_case_time(15.0), 1)
        if not (state.get("acm_generation", 0) > baseline_gen and
                state.get("acm_completed", 0) > baseline_count and
                state.get("acm_valid_mask", 0) == 0xFF and
                len(state.get("acm_factors", [])) == 8):
            raise AssertionError(f"ACM完成后的状态不完整: {state}")
        frames = self._need_spi().collect_telemetry(
            2, self._remaining_case_time(12.0))
        if all(all(item["i"] == 0 and item["q"] == 0 for item in frame["acm"]) for frame in frames):
            raise AssertionError("SPI遥测ACM因子仍全为0")
        evidence.append(Evidence("570_UART", "ACM完成、generation递增且8天线有效", state))
        evidence.append(Evidence("SPI1_TM", "ACM遥测因子已更新"))
        return self._result("RF-04", Verdict.PASS, "ACM校准完成且570/SPI1遥测结果一致", evidence, started)

    def _case_rf_05(self, started: str) -> CaseResult:
        tms = self._need_tms()
        isolation = self._clear_case_data("RF-05")
        before = tms.app_tm(self._remaining_case_time(15.0), 1)
        baseline = before.get("capture_generation", 0)
        errors = before.get("capture_errors", 0)
        evidence = [isolation] + self._set_common(0, 6, True)
        automatic = self._wait_capture_store(baseline, self.config.test.capture_timeout_s)
        evidence.append(Evidence("570_UART", "自动日志确认一代8天线采数已保存", automatic))
        evidence += self._exit_busy_mode()
        state = tms.app_tm(self._remaining_case_time(15.0), 1)
        if not (state.get("capture_generation", 0) > baseline and
                state.get("capture_valid_mask", 0) == 0xFF and
                state.get("capture_bytes", 0) == automatic["capture_bytes"] and
                state.get("capture_errors", 0) == errors):
            raise AssertionError(f"采数结束状态异常: {state}")
        evidence.append(Evidence("570_UART", "模式3下复核generation、8天线、长度和错误计数", state))
        transfer = self._start_data_transfer("RF05")
        validation = validate_capture(transfer["records"], baseline + 1)
        if not validation["valid"]: raise AssertionError(str(validation))
        evidence.append(Evidence("SPI2", "完整重组0x03八天线原始数据", validation))
        return self._result("RF-05", Verdict.PASS, "8天线采数及原始数据数传完整", evidence, started)

    def _case_rf_06(self, started: str) -> CaseResult:
        evidence: List[Evidence] = []
        for mode, name in ((1, "A"), (2, "B"), (3, "C")):
            checkpoint = self._send_checked(0x01, mode, {"mode": mode}, True)
            evidence.extend(checkpoint)
            evidence.append(Evidence("mode", f"模式{name}进入成功", {"mode": mode}))
        return self._result("RF-06", Verdict.PASS,
                            "模式A/B/C均完成遥控、板端状态和SPI1遥测确认",
                            evidence, started)

    def _unique_payload(self, sequence: int, terminal_index: int = 0) -> str:
        raw = bytearray.fromhex(self.config.test.ack_payload)
        if len(raw) < 2: raise ValueError("ACK负载至少2字节")
        suffix = (int.from_bytes(raw[-2:], "big") + sequence + terminal_index * 16) & 0xFFFF
        raw[-2:] = suffix.to_bytes(2, "big")
        return raw.hex().upper()

    def _configure_business(self, rate: int, terminal_count: int) -> Tuple[List[TerminalConsole], List[str], List[Evidence]]:
        mqtt = self._need_mqtt()
        if len(self.hardware.terminals) < terminal_count:
            raise MissingDependency(f"需要{terminal_count}个终端，实际{len(self.hardware.terminals)}个")
        terminal_rate, ns_rate = RATE_MAP[rate]
        evidence = self._set_common(rate, 3, False)
        self._log("payload", f"570载荷配置及遥测确认完成: rate={rate} mode=3")
        gateway = mqtt.configure_gateway(self.config.gateway, ns_rate)
        self._log("NS", f"网关速率更新为NS {ns_rate}", gateway=gateway)
        self._case_sleep(self.config.test.gateway_wait_s)
        terminals = self.hardware.terminals[:terminal_count]
        dev_euis = self.hardware.dev_euis[:terminal_count]
        missing = [port.endpoint.port for port, eui in zip(terminals, dev_euis)
                   if not eui or not mqtt.terminal_exists(eui)]
        if missing: raise MissingDependency(f"NS中缺少终端登记或DevEUI不可读: {missing}")
        parallel_call(terminals, lambda terminal: terminal.configure_and_join(
            terminal_rate, self.config.test.frequency_hz,
            self._remaining_case_time(self.config.serial.join_timeout_s)))
        evidence.append(Evidence("terminal", f"{terminal_count}个终端均+NWKINFO:4",
                                 {"terminal_rate": terminal_rate, "dev_euis": dev_euis}))
        return terminals, dev_euis, evidence

    def _business_attempts(self, terminals: List[TerminalConsole], dev_euis: List[str]) -> Tuple[bool, List[Evidence]]:
        mqtt = self._need_mqtt()
        successful: set[int] = set()
        evidence: List[Evidence] = []
        for attempt in range(self.config.test.attempts):
            self._remaining_case_time(0.1)
            targets = [i for i in range(len(terminals)) if i not in successful]
            payloads = {i: self._unique_payload(attempt, i) for i in targets}
            with threading.Semaphore(1):
                results: Dict[int, str] = {}
                errors: Dict[int, str] = {}
                threads = []
                def send(index: int) -> None:
                    try:
                        timeout = self._remaining_case_time(20.0)
                        results[index] = terminals[index].send_ack(
                            payloads[index], self.config.test.ack_port, timeout)
                    except Exception as exc: errors[index] = str(exc)
                for index in targets:
                    thread = threading.Thread(target=send, args=(index,), daemon=True)
                    threads.append(thread); thread.start()
                for thread in threads: thread.join()
            for index in targets:
                if index in errors: continue
                try:
                    uplink = mqtt.uplinks.wait(dev_euis[index], self.config.test.ack_port,
                                               payloads[index],
                                               self._remaining_case_time(10.0))
                    successful.add(index)
                    evidence.append(Evidence("terminal+MQTT", f"终端{index + 1} TXSTATUS7及MQTT闭环",
                                             {"dev_eui": dev_euis[index], "payload": payloads[index],
                                              "rssi": uplink.get("rssi"), "snr": uplink.get("snr")}))
                except Exception as exc: errors[index] = str(exc)
            self._log("business", f"attempt={attempt + 1} success={len(successful)}/{len(terminals)} errors={errors}")
            if len(successful) == len(terminals): break
        return len(successful) == len(terminals), evidence

    def _single_user_case(self, case_id: str, rate: int, started: str) -> CaseResult:
        isolation = self._clear_case_data(case_id)
        terminals, dev_euis, evidence = self._configure_business(rate, 1)
        evidence.insert(0, isolation)
        passed, packet_evidence = self._business_attempts(terminals, dev_euis)
        evidence += packet_evidence
        if not passed: raise AssertionError("3次尝试均未形成TXSTATUS7+MQTT闭环")
        transfer = self._start_data_transfer(case_id.replace("-", ""))
        users = [record.decoded for record in transfer["records"] if record.type == 0x02]
        expected_user_id = int(dev_euis[0][-8:], 16)
        tolerance_hz = self.config.test.user_frequency_tolerance_hz
        matching = [user for user in users
                    if user.get("user_id") == expected_user_id
                    and user.get("rate") == RATE_MAP[rate][0]
                    and abs(user.get("frequency_hz", 0) - self.config.test.frequency_hz) <= tolerance_hz]
        if not matching:
            raise AssertionError(
                f"数传中没有匹配用户ID/速率/中心频率±{tolerance_hz}Hz的0x02首用户记录")
        selected = dict(matching[-1])
        selected["configured_frequency_hz"] = self.config.test.frequency_hz
        selected["frequency_error_hz"] = selected["frequency_hz"] - self.config.test.frequency_hz
        selected["frequency_tolerance_hz"] = tolerance_hz
        evidence.append(Evidence("SPI2", "首用户0x02记录用户ID和速率匹配，测量频率在容差内", selected))
        if self.hardware.tms570:
            health = self.hardware.tms570.app_tm(
                self._remaining_case_time(15.0), 1)
            evidence.append(Evidence("570_UART", "业务后570运行状态", health))
        return self._result(case_id, Verdict.PASS, f"单用户速率{rate}双向收发及首用户数传正确",
                            evidence, started)

    def _case_rf_07(self, started: str) -> CaseResult: return self._single_user_case("RF-07", 0, started)
    def _case_rf_08(self, started: str) -> CaseResult: return self._single_user_case("RF-08", 1, started)
    def _case_rf_09(self, started: str) -> CaseResult: return self._single_user_case("RF-09", 2, started)

    def _multi_user_case(self, case_id: str, rate: int, started: str) -> CaseResult:
        if len(self.hardware.terminals) < 16:
            return self._result(case_id, Verdict.SKIP,
                                f"需要16个终端，自动发现{len(self.hardware.terminals)}个", started=started)
        terminals, dev_euis, evidence = self._configure_business(rate, 16)
        passed, packet_evidence = self._business_attempts(terminals, dev_euis)
        evidence += packet_evidence
        if not passed: raise AssertionError("16终端未能在3轮内全部形成闭环")
        return self._result(case_id, Verdict.PASS, f"16用户速率{rate}均完成双向收发", evidence, started)

    def _case_rf_10(self, started: str) -> CaseResult: return self._multi_user_case("RF-10", 0, started)
    def _case_rf_11(self, started: str) -> CaseResult: return self._multi_user_case("RF-11", 1, started)
    def _case_rf_12(self, started: str) -> CaseResult: return self._multi_user_case("RF-12", 2, started)

    def _case_rf_13(self, started: str) -> CaseResult:
        if len(self.hardware.terminals) < 16:
            return self._result("RF-13", Verdict.SKIP,
                                f"需要16个终端，自动发现{len(self.hardware.terminals)}个", started=started)
        rf12 = next((case for case in (self.summary.cases if self.summary else []) if case.case_id == "RF-12"), None)
        if not rf12 or rf12.verdict != Verdict.PASS:
            raise MissingDependency("RF-13要求RF-12先通过")
        terminals, dev_euis, evidence = self._configure_business(2, 16)
        tms = self.hardware.tms570
        baseline_fpga = (self._fpga_tm(15.0)
                         if tms else {})
        baseline_app = (tms.app_tm(self._remaining_case_time(15.0), 1)
                        if tms else {})
        baseline_spi_tm = self._need_spi().collect_telemetry(
            1, self._remaining_case_time(10.0))[0]
        start_monotonic = time.monotonic()
        duration_s = self._remaining_case_time(self.config.test.rf13_duration_s)
        deadline = start_monotonic + duration_s
        next_send = start_monotonic
        next_health = start_monotonic
        hourly: Dict[int, set[int]] = {}
        while time.monotonic() < deadline:
            if self.stop_event.is_set(): raise RuntimeError("用户停止RF-13")
            now = time.monotonic()
            if now >= next_send:
                bucket = int((now - start_monotonic) // 3600)
                passed, batch = self._business_attempts(terminals, dev_euis)
                evidence.extend(batch[-16:])
                hourly.setdefault(bucket, set()).update(
                    int(item.message.split("终端", 1)[1].split(" ", 1)[0]) - 1 for item in batch
                    if item.source == "terminal+MQTT")
                self._log("RF-13", f"hour={bucket} success={len(hourly[bucket])}/16")
                next_send += self.config.test.rf13_send_interval_s
            if tms and now >= next_health:
                fpga = self._fpga_tm(15.0)
                app = tms.app_tm(self._remaining_case_time(15.0), 1)
                if fpga.get("reset_count") != baseline_fpga.get("reset_count"):
                    raise AssertionError("RF-13期间resetCount变化")
                if app.get("uptime_ms", 0) < baseline_app.get("uptime_ms", 0):
                    raise AssertionError("RF-13期间uptime倒退")
                if app.get("spi_error_count", 0) > baseline_app.get("spi_error_count", 0):
                    raise AssertionError("RF-13期间SPI错误计数增加")
            if now >= next_health:
                current_tm = self._need_spi().collect_telemetry(
                    1, self._remaining_case_time(10.0))[0]
                if current_tm.get("reset_count") != baseline_spi_tm.get("reset_count"):
                    raise AssertionError("RF-13期间SPI遥测resetCount变化")
                if current_tm.get("uptime_s", 0) < baseline_spi_tm.get("uptime_s", 0):
                    raise AssertionError("RF-13期间SPI遥测uptime倒退")
                next_health += self.config.test.rf13_health_interval_s
            self._case_sleep(min(1.0, max(0.01, deadline - time.monotonic())))
        incomplete = {hour: len(users) for hour, users in hourly.items() if len(users) < 16}
        if incomplete: raise AssertionError(f"存在小时未满足每终端至少1次成功: {incomplete}")
        evidence.append(Evidence(
            "stability", "受用例超时约束的短时稳定性闭环",
            {"duration_s": round(duration_s, 1),
             "hours": {str(k): len(v) for k, v in hourly.items()},
             "coverage": "不替代24小时稳定性测试"}))
        return self._result(
            "RF-13", Verdict.PASS,
            f"{duration_s:.0f}秒短时16用户速率2稳定运行通过；不等同24小时结论",
            evidence, started)
