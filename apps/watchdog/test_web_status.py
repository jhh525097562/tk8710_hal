"""ARM/QEMU status-reader tests with fake process identity/time. No board writes."""
import json, pathlib, subprocess, tempfile, shutil
sdk = pathlib.Path('/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0')
apps = sdk/'external/rk3506app'
host = sdk/'buildroot/output/rockchip_hd_rk3506b_iot_emmc/host'
cc = host/'bin/arm-buildroot-linux-gnueabihf-gcc'
sysroot = host/'arm-buildroot-linux-gnueabihf/sysroot'
with tempfile.TemporaryDirectory(prefix='wdt-web-test-') as tmp:
    root = pathlib.Path(tmp); status = root/'status.json'
    (root/'probe.c').write_text(r'''
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "watchdog_status.h"
FILE *__real_fopen(const char *,const char *);
FILE *__wrap_fopen(const char *p,const char *m) {
 if(!strcmp(p,"/proc/sys/kernel/random/boot_id"))return fmemopen("test-boot\n",10,"r");
 if(!strcmp(p,"/proc/123/comm"))return fmemopen("rk_wdt\n",7,"r");
 return __real_fopen(p,m);
}
int __wrap_kill(pid_t p,int sig) {(void)sig;return p==123?0:-1;}
int __wrap_clock_gettime(clockid_t id,struct timespec *t) {(void)id;t->tv_sec=100;t->tv_nsec=0;return 0;}
int main(void) {struct json_object *o=watchdog_status_read();puts(json_object_to_json_string(o));json_object_put(o);return 0;}
''')
    exe=root/'probe'
    subprocess.run([str(cc),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(apps/'webserver/inc'),f'-DWDT_STATUS_FILE="{status}"',
                    str(apps/'webserver/src/watchdog_status.c'),str(root/'probe.c'),
                    '-ljson-c','-Wl,--wrap=fopen','-Wl,--wrap=kill','-Wl,--wrap=clock_gettime','-o',str(exe)],check=True)
    emulator=shutil.which('qemu-arm') or shutil.which('qemu-arm-static')
    assert emulator, 'Install qemu-user or qemu-user-static to execute the ARM test'
    def run():return json.loads(subprocess.check_output([emulator,'-L',str(sysroot),str(exe)],text=True))
    assert run()['state']=='unavailable'
    status.write_text('{invalid');assert run()['state']=='unavailable'
    for state in ['waiting','feeding','degraded','failed','fault','stopped']:
        obj={'state':state,'boot_id':'test-boot','pid':123,'updated_uptime_s':99}
        status.write_text(json.dumps(obj));assert run()['state']==state,state
    for update in [{'pid':234},{'updated_uptime_s':50},{'updated_uptime_s':110}]:
        obj={'state':'feeding','boot_id':'test-boot','pid':123,'updated_uptime_s':99};obj.update(update)
        status.write_text(json.dumps(obj));assert run()['state']=='stale',update
    status.write_text(json.dumps({'state':'feeding','boot_id':'old-boot'}));assert run()['state']=='unavailable'
    print('PASS ARM status: missing/corrupt, six states, dead owner, stale/future time, previous boot')
