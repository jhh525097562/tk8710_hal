"""WSL/Linux isolated watchdog tests. No real watchdog is opened; reboot/ioctl
and clocks are wrapped at link time only in the test executable.
"""
import fcntl, json, os, pathlib, subprocess, tempfile

here = pathlib.Path(__file__).resolve().parent
mock_source = r'''
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
static double elapsed;
static int rebooted, feeds, opened;
static int mode(const char *s) { return !strcmp(getenv("TEST_CASE"), s); }
int __wrap_setpriority(__priority_which_t which, id_t who, int prio) {
 (void)which; (void)who; (void)prio;
 if(mode("protection_denied")) {errno=EPERM;return -1;} return 0;
}
int __wrap_getpriority(__priority_which_t which, id_t who) {
 (void)which; (void)who; return mode("protection_denied")?0:-10;
}
int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *p, struct stat *st) {
 if (!strcmp(p, WDT_DEV)) {
  if(mode("missing") || mode("wait_stop") || (mode("late") && elapsed<3)) { errno=ENOENT; return -1; }
  memset(st,0,sizeof(*st));st->st_mode=S_IFCHR|0600;return 0;
 }
 return __real_stat(p,st);
}
int __real_open(const char *, int, ...);
int __wrap_open(const char *p,int flags,...) {
 mode_t perms=0;
 if(flags&O_CREAT) { va_list ap;va_start(ap,flags);perms=va_arg(ap,int);va_end(ap); }
 if(!strcmp(p,WDT_DEV)) {
  if(mode("busy")) {errno=EBUSY;return -1;}
  opened++;return __real_open(p,flags);
 }
 return __real_open(p,flags,perms);
}
int __wrap_ioctl(int fd,unsigned long request,...) {
 (void)fd;va_list ap;va_start(ap,request);void *arg=va_arg(ap,void *);va_end(ap);
 if(request==WDIOC_SETTIMEOUT) {
  if(mode("set_fails")) {errno=EINVAL;return -1;}
  return 0;
 }
 if(request==WDIOC_GETTIMEOUT) {
  if(mode("get_fails")) {errno=EIO;return -1;}
  *(int*)arg=mode("too_short")?5:mode("set_fails")?45:60;return 0;
 }
 if(request==WDIOC_KEEPALIVE) {
  feeds++;
  if(mode("feed_fails") || (mode("runtime_fails") && feeds>1)) {errno=EIO;return -1;}
  return 0;
 }
 abort();
}
int __wrap_clock_gettime(clockid_t id,struct timespec *ts) {
 (void)id;ts->tv_sec=(time_t)elapsed;ts->tv_nsec=(long)((elapsed-ts->tv_sec)*1e9);return 0;
}
int __wrap_nanosleep(const struct timespec *req,struct timespec *rem) {
 (void)rem;
 if(rebooted) { if(!opened) abort();exit(86); }
 if(req->tv_sec==5) {
  FILE *src=fopen(WDT_RUN "/status.json","r"),*dst=fopen(WDT_RUN "/feeding-snapshot.json","w");
  if(src && dst){int ch;while((ch=fgetc(src))!=EOF)fputc(ch,dst);}
  if(src)fclose(src);
  if(dst)fclose(dst);
 }
 elapsed+=req->tv_sec+req->tv_nsec/1e9;
 if(mode("wait_stop") || (elapsed>=16 && !mode("missing"))) raise(SIGTERM);
 return 0;
}
void __wrap_sync(void) {}
int __wrap_reboot(int how) { (void)how;rebooted++;errno=EPERM;return -1; }
'''

with tempfile.TemporaryDirectory(prefix='rk-wdt-test-') as tmp:
    root = pathlib.Path(tmp)
    run = root/'run'; run.mkdir()
    sysfs = root/'sys'; sysfs.mkdir()
    device = root/'fake-device'; device.touch()
    mock = root/'mock.c'; mock.write_text(mock_source)
    exe = root/'rk_wdt'
    macros = [f'-DWDT_RUN="{run}"', f'-DWDT_SYS="{sysfs}"',
              f'-DWDT_DEV="{device}"', f'-DWDT_LOG="{root}/log"',
              f'-DWDT_OOM_FILE="{root}/oom"']
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra']+macros+
                   [str(here/'rk_wdt.c'),str(mock),'-o',str(exe)]+
                   [f'-Wl,--wrap={name}' for name in ['stat','open','ioctl','clock_gettime','nanosleep','sync','reboot','setpriority','getpriority']],check=True)
    cases = {'missing':(1,'failed','device_missing'), 'busy':(1,'failed','device_busy'),
             'wrong':(1,'failed','wrong_device_or_sysfs_missing'),
             'no_nowayout':(1,'failed','nowayout_required'),
             'healthy':(0,'stopped','armed_service_terminated'),
             'protection_denied':(0,'stopped','armed_service_terminated'),
             'late':(0,'stopped','armed_service_terminated'),
             'set_fails':(0,'stopped','armed_service_terminated'),
             'get_fails':(86,'fault','armed_setup_failed'),
             'too_short':(86,'fault','armed_setup_failed'),
             'feed_fails':(86,'fault','armed_setup_failed'),
             'runtime_fails':(86,'fault','keepalive_failed'),
             'wait_stop':(0,'stopped','stopped_before_open')}
    for name,(rc,state,reason) in cases.items():
        oom = root/'oom'
        if oom.exists(): oom.unlink()
        if name == 'protection_denied': oom.symlink_to('/dev/full')
        (sysfs/'identity').write_text('other' if name=='wrong' else 'Synopsys DesignWare Watchdog\n')
        (sysfs/'nowayout').write_text('0' if name=='no_nowayout' else '1\n')
        env=dict(os.environ,TEST_CASE=name)
        result=subprocess.run([str(exe)],env=env,timeout=5)
        status=json.loads((run/'status.json').read_text())
        assert (result.returncode,status['state'],status['reason'])==(rc,state,reason),(name,status)
        if name in ['healthy','late','set_fails','protection_denied']:
            active=json.loads((run/'feeding-snapshot.json').read_text())
            assert active['state']==('degraded' if name=='set_fails' else 'feeding')
            assert active['actual_timeout_s']==(45 if name=='set_fails' else 60)
            assert active['last_feed_uptime_s']>=10
            assert active['oom_protected'] == (name != 'protection_denied')
            assert active['priority_protected'] == (name != 'protection_denied')
        print('PASS',name,flush=True)
    before=(run/'status.json').read_bytes()
    with (run/'service.lock').open('w') as held:
        fcntl.flock(held,fcntl.LOCK_EX|fcntl.LOCK_NB)
        assert subprocess.run([str(exe)],env=dict(os.environ,TEST_CASE='healthy'),timeout=3).returncode==0
        assert (run/'status.json').read_bytes()==before
    print('PASS duplicate instance leaves owner state untouched')
