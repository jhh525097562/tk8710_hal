"""ARM/QEMU regression: run production main and registered MQTT callbacks.

Simulated broker/GPIO/IPC, isolated files, actual status writer and GPS packer.
The test must fail if main loses its observer initialization, callbacks or tick.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('SDK_ROOT', '/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0'))
HOST = SDK / 'buildroot/output/rockchip_hd_rk3506b_iot_emmc/host'


class MainStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='mqtt-main-status-')
        cls.exe = Path(cls.build.name) / 'test'
        command = [str(HOST / 'bin/arm-buildroot-linux-gnueabihf-gcc'),
                   '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O0',
                   '-I' + str(APP / 'inc'), '-I' + str(APP.parent / 'common/inc'),
                   str(APP / 'tests/test_main_status.c')]
        command += [str(APP / ('src/' + f + '.c')) for f in
                    ['mqtt_status', 'gps_info', 'mac_msg_parser', 'gateway_telemetry']]
        command += ['-Wl,--wrap=' + f for f in
                    ['fopen', 'mkdir', 'rename', 'clock_gettime', 'mqtt_status_init',
                     'mosquitto_new', 'epoll_wait']]
        command += ['-lmosquitto', '-ljson-c', '-lmbedcrypto', '-lgpiod', '-pthread',
                    '-o', str(cls.exe)]
        try:
            subprocess.run(command, check=True)
        except BaseException:
            cls.build.cleanup()
            raise
        cls.runner = ['qemu-arm-static', '-L', str(HOST / 'arm-buildroot-linux-gnueabihf/sysroot')]

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def run_case(self, scenario):
        with tempfile.TemporaryDirectory(prefix='mqtt-main-files-') as directory:
            root = Path(directory)
            proc = root / 'proc'
            proc.write_text(Path('/proc/self/stat').read_text())
            gps = root / 'gps'
            gps.write_text('lon=114.5\nlat=30.4\nstatus=18\ncount=32\navgsnr=37\n')
            result = subprocess.run(self.runner + [str(self.exe), scenario, str(root / 'led')],
                                    env=dict(os.environ, PROC_FIXTURE=str(proc), GPS_FIXTURE=str(gps)),
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            snapshots = {}
            for line in result.stdout.splitlines():
                if line.startswith('SNAP '):
                    _, label, payload = line.split(' ', 2)
                    snapshots[label] = json.loads(payload)
            self.assertIn('exit', snapshots, result.stdout)
            if scenario == 'normal':
                legacy = (root / 'led/mqtt.state').read_text()
                self.assertIn('STATUS=connected', legacy)
                self.assertIn('TS_MS=', legacy)
            return snapshots, result.stdout

    def test_lifecycle_refresh_and_gps_compatibility(self):
        s, output = self.run_case('normal')
        for label in ['startup', 'connack', 'wrong_mid', 'disconnected', 'late_ack',
                      'heartbeat_disconnected', 'reconnecting', 'old_mid', 'teardown', 'exit']:
            self.assertFalse(s[label]['ready'], label)
        for label in ['suback', 'refreshed', 'recovered']:
            self.assertTrue(s[label]['ready'], label)
        self.assertGreater(s['refreshed']['updated_mono_ms'], s['suback']['updated_mono_ms'])
        self.assertEqual(s['exit']['state'], 'stopped')
        self.assertTrue(s['exit']['exited'])
        self.assertIn('RESULT 0', output)

    def test_broker_and_subscription_failures(self):
        for scenario, reason in [('broker_rejected', 'broker_rejected'),
                                 ('subscribe_error', 'subscribe_request_failed'),
                                 ('rejected_suback', 'subscription_rejected'),
                                 ('missing_topic', 'subscription_topic_missing')]:
            with self.subTest(scenario=scenario):
                s, _ = self.run_case(scenario)
                self.assertEqual(s['exit']['reason'], reason)
                self.assertEqual(s['exit']['state'], 'fault')
                self.assertFalse(s['exit']['ready'])

    def test_connection_and_suback_timeouts(self):
        for scenario, reason in [('connect_error', 'connection_timeout'),
                                 ('suback_timeout', 'suback_timeout')]:
            with self.subTest(scenario=scenario):
                s, _ = self.run_case(scenario)
                self.assertEqual(s['refreshed']['reason'], reason)
                if scenario == 'connect_error':
                    self.assertEqual(s['first_tick']['reason'], 'connect_attempt_failed')
                self.assertFalse(s['refreshed']['ready'])

    def test_main_failures_preserved_on_exit(self):
        for scenario, reason in [('config_error', 'config_load_failed'),
                                 ('ipc_error', 'ipc_init_failed'),
                                 ('loop_error', 'mqtt_loop_start_failed'),
                                 ('epoll_error', 'epoll_wait_failed'),
                                 ('epoll_hup', 'epoll_fd_error')]:
            with self.subTest(scenario=scenario):
                s, output = self.run_case(scenario)
                self.assertEqual(s['exit']['reason'], reason)
                self.assertTrue(s['exit']['exited'])
                self.assertFalse(s['exit']['ready'])
                self.assertIn('RESULT 1', output)


if __name__ == '__main__':
    unittest.main(verbosity=2)
