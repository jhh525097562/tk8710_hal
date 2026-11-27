"""WSL tests: mocked GPIO and stop ordering; never sends signals to real apps."""
import pathlib, subprocess, tempfile
here = pathlib.Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tk8710-stop-test-') as tmp:
    root = pathlib.Path(tmp)
    (root/'gpiod.h').write_text('''
struct gpiod_chip; struct gpiod_line;
struct gpiod_chip *gpiod_chip_open_by_name(const char *);
struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *, unsigned);
int gpiod_line_request_output(struct gpiod_line *, const char *, int);
int gpiod_line_set_value(struct gpiod_line *, int);
void gpiod_line_release(struct gpiod_line *);
void gpiod_chip_close(struct gpiod_chip *);
''')
    (root/'mock.c').write_text('''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gpiod.h"
static int step;
struct gpiod_chip *gpiod_chip_open_by_name(const char *s) {
 assert(!strcmp(s,"gpiochip0")); return (void*)1;
}
struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *c,unsigned n) {
 (void)c;assert(n==13);return (void*)2;
}
int gpiod_line_request_output(struct gpiod_line *l,const char *c,int v) {
 (void)l;(void)c;assert(v==0);step=1;return getenv("FAIL_REQUEST")?-1:0;
}
int nanosleep(const struct timespec *r,struct timespec *remaining) {
 (void)remaining;assert(step==1);assert(r->tv_sec==0 && r->tv_nsec==10000000);step=2;return 0;
}
int gpiod_line_set_value(struct gpiod_line *l,int v) {
 (void)l;assert(step==2 && v==1);step=3;return getenv("FAIL_HIGH")?-1:0;
}
void gpiod_line_release(struct gpiod_line *l) {(void)l;assert(step==3);}
void gpiod_chip_close(struct gpiod_chip *c) {(void)c;}
''')
    binary=root/'reset'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(root),str(here/'tk8710_reset.c'),str(root/'mock.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    import os
    for var in ['FAIL_REQUEST','FAIL_HIGH']:
        env=os.environ.copy();env[var]='1'
        assert subprocess.run([str(binary)],env=env).returncode!=0
    source=(here/'S99tdaemon_job').read_text(encoding="utf-8")
    function=source.split('reset_stopped_tk8710() {',1)[1].split('\n}\n',1)[0]
    script='reset_stopped_tk8710() {'+function+'\n}\n'
    # Mock pidof/sleep/tool: ensure a live process blocks reset, and failure propagates.
    script+='''
pidof() { return "$BUSY"; }
sleep() { :; }
TK8710_RESET_TOOL=/bin/true
BUSY=0
if reset_stopped_tk8710; then exit 10; fi
BUSY=1
reset_stopped_tk8710 || exit 11
TK8710_RESET_TOOL=/bin/false
if reset_stopped_tk8710; then exit 12; fi
TK8710_RESET_TOOL=/nonexistent/reset-tool
if reset_stopped_tk8710; then exit 13; fi
'''
    subprocess.run(['sh'],input=script,text=True,check=True)
    assert source.index('kill_configured_processes_force\n',source.index('  stop)')) < source.index('reset_stopped_tk8710 ||')
    assert '"$0" stop || exit $?' in source
    print('PASS: GPIO low/10ms/high, error propagation, process-exit guard, stop-before-reset, restart failure gate')

    # Exercise the complete stop branch against synthetic processes. Every signal
    # command is a shell stub; /proc and service paths point into this temp tree.
    trace=root/'trace'
    proc=root/'proc'
    for pid,command in [(4242,'/userdata/frp/start_frpc.sh'),
                        (4243,'/userdata/mqtt_bridge')]:
        entry=proc/str(pid);entry.mkdir(parents=True)
        (entry/'cmdline').write_bytes(command.encode()+b'\0')
        (entry/'comm').write_text(command.rsplit('/',1)[-1])
    config=root/'tdaemon.conf'
    config.write_text('proc=/userdata/frp/start_frpc.sh\nproc=/userdata/mqtt_bridge\n')
    ipc=root/'ipc_spi_to_mqtt';ipc.write_text('pending uplink')
    tool=root/'reset-tool'
    tool.write_text('#!/bin/sh\necho reset >> "$TRACE"\nexit "${RESET_FAIL:-0}"\n')
    tool.chmod(0o755)
    sandbox=source.replace('/proc/',str(proc)+'/')
    sandbox=sandbox.replace('/var/run/tdaemon_job.pid',str(root/'pid'))
    sandbox=sandbox.replace('/var/run/tdaemon_job.lock',str(root/'lock'))
    sandbox=sandbox.replace('/etc/tdaemon.conf',str(config))
    sandbox=sandbox.replace('/usr/bin/tk8710_reset',str(tool))
    stubs='''
kill() { echo "kill $*" >> "$TRACE"; }
killall() { echo "killall $*" >> "$TRACE"; }
pidof() { return "${PROCESS_GONE:-1}"; }
sleep() { :; }
'''
    sandbox=sandbox.replace('\ncase "$1" in\n','\n'+stubs+'\ncase "$1" in\n',1)
    stop=root/'stop';stop.write_text(sandbox);stop.chmod(0o755)
    env=dict(os.environ,TRACE=str(trace))
    result=subprocess.run(['sh',str(stop),'stop'],env=env,capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    calls=trace.read_text().splitlines()
    assert 'killall -q -9 start_frpc.sh' in calls,calls
    assert 'killall -q -9 mqtt_bridge' in calls,calls
    assert 'killall -q -9 tk8710_gw' in calls,calls
    assert calls[-1]=='reset',calls
    assert ipc.read_text()=='pending uplink'
    trace.write_text('')
    result=subprocess.run(['sh',str(stop),'stop'],env=dict(env,PROCESS_GONE='0'),capture_output=True,text=True)
    assert result.returncode!=0 and 'reset' not in trace.read_text().splitlines()
    result=subprocess.run(['sh',str(stop),'restart'],env=dict(env,RESET_FAIL='7'),capture_output=True,text=True)
    assert result.returncode==7 and 'Starting' not in result.stdout
    print('PASS: WSL stop policy, configured FRP/MQTT force-stop, IPC preservation, reset/restart failure gates')
