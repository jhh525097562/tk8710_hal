/* Execute the production main loop/callback registrations with simulated IO.
 * No GPIO, shared memory, broker, or production status files are touched. */
#define _GNU_SOURCE
#define main bridge_main
#include "../src/main.c"
#undef main
#include <assert.h>
#include <sys/eventfd.h>

static const char *root, *scenario;
static uint64_t mono_seconds = 100;
static int step, notify_event, next_mid = 40, publish_count;
static void *callback_context;
static struct mosquitto *client;
static void (*connected_cb)(struct mosquitto *, void *, int);
static void (*subscribed_cb)(struct mosquitto *, void *, int, int, const int *);
static void (*disconnected_cb)(struct mosquitto *, void *, int);

static const char *mapped(const char *path, char *buf, size_t size) {
    if (!strcmp(path, "/run/led") || !strcmp(path, "/run/led/mqtt.tmp") ||
        !strcmp(path, "/run/led/mqtt.state") || !strncmp(path, "/var/run/mqtt_stats.", 20)) {
        const char *name = !strcmp(path, "/run/led") ? "" : strrchr(path, '/');
        snprintf(buf, size, "%s%s", root, name);
        return buf;
    }
    return path;
}
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *path, const char *mode) {
    char buf[1024];
    if (!strncmp(path, "/proc/", 6) && strstr(path, "/stat"))
        return __real_fopen(getenv("PROC_FIXTURE"), mode);
    if (!strcmp(path, GPS_INFO_FILE_PATH))
        return __real_fopen(getenv("GPS_FIXTURE"), mode);
    return __real_fopen(mapped(path, buf, sizeof(buf)), mode);
}
int __real_mkdir(const char *, mode_t);
int __wrap_mkdir(const char *path, mode_t mode) {
    char buf[1024];
    return __real_mkdir(mapped(path, buf, sizeof(buf)), mode);
}
int __real_rename(const char *, const char *);
int __wrap_rename(const char *from, const char *to) {
    char a[1024], b[1024];
    return __real_rename(mapped(from,a,sizeof(a)), mapped(to,b,sizeof(b)));
}
int __real_clock_gettime(clockid_t, struct timespec *);
int __wrap_clock_gettime(clockid_t id, struct timespec *t) {
    if (id != CLOCK_MONOTONIC) return __real_clock_gettime(id,t);
    t->tv_sec = mono_seconds; t->tv_nsec = 0; return 0;
}
int __real_mqtt_status_init(const char *);
int __wrap_mqtt_status_init(const char *directory) {
    assert(!strcmp(directory, "/run/led"));
    return __real_mqtt_status_init(root);
}
static void snapshot(const char *label) {
    char path[1024]; snprintf(path,sizeof(path),"%s/mqtt_status.json",root);
    struct json_object *j=json_object_from_file(path);
    assert(j); printf("SNAP %s %s\n",label,json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN));
    json_object_put(j);
}

/* Hardware/filesystem boundaries. */
int file_logger_write(const char *path, const char *level, const char *fmt, ...) {
    (void)path; (void)level; (void)fmt; return 0;
}
int file_logger_fprintf(const char *path, FILE *stream, const char *fmt, ...) {
    (void)path; (void)stream; (void)fmt; return 0;
}
int log_send(int level,const char *content,time_t when) {
    (void)level; (void)content; (void)when; return 0;
}
struct gpiod_chip *gpiod_chip_open_by_name(const char *name) {
    (void)name; errno=ENODEV; return NULL;
}
int load_config(const char *path,AppConfig *cfg) {
    (void)path; snapshot("startup");
    if (!strcmp(scenario,"config_error")) return -1;
    strcpy(cfg->gateway_id,"status-integration"); strcpy(cfg->mqtt_host,"127.0.0.1");
    strcpy(cfg->pub_topic,"test/up");
    if (strcmp(scenario,"missing_topic")) strcpy(cfg->sub_topic,"test/down");
    cfg->mqtt_port=1883; cfg->heartbeat_interval=60; return 0;
}
const char *get_net_interface_name(NetworkInterfaceType t) {(void)t;return "test";}
int ipc_smp_channel_init(const ipc_smp_cfg_t *cfg,ipc_smp_handle_t **out) {
    (void)cfg;
    if (!strcmp(scenario,"ipc_error")) return IPC_SMP_ERR_SYS;
    *out=(ipc_smp_handle_t *)(uintptr_t)1; return IPC_SMP_OK;
}
int ipc_smp_get_eventfd(ipc_smp_handle_t *h) {(void)h;return notify_event;}
int ipc_smp_periodic_check(ipc_smp_handle_t *h) {(void)h;return 0;}
int ipc_smp_clear_notify(ipc_smp_handle_t *h) {(void)h;return 0;}
int ipc_smp_ack(ipc_smp_handle_t *h,uint32_t seq) {(void)h;(void)seq;return 0;}
int ipc_smp_send(ipc_smp_handle_t *h,uint16_t type,const void *buf,uint32_t len,uint32_t flags) {
    (void)h;(void)type;(void)buf;(void)len;(void)flags;return 0;
}
int ipc_smp_recv(ipc_smp_handle_t *h,ipc_smp_msg_t *msg,int timeout) {
    (void)h;(void)msg;(void)timeout;return IPC_SMP_ERR_EMPTY;
}
void ipc_smp_channel_close(ipc_smp_handle_t *h) {(void)h;}
ns_scan_manager *ns_scan_manager_open(const char *d,const char *s,ns_scan_publish_fn p,void *c) {
    (void)d;(void)s;(void)p;(void)c;return (ns_scan_manager *)(uintptr_t)1;
}
int ns_scan_manager_handle(ns_scan_manager *m,struct json_object *j) {(void)m;(void)j;return 0;}
void ns_scan_manager_tick(ns_scan_manager *m) {(void)m;}
void ns_scan_manager_close(ns_scan_manager *m) {(void)m;}

/* Simulate broker results at the registered production callback boundary. */
struct mosquitto *__real_mosquitto_new(const char *,bool,void *);
struct mosquitto *__wrap_mosquitto_new(const char *id,bool clean,void *obj) {
    callback_context=obj; client=__real_mosquitto_new(id,clean,obj); return client;
}
void mosquitto_connect_callback_set(struct mosquitto *m,void (*cb)(struct mosquitto *,void *,int)) {
    (void)m;connected_cb=cb;
}
void mosquitto_disconnect_callback_set(struct mosquitto *m,void (*cb)(struct mosquitto *,void *,int)) {
    (void)m;disconnected_cb=cb;
}
void mosquitto_subscribe_callback_set(struct mosquitto *m,void (*cb)(struct mosquitto *,void *,int,int,const int *)) {
    (void)m;subscribed_cb=cb;
}
int mosquitto_connect(struct mosquitto *m,const char *host,int port,int keepalive) {
    (void)m;(void)host;(void)port;(void)keepalive;
    return !strcmp(scenario,"connect_error") ? MOSQ_ERR_ERRNO : MOSQ_ERR_SUCCESS;
}
int mosquitto_subscribe(struct mosquitto *m,int *mid,const char *topic,int qos) {
    (void)m;(void)qos;assert(!strcmp(topic,"test/down"));
    if(mid)*mid=++next_mid;
    return !strcmp(scenario,"subscribe_error") ? MOSQ_ERR_NO_CONN : MOSQ_ERR_SUCCESS;
}
int mosquitto_publish(struct mosquitto *m,int *mid,const char *topic,int len,const void *payload,int qos,bool retain) {
    (void)m;(void)mid;(void)topic;(void)len;(void)qos;(void)retain;
    struct json_object *j=json_tokener_parse(payload), *alive, *value;
    assert(j && json_object_object_get_ex(j,"alive",&alive));
    assert(json_object_object_get_ex(alive,"count",&value) && json_object_get_int(value)==32);
    assert(json_object_object_get_ex(alive,"avgsnr",&value) && json_object_get_int(value)==37);
    json_object_put(j); ++publish_count; return MOSQ_ERR_SUCCESS;
}
int mosquitto_loop_start(struct mosquitto *m) {
    (void)m;assert(connected_cb && subscribed_cb && disconnected_cb);
    return !strcmp(scenario,"loop_error") ? MOSQ_ERR_ERRNO : MOSQ_ERR_SUCCESS;
}
int mosquitto_loop_stop(struct mosquitto *m,bool force) {
    (void)force; snapshot("teardown");
    /* A late network callback during teardown must not revive readiness. */
    connected_cb(m,callback_context,0); return 0;
}

int __wrap_epoll_wait(int fd,struct epoll_event *events,int max,int timeout) {
    (void)fd;(void)max;(void)timeout;
    int qos=0;
    if (!step++) {
        snapshot("first_tick");
        if (!strcmp(scenario,"connect_error")) {mono_seconds+=11;return 0;}
        if (!strcmp(scenario,"epoll_error")) {errno=EIO;return -1;}
        if (!strcmp(scenario,"epoll_hup")) {
            events[0].data.fd=notify_event;events[0].events=EPOLLHUP;return 1;
        }
        connected_cb(client,callback_context,!strcmp(scenario,"broker_rejected") ? 5 : 0);
        snapshot("connack");
        if (!strcmp(scenario,"suback_timeout")) {mono_seconds+=11;return 0;}
        if (strcmp(scenario,"normal") && strcmp(scenario,"rejected_suback")) {raise(SIGTERM);return 0;}
        subscribed_cb(client,callback_context,next_mid+1,1,&qos);snapshot("wrong_mid");
        if (!strcmp(scenario,"rejected_suback")) qos=128;
        subscribed_cb(client,callback_context,next_mid,1,&qos);snapshot("suback");
        if (qos==128) {raise(SIGTERM);return 0;}
        mono_seconds+=2; return 0;
    }
    snapshot("refreshed");
    if (!strcmp(scenario,"normal")) {
        disconnected_cb(client,callback_context,7); snapshot("disconnected");
        subscribed_cb(client,callback_context,next_mid,1,&qos);snapshot("late_ack");
        send_heartbeat(callback_context); snapshot("heartbeat_disconnected");
        connected_cb(client,callback_context,0);snapshot("reconnecting");
        subscribed_cb(client,callback_context,next_mid-1,1,&qos);snapshot("old_mid");
        subscribed_cb(client,callback_context,next_mid,1,&qos);snapshot("recovered");
        assert(publish_count>=3);
    }
    raise(SIGTERM);return 0;
}

int main(int argc,char **argv) {
    assert(argc==3);scenario=argv[1];root=argv[2];
    notify_event=eventfd(0,EFD_CLOEXEC);assert(notify_event>=0);
    char *args[]={"mqtt_bridge",NULL};
    int rc=bridge_main(1,args);
    snapshot("exit");printf("RESULT %d\n",rc);
    close(notify_event);return 0;
}
