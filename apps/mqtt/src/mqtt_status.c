#define _GNU_SOURCE
#include "mqtt_status.h"
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int dir_fd = -1, lock_fd = -1, connack_ok, subscribed, pending_mid = -1;
static int closed, ever_connected, error_code, warned, terminal_fault;
static const char *state = "connecting";
static char reason[64] = "process_start", boot_id[64];
static unsigned long long start_ticks;
static uint64_t since, written;

static uint64_t now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void set_state(const char *next, const char *why, int rc)
{
    if (strcmp(state, next)) since = now_ms();
    state = next; error_code = rc;
    snprintf(reason, sizeof(reason), "%s", why);
}

static void publish(int force)
{
    uint64_t now = now_ms();
    if (dir_fd < 0 || (!force && written && now - written < 1000)) return;
    struct json_object *j = json_object_new_object();
    if (!j) return;
#define STR(k,v) json_object_object_add(j,k,json_object_new_string(v))
#define NUM(k,v) json_object_object_add(j,k,json_object_new_int64((int64_t)(v)))
#define BOOL(k,v) json_object_object_add(j,k,json_object_new_boolean(v))
    NUM("schema",1); NUM("pid",getpid()); NUM("start_ticks",start_ticks); STR("boot_id",boot_id);
    STR("clock","monotonic"); STR("state",state); STR("reason",reason); NUM("error_code",error_code);
    BOOL("connack_ok",connack_ok); BOOL("subscribed",subscribed);
    BOOL("ready",connack_ok && subscribed && !closed && !strcmp(state,"connected"));
    BOOL("exited",closed); NUM("pending_mid",pending_mid);
    NUM("state_since_mono_ms",since); NUM("updated_mono_ms",now);
#undef STR
#undef NUM
#undef BOOL
    char tmp[64]; snprintf(tmp,sizeof(tmp),"mqtt_status.%ld.tmp",(long)getpid());
    int fd=openat(dir_fd,tmp,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0644);
    int bad=fd<0;
    if (!bad) {
        if (fchmod(fd,0644)) bad=1;
        const char *data=json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN);
        size_t length=strlen(data), offset=0;
        while (!bad && offset<length) {
            ssize_t n=write(fd,data+offset,length-offset);
            if (n<0 && errno==EINTR) continue;
            if (n<=0) { bad=1; break; }
            offset+=(size_t)n;
        }
        if (close(fd)) bad=1;
    }
    if (!bad && renameat(dir_fd,tmp,dir_fd,"mqtt_status.json")) bad=1;
    if (bad) {
        int e=errno; (void)unlinkat(dir_fd,tmp,0);
        if (!warned) fprintf(stderr,"MQTT status write failed: %s\n",strerror(e));
        warned=1;
    } else { written=now; warned=0; }
    json_object_put(j);
}

int mqtt_status_init(const char *directory)
{
    if (!directory || !*directory) return -1;
    char path[64], buf[2048], *p, *end;
    FILE *fp;
    pthread_mutex_lock(&lock);
    if (dir_fd>=0) { pthread_mutex_unlock(&lock); return -1; }
    if (mkdir(directory,0755) && errno!=EEXIST) goto fail;
    dir_fd=open(directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (dir_fd<0) goto fail;
    lock_fd=openat(dir_fd,"mqtt_status.lock",O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);
    if (lock_fd<0 || flock(lock_fd,LOCK_EX|LOCK_NB)) goto fail;
    snprintf(path,sizeof(path),"/proc/%ld/stat",(long)getpid());
    fp=fopen(path,"r"); if (!fp) goto fail;
    int ok=fgets(buf,sizeof(buf),fp)!=NULL; fclose(fp);
    if (!ok || !(p=strrchr(buf,')'))) goto fail;
    ++p;
    for (int field=3;field<22;++field) {
        while (*p==' ') ++p;
        if (!*p) goto fail;
        while (*p && *p!=' ') ++p;
    }
    errno=0; start_ticks=strtoull(p,&end,10);
    if (errno || end==p || !start_ticks) goto fail;
    fp=fopen("/proc/sys/kernel/random/boot_id","r"); if (!fp) goto fail;
    ok=fgets(boot_id,sizeof(boot_id),fp)!=NULL; fclose(fp);
    if (!ok) goto fail;
    boot_id[strcspn(boot_id,"\r\n")]=0;
    if (strlen(boot_id)!=36) goto fail;
    closed=connack_ok=subscribed=ever_connected=error_code=warned=terminal_fault=0;
    pending_mid=-1; since=now_ms(); written=0;
    set_state("connecting","process_start",0); publish(1);
    pthread_mutex_unlock(&lock); return 0;
fail:
    if (lock_fd>=0) close(lock_fd);
    if (dir_fd>=0) close(dir_fd);
    lock_fd=dir_fd=-1;
    pthread_mutex_unlock(&lock); return -1;
}

void mqtt_status_connack(int rc)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault) {
        connack_ok=rc==0; subscribed=0; pending_mid=-1;
        set_state(rc ? "fault" : "connecting",rc ? "broker_rejected" : "awaiting_suback",rc);
        /* Reconnection starts a fresh SUBACK deadline, even from connecting. */
        since=now_ms(); publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_subscribe(int mid,int rc)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault && connack_ok) {
        pending_mid=rc ? -1 : mid;
        if (rc) set_state("fault","subscribe_request_failed",rc);
        publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_suback(int mid,int count,const int *qos)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault && connack_ok && pending_mid>=0 && pending_mid==mid) {
        int ok=count==1 && qos && qos[0]>=0 && qos[0]<=2;
        pending_mid=-1; subscribed=ok;
        if (ok) ever_connected=1;
        set_state(ok ? "connected" : "fault",ok ? "subscription_confirmed" : "subscription_rejected",ok ? 0 : -1);
        publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_disconnect(int rc)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault) {
        connack_ok=subscribed=0; pending_mid=-1;
        set_state("reconnecting","broker_disconnected",rc); publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_connect_error(int rc)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault) {
        connack_ok=subscribed=0; pending_mid=-1;
        set_state(ever_connected ? "reconnecting" : "connecting","connect_attempt_failed",rc); publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_fault(const char *why,int rc)
{
    pthread_mutex_lock(&lock);
    if (!closed && !terminal_fault) {
        terminal_fault=1;
        set_state("fault",why,rc); subscribed=0; pending_mid=-1; publish(1);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_tick(void)
{
    pthread_mutex_lock(&lock);
    if (!closed) {
        if ((!strcmp(state,"connecting") || !strcmp(state,"reconnecting")) && now_ms()-since>=10000)
            set_state("fault",connack_ok ? "suback_timeout" : "connection_timeout",-1);
        publish(0);
    }
    pthread_mutex_unlock(&lock);
}

void mqtt_status_stop(void)
{
    pthread_mutex_lock(&lock);
    if (!closed) {
        closed=1; connack_ok=subscribed=0; pending_mid=-1;
        if (strcmp(state,"fault")) set_state("stopped","process_exit",0);
        publish(1);
        if (dir_fd>=0) close(dir_fd);
        if (lock_fd>=0) close(lock_fd);
        dir_fd=lock_fd=-1;
    }
    pthread_mutex_unlock(&lock);
}
