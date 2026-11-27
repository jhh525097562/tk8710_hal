/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-02 11:42:54
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-02 14:07:02
 * @FilePath: \web\inc\system_manager.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <stdint.h>

typedef struct {
    char hostname[64];
    char model[64];
    char arch[32];
    char platform[64];
    char device_coords[64];
    char firmware_version[64];
    char kernel_version[32];
    char app_version[32];
    char local_time[64];
    char uptime[64];
    uint64_t disk_total;
    uint64_t disk_used;
    uint64_t disk_free;
    uint64_t mem_total;
    uint64_t mem_used;
    uint64_t mem_free;
    double load_avg[3];
} SystemInfo;

void system_get_info(SystemInfo *info);

#endif // SYSTEM_MANAGER_H
