"""Explicitly deploy/test GPS defaults on a board already configured GPS-off."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import threading
import time

import paramiko
import paho.mqtt.client as mqtt

p = argparse.ArgumentParser()
p.add_argument('--host', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
stamp = time.strftime('%Y%m%d_%H%M%S')
out = root/'remote_logs'/('gps_reset_'+a.host+'_'+stamp)
out.mkdir(parents=True)
remote = '/userdata/gps_reset_'+stamp
record = {'host': a.host, 'backup': remote, 'started': time.strftime('%F %T')}
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(a.host, username='root', password=os.environ['RK3506_SSH_PASSWORD'], timeout=10)
s = c.open_sftp()


def read(path):
    with s.open(path, 'rb') as fp:
        return fp.read()


def run(command):
    _, stdout, stderr = c.exec_command(command, timeout=20)
    text, error = stdout.read().decode(), stderr.read().decode()
    if stdout.channel.recv_exit_status():
        raise RuntimeError(command+': '+error+text)
    return text.strip()


def pids(name):
    return run('pidof '+name+' || true').split()


def stop(name):
    for pid in pids(name):
        assert s.readlink('/proc/'+pid+'/exe') == '/userdata/'+name
        run('kill -TERM '+pid)
    for _ in range(30):
        if not pids(name):
            return
        time.sleep(1)
    raise RuntimeError('graceful stop timeout '+name)


def start(name):
    run('cd /userdata && start-stop-daemon -S -q -b -x /userdata/'+name+
        ' -- '+shlex.join(argv[name]))


def wait(predicate, seconds, label):
    deadline = time.monotonic()+seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(.5)
    raise RuntimeError('timeout '+label)


def state():
    return json.loads(read('/run/tk8710_gw/status.json'))


def ready():
    value = state()
    if value['state'] == 'fault':
        raise RuntimeError(value['reason'])
    return value if value['gps_enable'] == 0 and value['business_ready'] else None


expected = b'lon=unknown\nlat=unknown\nstatus=0\ncount=null\navgsnr=null\n'
config_paths = ['/etc/config.json', '/etc/tdaemon.conf']
config = json.loads(read(config_paths[0]))
record['config_before'] = {f: hashlib.sha256(read(f)).hexdigest() for f in config_paths}
record['old_state'] = state()
assert ready(), 'Run only on a board already configured GPS-off'
record['old_gps_file'] = read('/userdata/GPS/GPSinfo.txt').decode()
argv = {n: [x.decode() for x in read('/proc/'+pids(n)[0]+'/cmdline').split(b'\0')[1:] if x]
        for n in ['tk8710_gw', 'mqtt_bridge']}
record['commands'] = {n: shlex.join(['/userdata/'+n]+v) for n, v in argv.items()}
boot = read('/proc/sys/kernel/random/boot_id')
subscribed = threading.Event()
received = []


def connected(instance, userdata, flags, reason, properties):
    if reason == 0:
        instance.subscribe(config['pub_topic'], qos=1)


def message(instance, userdata, msg):
    try:
        value = json.loads(msg.payload)
        if isinstance(value, dict) and 'alive' in value and not msg.retain:
            received.append(value['alive'])
    except ValueError:
        pass


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='gps-reset-'+stamp)
client.username_pw_set(config.get('username', ''), config.get('password', ''))
client.on_connect = connected
client.on_subscribe = lambda *unused: subscribed.set()
client.on_message = message
marker = installed = stopped = False
try:
    run('test ! -e /var/run/app-upgrade-maintenance && test -x /usr/bin/tk8710_reset')
    s.mkdir(remote, 0o700)
    run('cp -p /userdata/tk8710_gw '+remote+'/tk8710_gw.old')
    binary = root/'build_rk3506/tk8710_gw'
    s.put(str(binary), remote+'/tk8710_gw.new')
    s.chmod(remote+'/tk8710_gw.new', 0o755)
    record['sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    assert hashlib.sha256(read(remote+'/tk8710_gw.new')).hexdigest() == record['sha256']
    client.connect(config['mqtt_host'], config['mqtt_port'], 60)
    client.loop_start()
    assert subscribed.wait(15)
    with s.open('/var/run/app-upgrade-maintenance', 'wx') as fp:
        fp.write('GPS reset validation\n')
    marker = True
    time.sleep(3)
    stopped = True
    stop('tk8710_gw')
    stop('mqtt_bridge')
    run('/usr/bin/tk8710_reset')
    s.posix_rename(remote+'/tk8710_gw.new', '/userdata/tk8710_gw')
    installed = True
    start('tk8710_gw')
    wait(lambda: state()['state'] == 'wait_ns_config', 10, 'awaiting NS config')
    assert read('/userdata/GPS/GPSinfo.txt') == expected
    record['startup_file'] = expected.decode()
    print('PASS startup defaults before NS config', flush=True)
    received.clear()
    start('mqtt_bridge')
    record['ready_state'] = wait(ready, 120, 'GPS-off business ready')
    assert read('/userdata/GPS/GPSinfo.txt') == expected
    record['disabled_file'] = expected.decode()

    def correct_heartbeat():
        return next((x for x in received if x.get('lon') == 'unknown' and x.get('lat') == 'unknown'
                     and x.get('status') == 0 and 'count' in x and x['count'] is None
                     and 'avgsnr' in x and x['avgsnr'] is None), None)

    record['broker_heartbeat'] = wait(correct_heartbeat, 85, 'GPS-off broker heartbeat')
    # Include a periodic heartbeat, beyond the initial connect heartbeat.
    initial = len(received)
    wait(lambda: len(received) > initial, 85, 'periodic broker heartbeat')
    assert correct_heartbeat() and read('/userdata/GPS/GPSinfo.txt') == expected
    record['heartbeat_count'] = len(received)
    record['final_state'] = ready()
    assert record['final_state']['pid'] == record['ready_state']['pid']
    assert read('/proc/sys/kernel/random/boot_id') == boot
    assert run('sha256sum /proc/'+pids('tk8710_gw')[0]+'/exe').split()[0] == record['sha256']
    record['result'] = 'PASS'
    print('PASS GPS-off file and broker heartbeat', json.dumps(record['broker_heartbeat']), flush=True)
except BaseException as error:
    record['result'] = 'FAIL'
    record['error'] = repr(error)
    if stopped:
        stop('tk8710_gw')
        run('/usr/bin/tk8710_reset')
        if installed:
            run('cp -p '+remote+'/tk8710_gw.old '+remote+'/tk8710_gw.rollback')
            s.posix_rename(remote+'/tk8710_gw.rollback', '/userdata/tk8710_gw')
        if not pids('mqtt_bridge'):
            start('mqtt_bridge')
        start('tk8710_gw')
        record['rollback'] = 'old gateway restored; services restarted'
    raise
finally:
    if marker:
        s.remove('/var/run/app-upgrade-maintenance')
    client.disconnect()
    client.loop_stop()
    record['config_after'] = {f: hashlib.sha256(read(f)).hexdigest() for f in config_paths}
    record['finished'] = time.strftime('%F %T')
    for source in ['/var/log/mqtt_bridge.log', '/root/daemon_guard.log', '/var/log/messages']:
        s.get(source, str(out/Path(source).name))
    folder = out/'8710log'
    folder.mkdir()
    for name in s.listdir('/userdata/8710log'):
        if name.endswith('.log'):
            s.get('/userdata/8710log/'+name, str(folder/name))
    (out/'evidence.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf8')
    print('EVIDENCE', out, flush=True)
    s.close()
    c.close()
assert record['config_after'] == record['config_before']
