"""Explicit two-app deployment/verification with backups and automatic rollback."""
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

parser = argparse.ArgumentParser()
parser.add_argument('--host', required=True)
parser.add_argument('--gateway-bin', type=Path, required=True)
parser.add_argument('--mqtt-bin', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
binaries = {'tk8710_gw': args.gateway_bin, 'mqtt_bridge': args.mqtt_bin}
for binary in binaries.values():
    elf = binary.read_bytes()
    assert elf[:4] == b'\x7fELF' and elf[18:20] == b'\x28\x00', binary

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(args.host, username='root', password=os.environ['RK3506_SSH_PASSWORD'], timeout=10)
s = c.open_sftp()
remote = '/userdata/gps_telemetry_' + time.strftime('%Y%m%d_%H%M%S')
record = {'host': args.host, 'remote_backup': remote, 'commands': {}, 'states': [], 'gps_samples': []}
maintenance = False
installed = []
stopped = []
saved_args = {}
messages = []
message_lock = threading.Lock()
subscribed = threading.Event()
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                     client_id='codex-gps-verify-' + str(time.time_ns()))


def read(path):
    with s.open(path, 'rb') as fp:
        return fp.read()


def run(command, check=True):
    _, stdout, stderr = c.exec_command(command, timeout=40)
    output = stdout.read().decode('utf-8', 'replace')
    error = stderr.read().decode('utf-8', 'replace')
    code = stdout.channel.recv_exit_status()
    if check and code:
        raise RuntimeError(f'{command}: rc={code} {error} {output}')
    return output.strip()


def live(name):
    result = []
    for pid in run('pidof ' + name + ' || true').split():
        try:
            stat = read('/proc/' + pid + '/stat').decode()
            if stat[stat.rfind(')') + 2] != 'Z':
                result.append(pid)
        except OSError:
            pass
    return result


def stop(name):
    pids = live(name)
    assert len(pids) <= 1, (name, pids)
    for pid in pids:
        assert s.readlink('/proc/' + pid + '/exe') == '/userdata/' + name
        run('kill -TERM ' + pid)
    for _ in range(30):
        if not live(name):
            return
        time.sleep(1)
    raise RuntimeError(name + ' did not exit gracefully')


def start(name, suffix):
    command = shlex.join(['/userdata/' + name] + saved_args[name])
    record['commands'][name] = command
    # BusyBox daemonization closes the SSH channel before exec; a background
    # shell can retain the channel even when the child's stdio is redirected.
    run('cd /userdata && start-stop-daemon -S -q -b -x /userdata/' + name +
        ' -- ' + shlex.join(saved_args[name]))


def state():
    try:
        return json.loads(read('/var/run/tk8710_gw/status.json'))
    except (OSError, ValueError):
        return None


def wait_ready(previous_pid):
    deadline = time.monotonic() + 450
    previous = None
    while time.monotonic() < deadline:
        value = state()
        if value and str(value['pid']) != previous_pid:
            phase = (value['state'], value['reason'])
            if phase != previous:
                record['states'].append(value)
                print('STATE', json.dumps(value), flush=True)
                previous = phase
            if value['state'] == 'fault':
                raise RuntimeError('gateway fault: ' + value['reason'])
            if value['business_ready'] and value['gps_enable'] == 1:
                return value
        time.sleep(.5)
    raise RuntimeError('gateway did not reach GPS-enabled business readiness')


def on_connect(instance, userdata, flags, reason, properties):
    if reason == 0:
        instance.subscribe(config['pub_topic'], qos=1)


def on_subscribe(instance, userdata, mid, reason_codes, properties):
    if all(not code.is_failure for code in reason_codes):
        subscribed.set()


def on_message(instance, userdata, message):
    try:
        payload = json.loads(message.payload)
        if 'alive' in payload:
            with message_lock:
                messages.append({'received_time': time.time(), 'payload': payload,
                                 'retained': message.retain})
    except ValueError:
        pass


try:
    config_bytes = read('/etc/config.json')
    config = json.loads(config_bytes)
    config_hashes = {path: hashlib.sha256(read(path)).hexdigest()
                     for path in ['/etc/config.json', '/etc/tdaemon.conf']}
    record['config_hashes_before'] = config_hashes
    record['boot_id_before'] = read('/proc/sys/kernel/random/boot_id').decode().strip()
    record['other_apps_before'] = {name: live(name) for name in ['web_server', 'frpc', 'tdaemon.sh']}
    run('test -x /usr/bin/tk8710_reset && test ! -e /var/run/app-upgrade-maintenance')
    s.mkdir(remote, 0o700)
    print('BACKUP', remote, flush=True)
    for name, binary in binaries.items():
        pids = live(name)
        assert len(pids) == 1, (name, pids)
        saved_args[name] = [x.decode() for x in read('/proc/' + pids[0] + '/cmdline').split(b'\0')[1:] if x]
        record.setdefault('original_args', {})[name] = saved_args[name]
        run('cp -p /userdata/' + name + ' ' + remote + '/' + name + '.old')
        s.put(str(binary), remote + '/' + name + '.new')
        s.chmod(remote + '/' + name + '.new', 0o755)
        digest = hashlib.sha256(binary.read_bytes()).hexdigest()
        assert hashlib.sha256(read(remote + '/' + name + '.new')).hexdigest() == digest
        record.setdefault('new_sha256', {})[name] = digest
    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    if config.get('username'):
        client.username_pw_set(config['username'], config.get('password', ''))
    client.connect(config['mqtt_host'], config['mqtt_port'], keepalive=60)
    client.loop_start()
    assert subscribed.wait(15), 'independent MQTT subscriber not ready'
    with s.open('/var/run/app-upgrade-maintenance', 'wx') as fp:
        fp.write('GPS telemetry verification\n')
    maintenance = True
    print('MAINTENANCE enabled; stopping the two apps', flush=True)
    time.sleep(3)
    old_pid = live('tk8710_gw')[0]
    for name in ['tk8710_gw', 'mqtt_bridge']:
        stop(name)
        stopped.append(name)
    run('/usr/bin/tk8710_reset')
    for name in binaries:
        s.posix_rename(remote + '/' + name + '.new', '/userdata/' + name)
        installed.append(name)
    print('INSTALLED verified binaries; starting MQTT then gateway', flush=True)
    start('mqtt_bridge', 'validation')
    start('tk8710_gw', 'first')
    first = wait_ready(old_pid)
    # Verify a real graceful shutdown writes a durable reason, then restart once.
    stop('tk8710_gw')
    previous_exit = json.loads(read('/var/run/tk8710_gw/last_exit.json'))
    assert previous_exit['exited'] and previous_exit['reason'] == 'signal_received', previous_exit
    record['graceful_exit'] = previous_exit
    run('/usr/bin/tk8710_reset')
    start('tk8710_gw', 'second')
    ready = wait_ready(str(first['pid']))
    assert json.loads(read('/var/run/tk8710_gw/last_exit.json')) == previous_exit
    s.remove('/var/run/app-upgrade-maintenance')
    maintenance = False
    observed_after = time.time()
    deadline = time.monotonic() + 150
    last_gps = None
    while time.monotonic() < deadline:
        current = state()
        assert current and current['pid'] == ready['pid'] and not current['exited'], current
        assert current['state'] != 'fault', current
        try:
            text = read('/userdata/GPS/GPSinfo.txt').decode()
            gps = dict(line.split('=', 1) for line in text.splitlines())
            assert set(gps) == {'lon', 'lat', 'status', 'count', 'avgsnr'}, gps
            if gps != last_gps:
                sample = {'received_time': time.time(), 'mtime': s.stat('/userdata/GPS/GPSinfo.txt').st_mtime,
                          'values': gps}
                record['gps_samples'].append(sample)
                print('GPS', json.dumps(sample), flush=True)
                last_gps = gps
        except FileNotFoundError:
            pass
        time.sleep(1)
    with message_lock:
        heartbeats = [item for item in messages if item['received_time'] >= observed_after and not item['retained']]
    matched = [item for item in heartbeats if isinstance(item['payload']['alive'].get('count'), int)
               and isinstance(item['payload']['alive'].get('avgsnr'), int)]
    assert len(matched) >= 2, 'fewer than two independent broker heartbeat samples'
    assert len(record['gps_samples']) >= 8, 'GPS file did not update repeatedly'
    intervals = [b['mtime'] - a['mtime'] for a, b in zip(record['gps_samples'], record['gps_samples'][1:])]
    assert all(9 <= delta <= 12 for delta in intervals), intervals
    assert all(int(item['values']['count']) > 0 and int(item['values']['avgsnr']) > 0
               for item in record['gps_samples'])
    gps_pairs = {(int(item['values']['count']), int(item['values']['avgsnr']))
                 for item in record['gps_samples']}
    assert any((item['payload']['alive']['count'], item['payload']['alive']['avgsnr']) in gps_pairs
               for item in matched)
    record['gps_update_intervals_s'] = intervals
    record['broker_heartbeats'] = matched
    record['final_status'] = state()
    record['boot_id_after'] = read('/proc/sys/kernel/random/boot_id').decode().strip()
    assert record['boot_id_after'] == record['boot_id_before']
    record['other_apps_after'] = {name: live(name) for name in record['other_apps_before']}
    assert record['other_apps_after'] == record['other_apps_before']
    for name in binaries:
        pids = live(name)
        assert len(pids) == 1
        assert run('sha256sum /proc/' + pids[0] + '/exe').split()[0] == record['new_sha256'][name]
    record['result'] = 'PASS'
    print('PASS board GPS/status/log/independent MQTT heartbeat verification', flush=True)
except BaseException as error:
    record['error'] = str(error)
    record['result'] = 'FAIL'
    if installed or stopped:
        for name in ['tk8710_gw', 'mqtt_bridge']:
            stop(name)
        run('/usr/bin/tk8710_reset')
        for name in installed:
            run('cp -p ' + remote + '/' + name + '.old ' + remote + '/' + name + '.rollback')
            s.posix_rename(remote + '/' + name + '.rollback', '/userdata/' + name)
        for name in ['mqtt_bridge', 'tk8710_gw']:
            start(name, 'rollback')
        record['rollback'] = 'original binaries restored and restarted'
    raise
finally:
    if maintenance:
        s.remove('/var/run/app-upgrade-maintenance')
    client.disconnect()
    client.loop_stop()
    record['config_hashes_after'] = {path: hashlib.sha256(read(path)).hexdigest() for path in config_hashes}
    for path, expected in config_hashes.items():
        assert record['config_hashes_after'][path] == expected
    for source, folder in [('/userdata/8710log', '8710log'), (remote, 'stdout')]:
        destination = args.output / folder
        destination.mkdir(exist_ok=True)
        for item in s.listdir_attr(source):
            if item.filename.endswith('.log'):
                s.get(source + '/' + item.filename, str(destination / item.filename))
    for source in ['/var/log/mqtt_bridge.log', '/root/daemon_guard.log', '/var/log/messages']:
        s.get(source, str(args.output / Path(source).name))
    (args.output / 'evidence.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    print('EVIDENCE', args.output, flush=True)
    s.close()
    c.close()
