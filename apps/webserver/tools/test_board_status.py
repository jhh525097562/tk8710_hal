import json, pathlib, time, subprocess, sys, shlex, urllib.request, urllib.parse, http.cookiejar
import paramiko

base=pathlib.Path(__file__).resolve().parents[1]
out=base/'docs'/('status-board-'+time.strftime('%Y%m%d-%H%M%S'));out.mkdir(parents=True)
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy());c.connect('192.168.200.106',username='root',password=__import__("os").environ["RK3506_SSH_PASSWORD"],timeout=10)
s=c.open_sftp();remote='/userdata/'+out.name
def cmd(x):
    _,o,e=c.exec_command(x,timeout=15);text=o.read().decode('utf8','replace');err=e.read().decode('utf8','replace');rc=o.channel.recv_exit_status()
    if rc:raise RuntimeError(err+text)
    return text.strip()
def api():
    return json.loads(op.open('http://192.168.200.106:8090/api/system_info',timeout=8).read())
def login():
    cfg=json.loads(s.open('/etc/config.json').read())
    op.open(urllib.request.Request('http://192.168.200.106:8090/login',data=urllib.parse.urlencode({'username':'admin','password':cfg.get('web_password','admin')}).encode()),timeout=8).read()
op=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
guards=[];pid=None;old=None;child=None;report={};samples=[]
try:
    old=cmd('pidof web_server').split(); assert len(old)==1
    assert cmd('readlink /proc/'+old[0]+'/exe')=='/userdata/web_server'
    guards=cmd('pidof tdaemon.sh || true').split()
    if guards:cmd('kill -STOP '+' '.join(guards))
    s.mkdir(remote);s.put(str(base/'build_arm/web_server'),remote+'/web_server');s.chmod(remote+'/web_server',0o755)
    cmd('kill -TERM '+old[0]);
    for _ in range(20):
        if not cmd('ss -lnt | grep ":8090 " || true'):break
        time.sleep(1)
    else:raise RuntimeError('Original web listener did not stop')
    pid=cmd('nohup '+remote+'/web_server >'+remote+'/web.log 2>&1 </dev/null & echo $!');time.sleep(2);login()
    initial=api();report['initial']=initial
    assert initial['tk8710_gw']['running'] is False
    assert initial['gps_enabled'] is None
    report['missing_state_pass']=True
    child=subprocess.Popen([sys.executable,str(base.parent/'mqtt/tests/board_scan_e2e.py'),'--gps-policy','1','--gateway','D:/CodexWorkSpace/tk8710_hal_v1.6/build_rk3506/tk8710_gw','--mqtt',str(base.parent/'mqtt/build_arm/mqtt_bridge'),'--output',str(out/'gateway-test')],stdout=open(out/'runner.log','w'),stderr=subprocess.STDOUT)
    while child.poll() is None:
        try:
            data=api();samples.append(data)
            if data.get('gps_enabled') is True and not report.get('off_display_fixture_pass'):
                policy_path='/var/run/tk8710_gps_policy'
                saved=s.open(policy_path).read()
                fields=saved.decode().split()
                try:
                    with s.open(policy_path,'w') as f:f.write(fields[0]+' 0 '+fields[2]+'\n')
                    assert api()['gps_enabled'] is False
                    report['off_display_fixture_pass']=True
                finally:
                    with s.open(policy_path,'wb') as f:f.write(saved)
            for path,key in [('/userdata/GPS/GPSinfo.txt','gps_file'),('/userdata/RFStatus/RFstatus.txt','rf_file'),('/var/run/tk8710_gps_policy','policy_file')]:
                try:data[key]=s.open(path).read().decode()
                except OSError:pass
        except Exception as e:report.setdefault('poll_errors',[]).append(str(e))
        time.sleep(1)
    assert child.returncode==0
    assert report.get('off_display_fixture_pass')
    assert any(x['gps_enabled'] is True and x['tk8710_gw']['running'] for x in samples)
    assert any(x['tk8710_gw']['uptime_s'] and x['tk8710_gw']['uptime_s']>0 for x in samples)
    assert any(x.get('rf_file','').strip()=='rfstatus='+str(x.get('rfstatus')) for x in samples)
    located=False
    for x in samples:
        fields=dict(line.split('=',1) for line in x.get('gps_file','').splitlines() if '=' in line)
        if fields.get('lon') and fields.get('lat') and x['device_coords']==fields['lon']+', '+fields['lat']:located=True
    assert located
    time.sleep(2);final=api();report['after_gateway_exit']=final
    assert final['gps_enabled'] is None and final['tk8710_gw']['running'] is False
    report['passed']=True
finally:
    if child and child.poll() is None:child.wait(timeout=180)
    if pid:cmd('kill -TERM '+pid+' 2>/dev/null || true');time.sleep(1)
    if old and not cmd('pidof web_server || true'):cmd('nohup /userdata/web_server >/tmp/web-restored.log 2>&1 </dev/null &')
    if guards:cmd('kill -CONT '+' '.join(guards)+' 2>/dev/null || true')
    try:s.get(remote+'/web.log',str(out/'web.log'))
    except OSError:pass
    report['restored_web']=cmd('for p in $(pidof web_server); do readlink /proc/$p/exe; done')
    (out/'samples.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2),encoding='utf8')
    (out/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    s.close();c.close();print(out);print('PASS' if report.get('passed') else 'FAIL')
