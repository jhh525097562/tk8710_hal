#define _POSIX_C_SOURCE 200809L
#define NS_SCAN_ARCHIVE_LIMIT 4
#include "../src/ns_scan_manager.c"
#include <assert.h>
#include <utime.h>

static char reply[2048];
static int publish(void *ctx, const char *data, int qos)
{
    (void)ctx; assert(qos == 1);
    snprintf(reply, sizeof(reply), "%s", data); return 0;
}
static void request(ns_scan_manager *m, int id, int conflict)
{
    char text[256];
    snprintf(text, sizeof(text), "{\"request_id\":\"test%d\",\"set\":{\"scan\":{"
        "\"start_freq_hz\":470000000,\"end_freq_hz\":%d,\"mode\":\"500k\"}}}",
        id, conflict ? 471000000 : 470000000);
    struct json_object *o = json_tokener_parse(text);
    assert(ns_scan_manager_handle(m, o) == 1); json_object_put(o);
}
static void ack(ns_scan_manager *m, int id)
{
    char text[128];
    snprintf(text,sizeof(text),"{\"scan_result_ack\":{\"request_id\":\"test%d\"}}",id);
    struct json_object *o = json_tokener_parse(text);
    assert(ns_scan_manager_handle(m,o)==1); json_object_put(o);
}
static void complete(ns_scan_manager *m, int id, int confirm)
{
    scan_job *j = &m->jobs[m->count-1];
    finish(m,j,NULL,"test_failure"); /* Failed results also require a business ACK. */
    if(confirm) ack(m,id);
}
static int archives(const char *dir)
{
    DIR *d=opendir(dir);assert(d);struct dirent *e;int n=0;
    while((e=readdir(d))) if(!strncmp(e->d_name,"ack-",4)) ++n;
    closedir(d);return n;
}
int main(void)
{
    char dir[]="/tmp/scan_history_XXXXXX";assert(mkdtemp(dir));
    ns_scan_manager *m=ns_scan_manager_open(dir,"/tmp/not_used",publish,NULL);assert(m);
    for(int i=0;i<32;++i){request(m,i,0);assert(strstr(reply,"accepted"));complete(m,i,1);}
    /* Simulate a crash after archive fsync, before removing the active job. */
    assert(save_record(m,&m->jobs[0],"ack")==0);
    ns_scan_manager_close(m);
    m=ns_scan_manager_open(dir,"/tmp/not_used",publish,NULL);assert(m && m->count==32);
    assert(archives(dir)==0);
    request(m,32,0);assert(strstr(reply,"accepted") && m->count==32);
    request(m,0,0);assert(strstr(reply,"acknowledged"));
    request(m,0,1);assert(strstr(reply,"request_id_conflict"));
    complete(m,32,1);
    ns_scan_manager_close(m);
    m=ns_scan_manager_open(dir,"/tmp/not_used",publish,NULL);assert(m);
    request(m,0,0);assert(strstr(reply,"acknowledged"));
    /* Continuous confirmed traffic never exhausts the 32-slot pool. */
    for(int i=33;i<100;++i){request(m,i,0);assert(strstr(reply,"accepted"));complete(m,i,1);assert(archives(dir)<=4);}
    /* Test TTL expiration and paired executor cleanup, not just count eviction. */
    assert(prune_archives(m,4)==0); /* Empty compact test archive capacity. */
    assert(save_record(m,&m->jobs[0],"ack")==0);
    char id[49];strcpy(id,m->jobs[0].request.request_id);
    char path[384];job_path(m,path,sizeof(path),"job",id,"json");assert(!unlink(path));
    json_object_put(m->jobs[0].record);memmove(m->jobs,m->jobs+1,(--m->count)*sizeof(scan_job));
    job_path(m,path,sizeof(path),"exec",id,"json");
    FILE *f=fopen(path,"w");assert(f);fprintf(f,"{\"request_id\":\"%s\",\"state\":\"running\"}",id);fclose(f);
    char archived[384];job_path(m,archived,sizeof(archived),"ack",id,"json");
    struct utimbuf old={time(NULL)-NS_SCAN_ARCHIVE_TTL-1,time(NULL)-NS_SCAN_ARCHIVE_TTL-1};assert(!utime(archived,&old));
    assert(prune_archives(m,0)==-1 && access(archived,F_OK)==0);
    f=fopen(path,"w");assert(f);fprintf(f,"{\"request_id\":\"%s\",\"state\":\"done\"}",id);fclose(f);
    assert(!prune_archives(m,0));assert(access(archived,F_OK)!=0 && access(path,F_OK)!=0);
    ns_scan_manager_close(m);

    char unacked[]="/tmp/scan_unacked_XXXXXX";assert(mkdtemp(unacked));
    m=ns_scan_manager_open(unacked,"/tmp/not_used",publish,NULL);assert(m);
    for(int i=0;i<32;++i){request(m,i,0);assert(strstr(reply,"accepted"));complete(m,i,0);}
    request(m,32,0);assert(strstr(reply,"history_full"));
    ack(m,0);
    /* Filesystem failure must retain all active records. */
    char blocked[384];job_path(m,blocked,sizeof(blocked),"ack","test0","json");assert(!mkdir(blocked,0700));
    request(m,32,0);assert(strstr(reply,"storage_failed") && m->count==32);
    assert(!rmdir(blocked));request(m,32,0);assert(strstr(reply,"accepted"));
    assert(!strcmp(string_field(m->jobs[0].record,"phase"),"ready"));
    ns_scan_manager_close(m);
    puts("PASS history: legacy32,100 requests,durable dedup,conflict,cap,TTL,executor safety,unacked protection,storage failure");
    return 0;
}
