"""Explicit lab validation: prepare base, upload SWU through Web, verify after reboot.

Requires paramiko and requests. SSH password comes from BOARD_PASSWORD.
prepare installs the SDK recovery/daemon/Web files and briefly restarts Web/daemon.
install replaces the three apps and REBOOTS the selected board.
"""
import argparse, hashlib, json, os, pathlib, shlex, sys, time
import paramiko
import requests

sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser()
p.add_argument('action',choices=['snapshot','prepare','install','verify','reject'])
p.add_argument('--host',required=True)
p.add_argument('--sdk',required=True, type=pathlib.Path)
p.add_argument('--package',required=True,type=pathlib.Path)
p.add_argument('--evidence',required=True,type=pathlib.Path)
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(a.host,username='root',password=os.environ['BOARD_PASSWORD'],timeout=8)
s=c.open_sftp()
def command(cmd,check=True):
 _,out,err=c.exec_command(cmd,timeout=45)
 text=out.read().decode('utf8',errors='replace')+err.read().decode('utf8',errors='replace')
 rc=out.channel.recv_exit_status()
 if check and rc:raise RuntimeError(f'command failed ({rc}): {cmd}\n{text}')
 return text
def read(path):
 with s.open(path,'rb') as f:return f.read()
def snapshot():
 paths=['/etc/config.json','/etc/network/interfaces','/etc/tdaemon.conf','/userdata/frp/frpc.toml']
 hashes={}
 for path in paths:
  try:hashes[path]=hashlib.sha256(read(path)).hexdigest()
  except FileNotFoundError:pass
 return {'host':a.host,'boot_id':read('/proc/sys/kernel/random/boot_id').decode().strip(),
         'preserved_sha256':hashes,'binaries':{n:hashlib.sha256(read('/userdata/'+n)).hexdigest() for n in ['mqtt_bridge','web_server','tk8710_gw']},
         'version':json.loads(read('/etc/device_version.json')),
         'processes':command('ps | grep -E "[m]qtt_bridge|[w]eb_server|[t]k8710_gw|[t]daemon.sh|[f]rpc"')}
def record(name,data):
 (a.evidence/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
 print(json.dumps(data,ensure_ascii=False,indent=2),flush=True)
def session():
 cfg=json.loads(read('/etc/config.json'));session=requests.Session();session.trust_env=False
 r=session.post('http://'+a.host+':8090/login',data={'username':'admin','password':cfg['web_password']},allow_redirects=False,timeout=10)
 assert r.status_code==302 and session.cookies.get('session'), 'Web login failed'
 return session
if a.action=='snapshot':record('before.json',snapshot())
elif a.action=='prepare':
 # Install the recovery-capable base once; this is not part of an ordinary SWU.
 record('before-base.json',snapshot())
 backup=command('mktemp -d /userdata/swu-base-backup-XXXXXX').strip()
 overlay=a.sdk/'buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay'
 files={overlay/'usr/bin/app_upgrade.sh':'/usr/bin/app_upgrade.sh',
        overlay/'etc/init.d/S80swupdate':'/etc/init.d/S80swupdate',
        overlay/'etc/init.d/S98app_upgrade_recover':'/etc/init.d/S98app_upgrade_recover',
        overlay/'usr/bin/tdaemon.sh':'/usr/bin/tdaemon.sh',
        overlay/'usr/bin/tdaemon_proc.sh':'/usr/bin/tdaemon_proc.sh',
        a.sdk/'device/rockchip/common/extra-parts/userdata/normal/web_server':'/userdata/web_server'}
 for local,remote in files.items():
  assert local.is_file(),local
  command(f'if [ -f {shlex.quote(remote)} ]; then cp -p {shlex.quote(remote)} {backup}/{pathlib.PurePosixPath(remote).name}; fi')
  s.put(str(local),remote+'.swu-base-new');s.chmod(remote+'.swu-base-new',0o755)
 # Stop only the daemon interpreter, never S99 stop (which kills all apps).
 raw=command('cat /var/run/tdaemon_job.pid 2>/dev/null || true').strip()
 if raw:
  assert raw.isdigit()
  assert '/usr/bin/tdaemon.sh' in read('/proc/'+raw+'/cmdline').decode()
  command('kill -TERM '+raw)
  for _ in range(25):
   if command(f'test -d /proc/{raw} && echo alive || true').strip()=='':break
   time.sleep(0.2)
  else:raise RuntimeError('daemon did not stop')
 for pid in command('pidof web_server || true').split():
  assert command('readlink /proc/'+pid+'/exe').strip()=='/userdata/web_server'
  command('kill -TERM '+pid)
 time.sleep(1)
 for local,remote in files.items():command(f'mv {shlex.quote(remote+".swu-base-new")} {shlex.quote(remote)}')
 # No installation is active during base provisioning. Stop the vendor listener
 # before starting Web; progress must stop first because it can request reboot.
 for name in ['swupdate-progress','swupdate']:
  for pid in command('pidof '+name+' || true').split():
   try:exe=command('readlink /proc/'+pid+'/exe').strip()
   except RuntimeError:continue
   assert exe=='/usr/bin/'+name,exe
   command('kill -TERM '+pid,check=False)
  time.sleep(1)
 assert not command('pidof swupdate swupdate-progress || true').strip(),'legacy updater still running'
 command('sh -n /usr/bin/app_upgrade.sh && /usr/bin/app_upgrade.sh recover')
 command('cd /userdata; nohup ./web_server > /userdata/swu-web-validation.log 2>&1 < /dev/null &',check=False)
 command('/etc/init.d/S99tdaemon_job start')
 record('base-installed.json',{'backup':backup,'files':list(files.values())})
elif a.action in ['install','reject']:
 web=session();base='http://'+a.host+':8090'
 blob=a.package.read_bytes() if a.action=='install' else b'NOT-A-CPIO-SWU\n'
 before=snapshot();record(a.action+'-before.json',before)
 for off in range(0,len(blob),512*1024):
  r=web.post(base+'/upload?file=image.swu&offset='+str(off),data=blob[off:off+512*1024],timeout=20)
  assert r.status_code==200,(r.status_code,r.text)
 r=web.post(base+'/upload/start',timeout=10);assert r.status_code==200,(r.status_code,r.text)
 if a.action=='install':
  blocked=web.post(base+'/upload?file=image.swu&offset=0',data=b'concurrent upload',timeout=5)
  assert blocked.status_code==409,'upload not frozen during installation'
 statuses=[]
 for _ in range(90):
  r=web.get(base+'/api/upgrade_status',timeout=5);r.raise_for_status();j=r.json()
  if not statuses or statuses[-1]!=j:statuses.append(j);print(j,flush=True)
  if j.get('status') in ['done','error']:break
  time.sleep(0.5)
 else:raise RuntimeError('upgrade status timeout')
 record(a.action+'-status.json',statuses)
 expected='done' if a.action=='install' else 'error'
 assert statuses[-1]['status']==expected,statuses
 try:s.get('/tmp/swupdate.log',str(a.evidence/(a.action+'-swupdate.log')))
 except OSError:pass
 if a.action=='reject':
  after=snapshot()
  for key in ['boot_id','preserved_sha256','binaries','version']:assert before[key]==after[key],key
  record('reject-verified.json',after)
elif a.action=='verify':
 now=snapshot();manifest=json.loads(a.package.with_suffix('.manifest.json').read_text())
 before=json.loads((a.evidence/'install-before.json').read_text(encoding='utf8'))
 assert now['boot_id']!=before['boot_id'],'board did not reboot'
 assert now['preserved_sha256']==before['preserved_sha256'],'configuration changed'
 assert all(now['binaries'][n]==h for n,h in manifest['sha256'].items() if n in now['binaries'])
 assert now['version']['app_bundle_version']==manifest['version']
 assert read('/userdata/.app-upgrade/transaction/phase').strip()==b'committed'
 now['legacy_updater_pids']=command('pidof swupdate swupdate-progress || true').strip()
 assert not now['legacy_updater_pids'],'legacy updater returned after reboot'
 now['maintenance_clear']=command('test ! -e /var/run/app-upgrade-maintenance && test ! -d /var/run/app-upgrade-lock && echo yes').strip()=='yes'
 assert now['maintenance_clear']
 for name in ['mqtt_bridge','web_server','tk8710_gw']:
  assert command('pidof '+name).strip(),name+' not running'
 web=session();r=web.get('http://'+a.host+':8090/status',timeout=10);assert r.status_code==200
 s.get('/userdata/.app-upgrade/last-swupdate.log',str(a.evidence/'last-swupdate.log'))
 record('verified.json',now)
s.close();c.close()
