"""Test only a temporary copy of init script; no hardware or service is started."""
import pathlib, subprocess, tempfile
here=pathlib.Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='wdt-init-test-') as tmp:
    root=pathlib.Path(tmp);script=root/'S15rk-watchdog'
    # Redirect every writable location; mock process launch below.
    source=(here/'S15rk-watchdog').read_text().replace('/run/rk-watchdog',str(root/'state')).replace('/var/log',str(root/'logs'))
    source=source.replace('DAEMON=/usr/sbin/rk_wdt','DAEMON='+str(root/'rk_wdt'))
    script.write_text(source)
    assert subprocess.run(['sh',str(script),'start']).returncode==0
    assert 'executable_missing' in (root/'state/status.json').read_text()
    (root/'rk_wdt').write_text('#!/bin/sh\nexit 0\n');(root/'rk_wdt').chmod(0o755)
    source=source.replace('start-stop-daemon','mock_launcher')
    source=source.replace('case "${1:-}" in','mock_launcher() { return 1; }\npidof() { return 1; }\ncase "${1:-}" in',1)
    script.write_text(source)
    assert subprocess.run(['sh',str(script),'start']).returncode==0
    assert 'launch_failed' in (root/'state/status.json').read_text()
    before=(root/'state/status.json').read_bytes()
    assert subprocess.run(['sh',str(script),'stop']).returncode==0
    assert (root/'state/status.json').read_bytes()==before
    assert subprocess.run(['sh',str(script),'restart']).returncode==1
    print('PASS init: missing/failed launch do not block rcS; stop leaves feeder; live restart refused')
