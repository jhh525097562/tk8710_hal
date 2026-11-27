"""ARM/QEMU tests of real snapshot parser and arbitration. Never requests GPIO."""
import json,os,pathlib,subprocess,tempfile,unittest
HERE=pathlib.Path(__file__).resolve().parent
HOST=pathlib.Path('/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506b_iot_emmc/host')
HARNESS=r'''
#define main led_program_main
#include "led_agent.c"
#undef main
#include <assert.h>
FILE *__real_fopen(const char*,const char*);
FILE *__wrap_fopen(const char *p,const char *m) {
 if (!strncmp(p,"/proc/",6)) {
  if (strstr(p,"/stat"))return __real_fopen(getenv("PROC_STAT"),m);
  if (strstr(p,"/cmdline"))return __real_fopen(getenv("PROC_CMD"),m);
 }
 return __real_fopen(p,m);
}
int main(int argc,char **argv) {
 if(argc==3) {
  strcpy(boot_id,"01234567-89ab-cdef-0123-456789abcdef");
  printf("%d\n",read_input(argv[1],atoi(argv[2]),100000,100000));return 0;
 }
 LedPolicy p={0};
 assert(LedDecide(&p,INPUT_MISSING,INPUT_MISSING,INPUT_MISSING,1000,1000,0)==LED_PREPARING);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_READY,2000,2000,0)==LED_PREPARING);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_READY,5000,5000,0)==LED_READY);
 assert(LedDecide(&p,INPUT_DOWN,INPUT_FAULT,INPUT_FAULT,6000,6000,0)==LED_NETWORK_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_PREPARING,7000,7000,0)==LED_PREPARING);
 assert(LedDecide(&p,INPUT_READY,INPUT_FAULT,INPUT_READY,8000,8000,0)==LED_BUSINESS_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_PREPARING,9000,9000,0)==LED_BUSINESS_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_READY,10000,10000,0)==LED_BUSINESS_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_READY,40000,40000,0)==LED_READY);
 assert(LedDecide(&p,INPUT_READY,INPUT_MISSING,INPUT_READY,41000,41000,0)==LED_BUSINESS_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_MISSING,INPUT_MISSING,42000,42000,1)==LED_MAINTENANCE);
 assert(LedDecide(&p,INPUT_DOWN,INPUT_MISSING,INPUT_MISSING,43000,43000,1)==LED_NETWORK_FAULT);
 memset(&p,0,sizeof(p));
 LedObserveGateway(&p,10,1000,0);LedObserveGateway(&p,20,2000,0);
 LedObserveGateway(&p,30,3000,0);LedObserveGateway(&p,40,4000,0);
 assert(p.fault_latched && p.restarts==3);
 for(int low=0;low<2;low++)for(int m=0;m<10;m++)for(int t=0;t<8000;t+=50) {
  int values[2];LedLevels(m,t,low,values);
  assert(!((values[0]!=low) && (values[1]!=low)));
  if(m==LED_READY)assert(values[0]!=low && values[1]==low);
  if(m==LED_NETWORK_FAULT)assert(values[0]==low && values[1]!=low);
  if(m==LED_MAINTENANCE)assert((values[0]!=low)==((t/1000)%2==0));
 }
 const int modes[]={LED_PREPARING,LED_GREEN_NS,LED_GREEN_GPS,LED_RED_MQTT,LED_RED_NS,LED_RED_GPS,LED_BUSINESS_FAULT};
 const int counts[]={1,2,3,1,2,3,4};
 for(int i=0;i<7;i++) {
  int rises=0,last=0,period=(counts[i]*2-1)*250+2000;
  for(int t=0;t<period;t+=50) {
   int v[2];LedLevels(modes[i],t,0,v);int on=v[0]||v[1];
   if(on && !last)rises++;
   last=on;
   if(t>=(counts[i]*2-1)*250)assert(!on);
  }
  assert(rises==counts[i]);
 }
 memset(&p,0,sizeof(p));
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_WAIT_NS,1000,1000,0)==LED_GREEN_NS);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_WAIT_GPS,1100,1100,0)==LED_GREEN_GPS);
 assert(LedDecide(&p,INPUT_READY,INPUT_MQTT_FAULT,INPUT_NS_FAULT,1200,1200,0)==LED_RED_MQTT);
 assert(LedDecide(&p,INPUT_READY,INPUT_MQTT_FAULT,INPUT_GPS_FAULT,1300,1300,0)==LED_RED_GPS);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_MISSING,1400,1400,0)==LED_RED_GPS);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_WAIT_NS,1500,1500,0)==LED_RED_GPS);
 assert(LedDecide(&p,INPUT_DOWN,INPUT_MQTT_FAULT,INPUT_GPS_FAULT,1600,1600,0)==LED_NETWORK_FAULT);
 assert(LedDecide(&p,INPUT_READY,INPUT_READY,INPUT_HW_FAULT,1700,1700,0)==LED_BUSINESS_FAULT);
 puts("PASS grouped pulses, 2s gap, category priority/latching, grace/recovery and exclusive colors");return 0;
}
'''
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory(prefix='led-arbiter-test-');cls.root=pathlib.Path(cls.tmp.name)
  (cls.root/'test.c').write_text(HARNESS);cls.exe=cls.root/'test'
  subprocess.run([str(HOST/'bin/arm-buildroot-linux-gnueabihf-gcc'),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',
   '-I'+str(HERE/'src'),'-I'+str(HERE/'inc'),'-I'+str(HERE.parent/'common/inc'),str(cls.root/'test.c'),
   str(HERE/'src/led_policy.c'),str(HERE.parent/'common/src/file_logger.c'),'-lgpiod','-ljson-c','-Wl,--wrap=fopen','-o',str(cls.exe)],check=True)
  cls.runner=['qemu-arm-static','-L',str(HOST/'arm-buildroot-linux-gnueabihf/sysroot'),str(cls.exe)]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def data(self,kind=2,**kw):
  j=dict(schema=1,pid=123,start_ticks=456,boot_id='01234567-89ab-cdef-0123-456789abcdef',state='running',reason='business_running',
    business_ready=True,exited=False,updated_mono_ms=99000,state_since_mono_ms=90000)
  if kind==1:j.update(clock='monotonic',state='connected',ready=True,connack_ok=True,subscribed=True)
  if kind==0:j.update(clock='boottime',state='up',ready=True,link_up=True,ipv4_ready=True,updated_boottime_ms=99000,checked_boottime_ms=95000,publisher_progress_boottime_ms=99000)
  j.update(kw);return j
 def evaluate(self,j,kind=2,cmd=None,state='S'):
  (self.root/'data').write_text(json.dumps(j))
  (self.root/'stat').write_text('123 (test name) '+state+' '+'0 '*18+'456 0\n')
  (self.root/'cmd').write_bytes(cmd or ([b'/bin/sh\0/usr/bin/tdaemon.sh\0',b'/userdata/mqtt_bridge\0',b'/userdata/tk8710_gw\0'][kind]))
  r=subprocess.run(self.runner+[str(self.root/'data'),str(kind)],env=dict(os.environ,PROC_STAT=str(self.root/'stat'),PROC_CMD=str(self.root/'cmd')),capture_output=True,text=True,check=True)
  return int(r.stdout.strip())
 def test_policy_and_output(self):subprocess.run(self.runner,check=True)
 def test_all_ready(self):
  for k in range(3):self.assertEqual(self.evaluate(self.data(k),k),2)
 def test_reused_pid(self):self.assertEqual(self.evaluate(self.data(start_ticks=457)),0)
 def test_wrong_boot(self):self.assertEqual(self.evaluate(self.data(boot_id='old')),0)
 def test_zombie(self):self.assertEqual(self.evaluate(self.data(),state='Z'),0)
 def test_wrong_process(self):self.assertEqual(self.evaluate(self.data(),cmd=b'/userdata/other\0'),0)
 def test_old_gateway(self):self.assertEqual(self.evaluate(self.data(updated_mono_ms=1)),0)
 def test_gps_wait_timeout(self):
  self.assertEqual(self.evaluate(self.data(state='wait_gps_sync',state_since_mono_ms=1,business_ready=False)),6)
  self.assertEqual(self.evaluate(self.data(state='wait_pps_alignment',state_since_mono_ms=1,business_ready=False)),10)
 def test_small_clock_race_allowed(self):self.assertEqual(self.evaluate(self.data(updated_mono_ms=100001)),2)
 def test_future_rejected(self):self.assertEqual(self.evaluate(self.data(updated_mono_ms=200000)),0)
 def test_mqtt_suback_required(self):self.assertEqual(self.evaluate(self.data(1,subscribed=False),1),0)
 def test_network_down(self):self.assertEqual(self.evaluate(self.data(0,state='down',ready=False),0),4)
 def test_network_probe_stale(self):self.assertEqual(self.evaluate(self.data(0,checked_boottime_ms=1),0),0)
 def test_fault_snapshot(self):self.assertEqual(self.evaluate(self.data(state='fault',exited=True,business_ready=False)),3)
 def test_schema_types(self):self.assertEqual(self.evaluate(self.data(schema='1')),0)
 def test_fault_categories(self):
  for reason,expected in [('gps_module_unavailable',10),('pps_alignment_failed',10),('invalid_gps_policy',9),('ns_config_timeout',9),('hal_init_failed',11),('rf_channels_failed',11),('other_error',3)]:
   self.assertEqual(self.evaluate(self.data(state='fault',reason=reason,business_ready=False)),expected)
 def test_mqtt_fault_category(self):self.assertEqual(self.evaluate(self.data(1,state='fault',ready=False),1),8)
 def test_ns_wait_and_scan(self):
  self.assertEqual(self.evaluate(self.data(state='wait_ns_config',business_ready=False)),5)
  self.assertEqual(self.evaluate(self.data(state='scanning',business_ready=False)),7)
if __name__=='__main__':unittest.main(verbosity=2)
