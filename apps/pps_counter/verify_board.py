import datetime
import json
import pathlib
import time
import paramiko

root = pathlib.Path(__file__).resolve().parent
stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
out = root/'logs'/stamp
out.mkdir(parents=True)
remote = '/userdata/pps_counter_'+stamp
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.100.239', username='root', password=__import__("os").environ["RK3506_SSH_PASSWORD"], timeout=10)
def cmd(s):
    _, o, e = c.exec_command(s, timeout=15)
    return dict(stdout=o.read().decode(errors='replace'), stderr=e.read().decode(errors='replace'), rc=o.channel.recv_exit_status())
health = 'cat /proc/sys/kernel/random/boot_id; pidof rk_wdt tk8710_gw mqtt_bridge web_server; cat /run/rk-watchdog/status.json; gpioinfo gpiochip1 | grep "line  18:"; grep -i pps /proc/interrupts'
try:
    sf=c.open_sftp(); sf.mkdir(remote)
    sf.put(str(root/'build_arm/pps_counter'), remote+'/pps_counter'); sf.chmod(remote+'/pps_counter',0o755)
    evidence={'before':cmd(health), 'remote':remote}
    command=remote+'/pps_counter 60 > '+remote+'/counter.log 2>&1'
    _, o, e=c.exec_command(command, timeout=80)
    time.sleep(2)
    evidence['during']=cmd(health)
    print(json.dumps(evidence['during'],ensure_ascii=False),flush=True)
    while not o.channel.exit_status_ready(): time.sleep(1)
    evidence['exit_code']=o.channel.recv_exit_status()
    sf.get(remote+'/counter.log',str(out/'counter.log'))
    evidence['after']=cmd(health)
    (out/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8')
    print((out/'counter.log').read_text())
    print(json.dumps(evidence['after'],ensure_ascii=False))
    print('EVIDENCE',out)
    sf.close()
finally:
    c.close()
