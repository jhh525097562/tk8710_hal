"""Explicit LED-only restart and test-lease validation on RK3506."""
import hashlib,json,os,shlex,time
from pathlib import Path
import paramiko

HERE=Path(__file__).resolve().parent
host=os.environ.get('RK3506_HOST','192.168.100.202')
stamp=time.strftime('%Y%m%d_%H%M%S')
out=HERE/'logs'/('supervisor_'+host+'_'+stamp);out.mkdir(parents=True)
remote='/userdata/led_supervisor_verify_'+stamp
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy());c.connect(host,username='root',password=os.environ['RK3506_SSH_PASSWORD'],timeout=10)
s=c.open_sftp();data={'host':host,'backup':remote}
def run(cmd,check=True):
 _,o,e=c.exec_command(cmd,timeout=30)
 result={'stdout':o.read().decode(errors='replace'),'stderr':e.read().decode(errors='replace'),'rc':o.channel.recv_exit_status()}
 if check and result['rc']:raise RuntimeError(result)
 return result
def read(path):
 with s.open(path) as f:return f.read()
def led():
 p=run('pidof led_agent',False)['stdout'].strip()
 return int(p) if p.isdecimal() else None
def health():return run('cat /proc/sys/kernel/random/boot_id; pidof mqtt_bridge tk8710_gw web_server rk_wdt; cat /run/rk-watchdog/status.json')['stdout']
try:
 data['before']=health();data['history_before']=read('/run/led/arbiter_history').decode()
 s.mkdir(remote)
 files={'S37led_agent':'/etc/init.d/S37led_agent','led_supervisor.sh':'/usr/bin/led_supervisor.sh',
        'test_run_net.sh':'/userdata/test_run_net.sh','test_net_blink.sh':'/userdata/test_net_blink.sh'}
 for name,target in files.items():
  if run('test -f '+shlex.quote(target),False)['rc']==0:run('cp -p '+shlex.quote(target)+' '+shlex.quote(remote+'/'+name+'.old'))
  s.put(str(HERE/name),remote+'/'+name);s.chmod(remote+'/'+name,0o755)
  assert hashlib.sha256(read(remote+'/'+name)).digest()==hashlib.sha256((HERE/name).read_bytes()).digest()
  run('sh -n '+shlex.quote(remote+'/'+name))
 for name,target in files.items():
  run('cp '+shlex.quote(remote+'/'+name)+' '+shlex.quote(target+'.new')+' && chmod 755 '+shlex.quote(target+'.new')+' && mv -f '+shlex.quote(target+'.new')+' '+shlex.quote(target))
 data['start']=run('/etc/init.d/S37led_agent start')
 original=led();assert original
 assert read(f'/proc/{original}/cmdline').split(b'\0')[0]==b'/usr/bin/led_agent'
 print('Supervisor active, killing only LED PID',original,flush=True)
 start=time.monotonic();run('kill -KILL '+str(original))
 for _ in range(40):
  current=led()
  if current and current!=original:break
  time.sleep(.25)
 else:raise RuntimeError('LED restart not observed')
 data['restart']={'old_pid':original,'new_pid':current,'seconds':round(time.monotonic()-start,3)}
 print('Restart observed:',data['restart'],flush=True)
 time.sleep(4)
 data['normal_after_restart']=run('/usr/bin/led_agent --check; cat /sys/kernel/debug/gpio | grep led_arbiter; tail -5 /var/log/led_agent.log')['stdout']
 # The real RUN test must hold the lease for >5s, exceeding the retry interval.
 _,o,e=c.exec_command('sh /userdata/test_run_net.sh run > '+remote+'/run-test.log 2>&1',timeout=60)
 time.sleep(5)
 data['during_test']={'led_pid':led(),'lease':read('/run/led/test.lease').decode(),'gpio':run('cat /sys/kernel/debug/gpio | grep gpio-5')['stdout']}
 assert data['during_test']['led_pid'] is None,'Supervisor raced manual GPIO test'
 print('Manual RUN test lease active; LED agent correctly remains stopped.',flush=True)
 data['test_rc']=o.channel.recv_exit_status()
 assert data['test_rc']==0,e.read().decode()
 data['test_log']=read(remote+'/run-test.log').decode()
 time.sleep(4)
 assert led(),'LED did not resume after test'
 data['after']=health();data['history_after']=read('/run/led/arbiter_history').decode()
 data['final']=run('/etc/init.d/S37led_agent status; /usr/bin/led_agent --check; cat /sys/kernel/debug/gpio | grep led_arbiter; tail -8 /var/log/led_supervisor.log')['stdout']
 data['result']='pass'
 print(data['final'],flush=True)
finally:
 # Ensure normal LED control is restored even if a validation assertion failed.
 run('/usr/bin/led_supervisor.sh start',False)
 (out/'evidence.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
 print('EVIDENCE',out,flush=True);s.close();c.close()
