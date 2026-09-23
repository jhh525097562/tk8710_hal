#ifndef GATEWAY_TELEMETRY_H
#define GATEWAY_TELEMETRY_H

#include <stdint.h>

#include <json-c/json.h>

int telemetry_parse_starttime(const char *stat, uint64_t *ticks);
struct json_object *telemetry_process(const char *proc_root, const char *name,
                                      double uptime_s, long ticks_per_second);
void telemetry_add(struct json_object *alive);
/* Eight binary digits, antenna 7 to 0; NULL denotes unavailable/invalid data. */
struct json_object *telemetry_rf_status(const char *path);

#endif
