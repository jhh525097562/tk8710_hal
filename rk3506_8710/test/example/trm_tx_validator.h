/**
 * @file trm_tx_validator.h
 * @brief TRM发送逻辑验证模块
 * @note 提供可移植的发送验证功能，支持多种触发方式
 */

#ifndef TRM_TX_VALIDATOR_H
#define TRM_TX_VALIDATOR_H

#include "trm/trm_api.h"
#include "trm/trm_internal.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 配置参数
 * ============================================================================
 */

/** 最大验证用户数量 */
#define TX_VALIDATOR_MAX_USERS    16

/** 默认发送功率 */
#define TX_VALIDATOR_DEFAULT_POWER  20

/** 默认帧偏移（接收帧 + 偏移 = 发送帧） */
#define TX_VALIDATOR_DEFAULT_FRAME_OFFSET  3

/* ============================================================================
 * 数据结构
 * ============================================================================
 */

/**
 * @brief 验证配置结构
 */
typedef struct {
    uint8_t txPower;                              /**< 发射功率 */
    uint8_t frameOffset;                          /**< 帧偏移 */
    uint32_t userIdBase;                          /**< 用户ID基数 */
    bool enableAutoResponse;                      /**< 是否启用自动应答 */
    bool enablePeriodicTest;                      /**< 是否启用周期测试 */
    uint16_t periodicIntervalFrames;              /**< 周期测试间隔（帧数） */
    uint16_t responseDataLength;                  /**< 应答数据长度 */
} TRM_TxValidatorConfig;

/**
 * @brief 验证统计信息
 */
typedef struct {
    uint32_t totalTriggerCount;                   /**< 总触发次数 */
    uint32_t successSendCount;                    /**< 成功发送次数 */
    uint32_t failedSendCount;                     /**< 失败发送次数 */
    uint32_t lastErrorCode;                       /**< 最后一次错误码 */
    uint32_t currentUserId;                       /**< 当前用户ID */
} TRM_TxValidatorStats;

/* ============================================================================
 * 公共接口
 * ============================================================================
 */

/**
 * @brief 初始化发送验证器
 * @param config 验证配置
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorInit(const TRM_TxValidatorConfig* config);

/**
 * @brief 反初始化发送验证器
 */
void TRM_TxValidatorDeinit(void);

/**
 * @brief 在接收回调中触发发送验证
 * @param rxDataList 接收数据列表
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorOnRxData(const TRM_RxDataList* rxDataList);

/**
 * @brief 手动触发发送测试
 * @param testData 测试数据
 * @param dataLen 数据长度
 * @param targetUserId 目标用户ID（0表示自动生成）
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorTriggerTest(const uint8_t* testData, uint16_t dataLen, uint32_t targetUserId);

/**
 * @brief 获取验证统计信息
 * @param stats 输出统计信息
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorGetStats(TRM_TxValidatorStats* stats);

/**
 * @brief 重置统计信息
 */
void TRM_TxValidatorResetStats(void);

/**
 * @brief 设置配置参数
 * @param config 新的配置
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorSetConfig(const TRM_TxValidatorConfig* config);

/**
 * @brief 获取当前配置
 * @param config 输出当前配置
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorGetConfig(TRM_TxValidatorConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* TRM_TX_VALIDATOR_H */
