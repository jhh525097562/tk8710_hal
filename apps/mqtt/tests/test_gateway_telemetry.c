#define _POSIX_C_SOURCE 200809L
#include "gateway_telemetry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char path[] = "/tmp/mqtt_rfstatus_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    const char *inputs[] = {
        "rfstatus=11111111\n", "rfstatus=11111110\n", "rfstatus=01111111\r\n",
        "rfstatus=00000000", "rfstatus=1111111\n", "rfstatus=111111111\n",
        "rfstatus=11111x11\n", "", "status=11111111\n",
        "rfstatus=11111111\nrfstatus=00000000\n"
    };
    const char *expected[] = {"11111111", "11111110", "01111111", "00000000"};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); ++i) {
        FILE *fp = fopen(path, "wb"); assert(fp);
        assert(fwrite(inputs[i], 1, strlen(inputs[i]), fp) == strlen(inputs[i]));
        fclose(fp);
        struct json_object *rf = telemetry_rf_status(path);
        if (i < 4) {
            assert(rf && json_object_is_type(rf, json_type_string));
            assert(!strcmp(json_object_get_string(rf), expected[i]));
        } else assert(rf == NULL);
        json_object_put(rf);
    }
    FILE *fp = fopen(path, "wb"); assert(fp);
    const char embedded[] = "rfstatus=11111111\0hidden";
    fwrite(embedded, 1, sizeof(embedded)-1, fp); fclose(fp);
    assert(telemetry_rf_status(path) == NULL);
    unlink(path);
    assert(telemetry_rf_status(path) == NULL);
    puts("PASS RF status: 12 cases including leading zero, antenna order, missing and malformed data");
    if (argc == 2 && strcmp(argv[1], "--rf-only") == 0) return 0;
    uint64_t ticks = 0;
    assert(telemetry_parse_starttime("42 (a name ) nested) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 5000 0", &ticks) == 0);
    assert(ticks == 5000);
    assert(telemetry_parse_starttime("42 bad", &ticks) != 0);
    assert(telemetry_parse_starttime("42 (bad) S 1", &ticks) != 0);
    struct json_object *obj = telemetry_process("/does-not-exist", "tk8710_gw", 10, 100), *value;
    assert(json_object_object_get_ex(obj, "running", &value) && value == NULL);
    assert(!json_object_object_get_ex(obj, "pid", &value));
    json_object_put(obj);
    obj = telemetry_process("/proc", "unlikely_missing_process_913", 10000000, 100);
    assert(json_object_object_get_ex(obj, "running", &value) && value != NULL && !json_object_get_boolean(value));
    json_object_put(obj);
    obj = json_object_new_object();
    telemetry_add(obj);
    struct json_object *system;
    assert(json_object_object_get_ex(obj, "system", &system));
    assert(json_object_object_get_ex(system, "ram_total_bytes", &value));
    assert(json_object_get_int64(value) > 0);
    puts(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
    json_object_put(obj);
    puts("PASS telemetry parser, missing/unknown process, system collection");
    return 0;
}
