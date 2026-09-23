#define _GNU_SOURCE
#include "log_export.h"
#include <json-c/json.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>

#define MAX_FILES 512
#define MAX_BYTES (128ULL * 1024 * 1024)
#define DMESG_BYTES (4ULL * 1024 * 1024)
#define EXPORT_ROOT "/userdata/.web-log-export"
static const char *types[] = {"web","mqtt","tk8710","daemon","system","upgrade","business"};
static const char *labels[] = {"Web服务","MQTT通信","TK8710网关","守护进程","系统日志","升级日志","业务归档"};
struct source { int type; const char *dir, *base; int mode; };
/* mode 0: exact basename and numeric rotations; 1: Driver/TRM .log; 2: .dat */
static const struct source sources[] = {
    {0,"/var/log","web_server.log",0}, {1,"/var/log","mqtt_bridge.log",0},
    {2,"/userdata/8710log","",1}, {3,"/root","daemon_guard.log",0},
    {3,"/root","tdaemon_debug_full.log",0}, {4,"/var/log","messages",0},
    {5,"/tmp","swupdate.log",0}, {5,"/userdata/.app-upgrade","last-swupdate.log",0},
    {6,"/userdata/logs","",2}
};
struct entry { char id[300], path[512], name[256]; int type; off_t size; time_t mtime; dev_t dev; ino_t ino; };
struct task {
    struct entry *files; int count, mask, completed, exported, skipped;
    char id[32], dir[256], filename[160], error[192];
    enum {IDLE, WORKING, READY, FAILED} state;
};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct task job;
static void reply(struct mg_connection *c, int status, struct json_object *obj) {
    mg_http_reply(c,status,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",
                  "%s",json_object_to_json_string_ext(obj,JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
}
static void error_reply(struct mg_connection *c, int status, const char *message) {
    struct json_object *o=json_object_new_object();
    json_object_object_add(o,"message",json_object_new_string(message)); reply(c,status,o);
}
static int parse_types(const char *input) {
    if (!input || !*input) return 127;
    char copy[128]; if (strlen(input)>=sizeof(copy)) return -1;
    strcpy(copy,input); int mask=0; char *save=NULL;
    for(char *p=strtok_r(copy,",",&save);p;p=strtok_r(NULL,",",&save)) {
        int i; for(i=0;i<7;i++) if(!strcmp(p,types[i])) {mask|=1<<i;break;}
        if(i==7) return -1;
    }
    return mask ? mask : -1;
}
static int ends(const char *s,const char *suffix) {
    return strlen(s)>=strlen(suffix) && !strcmp(s+strlen(s)-strlen(suffix),suffix);
}
static int allowed(const struct source *s,const char *n) {
    if(strchr(n,'/') || strchr(n,'\\') || strstr(n,".."))return 0;
    for(const unsigned char *p=(const unsigned char *)n;*p;p++)if(*p<32 || *p==127)return 0;
    if(s->mode==1)return (!strncmp(n,"tk8710_driver_",14)||!strncmp(n,"trm_log_",8)) && ends(n,".log");
    if(s->mode==2)return ends(n,".dat");
    size_t len=strlen(s->base);
    if(!strcmp(n,s->base))return 1;
    if(strncmp(n,s->base,len)||n[len]!='.')return 0;
    const char *p=n+len+1;if(!isdigit((unsigned char)*p))return 0;
    while(isdigit((unsigned char)*p))p++;
    return !*p || !strcmp(p,".gz");
}
static int enumerate(struct entry *entries,int mask,int *overflow) {
    int count=0;*overflow=0;
    for(size_t i=0;i<sizeof(sources)/sizeof(sources[0]);i++) {
        const struct source *s=&sources[i]; if(!(mask&(1<<s->type)))continue;
        DIR *dir=opendir(s->dir);if(!dir)continue;struct dirent *de;
        while((de=readdir(dir))) {
            if(!allowed(s,de->d_name))continue;
            char path[512];int len=snprintf(path,sizeof(path),"%s/%s",s->dir,de->d_name);
            struct stat st;
            if(len<0 || (size_t)len>=sizeof(path) || lstat(path,&st) || !S_ISREG(st.st_mode))continue;
            if(count>=MAX_FILES){*overflow=1;break;}
            struct entry *e=&entries[count++];e->type=s->type;e->size=st.st_size;
            e->mtime=st.st_mtime;e->dev=st.st_dev;e->ino=st.st_ino;
            snprintf(e->name,sizeof(e->name),"%s",de->d_name);strcpy(e->path,path);
            snprintf(e->id,sizeof(e->id),"%s/%s",types[e->type],e->name);
        }
        closedir(dir);
    }
    if(mask&(1<<4)) {
        if(count>=MAX_FILES)*overflow=1;
        else {struct entry *e=&entries[count++];memset(e,0,sizeof(*e));e->type=4;
            strcpy(e->id,"system/dmesg.txt");strcpy(e->name,"dmesg.txt");}
    }
    return count;
}
void log_export_list(struct mg_connection *c,struct mg_http_message *hm) {
    char filter[128]={0}; int got=mg_http_get_var(&hm->query,"types",filter,sizeof(filter));
    int mask=got < -1 ? -1 : parse_types(filter);
    if(mask<0){error_reply(c,400,"未知日志类型");return;}
    struct entry *entries=calloc(MAX_FILES,sizeof(*entries));if(!entries){error_reply(c,500,"内存不足");return;}
    int overflow,count=enumerate(entries,mask,&overflow),counts[7]={0};
    struct json_object *o=json_object_new_object(),*files=json_object_new_array(),*categories=json_object_new_array();
    for(int i=0;i<count;i++) {
        struct entry *e=&entries[i];counts[e->type]++;
        struct json_object *f=json_object_new_object();char timestamp[32]="导出时采集";struct tm tm;
        if(e->mtime && localtime_r(&e->mtime,&tm))strftime(timestamp,sizeof(timestamp),"%Y-%m-%d %H:%M:%S",&tm);
        json_object_object_add(f,"id",json_object_new_string(e->id));
        json_object_object_add(f,"type",json_object_new_string(types[e->type]));
        json_object_object_add(f,"name",json_object_new_string(e->name));
        json_object_object_add(f,"size",json_object_new_int64(e->size));
        json_object_object_add(f,"mtime",json_object_new_string(timestamp));json_object_array_add(files,f);
    }
    for(int i=0;i<7;i++)if(mask&(1<<i)) {
        struct json_object *cat=json_object_new_object();
        json_object_object_add(cat,"type",json_object_new_string(types[i]));
        json_object_object_add(cat,"label",json_object_new_string(labels[i]));
        json_object_object_add(cat,"count",json_object_new_int(counts[i]));json_object_array_add(categories,cat);
    }
    json_object_object_add(o,"status",json_object_new_string("ok"));
    json_object_object_add(o,"files",files);json_object_object_add(o,"categories",categories);
    json_object_object_add(o,"truncated",json_object_new_boolean(overflow));free(entries);reply(c,200,o);
}
/* Only called on directories created by mkdtemp below EXPORT_ROOT; never follows links. */
static void remove_tree(const char *path) {
    struct stat st;if(lstat(path,&st))return;
    if(!S_ISDIR(st.st_mode)){unlink(path);return;}
    DIR *d=opendir(path);if(!d)return;struct dirent *e;
    while((e=readdir(d)))if(strcmp(e->d_name,".") && strcmp(e->d_name,"..")) {
        char child[1024];int n=snprintf(child,sizeof(child),"%s/%s",path,e->d_name);
        if(n>0 && (size_t)n<sizeof(child))remove_tree(child);
    }
    closedir(d);rmdir(path);
}
static int wait_child(pid_t p) {
    int status;if(p<0)return -1;
    while(waitpid(p,&status,0)<0)if(errno!=EINTR)return -1;
    return WIFEXITED(status) && WEXITSTATUS(status)==0 ? 0 : -1;
}
static int capture_dmesg(int fd) {
    pid_t p=fork();if(p==0) {
        struct rlimit limit={DMESG_BYTES,DMESG_BYTES};setrlimit(RLIMIT_FSIZE,&limit);
        dup2(fd,STDOUT_FILENO);int err=open("/dev/null",O_WRONLY);if(err>=0)dup2(err,STDERR_FILENO);
        execlp("dmesg","dmesg",(char *)NULL);_exit(127);
    }
    return wait_child(p);
}
static int copy_entry(const struct entry *e,const char *dest,unsigned long long *copied) {
    *copied=0;int out=open(dest,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);if(out<0)return -1;
    if(!e->path[0]) {int rc=capture_dmesg(out);struct stat st;
        if(!fstat(out,&st))*copied=st.st_size;
        close(out);if(rc)unlink(dest);return rc;}
    int in=open(e->path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat st;
    if(in<0 || fstat(in,&st) || !S_ISREG(st.st_mode) || st.st_dev!=e->dev || st.st_ino!=e->ino) {
        if(in>=0)close(in);
        close(out);unlink(dest);return -1;
    }
    char buf[32768];int rc=0;off_t remaining=e->size;
    while(remaining>0) {
        ssize_t n=read(in,buf,remaining>(off_t)sizeof(buf)?sizeof(buf):(size_t)remaining);
        if(n<0 && errno==EINTR)continue;
        if(n<0){rc=-1;break;}if(!n){rc=1;break;}
        size_t offset=0;while(offset<(size_t)n) {
            ssize_t w=write(out,buf+offset,(size_t)n-offset);
            if(w<0 && errno==EINTR)continue;
            if(w<=0){rc=-1;break;}offset+=(size_t)w;
        }
        if(rc<0)break;
        *copied+=n;remaining-=n;
    }
    close(in);if(close(out))rc=-1;if(rc<0)unlink(dest);return rc;
}
static void *build_archive(void *unused) {
    (void)unused;char snapshot[300],archive[512],manifest_path[512];
    snprintf(snapshot,sizeof(snapshot),"%s/snapshot",job.dir);
    snprintf(archive,sizeof(archive),"%s/logs.tar.gz",job.dir);
    snprintf(manifest_path,sizeof(manifest_path),"%s/manifest.json",snapshot);
    struct json_object *manifest=json_object_new_object(),*records=json_object_new_array(),*categories=json_object_new_array();
    json_object_object_add(manifest,"created_at_unix",json_object_new_int64(time(NULL)));
    json_object_object_add(manifest,"note",json_object_new_string("Bounded live snapshots; files may change while applications run. Original logs are retained."));
    int exported=0,failed=0;char failure[192]="";
    if(mkdir(snapshot,0700))snprintf(failure,sizeof(failure),"无法创建日志快照目录");
    for(int t=0;t<7;t++)if(job.mask&(1<<t)) {
        char dir[512];snprintf(dir,sizeof(dir),"%s/%s",snapshot,types[t]);
        if(!failure[0] && mkdir(dir,0700))snprintf(failure,sizeof(failure),"无法创建日志分类目录");
        int count=0;for(int i=0;i<job.count;i++)if(job.files[i].type==t)count++;
        struct json_object *cat=json_object_new_object();
        json_object_object_add(cat,"type",json_object_new_string(types[t]));
        json_object_object_add(cat,"selected_files",json_object_new_int(count));
        json_object_object_add(cat,"status",json_object_new_string(count?"selected":"no_selected_or_available_files"));
        json_object_array_add(categories,cat);
    }
    for(int i=0;i<job.count && !failure[0];i++) {
        struct entry *e=&job.files[i];char dest[1024];snprintf(dest,sizeof(dest),"%s/%s",snapshot,e->id);
        unsigned long long bytes;int rc=copy_entry(e,dest,&bytes);
        struct json_object *r=json_object_new_object();
        json_object_object_add(r,"id",json_object_new_string(e->id));
        json_object_object_add(r,"source",json_object_new_string(e->path[0]?e->path:"dmesg"));
        json_object_object_add(r,"source_bytes",json_object_new_int64(e->size));
        json_object_object_add(r,"copied_bytes",json_object_new_int64(rc<0?0:bytes));
        json_object_object_add(r,"status",json_object_new_string(rc<0?"missing_changed_or_unreadable":rc>0?"short_snapshot":"ok"));
        json_object_array_add(records,r);if(rc<0)failed++;else exported++;
        pthread_mutex_lock(&lock);job.completed=i+1;job.exported=exported;job.skipped=failed;pthread_mutex_unlock(&lock);
    }
    json_object_object_add(manifest,"categories",categories);json_object_object_add(manifest,"files",records);
    if(!failure[0] && !exported)snprintf(failure,sizeof(failure),"所选日志均无法读取，请刷新列表后重试");
    if(!failure[0] && json_object_to_file_ext(manifest_path,manifest,JSON_C_TO_STRING_PRETTY))snprintf(failure,sizeof(failure),"无法写入导出清单");
    json_object_put(manifest);
    if(!failure[0]) {
        pid_t p=fork();if(p==0){execlp("tar","tar","-czf",archive,"-C",snapshot,".",(char *)NULL);_exit(127);}
        if(wait_child(p))snprintf(failure,sizeof(failure),"日志压缩失败，请检查可用空间与tar/gzip");
    }
    remove_tree(snapshot);
    pthread_mutex_lock(&lock);free(job.files);job.files=NULL;
    snprintf(job.error,sizeof(job.error),"%s",failure);job.state=failure[0]?FAILED:READY;
    pthread_mutex_unlock(&lock);return NULL;
}
void log_export_start(struct mg_connection *c,struct mg_http_message *hm) {
    if(mg_vcmp(&hm->method,"POST")){error_reply(c,405,"请使用POST");return;}
    if(hm->body.len>192*1024){error_reply(c,413,"请求过大");return;}
    struct json_tokener *tok=json_tokener_new();struct json_object *root=json_tokener_parse_ex(tok,hm->body.ptr,(int)hm->body.len);
    int valid=json_tokener_get_error(tok)==json_tokener_success;json_tokener_free(tok);
    struct json_object *ids=NULL,*filter=NULL;
    if(!valid || !root || !json_object_object_get_ex(root,"files",&ids) || !json_object_is_type(ids,json_type_array) ||
       !json_object_object_get_ex(root,"types",&filter) || !json_object_is_type(filter,json_type_string)) {
        json_object_put(root);error_reply(c,400,"需要types字符串和files数组");return;
    }
    int mask=parse_types(json_object_get_string(filter));size_t n=json_object_array_length(ids);
    if(mask<0 || !n || n>MAX_FILES){json_object_put(root);error_reply(c,400,"类型或文件数量非法");return;}
    pthread_mutex_lock(&lock);
    if(job.state==WORKING){pthread_mutex_unlock(&lock);json_object_put(root);error_reply(c,409,"已有日志导出任务，请等待完成");return;}
    struct entry *all=calloc(MAX_FILES,sizeof(*all)),*selected=calloc(n,sizeof(*selected));
    if(!all || !selected){free(all);free(selected);pthread_mutex_unlock(&lock);json_object_put(root);error_reply(c,500,"内存不足");return;}
    int overflow,count=enumerate(all,mask,&overflow),bad=0;unsigned long long total=0;
    for(size_t i=0;i<n;i++) {
        struct json_object *v=json_object_array_get_idx(ids,i);
        if(!json_object_is_type(v,json_type_string)){bad=1;break;}
        const char *id=json_object_get_string(v);int j;
        for(j=0;j<count;j++)if(!strcmp(id,all[j].id))break;
        if(j==count){bad=1;break;}
        for(size_t k=0;k<i;k++)if(!strcmp(id,selected[k].id))bad=1;
        selected[i]=all[j];total+=all[j].path[0]?(unsigned long long)all[j].size:DMESG_BYTES;
    }
    free(all);json_object_put(root);
    struct stat rootst;struct statvfs space;
    if(bad || total>MAX_BYTES) {
        free(selected);pthread_mutex_unlock(&lock);error_reply(c,bad?400:413,bad?"文件已变化或选择非法，请刷新列表":"所选日志超过128MiB，请减少选择");return;
    }
    if((mkdir(EXPORT_ROOT,0700) && errno!=EEXIST) || lstat(EXPORT_ROOT,&rootst) || !S_ISDIR(rootst.st_mode) ||
       statvfs(EXPORT_ROOT,&space) || (unsigned long long)space.f_bavail*space.f_frsize<total*2+16*1024*1024) {
        free(selected);pthread_mutex_unlock(&lock);error_reply(c,507,"导出目录不可用或空间不足");return;
    }
    /* Discard old exports (including abandoned jobs after a Web restart).
     * Only our exact mkdtemp naming scheme is eligible, never other userdata. */
    DIR *old=opendir(EXPORT_ROOT);struct dirent *de;
    if(old) {
        while((de=readdir(old))) {
            if(strlen(de->d_name)!=13 || strncmp(de->d_name,"export-",7))continue;
            int safe=1;for(int i=7;i<13;i++)if(!isalnum((unsigned char)de->d_name[i]))safe=0;
            if(safe){char path[256];snprintf(path,sizeof(path),EXPORT_ROOT "/%.13s",de->d_name);remove_tree(path);}
        }
        closedir(old);
    }
    memset(&job,0,sizeof(job));snprintf(job.dir,sizeof(job.dir),EXPORT_ROOT "/export-XXXXXX");
    if(!mkdtemp(job.dir)){job.dir[0]=0;free(selected);pthread_mutex_unlock(&lock);error_reply(c,500,"创建任务失败");return;}
    snprintf(job.id,sizeof(job.id),"%s",strrchr(job.dir,'/')+1);job.files=selected;job.count=n;job.mask=mask;job.state=WORKING;
    char gateway[65]="gateway",stamp[32];struct json_object *cfg=json_object_from_file("/etc/config.json"),*gw=NULL;
    if(cfg && (json_object_object_get_ex(cfg,"gateway_id",&gw)||json_object_object_get_ex(cfg,"gwid",&gw))) {
        const char *v=json_object_get_string(gw);size_t i=0;if(v)for(;v[i]&&i<sizeof(gateway)-1;i++)gateway[i]=isalnum((unsigned char)v[i])?v[i]:'_';gateway[i]=0;
    }
    json_object_put(cfg);time_t now=time(NULL);struct tm tm;localtime_r(&now,&tm);strftime(stamp,sizeof(stamp),"%Y%m%d_%H%M%S",&tm);
    snprintf(job.filename,sizeof(job.filename),"%s_%s_logs.tar.gz",gateway,stamp);
    pthread_t worker;int rc=pthread_create(&worker,NULL,build_archive,NULL);
    if(rc){free(job.files);job.files=NULL;job.state=FAILED;snprintf(job.error,sizeof(job.error),"创建后台任务失败");}
    else pthread_detach(worker);
    struct json_object *o=json_object_new_object();json_object_object_add(o,"id",json_object_new_string(job.id));
    pthread_mutex_unlock(&lock);reply(c,rc?500:202,o);
}
void log_export_status(struct mg_connection *c,struct mg_http_message *hm) {
    char id[32]={0};mg_http_get_var(&hm->query,"id",id,sizeof(id));pthread_mutex_lock(&lock);
    if(!*id || strcmp(id,job.id)){pthread_mutex_unlock(&lock);error_reply(c,404,"任务不存在或已过期");return;}
    struct json_object *o=json_object_new_object();
    json_object_object_add(o,"status",json_object_new_string(job.state==WORKING?"working":job.state==READY?"ready":"error"));
    json_object_object_add(o,"completed",json_object_new_int(job.completed));json_object_object_add(o,"total",json_object_new_int(job.count));
    json_object_object_add(o,"exported",json_object_new_int(job.exported));json_object_object_add(o,"skipped",json_object_new_int(job.skipped));
    json_object_object_add(o,"message",json_object_new_string(job.error));json_object_object_add(o,"filename",json_object_new_string(job.filename));
    pthread_mutex_unlock(&lock);reply(c,200,o);
}
void log_export_download(struct mg_connection *c,struct mg_http_message *hm) {
    char id[32]={0};mg_http_get_var(&hm->query,"id",id,sizeof(id));pthread_mutex_lock(&lock);
    if(!*id || strcmp(id,job.id) || job.state!=READY){pthread_mutex_unlock(&lock);error_reply(c,404,"导出包不存在或尚未完成");return;}
    char path[512],headers[256];snprintf(path,sizeof(path),"%s/logs.tar.gz",job.dir);
    snprintf(headers,sizeof(headers),"Content-Disposition: attachment; filename=\"%s\"\r\nCache-Control: no-store\r\n",job.filename);
    struct mg_http_serve_opts opts={0};opts.extra_headers=headers;opts.mime_types="gz=application/gzip";
    mg_http_serve_file(c,hm,path,&opts);pthread_mutex_unlock(&lock);
}
