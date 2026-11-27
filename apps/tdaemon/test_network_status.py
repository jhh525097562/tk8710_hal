"""BusyBox ash isolated network observer tests; no real link/route changes."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

HERE=Path(__file__).resolve().parent
class Tests(unittest.TestCase):
    def test_usb_recovery_preserved_with_observer(self):
        script='''
. "$MODULE"
log() { :; }
network_status_event() { echo "event $*" >> "$TRACE"; }
gpio_modem_reset_pulse() { echo reset >> "$TRACE"; }
gpio_modem_power_cycle() { echo power_cycle >> "$TRACE"; }
net_usb_tty_present() { return 1; }
net_usb_wait_tty_and_ip() { return 1; }
net_prepare_candidate_iface usb0 target 1
net_prepare_candidate_iface usb0 target 1
net_prepare_candidate_iface usb0 target 1
[ "$usb0_candidate_reset_count" = 0 ]
'''
        trace=self.root/'trace'
        result=subprocess.run(['busybox','sh'],input=script,text=True,capture_output=True,
            env=dict(os.environ,MODULE=str(HERE/'tdaemon_net.sh'),TRACE=str(trace)),timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        calls=trace.read_text().splitlines()
        self.assertEqual(calls.count('reset'),3)
        self.assertEqual(calls.count('power_cycle'),1)
        self.assertEqual(calls.count('event checking usb0 candidate_probe'),3)

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='network-status-test-');self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
    def render(self,control='up eth0 probe_passed 100000 up eth0 100000',progress='100000 eth0,usb0,wlan0',now=101000,link=True,ip=True,extra=''):
        (self.root/'control').write_text(control+'\n');(self.root/'progress').write_text(progress+'\n')
        script='''
. "$MODULE"
network_status_link() {
 NETWORK_STATUS_LINK_UP=$LINK; NETWORK_STATUS_IPV4_READY=$IP
 [ "$LINK" = true ] && [ "$IP" = true ]
}
'''+extra+'''
network_status_render 123 456 01234567-89ab-cdef-0123-456789abcdef "$NOW" "$DATA"
'''
        r=subprocess.run(['busybox','sh'],input=script,text=True,capture_output=True,
            env=dict(os.environ,MODULE=str(HERE/'tdaemon_status.sh'),DATA=str(self.root),NOW=str(now),
                     LINK=str(link).lower(),IP=str(ip).lower()),timeout=5)
        self.assertEqual(r.returncode,0,r.stderr);return json.loads(r.stdout)
    def test_up(self):
        s=self.render();self.assertTrue(s['ready']);self.assertEqual(s['state'],'up')
    def test_cable_loss_without_waiting_for_probe(self):
        s=self.render(link=False);self.assertEqual(s['state'],'switching');self.assertFalse(s['ready'])
    def test_ip_loss_separate_from_carrier(self):
        s=self.render(ip=False);self.assertTrue(s['link_up']);self.assertFalse(s['ipv4_ready'])
    def test_replug_needs_fresh_probe(self):
        s=self.render(extra='NETWORK_STATUS_INVALIDATED_CHECKED=100000\n')
        self.assertEqual(s['reason'],'awaiting_fresh_probe');self.assertFalse(s['ready'])
    def test_fresh_probe_after_replug(self):
        s=self.render(control='up eth0 probe_passed 100001 up eth0 100001',extra='NETWORK_STATUS_INVALIDATED_CHECKED=100000\n')
        self.assertTrue(s['ready'])
    def test_switching_invalidates_old_up(self):
        s=self.render(control='switching eth0 link_recovery 100000 unknown none 99999')
        self.assertFalse(s['ready']);self.assertEqual(s['state'],'switching')
    def test_slow_probe_keeps_only_fresh_up(self):
        s=self.render(control='checking eth0 periodic_probe 100000 up eth0 99000')
        self.assertTrue(s['ready']);self.assertEqual(s['checked_boottime_ms'],99000)
    def test_old_result_not_renewed_by_observer(self):
        s=self.render(progress='150000 eth0',now=150001)
        self.assertEqual(s['reason'],'probe_result_stale');self.assertFalse(s['ready'])
    def test_stuck_parent_not_masked(self):
        s=self.render(now=150001);self.assertEqual(s['reason'],'publisher_progress_stale')
    def test_probe_timeout(self):
        s=self.render(control='checking eth0 periodic_probe 100000 unknown none 0',progress='170000 eth0',now=170001)
        self.assertEqual(s['reason'],'probe_timeout')
    def test_all_down(self):
        s=self.render(control='down none no_qualified_interface 100000 down none 100000')
        self.assertEqual(s['state'],'down');self.assertFalse(s['ready'])
    def test_4g_active_is_network_up(self):
        s=self.render(control='up usb0 probe_passed 100000 up usb0 100000')
        self.assertEqual(s['active_iface'],'usb0');self.assertTrue(s['ready'])
    def test_disabled_active_not_ready(self):
        s=self.render(progress='100000 usb0,wlan0');self.assertEqual(s['reason'],'active_interface_disabled')
    def test_future_timestamp_unknown(self):
        s=self.render(now=90000);self.assertEqual(s['state'],'unknown')
    def test_allowed_and_event_writer(self):
        script='''
. "$MODULE"
NETWORK_STATUS_DIR=$DATA
ACTIVE_NET_DISABLED=usb0,wlan0
network_status_now() { echo 100000; }
network_status_progress
network_status_event up eth0 probe_passed
network_status_event switching eth0 link_recovery
network_status_event checking eth0 periodic_probe
cat "$DATA/control"
cat "$DATA/progress"
'''
        r=subprocess.run(['busybox','sh'],input=script,text=True,capture_output=True,
            env=dict(os.environ,MODULE=str(HERE/'tdaemon_status.sh'),DATA=str(self.root)),check=True)
        self.assertIn('unknown none',r.stdout);self.assertIn('100000 eth0',r.stdout)
    def test_live_worker_exits_after_parent(self):
        script='''
. "$MODULE"
network_status_start || exit 1
echo "$NETWORK_STATUS_WORKER"
sleep 3
'''
        p=subprocess.Popen(['busybox','sh'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,
            text=True,env=dict(os.environ,MODULE=str(HERE/'tdaemon_status.sh'),NETWORK_STATUS_ROOT=str(self.root)))
        p.stdin.write(script);p.stdin.close()
        worker=int(p.stdout.readline())
        time.sleep(1.3)
        s=json.loads((self.root/'network_status.json').read_text())
        self.assertEqual(s['pid'],p.pid);self.assertFalse(s['ready'])
        p.wait(timeout=5);p.stdout.close();time.sleep(1.3)
        stat=Path(f'/proc/{worker}/stat')
        self.assertTrue(not stat.exists() or stat.read_text().rsplit(')',1)[1].split()[0]=='Z')

    def test_existing_net_tick_publishes_both_protocols(self):
        # Relocate only legacy /run/led writes; keep actual net_tick/event wiring.
        net=self.root/'net.sh'
        net.write_text((HERE/'tdaemon_net.sh').read_text().replace('/run/led',str(self.root)))
        script='''
. "$MODULE"
. "$NET_MODULE"
NETWORK_STATUS_DIR=$DATA
ACTIVE_NET_DISABLED=
ACTIVE_NET_TARGETS=127.0.0.1
ACTIVE_NET_PING_COUNT=1
ACTIVE_NET_FAIL_THRESHOLD=3
CURRENT_IFACE=eth0
network_status_now() { echo 100000; }
log() { :; }
net_is_disabled() { return 1; }
net_check_iface_qualified() { return "$PROBE_RC"; }
recovery_iface_reset_state() { :; }
net_route_activate_iface() { :; }
net_tick
cat "$DATA/control"
cat "$DATA/net.state"
'''
        env=dict(os.environ,MODULE=str(HERE/'tdaemon_status.sh'),NET_MODULE=str(net),DATA=str(self.root))
        r=subprocess.run(['busybox','sh'],input=script,text=True,capture_output=True,env=dict(env,PROBE_RC='0'),check=True)
        self.assertIn('up eth0 probe_passed',r.stdout);self.assertIn('STATUS=L3_OK',r.stdout)
        r=subprocess.run(['busybox','sh'],input=script,text=True,capture_output=True,env=dict(env,PROBE_RC='1'),check=True)
        self.assertIn('switching eth0 link_recovery',r.stdout);self.assertIn('STATUS=RECOVERING',r.stdout)

if __name__=='__main__':unittest.main(verbosity=2)
