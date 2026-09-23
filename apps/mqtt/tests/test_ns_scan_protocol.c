#define _POSIX_C_SOURCE 200809L
#include "ns_scan_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int parse(const char *text, ns_scan_request *r)
{
    struct json_object *j = json_tokener_parse(text);
    const char *error;
    int ret = ns_scan_parse(j, r, &error);
    json_object_put(j);
    return ret;
}

int main(int argc, char **argv)
{
    ns_scan_request r;
    assert(parse("{\"get\":{\"config\":true}}", &r) == 0);
    assert(parse("{\"request_id\":\"../bad\",\"set\":{\"scan\":{}}}", &r) == -1);
    assert(parse("{\"request_id\":\"x\\u0000bad\",\"set\":{\"scan\":{}}}", &r) == -1);
    const char *modes[] = {"62.5k", "125k", "250k", "500k"};
    for (int i = 0; i < 4; ++i) {
        char text[256];
        snprintf(text, sizeof(text), "{\"request_id\":\"scan-1\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":470000000,\"mode\":\"%s\"}}}", modes[i]);
        assert(parse(text, &r) == 1);
    }
    ns_scan_request same = r;
    assert(ns_scan_same(&r, &same));
    same.end_hz++;
    assert(!ns_scan_same(&r, &same));
    const char *invalid[] = {
        "{\"request_id\":\"a\",\"set\":{\"scan\":{\"start_freq_hz\":\"470000000\",\"end_freq_hz\":510000000,\"mode\":\"500k\"}}}",
        "{\"request_id\":\"a\",\"set\":{\"scan\":{\"start_freq_hz\":510000000,\"end_freq_hz\":470000000,\"mode\":\"500k\"}}}",
        "{\"request_id\":\"a\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":510000000,\"mode\":\"500k\\u0000x\"}}}",
        "{\"request_id\":\"a\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":4294967295,\"mode\":\"500k\"}}}",
        "{\"request_id\":\"a\",\"set\":{\"scan\":{\"start_freq_hz\":1,\"end_freq_hz\":4000000000,\"mode\":\"62.5k\"}}}"
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(parse(invalid[i], &r) == -1);
    }
    r = same;
    r.end_hz = r.start_hz;
    char path[] = "/tmp/ns_scan_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    const char *records[] = {
        "470000000 -120 -121 -122 -123 -124 -125 -126 -127\n",
        "470000000 -120 -121\n",
        "470500000 -120 -121 -122 -123 -124 -125 -126 -127\n",
        "470000000 nan -121 -122 -123 -124 -125 -126 -127\n",
        "470000000 -120 -121 -122 -123 -124 -125 -126 -127 99\n",
        "470000000 -120 -121 -122 -123 -124 -125 -126 -127\n470000000 -120 -121 -122 -123 -124 -125 -126 -127\n"
    };
    for (int i = 0; i < 6; ++i) {
        FILE *fp = fopen(path, "w");
        assert(fp);
        fputs(records[i], fp);
        fclose(fp);
        const char *error;
        struct json_object *result = ns_scan_result(&r, path, &error);
        assert((result != NULL) == (i == 0));
        if (result) puts(json_object_to_json_string(result));
        json_object_put(result);
    }
    unlink(path);
    if (argc == 2) {
        r.start_hz = 470000000;
        r.end_hz = 510000000;
        r.step_hz = 500000;
        const char *error;
        struct json_object *result = ns_scan_result(&r, argv[1], &error), *scan, *count;
        assert(result != NULL);
        assert(json_object_object_get_ex(result, "scan_result", &scan));
        assert(json_object_object_get_ex(scan, "point_count", &count));
        assert(json_object_get_int(count) == 81);
        json_object_put(result);
        puts("PASS existing board result: 81 frequencies x 8 antennas (format validation only)");
    }
    puts("PASS scan protocol: unrelated/invalid IDs, four modes, duplicate comparison, complete/partial/wrong-frequency/NaN/extra-record results");
    return 0;
}
