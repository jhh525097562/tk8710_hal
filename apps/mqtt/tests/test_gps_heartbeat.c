#define _POSIX_C_SOURCE 200809L
#include "gps_info.h"
#include "mac_msg_parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

static void check(const char *text, int64_t expected_count, int64_t expected_snr)
{
    char path[] = "/tmp/gps-heartbeat-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "w");
    assert(fp && fputs(text, fp) >= 0 && fclose(fp) == 0);
    GpsInfo gps;
    assert(gps_info_load(path, &gps) == 0);
    unlink(path);
    assert(gps.count == expected_count && gps.avgsnr == expected_snr);
    assert(gps.status == 18 && !strcmp(gps.lon, "114.537037"));
    GwAliveUp_t msg = {.msg_type = MSG_TYPE_GW_ALIVE_UP, .uptime = 123, .power = 1};
    strcpy(msg.ver, "test"); strcpy(msg.net, "eth0");
    char *json = pack_gw_alive_up_msg_with_gps_metrics(&msg, gps.lon, gps.lat,
        gps.status, gps.count, gps.avgsnr);
    assert(json);
    struct json_object *root = json_tokener_parse(json), *alive, *value;
    assert(root && json_object_object_get_ex(root, "alive", &alive));
    assert(json_object_object_get_ex(alive, "status", &value) && json_object_get_int64(value) == 18);
    assert(json_object_object_get_ex(alive, "count", &value));
    if (expected_count < 0) assert(value == NULL);
    else assert(json_object_is_type(value, json_type_int) && json_object_get_int64(value) == expected_count);
    assert(json_object_object_get_ex(alive, "avgsnr", &value));
    if (expected_snr < 0) assert(value == NULL);
    else assert(json_object_is_type(value, json_type_int) && json_object_get_int64(value) == expected_snr);
    assert(json_object_object_get_ex(alive, "system", &value));
    json_object_put(root); free_json_str(json);
}

static void check_disabled(void)
{
    char path[] = "/tmp/gps-disabled-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *fp = fdopen(fd, "w");
    assert(fp);
    assert(fputs("lon=unknown\nlat=unknown\nstatus=0\ncount=null\navgsnr=null\n", fp) >= 0);
    assert(fclose(fp) == 0);
    GpsInfo gps;
    assert(gps_info_load(path, &gps) == 0);
    unlink(path);
    assert(!strcmp(gps.lon, "unknown") && !strcmp(gps.lat, "unknown"));
    assert(gps.status == 0 && gps.count == -1 && gps.avgsnr == -1);
    GwAliveUp_t msg = {.msg_type = MSG_TYPE_GW_ALIVE_UP};
    char *json = pack_gw_alive_up_msg_with_gps_metrics(&msg, gps.lon, gps.lat,
        gps.status, gps.count, gps.avgsnr);
    assert(json);
    struct json_object *root = json_tokener_parse(json), *alive, *value;
    assert(root && json_object_object_get_ex(root, "alive", &alive));
    assert(json_object_object_get_ex(alive, "lon", &value));
    assert(!strcmp(json_object_get_string(value), "unknown"));
    assert(json_object_object_get_ex(alive, "lat", &value));
    assert(!strcmp(json_object_get_string(value), "unknown"));
    assert(json_object_object_get_ex(alive, "status", &value));
    assert(json_object_is_type(value, json_type_int) && json_object_get_int64(value) == 0);
    assert(json_object_object_get_ex(alive, "count", &value) && value == NULL);
    assert(json_object_object_get_ex(alive, "avgsnr", &value) && value == NULL);
    json_object_put(root); free_json_str(json);
}

int main(void)
{
    check_disabled();
    const char *base = "lon=114.537037\nlat=30.492981\nstatus=18\n";
    char text[512];
    snprintf(text, sizeof(text), "%scount=16\navgsnr=31\n", base); check(text,16,31);
    snprintf(text, sizeof(text), "%scount=0\r\navgsnr=0\r\n", base); check(text,0,0);
    check(base,-1,-1);
    snprintf(text, sizeof(text), "%scount=\navgsnr=\n", base); check(text,-1,-1);
    snprintf(text, sizeof(text), "%scount=-1\navgsnr=nan\n", base); check(text,-1,-1);
    snprintf(text, sizeof(text), "%scount=4294967296\navgsnr=31junk\n", base); check(text,-1,-1);
    snprintf(text, sizeof(text), "%scount=12\navgsnr=18446744073709551616\n", base); check(text,12,-1);
    GpsInfo info;
    assert(gps_info_load("/does-not-exist", &info) != 0);
    assert(!strcmp(info.lon,"unknown") && info.status == 0 && info.count == -1 && info.avgsnr == -1);
    puts("PASS GPS file -> heartbeat: new/legacy/zero/CRLF/missing/invalid/overflow");
    return 0;
}
