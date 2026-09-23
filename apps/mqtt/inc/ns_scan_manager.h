#ifndef NS_SCAN_MANAGER_H
#define NS_SCAN_MANAGER_H

#include <json-c/json.h>

typedef struct ns_scan_manager ns_scan_manager;
typedef int (*ns_scan_publish_fn)(void *context, const char *payload, int qos);

ns_scan_manager *ns_scan_manager_open(const char *directory, const char *socket_path,
                                      ns_scan_publish_fn publish, void *context);
int ns_scan_manager_handle(ns_scan_manager *manager, struct json_object *message);
void ns_scan_manager_tick(ns_scan_manager *manager);
void ns_scan_manager_close(ns_scan_manager *manager);

#endif
