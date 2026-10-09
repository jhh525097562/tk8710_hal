"""Explicit board deployment of tk8710_gw; backup, exact-PID stop, hardware reset."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import time
import paramiko

HOST=os.environ.get('RK3506_HOST','192.168.100.202')
ROOT=Path(__file__).resolve().parents[2]
binary=ROOT/'build_rk3506/tk8710_gw'
stamp=time.strftime('%Y%m%d_%H%M%S')
out=ROOT/'remote_logs'/('gateway_status_'+HOST+'_'+stamp);out.mkdir(parents=True)
remote='/userdata/gateway_status_verify_'+stamp
c=paramiko.SSHClient();c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST,username='root',password=os.environ['RK3506_SSH_PASSWORD'],timeout=10)
s=c.open_sftp();record={'host':HOST,'backup':remote,'states':[]}
maintenance=False;stopped=False;installed=False
def run(command,check=True):
    _,o,e=c.exec_command(command,timeout=40)
    data={'stdout':o.read().decode(errors='replace'),'stderr':e.read().decode(errors='replace'),'rc':o.channel.recv_exit_status()}
    if check and data['rc']:raise RuntimeError(str(data))
    return data
def read(path):
    with s.open(path) as f:return f.read()
def stop(pid):
    assert read(f'/proc/{pid}/cmdline').split(b'\0')[0] in (b'./tk8710_gw',b'/userdata/tk8710_gw')
    run(f'kill -TERM {pid}')
    for _ in range(30):
        if run(f'test -e /proc/{pid}/exe',False)['rc']:return
        time.sleep(1)
    raise RuntimeError('Gateway did not exit gracefully; no SIGKILL issued')
def start():
    # The board BusyBox launcher closes SSH streams before executing the program.
    return run('cd /userdata && start-stop-daemon -S -q -b -x /userdata/tk8710_gw -- '+
               ' '.join(shlex.quote(x) for x in args))
def sample():
    try:return json.loads(read('/var/run/tk8710_gw/status.json'))
    except (OSError,ValueError):return None
try:
    record['before']=run('cat /proc/sys/kernel/random/boot_id; pidof mqtt_bridge web_server rk_wdt; cat /run/rk-watchdog/status.json; sha256sum /etc/config.json /etc/tdaemon.conf')['stdout']
    pid_text=run('pidof tk8710_gw')['stdout'].strip();assert pid_text.isdecimal()
    pid=int(pid_text)
    args=[x.decode() for x in read(f'/proc/{pid}/cmdline').split(b'\0')[1:] if x]
    record['args']=args
    s.mkdir(remote)
    run('cp -p /userdata/tk8710_gw '+shlex.quote(remote+'/tk8710_gw.old'))
    s.put(str(binary),remote+'/tk8710_gw.new');s.chmod(remote+'/tk8710_gw.new',0o755)
    record['sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    assert hashlib.sha256(read(remote+'/tk8710_gw.new')).hexdigest()==record['sha256']
    run('test -x /usr/bin/tk8710_reset')
    # Exclusive creation so we never remove another upgrade's maintenance marker.
    with s.open('/var/run/app-upgrade-maintenance','wx') as f:f.write('gateway status validation\n')
    maintenance=True
    time.sleep(3)
    print('Stopping gateway PID',pid,'and resetting TK8710 hardware.',flush=True)
    stop(pid);stopped=True
    record['reset']=run('/usr/bin/tk8710_reset')
    run('cp '+shlex.quote(remote+'/tk8710_gw.new')+' /userdata/tk8710_gw.status-new && chmod 755 /userdata/tk8710_gw.status-new && mv -f /userdata/tk8710_gw.status-new /userdata/tk8710_gw')
    installed=True;start()
    deadline=time.monotonic()+450;last=None
    while time.monotonic()<deadline:
        state=sample()
        if state and state['pid']!=pid:
            if (state['state'],state['reason'])!=last:
                record['states'].append(state)
                print(json.dumps(state),flush=True);last=(state['state'],state['reason'])
            if state['state']=='fault':raise RuntimeError('Gateway reported '+state['reason'])
            if state['business_ready']:break
        time.sleep(.1)
    else:raise RuntimeError('No business-ready state before deadline')
    # Release maintenance immediately after readiness; verify all publishers together.
    s.remove('/var/run/app-upgrade-maintenance');maintenance=False
    record['samples']=[]
    for _ in range(8):
        time.sleep(5)
        frame={'gateway':sample()}
        for key,path in [('mqtt','/run/led/mqtt_status.json'),('network','/run/led/network_status.json')]:
            frame[key]=json.loads(read(path))
        record['samples'].append(frame)
        print(json.dumps({k:v.get('state') if v else None for k,v in frame.items()}),flush=True)
    record['after']=run('cat /proc/sys/kernel/random/boot_id; pidof mqtt_bridge web_server rk_wdt tk8710_gw; cat /run/rk-watchdog/status.json; sha256sum /etc/config.json /etc/tdaemon.conf; cat /var/run/tk8710_gps_policy; cat /userdata/RFStatus/RFstatus.txt')['stdout']
    record['result']='deployed_and_observed'
except BaseException as error:
    record['error']=str(error)
    if installed:
        now=run('pidof tk8710_gw',False)['stdout'].strip()
        if now.isdecimal():stop(int(now))
        run('/usr/bin/tk8710_reset')
        run('cp -p '+shlex.quote(remote+'/tk8710_gw.old')+' /userdata/tk8710_gw.status-old && mv -f /userdata/tk8710_gw.status-old /userdata/tk8710_gw')
        start();record['rollback']='restored old gateway'
    elif stopped:
        run('/usr/bin/tk8710_reset');start()
    raise
finally:
    if maintenance:s.remove('/var/run/app-upgrade-maintenance')
    (out/'evidence.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print('EVIDENCE',out,flush=True)
    s.close();c.close()
