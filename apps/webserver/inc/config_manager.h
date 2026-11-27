#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>

typedef struct {
    char mqtt_host[256];
    int mqtt_port;
    char username[128];
    char password[128];
    char gateway_id[64];
    char pub_topic[128];
    char sub_topic[128];
    int heartbeat_interval;
    int qos;
    char net_interface[32];
    int antenna_count;
    char web_password[64];
    
    // System Info (cached/overridden in config)
    char hostname[64];
    char model[64];
    char arch[32];
    char platform[64];
    char kernel_version[32];
    char comm_version[64];
    char firmware_version[64];
    char app_version[32];
    char build_time[64];
    char git_commit[64];
} AppConfig;

bool config_load(AppConfig *config);
bool config_save(const AppConfig *config);
bool config_sync_tdaemon_disabled(const char *disabled_ifaces);

#endif // CONFIG_MANAGER_H
