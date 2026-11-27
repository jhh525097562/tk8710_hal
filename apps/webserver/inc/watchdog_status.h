#ifndef RK_WATCHDOG_STATUS_H
#define RK_WATCHDOG_STATUS_H
#include <json-c/json.h>
struct json_object *watchdog_status_read(void);
#endif
