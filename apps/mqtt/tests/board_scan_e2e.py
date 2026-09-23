"""Isolated NS simulator on a board; restores its original MQTT configuration."""
import argparse
import hashlib
import json
import pathlib
import shlex
import time
import sys
import paramiko
import urllib.request
import urllib.parse
import http.cookiejar

p = argparse.ArgumentParser()
p.add_argument('--host', default='192.168.200.106')
p.add_argument('--gateway', required=True)
p.add_argument('--mqtt', required=True)
p.add_argument('--output', required=True)
p.add_argument('--extended', action='store_true')
p.add_argument('--gps-policy', type=int, choices=[0, 1])
p.add_argument('--gps-no-module', action='store_true')
p.add_argument('--gps-toggle', action='store_true')
args = p.parse_args()
out = pathlib.Path(args.output); out.mkdir(parents=True, exist_ok=False)
c = paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(args.host, username='root', password=__import__("os").environ["RK3506_SSH_PASSWORD"], timeout=10)
s = c.open_sftp()
run_id = 'ns-scan-' + time.strftime('%Y%m%d-%H%M%S')
remote = '/userdata/' + run_id
topic = 'codex-test/' + run_id
port = 18884 if args.extended else 1883
pids = []; stopped = []; original = None; old_mqtt = []; summary = {'run_id': run_id}

def command(text, timeout=15):
    if text == 'pidof mqtt_bridge || true':
        text = 'for p in $(pidof mqtt_bridge); do test -e /proc/$p/exe && echo $p; done; true'
    _, stdout, stderr = c.exec_command(text, timeout=timeout)
    data = stdout.read().decode('utf-8', 'replace')
    err = stderr.read().decode('utf-8', 'replace')
    rc = stdout.channel.recv_exit_status()
    if rc: raise RuntimeError(f'{text}: exit {rc}: {err} {data}')
    return data.strip()

def write(path, data):
    with s.open(path, 'wb') as f: f.write(data)

def launch(executable, extra, name):
    cmd = 'nohup ' + shlex.quote(executable) + ' ' + extra
    pid = int(command(cmd + ' >' + shlex.quote(remote+'/'+name+'.log') + ' 2>&1 </dev/null & echo $!'))
    pids.append(pid); return pid

def publish(data):
    command(f'mosquitto_pub -h 127.0.0.1 -p {port} -q 1 -t ' + shlex.quote(topic+'/down') + ' -m ' + shlex.quote(json.dumps(data)))

def messages():
    result = []
    for line in s.open(remote+'/mqtt-messages.log').read().decode('utf8','replace').splitlines():
        try: msg = json.loads(line)
        except ValueError: continue
        if isinstance(msg, dict): result.append(msg)
    return result

def results():
    return [m['scan_result'] for m in messages() if m.get('scan_result',{}).get('request_id') == run_id]

def wait_for(predicate, seconds=30):
    for _ in range(seconds):
        if predicate(): return
        time.sleep(1)
    raise RuntimeError('Timed out waiting for '+predicate.__name__)

def stop_test(pid):
    command('kill -TERM '+str(pid)+' 2>/dev/null || true')
    wait_for(lambda: command('test -d /proc/'+str(pid)+' && echo live || true') != 'live',15)
    pids.remove(pid)

try:
    existing = command('pidof tk8710_gw || true')
    if existing: raise RuntimeError('Gateway already running; inspect before replacing: '+existing)
    s.mkdir(remote, 0o700)
    if args.extended:
        if command("ss -lnt | grep ':18884 ' || true"): raise RuntimeError('Test broker port busy')
        write(remote+'/broker.conf', b'listener 18884 127.0.0.1\nallow_anonymous true\npersistence false\n')
        broker = launch('/usr/sbin/mosquitto', '-c '+shlex.quote(remote+'/broker.conf'), 'broker')
        time.sleep(1)
    original = s.open('/etc/config.json', 'rb').read()
    write(remote+'/config.original.json', original); s.chmod(remote+'/config.original.json',0o600)
    old_mqtt = command('pidof mqtt_bridge || true').split()
    for pid in old_mqtt:
        exe = command('readlink /proc/'+pid+'/exe')
        if exe != '/userdata/mqtt_bridge': raise RuntimeError('Unexpected mqtt executable '+exe)
    stopped = command('pidof tdaemon.sh || true').split()
    if stopped: command('kill -STOP ' + ' '.join(stopped))
    if old_mqtt:
        command('kill -TERM ' + ' '.join(old_mqtt))
        for _ in range(15):
            if not command('pidof mqtt_bridge || true'): break
            time.sleep(1)
    remaining = command('pidof mqtt_bridge || true').split()
    if remaining:
        for pid in remaining:
            if command('readlink /proc/'+pid+'/exe') != '/userdata/mqtt_bridge':
                raise RuntimeError('Unexpected replacement MQTT process')
        command('kill -KILL '+' '.join(remaining))
        summary['original_mqtt_forced_stop'] = remaining
        time.sleep(1)
    if command('pidof mqtt_bridge || true'): raise RuntimeError('Original MQTT did not stop')
    cfg = json.loads(original)
    cfg.update(mqtt_host='127.0.0.1',mqtt_port=port,username='',password='',
               pub_topic=topic+'/up',sub_topic=topic+'/down',heartbeat_interval=5)
    write('/etc/config.json', json.dumps(cfg).encode())
    for local, name in [(args.mqtt, 'mqtt_bridge'),(args.gateway, 'tk8710_gw')]:
        s.put(local, remote+'/'+name); s.chmod(remote+'/'+name,0o755)
        summary[name+'_sha256'] = hashlib.sha256(pathlib.Path(local).read_bytes()).hexdigest()
    launch('/usr/bin/mosquitto_sub', f'-h 127.0.0.1 -p {port} -t '+shlex.quote(topic+'/up'), 'mqtt-messages')
    mqtt_pid = launch(remote+'/mqtt_bridge', 'raw', 'mqtt')
    for _ in range(15):
        time.sleep(1)
        received = s.open(remote+'/mqtt-messages.log').read().decode('utf8','replace')
        if '"alive"' in received: break
    else: raise RuntimeError('MQTT heartbeat not received; abort before RF startup')
    gateway_args = '-w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level warn'
    if args.gps_no_module: gateway_args += ' --gps-test-scenario no-module'
    gw_pid = launch(remote+'/tk8710_gw', gateway_args, 'gateway')
    config = {'set':{'config':{'gps_enable':args.gps_policy if args.gps_policy is not None else 1,'freq':470000000,'nwk_num':4,'tdd_num':1,'slot_cfg':0,'rate_num':1,
                            'rate_cfgs':[{'rate':3,'uplink_pkt':1,'downlink_pkt':1}]}}}
    for i in range(45):
        publish(config)
        if args.gps_policy == 1 and args.gps_no_module:
            try: log = s.open(remote+'/gateway.log').read().decode('utf8','replace')
            except OSError: log = ''
            if 'refusing business startup' in log: break
        if command('test -S /tmp/data_collect.sock && echo ready || true') == 'ready':
            if args.gps_policy is None: break
            current_log = s.open(remote+'/gateway.log').read().decode('utf8','replace')
            marker = 'TK8710 synchronization source: '+('external GPS/PPS' if args.gps_policy else 'local')
            if marker in current_log: break
        time.sleep(2)
    else: raise RuntimeError('Gateway scan socket did not become ready')
    if args.gps_policy is not None:
        time.sleep(3)
        log = s.open(remote+'/gateway.log').read().decode('utf8','replace')
        if args.gps_policy == 1 and args.gps_no_module:
            assert 'refusing business startup' in log
            assert 'HAL started successfully' not in log
        else:
            expected = 'external GPS/PPS' if args.gps_policy else 'local'
            assert 'TK8710 synchronization source: '+expected in log
            if args.gps_policy == 0:
                assert 'NS gps_enable=0: GPS disabled' in log
                assert 'Configuring GPS/PPS period' not in log[log.rindex('NS gps_enable=0: GPS disabled'):]
                assert command('for f in /proc/'+str(gw_pid)+'/fd/*; do readlink "$f"; done | grep /dev/ttyS4 || true') == ''
        summary['gps_policy'] = args.gps_policy
        if args.gps_toggle:
            for enabled in [1, 0]:
                baseline = len(s.open(remote+'/gateway.log').read())
                config['set']['config']['gps_enable'] = enabled
                publish(config)
                expected = 'TK8710 synchronization source: '+('external GPS/PPS' if enabled else 'local')
                def switched():
                    text = s.open(remote+'/gateway.log').read()[baseline:].decode('utf8','replace')
                    return expected in text and 'HAL started successfully' in text
                wait_for(switched,90)
                print('PASS runtime GPS switch',enabled,flush=True)
            summary['gps_runtime_toggle'] = True
        summary['gps_no_module_simulated'] = args.gps_no_module
        summary['passed'] = True
        print('PASS GPS policy',args.gps_policy,'no-module',args.gps_no_module,flush=True)
        raise SystemExit(0)
    print('Gateway socket ready; sending real NS sweep', flush=True)
    request = {'request_id':run_id,'set':{'scan':{'start_freq_hz':470000000,'end_freq_hz':510000000,'mode':'500k'}}}
    publish(request)
    if args.extended:
        time.sleep(2)
        publish(request)
        conflicting = json.loads(json.dumps(request)); conflicting['set']['scan']['end_freq_hz'] = 509000000
        publish(conflicting)
        another = json.loads(json.dumps(request)); another['request_id'] = run_id+'-busy'
        publish(another)
    result = None
    for i in range(180):
        time.sleep(1)
        raw = s.open(remote+'/mqtt-messages.log').read().decode('utf8','replace')
        for line in raw.splitlines():
            try: msg = json.loads(line)
            except ValueError: continue
            if not isinstance(msg, dict): continue
            if msg.get('scan_result',{}).get('request_id') == run_id: result = msg['scan_result']
        if result: break
        if i % 15 == 0: print('Waiting for sweep result, seconds:',i,flush=True)
    if result is None: raise RuntimeError('No MQTT sweep result within 180 seconds')
    summary['result'] = result
    if result['state'] != 'completed': raise RuntimeError('Scan failed: '+json.dumps(result))
    assert result['point_count'] == 81
    assert len(result['points']) == 81
    assert all(len(x['noise_dbm']) == 8 for x in result['points'])
    snapshot_path = '/userdata/mqtt_scan/exec-'+run_id+'.txt'
    snapshot = s.open(snapshot_path).read()
    values = snapshot.decode().split()
    assert len(values) == 81*9
    for i, point in enumerate(result['points']):
        assert point['freq_hz'] == int(values[i*9])
        assert point['noise_dbm'] == [float(x) for x in values[i*9+1:i*9+9]]
    summary['snapshot_exact_match'] = True
    if args.extended:
        acks = [m['scan_ack'] for m in messages() if 'scan_ack' in m]
        assert any(a.get('error_code') == 'request_id_conflict' for a in acks)
        assert any(a.get('state') == 'busy' for a in acks)
        print('Checking broker disconnect/reconnect and process restart',flush=True)
        stop_test(broker)
        time.sleep(12)
        prior = len(results())
        broker = launch('/usr/sbin/mosquitto', '-c '+shlex.quote(remote+'/broker.conf'), 'broker-restart')
        wait_for(lambda: len(results()) > prior, 35)
        summary['broker_reconnect_resend'] = True
        stop_test(mqtt_pid)
        prior = len(results())
        mqtt_pid = launch(remote+'/mqtt_bridge', 'raw', 'mqtt-restart')
        wait_for(lambda: len(results()) > prior, 25)
        assert all(x == result for x in results())
        summary['process_restart_resend'] = True
    publish({'scan_result_ack':{'request_id':run_id}})
    time.sleep(2)
    journal = json.loads(s.open('/userdata/mqtt_scan/job-'+run_id+'.json').read())
    assert journal['phase'] == 'acknowledged'
    if args.extended:
        prior = len(results()); time.sleep(12); assert len(results()) == prior
        print('Checking legacy Web sweep and immutable NS snapshot',flush=True)
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        base_url = f'http://{args.host}:8090'
        credentials = urllib.parse.urlencode({'username':'admin','password':json.loads(original).get('web_password','admin')}).encode()
        opener.open(urllib.request.Request(base_url+'/login',data=credentials),timeout=10).read()
        data = urllib.parse.urlencode({'freq_start':'470M','freq_stop':'470M','mode':'500k'}).encode()
        response = json.loads(opener.open(urllib.request.Request(base_url+'/api/rf_scan/start',data=data),timeout=10).read())
        assert response['status'] == 'ok'
        def web_finished():
            state = json.loads(s.open('/tmp/data_collect_status.json').read())
            return state.get('state') == 'done' and state.get('request_id') == ''
        wait_for(web_finished,60)
        assert s.open(snapshot_path).read() == snapshot
        summary['web_compatible_snapshot_preserved'] = True
    summary['passed'] = True
    print('PASS real NS sweep 81 x 8',flush=True)
except Exception as e:
    summary['passed'] = False; summary['error'] = str(e); print('FAIL:',e,flush=True)
finally:
    for pid in reversed(pids):
        try: command('kill -TERM '+str(pid)+' 2>/dev/null || true')
        except Exception as e: summary.setdefault('cleanup_errors',[]).append(str(e))
    time.sleep(3)
    for pid in pids:
        try:
            if command('test -d /proc/'+str(pid)+' && echo live || true') == 'live':
                command('kill -KILL '+str(pid)+' 2>/dev/null || true')
                summary.setdefault('forced_stop',[]).append(pid)
        except Exception as e: summary.setdefault('cleanup_errors',[]).append(str(e))
    if original is not None:
        try: write('/etc/config.json', original)
        except Exception as e: summary.setdefault('cleanup_errors',[]).append(str(e))
    if old_mqtt:
        try:
            if not command('pidof mqtt_bridge || true'):
                command('nohup /userdata/mqtt_bridge >/tmp/mqtt-restored.log 2>&1 </dev/null &')
        except Exception as e: summary.setdefault('cleanup_errors',[]).append(str(e))
    if stopped:
        try: command('kill -CONT '+' '.join(stopped)+' 2>/dev/null || true')
        except Exception as e: summary.setdefault('cleanup_errors',[]).append(str(e))
    for name in ['mqtt.log','gateway.log','mqtt-messages.log','mqtt-restart.log','broker.log','broker-restart.log']:
        try: s.get(remote+'/'+name, str(out/name))
        except Exception: pass
    summary['config_restored'] = original is not None and s.open('/etc/config.json','rb').read() == original
    (out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
    s.close();c.close()
    print('Evidence:',out,flush=True)
sys.exit(0 if summary.get('passed') and summary.get('config_restored') and not summary.get('cleanup_errors') else 1)
