import json
import threading
import time
import unittest

from payload_tester.config import GatewayConfig, MqttConfig
from payload_tester.mqtt_ns import NsMqtt


class FakePublish:
    rc = 0


class FakeClient:
    def __init__(self): self.published = []
    def publish(self, topic, text, **kwargs): self.published.append((topic, json.loads(text))); return FakePublish()


class MqttTests(unittest.TestCase):
    def test_gateway_rate_body(self):
        body = NsMqtt.gateway_body(GatewayConfig(), 3)
        self.assertEqual(body["rate_cfgs"], [{"rate_mode": 3, "uplink_len": 30, "downlink_len": 30}])
        self.assertEqual(body["slot_cfg_num"], 2)

    def test_response_and_uplink_matching(self):
        client = FakeClient(); adapter = NsMqtt(MqttConfig(), client=client)
        answer = {}
        thread = threading.Thread(target=lambda: answer.setdefault("value", adapter.request("get_gateway", {})))
        thread.start()
        while not client.published: time.sleep(0.001)
        request = client.published[0][1]
        adapter.feed_message("x", json.dumps({"req_id": request["req_id"], "req_opt": "get_gateway", "rsp_code": 0}).encode())
        thread.join(); self.assertEqual(answer["value"]["rsp_code"], 0)
        adapter.feed_message("x", json.dumps({"req_opt": "push_uplink", "req_body": {"dev_eui": "A" * 16, "port": 2, "data": "1122"}}).encode())
        self.assertEqual(adapter.uplinks.wait("A" * 16, 2, "1122", 0.1)["data"], "1122")


if __name__ == "__main__": unittest.main()
