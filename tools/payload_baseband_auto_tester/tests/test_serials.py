import unittest

from payload_tester.serials import PortDiscovery, Tms570Console, natural_port_key


class FakeEndpoint:
    replies = {}
    opened = []
    def __init__(self, port, baudrate, line_sink=None):
        self.port = port
        self.baudrate = baudrate
        self.opened.append((port, baudrate))
    def collect(self, duration, markers=()): return self.replies.get((self.port, "passive"), "")
    def command(self, command, timeout=3, markers=(), any_markers=()): return self.replies.get((self.port, command), "")
    def close(self): pass


class SerialTests(unittest.TestCase):
    def test_natural_order(self):
        self.assertEqual(sorted(["COM14", "COM3", "COM10"], key=natural_port_key), ["COM3", "COM10", "COM14"])

    def test_discovery_separates_570_before_reset(self):
        FakeEndpoint.opened = []
        FakeEndpoint.replies = {
            ("COM3", "AT+FPGATM"): "FPGA_TM rxFrames=1\nFPGA_PARAM mode=3\nDT head=0",
            ("COM14", "AT+FPGATM"): "AT_PARAM_ERROR",
            ("COM14", "AT+RST"): "MAC AT CMD!",
        }
        found = PortDiscovery(1000000, 115200, endpoint_factory=FakeEndpoint).discover(
            ["COM14", "COM3"], "COM14")
        self.assertEqual(found.tms570, "COM3")
        self.assertEqual(found.terminals, ["COM14"])
        self.assertIn(("COM3", 1000000), FakeEndpoint.opened)
        self.assertIn(("COM14", 115200), FakeEndpoint.opened)

    def test_fpga_tm_retries_lost_uart_command(self):
        class Endpoint:
            calls = 0
            def command(self, command, timeout=3, markers=(), any_markers=()):
                self.calls += 1
                if self.calls == 1: return "busy log only"
                return ("FPGA_TM rxFrames=7 rxErrors=0 lastCmd=0x1 physicalRx=9\n"
                        "FPGA_PARAM mode=3 rate=0 freqHz=477800000 rfMask=255\n"
                        "DT pending=0 active=0 starts=1 built=1 sent=1 sendErr=0 dmaRxSum=0\nOK")
            def close(self): pass
        endpoint = Endpoint()
        snapshot = Tms570Console(endpoint).fpga_tm(0.01, 2)
        self.assertEqual(snapshot["rx_frames"], 7)
        self.assertEqual(endpoint.calls, 2)


if __name__ == "__main__": unittest.main()
