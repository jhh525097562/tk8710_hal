#define _POSIX_C_SOURCE 200809L

#include "ns_scan_protocol.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int ns_scan_id_valid(const char *id)
{
    if (id == NULL || *id == '\0' || strlen(id) > NS_SCAN_ID_MAX) {
        return 0;
    }
    for (; *id != '\0'; ++id) {
        if (!((*id >= 'A' && *id <= 'Z') || (*id >= 'a' && *id <= 'z') ||
              (*id >= '0' && *id <= '9') || *id == '_' || *id == '-')) {
            return 0;
        }
    }
    return 1;
}

static int get_hz(struct json_object *obj, const char *key, uint32_t *out)
{
    struct json_object *value;
    int64_t hz;

    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        return -1;
    }
    hz = json_object_get_int64(value);
    if (hz <= 0 || hz > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)hz;
    return 0;
}

int ns_scan_parse(struct json_object *root, ns_scan_request *r, const char **error)
{
    struct json_object *set;
    struct json_object *scan;
    struct json_object *id;
    struct json_object *mode;
    const char *text;
    const char *modes[] = {"62.5k", "125k", "250k", "500k"};
    const uint32_t steps[] = {62500, 125000, 250000, 500000};

    *error = "invalid_request";
    memset(r, 0, sizeof(*r));
    if (!json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "set", &set) ||
        !json_object_is_type(set, json_type_object) ||
        !json_object_object_get_ex(set, "scan", &scan)) {
        return 0;
    }
    if (!json_object_object_get_ex(root, "request_id", &id) ||
        !json_object_is_type(id, json_type_string)) {
        return -1;
    }
    text = json_object_get_string(id);
    if (strlen(text) != (size_t)json_object_get_string_len(id) || !ns_scan_id_valid(text)) {
        return -1;
    }
    strcpy(r->request_id, text);
    if (!json_object_is_type(scan, json_type_object) ||
        get_hz(scan, "start_freq_hz", &r->start_hz) ||
        get_hz(scan, "end_freq_hz", &r->end_hz) ||
        r->start_hz > r->end_hz) {
        *error = "invalid_frequency";
        return -1;
    }
    if (!json_object_object_get_ex(scan, "mode", &mode) ||
        !json_object_is_type(mode, json_type_string)) {
        *error = "invalid_mode";
        return -1;
    }
    text = json_object_get_string(mode);
    for (unsigned i = 0; i < 4; ++i) {
        if ((size_t)json_object_get_string_len(mode) == strlen(modes[i]) && strcmp(text, modes[i]) == 0) {
            strcpy(r->mode, text);
            r->step_hz = steps[i];
            break;
        }
    }
    if (r->step_hz == 0) {
        *error = "invalid_mode";
        return -1;
    }
    if (r->end_hz > UINT32_MAX - r->step_hz ||
        (r->end_hz - r->start_hz) / r->step_hz + 1 > NS_SCAN_MAX_POINTS) {
        *error = "scan_too_large";
        return -1;
    }
    *error = NULL;
    return 1;
}

int ns_scan_same(const ns_scan_request *a, const ns_scan_request *b)
{
    return strcmp(a->request_id, b->request_id) == 0 && a->start_hz == b->start_hz &&
           a->end_hz == b->end_hz && a->step_hz == b->step_hz;
}

struct json_object *ns_scan_result(const ns_scan_request *r, const char *path, const char **error)
{
    struct stat st;
    FILE *fp = NULL;
    struct json_object *root = NULL;
    struct json_object *result = NULL;
    struct json_object *points = NULL;
    uint32_t expected;
    char token[128];
    uint32_t count = 0;

    *error = "invalid_result";
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > NS_SCAN_MAX_JSON) {
        return NULL;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return NULL;
    }
    root = json_object_new_object();
    result = json_object_new_object();
    points = json_object_new_array();
    if (root == NULL || result == NULL || points == NULL) {
        goto fail;
    }
    if (r->step_hz == 0 || r->end_hz < r->start_hz) {
        goto fail;
    }
    expected = (r->end_hz - r->start_hz) / r->step_hz + 1;
    if (expected > NS_SCAN_MAX_POINTS) {
        goto fail;
    }
    while (fscanf(fp, "%127s", token) == 1) {
        char *end;
        unsigned long long frequency;
        struct json_object *point;
        struct json_object *noise;

        if (*token < '0' || *token > '9') {
            goto fail;
        }
        errno = 0;
        frequency = strtoull(token, &end, 10);
        if (errno || *end != '\0' || count >= expected ||
            frequency != (uint64_t)r->start_hz + (uint64_t)count * r->step_hz) {
            goto fail;
        }
        point = json_object_new_object();
        noise = json_object_new_array();
        if (point == NULL || noise == NULL) {
            json_object_put(point);
            json_object_put(noise);
            goto fail;
        }
        json_object_object_add(point, "freq_hz", json_object_new_int64(frequency));
        json_object_object_add(point, "noise_dbm", noise);
        json_object_array_add(points, point);
        for (int ant = 0; ant < 8; ++ant) {
            double value;

            if (fscanf(fp, "%127s", token) != 1) {
                goto fail;
            }
            errno = 0;
            value = strtod(token, &end);
            if (errno || end == token || *end != '\0' || !isfinite(value)) {
                goto fail;
            }
            json_object_array_add(noise, json_object_new_double(value));
        }
        ++count;
    }
    if (ferror(fp) || count != expected) {
        goto fail;
    }
    fclose(fp);
    fp = NULL;
    json_object_object_add(result, "request_id", json_object_new_string(r->request_id));
    json_object_object_add(result, "state", json_object_new_string("completed"));
    json_object_object_add(result, "start_freq_hz", json_object_new_int64(r->start_hz));
    json_object_object_add(result, "end_freq_hz", json_object_new_int64(r->end_hz));
    json_object_object_add(result, "mode", json_object_new_string(r->mode));
    json_object_object_add(result, "point_count", json_object_new_int(count));
    json_object_object_add(result, "antenna_count", json_object_new_int(8));
    json_object_object_add(result, "points", points);
    points = NULL;
    json_object_object_add(root, "scan_result", result);
    result = NULL;
    if (strlen(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN)) > NS_SCAN_MAX_JSON) {
        *error = "result_too_large";
        goto fail;
    }
    *error = NULL;
    return root;

fail:
    if (fp != NULL) {
        fclose(fp);
    }
    json_object_put(points);
    json_object_put(result);
    json_object_put(root);
    return NULL;
}
