#include "gateway_version.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char dir[] = "/tmp/gw_version_XXXXXX", path[256], process[256];
    assert(mkdtemp(dir));
    snprintf(path, sizeof(path), "%s/version.txt", dir);
    snprintf(process, sizeof(process), "%s/123", dir); assert(!mkdir(process, 0700));
    snprintf(process, sizeof(process), "%s/123/stat", dir);
    FILE *fp = fopen(process, "w"); assert(fp);
    fputs("123 (tk8710_gw) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 5000 0", fp); fclose(fp);
    const char *data[] = {
        "version=1.0.0 (RK3506)\npid=123\nstart_ticks=5000\n",
        "version=old\npid=123\nstart_ticks=4999\n",
        "version=old\npid=124\nstart_ticks=5000\n",
        "version=old\npid=123\n", "version=old\npid=-1\nstart_ticks=5000\n"
    };
    for (unsigned i=0; i<5; ++i) {
        fp=fopen(path,"w");assert(fp);fputs(data[i],fp);fclose(fp);
        struct json_object *v=gateway_version_read(path,dir);
        if(!i){assert(v);assert(!strcmp(json_object_get_string(v),"1.0.0 (RK3506)"));}
        else assert(!v);
        json_object_put(v);
    }
    unlink(path);assert(!gateway_version_read(path,dir));
    unlink(process);snprintf(process,sizeof(process),"%s/123",dir);rmdir(process);rmdir(dir);
    puts("PASS version reader: valid, stale start, absent process, incomplete, invalid PID, missing file");
    return 0;
}
