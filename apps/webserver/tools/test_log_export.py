"""Explicit board deployment and end-to-end log export verification.

BOARD_PASSWORD is required; defaults never select a board. Tests create uniquely
named .dat fixtures, remove only those fixtures, and leave real logs untouched.
"""
import argparse, hashlib, io, json, os, pathlib, shlex, sys, tarfile, time, uuid
import paramiko
import requests

sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser()
p.add_argument('action',choices=['deploy','test','ui'])
p.add_argument('--host',required=True)
p.add_argument('--binary',type=pathlib.Path)
p.add_argument('--output',required=True,type=pathlib.Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(a.host,username='root',password=os.environ['BOARD_PASSWORD'],timeout=8)
s=c.open_sftp();base='http://'+a.host+':8090'
def cmd(command):
 _,o,e=c.exec_command(command,timeout=30)
 output=o.read().decode('utf8',errors='replace');err=e.read().decode('utf8',errors='replace')
 rc=o.channel.recv_exit_status()
 if rc:raise RuntimeError(f'{command}: {rc}: {err}')
 return output
def read(path):
 with s.open(path,'rb') as f:return f.read()
config=json.loads(read('/etc/config.json'))
def login():
 r=requests.Session();r.trust_env=False
 response=r.post(base+'/login',data={'username':'admin','password':config['web_password']},allow_redirects=False,timeout=8)
 assert response.status_code==302 and r.cookies.get('session')
 return r
def save(name,value):
 (a.output/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf8')
 print(json.dumps(value,ensure_ascii=False,indent=2),flush=True)
if a.action=='deploy':
 assert a.binary and a.binary.is_file()
 binary=a.binary.read_bytes();assert binary[:6]==b'\x7fELF\x01\x01' and binary[18:20]==b'\x28\x00'
 web=login();assert web.get(base+'/api/upgrade_status',timeout=5).json()['status'] not in ['installing','done']
 assert cmd('test ! -e /var/run/app-upgrade-maintenance && echo clear').strip()=='clear'
 before=cmd('pidof mqtt_bridge tk8710_gw').split()
 backup=cmd('mktemp -d /userdata/web-log-export-backup-XXXXXX').strip()
 cmd('cp -p /userdata/web_server '+backup+'/web_server')
 s.put(str(a.binary),backup+'/new_web_server');s.chmod(backup+'/new_web_server',0o755)
 assert hashlib.sha256(read(backup+'/new_web_server')).digest()==hashlib.sha256(binary).digest()
 cmd('touch /var/run/app-upgrade-maintenance')
 try:
  time.sleep(2)
  pids=cmd('pidof web_server').split()
  for pid in pids:
   assert cmd('readlink /proc/'+pid+'/exe').strip().removesuffix(' (deleted)')=='/userdata/web_server'
  cmd('mv '+backup+'/new_web_server /userdata/web_server')
  for pid in pids:cmd('kill -TERM '+pid)
  time.sleep(1)
  cmd('cd /userdata; nohup ./web_server >/userdata/web-log-export-launch.log 2>&1 </dev/null &')
  for _ in range(20):
   try:
    web=login();r=web.get(base+'/api/logs?types=web',timeout=3)
    assert r.status_code==200 and r.json()['files'][0]['type']=='web'
    break
   except Exception:time.sleep(.5)
  else:raise RuntimeError('new Web did not become healthy')
 except Exception:
  cmd('cp -p '+backup+'/web_server /userdata/.web_server_restore && mv /userdata/.web_server_restore /userdata/web_server')
  cmd('killall web_server 2>/dev/null || true')
  cmd('cd /userdata; nohup ./web_server >/userdata/web-log-export-launch.log 2>&1 </dev/null &')
  raise
 finally:cmd('rm -f /var/run/app-upgrade-maintenance')
 assert set(before)==set(cmd('pidof mqtt_bridge tk8710_gw').split()),'business process changed during deployment'
 save('deployment.json',{'host':a.host,'backup':backup,'web_sha256':hashlib.sha256(binary).hexdigest(),'business_pids_unchanged':True})
elif a.action=='test':
 web=login();checks=[];latencies=[]
 def listing(types):
  r=web.get(base+'/api/logs',params={'types':types},timeout=5);r.raise_for_status();return r.json()
 def start(types,ids):return web.post(base+'/api/log_export/start',json={'types':types,'files':ids},timeout=5)
 def finish(job):
  for _ in range(180):
   t=time.monotonic();r=web.get(base+'/api/log_export/status',params={'id':job},timeout=5);latencies.append(time.monotonic()-t)
   r.raise_for_status();state=r.json()
   if state['status']=='ready':return state
   assert state['status']=='working',state
   time.sleep(.2)
  raise RuntimeError('export timeout')
 def download(job,label):
  r=web.get(base+'/api/log_export/download',params={'id':job},timeout=30);r.raise_for_status()
  (a.output/(label+'.tar.gz')).write_bytes(r.content)
  with tarfile.open(fileobj=io.BytesIO(r.content),mode='r:gz') as tar:
   names=[m.name.removeprefix('./') for m in tar.getmembers() if m.isfile()]
   assert all(not n.startswith('/') and '..' not in n.split('/') for n in names)
   manifest=json.load(tar.extractfile('./manifest.json'))
   return names,manifest
 unauth=requests.Session();unauth.trust_env=False
 assert unauth.get(base+'/api/logs',allow_redirects=False,timeout=5).status_code==302
 assert web.get(base+'/api/logs?types=unknown',timeout=5).status_code==400
 assert start('web',['../../etc/config.json']).status_code==400
 assert start('web',[]).status_code==400
 assert web.get(base+'/api/log_export/download?id=unknown',timeout=5).status_code==404
 checks.append('authentication, invalid type/path/empty selection/unknown job rejected')
 before_config=hashlib.sha256(read('/etc/config.json')).hexdigest()
 single=listing('web');assert single['files'] and all(f['type']=='web' for f in single['files'])
 ids=[f['id'] for f in single['files']];r=start('web',ids);assert r.status_code==202
 first=r.json()['id'];finish(first);names,manifest=download(first,'web-only')
 assert all(n=='manifest.json' or n.startswith('web/') for n in names)
 checks.append('Web-only archive contains only selected Web logs and manifest')
 token=uuid.uuid4().hex;fixture='/userdata/logs/log_export_test_'+token+'.dat'
 huge='/userdata/logs/log_export_large_'+token+'.dat';link='/userdata/logs/log_export_link_'+token+'.dat'
 originals={f['id'] for f in listing('business')['files']}
 try:
  payload=os.urandom(8*1024*1024)
  with s.open(fixture,'wb') as f:f.write(payload)
  with s.open(huge,'wb') as f:f.truncate(129*1024*1024)
  # This board's SFTP server reverses the SYMLINK argument convention.
  cmd('ln -s /etc/config.json '+shlex.quote(link))
  entries=listing('business')['files'];listed={f['id'] for f in entries}
  assert 'business/'+pathlib.PurePosixPath(link).name not in listed
  assert start('business',['business/'+pathlib.PurePosixPath(huge).name]).status_code==413
  s.remove(huge);s.remove(link)
  selected=['business/'+pathlib.PurePosixPath(fixture).name]+ids
  r=start('web,business',selected);assert r.status_code==202
  job=r.json()['id'];assert start('web',ids).status_code==409
  begin=time.monotonic();assert web.get(base+'/status',timeout=5).status_code==200;page_latency=time.monotonic()-begin
  assert page_latency<2.5,page_latency
  finish(job);names,manifest=download(job,'mixed-fixture')
  assert set(names)==set(selected+['manifest.json'])
  with tarfile.open(a.output/'mixed-fixture.tar.gz') as tar:
   assert hashlib.sha256(tar.extractfile('./'+selected[0]).read()).digest()==hashlib.sha256(payload).digest()
  assert hashlib.sha256(read(fixture)).digest()==hashlib.sha256(payload).digest()
  assert web.get(base+'/api/log_export/download',params={'id':first},timeout=5).status_code==404
  checks.append('mixed export, fixture content preserved, 128MiB cap, symlink exclusion, concurrent 409, old job expiry')
 finally:
  for path in [fixture,huge,link]:
   try:s.remove(path)
   except FileNotFoundError:pass
 alltypes='web,mqtt,tk8710,daemon,system,upgrade,business';allfiles=listing(alltypes)
 assert len(allfiles['categories'])==7
 ids=[f['id'] for f in allfiles['files']];r=start(alltypes,ids);assert r.status_code==202,r.text
 job=r.json()['id'];state=finish(job);names,manifest=download(job,'all-logs')
 successful={f['id'] for f in manifest['files'] if f['status'] in ['ok','short_snapshot']}
 assert set(names)==successful|{'manifest.json'}
 assert originals.issubset({f['id'] for f in listing('business')['files']})
 assert hashlib.sha256(read('/etc/config.json')).hexdigest()==before_config
 checks.append('all seven categories, manifest matches archive, original business files and configuration retained')
 save('api-results.json',{'host':a.host,'checks':checks,'categories':allfiles['categories'],'all_export':state,'status_page_seconds_during_export':page_latency,'max_poll_seconds':max(latencies),'final_job':job,'fixtures_removed':True})
elif a.action=='ui':
 from playwright.sync_api import sync_playwright
 with sync_playwright() as pw:
  browser=pw.chromium.launch(channel='chrome',headless=True)
  context=browser.new_context(viewport={'width':1360,'height':1000},accept_downloads=True)
  response=context.request.post(base+'/login',form={'username':'admin','password':config['web_password']},max_redirects=0)
  assert response.status==302
  page=context.new_page();errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
  page.goto(base+'/upgrade');page.locator('.log-file-choice').first.wait_for()
  assert page.locator('#log-types input').count()==7
  page.locator('#log-all-types').click()
  page.wait_for_function("document.querySelectorAll('.log-file-choice').length > 3 && !document.getElementById('log-export-btn').disabled")
  page.locator('#log-export-btn').click()
  page.locator('#log-package-download').wait_for(state='visible',timeout=60000)
  with page.expect_download() as pending:page.locator('#log-package-download').click()
  downloaded=pending.value;downloaded.save_as(a.output/'ui-all-logs.tar.gz')
  page.locator('#log-types').scroll_into_view_if_needed();page.screenshot(path=str(a.output/'log-export-page.png'),full_page=True)
  assert not errors,errors
  save('ui-results.json',{'host':a.host,'seven_categories':True,'all_export_downloaded':True,'filename':downloaded.suggested_filename,'javascript_errors':errors})
  browser.close()
s.close();c.close()
