"""Linux isolated S99 matching tests: fake /proc, no real process signals."""
import pathlib
import subprocess
import tempfile

here = pathlib.Path(__file__).resolve().parent
source = (here.parent/'tdaemon/S99tdaemon_job').read_text()
functions = source.split('case "$1" in\n  start)', 1)[0]
with tempfile.TemporaryDirectory(prefix='wdt-stop-test-') as tmp:
    root = pathlib.Path(tmp)
    proc = root/'proc'; proc.mkdir()
    for pid, name, argv in [(901, 'rk_wdt', ['/usr/sbin/rk_wdt']),
                            (902, 'mqtt_bridge', ['/userdata/mqtt_bridge']),
                            (903, 'sh', ['/bin/sh', '/usr/bin/tdaemon.sh'])]:
        p = proc/str(pid); p.mkdir()
        (p/'comm').write_text(name+'\n')
        (p/'cmdline').write_bytes(('\0'.join(argv)+'\0').encode())
        (p/'exe').symlink_to('/usr/sbin/rk_wdt (deleted)' if pid == 901 else argv[0])
    script = root/'test.sh'
    script.write_text(functions.replace('/proc/', str(proc)+'/') + '''
kill() { echo "kill $*" >> "$TEST_SIGNALS"; }
killall() { echo "killall $*" >> "$TEST_SIGNALS"; }
is_watchdog_pid 901 || exit 11
if is_daemon_pid 901; then exit 12; fi
if is_daemon_pid invalid; then exit 13; fi
is_daemon_pid 903 || exit 14
kill_proc_entry_force /usr/sbin/rk_wdt
kill_proc_entry_force /etc/init.d/S15rk-watchdog
# A broad configured prefix must still leave the watchdog alone.
kill_proc_entry_force /usr
kill_proc_entry_force /userdata/mqtt_bridge
''')
    import os
    signals = root/'signals'
    subprocess.run(['sh', str(script)], env=dict(os.environ, TEST_SIGNALS=str(signals)), check=True)
    result = signals.read_text()
    assert '901' not in result and 'killall -q -9 rk_wdt' not in result, result
    assert 'kill -KILL 902' in result, result
    print('PASS watchdog exclusions, deleted executable, stale PID identity, normal MQTT stop')
