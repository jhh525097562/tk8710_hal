/**
 * @file trm_data.c
 * @brief TRM数据管理功能实现
 */

#include "../inc/trm/trm_api.h"
#include "../inc/trm/trm_internal.h"
#include "../inc/trm/trm_log.h"
#include "../inc/trm/trm_beam.h"
#include "../inc/trm/trm_satellite.h"
#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../port/tk8710_hal.h"
#include "../inc/trm/trm_mac_parser.h"
#include "../inc/driver/tk8710_platform.h"
#include <string.h>
#include <stdio.h>

/* 外部函数声明 */
extern void TK8710EnterCritical(void);
extern void TK8710ExitCritical(void);
extern uint64_t TK8710GetTimeUs(void);
extern TrmContext* TRM_GetContext(void);

/* 外部变量声明 */
extern uint32_t g_trmCurrentFrame;
extern uint32_t g_trmMaxFrameCount;

/*==============================================================================
 * 私有定义
 *============================================================================*/

#define TX_QUEUE_SIZE   512       /* 每个优先级队列大小 */
#define TX_QUEUE_PRIORITY_COUNT 4 /* 优先级队列数量 (Pri=0最高, Pri=3最低) */
#define TX_DATA_MAX_LEN 520       /* Maximum payload length */
#define BEAM_RELEASE_QUEUE_SIZE 2048  /* 波束RAM释放队列大小 */
#define MAX_PENDING_USERS 128      /* 最大待发送用户数量 */


/* 发送数据项 */
#if defined(PLATFORM_TMS570)
#undef TX_QUEUE_SIZE
#define TX_QUEUE_SIZE 128
#undef TX_QUEUE_PRIORITY_COUNT
#define TX_QUEUE_PRIORITY_COUNT 1
#undef TX_DATA_MAX_LEN
#define TX_DATA_MAX_LEN 64
#undef BEAM_RELEASE_QUEUE_SIZE
#define BEAM_RELEASE_QUEUE_SIZE 512
#endif

typedef struct {
    uint32_t userId;
    uint8_t  data[TX_DATA_MAX_LEN];
    uint16_t len;
    uint8_t  power;
    uint8_t  valid;
    uint8_t  targetRateMode;  /**< 目标发送速率模式 (0=使用帧号, 5-11,18=使用速率模式) */
    uint8_t  beamType;        /**< 波束类型 (TK8710_DATA_TYPE_BRD=广播波束, TK8710_DATA_TYPE_DED=指定波束) */
    uint8_t  priority;        /**< QoS优先级 (0=最高, 3=最低) */
    uint8_t  ttl;             /**< QoS生存时间等级 (0-3) */
    uint32_t timestamp;
    uint32_t frameNo;         /**< 目标发送帧号 */
    uint32_t systemFrameNo;   /**< 入队时的系统帧号 */
} TxItem;

/* 待发送用户信息 */
typedef struct {
    uint32_t userId;
    uint8_t  data[TX_DATA_MAX_LEN];
    uint16_t len;
    uint8_t  beamType;
    TRM_BeamInfo beam;
    uint8_t  originalPower;  /* 原始功率 */
    uint8_t  finalPower;     /* 最终设置的功率 */
    uint8_t  priority;       /* 优先级 */
    uint8_t  queueIndex;     /* 在原队列中的位置 */
    uint8_t  queuePriority;  /* 队列优先级 */
    uint8_t  satelliteForward;
    uint8_t  groundStationTx;
} PendingTxUser;

/* 发送队列 */
typedef struct {
    TxItem items[TX_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} TxQueue;

/* 波束RAM延时释放项 */
typedef struct {
    uint32_t userId;
    uint32_t releaseFrame;
    uint8_t  valid;
} BeamReleaseItem;

/* 波束RAM释放队列 */
typedef struct {
    BeamReleaseItem items[BEAM_RELEASE_QUEUE_SIZE];  /* 最多支持延时释放任务 */
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} BeamReleaseQueue;

/*==============================================================================
 * 私有变量
 *============================================================================*/

static TxQueue g_txQueues[TX_QUEUE_PRIORITY_COUNT];  /* 4个优先级队列 */
static BeamReleaseQueue g_beamReleaseQueue;  /* 波束RAM释放队列 */

/* 广播数据管理 */
typedef struct {
    uint8_t data[TX_DATA_MAX_LEN];  /* 广播数据 */
    uint16_t len;                    /* 数据长度 */
    uint8_t power;                   /* 发送功率 */
    uint8_t valid;                   /* 有效标志 */
    uint8_t hasPayload;              /* 是否包含payload */
    uint32_t userId_brdIndex;        /* 广播索引 */
    uint8_t beamType;                /* 波束类型 */
    uint32_t timestamp;              /* 设置时间戳 */
} TrmBroadcastData;

#if !defined(TK8710_TMS570_PAYLOAD_ONLY)

static TrmBroadcastData g_broadcastData;  /* 广播数据存储 */
#endif
static PendingTxUser g_pendingUsers[MAX_PENDING_USERS];
static TRM_TxUserResult g_txResults[MAX_PENDING_USERS];
// static MemPool* g_txMemPool __attribute__((unused)) = NULL;  /* 保留供将来内存池优化使用 */

/*==============================================================================
 * 私有函数前向声明
============================================================================*/
static uint32_t TRM_GetTotalQueueCount(void);
static uint8_t TRM_CollectPendingUsers(PendingTxUser* pendingUsers, uint8_t maxUserCount, uint8_t isMultiRate, uint8_t currentRateMode, uint8_t nextRateMode);
static uint8_t TRM_SendCollectedUsers(PendingTxUser* pendingUsers, uint8_t userCount, TRM_TxUserResult* txResults, uint32_t* resultCount);

static void TRM_TxQueueInsert(TxQueue* queue, const TxItem* item)
{
    uint32_t insertIndex;

    insertIndex = queue->count;
    while ((insertIndex > 0U) &&
           (queue->items[insertIndex - 1U].priority > item->priority)) {
        queue->items[insertIndex] = queue->items[insertIndex - 1U];
        insertIndex--;
    }

    queue->items[insertIndex] = *item;
    queue->count++;
    queue->head = 0U;
    queue->tail = queue->count % TX_QUEUE_SIZE;
}

static void TRM_TxQueueRemoveAt(TxQueue* queue, uint32_t index)
{
    uint32_t moveCount;

    if ((queue == NULL) || (index >= queue->count)) {
        return;
    }

    moveCount = queue->count - index - 1U;
    if (moveCount > 0U) {
        memmove(&queue->items[index], &queue->items[index + 1U],
                moveCount * sizeof(TxItem));
    }
    queue->count--;
    memset(&queue->items[queue->count], 0, sizeof(TxItem));
    queue->head = 0U;
    queue->tail = queue->count % TX_QUEUE_SIZE;
}

static uint32_t TRM_EncodeFreqForTxCache(uint32_t freqRaw)
{
    return freqRaw & 0x03FFFFFFU;
}

/*==============================================================================
 * 公共接口实现
 *============================================================================*/

/*==============================================================================
 * 私有函数
 *============================================================================*/

/**
 * @brief 添加波束RAM延时释放任务
 * @param userId 用户ID
 * @param delayFrames 延时帧数
 */
void TRM_ScheduleBeamRamRelease(uint32_t userId, uint32_t delayFrames)
{
    /* 检查是否已存在该用户的释放任务 */
    uint32_t head = g_beamReleaseQueue.head;
    uint32_t count = 0;
    
    while (count < g_beamReleaseQueue.count) {
        uint32_t index = (head + count) % BEAM_RELEASE_QUEUE_SIZE;
        BeamReleaseItem* item = &g_beamReleaseQueue.items[index];
        
        if (item->valid && item->userId == userId) {
            uint32_t newReleaseFrame = g_trmCurrentFrame + delayFrames;
            if (newReleaseFrame > item->releaseFrame) {
                item->releaseFrame = newReleaseFrame;
            }
            TRM_LOG_DEBUG("TRM: Updated existing beam RAM release for user[%u] at frame=%u (delay=%u)", 
                          userId, item->releaseFrame, delayFrames);
            return;
        }
        count++;
    }
    
    /* 检查队列是否已满 */
    if (g_beamReleaseQueue.count >= BEAM_RELEASE_QUEUE_SIZE) {
        TRM_LOG_WARN("Beam release queue full, cannot schedule release for user[%u]", userId);
        return;
    }
    
    uint32_t tail = (g_beamReleaseQueue.head + g_beamReleaseQueue.count) % BEAM_RELEASE_QUEUE_SIZE;
    BeamReleaseItem* item = &g_beamReleaseQueue.items[tail];
    
    item->userId = userId;
    item->releaseFrame = g_trmCurrentFrame + delayFrames;
    item->valid = 1;
    
    g_beamReleaseQueue.count++;
    
    TRM_LOG_DEBUG("TRM: Scheduled beam RAM release for user[%u] at frame=%u (delay=%u)", 
                  userId, item->releaseFrame, delayFrames);
}

/**
 * @brief 处理波束RAM延时释放
 */
void TRM_ProcessBeamRamReleases(void)
{
    static uint32_t lastReportFrame = 0;
    uint32_t processedCount = 0;
    
    /* 遍历队列，处理所有项目 */
    while (g_beamReleaseQueue.count > 0) {
        BeamReleaseItem* item = &g_beamReleaseQueue.items[g_beamReleaseQueue.head];
        
        if (!item->valid) {
            /* 无效项，直接清理并移除 */
            g_beamReleaseQueue.head = (g_beamReleaseQueue.head + 1) % BEAM_RELEASE_QUEUE_SIZE;
            g_beamReleaseQueue.count--;
            continue;
        }
        
        if (item->releaseFrame <= g_trmCurrentFrame) {
            /* 到达释放时间，执行释放 */
            TRM_LOG_DEBUG("TRM: Releasing beam RAM for user[%u] at frame=%u (scheduled=%u)", 
                          item->userId, g_trmCurrentFrame, item->releaseFrame);
            
            /* 调用波束信息清理函数 */
            TRM_ClearBeamInfo(item->userId);
            
            /* 移除已处理的项 */
            g_beamReleaseQueue.head = (g_beamReleaseQueue.head + 1) % BEAM_RELEASE_QUEUE_SIZE;
            g_beamReleaseQueue.count--;
            processedCount++;
        } else {
            /* 头项还未到释放时间，停止处理（后面的项目时间更晚） */
            break;
        }
    }
    
    /* 每100帧报告一次队列状态 */
    if (g_trmCurrentFrame - lastReportFrame >= 100) {
#if TX_QUEUE_PRIORITY_COUNT == 1
        TRM_LOG_INFO("TRM: Queue status - BeamRelease: %u/%u, TxQueue=%u/%u, processed=%u",
                     g_beamReleaseQueue.count, BEAM_RELEASE_QUEUE_SIZE,
                     g_txQueues[0].count, TX_QUEUE_SIZE, processedCount);
#else
        TRM_LOG_INFO("TRM: Queue status - BeamRelease: %u/%u, TxQueue[Pri0]=%u, [Pri1]=%u, [Pri2]=%u, [Pri3]=%u, Total=%u/%u, processed=%u",
                     g_beamReleaseQueue.count, BEAM_RELEASE_QUEUE_SIZE,
                     g_txQueues[0].count, g_txQueues[1].count, g_txQueues[2].count, g_txQueues[3].count,
                     TRM_GetTotalQueueCount(), TX_QUEUE_SIZE * TX_QUEUE_PRIORITY_COUNT, processedCount);
#endif
        lastReportFrame = g_trmCurrentFrame;
    }
}

/*==============================================================================
 * 公共函数实现
 *============================================================================*/


/**
 * @brief 统一发送数据接口（支持用户数据和广播数据）
 * @param downlinkType 下行位置 (TK8710_DOWNLINK_A=slot1发送, TK8710_DOWNLINK_B=slot3发送)
 * @param userId_brdIndex 用户ID或广播索引
 * @param data 数据指针
 * @param len 数据长度
 * @param txPower 发送功率
 * @param frameNo 帧号 (仅用户数据使用，广播时忽略)
 * @param targetRateMode 目标速率模式 (仅用户数据使用，广播时忽略)
 * @param BeamType 波束类型 (0=广播波束, 1=指定波束)
 * @return TRM_OK成功，其他失败
 */
int TRM_SetTxData(TK8710DownlinkType downlinkType, uint32_t userId_brdIndex, const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo, uint8_t targetRateMode, uint8_t BeamType)
{
    if (data == NULL || len == 0 || len > TX_DATA_MAX_LEN) {
        TRM_LOG_ERROR("TRM_SetTxData failed: invalid parameters - data=%p, len=%d", data, len);
        return TRM_ERR_PARAM;
    }
    
    if (downlinkType == TK8710_DOWNLINK_A) {
#if defined(TK8710_TMS570_PAYLOAD_ONLY)
        TRM_LOG_ERROR("TRM broadcast is unsupported on TMS570 satellite payload");
        return TRM_ERR_STATE;
#else
        /* 广播数据模式 - 存储广播数据，由广播管理函数统一管理 */
        TRM_LOG_DEBUG("TRM_SetTxData store broadcast - index=%d, len=%d, power=%d, beamType=%d", 
                      (uint8_t)userId_brdIndex, len, txPower, BeamType);
        
        /* 存储广播数据 */
        memcpy(g_broadcastData.data, data, len);
        g_broadcastData.len = len;
        g_broadcastData.power = txPower;
        g_broadcastData.userId_brdIndex = userId_brdIndex;
        g_broadcastData.beamType = BeamType;
        g_broadcastData.valid = 1;
        g_broadcastData.hasPayload = 1;  /* 标记包含payload */
        g_broadcastData.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
        
        TRM_LOG_INFO("TRM broadcast data stored - index=%d, len=%d, hasPayload=true", 
                     (uint8_t)userId_brdIndex, len);
        
        return TRM_OK;
#endif
        
    } else if (downlinkType == TK8710_DOWNLINK_B) {
        uint32_t userId;
        int ret;

        if (TRM_ExtractUserIdFromMacFrame(data, len, &userId) != 0) {
            userId = userId_brdIndex;
        }
        ret = TRM_SatelliteBeforeTxData(userId, data, len);
        if (ret != TRM_OK) {
            return ret;
        }
        /* 用户数据模式 - 缓存到发送队列 */
        TRM_LOG_DEBUG("TRM_SetTxData send user data - userId=0x%08X, len=%d, power=%d, frame=%u, rateMode=%d", 
                      userId, len, txPower, frameNo, targetRateMode);
        ret = TRM_SendData(userId, data, len, txPower, frameNo, targetRateMode, BeamType);
        TRM_SatelliteAfterTxData(userId, ret);
        return ret;
        
    } else {
        TRM_LOG_ERROR("TRM_SetTxData failed: invalid downlink type - downlinkType=%d", downlinkType);
        return TRM_ERR_PARAM;
    }
}

/**
 * @brief 广播发送管理函数
 * @note 在每个时隙开始时调用，负责广播数据的发送管理
 * @return TRM_OK成功，其他失败
 */
int TRM_ManageBroadcast(void)
{
#if defined(TK8710_TMS570_PAYLOAD_ONLY)
    return TRM_OK;
#else
    static uint8_t brdCounter = 0;  /* 自主管理广播计数器 */
    // uint32_t currentSuperFramePos = TRM_GetSuperFramePosition();
    uint32_t currentSuperFramePos = TRM_GetSuperFramePosition() + 2;
    if(currentSuperFramePos == (g_trmMaxFrameCount + 1)){
        currentSuperFramePos = 1;
    }else if(currentSuperFramePos == (g_trmMaxFrameCount + 2)){
        currentSuperFramePos = 2;
    }else{

    }
    if(g_trmMaxFrameCount==1){
        currentSuperFramePos = 1;
    }
    TRM_ConfigureTddPeriodInBroadcast(g_broadcastData.data, g_broadcastData.len, (uint8_t)currentSuperFramePos);
    /* 检查是否有上层设置的广播数据（包含payload） */
    if (g_broadcastData.valid && g_broadcastData.hasPayload) {
        /* 发送包含payload的广播数据 */
        TRM_LOG_DEBUG("TRM send payload broadcast - index=%d, len=%d, power=%d", 
                      (uint8_t)g_broadcastData.userId_brdIndex, g_broadcastData.len, g_broadcastData.power);
        
        int ret = TK8710SetTxData(TK8710_DOWNLINK_A, (uint8_t)g_broadcastData.userId_brdIndex, 
                                 g_broadcastData.data, g_broadcastData.len, g_broadcastData.power, g_broadcastData.beamType);
        
        if (ret == TK8710_OK) {
            TRM_LOG_INFO("TRM payload broadcast sent - index=%d, frame=%u", 
                         (uint8_t)g_broadcastData.userId_brdIndex, currentSuperFramePos);
            
            /* Payload只发送一次，后续恢复自主管理 */
            g_broadcastData.hasPayload = 0;
            g_broadcastData.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
        } else {
            TRM_LOG_ERROR("TRM payload broadcast send failed - index=%d, code=%d", 
                          (uint8_t)g_broadcastData.userId_brdIndex, ret);
        }
        
        return (ret == TK8710_OK) ? TRM_OK : TRM_ERR_DRIVER;
        
    } else {
        /* 自主管理广播 - 使用测试数据 */
        uint8_t testData[64] = {0};
        
        /* 从g_broadcastData.data复制前16个字节到testData */
        if (g_broadcastData.len > 0) {
            uint16_t copyLen = (g_broadcastData.len > 16) ? 16 : g_broadcastData.len;
            memcpy(testData, g_broadcastData.data, copyLen);
            TRM_LOG_DEBUG("TRM autonomous broadcast - copied %d bytes from broadcast data", copyLen);
        }
        
        /* 使用默认广播参数 */
        uint8_t brdIndex = 0;  /* 默认广播索引 */
        uint8_t txPower = 35;  /* 默认发送功率 */
        uint8_t beamType = TK8710_DATA_TYPE_BRD;  /* 广播波束类型 */
        
        TRM_LOG_DEBUG("TRM send autonomous broadcast - index=%d, counter=%d, superFramePos=%u", 
                      brdIndex, brdCounter, currentSuperFramePos);
        
        int ret = TK8710SetTxData(TK8710_DOWNLINK_A, brdIndex, testData, g_broadcastData.len, txPower, beamType);
        
        if (ret == TK8710_OK) {
            brdCounter++;
            TRM_LOG_DEBUG("TRM autonomous broadcast sent - index=%d, counter=%d", brdIndex, brdCounter);
        } else {
            TRM_LOG_ERROR("TRM autonomous broadcast send failed - index=%d, code=%d", brdIndex, ret);
        }
        
        return (ret == TK8710_OK) ? TRM_OK : TRM_ERR_DRIVER;
    }
#endif
}

/**
 * @brief 清除广播数据
 * @return TRM_OK成功
 */
int TRM_ClearBroadcast(void)
{
#if defined(TK8710_TMS570_PAYLOAD_ONLY)
    return TRM_OK;
#else
    memset(&g_broadcastData, 0, sizeof(g_broadcastData));
    TRM_LOG_DEBUG("TRM broadcast data cleared");
    return TRM_OK;
#endif
}

/**
 * @brief 获取广播状态
 * @param hasPayload 输出是否包含payload
 * @param valid 输出是否有效
 * @return TRM_OK成功
 */
int TRM_GetBroadcastStatus(uint8_t* hasPayload, uint8_t* valid)
{
#if defined(TK8710_TMS570_PAYLOAD_ONLY)
    if (hasPayload != NULL) *hasPayload = 0U;
    if (valid != NULL) *valid = 0U;
#else
    if (hasPayload) *hasPayload = g_broadcastData.hasPayload;
    if (valid) *valid = g_broadcastData.valid;
#endif
    return TRM_OK;
}

int TRM_SendData(uint32_t userId, const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo, uint8_t targetRateMode, uint8_t BeamType)
{
    if (data == NULL || len == 0 || len > TX_DATA_MAX_LEN) {
        TRM_LOG_ERROR("TRM send data failed: invalid parameters - data=%p, len=%d", data, len);
        return TRM_ERR_PARAM;
    }
    
    /* 检查速率模式有效性 */
    if (targetRateMode != 0 && (targetRateMode < 5 || targetRateMode > 11) && targetRateMode != 18) {
        TRM_LOG_ERROR("TRM send data failed: invalid rate mode - targetRateMode=%d", targetRateMode);
        return TRM_ERR_PARAM;
    }
    
    /* 从MHDR第二个字节提取QosPri和QosTTL (参考MAC协议规范6.1.1) */
    uint8_t priority = 3;  /* 默认最低优先级 */
    uint8_t ttl = 0;       /* 默认TTL */
    if (len >= 2) {
        uint8_t mhdrByte2 = data[1];
        priority = (mhdrByte2 >> 6) & 0x03;
        ttl = (mhdrByte2 >> 4) & 0x03;
    }
    
    /* 同时获取QoS信息 */
    uint8_t qosPri, qosTtl;
    if (TRM_GetMacFrameQosInfo(data, len, &qosPri, &qosTtl) == 0) {
        TRM_LOG_DEBUG("TRM: MAC Frame - User ID: 0x%08X, QoS Pri=%d, TTL=%d", 
                      userId, qosPri, qosTtl);
    } else {
        TRM_LOG_DEBUG("TRM: MAC Frame - User ID: 0x%08X, QoS info unavailable", userId);
    }
    
    /* 检查帧号有效性 - 帧号应该是循环的 */
    // uint32_t normalizedFrameNo = frameNo == 0xFF ? 0xFF : (frameNo % g_trmMaxFrameCount + 1);
    uint32_t normalizedFrameNo = frameNo;
    TK8710EnterCritical();
    
    /* 获取对应优先级的队列 */
    TxQueue* queue = &g_txQueues[(TX_QUEUE_PRIORITY_COUNT == 1U) ? 0U : priority];
    
    /* 检查队列是否满 */
    if (queue->count >= TX_QUEUE_SIZE) {
        TK8710ExitCritical();
        TRM_LOG_WARN("TRM send data failed: priority %d queue is full - count=%u", priority, queue->count);
        return TRM_ERR_QUEUE_FULL;
    }
    
    /* 查找空闲队列项 */
    TxItem item;
    memset(&item, 0, sizeof(item));
    
    /* 填充发送数据项 */
    item.userId = userId;
    memcpy(item.data, data, len);
    item.len = len;
    item.power = txPower;
    item.valid = 1;
    item.targetRateMode = targetRateMode;
    item.beamType = BeamType;
    item.priority = priority;
    item.ttl = ttl;
    item.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
    item.frameNo = normalizedFrameNo;
    item.systemFrameNo = g_trmCurrentFrame; /* 记录入队时的系统帧号 */
    
    TRM_TxQueueInsert(queue, &item);
    
    TK8710ExitCritical();
    
    TRM_LOG_DEBUG("TRM data enqueued - userId=0x%08X, len=%d, priority=%d, TTL=%d, queueCount=%u", 
                  userId, len, priority, ttl, queue->count);
    
    return TRM_OK;
}


int TRM_ClearTxData(uint32_t userId)
{
    TK8710EnterCritical();
    
    if (userId == 0xFFFFFFFF) {
        /* 清除所有优先级队列 */
        for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
            memset(&g_txQueues[pri], 0, sizeof(TxQueue));
        }
    } else {
        /* 清除指定用户 - 遍历所有优先级队列 */
        for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
            uint32_t i = 0U;
            while (i < g_txQueues[pri].count) {
                if (g_txQueues[pri].items[i].valid &&
                    g_txQueues[pri].items[i].userId == userId) {
                    TRM_TxQueueRemoveAt(&g_txQueues[pri], i);
                } else {
                    i++;
                }
            }
        }
    }
    
    TK8710ExitCritical();
    
    return TRM_OK;
}

/**
 * @brief 获取所有优先级队列的总数量
 * @return 所有队列的总数量
 */
static uint32_t TRM_GetTotalQueueCount(void)
{
    uint32_t total = 0;
    for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
        total += g_txQueues[pri].count;
    }
    return total;
}

/**
 * @brief 处理单个队列项
 * @param queue 队列指针
 * @param item 队列项指针
 * @param isMultiRate 是否多速率模式
 * @param nextRateMode 下一帧速率模式
 * @param txUserIndex 发送用户索引指针
 * @param txResults 发送结果数组
 * @param resultCount 结果计数指针
 * @param sentCount 发送计数指针
 * @return 1=已处理(需出队), 0=跳过(保留在队列)
 */
static uint8_t TK8710_UNUSED TRM_ProcessQueueItem(TxQueue* queue, TxItem* item, uint8_t isMultiRate, 
                                     uint8_t nextRateMode, uint8_t* txUserIndex,
                                     TRM_TxUserResult* txResults, uint32_t* resultCount,
                                     uint8_t* sentCount)
{
    uint8_t shouldSend = 0;
    uint8_t shouldRemove = 0;
    
    if (isMultiRate) {
        /* 多速率模式：只检查目标速率模式是否匹配下一帧速率模式 */
        if (item->targetRateMode == nextRateMode) {
            shouldSend = 1;
        }
    } else {
        /* 单速率模式：使用相对帧差判断 */
        uint32_t currentSuperFramePos = TRM_GetSuperFramePosition();
        uint32_t frameDiff = (currentSuperFramePos - item->frameNo + g_trmMaxFrameCount) % g_trmMaxFrameCount;
        uint32_t systemFrameDiff = g_trmCurrentFrame - item->systemFrameNo;
        
        if (item->frameNo == currentSuperFramePos || item->frameNo == 0xFF) {
            shouldSend = 1;
        } else if (frameDiff > g_trmMaxFrameCount / 2) {
            if (systemFrameDiff > g_trmMaxFrameCount) {
                /* 超时丢弃 */
                if (*resultCount < MAX_PENDING_USERS) {
                    txResults[*resultCount].userId = item->userId;
                    txResults[*resultCount].result = TRM_TX_TIMEOUT;
                    (*resultCount)++;
                }
                shouldRemove = 1;
            }
            /* 未来帧，跳过 */
        } else {
            /* 过期帧，丢弃 */
            if (*resultCount < MAX_PENDING_USERS) {
                txResults[*resultCount].userId = item->userId;
                txResults[*resultCount].result = TRM_TX_TIMEOUT;
                (*resultCount)++;
            }
            shouldRemove = 1;
        }
    }
    
    if (shouldSend) {
        if (item->userId == 0) {
            shouldRemove = 1;
        } else {
            TRM_BeamInfo beam;
            int beamRet = TRM_GetBeamInfo(item->userId, &beam);
            if (beamRet == TRM_OK) {
                int ret = TK8710SetTxData(TK8710_DOWNLINK_B, *txUserIndex, item->data, item->len, item->power, item->beamType);
                if (ret == TK8710_OK) {
                    ret = TK8710SetTxUserInfo(*txUserIndex, beam.freq, beam.ahData, beam.pilotPower);
                    if (ret == TK8710_OK) {
                        (*sentCount)++;
                        if (*resultCount < MAX_PENDING_USERS) {
                            txResults[*resultCount].userId = item->userId;
                            txResults[*resultCount].result = TRM_TX_OK;
                            (*resultCount)++;
                        }
                        if (!TRM_SatelliteKeepTxBeam(item->userId)) {
                            TRM_ScheduleBeamRamRelease(item->userId, 4U);
                        }
                    } else {
                        if (*resultCount < MAX_PENDING_USERS) {
                            txResults[*resultCount].userId = item->userId;
                            txResults[*resultCount].result = TRM_TX_ERROR;
                            (*resultCount)++;
                        }
                    }
                } else {
                    if (*resultCount < MAX_PENDING_USERS) {
                        txResults[*resultCount].userId = item->userId;
                        txResults[*resultCount].result = TRM_TX_ERROR;
                        (*resultCount)++;
                    }
                }
                (*txUserIndex)++;
                if (*txUserIndex >= 128) *txUserIndex = 0;
            } else {
                if (*resultCount < MAX_PENDING_USERS) {
                    txResults[*resultCount].userId = item->userId;
                    txResults[*resultCount].result = TRM_TX_NO_BEAM;
                    (*resultCount)++;
                }
            }
            shouldRemove = 1;
        }
    }
    
    return shouldRemove;
}

/**
 * @brief 收集所有待发送用户
 * @param pendingUsers 输出：收集到的待发送用户数组
 * @param maxUserCount 最大用户数量
 * @param isMultiRate 是否为多速率模式
 * @param currentRateMode 当前速率模式
 * @param nextRateMode 下一个速率模式
 * @return 实际收集到的用户数量
 */
static uint8_t TRM_CollectPendingUsers(PendingTxUser* pendingUsers, uint8_t maxUserCount, uint8_t isMultiRate, uint8_t currentRateMode, uint8_t nextRateMode)
{
    uint8_t collectedCount = 0;
    static uint8_t singleUserSent = 0;  /* 静态变量记录本次是否已发送用户 */
    
    /* 重置单用户发送标志 */
    singleUserSent = 0;
    
    /* 按优先级顺序处理队列 (Pri=0最高优先级先处理) */
    for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT && collectedCount < maxUserCount; pri++) {
        TxQueue* queue = &g_txQueues[pri];
        uint16_t processedInQueue = 0;
        uint16_t scanIndex = 0;
        uint16_t maxProcess = queue->count;  /* 记录初始数量，避免无限循环 */
        
        while ((scanIndex < queue->count) &&
               (collectedCount < maxUserCount) &&
               (processedInQueue < maxProcess)) {
            TxItem* item = &queue->items[scanIndex];
            processedInQueue++;
            
            if (!item->valid) {
                /* 无效项，直接出队 */
                TRM_TxQueueRemoveAt(queue, scanIndex);
                continue;
            }
            
            /* 使用与TRM_ProcessQueueItem相同的判断逻辑 */
            uint8_t shouldSend = 0;
            uint8_t shouldRemove = 0;
            uint8_t sendCollected = 0;
            
            TRM_LOG_DEBUG("TRM: Processing item - userId=0x%08X, frameNo=%u, targetRateMode=%u, systemFrameNo=%u", 
                         item->userId, item->frameNo, item->targetRateMode, item->systemFrameNo);
            
            if (isMultiRate) {
                /* 多速率模式：只检查目标速率模式是否匹配下一帧速率模式 */
                if (item->targetRateMode == nextRateMode) {
                    shouldSend = 1;
                }
                TRM_LOG_DEBUG("TRM: MultiRate mode - item->targetRateMode=%u, nextRateMode=%u, shouldSend=%u", 
                             item->targetRateMode, nextRateMode, shouldSend);
            } else {
                /* 单速率模式：使用相对帧差判断 */
                if (item->targetRateMode == currentRateMode) {
                    uint32_t currentSuperFramePos = TRM_GetSuperFramePosition();
                    uint32_t frameDiff = (currentSuperFramePos - item->frameNo + g_trmMaxFrameCount) % g_trmMaxFrameCount;
                    uint32_t systemFrameDiff = g_trmCurrentFrame - item->systemFrameNo;
                    
                    TRM_LOG_DEBUG("TRM: SingleRate mode - currentSuperFramePos=%u, item->frameNo=%u, frameDiff=%u, systemFrameDiff=%u", 
                                 currentSuperFramePos, item->frameNo, frameDiff, systemFrameDiff);
                    
                    if (item->frameNo == currentSuperFramePos || item->frameNo == 0xFF) {
                        shouldSend = 1;
                        TRM_LOG_DEBUG("TRM: Frame match - shouldSend=1 (frameNo=%u, currentPos=%u)", 
                                     item->frameNo, currentSuperFramePos);
                    } else if (item->frameNo == currentSuperFramePos || item->frameNo == 0x00) {
                        /* 特殊条件：匹配当前帧号或0x00，但限制一次只发送一个用户 */
                        if (!singleUserSent) {
                            shouldSend = 1;
                            singleUserSent = 1;
                            TRM_LOG_DEBUG("TRM: Single user condition - shouldSend=1 (frameNo=%u, currentPos=%u)", 
                                         item->frameNo, currentSuperFramePos);
                        } else {
                            /* 已经发送过一个用户，跳过其他满足条件的用户 */
                            TRM_LOG_DEBUG("TRM: Single user already sent, skipping user (frameNo=%u)", item->frameNo);
                        }
                    } else if (frameDiff > g_trmMaxFrameCount / 2) {
                        if (systemFrameDiff > g_trmMaxFrameCount) {
                            /* 超时丢弃 */
                            shouldRemove = 1;
                            TRM_LOG_DEBUG("TRM: Timeout - shouldRemove=1");
                        }
                        /* 未来帧，跳过 */
                        TRM_LOG_DEBUG("TRM: Future frame - skipping");
                    } else {
                        /* 过期帧，丢弃 */
                        shouldRemove = 1;
                        TRM_LOG_DEBUG("TRM: Expired frame - shouldRemove=1");
                    }
                } else {
                    /* 单速率模式下指定了速率模式，丢弃 */
                    shouldRemove = 1;
                    TRM_LOG_DEBUG("TRM: Invalid rate mode for single rate - shouldRemove=1");
                }
            }
            
            if (shouldSend && item->userId == 0U) {
                shouldRemove = 1U;
            }

            /* 如果应该发送且用户ID有效，则收集用户信息 */
            if (shouldSend && item->userId != 0) {
                /* 收集用户信息 */
                PendingTxUser* pendingUser = &pendingUsers[collectedCount];
                pendingUser->userId = item->userId;
                memcpy(pendingUser->data, item->data, item->len);
                pendingUser->len = item->len;
                pendingUser->beamType = item->beamType;
                pendingUser->originalPower = item->power;
                pendingUser->finalPower = item->power;  /* 暂时使用原始功率 */
                pendingUser->priority = item->priority;
                pendingUser->queueIndex = (uint8_t)scanIndex;
                pendingUser->queuePriority = pri;
                pendingUser->satelliteForward = 0U;
                pendingUser->groundStationTx = 0U;
                
                /* 根据波束类型处理波束信息 */
                if (item->beamType == TK8710_DATA_TYPE_BRD) {
                    /* 广播波束：使用默认波束信息，不需要获取 */
                    memset(&pendingUser->beam, 0, sizeof(TRM_BeamInfo));
                    pendingUser->beam.valid = 1;
                    pendingUser->beam.freq = 20000;  /* 默认频率 */
                    pendingUser->beam.pilotPower = 1000000;  /* 默认Pilot功率 */
                    for (int j = 0; j < 16; j++) {
                        pendingUser->beam.ahData[j] = 8192 + j;  /* 默认AH值 */
                    }
                    pendingUser->beam.timestamp = TK8710GetTickMs();
                    TRM_LOG_DEBUG("TRM: Using default beam info for broadcast user[%u]", item->userId);
                    collectedCount++;
                    sendCollected = 1U;
                    shouldRemove = 1;  /* 标记为需要移除 */
                } else {
                    /* 指定波束：需要获取波束信息 */
                    TRM_BeamInfo beam;
                    int beamRet = TRM_GetBeamInfo(item->userId, &beam);

                    if (beamRet != TRM_OK) {
                        beamRet = TRM_SatelliteGetTxBeam(item->userId, &beam);
                    }
                     
                    if (beamRet == TRM_OK) {
                        pendingUser->beam = beam;
                        pendingUser->satelliteForward =
                            TRM_SatelliteIsPayloadGroundStationTx(item->userId);
                        pendingUser->groundStationTx = TRM_SatelliteIsGroundStationTx();
                        collectedCount++;
                        sendCollected = 1U;
                        shouldRemove = 1;  /* 标记为需要移除 */
                    } else {
                        TRM_LOG_WARN("TRM: Failed to get beam info for user[%u]: %d", item->userId, beamRet);
                        shouldRemove = 1U;
                    }
                }
            }
            
            /* Remove completed/expired items; leave future items sorted in place. */
            if (sendCollected) {
                TRM_TxQueueRemoveAt(queue, scanIndex);
            } else if (shouldRemove) {
                TRM_TxQueueRemoveAt(queue, scanIndex);
            } else {
                scanIndex++;
            }
        }
    }
    
    TRM_LOG_DEBUG("TRM: Collected %u pending users for sending", collectedCount);
    return collectedCount;
}

/**
 * @brief 发送收集到的用户（统一功率设置）
 * @param pendingUsers 待发送用户数组
 * @param userCount 用户数量
 * @param txResults 输出：发送结果数组
 * @param resultCount 输出：结果数量
 * @return 实际发送的用户数量
 */
static uint8_t TRM_SendCollectedUsers(PendingTxUser* pendingUsers, uint8_t userCount, TRM_TxUserResult* txResults, uint32_t* resultCount)
{
    uint8_t sentCount = 0;
    uint8_t txUserIndex = 0;
    uint8_t satelliteForwardCount = 0;
    uint8_t satelliteForwardIndex = 0;
    
    /* 功率设置阶段：所有用户使用固定功率 */
    uint8_t fixedPower = 31;
    if (userCount > 64U) {
        fixedPower = 55U;
    } else if (userCount > 32U) {
        fixedPower = 52U;
    } else if (userCount > 16U) {
        fixedPower = 46U;
    } else if (userCount > 8U) {
        fixedPower = 43U;
    } else if (userCount > 4U) {
        fixedPower = 37U;
    } else if (userCount > 1U) {
        fixedPower = 34U;
    }
    
    for (uint8_t i = 0; i < userCount; i++) {
        PendingTxUser* user = &pendingUsers[i];
        user->finalPower = fixedPower;  /* 统一设置固定功率 */
        if (user->satelliteForward != 0U) {
            satelliteForwardCount++;
        }
    }
    
    /* 发送阶段：统一调用发送接口 */
    for (uint8_t i = 0; i < userCount; i++) {
        PendingTxUser* user = &pendingUsers[i];

        if (user->satelliteForward != 0U) {
            TRM_SatelliteAdjustForwardBeam(satelliteForwardIndex,
                                           satelliteForwardCount,
                                           TK8710GetRateMode(),
                                           &user->beam);
            satelliteForwardIndex++;
        } else if (user->groundStationTx != 0U) {
            TRM_SatelliteAdjustGroundStationTxBeam(TK8710GetRateMode(), &user->beam);
        }
        
        /* 调用发送接口 */
        int ret = TK8710SetTxData(TK8710_DOWNLINK_B, txUserIndex, user->data, user->len, user->finalPower, user->beamType);
        
        if (ret == TK8710_OK) {
            uint32_t freqRaw = user->beam.freq;
            uint32_t freq26 = freqRaw & 0x03FFFFFF;  /* 取26位 */
            int32_t freqValue = freq26 > (1<<25) ? (int32_t)(freq26 - (1<<26)) : (int32_t)freq26;
            TRM_LOG_INFO("TRM: TX user=%u index=%u len=%u rate=%u freq=%d power=%u ah0=%05X/%05X pilot=%llu",
                         user->userId, txUserIndex, user->len, TK8710GetRateMode(),
                         freqValue / 128, user->finalPower,
                         user->beam.ahData[0] & 0xFFFFFU,
                         user->beam.ahData[1] & 0xFFFFFU,
                         (unsigned long long)(user->beam.pilotPower & 0xFFFFFFFFFFULL));
            ret = TK8710SetTxUserInfo(txUserIndex, user->beam.freq, user->beam.ahData, user->beam.pilotPower);
            
            if (ret == TK8710_OK) {
                /* 发送成功 */
                sentCount++;
                if (*resultCount < MAX_PENDING_USERS) {
                    txResults[*resultCount].userId = user->userId;
                    txResults[*resultCount].result = TRM_TX_OK;
                    (*resultCount)++;
                }
                if (!TRM_SatelliteKeepTxBeam(user->userId)) {
                    TRM_ScheduleBeamRamRelease(user->userId, 10);
                }
                if (user->beamType != TK8710_DATA_TYPE_BRD &&
                    user->satelliteForward == 0U) {
                    (void)TRM_TouchBeamInfoNoLock(user->userId);
                }
                TRM_LOG_DEBUG("TRM: Successfully sent user[%u] with power=%u", user->userId, user->finalPower);
            } else {
                /* 设置用户信息失败 */
                if (*resultCount < MAX_PENDING_USERS) {
                    txResults[*resultCount].userId = user->userId;
                    txResults[*resultCount].result = TRM_TX_ERROR;
                    (*resultCount)++;
                }
                TRM_LOG_WARN("TRM: Failed to set user info for user[%u]", user->userId);
            }
        } else {
            /* 发送数据失败 */
            if (*resultCount < MAX_PENDING_USERS) {
                txResults[*resultCount].userId = user->userId;
                txResults[*resultCount].result = TRM_TX_ERROR;
                (*resultCount)++;
            }
            TRM_LOG_WARN("TRM: Failed to send data for user[%u]", user->userId);
        }
        
        txUserIndex++;
        if (txUserIndex >= 128) txUserIndex = 0;
    }
    
    TRM_LOG_INFO("TRM: Sent %u/%u users with fixed power=%u", sentCount, userCount, fixedPower);
    return sentCount;
}

/* 内部函数 - 在发送时隙回调中调用 */
int TRM_ProcessTxSlot(uint8_t slotIndex, uint8_t maxUserCount, TK8710IrqResult* irqResult)
{
    uint8_t sentCount = 0;
    uint32_t resultCount = 0;
    
    /* 首先处理波束RAM延时释放 */
    TRM_ProcessBeamRamReleases();
        
    /* 获取当前时隙配置以判断是否为多速率模式 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint8_t isMultiRate = (slotCfg && slotCfg->rateCount > 1);
    uint8_t currentRateMode = 0;
    uint8_t nextRateMode = 0;
        
    if (isMultiRate && irqResult) {
        uint8_t currentRateIndex = irqResult->currentRateIndex;
        if (currentRateIndex < slotCfg->rateCount) {
            currentRateMode = slotCfg->rateModes[currentRateIndex];
            uint8_t nextRateIndex = (currentRateIndex + 1) % slotCfg->rateCount;
            nextRateMode = slotCfg->rateModes[nextRateIndex];
        } else {
            currentRateMode = slotCfg->rateModes[0];
            nextRateMode = slotCfg->rateModes[0];
        }
    } else if (slotCfg && slotCfg->rateCount > 0) {
        currentRateMode = slotCfg->rateModes[0];
        nextRateMode = slotCfg->rateModes[0];
    } else {
        currentRateMode = TK8710_RATE_MODE_8;
        nextRateMode = TK8710_RATE_MODE_8;
    }
        
    TK8710EnterCritical();
    
    /* 新的两阶段处理逻辑 */
    uint8_t pendingUserCount = 0;
    
    /* 第一阶段：收集所有待发送用户 */
    pendingUserCount = TRM_CollectPendingUsers(g_pendingUsers, maxUserCount,
                                               isMultiRate, currentRateMode,
                                               nextRateMode);
    
    /* 第二阶段：统一功率设置并发送 */
    if (pendingUserCount > 0) {
        sentCount = TRM_SendCollectedUsers(g_pendingUsers, pendingUserCount,
                                           g_txResults, &resultCount);
    }
    
    TK8710ExitCritical();
    
    uint32_t totalRemaining = TRM_GetTotalQueueCount();
    TRM_LOG_DEBUG("TRM: ProcessTxSlot completed - sentCount=%d, totalRemaining=%u, multiRate=%s", 
                 sentCount, totalRemaining, isMultiRate ? "true" : "false");
    
    /* 调用发送完成回调 */
    if (resultCount > 0) {
        TrmContext* ctx = TRM_GetContext();
        if (ctx && ctx->config.callbacks.onTxComplete) {
            TRM_TxCompleteResult txResult;
            txResult.totalUsers = resultCount;
            txResult.superFrameNo = TRM_GetSuperFramePosition();  /* 获取当前超帧号 */
            txResult.remainingQueue = totalRemaining;
            txResult.userCount = resultCount;
            txResult.users = g_txResults;
            ctx->config.callbacks.onTxComplete(&txResult);
        }
    }
    
    return sentCount;
}

/* 内部初始化函数 */
void TRM_DataInit(void)
{
    for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
        memset(&g_txQueues[pri], 0, sizeof(TxQueue));
    }
    memset(&g_beamReleaseQueue, 0, sizeof(g_beamReleaseQueue));
    g_trmCurrentFrame = 0;
}

/* 内部反初始化函数 */
void TRM_DataDeinit(void)
{
    for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
        memset(&g_txQueues[pri], 0, sizeof(TxQueue));
    }
    memset(&g_beamReleaseQueue, 0, sizeof(g_beamReleaseQueue));
    g_trmCurrentFrame = 0;
}

/**
 * @brief 获取发送队列当前数量 (所有优先级队列总和)
 * @return 发送队列当前数量
 */
uint32_t TRM_GetTxQueueCount(void)
{
    uint32_t total = 0;
    for (uint8_t pri = 0; pri < TX_QUEUE_PRIORITY_COUNT; pri++) {
        total += g_txQueues[pri].count;
    }
    return total;
}

/**
 * @brief 获取发送队列最大容量 (所有优先级队列总容量)
 * @return 发送队列最大容量
 */
uint32_t TRM_GetTxQueueCapacity(void)
{
    return TX_QUEUE_SIZE * TX_QUEUE_PRIORITY_COUNT;
}

/*==============================================================================
 * 接收数据处理实现
 *============================================================================*/

/**
 * @brief 批量处理接收的用户数据
 * @param userIndices 用户索引数组
 * @param userCount 用户数量
 * @param crcResults CRC结果数组
 * @param irqResult Driver中断结果
 * @return 0-成功, 其他-失败
 */
int TRM_ProcessRxUserDataBatch(uint8_t* userIndices, uint8_t userCount, TK8710CrcResult* crcResults, TK8710IrqResult* irqResult)
{
    /* 创建用户数据存储数组 */
    static TRM_RxUserData userStorage[128];  /* 静态存储用户数据数组 */
    TRM_RxDataList rxDataList;
    uint8_t deliverCount = 0U;
    // rxDataList.frameNo = TRM_GetCurrentFrame();  /* 获取当前系统帧号 */
    rxDataList.frameNo = TRM_GetSuperFramePosition();  /* 获取当前超帧帧号 */
    rxDataList.userCount = 0U;
    rxDataList.users = userStorage;  /* 指向用户数据数组 */
    
    TRM_LOG_DEBUG("TRM: Processing %d valid users in batch", userCount);
    
    /* 获取当前速率模式 - 直接从Driver中断结果中获取 */
    uint8_t currentRateMode = 0;
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    if (slotCfg && slotCfg->rateCount > 0 && irqResult) {
        /* 使用Driver提供的当前速率索引获取速率模式 */
        uint8_t currentRateIndex = irqResult->currentRateIndex;
        if (currentRateIndex < slotCfg->rateCount) {
            currentRateMode = slotCfg->rateModes[currentRateIndex];
            if (slotCfg->rateCount > 1) {
                TRM_LOG_DEBUG("TRM: Multi-rate mode - rateIndex=%u, rateMode=%d", 
                             currentRateIndex, currentRateMode);
            } else {
                TRM_LOG_DEBUG("TRM: Single-rate mode - rateIndex=%u, rateMode=%d", 
                             currentRateIndex, currentRateMode);
            }
        } else {
            TRM_LOG_WARN("TRM: Invalid rate index %u, using default rate mode", currentRateIndex);
            currentRateMode = slotCfg->rateModes[0];  /* 使用第一个速率模式 */
        }
    } else if (slotCfg && slotCfg->rateCount > 0) {
        /* 信号信息无效但配置有效，使用第一个速率模式 */
        TRM_LOG_DEBUG("TRM: Signal info invalid, using first configured rate mode");
        currentRateMode = slotCfg->rateModes[0];
    } else {
        /* 配置无效，使用默认速率模式 */
        TRM_LOG_WARN("TRM: Failed to get rate info from Driver, using default rate mode");
        currentRateMode = TK8710_RATE_MODE_8;  /* 默认速率模式 */
    }
    
    /* 批量处理用户数据 */
    for (uint8_t i = 0; i < userCount; i++) {
        uint8_t userIndex = userIndices[i];
        TRM_RxUserData* currentUser = &userStorage[i];
        uint8_t deliverUser = 1U;
        
        TRM_LOG_DEBUG("TRM: Processing user[%d] with CRC result", userIndex);
        
        /* 初始化用户数据结构 */
        memset(currentUser, 0, sizeof(TRM_RxUserData));
        currentUser->userId = userIndex;  /* 默认用户ID */
        currentUser->rateMode = currentRateMode;  /* 设置接收速率模式 */  // TODO: 后面id需要从获取的数据中提取
        
        /* 从接收数据中提取用户信息 - 参考test_Driver_TRM_main_3506.c:TK8710GetRxUserInfo实现 */
        uint32_t freq;
        uint32_t ahData[16];
        uint64_t pilotPower;
        TRM_BeamInfo beam;
        memset(&beam, 0, sizeof(beam));
        
        /* 调用TK8710GetRxUserInfo获取实际的用户信息 */
        int ret = TK8710GetRxUserInfo(userIndex, &freq, ahData, &pilotPower);
        if (ret != TK8710_OK) {
            /* 如果获取失败，使用默认值 */
            TRM_LOG_WARN("TRM: Failed to get RX user info for user[%d]: %d, using defaults", userIndex, ret);
            freq = 20000;  /* 默认频率 */
            for (int j = 0; j < 16; j++) {
                ahData[j] = 8192 + j;  /* 默认AH值 */
            }
            pilotPower = 1000000;  /* 默认Pilot功率 */
        }
        
        /* 使用获取到的用户信息 */
        beam.freq = freq;  /* 实际频率 */
        memcpy(beam.ahData, ahData, sizeof(beam.ahData));  /* 实际AH数据 */
        beam.pilotPower = pilotPower;  /* 实际Pilot功率 */
        
        /* 获取用户数据 */
        uint8_t* userData;
        uint16_t dataLen;
        if (TK8710GetRxUserData(userIndex, &userData, &dataLen) == TK8710_OK) {
            // TRM_LOG_DEBUG("TRM: User[%d] received %d bytes\n", userIndex, dataLen);
            
            /* 使用新的MAC协议解析辅助函数提取用户ID和QoS信息 */
            if (dataLen >= 4) {
                /* 尝试解析为标准MAC协议帧 */
                uint32_t extractedUserId;
                if (TRM_ExtractUserIdFromMacFrame(userData, dataLen, &extractedUserId) == 0) {
                    /* 成功从MAC帧中提取用户ID */
                    beam.userId = extractedUserId;
                } else {
                    /* 无法解析为标准MAC帧，使用前4字节作为用户ID */
                    beam.userId = (userData[0] << 24) | (userData[1] << 16) | (userData[2] << 8) | userData[3];
                    TRM_LOG_DEBUG("TRM: Extracted user ID from raw data (non-MAC frame): 0x%08X", beam.userId);
                }
            } else {
                /* 数据长度不足4字节，使用用户索引作为ID */
                beam.userId = userIndex;
                TRM_LOG_WARN("TRM: Data length %d < 4, using user index %d", dataLen, beam.userId);
            }
            
            beam.valid = 1;
            beam.timestamp = TK8710GetTickMs();
            
            /* 填充用户数据 */
            currentUser->userId = beam.userId;
            currentUser->data = userData;
            currentUser->dataLen = dataLen;
            
            /* 获取信号质量信息 - 参考test_Driver_TRM_main_3506.c:TK8710GetRxUserSignalQuality实现 */
            uint32_t rssi, freqSignal;
            uint8_t snr;
            if (TK8710GetRxUserSignalQuality(userIndex, &rssi, &snr, &freqSignal) == TK8710_OK) {
                /* SNR转换：uint8_t最大255，直接除以4 */
                uint8_t snrValue = snr / 4;
                
                /* RSSI转换：11位有符号数，需要转换为有符号值 */
                uint32_t rssiRaw = rssi;
                int16_t rssiValue = (int16_t)(rssiRaw - 2048) / 4;
                
                /* 频率转换：26-bit格式转换为实际频率Hz */
                uint32_t freq26 = freqSignal & 0x03FFFFFF;  /* 取26位 */
                int32_t freqValue = freq26 > (1<<25) ? (int32_t)(freq26 - (1<<26)) : (int32_t)freq26;
                if(snrValue > 28){/* 限制SNR值在0-25范围内 */
                    snrValue = 28;
                }
                /* 设置信号质量信息到currentUser */
                currentUser->rssi = rssiValue;               /* 设置实际RSSI值 (int16) */
                currentUser->snr = snrValue;                 /* 设置SNR值 (uint8) */
                currentUser->freq = freqValue;               /* 设置频率值 (int32) */
                if (beam.freq == 0U && freqSignal != 0U) {
                    beam.freq = TRM_EncodeFreqForTxCache(freqSignal);
                }
                
                // TRM_LOG_DEBUG("TRM: User[%d] Signal: SNR=%d, RSSI=%d, Freq=%d Hz", 
                //              userIndex, snrValue, rssiValue, freqValue/128);
            } else {
                currentUser->rssi = 0;    /* 获取失败时使用默认值 */
                currentUser->snr = 0;      /* SNR默认值 */
                currentUser->freq = 0;     /* 频率默认值 */
                TRM_LOG_WARN("TRM: Failed to get signal info for user[%d]", userIndex);
            }
            
            currentUser->beam = beam;  /* 设置波束信息，包含timestamp */
            deliverUser = TRM_SatelliteShouldDeliverRxUser(userData, dataLen);

            {
                int beamStoreRet = TRM_OK;
                uint8_t storedBeam = 0U;

                if (TRM_SatelliteAllowRxBeamStore(userData, dataLen)) {
                    TRM_SatelliteBeforeRxBeamStore(beam.userId, userData, dataLen);
                    beamStoreRet = TRM_SetBeamInfo(beam.userId, &beam);
                    storedBeam = (beamStoreRet == TRM_OK) ? 1U : 0U;
                }

                if (beamStoreRet == TRM_OK) {
                    if (storedBeam != 0U) {
                        TRM_SatelliteAfterRxBeamStore(beam.userId, userData, dataLen);
                    }
                    TRM_SatelliteProcessRxUser(currentUser);
                    if (storedBeam != 0U &&
                        !TRM_SatelliteKeepRxBeam(userData, dataLen)) {
                        TRM_ScheduleBeamRamRelease(beam.userId, 30U);
                    }
                } else {
                    TRM_LOG_WARN("TRM: Failed to store beam info for user ID=0x%08X, error=%d",
                                 beam.userId, beamStoreRet);
                }
            }
        } else {
            /* 获取数据失败，设置默认值 */
            currentUser->userId = beam.userId;
            currentUser->data = NULL;
            currentUser->dataLen = 0;
            currentUser->rssi = 0;
            currentUser->snr = 0;
            currentUser->freq = 0;
            currentUser->beam = beam;
            TRM_LOG_WARN("TRM: Failed to get RX data for user[%d]", userIndex);
        }

        if (deliverUser != 0U) {
            if (deliverCount != i) {
                userStorage[deliverCount] = *currentUser;
            }
            deliverCount++;
        } else {
            TRM_LOG_INFO("TRM: RX user filtered from upper callback userId=0x%08X",
                         currentUser->userId);
        }
    }

    rxDataList.userCount = deliverCount;
    
    /* 一次性调用接收回调，处理所有用户 */
    TrmContext* ctx = TRM_GetContext();
    if (ctx && ctx->config.callbacks.onRxData != NULL && deliverCount > 0U) {
        if (deliverCount > 0U) {
            TRM_LOG_INFO("TRM: RX users - rateMode=%u, systemFrame=%u, userCount=%u",
                         currentRateMode, g_trmCurrentFrame, deliverCount);
        }
        TRM_LOG_DEBUG("TRM: Calling onRxData callback for %d users", deliverCount);
        ctx->config.callbacks.onRxData(&rxDataList);
    }
    
    /* 批量释放接收数据Buffer */
    for (uint8_t idx = 0; idx < userCount; idx++) {
        uint8_t userIndex = userIndices[idx];
        TK8710ReleaseRxData(userIndex);
    }
    
    return TRM_OK;
}
