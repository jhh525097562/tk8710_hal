import struct
import unittest

from payload_tester.protocol import (DT_FRAME_LEN, RecordDecoder, TelemetryAssembler,
                                     build_remote_control, checksum8, dt_checksum,
                                     parse_app_tm, parse_fpga_tm, parse_telemetry,
                                     validate_capture, validate_sweep)


def telemetry_frame(mode=3, rate=2, freq=477800000, rf_mask=255):
    frame = bytearray(144); frame[:2] = b"\xEB\x90"
    frame[20], frame[21], frame[22], frame[23] = mode, rate, 0, 26
    frame[24:28] = freq.to_bytes(4, "big"); frame[28] = rf_mask
    frame[29:32] = bytes((1, 0, 1)); frame[32:36] = (241).to_bytes(4, "big")
    frame[143] = checksum8(frame, 2, 143)
    return bytes(frame)


def data_frame(seq, payload):
    frame = bytearray([0x5A] * DT_FRAME_LEN)
    frame[:4] = bytes.fromhex("1ACFFC1D"); frame[4:6] = len(payload).to_bytes(2, "big")
    frame[6:8] = seq.to_bytes(2, "big"); frame[8:8 + len(payload)] = payload
    frame[510:512] = dt_checksum(bytes(frame)).to_bytes(2, "big")
    return bytes(frame)


def record(record_type, payload, timestamp=1):
    return timestamp.to_bytes(4, "big") + bytes((2, record_type)) + len(payload).to_bytes(2, "big") + payload


class ProtocolTests(unittest.TestCase):
    def test_remote_control_frequency(self):
        frame = build_remote_control(5, 477800000)
        self.assertEqual(frame[:3], bytes.fromhex("762505"))
        self.assertEqual(int.from_bytes(frame[3:7], "big"), 477800000)
        self.assertEqual(frame[9], sum(frame[2:9]) & 0xFF)

    def test_remote_control_data_transfer_switch(self):
        self.assertEqual(build_remote_control(0x09, 1).hex().upper(),
                         "7625090100000000000A")
        self.assertEqual(build_remote_control(0x09, 0).hex().upper(),
                         "76250900000000000009")
        with self.assertRaises(ValueError):
            build_remote_control(0x09)
        with self.assertRaises(ValueError):
            build_remote_control(0x09, 2)

    def test_telemetry_two_pages(self):
        raw = telemetry_frame()
        assembler = TelemetryAssembler()
        self.assertIsNone(assembler.feed(raw[:128]))
        parsed = assembler.feed(raw[128:] + bytes(112))
        self.assertEqual(parsed["frequency_hz"], 477800000)
        self.assertEqual(parsed["mode"], 3)

    def test_record_split_across_frames(self):
        payload = bytes(range(256)) * 3
        stream = record(3, payload)
        decoder = RecordDecoder(); decoded = []
        decoded += decoder.feed_frame(data_frame(0, stream[:502]))
        decoded += decoder.feed_frame(data_frame(1, stream[502:]))
        self.assertEqual(len(decoded), 1)
        self.assertEqual(decoded[0].payload, payload)

    def test_capture_reassembly(self):
        records = []
        for antenna in range(8):
            body = bytearray(20)
            body[0:4] = bytes((1, 6, antenna, 1)); body[4:8] = (7).to_bytes(4, "big")
            body[8:12] = (4).to_bytes(4, "big"); body[12:16] = (0).to_bytes(4, "big")
            body[16:18] = (4).to_bytes(2, "big"); body += bytes((antenna,)) * 4
            frame = data_frame(antenna, record(3, body))
            records += RecordDecoder().feed_frame(frame)
        result = validate_capture(records)
        self.assertTrue(result["valid"])
        self.assertFalse(validate_capture(records, minimum_generation=8)["valid"])

    def test_sweep_two_generations(self):
        decoder = RecordDecoder(); records = []
        for seq, generation in enumerate((10, 11)):
            header = bytearray(32); header[:4] = bytes((1, 1, 6, 8))
            header[4:8] = generation.to_bytes(4, "big")
            header[8:12] = (477800000).to_bytes(4, "big")
            header[12:16] = (478675000).to_bytes(4, "big")
            header[16:20] = (125000).to_bytes(4, "big")
            header[20:24] = (8).to_bytes(4, "big"); header[28:30] = (8).to_bytes(2, "big")
            points = b"".join((477800000 + 125000 * i).to_bytes(4, "big") + struct.pack(">8f", *([-172.0] * 8)) for i in range(8))
            records += decoder.feed_frame(data_frame(seq, record(4, bytes(header) + points)))
        result = validate_sweep(records, 2)
        self.assertTrue(result["valid"])
        self.assertFalse(validate_sweep(records, 2, minimum_generation=12)["valid"])

    def test_sweep_current_33_points_in_chunks(self):
        records = []
        sequence = 0
        for start_index, point_count in ((0, 8), (8, 8), (16, 8), (24, 8), (32, 1)):
            header = bytearray(32); header[:4] = bytes((1, 1, 6, 8))
            header[4:8] = (20).to_bytes(4, "big")
            header[8:12] = (504000000).to_bytes(4, "big")
            header[12:16] = (508000000).to_bytes(4, "big")
            header[16:20] = (125000).to_bytes(4, "big")
            header[20:24] = (33).to_bytes(4, "big")
            header[24:28] = start_index.to_bytes(4, "big")
            header[28:30] = point_count.to_bytes(2, "big")
            points = b"".join(
                (504000000 + 125000 * i).to_bytes(4, "big") +
                struct.pack(">8f", *([-172.0] * 8))
                for i in range(start_index, start_index + point_count))
            records += RecordDecoder().feed_frame(
                data_frame(sequence, record(4, bytes(header) + points)))
            sequence += 1
        self.assertTrue(validate_sweep(records, 1, minimum_generation=20)["valid"])

    def test_uart_parsers(self):
        fpga = parse_fpga_tm("FPGA_TM rxFrames=3 rxErrors=0 unsupported=0 lastCmd=0x05 physicalRx=4 lastPhysicalRx=76 25 05 00 00 00 00 00 00 05\nFPGA_PARAM mode=3 rate=2 slotConfig=0 txPower=26 freqHz=477800000 rfMask=255 resetCount=1\nDT head=0 tail=0 ram=0 pending=0 active=0 starts=1 built=2 sent=2 sendErr=0")
        self.assertEqual(fpga["physical_rx"], 4); self.assertEqual(fpga["last_cmd"], 5)
        app = parse_app_tm("TM seq=1 uptimeMs=200 state=RUNNING mode=5\nPORT heap=0 spiErr=0\nACM pending=0 running=0 completed=2 last=0\nACM_RESULT valid=1 gen=2 timestampMs=1 validCount=1 antMask=0x000000FF last=0\nSWEEP gen=3 active=0 complete=1 points=8/8 last=0")
        self.assertEqual(app["acm_generation"], 2); self.assertEqual(app["sweep_points"], (8, 8))


if __name__ == "__main__": unittest.main()
