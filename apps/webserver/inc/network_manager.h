/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-02 11:43:24
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-02 13:37:03
 * @FilePath: \web\inc\network_manager.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char type[16]; // static or dhcp
    char ip[64];
    char mask[64];
    char gateway[64];
} NetworkConfig;

bool network_load_config(NetworkConfig *config);
bool network_save_config(const NetworkConfig *config);
bool network_ping(const char *target, char *result, size_t len);
bool wifi_load_config(char *ssid, size_t ssid_len, char *psk, size_t psk_len);
bool wifi_save_config(const char *ssid, const char *psk, char *err, size_t err_len);
bool wifi_is_connected(void);

#endif // NETWORK_MANAGER_H
