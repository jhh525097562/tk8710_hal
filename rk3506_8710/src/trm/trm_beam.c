/**
 * @file trm_beam.c
 * @brief TRM波束管理功能实现
 */

#include "trm_beam.h"
#include "../inc/trm/trm_log.h"
#include "../port/tk8710_hal.h"
#include "../inc/driver/tk8710_platform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 外部函数声明 */
extern void TK8710EnterCritical(void);
extern void TK8710ExitCritical(void);
extern uint64_t TK8710GetTimeUs(void);

/*==============================================================================
 * 私有定义
 *============================================================================*/

#if defined(PLATFORM_TMS570)
#define BEAM_TABLE_SIZE     512U
#define BEAM_NODE_POOL_SIZE 512U
#else
#define BEAM_TABLE_SIZE     4096U
#endif

/* 波束条目 */
typedef struct BeamEntry {
    uint32_t userId;
    TRM_BeamInfo beamInfo;
    struct BeamEntry* next;
} BeamEntry;

/* 波束表 */
typedef struct {
    BeamEntry* table[BEAM_TABLE_SIZE];
    uint32_t count;
    uint32_t maxUsers;
    uint32_t timeoutMs;
} BeamTable;

/*==============================================================================
 * 私有变量
 *============================================================================*/

static BeamTable g_beamTable;
#if defined(PLATFORM_TMS570)
static BeamEntry g_beamPool[BEAM_NODE_POOL_SIZE];
static BeamEntry* g_beamFreeList;
#endif

/*==============================================================================
 * 私有函数
 *============================================================================*/

static uint32_t BeamHashFunc(uint32_t userId)
{
    /* 使用黄金比例哈希函数，减少倍数冲突
     * 2654435761 是 2^32 / φ 的整数部分，其中 φ ≈ 1.618033988749895
     * 这个常数能提供良好的哈希分布特性
     */
    return (userId * 2654435761U) % BEAM_TABLE_SIZE;
}

static BeamEntry* BeamAllocEntry(void)
{
#if defined(PLATFORM_TMS570)
    BeamEntry* entry = g_beamFreeList;
    if (entry != NULL) {
        g_beamFreeList = entry->next;
        entry->next = NULL;
    }
    return entry;
#else
    return (BeamEntry*)TK8710_MALLOC(sizeof(BeamEntry));
#endif
}

static void BeamFreeEntry(BeamEntry* entry)
{
    if (entry == NULL) {
        return;
    }
#if defined(PLATFORM_TMS570)
    (void)memset(entry, 0, sizeof(*entry));
    entry->next = g_beamFreeList;
    g_beamFreeList = entry;
#else
    TK8710_FREE(entry);
#endif
}

static BeamEntry* BeamFindEntry(uint32_t userId)
{
    uint32_t index = BeamHashFunc(userId);
    TRM_LOG_DEBUG("TRM: BeamFindEntry - userId=0x%08X, index=%u", userId, index);

    BeamEntry* entry = g_beamTable.table[index];
    TRM_LOG_DEBUG("TRM: Table entry at index %u is %p", index, entry);

    /* 检查表项是否已初始化 */
    if (entry == NULL) {
        TRM_LOG_DEBUG("TRM: Table entry is NULL");
        return NULL;
    }

    int count = 0;
    while (entry != NULL && count < 10) { /* 防止无限循环 */
        TRM_LOG_DEBUG("TRM: Checking entry %d - userId=0x%08X", count, entry->userId);
        if (entry->userId == userId) {
            TRM_LOG_DEBUG("TRM: Found matching entry");
            return entry;
        }
        entry = entry->next;
        count++;
    }

    TRM_LOG_DEBUG("TRM: No matching entry found");
    return NULL;
}

/*==============================================================================
 * 公共函数实现
 *============================================================================*/

/*==============================================================================
 * 公共接口实现
 *============================================================================*/

int TRM_SetBeamInfo(uint32_t userId, const TRM_BeamInfo* beamInfo)
{
    if (beamInfo == NULL) {
        TRM_LOG_ERROR("TRM: TRM_SetBeamInfo - beamInfo is NULL");
        return TRM_ERR_PARAM;
    }

    TRM_LOG_DEBUG("TRM: TRM_SetBeamInfo - userId=0x%08X, valid=%d", userId, beamInfo->valid);

    TK8710EnterCritical();

    /* 查找是否已存在 */
    BeamEntry* entry = BeamFindEntry(userId);

    if (entry != NULL) {
        /* 更新 */
        TRM_LOG_DEBUG("TRM: Updating existing beam entry for user ID=0x%08X", userId);
        memcpy(&entry->beamInfo, beamInfo, sizeof(TRM_BeamInfo));
        entry->beamInfo.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
        entry->beamInfo.valid = 1;
    } else {
        /* 检查是否超过最大用户数 */
        if (g_beamTable.count >= g_beamTable.maxUsers) {
            TK8710ExitCritical();
            TRM_LOG_ERROR("TRM: Beam table full, count=%u, maxUsers=%u", g_beamTable.count, g_beamTable.maxUsers);
            return TRM_ERR_NO_MEM;
        }

        /* 新建 */
        TRM_LOG_DEBUG("TRM: Creating new beam entry for user ID=0x%08X", userId);
        entry = BeamAllocEntry();
        if (entry == NULL) {
            TK8710ExitCritical();
            TRM_LOG_ERROR("TRM: Failed to allocate memory for beam entry");
            return TRM_ERR_NO_MEM;
        }

        entry->userId = userId;
        memcpy(&entry->beamInfo, beamInfo, sizeof(TRM_BeamInfo));
        entry->beamInfo.userId = userId;
        entry->beamInfo.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
        entry->beamInfo.valid = 1;

        /* 插入哈希表 */
        uint32_t index = BeamHashFunc(userId);
        TRM_LOG_DEBUG("TRM: Inserting beam entry at index %u", index);
        entry->next = g_beamTable.table[index];
        g_beamTable.table[index] = entry;
        g_beamTable.count++;

        TRM_LOG_DEBUG("TRM: Beam entry inserted, table count=%u", g_beamTable.count);
    }

    TK8710ExitCritical();

    TRM_LOG_DEBUG("TRM: TRM_SetBeamInfo completed successfully for user ID=0x%08X", userId);
    return TRM_OK;
}

int TRM_GetBeamInfo(uint32_t userId, TRM_BeamInfo* beamInfo)
{
    if (beamInfo == NULL) {
        return TRM_ERR_PARAM;
    }

    // TRM_LOG_DEBUG("TRM: Starting beam info query for user ID=0x%08X", userId);

    /* 使用读写分离：只读操作不需要锁 */
    uint32_t index = BeamHashFunc(userId);
    // TRM_LOG_DEBUG("TRM: BeamFindEntry - userId=0x%08X, index=%u", userId, index);

    /* 读取操作 - 不加锁，避免死锁 */
    BeamEntry* entry = g_beamTable.table[index];
    // TRM_LOG_DEBUG("TRM: Table entry at index %u is %p", index, entry);

    /* 检查表项是否已初始化 */
    if (entry == NULL) {
        TRM_LOG_DEBUG("TRM: Table entry is NULL");
        return TRM_ERR_NO_BEAM;
    }

    /* 简化遍历逻辑 */
    int count = 0;
    BeamEntry* current = entry;
    while (current != NULL && count < 10) { /* 防止无限循环 */
        // TRM_LOG_DEBUG("TRM: Checking entry %d - userId=0x%08X", count, current->userId);
        if (current->userId == userId) {
            // TRM_LOG_DEBUG("TRM: Found matching entry");

            /* 检查有效性 */
            if (!current->beamInfo.valid) {
                TRM_LOG_DEBUG("TRM: Beam entry exists but not valid");
                return TRM_ERR_NO_BEAM;
            }

            /* 检查超时 */
            uint32_t now = (uint32_t)(TK8710GetTimeUs() / 1000);
            // TRM_LOG_DEBUG("TRM: Current time=%u, beam timestamp=%u, timeout=%u",
                        //   now, current->beamInfo.timestamp, g_beamTable.timeoutMs);

            if (g_beamTable.timeoutMs > 0 &&
                (now - current->beamInfo.timestamp) > g_beamTable.timeoutMs) {
                /* 标记过期但不修改，避免并发问题 */
                // TRM_LOG_DEBUG("TRM: Beam info expired for user ID=0x%08X", userId);
                return TRM_ERR_NO_BEAM;
            }

            /* 逐字段复制，避免memcpy问题 */
            beamInfo->userId = current->beamInfo.userId;
            beamInfo->freq = current->beamInfo.freq;
            beamInfo->pilotPower = current->beamInfo.pilotPower;
            beamInfo->valid = current->beamInfo.valid;
            beamInfo->timestamp = current->beamInfo.timestamp;
            memcpy(beamInfo->ahData, current->beamInfo.ahData, sizeof(beamInfo->ahData));

            // TRM_LOG_DEBUG("TRM: Found beam info for user ID=0x%08X", userId);
            return TRM_OK;
        }
        current = current->next;
        count++;
    }

    TRM_LOG_DEBUG("TRM: No matching entry found");
    return TRM_ERR_NO_BEAM;
}

int TRM_TouchBeamInfoNoLock(uint32_t userId)
{
    BeamEntry* entry = BeamFindEntry(userId);

    if (entry == NULL || !entry->beamInfo.valid) {
        return TRM_ERR_NO_BEAM;
    }

    entry->beamInfo.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000U);
    return TRM_OK;
}

int TRM_ClearBeamInfo(uint32_t userId)
{
    TK8710EnterCritical();

    if (userId == 0xFFFFFFFF) {
        /* 清除所有*/
        for (uint32_t i = 0; i < BEAM_TABLE_SIZE; i++) {
            BeamEntry* entry = g_beamTable.table[i];
            while (entry != NULL) {
                BeamEntry* next = entry->next;
                BeamFreeEntry(entry);
                entry = next;
            }
            g_beamTable.table[i] = NULL;
        }
        g_beamTable.count = 0;
    } else {
        /* 清除指定用户 */
        uint32_t index = BeamHashFunc(userId);
        BeamEntry* entry = g_beamTable.table[index];
        BeamEntry* prev = NULL;

        while (entry != NULL) {
            if (entry->userId == userId) {
                if (prev == NULL) {
                    g_beamTable.table[index] = entry->next;
                } else {
                    prev->next = entry->next;
                }
                BeamFreeEntry(entry);
                g_beamTable.count--;
                break;
            }
            prev = entry;
            entry = entry->next;
        }
    }

    TK8710ExitCritical();

    return TRM_OK;
}

int TRM_SetBeamTimeout(uint32_t timeoutMs)
{
    g_beamTable.timeoutMs = timeoutMs;
    return TRM_OK;
}

/* 内部初始化函�?*/
void TRM_BeamInit(uint32_t maxUsers, uint32_t timeoutMs)
{
    memset(&g_beamTable, 0, sizeof(g_beamTable));
#if defined(PLATFORM_TMS570)
    {
        uint32_t i;
        (void)memset(g_beamPool, 0, sizeof(g_beamPool));
        for (i = 0U; i + 1U < BEAM_NODE_POOL_SIZE; i++) {
            g_beamPool[i].next = &g_beamPool[i + 1U];
        }
        g_beamPool[BEAM_NODE_POOL_SIZE - 1U].next = NULL;
        g_beamFreeList = &g_beamPool[0];
        if (maxUsers > BEAM_NODE_POOL_SIZE) {
            maxUsers = BEAM_NODE_POOL_SIZE;
        }
    }
#endif
    g_beamTable.maxUsers = maxUsers;
    g_beamTable.timeoutMs = timeoutMs;
}

/* 内部反初始化函数 */
void TRM_BeamDeinit(void)
{
    TRM_ClearBeamInfo(0xFFFFFFFF);
}
