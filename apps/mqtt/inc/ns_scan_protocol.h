#ifndef NS_SCAN_PROTOCOL_H
#define NS_SCAN_PROTOCOL_H

#include <stdint.h>

#include <json-c/json.h>

#define NS_SCAN_ID_MAX 48
#define NS_SCAN_MAX_POINTS 4096
#define NS_SCAN_MAX_JSON (1024 * 1024)

typedef struct {
    char request_id[NS_SCAN_ID_MAX + 1];
    uint32_t start_hz;
    uint32_t end_hz;
    uint32_t step_hz;
    char mode[8];
} ns_scan_request;

int ns_scan_parse(struct json_object *root, ns_scan_request *request, const char **error);
int ns_scan_id_valid(const char *id);
int ns_scan_same(const ns_scan_request *a, const ns_scan_request *b);
struct json_object *ns_scan_result(const ns_scan_request *request, const char *path, const char **error);

#endif
