"""Host or ARM/QEMU isolated lifecycle snapshot tests; no GPIO/SPI/board I/O."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
#include "tk8710_gw_status.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FILE* __real_fopen(const char*,const char*);
FILE* __wrap_fopen(const char* path,const char* mode) {
    const char* fixture=getenv("GW_STATUS_PROC_FIXTURE");
    if (fixture && !strncmp(path,"/proc/",6) && strstr(path,"/stat"))
        return __real_fopen(fixture,mode);
    return __real_fopen(path,mode);
}
static void* writer(void* unused) {
    (void)unused;
    for (int i = 0; i < 100; ++i) {
        GwStatusSet(GW_STATUS_RUNNING, "thread_test", 0);
        GwStatusTouch();
    }
    return NULL;
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (GwStatusInit(argv[1])) { perror("GwStatusInit"); return 3; }
    puts("READY"); fflush(stdout);
    char command[128];
    while (fgets(command, sizeof(command), stdin)) {
        int state, code;
        char reason[64];
        if (sscanf(command,"set %d %63s %d",&state,reason,&code)==3)
            GwStatusSet((GwStatusPhase)state,reason,code);
        else if (sscanf(command,"gps %d",&code)==1) GwStatusSetGpsPolicy(code);
        else if (sscanf(command,"finish %d",&code)==1) GwStatusFinish(code);
        else if (!strncmp(command,"touch",5)) GwStatusTouch();
        else if (!strncmp(command,"stress",6)) {
            pthread_t a,b;
            if (pthread_create(&a,NULL,writer,NULL) || pthread_create(&b,NULL,writer,NULL)) return 4;
            pthread_join(a,NULL); pthread_join(b,NULL);
        } else return 5;
        puts("OK"); fflush(stdout);
    }
    GwStatusFinish(0);
    return 0;
}
'''

class StatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='gw-status-build-')
        root=Path(cls.build.name)
        (root/'harness.c').write_text(HARNESS)
        cls.exe=root/'status-test'
        cc=shlex.split(os.environ.get('GW_STATUS_TEST_CC','gcc'))
        subprocess.run(cc+['-std=c99','-Wall','-Wextra','-Werror','-pthread',
            '-I'+str(ROOT/'src/gateway'),str(root/'harness.c'),
            str(ROOT/'src/gateway/tk8710_gw_status.c'),'-Wl,--wrap=fopen','-o',str(cls.exe)],check=True)
        cls.runner=shlex.split(os.environ.get('GW_STATUS_TEST_RUNNER',''))

    @classmethod
    def tearDownClass(cls): cls.build.cleanup()

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='gw-status-data-')
        self.addCleanup(self.tmp.cleanup)
        self.directory=Path(self.tmp.name)/'run'
        environment=dict(os.environ)
        if self.runner:
            # QEMU user-mode synthesizes /proc/<self>/stat with starttime=0.
            # Only this read is replaced; native tests check actual PID identity.
            fixture=Path(self.tmp.name)/'proc-stat'
            fixture.write_text(Path('/proc/self/stat').read_text())
            environment['GW_STATUS_PROC_FIXTURE']=str(fixture)
        self.environment=environment
        self.proc=subprocess.Popen(self.runner+[str(self.exe),str(self.directory)],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=environment)
        self.addCleanup(self.close_process)
        first=self.proc.stdout.readline().strip()
        if first != 'READY':
            self.proc.wait(timeout=5)
            self.fail(self.proc.stderr.read())

    def close_process(self):
        self.proc.stdin.close()
        try:self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:self.proc.kill();self.proc.wait()
        self.proc.stdout.close();self.proc.stderr.close()

    def command(self, text):
        self.proc.stdin.write(text+'\n');self.proc.stdin.flush()
        self.assertEqual(self.proc.stdout.readline().strip(),'OK')
        return self.read()

    def read(self):return json.loads((self.directory/'status.json').read_text())

    def test_identity_and_unknown_policy(self):
        s=self.read()
        self.assertEqual(s['state'],'starting');self.assertEqual(s['gps_enable'],-1)
        self.assertEqual(s['pid'],self.proc.pid)
        self.assertEqual(s['boot_id'],Path('/proc/sys/kernel/random/boot_id').read_text().strip())
        stat_path=Path(self.tmp.name)/'proc-stat' if self.runner else Path(f'/proc/{self.proc.pid}/stat')
        fields=stat_path.read_text().rsplit(')',1)[1].split()
        self.assertEqual(s['start_ticks'],int(fields[19]));self.assertFalse(s['business_ready'])

    def test_external_startup_phases(self):
        self.command('set 1 awaiting_ns_config 0')
        self.command('gps 1')
        s=self.command('set 3 gps_acquisition 0')
        self.assertEqual(s['phase_timeout_s'],330);self.assertFalse(s['business_ready'])
        s=self.command('set 4 pps_period_alignment 0')
        self.assertEqual(s['phase_timeout_s'],90)
        self.command('set 2 hal_configuration 0')
        s=self.command('set 5 business_started 0')
        self.assertEqual(s['gps_enable'],1);self.assertTrue(s['business_ready'])

    def test_local_sync_is_ready_without_gps(self):
        self.command('gps 0');s=self.command('set 5 business_started 0')
        self.assertEqual(s['gps_enable'],0);self.assertTrue(s['business_ready'])

    def test_fault_latches_first_reason(self):
        self.command('set 10 gps_module_unavailable -3')
        self.command('set 5 late_callback 0')
        self.command('set 10 generic_failure -1')
        s=self.command('finish 1')
        self.assertEqual(s['reason'],'gps_module_unavailable');self.assertEqual(s['error_code'],-3)
        self.assertTrue(s['exited']);self.assertFalse(s['business_ready'])
        previous=json.loads((self.directory/'last_exit.json').read_text())
        self.assertEqual(previous['reason'],s['reason'])
        self.assertEqual(previous['pid'],s['pid'])
        self.assertEqual(previous['error_code'],s['error_code'])
        self.assertTrue(previous['exited'])

    def test_last_exit_survives_new_process(self):
        self.command('set 10 md_data_irq_stalled -1')
        self.command('finish 1')
        before=json.loads((self.directory/'last_exit.json').read_text())
        with subprocess.Popen(self.runner+[str(self.exe),str(self.directory)],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                text=True,env=self.environment) as next_proc:
            try:
                self.assertEqual(next_proc.stdout.readline().strip(),'READY')
                self.assertNotEqual(self.read()['pid'],before['pid'])
                self.assertEqual(self.read()['state'],'starting')
                self.assertEqual(json.loads((self.directory/'last_exit.json').read_text()),before)
            finally:
                next_proc.communicate(timeout=5)

    def test_restart_reason_retained_on_finish(self):
        self.command('set 8 gps_recovered_restart 0')
        s=self.command('finish 0')
        self.assertEqual(s['reason'],'gps_recovered_restart')
        self.assertEqual(s['state'],'stopped')

    def test_scan_and_restore(self):
        self.command('set 5 business_started 0')
        s=self.command('set 7 frequency_sweep 0');self.assertFalse(s['business_ready'])
        self.command('set 2 restore_after_scan 0')
        s=self.command('set 5 business_restored 0');self.assertTrue(s['business_ready'])

    def test_degraded_and_recovered(self):
        self.command('set 5 business_started 0')
        s=self.command('set 6 gps_health_warning 0');self.assertFalse(s['business_ready'])
        s=self.command('set 5 business_running 0');self.assertTrue(s['business_ready'])

    def test_shutdown_does_not_resurrect(self):
        self.command('set 8 shutdown_requested 0')
        s=self.command('set 5 late_callback 0');self.assertEqual(s['state'],'stopping')
        s=self.command('finish 0');self.assertEqual(s['state'],'stopped');self.assertTrue(s['exited'])
        s=self.command('set 5 late_callback 0');self.assertEqual(s['state'],'stopped')

    def test_tick_does_not_reset_phase_deadline(self):
        before=self.command('set 3 gps_acquisition 0');time.sleep(1.05)
        after=self.command('touch')
        self.assertGreater(after['updated_mono_ms'],before['updated_mono_ms'])
        self.assertEqual(after['state_since_mono_ms'],before['state_since_mono_ms'])

    def test_duplicate_publisher_refused(self):
        old=(self.directory/'status.json').read_bytes()
        result=subprocess.run(self.runner+[str(self.exe),str(self.directory)],capture_output=True,timeout=5)
        self.assertEqual(result.returncode,3)
        self.assertEqual((self.directory/'status.json').read_bytes(),old)

    def test_tmp_symlink_does_not_overwrite_other_file(self):
        victim=Path(self.tmp.name)/'victim';victim.write_text('untouched')
        link=self.directory/f'status.{self.proc.pid}.tmp';link.symlink_to(victim)
        self.command('set 5 business_started 0')
        self.assertEqual(victim.read_text(),'untouched')
        self.assertEqual(self.read()['state'],'starting')
        s=self.command('touch');self.assertIn(s['state'],['starting','running'])

    def test_reader_never_sees_partial_json(self):
        errors=[];stop=threading.Event()
        def reader():
            while not stop.is_set():
                try:self.read()
                except Exception as error:errors.append(error)
        thread=threading.Thread(target=reader);thread.start()
        try:self.command('stress')
        finally:stop.set();thread.join()
        self.assertEqual(errors,[])

    def test_reason_safe_and_invalid_state_ignored(self):
        s=self.command('set 2 bad"reason 0')
        self.assertEqual(s['reason'],'bad_reason')
        s=self.command('set 99 invalid 0');self.assertEqual(s['state'],'configuring')

if __name__=='__main__': unittest.main(verbosity=2)
