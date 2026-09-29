import io
import json
import unittest

from protocol import Config
from run import monitor


class FakeChannel:
    def __init__(self, events, exit_code=0):
        encoded = ("driver log\n"+"\n".join("FREQ_EVENT "+json.dumps(e) for e in events)+"\n").encode()
        self.chunks = [encoded[i:i+19] for i in range(0, len(encoded), 19)]
        self.exit_code = exit_code

    def recv_ready(self):
        return bool(self.chunks)

    def recv(self, limit):
        return self.chunks.pop(0)

    def exit_status_ready(self):
        return not self.chunks

    def recv_exit_status(self):
        return self.exit_code


class FakeTerminal:
    def check(self):
        pass


class FakeJournal:
    def __init__(self):
        self.records = []

    def emit(self, kind, **record):
        self.records.append(record)


def point(name, index=0, offset=0, elapsed=1000):
    return dict(event=name, index=index, offset_hz=offset, status=0, elapsed_ms=elapsed)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.config = Config(host="test", start_hz=0, end_hz=0, hold_seconds=1, boundary_guard_ms=0)

    def test_fragmented_events_and_complete_exit(self):
        events = [point("POINT_START"), point("POINT_END"), point("SWEEP_DONE", index=-1)]
        journal = FakeJournal()
        monitor(FakeChannel(events), FakeTerminal(), journal, io.StringIO(), self.config)
        self.assertEqual(len(journal.records), 3)

    def test_missing_done_nonzero_exit_short_hold_and_wrong_offset_fail(self):
        cases = [([point("POINT_START"), point("POINT_END")], 0),
                 ([point("POINT_END"), point("SWEEP_DONE", index=-1)], 0),
                 ([point("POINT_END"), point("SWEEP_DONE", index=-1)], 1),
                 ([point("POINT_END", elapsed=900)], 0),
                 ([point("POINT_START", offset=100)], 0),
                 ([point("POINT_FAILED")], 0)]
        for events, code in cases:
            with self.subTest(events=events), self.assertRaises(RuntimeError):
                monitor(FakeChannel(events, code), FakeTerminal(), FakeJournal(), io.StringIO(), self.config)


if __name__ == "__main__":
    unittest.main()
