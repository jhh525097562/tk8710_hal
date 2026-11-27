/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-02-28 15:46:22
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-02-28 15:46:24
 * @FilePath: \mqtt\inc\config.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Network interface types
typedef enum {
    NET_IF_UNKNOWN = 0,
    NET_IF_4G,
    NET_IF_ETHERNET,
    NET_IF_WIFI
} NetworkInterfaceType;

// Application configuration structure
typedef struct {
    char mqtt_host[256];
    int mqtt_port;
    char mqtt_username[128];
    char mqtt_password[128];
    char gateway_id[64];
    char pub_topic[128];
    char sub_topic[128];
    char kernel_version[64];
    char firmware_version[64];
    char app_version[64];
    int heartbeat_interval; // in seconds
    int qos; // MQTT QoS level
    NetworkInterfaceType net_interface;
} AppConfig;

/**
 * @brief Load configuration from a JSON file.
 * 
 * @param filename Path to the JSON configuration file.
 * @param config Pointer to the AppConfig structure to populate.
 * @return 0 on success, -1 on failure.
 */
int load_config(const char *filename, AppConfig *config);

/**
 * @brief Get the string representation of the network interface.
 * 
 * @param type Network interface type.
 * @return String representation.
 */
const char* get_net_interface_name(NetworkInterfaceType type);

#endif // CONFIG_H
