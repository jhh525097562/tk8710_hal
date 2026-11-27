#define _GNU_SOURCE
#include "led_policy.h"
#include "file_logger.h"
#include <gpiod.h>
#include <json-c/json.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif
#define LOG "/var/log/led_agent.log"
static volatile sig_atomic_t stopping;
static char boot_id[64];
static uint64_t observed_gw_start;
static void stop(int sig) { (void)sig; stopping=1; }
static uint64_t clock_ms(clockid_t clock)
{
    struct timespec ts;
    if (clock_gettime(clock,&ts)) return 0;
    return (uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000;
}
static uint64_t age_ms(uint64_t now,uint64_t stamp) {return now>=stamp ? now-stamp : 0;}
static int stale(uint64_t now,uint64_t stamp,uint64_t limit)
{
    /* Producers may replace a snapshot just after this polling tick sampled now. */
    return stamp>now ? stamp-now>1000 : now-stamp>limit;
}
static int number(struct json_object *j,const char *key,uint64_t *out)
{
    struct json_object *v;
    if (!json_object_object_get_ex(j,key,&v) || !json_object_is_type(v,json_type_int) ||
        json_object_get_int64(v)<0) return 0;
    *out=(uint64_t)json_object_get_int64(v); return 1;
}
static const char *text(struct json_object *j,const char *key)
{
    struct json_object *v;
    return json_object_object_get_ex(j,key,&v) && json_object_is_type(v,json_type_string) ? json_object_get_string(v) : "";
}
static int boolean(struct json_object *j,const char *key,int expected)
{
    struct json_object *v;
    return json_object_object_get_ex(j,key,&v) && json_object_is_type(v,json_type_boolean) &&
        !!json_object_get_boolean(v)==expected;
}
static int identity(struct json_object *j,int kind)
{
    uint64_t pid,start;
    char path[96],buf[4096];
    if (!number(j,"pid",&pid) || pid<2 || pid>4194304 || !number(j,"start_ticks",&start) ||
        strcmp(text(j,"boot_id"),boot_id)) return 0;
    snprintf(path,sizeof(path),"/proc/%llu/stat",(unsigned long long)pid);
    FILE *f=fopen(path,"r"); if (!f) return 0;
    int ok=fgets(buf,sizeof(buf),f)!=NULL; fclose(f);
    char *p=ok ? strrchr(buf,')') : NULL;
    if (!p) return 0;
    p+=2; if (*p=='Z' || *p=='X') return 0;
    for(int n=3;n<22;n++) { while(*p==' ')p++; if(!*p)return 0; while(*p && *p!=' ')p++; }
    if (strtoull(p,NULL,10)!=start) return 0;
    snprintf(path,sizeof(path),"/proc/%llu/cmdline",(unsigned long long)pid);
    f=fopen(path,"r"); if (!f) return 0;
    size_t size=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[size]=0;
    if (!size) return 0;
    if (kind==0) {
        for(size_t i=0;i<size;) {
            if (!strcmp(buf+i,"/usr/bin/tdaemon.sh")) return 1;
            i+=strlen(buf+i)+1;
        }
        return 0;
    }
    const char *base=strrchr(buf,'/');base=base ? base+1 : buf;
    return !strcmp(base,kind==1 ? "mqtt_bridge" : "tk8710_gw");
}
static LedInput gateway_fault(const char *reason)
{
    if(!strcmp(reason,"invalid_gps_policy") || !strncmp(reason,"ns_config",9))return INPUT_NS_FAULT;
    if(!strncmp(reason,"gps_",4) || !strncmp(reason,"pps_",4))return INPUT_GPS_FAULT;
    if(!strncmp(reason,"hal_",4) || !strncmp(reason,"rf_",3) ||
       !strncmp(reason,"trm_",4) || !strncmp(reason,"console_or_irq",14) ||
       !strcmp(reason,"scan_restore_failed"))return INPUT_HW_FAULT;
    return INPUT_FAULT;
}
static LedInput read_input(const char *path,int kind,uint64_t mono,uint64_t boot)
{
    struct stat st;
    if (stat(path,&st) || st.st_size>8192) return INPUT_MISSING;
    struct json_object *j=json_object_from_file(path);
    if (!j) return INPUT_MISSING;
    LedInput result=INPUT_MISSING;
    uint64_t schema,updated,since;
    const char *state=text(j,"state");
    if (!number(j,"schema",&schema) || schema!=1) goto done;
    if(!identity(j,kind)) {
        uint64_t previous;
        /* Only retain a fault from the exact previously observed gateway life;
         * never trust a dead process's ready/preparing snapshot. */
        if(kind!=2 || strcmp(state,"fault") || strcmp(text(j,"boot_id"),boot_id) ||
           !number(j,"start_ticks",&previous) || !previous || previous!=observed_gw_start)goto done;
    }
    if(kind==2) (void)number(j,"start_ticks",&observed_gw_start);
    if (kind==0) {
        uint64_t checked,progress;
        if (strcmp(text(j,"clock"),"boottime") || !number(j,"updated_boottime_ms",&updated) ||
            stale(boot,updated,5000) || !number(j,"checked_boottime_ms",&checked) ||
            !number(j,"publisher_progress_boottime_ms",&progress) || checked>boot+1000 || progress>boot+1000) goto done;
        if (!strcmp(state,"unknown")) {
            result=!strcmp(text(j,"reason"),"process_start") || !strcmp(text(j,"reason"),"maintenance") ? INPUT_PREPARING : INPUT_FAULT;
        } else if (age_ms(boot,progress)>45000) result=INPUT_FAULT;
        else if (!strcmp(state,"down") && age_ms(boot,checked)<=45000) result=INPUT_DOWN;
        else if (!strcmp(state,"up") && age_ms(boot,checked)<=45000 && boolean(j,"ready",1) &&
                 boolean(j,"link_up",1) && boolean(j,"ipv4_ready",1)) result=INPUT_READY;
        else if (!strcmp(state,"checking") || !strcmp(state,"switching")) result=INPUT_PREPARING;
    } else {
        if ((kind==1 && strcmp(text(j,"clock"),"monotonic")) ||
            !number(j,"updated_mono_ms",&updated) || stale(mono,updated,kind==1 ? 5000 : 45000) ||
            !number(j,"state_since_mono_ms",&since) || since>mono+1000) goto done;
        if (!strcmp(state,"fault")) { result=kind==1 ? INPUT_MQTT_FAULT : gateway_fault(text(j,"reason")); goto done; }
        if (!boolean(j,"exited",0)) goto done;
        if (kind==1) {
            if (!strcmp(state,"connected") && boolean(j,"ready",1) && boolean(j,"connack_ok",1) && boolean(j,"subscribed",1)) result=INPUT_READY;
            else if (!strcmp(state,"connecting") || !strcmp(state,"reconnecting"))
                result=age_ms(mono,since)<=10000 ? INPUT_PREPARING : INPUT_MQTT_FAULT;
        } else {
            if (!strcmp(state,"running") && boolean(j,"business_ready",1)) result=INPUT_READY;
            else if (!strcmp(state,"degraded")) result=gateway_fault(text(j,"reason"));
            else {
                const char *names[]={"starting","wait_ns_config","configuring","wait_gps_sync","wait_pps_alignment","stopping","scanning"};
                const unsigned limits[]={60,35,120,330,90,30,600};
                for(unsigned i=0;i<sizeof(limits)/sizeof(limits[0]);i++)
                    if (!strcmp(state,names[i])) {
                        if(age_ms(mono,since)>(uint64_t)limits[i]*1000)
                            result=i==1 ? INPUT_NS_FAULT : (i==3 || i==4) ? INPUT_GPS_FAULT : INPUT_HW_FAULT;
                        else result=i==1 ? INPUT_WAIT_NS : (i==3 || i==4) ? INPUT_WAIT_GPS : i==6 ? INPUT_OPERATION : INPUT_PREPARING;
                    }
            }
        }
    }
done:
    json_object_put(j);return result;
}
static const char *mode_name(LedMode m)
{
    static const char *names[]={"green_single","ready","network_fault","red_four",
        "green_double","green_triple","green_slow","red_single","red_double","red_triple"};
    return names[m];
}
static void save_policy(const LedPolicy *p)
{
    FILE *f=fopen("/run/led/arbiter_history.tmp","w"); if (!f) return;
    int bad=fprintf(f,"%s %u %u %llu %llu %u %u\n",boot_id,p->ready_seen,p->fault_latched,
        (unsigned long long)p->gw_start,(unsigned long long)p->restart_since,p->restarts,p->fault_class)<0;
    if(fclose(f))bad=1;
    if(!bad)(void)rename("/run/led/arbiter_history.tmp","/run/led/arbiter_history");
}
int main(int argc,char **argv)
{
    int check=argc==2 && !strcmp(argv[1],"--check");
    if(argc>1 && !check) { fprintf(stderr,"Usage: led_agent [--check]\n");return 2; }
    FILE *f=fopen("/proc/sys/kernel/random/boot_id","r");
    if(!f)return 1;
    int ok=fgets(boot_id,sizeof(boot_id),f)!=NULL;fclose(f);if(!ok)return 1;
    boot_id[strcspn(boot_id,"\r\n")]=0;
    if(check) {
        uint64_t mono=clock_ms(CLOCK_MONOTONIC),boot=clock_ms(CLOCK_BOOTTIME);
        printf("inputs: 0=missing 1=preparing 2=ready 3=unknown_fault 4=network_down 5=wait_ns 6=wait_gps 7=operation 8=mqtt_fault 9=ns_fault 10=gps_fault 11=hardware_fault\nnetwork=%d mqtt=%d gateway=%d\n",
            read_input("/run/led/network_status.json",0,mono,boot),read_input("/run/led/mqtt_status.json",1,mono,boot),
            read_input("/var/run/tk8710_gw/status.json",2,mono,boot));return 0;
    }
    if(mkdir("/run/led",0755) && errno!=EEXIST)return 1;
    int lockfd=open("/run/led/arbiter.lock",O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(lockfd<0)return 1;
    if(flock(lockfd,LOCK_EX|LOCK_NB)) {close(lockfd);return 1;}
    struct gpiod_chip *chip=gpiod_chip_open_by_name("gpiochip1");
    struct gpiod_line_bulk lines;
    unsigned offsets[]={23,24};int off[]={LED_ACTIVE_LOW,LED_ACTIVE_LOW};
    if(!chip || gpiod_chip_get_lines(chip,offsets,2,&lines) ||
       gpiod_line_request_bulk_output(&lines,"led_arbiter",off)) {
        file_logger_perror(LOG,"LED GPIO init");if(chip)gpiod_chip_close(chip);close(lockfd);return 1;
    }
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
    LedPolicy policy={0};
    char previous_boot[64];unsigned was_ready,was_fault,restarts,category=0;
    unsigned long long gw_start,restart_since;
    f=fopen("/run/led/arbiter_history","r");
    if(f) {
        if(fscanf(f,"%63s %u %u %llu %llu %u %u",previous_boot,&was_ready,&was_fault,&gw_start,&restart_since,&restarts,&category)>=6 && !strcmp(previous_boot,boot_id)) {
            policy.ready_seen=!!was_ready;policy.fault_latched=!!was_fault;
            policy.gw_start=gw_start;policy.restart_since=restart_since;policy.restarts=restarts;
            policy.fault_class=was_fault && category<=5 ? category : 0;
            observed_gw_start=gw_start;
        }
        fclose(f);
    }
    int previous=-1, last[2]={LED_ACTIVE_LOW,LED_ACTIVE_LOW}, result=0;
    uint64_t maintenance_since=0;
    uint64_t pattern_since=clock_ms(CLOCK_MONOTONIC);
    while(!stopping) {
        uint64_t mono=clock_ms(CLOCK_MONOTONIC),boot=clock_ms(CLOCK_BOOTTIME);
        int maintenance=access("/var/run/app-upgrade-maintenance",F_OK)==0;
        if(maintenance && !maintenance_since)maintenance_since=mono;
        if(!maintenance)maintenance_since=0;
        LedInput net=read_input("/run/led/network_status.json",0,mono,boot);
        LedInput mqtt=read_input("/run/led/mqtt_status.json",1,mono,boot);
        LedInput gw=read_input("/var/run/tk8710_gw/status.json",2,mono,boot);
        uint64_t old_start=policy.gw_start;
        unsigned old_restarts=policy.restarts;
        unsigned old_category=policy.fault_class;
        LedObserveGateway(&policy,observed_gw_start,mono,maintenance);
        if(maintenance && mono-maintenance_since>600000) {maintenance=0;gw=INPUT_FAULT;}
        LedMode mode=LedDecide(&policy,net,mqtt,gw,mono,boot,maintenance);
        if((int)mode!=previous)pattern_since=mono;
        int levels[2];LedLevels(mode,mono-pattern_since,LED_ACTIVE_LOW,levels);
        if(levels[0]!=last[0] || levels[1]!=last[1]) {
            if(gpiod_line_set_value_bulk(&lines,off) || gpiod_line_set_value_bulk(&lines,levels)) {
                file_logger_perror(LOG,"LED output failed");result=1;break;
            }
            memcpy(last,levels,sizeof(last));
        }
        if((int)mode!=previous || old_start!=policy.gw_start || old_restarts!=policy.restarts || old_category!=policy.fault_class) {
            file_logger_printf(LOG,"INFO","mode=%s network=%d mqtt=%d gateway=%d\n",mode_name(mode),net,mqtt,gw);
            save_policy(&policy);previous=mode;
        }
        struct timespec delay={0,50000000};nanosleep(&delay,NULL);
    }
    (void)gpiod_line_set_value_bulk(&lines,off);
    gpiod_line_release_bulk(&lines);gpiod_chip_close(chip);close(lockfd);
    return result;
}
