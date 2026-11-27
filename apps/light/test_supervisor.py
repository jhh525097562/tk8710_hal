"""Linux process tests using a fake LED executable, never real GPIO/services."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
import unittest

HERE=Path(__file__).resolve().parent
class Tests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='led-supervisor-test-')
  self.root=Path(self.tmp.name)
  src=self.root/'agent.c';src.write_text('#include <unistd.h>\nint main(void){for(;;)pause();}\n')
  self.agent=self.root/'led_agent'
  subprocess.run(['gcc',str(src),'-o',str(self.agent)],check=True)
  self.script=self.root/'led_supervisor.sh';shutil.copyfile(HERE/'led_supervisor.sh',self.script);self.script.chmod(0o755)
  self.env=dict(os.environ,LED_SUPERVISOR_ROOT=str(self.root/'run'),LED_AGENT_BIN=str(self.agent),
                LED_SUPERVISOR_BIN=str(self.script),LED_SUPERVISOR_LOG=str(self.root/'guard.log'),LED_RETRY_SECONDS='1',LED_AGENT_PIDFILE=str(self.root/'legacy.pid'))
  self.proc=subprocess.Popen(['busybox','sh',str(self.script),'run'],env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  self.addCleanup(self.cleanup)
  self.pid=self.wait_led()
 def alive(self,pid):
  try:return Path(f'/proc/{pid}/exe').resolve(strict=True)==self.agent and Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()[0]!='Z'
  except OSError:return False
 def wait_led(self,old=None):
  for _ in range(80):
   try:p=int((self.root/'run/agent.pid').read_text())
   except (OSError,ValueError):p=0
   if p and p!=old and self.alive(p):return p
   time.sleep(.1)
  self.fail('LED was not launched')
 def ctl(self,*args,check=True):
  return subprocess.run(['busybox','sh',str(self.script),*map(str,args)],env=self.env,capture_output=True,text=True,timeout=12,check=check)
 def cleanup(self):
  try:self.ctl('stop',check=False)
  finally:
   if self.proc.poll() is None:self.proc.terminate()
   self.proc.wait(timeout=5)
   self.tmp.cleanup()
 def test_kill_restarts(self):
  os.kill(self.pid,signal.SIGKILL);new=self.wait_led(self.pid);self.assertNotEqual(new,self.pid)
 def test_term_restarts(self):
  os.kill(self.pid,signal.SIGTERM);self.wait_led(self.pid)
 def test_pause_and_resume(self):
  self.ctl('pause',os.getpid());os.kill(self.pid,signal.SIGTERM);time.sleep(2)
  self.assertFalse(self.alive(self.pid));self.assertEqual(int((self.root/'run/agent.pid').read_text()),self.pid)
  self.ctl('resume',os.getpid());self.wait_led(self.pid)
 def test_dead_test_owner_resumes(self):
  owner=subprocess.Popen(['sleep','20'])
  try:
   self.ctl('pause',owner.pid);os.kill(self.pid,signal.SIGTERM);time.sleep(1)
   owner.terminate();owner.wait();self.wait_led(self.pid)
  finally:
   if owner.poll() is None:owner.terminate();owner.wait()
 def test_expired_lease_resumes(self):
  self.ctl('pause',os.getpid());os.kill(self.pid,signal.SIGTERM)
  path=self.root/'run/test.lease';fields=path.read_text().split();fields[2]='0';path.write_text(' '.join(fields)+'\n')
  self.wait_led(self.pid)
 def test_admin_stop_no_respawn(self):
  self.ctl('stop');time.sleep(2);self.assertFalse(self.alive(self.pid));self.assertTrue((self.root/'run/supervisor.disabled').exists())
 def test_duplicate_supervisor(self):
  second=self.ctl('run');self.assertEqual(second.returncode,0);self.assertEqual(self.wait_led(),self.pid)
 def test_invalid_owner_and_other_lease(self):
  self.assertNotEqual(self.ctl('pause','invalid',check=False).returncode,0)
  self.ctl('pause',os.getpid())
  self.assertNotEqual(self.ctl('pause',os.getpid(),check=False).returncode,0)
 def test_stale_supervisor_pid_does_not_kill_other_process(self):
  other=subprocess.Popen(['sleep','20'])
  try:
   stat=Path(f'/proc/{other.pid}/stat').read_text().rsplit(')',1)[1].split()[19]
   (self.root/'run/supervisor.pid').write_text(f'{other.pid} {stat}\n')
   self.ctl('stop');self.assertIsNone(other.poll())
  finally:other.terminate();other.wait()
if __name__=='__main__':unittest.main(verbosity=2)
