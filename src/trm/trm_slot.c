/**
 * @file trm_slot.c
 * @brief TRM时隙计算器实现
 * @note 基于8710_HAL用户指南v1.0 7.2.4章节实现
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "trm_api.h"
#include "trm_internal.h"
#include "trm/trm_log.h"

/*============================================================================
 * TRM时隙计算器 - 基于8710_HAL用户指南v1.0 7.2.4章节
 *============================================================================*/

/* 各模式基础时隙长度(us)，包块数=1时的值 */
static const uint32_t g_bcnSlotLen[] = {
    [5] = 69583,  [6] = 36340,  [7] = 19129,  [8] = 10732,
    [9] = 6510,   [10] = 6510,  [11] = 6510,  [18] = 6510
};

static const uint32_t g_brdBaseBody[] = {
    [5] = 131072, [6] = 65536, [7] = 32768,  [8] = 16384,
    [9] = 8192,  [10] = 4096, [11] = 2048,  [18] = 2048
};

static const uint32_t g_brdBaseGap[] = {
    // [5] = 13000,  [6] = 14000,  [7] = 7000,   [8] = 3600,
    // [9] = 1900,   [10] = 1200,  [11] = 800,   [18] = 800
    [5] = 21492,  [6] = 19728,  [7] = 12000,   [8] = 5600,
    [9] = 2800,   [10] = 1400,  [11] = 800,   [18] = 800
};

static const uint32_t g_ulBaseBody[] = {
    [5] = 131072, [6] = 65536, [7] = 32768,  [8] = 16384,
    [9] = 8192,  [10] = 4096, [11] = 2048,  [18] = 2048
};

static const uint32_t g_ulBaseGap[] = {
    [5] = 21492,  [6] = 19728,  [7] = 12000,   [8] = 5600,
    [9] = 2800,   [10] = 1400,  [11] = 800,   [18] = 800
};

static const uint32_t g_dlBaseBody[] = {
    [5] = 131072, [6] = 65536, [7] = 32768,  [8] = 16384,
    [9] = 8192,  [10] = 4096, [11] = 2048,  [18] = 2048
};

static const uint32_t g_dlBaseGap[] = {
    [5] = 21492,  [6] = 19728,  [7] = 12000,   [8] = 5600,
    [9] = 2800,   [10] = 1400,  [11] = 800,   [18] = 800
};

#define INTERVAL_US     1024
#define ONE_SECOND_US   1000000

/**
 * @brief 求最大公约数
 * @param a 第一个数
 * @param b 第二个数
 * @return 最大公约数
 */
static uint32_t __attribute__((unused)) trm_gcd(uint32_t a, uint32_t b)
{
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/**
 * @brief TRM时隙计算器
 * @param input 输入参数
 * @param output 输出结果
 * @return 0-成功, 非0-失败
 * @note 基于8710_HAL用户指南v1.0 7.2.4章节实现
 */
int trm_calc_slot_config(const TRM_SlotCalcInput* input, TRM_SlotCalcOutput* output)
{
    if (!input || !output) {
        TRM_LOG_ERROR("Invalid parameters: input=%p, output=%p", input, output);
        return -1;
    }
    
    uint8_t mode = input->rateMode;
    if ((mode < 5 || mode > 11) && mode != 18) {
        TRM_LOG_ERROR("Invalid rate mode: %d (supported: 5-11, 18)", mode);
        return -1;
    }
  /* 参数检查 - 允许包块数为0 */
    if (input->ulBlockNum > 16 || input->dlBlockNum > 16 || input->brdBlockNum > 16) {
        TRM_LOG_ERROR("Invalid block numbers: brd=%d, ul=%d, dl=%d (must be <= 10)", 
                     input->brdBlockNum, input->ulBlockNum, input->dlBlockNum);
        return -1;
    }
    
    TRM_LOG_INFO("Calculating slot config: mode=%d, ulBlocks=%d, dlBlocks=%d", 
                mode, input->ulBlockNum, input->dlBlockNum);
    
    TRM_LOG_INFO("MinGap position config: BCN=%d, BRD=%d, UL=%d, DL=%d", 
                input->minGapPos[0], input->minGapPos[1], input->minGapPos[2], input->minGapPos[3]);
    
    /* 计算各时隙长度 */
    output->bcnSlotLen = g_bcnSlotLen[mode];
    /* 初始间隔 */
    output->bcnGap = 0;
    output->brdGap = g_brdBaseGap[mode];
    output->ulGap  = g_ulBaseGap[mode];
    output->dlGap  = g_dlBaseGap[mode];
    /* 如果包块数为0，对应时隙长度为0 */
    if (input->brdBlockNum == 0) {
        output->brdSlotLen = 0;
        output->brdGap = 0;
    } else {
        output->brdSlotLen = INTERVAL_US + g_brdBaseBody[mode] * (input->brdBlockNum * 2 + 1) + g_brdBaseGap[mode];
    }
    
    if (input->ulBlockNum == 0) {
        output->ulSlotLen = 0;
        output->ulGap = 0;
    } else {
        output->ulSlotLen = INTERVAL_US + g_ulBaseBody[mode] * (input->ulBlockNum * 2 + 1) + g_ulBaseGap[mode];
    }
    
    if (input->dlBlockNum == 0) {
        output->dlSlotLen = 0;
        output->dlGap = 0;
    } else {
        output->dlSlotLen = INTERVAL_US + g_dlBaseBody[mode] * (input->dlBlockNum * 2 + 1) + g_dlBaseGap[mode];
    }
    
    /* 计算原始帧周期 */
    uint32_t rawPeriod = output->bcnSlotLen + output->brdSlotLen + 
                         output->ulSlotLen + output->dlSlotLen;
    
    TRM_LOG_INFO("Raw frame period: %u us (BCN:%u + BRD:%u + UL:%u + DL:%u)", 
                rawPeriod, output->bcnSlotLen, output->brdSlotLen, 
                output->ulSlotLen, output->dlSlotLen);
    
    /* 新算法：严格满足 framePeriod * frameCount = M秒 */
    uint32_t bestPeriod = 0;
    uint32_t bestCount = 0;
    uint32_t minGap = UINT32_MAX;
    
    // 遍历所有可能的M和N组合，寻找最小间隔
    for (uint32_t M = 1; M <= 10; M++) {
        uint32_t totalUs = M * ONE_SECOND_US;
        
        // 寻找所有能整除totalUs的N
        for (uint32_t N = 1; N <= 64; N++) {
            if (totalUs % N != 0) continue; // N必须能整除totalUs
            
            uint32_t candidatePeriod = totalUs / N;
            
            // 只考虑candidatePeriod >= rawPeriod的情况
            if (candidatePeriod >= rawPeriod) {
                uint32_t gap = candidatePeriod - rawPeriod;
                
                // 优先选择间隔最小的解，且N必须是superFrameNum的整数倍
                if (gap < minGap && (N % input->superFrameNum == 0)) {
                // if ((N % input->superFrameNum == 0)) {
                    minGap = gap;
                    bestPeriod = candidatePeriod;
                    bestCount = N;
                    
                    TRM_LOG_INFO("Found better solution: M=%ds, N=%u, gap=%u us, period=%u us", 
                           M, N, gap, candidatePeriod);
                    
                    // 如果gap为0，这是最优解，直接退出
                    if (gap == 0) {
                        goto found_solution;
                    }
                }
            }
        }
    }
    
found_solution:
    if (bestPeriod == 0) {
        // 兜底：使用原始周期
        bestPeriod = rawPeriod;
        bestCount = 1;
        TRM_LOG_INFO("No solution found, using raw period");
    }
    
    /* 根据输入参数配置minGap位置 */
    if (input->minGapPos[0]) {
        output->bcnGap = output->bcnGap + minGap;
        output->bcnSlotLen = output->bcnSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to BCN gap", minGap);
    }
    if (input->minGapPos[1]) {
        output->brdGap = output->brdGap + minGap;
        output->brdSlotLen = output->brdSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to BRD gap", minGap);
    }
    if (input->minGapPos[2]) {
        output->ulGap = output->ulGap + minGap;
        output->ulSlotLen = output->ulSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to UL gap", minGap);
    }
    if (input->minGapPos[3]) {
        output->dlGap = output->dlGap + minGap;
        output->dlSlotLen = output->dlSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to DL gap", minGap);
    }
    output->framePeriod = bestPeriod;
    output->frameCount = bestCount;
    TRM_LOG_INFO("Slot calculation completed:");
    TRM_LOG_INFO("  Frame period: %u us, Frame count: %u (total %u ms)", 
                output->framePeriod, output->frameCount,
                output->framePeriod * output->frameCount / 1000);
    TRM_LOG_INFO("  Gaps - BRD:%u, UL:%u, DL:%u us", 
                output->brdGap, output->ulGap, output->dlGap);
    TRM_LOG_INFO("  Slot lengths - BCN:%u, BRD:%u, UL:%u, DL:%u us", 
                output->bcnSlotLen, output->brdSlotLen, 
                output->ulSlotLen, output->dlSlotLen);
    
    return 0;
}

/**
 * @brief 打印时隙计算结果
 * @param output 计算结果
 */
void trm_print_slot_calc_result(const TRM_SlotCalcOutput* output)
{
    if (!output) {
        TRM_LOG_ERROR("Invalid output parameter");
        return;
    }
    
    TRM_LOG_DEBUG("=== TRM Slot Calculation Result ===");
    TRM_LOG_DEBUG("Frame Period: %u us", output->framePeriod);
    TRM_LOG_DEBUG("Frame Count: %u", output->frameCount);
    TRM_LOG_DEBUG("Total Duration: %u ms", output->framePeriod * output->frameCount / 1000);
    TRM_LOG_DEBUG("Slot Lengths: BCN=%u, Broadcast=%u, Uplink=%u, Downlink=%u us",
                  output->bcnSlotLen, output->brdSlotLen, output->ulSlotLen, output->dlSlotLen);
    TRM_LOG_DEBUG("Downlink Gap: %u us", output->dlGap);
}
