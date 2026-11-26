"""ARM/QEMU lifecycle tests, replaying broker callbacks without hardware/network."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

APP=Path(__file__).resolve().parents[1]
SDK=Path(os.environ.get('SDK_ROOT','/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0'))
HOST=SDK/'buildroot/output/rockchip_hd_rk3506b_iot_emmc/host'
HARNESS=r'''
#define _POSIX_C_SOURCE 200809L
#include "mqtt_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static long tick=100;
int __wrap_clock_gettime(clockid_t id,struct timespec *t) {
 (void)id; t->tv_sec=tick;t->tv_nsec=0;return 0;
}
FILE *__real_fopen(const char*,const char*);
FILE *__wrap_fopen(const char *p,const char *m) {
 if (!strncmp(p,"/proc/",6) && strstr(p,"/stat")) return __real_fopen(getenv("PROC_FIXTURE"),m);
 return __real_fopen(p,m);
}
int main(int argc,char **argv) {
 if(argc!=2 || mqtt_status_init(argv[1]))return 3;
 puts("READY");fflush(stdout);
 char buf[100];int a,b,q;
 while(fgets(buf,sizeof(buf),stdin)) {
  if(sscanf(buf,"conn %d",&a)==1)mqtt_status_connack(a);
  else if(sscanf(buf,"subscribe %d %d",&a,&b)==2)mqtt_status_subscribe(a,b);
  else if(sscanf(buf,"ack %d %d %d",&a,&b,&q)==3)mqtt_status_suback(a,b,&q);
  else if(sscanf(buf,"disconnect %d",&a)==1)mqtt_status_disconnect(a);
  else if(sscanf(buf,"error %d",&a)==1)mqtt_status_connect_error(a);
  else if(sscanf(buf,"tick %d",&a)==1){tick+=a;mqtt_status_tick();}
  else if(!strncmp(buf,"fault",5))mqtt_status_fault("test_failure",-9);
  else if(!strncmp(buf,"stop",4))mqtt_status_stop();
  else return 4;
  puts("OK");fflush(stdout);
 }
 mqtt_status_stop();return 0;
}
'''

class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=tempfile.TemporaryDirectory(prefix='mqtt-status-build-')
        p=Path(cls.build.name);(p/'h.c').write_text(HARNESS);cls.exe=p/'test'
        subprocess.run([str(HOST/'bin/arm-buildroot-linux-gnueabihf-gcc'),'-std=c99','-Wall','-Wextra','-Werror',
            '-I'+str(APP/'inc'),str(p/'h.c'),str(APP/'src/mqtt_status.c'),'-pthread','-ljson-c',
            '-Wl,--wrap=fopen','-Wl,--wrap=clock_gettime','-o',str(cls.exe)],check=True)
        cls.runner=['qemu-arm-static','-L',str(HOST/'arm-buildroot-linux-gnueabihf/sysroot')]
    @classmethod
    def tearDownClass(cls):cls.build.cleanup()
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='mqtt-status-test-');self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.directory=self.root/'led'
        stat=self.root/'proc';stat.write_text(Path('/proc/self/stat').read_text())
        self.env=dict(os.environ,PROC_FIXTURE=str(stat))
        self.p=subprocess.Popen(self.runner+[str(self.exe),str(self.directory)],env=self.env,
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.addCleanup(self.close)
        self.assertEqual(self.p.stdout.readline().strip(),'READY')
    def close(self):
        self.p.stdin.close()
        try:self.p.wait(timeout=5)
        except subprocess.TimeoutExpired:self.p.kill();self.p.wait()
        self.p.stdout.close();self.p.stderr.close()
    def read(self):return json.loads((self.directory/'mqtt_status.json').read_text())
    def cmd(self,c):
        self.p.stdin.write(c+'\n');self.p.stdin.flush()
        self.assertEqual(self.p.stdout.readline().strip(),'OK');return self.read()
    def ready(self):
        self.cmd('conn 0');self.cmd('subscribe 42 0');return self.cmd('ack 42 1 0')
    def test_connack_alone_not_ready(self):
        s=self.cmd('conn 0');self.assertTrue(s['connack_ok']);self.assertFalse(s['ready'])
    def test_successful_suback_required(self):
        s=self.ready();self.assertTrue(s['ready']);self.assertEqual(s['state'],'connected')
    def test_wrong_mid_ignored(self):
        self.cmd('conn 0');self.cmd('subscribe 42 0')
        self.assertFalse(self.cmd('ack 43 1 0')['ready'])
    def test_rejected_suback(self):
        self.cmd('conn 0');self.cmd('subscribe 42 0')
        s=self.cmd('ack 42 1 128');self.assertEqual(s['state'],'fault');self.assertFalse(s['ready'])
    def test_zero_qos_count_rejected(self):
        self.cmd('conn 0');self.cmd('subscribe 42 0');self.assertEqual(self.cmd('ack 42 0 0')['state'],'fault')
    def test_disconnect_revokes_ready_and_late_ack_ignored(self):
        self.ready();s=self.cmd('disconnect 7');self.assertFalse(s['ready']);self.assertEqual(s['state'],'reconnecting')
        self.assertFalse(self.cmd('ack 42 1 0')['ready'])
    def test_reconnect_requires_new_suback(self):
        self.ready();self.cmd('disconnect 1');self.cmd('conn 0');self.cmd('subscribe 43 0')
        self.assertFalse(self.cmd('ack 42 1 0')['ready']);self.assertTrue(self.cmd('ack 43 1 1')['ready'])
    def test_heartbeat_never_claims_connected(self):
        s=self.cmd('tick 1');self.assertFalse(s['ready']);self.assertEqual(s['state'],'connecting')
        s=self.cmd('tick 10');self.assertEqual(s['reason'],'connection_timeout')
    def test_suback_timeout(self):
        self.cmd('conn 0');self.cmd('subscribe 42 0')
        self.assertEqual(self.cmd('tick 11')['reason'],'suback_timeout')
    def test_broker_rejection(self):
        s=self.cmd('conn 5');self.assertEqual(s['reason'],'broker_rejected');self.assertFalse(s['ready'])
    def test_subscribe_call_failure(self):
        self.cmd('conn 0');s=self.cmd('subscribe -1 4');self.assertEqual(s['reason'],'subscribe_request_failed')
    def test_stop_and_fault_preservation(self):
        self.ready();self.cmd('fault')
        s=self.cmd('conn 0');self.assertEqual(s['reason'],'test_failure')
        s=self.cmd('stop')
        self.assertTrue(s['exited']);self.assertEqual(s['reason'],'test_failure')
        self.assertFalse(self.cmd('conn 0')['ready'])
    def test_normal_stop(self):
        self.ready();s=self.cmd('stop');self.assertEqual(s['state'],'stopped');self.assertFalse(s['ready'])
    def test_process_identity_and_no_credentials(self):
        s=self.read();self.assertEqual(s['pid'],self.p.pid);self.assertEqual(s['clock'],'monotonic')
        self.assertNotIn('password',s);self.assertNotIn('topic',s);self.assertGreater(s['start_ticks'],0)
    def test_second_publisher_refused(self):
        before=self.read()
        r=subprocess.run(self.runner+[str(self.exe),str(self.directory)],env=self.env,capture_output=True,timeout=5)
        self.assertEqual(r.returncode,3);self.assertEqual(self.read(),before)

if __name__=='__main__':unittest.main(verbosity=2)
