/**
 * @file trm_beam.h
 * @brief TRM波束管理接口
 */

#ifndef TRM_BEAM_H
#define TRM_BEAM_H

#include <stdint.h>
#include "trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 波束管理接口
 *============================================================================*/

/**
 * @brief 设置波束信息
 * @param userId 用户ID
 * @param beamInfo 波束信息
 * @return 0-成功, 非0-失败
 */
int TRM_SetBeamInfo(uint32_t userId, const TRM_BeamInfo* beamInfo);

/**
 * @brief 获取波束信息
 * @param userId 用户ID
 * @param beamInfo 输出波束信息
 * @return 0-成功, 非0-失败
 */
int TRM_GetBeamInfo(uint32_t userId, TRM_BeamInfo* beamInfo);

/**
 * @brief 清除波束信息
 * @param userId 用户ID (0xFFFFFFFF表示清除所有)
 * @return 0-成功, 非0-失败
 */
int TRM_ClearBeamInfo(uint32_t userId);

/**
 * @brief 设置波束超时时间
 * @param timeoutMs 超时时间(毫秒)
 * @return 0-成功, 非0-失败
 */
int TRM_SetBeamTimeout(uint32_t timeoutMs);

/*==============================================================================
 * 内部函数（供TRM核心模块使用）
 *============================================================================*/

/**
 * @brief 波束管理初始化
 * @param maxUsers 最大用户数
 * @param timeoutMs 超时时间(毫秒)
 */
void TRM_BeamInit(uint32_t maxUsers, uint32_t timeoutMs);

/**
 * @brief 波束管理反初始化
 */
void TRM_BeamDeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TRM_BEAM_H */
