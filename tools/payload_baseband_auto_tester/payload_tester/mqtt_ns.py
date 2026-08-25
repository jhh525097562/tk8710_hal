from __future__ import annotations

import json
import queue
import threading
import time
from typing import Any, Callable, Dict, List, Optional, Tuple

from .config import GatewayConfig, MqttConfig


class NsError(RuntimeError):
    pass


class UplinkRegistry:
    def __init__(self) -> None:
        self._condition = threading.Condition()
        self._messages: List[Dict[str, Any]] = []

    def add(self, body: Dict[str, Any]) -> None:
        with self._condition:
            self._messages.append(body)
            self._condition.notify_all()

    def wait(self, dev_eui: str, port: int, payload: str, timeout: float) -> Dict[str, Any]:
        key = (dev_eui.upper(), int(port), payload.upper())
        deadline = time.monotonic() + timeout
        with self._condition:
            while time.monotonic() < deadline:
                for body in self._messages:
                    candidate = (str(body.get("dev_eui", "")).upper(),
                                 int(body.get("port", -1)), str(body.get("data", "")).upper())
                    if candidate == key: return body
                self._condition.wait(max(0.01, deadline - time.monotonic()))
        raise TimeoutError(f"MQTT未收到上行: devEUI={dev_eui} port={port} payload={payload}")


class NsMqtt:
    def __init__(self, config: MqttConfig,
                 message_sink: Optional[Callable[[str, bytes], None]] = None,
                 client: Any = None):
        self.config = config
        self.message_sink = message_sink or (lambda topic, payload: None)
        self.client = client
        self.connected = threading.Event()
        self.uplinks = UplinkRegistry()
        self._pending: Dict[Tuple[int, str], queue.Queue[Dict[str, Any]]] = {}
        self._request_id = 0
        self._lock = threading.Lock()

    def _make_client(self) -> Any:
        try:
            import paho.mqtt.client as mqtt
        except ImportError as exc:
            raise NsError("缺少paho-mqtt，请先安装requirements.txt") from exc
        try: return mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except (AttributeError, TypeError): return mqtt.Client()

    def connect(self, timeout: float = 10.0) -> None:
        self.client = self.client or self._make_client()
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        if self.config.username:
            self.client.username_pw_set(self.config.username, self.config.password)
        self.client.connect(self.config.host, self.config.port, keepalive=30)
        self.client.loop_start()
        if not self.connected.wait(timeout): raise TimeoutError("MQTT连接或订阅超时")

    def close(self) -> None:
        if self.client:
            try: self.client.loop_stop(); self.client.disconnect()
            except Exception: pass
        self.connected.clear()

    def _on_connect(self, client: Any, userdata: Any, flags: Any, rc: int, *extra: Any) -> None:
        if rc != 0: return
        client.subscribe(self.config.uplink_topic, qos=1)
        if self.config.response_topic: client.subscribe(self.config.response_topic, qos=1)
        self.connected.set()

    def _on_disconnect(self, *args: Any) -> None:
        self.connected.clear()

    def _on_message(self, client: Any, userdata: Any, message: Any) -> None:
        self.feed_message(message.topic, message.payload)

    def feed_message(self, topic: str, payload: bytes) -> None:
        self.message_sink(topic, payload)
        try: data = json.loads(payload.decode("utf-8"))
        except (ValueError, UnicodeDecodeError): return
        if data.get("req_opt") == "push_uplink" and isinstance(data.get("req_body"), dict):
            self.uplinks.add(data["req_body"]); return
        if "rsp_code" not in data: return
        waiter = self._pending.get((data.get("req_id"), data.get("req_opt")))
        if waiter: waiter.put(data)

    def request(self, operation: str, body: Any, timeout: float = 10.0) -> Dict[str, Any]:
        if not self.client: raise NsError("MQTT未连接")
        with self._lock:
            self._request_id = self._request_id % 2147483647 + 1
            request_id = self._request_id
        key = (request_id, operation)
        waiter: queue.Queue[Dict[str, Any]] = queue.Queue(maxsize=1)
        self._pending[key] = waiter
        message = {"req_id": request_id, "req_opt": operation, "req_body": body}
        try:
            result = self.client.publish(self.config.downlink_topic,
                                         json.dumps(message, separators=(",", ":")), qos=1, retain=False)
            if hasattr(result, "rc") and result.rc != 0: raise NsError(f"MQTT发布失败: {result.rc}")
            try: response = waiter.get(timeout=timeout)
            except queue.Empty as exc: raise TimeoutError(f"NS响应超时: {operation}") from exc
        finally:
            self._pending.pop(key, None)
        codes = response.get("rsp_code", -1)
        codes = codes if isinstance(codes, list) else [codes]
        if not codes or any(code != 0 for code in codes):
            raise NsError(str(response.get("rsp_desc", response)))
        return response

    @staticmethod
    def gateway_body(config: GatewayConfig, ns_rate: int) -> Dict[str, Any]:
        return {
            "gw_id": config.gw_id.upper(), "freq_major": config.freq_major,
            "freq_minor": config.freq_minor, "nwk_num": config.nwk_num,
            "tdd_num": config.tdd_num, "slot_cfg_num": config.slot_cfg_num,
            "rate_num": 1,
            "rate_cfgs": [{"rate_mode": ns_rate, "uplink_len": config.uplink_len,
                           "downlink_len": config.downlink_len}],
            "description": config.description,
        }

    def configure_gateway(self, config: GatewayConfig, ns_rate: int) -> Dict[str, Any]:
        expected = self.gateway_body(config, ns_rate)
        response = self.request("get_gateway", {"gw_ids": [config.gw_id.upper()]})
        rows = response.get("rsp_body") or []
        if rows:
            # 只重建指定网关，不触碰其他网关；测试结束按需求保留最终速率。
            self.request("delete_gateway", {"gw_ids": [config.gw_id.upper()]})
        self.request("add_gateway", [expected])
        verify = self.request("get_gateway", {"gw_ids": [config.gw_id.upper()]})
        applied_rows = verify.get("rsp_body") or []
        if not applied_rows:
            raise NsError(f"Gateway configuration readback failed: {config.gw_id.upper()}")
        applied = applied_rows[0]
        if int(applied.get("slot_cfg_num", -1)) != config.slot_cfg_num:
            raise NsError(
                f"Gateway slot configuration mismatch: expected={config.slot_cfg_num} "
                f"actual={applied.get('slot_cfg_num')}"
            )
        return {"previous": rows[0] if rows else None, "requested": expected, "applied": applied}

    def terminal_exists(self, dev_eui: str) -> bool:
        response = self.request("get_terminal", {"dev_euis": [dev_eui.upper()]})
        return bool(response.get("rsp_body") or [])
