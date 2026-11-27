#ifndef GATEWAY_VERSION_H
#define GATEWAY_VERSION_H
#include <json-c/json.h>
/* NULL means no version belonging to a currently running gateway. */
struct json_object *gateway_version_read(const char *path, const char *proc_root);
#endif
