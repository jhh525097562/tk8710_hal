from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List


@dataclass
class SpiConfig:
    spi1_sn: str = "9a85a46b0453"
    spi2_sn: str = "9a85B9a50453"
    bridge_exe: str = "bridge/jtool_spi_bridge.exe"
    spi2_max_frames: int = 4096
    spi2_idle_timeout_ms: int = 2000


@dataclass
class SerialConfig:
    tms570_baudrate: int = 1000000
    terminal_baudrate: int = 115200
    preferred_terminal: str = "COM14"
    tms570_port: str = ""
    terminal_ports: List[str] = field(default_factory=list)
    probe_timeout_s: float = 15.0
    join_timeout_s: float = 60.0


@dataclass
class MqttConfig:
    host: str = "47.92.195.235"
    port: int = 41883
    username: str = "admin"
    password: str = ""
    uplink_topic: str = "uplink/huangjh/test"
    downlink_topic: str = "downlink/huangjh/test"
    response_topic: str = ""


@dataclass
class GatewayConfig:
    gw_id: str = "0599999999999999"
    freq_major: int = 2
    freq_minor: int = 2
    nwk_num: int = 1
    tdd_num: int = 1
    slot_cfg_num: int = 2
    uplink_len: int = 30
    downlink_len: int = 30
    description: str = "载荷软件3.6自动测试网关"


@dataclass
class TestConfig:
    frequency_hz: int = 477800000
    user_frequency_tolerance_hz: int = 125000
    rf_mask: int = 255
    slot_config: int = 0
    tx_power: int = 26
    ack_port: int = 2
    ack_payload: str = "1122334411223344"
    attempts: int = 3
    case_timeout_s: float = 120.0
    acm_timeout_s: float = 180.0
    sweep_timeout_s: float = 180.0
    sweep_point_count: int = 33
    capture_timeout_s: float = 180.0
    gateway_wait_s: float = 20.0
    preexisting_archive_limit_bytes: int = 64 * 1024
    rf13_duration_s: int = 24 * 60 * 60
    rf13_send_interval_s: int = 10 * 60
    rf13_health_interval_s: int = 5 * 60
    report_template: str = "../../docs/载荷基带软件测试报告_v0.1.md"
    selected_cases: List[str] = field(default_factory=lambda: [f"RF-{n:02d}" for n in range(1, 14)])


@dataclass
class AppConfig:
    spi: SpiConfig = field(default_factory=SpiConfig)
    serial: SerialConfig = field(default_factory=SerialConfig)
    mqtt: MqttConfig = field(default_factory=MqttConfig)
    gateway: GatewayConfig = field(default_factory=GatewayConfig)
    test: TestConfig = field(default_factory=TestConfig)

    def to_dict(self) -> Dict[str, Any]:
        data = asdict(self)
        data["mqtt"]["password"] = ""
        return data


def _merge_dataclass(target: Any, values: Dict[str, Any]) -> None:
    for key, value in values.items():
        if not hasattr(target, key):
            raise ValueError(f"未知配置字段: {type(target).__name__}.{key}")
        current = getattr(target, key)
        if hasattr(current, "__dataclass_fields__") and isinstance(value, dict):
            _merge_dataclass(current, value)
        else:
            setattr(target, key, value)


def load_config(path: str | Path | None = None) -> AppConfig:
    config = AppConfig()
    if path:
        with Path(path).open("r", encoding="utf-8") as stream:
            raw = json.load(stream)
        serial_values = raw.get("serial")
        if isinstance(serial_values, dict) and "baudrate" in serial_values:
            # 兼容旧配置：旧版只有一个公共波特率字段。
            legacy_baudrate = serial_values.pop("baudrate")
            serial_values.setdefault("tms570_baudrate", legacy_baudrate)
            serial_values.setdefault("terminal_baudrate", legacy_baudrate)
        _merge_dataclass(config, raw)
    config.mqtt.password = os.getenv("PAYLOAD_TEST_MQTT_PASSWORD", config.mqtt.password)
    if config.test.case_timeout_s <= 0:
        raise ValueError("test.case_timeout_s必须大于0")
    return config


def save_config(config: AppConfig, path: str | Path) -> None:
    with Path(path).open("w", encoding="utf-8") as stream:
        json.dump(config.to_dict(), stream, ensure_ascii=False, indent=2)
