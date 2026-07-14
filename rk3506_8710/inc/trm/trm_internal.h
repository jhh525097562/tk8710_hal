/**
 * @file trm_internal.h
 * @brief TRM (Transmission Resource Management) 内部函数头文件
 * @note 此文件包含TRM内部使用的函数，不建议应用层直接调用
 * 
 * 内容分类:
 *   - 波束管理内部函数
 *   - 系统控制内部函数
 *   - 调试和测试接口
 *   - 内部工具函数
 * 
 * 建议:
 *   - 应用层只需包含 trm_api.h
 *   - 需要内部功能和调试时包含此文件
 *   - 包含 trm.h 可获得所有功能 (向后兼容)
 */

#ifndef TRM_INTERNAL_H
#define TRM_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include "trm_api.h"
#include "trm_beam.h"
#include "trm_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 内部错误码定义
 * ============================================================================= */
#define TRM_ERR_INTERNAL        (-9)
#define TRM_ERR_INVALID_STATE   (-10)

/* =============================================================================
 * 内部类型定义
 * ============================================================================= */

/* 回调函数集合 */
typedef struct {
    TRM_OnRxData      onRxData;
    TRM_OnTxComplete  onTxComplete;
} TRM_Callbacks;

/* 时隙配置 */
typedef struct {
    uint8_t bcnSlotCount;       /* BCN时隙数 */
    uint8_t brdSlotCount;       /* 广播时隙数 */
    uint8_t ulSlotCount;        /* 上行时隙数 */
    uint8_t dlSlotCount;        /* 下行时隙数 */
} TRM_SlotConfig;

/* 广播接收数据 */
typedef struct {
    uint8_t  brdIndex;          /* 广播索引 */
    uint16_t dataLen;           /* 数据长度 */
    uint16_t reserved;
    uint8_t* data;              /* 数据指针 */
} TRM_RxBrdData;

/* TRM上下文 */
typedef struct {
    TrmState state;
    TRM_InitConfig config;
    TRM_Stats stats;
} TrmContext;

/* =============================================================================
 * 全局变量声明
 * ============================================================================= */

/* 当前系统帧号和最大帧数 - 由trm_core.c管理 */
extern uint32_t g_trmCurrentFrame;
extern uint32_t g_trmMaxFrameCount;

/* =============================================================================
 * 波束管理内部函数
 * ============================================================================= */

/**
 * @brief 调度波束RAM释放 (内部函数)
 * @param userId 用户ID
 * @param delayFrames 延迟帧数
 */
void TRM_ScheduleBeamRamRelease(uint32_t userId, uint32_t delayFrames);

/**
 * @brief 处理波束RAM释放 (内部函数)
 */
void TRM_ProcessBeamRamReleases(void);

/* =============================================================================
 * 系统控制内部函数
 * ============================================================================= */

/**
 * @brief 重置TRM系统 (内部函数)
 * @return TRM_OK成功，其他失败
 */
int TRM_Reset(void);

/**
 * @brief 获取TRM上下文 (内部函数)
 * @return TRM上下文指针
 */
TrmContext* TRM_GetContext(void);

/* =============================================================================
 * 数据管理内部函数
 * ============================================================================= */

/**
 * @brief 发送用户数据(支持单速率和多速率模式) - 内部函数
 * @param userId 用户ID
 * @param data 数据指针
 * @param len 数据长度
 * @param txPower 发送功率
 * @param frameNo 帧号
 * @param targetRateMode 目标速率模式
 * @param BeamType 波束类型 (0=广播波束, 1=指定波束)
 * @return TRM_OK成功，其他失败
 */
int TRM_SendData(uint32_t userId, const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo, uint8_t targetRateMode, uint8_t BeamType);


/* =============================================================================
 * 内部工具函数
 * ============================================================================= */

/**
 * @brief 设置当前系统帧号
 * @param frameNo 帧号
 */
void TRM_SetCurrentFrame(uint32_t frameNo);

/**
 * @brief 设置最大帧数
 * @param maxCount 最大帧数
 */
void TRM_SetMaxFrameCount(uint32_t maxCount);

/**
 * @brief 获取超帧中的帧号位置
 * @return 超帧中的帧号位置 (0 到 maxFrameCount-1)
 */
uint32_t TRM_GetSuperFramePosition(void);

/* =============================================================================
 * 广播管理内部API
 * ============================================================================= */

/**
 * @brief 广播发送管理函数
 * @note 在每个时隙开始时调用，负责广播数据的发送管理
 * @return TRM_OK成功，其他失败
 */
int TRM_ManageBroadcast(void);

/**
 * @brief 清除广播数据
 * @return TRM_OK成功
 */
int TRM_ClearBroadcast(void);

/**
 * @brief 获取广播状态
 * @param hasPayload 输出是否包含payload
 * @param valid 输出是否有效
 * @return TRM_OK成功
 */
int TRM_GetBroadcastStatus(uint8_t* hasPayload, uint8_t* valid);

/**
 * @brief 打印时隙计算结果 (内部函数)
 * @param output 计算结果
 */
void trm_print_slot_calc_result(const TRM_SlotCalcOutput* output);

/* =============================================================================
 * Driver回调管理API
 * =============================================================================
 */

/**
 * @brief 注册TRM到Driver的回调函数
 * @return TRM_OK成功，其他失败
 * @note 此函数由TRM内部调用，建立TRM与Driver层的通信通道
 */
int TRM_RegisterDriverCallbacks(void);

/* =============================================================================
 * 调试和测试接口
 * ============================================================================= */

/**
 * @brief TRM发送验证器接收数据处理 (测试接口)
 * @param rxDataList 接收数据列表
 * @return TRM_OK成功，其他失败
 */
int TRM_TxValidatorOnRxData(const TRM_RxDataList* rxDataList);

#ifdef __cplusplus
}
#endif

#endif /* TRM_INTERNAL_H */
