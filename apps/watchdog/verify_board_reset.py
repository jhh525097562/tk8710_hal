"""Explicit destructive watchdog validation: SIGSTOP or SIGKILL the feeder,
observe hardware timeout, then verify a new boot and automatic feeding.
Requires --host, --mode, --output and BOARD_PASSWORD. Never opens /dev/watchdog.
If the same boot survives beyond the deadline, restore feeding and report FAIL.
"""
import argparse, hashlib, json, os, pathlib, time
import paramiko, requests

p=argparse.ArgumentParser()
p.add_argument('--host',required=True)
p.add_argument('--mode',choices=['stop','kill'],required=True)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
start=time.monotonic();events=[];report={'host':a.host,'mode':a.mode,'events':events}
def save(): (a.output/(a.mode+'-result.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
def connect():
 c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
 c.connect(a.host,username='root',password=os.environ['BOARD_PASSWORD'],timeout=3,banner_timeout=4,auth_timeout=4)
 return c
def cmd(c,command):
 _,o,e=c.exec_command(command,timeout=8)
 text=o.read().decode('utf8',errors='replace');err=e.read().decode('utf8',errors='replace')
 if o.channel.recv_exit_status():raise RuntimeError(command+': '+err)
 return text
def read(c,path):
 with c.open_sftp() as s:
  with s.open(path,'rb') as f:return f.read()
def snapshot(c):
 result={'boot_id':read(c,'/proc/sys/kernel/random/boot_id').decode().strip(),
         'uptime_s':float(read(c,'/proc/uptime').split()[0]),
         'watchdog':json.loads(read(c,'/run/rk-watchdog/status.json')),
         'timeleft':int(read(c,'/sys/class/watchdog/watchdog0/timeleft')),
         'nowayout':read(c,'/sys/class/watchdog/watchdog0/nowayout').decode().strip(),
         'bootstatus':read(c,'/sys/class/watchdog/watchdog0/bootstatus').decode().strip()}
 return result
def collect(c,label):
 for path in ['/var/log/rk_watchdog.log','/root/daemon_guard.log','/var/log/mqtt_bridge.log']:
  try:(a.output/(label+'-'+path.rsplit('/',1)[1])).write_bytes(read(c,path))
  except OSError:pass
 (a.output/(label+'-dmesg.txt')).write_text(cmd(c,'dmesg'),encoding='utf8')
def web_health(cfg):
 session=requests.Session();session.trust_env=False;base='http://'+a.host+':8090'
 r=session.post(base+'/login',data={'username':'admin','password':cfg['web_password']},allow_redirects=False,timeout=5)
 assert r.status_code==302
 r=session.get(base+'/api/system_info',timeout=5);r.raise_for_status();return r.json()

c=connect();cfg=json.loads(read(c,'/etc/config.json'));report['gateway_id']=cfg['gateway_id']
report['config_sha256']=hashlib.sha256(read(c,'/etc/config.json')).hexdigest()
before=snapshot(c);report['before']=before;assert before['nowayout']=='1' and before['watchdog']['state']=='feeding'
assert before['uptime_s']-before['watchdog']['last_feed_uptime_s']<10
pid=before['watchdog']['pid'];assert cmd(c,'readlink /proc/'+str(pid)+'/exe').strip()=='/usr/sbin/rk_wdt'
report['web_before']=web_health(cfg)
collect(c,'before-'+a.mode)
samples=[]
for _ in range(3):
 time.sleep(5);samples.append(snapshot(c))
assert all(x['boot_id']==before['boot_id'] for x in samples)
assert samples[-1]['watchdog']['last_feed_uptime_s']>before['watchdog']['last_feed_uptime_s']+9
report['normal_feeding_samples']=samples;save()
print('PASS normal feeding; actual timeout',before['watchdog']['actual_timeout_s'],'pid',pid,flush=True)
cmd(c,'sync')
fresh=snapshot(c);assert fresh['boot_id']==before['boot_id'] and fresh['watchdog']['pid']==pid
signal='STOP' if a.mode=='stop' else 'KILL'
trigger=time.monotonic();cmd(c,'kill -'+signal+' '+str(pid))
report['signal_command']='kill -'+signal+' '+str(pid)
report['trigger_uptime_s']=fresh['uptime_s'];report['expected_timeleft_s']=fresh['timeleft'];save()
timeout=before['watchdog']['actual_timeout_s'];first_down=None;recovered=None
print('SIGNAL sent; waiting for hardware reset',flush=True)
while time.monotonic()-trigger < timeout+240:
 elapsed=time.monotonic()-trigger
 try:
  # Reconnect after every transport loss; never assume the IP implies same device.
  if c is None:c=connect()
  boot=read(c,'/proc/sys/kernel/random/boot_id').decode().strip()
  if boot!=before['boot_id']:
   newcfg=json.loads(read(c,'/etc/config.json'));assert newcfg['gateway_id']==report['gateway_id'],'different gateway at target IP'
   recovered=snapshot(c);report['recovered']=recovered
   report['reconnect_elapsed_s']=round(elapsed,2)
   save();print('NEW BOOT',boot,'after',round(elapsed,2),'seconds',flush=True);break
  remaining=int(read(c,'/sys/class/watchdog/watchdog0/timeleft'))
  events.append({'elapsed_s':round(elapsed,2),'timeleft_s':remaining,'boot_id':boot})
  if elapsed>25 and 'web_during_stopped_feeding' not in report:
   try:
    report['web_during_stopped_feeding']=web_health(cfg).get('watchdog')
    print('WEB during stopped feeding:',report['web_during_stopped_feeding'].get('state'),flush=True)
   except Exception:pass
  if len(events)%5==0:print('waiting',round(elapsed,1),'seconds; timeleft',remaining,flush=True)
  if elapsed>timeout+20:
   restore='kill -CONT '+str(pid) if a.mode=='stop' else '/etc/init.d/S15rk-watchdog start'
   cmd(c,restore);report['restored_after_failed_reset']=restore;save();raise RuntimeError('FAIL: same boot survived timeout; feeding restored')
 except (OSError,EOFError,paramiko.SSHException) as exc:
  if first_down is None:
   first_down=elapsed;report['first_unreachable_elapsed_s']=round(elapsed,2);print('TRANSPORT DOWN',round(elapsed,2),flush=True)
  if c:c.close()
  c=None
 time.sleep(2);save()
assert recovered,'FAIL: gateway not reachable again at this IP; locate it by gateway_id'
assert recovered['watchdog']['state']=='feeding' and recovered['nowayout']=='1'
post_samples=[]
for _ in range(3):
 time.sleep(5);post_samples.append(snapshot(c))
assert all(x['boot_id']==recovered['boot_id'] for x in post_samples)
assert post_samples[-1]['watchdog']['last_feed_uptime_s']>recovered['watchdog']['last_feed_uptime_s']+9
report['post_feeding_samples']=post_samples
report['reset_uptime_estimate_s']=round((time.monotonic()-trigger)-post_samples[-1]['uptime_s'],2)
for _ in range(70):
 try:
  health=web_health(cfg)
  if health.get('watchdog',{}).get('state')=='feeding':report['web_after']=health;break
 except Exception:pass
 time.sleep(2)
else:raise RuntimeError('Web did not recover with feeding status')
report['processes_after']=cmd(c,'for n in rk_wdt mqtt_bridge web_server tk8710_gw frpc; do printf "%s: " "$n"; pidof "$n" || true; done')
assert hashlib.sha256(read(c,'/etc/config.json')).hexdigest()==report['config_sha256'],'config changed'
collect(c,'after-'+a.mode)
report['hardware_reset_pass']=True;report['boot_reason_register_proven']=False
save();print('PASS watchdog reset, automatic restart, Web feeding status, config unchanged',flush=True)
c.close()
