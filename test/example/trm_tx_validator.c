/**
 * @file trm_tx_validator.c
 * @brief TRM发送逻辑验证模块实现
 */

#include "trm_tx_validator.h"
#include "trm/trm_log.h"
#include "hal_api.h"
#include "phy/phy_api.h"
#include "driver/tk8710_internal.h"
#include "../../port/tk8710_hal.h"
#include <string.h>

/* ============================================================================
 * 内部状态
 * ============================================================================
 */

static TRM_TxValidatorConfig g_validatorConfig;
static TRM_TxValidatorStats g_validatorStats;
static bool g_validatorInitialized = false;
static uint32_t g_lastPeriodicFrame = 0;

#define TRM_TX_VALIDATOR_MAX_DATA_LEN 520

/* ============================================================================
 * 内部函数
 * ============================================================================
 */

/**
 * @brief 生成测试数据
 * @param buffer 数据缓冲区
 * @param len 数据长度
 * @param userId 用户ID
 * @param frameNo 帧号
 */
static void GenerateTestData(uint8_t* buffer, uint16_t len, uint32_t userId, uint32_t frameNo)
{
    if (buffer == NULL || len == 0) return;
    
    /* 使用用户ID和帧号生成伪随机数据 */
    uint32_t seed = userId ^ frameNo;
    for (uint16_t i = 12; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buffer[i] = (uint8_t)(seed >> 24);
    }
    
    // /* 添加标识头 */
    // if (len >= 4) {
    //     buffer[0] = 0xAA;  /* 验证数据标识 */
    //     buffer[1] = 0xBB;
    //     buffer[2] = (uint8_t)(userId >> 24);
    //     buffer[3] = (uint8_t)(frameNo & 0xFF);
    // }
}

/**
 * @brief 生成应答用户ID
 * @param rxUserId 接收到的用户ID（从数据前4字节提取）
 * @return 应答用户ID
 */
static uint32_t GenerateResponseUserId(uint32_t rxUserId)
{
    /* 直接使用从数据中提取的用户ID */
    return rxUserId;
}

/**
 * @brief 执行发送操作
 * @param userId 用户ID
 * @param data 数据
 * @param len 数据长度
 * @param frameNo 目标帧号
 * @param rateMode 目标速率模式 (0=使用帧号, >0=使用速率模式)
 * @return TRM_OK成功，其他失败
 */
static int ExecuteSend(uint32_t userId, const uint8_t* data, uint16_t len, uint32_t frameNo, uint8_t rateMode)
{
    int ret;
    
    /* 根据速率模式选择发送接口 */
    if (rateMode == 0) {
        /* 使用帧号匹配（原有方式） */
        ret = TRM_SetTxData(TK8710_DOWNLINK_B, userId, data, len, g_validatorConfig.txPower, frameNo, 0, TK8710_DATA_TYPE_DED);
        TRM_LOG_DEBUG("验证发送(帧号模式): 用户ID=0x%08X, 帧号=%u, 长度=%d", userId, frameNo, len);
    } else {
        /* 使用速率模式匹配（新方式） */
        ret = TRM_SetTxData(TK8710_DOWNLINK_B, userId, data, len, g_validatorConfig.txPower, frameNo, rateMode, TK8710_DATA_TYPE_DED);
        TRM_LOG_DEBUG("验证发送(速率模式): 用户ID=0x%08X, 帧号=%u, 长度=%d, 速率=%d", userId, frameNo, len, rateMode);
    }
    
    g_validatorStats.totalTriggerCount++;
    
    if (ret == TRM_OK) {
        g_validatorStats.successSendCount++;
        g_validatorStats.lastErrorCode = 0;
    } else {
        g_validatorStats.failedSendCount++;
        g_validatorStats.lastErrorCode = ret;
        TRM_LOG_WARN("验证发送失败: 用户ID=0x%08X, 错误码=%d", userId, ret);
    }
    
    return ret;
}

/* ============================================================================
 * 公共接口实现
 * ============================================================================
 */

int TRM_TxValidatorInit(const TRM_TxValidatorConfig* config)
{
    if (config == NULL) {
        TRM_LOG_ERROR("验证器初始化失败: 配置为空");
        return TRM_ERR_PARAM;
    }
    
    /* 复制配置 */
    memcpy(&g_validatorConfig, config, sizeof(TRM_TxValidatorConfig));
    
    /* 初始化统计信息 */
    memset(&g_validatorStats, 0, sizeof(TRM_TxValidatorStats));
    g_validatorStats.currentUserId = config->userIdBase;
    
    /* 重置内部状态 */
    g_lastPeriodicFrame = 0;
    g_validatorInitialized = true;
    
    TRM_LOG_INFO("TRM发送验证器初始化成功");
    TRM_LOG_INFO("配置: 功率=%d, 帧偏移=%d, 用户ID基数=0x%08X", 
                 config->txPower, config->frameOffset, config->userIdBase);
    TRM_LOG_INFO("验证器将使用从接收数据前4字节提取的用户ID");
    
    return TRM_OK;
}

void TRM_TxValidatorDeinit(void)
{
    g_validatorInitialized = false;
    memset(&g_validatorStats, 0, sizeof(TRM_TxValidatorStats));
    TRM_LOG_INFO("TRM发送验证器已反初始化");
}

int TRM_TxValidatorOnRxData(const TRM_RxDataList* rxDataList)
{
    if (!g_validatorInitialized) {
        TRM_LOG_WARN("验证器未初始化");
        return TRM_ERR_NOT_INIT;
    }
    
    if (rxDataList == NULL || rxDataList->userCount == 0) {
        return TRM_OK; /* 无数据，直接返回 */
    }
    
    /* 检查系统是否为多速率模式 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint8_t isMultiRate = (slotCfg && slotCfg->rateCount > 1);
    
    if (isMultiRate) {
        TRM_LOG_DEBUG("TRM验证器: 多速率模式，速率数量=%u", slotCfg->rateCount);
        
        /* 多速率模式：只为与接收速率模式匹配的目标速率模式生成发送数据 */
        for (uint8_t i = 0; i < rxDataList->userCount; i++) {
            TRM_RxUserData* user = &rxDataList->users[i];
            
            /* 生成应答数据 - 使用配置的数据长度 */
            uint8_t respData[TRM_TX_VALIDATOR_MAX_DATA_LEN];  /* Downlink test buffer */
            // uint16_t dataLen = g_validatorConfig.responseDataLength;
            uint16_t dataLen = user->dataLen;
            if (dataLen > TRM_TX_VALIDATOR_MAX_DATA_LEN) {
                dataLen = TRM_TX_VALIDATOR_MAX_DATA_LEN;
            }
            /* 限制最大长度 */
            // if (dataLen > 64) dataLen = 64;
            // if (dataLen == 0) dataLen = 12;  /* 默认长度 */
            
            GenerateTestData(respData, dataLen, user->userId, rxDataList->frameNo);
            
            /* 如果接收到了数据，复制部分数据作为应答 */
            if (user->data != NULL && user->dataLen > 0) {
                uint16_t copyLen = (user->dataLen > 12) ? 12 : user->dataLen;
                if (copyLen > dataLen) {
                    copyLen = dataLen;
                }
                memcpy(&respData[0], user->data, copyLen);
            }
            
            /* 计算应答用户ID和目标帧 */
            uint32_t respUserId = GenerateResponseUserId(user->userId);

            uint32_t targetFrame;
            if(g_trmMaxFrameCount==1){
                targetFrame = 0xFF;
            }else{
                targetFrame = (rxDataList->frameNo + g_validatorConfig.frameOffset)%g_trmMaxFrameCount;
            }
            
            /* 执行发送 - 使用接收到的速率模式作为下行速率模式 */
            ExecuteSend(respUserId, respData, dataLen, targetFrame, user->rateMode);
            
            TRM_LOG_DEBUG("TRM验证器: 多速率应答 - 用户ID=0x%08X, 上行速率=%d, 下行速率=%d", 
                         user->userId, user->rateMode, user->rateMode);
        }
    } else {
        TRM_LOG_DEBUG("TRM验证器: 单速率模式");
        
        /* 单速率模式：原有逻辑 */
        if (g_validatorConfig.enableAutoResponse) {
            for (uint8_t i = 0; i < rxDataList->userCount; i++) {
                TRM_RxUserData* user = &rxDataList->users[i];
                
                /* 生成应答数据 - 使用配置的数据长度 */
                uint8_t respData[TRM_TX_VALIDATOR_MAX_DATA_LEN];  /* Downlink test buffer */
                // uint16_t dataLen = g_validatorConfig.responseDataLength;
                uint16_t dataLen = user->dataLen;
                if (dataLen > TRM_TX_VALIDATOR_MAX_DATA_LEN) {
                    dataLen = TRM_TX_VALIDATOR_MAX_DATA_LEN;
                }
                /* 限制最大长度 */
                // if (dataLen > 64) dataLen = 64;
                // if (dataLen == 0) dataLen = 12;  /* 默认长度 */
                
                GenerateTestData(respData, dataLen, user->userId, rxDataList->frameNo);
                
                /* 如果接收到了数据，复制部分数据作为应答 */
                if (user->data != NULL && user->dataLen > 0) {
                    uint16_t copyLen = (user->dataLen > 12) ? 12 : user->dataLen;
                    if (copyLen > dataLen) {
                        copyLen = dataLen;
                    }
                    memcpy(&respData[0], user->data, copyLen);
                }
                
                /* 计算应答用户ID和目标帧 */
                uint32_t respUserId = GenerateResponseUserId(user->userId);
                // uint32_t targetFrame = rxDataList->frameNo + g_validatorConfig.frameOffset;
                uint32_t targetFrame = 0xFF;
                /* 执行发送 - 使用帧号模式 */
                ExecuteSend(respUserId, respData, dataLen, targetFrame, user->rateMode);
            }
        }
    }
    
    /* 周期测试模式 - 保持原有逻辑 */
    if (g_validatorConfig.enablePeriodicTest) {
        uint32_t frameDiff = rxDataList->frameNo - g_lastPeriodicFrame;
        if (frameDiff >= g_validatorConfig.periodicIntervalFrames) {
            
            /* 生成周期测试数据 */
            uint8_t periodicData[32];
            GenerateTestData(periodicData, sizeof(periodicData), 
                           g_validatorStats.currentUserId, rxDataList->frameNo);
            
            uint32_t targetFrame = rxDataList->frameNo + g_validatorConfig.frameOffset;
            
            /* 执行发送 - 根据系统模式选择发送方式 */
            uint8_t sendRateMode = 0;
            if (isMultiRate && slotCfg) {
                /* 多速率模式：使用第一个速率模式 */
                sendRateMode = slotCfg->rateModes[0];
            }
            
            ExecuteSend(g_validatorStats.currentUserId, periodicData, 
                       sizeof(periodicData), targetFrame, sendRateMode);
            
            /* 更新用户ID和最后发送帧 */
            g_validatorStats.currentUserId++;
            g_lastPeriodicFrame = rxDataList->frameNo;
        }
    }
    
    return TRM_OK;
}

int TRM_TxValidatorTriggerTest(const uint8_t* testData, uint16_t dataLen, uint32_t targetUserId)
{
    if (!g_validatorInitialized) {
        TRM_LOG_WARN("验证器未初始化");
        return TRM_ERR_NOT_INIT;
    }
    
    /* 获取当前帧号 */
    uint32_t currentFrame = TRM_GetCurrentFrame();
    
    /* 如果未指定用户ID，使用当前ID */
    uint32_t userId = (targetUserId == 0) ? g_validatorStats.currentUserId : targetUserId;
    
    /* 如果未提供测试数据，生成默认数据 */
    uint8_t defaultData[16];
    const uint8_t* dataPtr = testData;
    uint16_t actualLen = dataLen;
    
    if (testData == NULL || dataLen == 0) {
        GenerateTestData(defaultData, sizeof(defaultData), userId, currentFrame);
        dataPtr = defaultData;
        actualLen = sizeof(defaultData);
    }
    
    /* 计算目标帧 */
    uint32_t targetFrame = currentFrame + g_validatorConfig.frameOffset;
    
    /* 检查系统是否为多速率模式 */
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint8_t sendRateMode = 0;
    if (slotCfg && slotCfg->rateCount > 1) {
        /* 多速率模式：使用第一个速率模式 */
        sendRateMode = slotCfg->rateModes[0];
    }
    
    /* 执行发送 */
    int ret = ExecuteSend(userId, dataPtr, actualLen, targetFrame, sendRateMode);
    
    /* 更新波束信息的时间戳，防止过期 */
    TRM_BeamInfo beam;
    if (TRM_GetBeamInfo(userId, &beam) == TRM_OK) {
        beam.timestamp = (uint32_t)(TK8710GetTimeUs() / 1000);
        TRM_SetBeamInfo(userId, &beam);
        TRM_LOG_DEBUG("验证器: 更新波束时间戳 - 用户ID=0x%08X", userId);
    }
    
    /* 更新当前用户ID */
    if (targetUserId == 0) {
        g_validatorStats.currentUserId++;
    }
    
    return ret;
}

int TRM_TxValidatorGetStats(TRM_TxValidatorStats* stats)
{
    if (!g_validatorInitialized || stats == NULL) {
        return TRM_ERR_PARAM;
    }
    
    memcpy(stats, &g_validatorStats, sizeof(TRM_TxValidatorStats));
    return TRM_OK;
}

void TRM_TxValidatorResetStats(void)
{
    uint32_t currentUserId = g_validatorStats.currentUserId;
    memset(&g_validatorStats, 0, sizeof(TRM_TxValidatorStats));
    g_validatorStats.currentUserId = currentUserId;
    TRM_LOG_INFO("验证器统计信息已重置");
}

int TRM_TxValidatorSetConfig(const TRM_TxValidatorConfig* config)
{
    if (!g_validatorInitialized || config == NULL) {
        return TRM_ERR_PARAM;
    }
    
    memcpy(&g_validatorConfig, config, sizeof(TRM_TxValidatorConfig));
    TRM_LOG_INFO("验证器配置已更新");
    return TRM_OK;
}

int TRM_TxValidatorGetConfig(TRM_TxValidatorConfig* config)
{
    if (!g_validatorInitialized || config == NULL) {
        return TRM_ERR_PARAM;
    }
    
    memcpy(config, &g_validatorConfig, sizeof(TRM_TxValidatorConfig));
    return TRM_OK;
}
