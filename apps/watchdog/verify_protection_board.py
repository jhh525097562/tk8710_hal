"""Read-only board checks plus signal-intercepted S99 function checks."""
import json
import pathlib
import time
import paramiko

out = pathlib.Path(__file__).parent/'protection-board-192.168.100.239'
out.mkdir(exist_ok=True)
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.100.239', username='root', password=__import__("os").environ["RK3506_SSH_PASSWORD"], timeout=10)
def run(command):
    _, stdout, stderr = c.exec_command(command, timeout=20)
    return dict(stdout=stdout.read().decode(errors='replace'),
                stderr=stderr.read().decode(errors='replace'), rc=stdout.channel.recv_exit_status())
try:
    sf = c.open_sftp()
    source = sf.open('/etc/init.d/S99tdaemon_job').read().decode(errors='replace')
    sf.close()
    functions = source.split('case "$1" in\n  start)', 1)[0]
    assert len(functions) < len(source) and 'is_watchdog_pid()' in functions
    check = functions + '''
kill() { echo "INTERCEPTED kill $*"; }
killall() { echo "INTERCEPTED killall $*"; }
wp=$(pidof rk_wdt)
is_watchdog_pid "$wp" || exit 21
if is_daemon_pid "$wp"; then exit 22; fi
dp=$(cat /var/run/tdaemon_job.pid)
is_daemon_pid "$dp" || exit 23
kill_proc_entry_force /usr/sbin/rk_wdt
kill_proc_entry_force /etc/init.d/S15rk-watchdog
echo PASS_watchdog_exclusion_and_daemon_identity
'''
    stdin, stdout, stderr = c.exec_command('/bin/sh -s', timeout=20)
    stdin.write(check); stdin.flush(); stdin.channel.shutdown_write()
    result = dict(stdout=stdout.read().decode(), stderr=stderr.read().decode(), rc=stdout.channel.recv_exit_status())
    assert result['rc'] == 0 and 'INTERCEPTED' not in result['stdout'], result
    data = dict(protection_checks=result, samples=[])
    for i in range(7):
        sample=run('cat /proc/sys/kernel/random/boot_id; cat /run/rk-watchdog/status.json; p=$(pidof rk_wdt); cat /proc/$p/oom_score_adj; cat /proc/$p/stat; cat /sys/class/watchdog/watchdog0/timeleft; pidof mqtt_bridge web_server tk8710_gw frpc; cat /var/run/tdaemon_job.pid')
        data['samples'].append(sample)
        print(sample['stdout'], flush=True)
        if i < 6: time.sleep(5)
    data['health']=run('sha256sum /usr/sbin/rk_wdt /etc/init.d/S99tdaemon_job; cat /sys/class/watchdog/watchdog0/nowayout; cat /sys/class/watchdog/watchdog0/timeout; tail -20 /var/log/rk_watchdog.log; tail -20 /root/daemon_guard.log; netstat -lnt; date; ps | grep -E "mqtt_bridge|web_server|tk8710_gw|frpc|tdaemon"')
    (out/'evidence.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(data['protection_checks'],ensure_ascii=False))
    print(data['health']['stdout'])
finally:
    c.close()
