#include "watchdog_status.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#ifndef WDT_STATUS_FILE
#define WDT_STATUS_FILE "/run/rk-watchdog/status.json"
#endif

struct json_object *watchdog_status_read(void)
{
    struct json_object *root = json_object_from_file(WDT_STATUS_FILE);
    struct json_object *state = NULL, *pid = NULL, *updated = NULL, *boot = NULL;
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "state", &state) || !json_object_is_type(state, json_type_string)) {
        if (root) json_object_put(root);
        root = json_object_new_object();
        json_object_object_add(root, "state", json_object_new_string("unavailable"));
        json_object_object_add(root, "reason", json_object_new_string("status_missing"));
        return root;
    }
    char current_boot[64] = {0};
    FILE *f = fopen("/proc/sys/kernel/random/boot_id", "r");
    if (f) { if (!fgets(current_boot, sizeof(current_boot), f)) current_boot[0] = 0; fclose(f); }
    current_boot[strcspn(current_boot, "\r\n")] = 0;
    int valid_boot = json_object_object_get_ex(root, "boot_id", &boot) &&
        json_object_is_type(boot, json_type_string) && current_boot[0] &&
        !strcmp(current_boot, json_object_get_string(boot));
    const char *value = json_object_get_string(state);
    if (!valid_boot) {
        json_object_object_add(root, "state", json_object_new_string("unavailable"));
        json_object_object_add(root, "reason", json_object_new_string("boot_mismatch"));
    } else if (!strcmp(value, "feeding") || !strcmp(value, "degraded") || !strcmp(value, "waiting")) {
        int alive = 0;
        if (json_object_object_get_ex(root, "pid", &pid) && json_object_is_type(pid, json_type_int)) {
            long id = json_object_get_int64(pid);
            if (id > 1 && id < 4194304 && kill((pid_t)id, 0) == 0) {
                char path[80], comm[32] = {0};
                snprintf(path, sizeof(path), "/proc/%ld/comm", id);
                f = fopen(path, "r");
                if (f) { alive = fgets(comm, sizeof(comm), f) && !strcmp(comm, "rk_wdt\n"); fclose(f); }
            }
        }
        struct timespec ts;
        double age = -1;
        if (!clock_gettime(CLOCK_BOOTTIME, &ts) && json_object_object_get_ex(root, "updated_uptime_s", &updated))
            age = ts.tv_sec + ts.tv_nsec / 1e9 - json_object_get_double(updated);
        if (!alive || !isfinite(age) || age < 0 || age > 20) {
            json_object_object_add(root, "state", json_object_new_string("stale"));
            json_object_object_add(root, "reason", json_object_new_string("service_not_confirmed"));
        }
    }
    return root;
}
