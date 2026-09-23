"""Board acceptance for traffic statistics and all-enabled network defaults.

Requires explicit --host and BOARD_PASSWORD. It resets the three displayed
traffic baselines and saves ETH,WiFi,4G through the existing Web form.
"""
import argparse, hashlib, json, os, pathlib, re, sys, time
import paramiko
import requests

sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser();p.add_argument('--host',required=True);p.add_argument('--output',required=True,type=pathlib.Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(a.host,username='root',password=os.environ['BOARD_PASSWORD'],timeout=8);s=c.open_sftp()
def read(path):
 with s.open(path,'rb') as f:return f.read()
def command(cmd):
 _,o,e=c.exec_command(cmd,timeout=30);out=o.read().decode('utf8',errors='replace');err=e.read().decode('utf8',errors='replace')
 rc=o.channel.recv_exit_status()
 if rc:raise RuntimeError(f'{cmd}: {rc}: {err}')
 return out
cfg=json.loads(read('/etc/config.json'))
web=requests.Session();web.trust_env=False
r=web.post('http://'+a.host+':8090/login',data={'username':'admin','password':cfg['web_password']},allow_redirects=False,timeout=8)
assert r.status_code==302 and web.cookies.get('session')
base='http://'+a.host+':8090'
def stats():
 r=web.get(base+'/api/traffic_stats',timeout=6);r.raise_for_status();data=r.json()
 assert data['ok'] is True and data['source']=='/proc/net/dev'
 assert set(data['interfaces'])=={'eth0','usb0','wlan0'}
 return data
def reset(iface):
 r=web.post(base+'/api/traffic_stats/reset',params={'iface':iface},timeout=6)
 assert r.status_code==200 and r.json()['status']=='ok',(r.status_code,r.text)
def kernel_counters():
 result={}
 for line in read('/proc/net/dev').decode().splitlines()[2:]:
  name,values=line.split(':',1);parts=values.split();result[name.strip()]={'rx':int(parts[0]),'tx':int(parts[8])}
 return result

before_cfg=read('/etc/config.json');before_tdaemon=read('/etc/tdaemon.conf')
initial=stats();after_kernel=kernel_counters()
for iface,item in initial['interfaces'].items():
 assert item['present'] is True
 assert item['raw_rx_bytes']<=after_kernel[iface]['rx'] and item['raw_tx_bytes']<=after_kernel[iface]['tx']
 assert item['total_bytes']==item['rx_bytes']+item['tx_bytes']

# Corrupt and impossible baselines must not hide current counters.
with s.open('/etc/turbotraffic','wb') as f:f.write(b'{invalid json')
corrupt=stats();assert corrupt['interfaces']['eth0']['total_bytes']>0
huge={'interfaces':{'eth0':{'base_rx_bytes':2**62,'base_tx_bytes':2**62}}}
with s.open('/etc/turbotraffic','w') as f:json.dump(huge,f)
wrapped=stats();assert wrapped['interfaces']['eth0']['total_bytes']>0

for iface in ['eth0','usb0','wlan0']:reset(iface)
cleared=stats()
for item in cleared['interfaces'].values():
 assert item['total_bytes']<256*1024,item
baseline=json.loads(read('/etc/turbotraffic'))
assert set(baseline['interfaces'])=={'eth0','usb0','wlan0'}

# Save all three checkboxes using every current field so config_save is lossless.
payload={k:str(cfg[k]) for k in ['mqtt_host','mqtt_port','username','password','gateway_id','pub_topic','sub_topic','heartbeat_interval','qos','antenna_count']}
payload.update(net_eth='ETH',net_wifi='WiFi',net_4g='4G')
r=web.post(base+'/config',data=payload,allow_redirects=False,timeout=8)
assert r.status_code==302 and r.headers.get('Location')=='/config?saved=1',(r.status_code,r.text)
new_cfg=json.loads(read('/etc/config.json'));assert new_cfg['net_interface']=='ETH,WiFi,4G'
for key,value in cfg.items():
 if key!='net_interface':assert new_cfg.get(key)==value,(key,new_cfg.get(key),value)
for _ in range(30):
 text=read('/etc/tdaemon.conf').decode(errors='replace')
 if re.search(r'^disabled=\s*$',text,re.M):break
 time.sleep(.5)
else:raise RuntimeError('tdaemon disabled list did not become empty')
page=web.get(base+'/config',timeout=8);page.raise_for_status()
for name in ['net_eth','net_wifi','net_4g']:
 assert re.search(r'name="'+name+r'"[^>]*\bchecked\b',page.text),name
for _ in range(40):
 pids={name:command('pidof '+name+' || true').strip() for name in ['mqtt_bridge','tk8710_gw','web_server']}
 if all(pids.values()):break
 time.sleep(.5)
else:raise RuntimeError('business processes did not recover after config save')

result={'host':a.host,'initial':initial,'corrupt_baseline_fallback':True,'counter_reset_fallback':True,
        'cleared':cleared,'baseline_file':baseline,'net_interface':new_cfg['net_interface'],
        'tdaemon_disabled_empty':True,'all_checkboxes_checked':True,'processes':pids,
        'config_keys_preserved':sorted(new_cfg),'config_sha_before':hashlib.sha256(before_cfg).hexdigest(),
        'tdaemon_sha_before':hashlib.sha256(before_tdaemon).hexdigest()}
(a.output/'traffic-api-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False,indent=2));s.close();c.close()
