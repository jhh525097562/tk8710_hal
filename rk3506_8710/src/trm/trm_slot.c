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
#include "driver/tk8710_platform.h"

/*============================================================================
 * TRM时隙计算器 - 基于8710_HAL用户指南v1.0 7.2.4章节
 *============================================================================*/

/* 各模式基础时隙长度(us)，包块数=1时的值 */
static const uint32_t g_bcnSlotLen[] = {
    [5] = 69583,  [6] = 36340,  [7] = 19129,  [8] = 10732,
    [9] = 6510,   [10] = 6510,  [11] = 6510,  [18] = 6510
};

/* 卫星和地面站各模式的BCN基础间隔(us)，地面WAN固定使用0 */
static const uint32_t g_bcnBaseGap[] = {
    [5] = 0,      [6] = 0,     [7] = 21000, [8] = 30000,
    [9] = 10000,  [10] = 5000, [11] = 0,    [18] = 0
};

static const uint32_t g_brdBaseBody[] = {
    [5] = 131072, [6] = 65536, [7] = 32768,  [8] = 16384,
    [9] = 8192,  [10] = 4096, [11] = 2048,  [18] = 2048
};

static const uint32_t g_brdBaseGap[] = {
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
    // [5] = 21492,  [6] = 19728,  [7] = 12000,   [8] = 5600,
    // [9] = 2800,   [10] = 1400,  [11] = 800,   [18] = 800
    [5] = 65000,  [6] = 31500,  [7] = 14500,  [8] = 8000,
    [9] = 4500,   [10] = 1400,  [11] = 800,   [18] = 800
};

static const uint32_t g_satDlBaseGap[] = {
    [5] = 65000,  [6] = 31500,  [7] = 14500,  [8] = 8000,
    [9] = 4500,   [10] = 1200,  [11] = 800,   [18] = 800
};

#define INTERVAL_US     1024
#define ONE_SECOND_US   1000000

static const char* trm_slot_calc_type_name(uint8_t calcType)
{
    switch (calcType) {
        case TRM_SLOT_CALC_TYPE_GROUND_WAN:
            return "ground WAN";
        case TRM_SLOT_CALC_TYPE_SATELLITE:
            return "satellite";
        default:
            return "unknown";
    }
}

static const uint32_t* trm_get_dl_base_gap_table(uint8_t calcType)
{
    if (calcType == TRM_SLOT_CALC_TYPE_SATELLITE) {
        return g_satDlBaseGap;
    }
    return g_dlBaseGap;
}

static uint32_t trm_get_bcn_base_gap(uint8_t calcType, uint8_t mode)
{
    if (calcType == TRM_SLOT_CALC_TYPE_SATELLITE) {
        return g_bcnBaseGap[mode];
    }
    return 0;
}

/**
 * @brief 求最大公约数
 * @param a 第一个数
 * @param b 第二个数
 * @return 最大公约数
 */
static uint32_t TK8710_UNUSED trm_gcd(uint32_t a, uint32_t b)
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
    uint8_t calcType = input->calcType;
    const uint32_t* dlBaseGap;

    if (calcType > TRM_SLOT_CALC_TYPE_SATELLITE) {
        TRM_LOG_ERROR("Invalid slot calculation type: %d", calcType);
        return -1;
    }
    dlBaseGap = trm_get_dl_base_gap_table(calcType);

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
    
    TRM_LOG_INFO("Calculating slot config: type=%s, mode=%d, ulBlocks=%d, dlBlocks=%d", 
                trm_slot_calc_type_name(calcType), mode, input->ulBlockNum, input->dlBlockNum);
    
    TRM_LOG_INFO("MinGap position config: BCN=%d, BRD=%d, UL=%d, DL=%d", 
                input->minGapPos[0], input->minGapPos[1], input->minGapPos[2], input->minGapPos[3]);
    
    /* 计算各时隙长度 */
    /* 初始间隔 */
    output->bcnGap = trm_get_bcn_base_gap(calcType, mode);
    output->bcnSlotLen = g_bcnSlotLen[mode] + output->bcnGap;
    output->brdGap = g_brdBaseGap[mode];
    output->ulGap  = g_ulBaseGap[mode];
    output->dlGap  = dlBaseGap[mode];
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
        output->dlSlotLen = INTERVAL_US + g_dlBaseBody[mode] * (input->dlBlockNum * 2 + 1) + dlBaseGap[mode];
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
 * @brief 计算多速率时隙配置参数
 * @param input 输入参数
 * @param output 输出结果
 * @return 0-成功, 非0-失败
 * @note 基于多速率方案实现，支持1-4个速率同时计算
 */
int trm_calc_multi_rate_slot_config(const TRM_MultiRateSlotCalcInput* input, TRM_MultiRateSlotCalcOutput* output)
{
    if (!input || !output) {
        TRM_LOG_ERROR("Invalid parameters: input=%p, output=%p", input, output);
        return -1;
    }
    uint8_t calcType = input->calcType;
    const uint32_t* dlBaseGap;

    if (calcType > TRM_SLOT_CALC_TYPE_SATELLITE) {
        TRM_LOG_ERROR("Invalid slot calculation type: %d", calcType);
        return -1;
    }
    dlBaseGap = trm_get_dl_base_gap_table(calcType);
    
    /* 参数检查 */
    if (input->rateCount < 1 || input->rateCount > 4) {
        TRM_LOG_ERROR("Invalid rate count: %d (supported: 1-4)", input->rateCount);
        return -1;
    }
    
    /* 检查每个速率的模式有效性 */
    for (uint8_t i = 0; i < input->rateCount; i++) {
        uint8_t mode = input->rateModes[i];
        if ((mode < 5 || mode > 11) && mode != 18) {
            TRM_LOG_ERROR("Invalid rate mode[%d]: %d (supported: 5-11, 18)", i, mode);
            return -1;
        }
        
        /* 检查包块数 */
        if (input->brdBlockNums[i] > 16 || input->ulBlockNums[i] > 16 || input->dlBlockNums[i] > 16) {
            TRM_LOG_ERROR("Invalid block numbers[%d]: brd=%d, ul=%d, dl=%d (must be <= 16)", 
                         i, input->brdBlockNums[i], input->ulBlockNums[i], input->dlBlockNums[i]);
            return -1;
        }
    }
    
    TRM_LOG_INFO("Calculating multi-rate slot config: type=%s, rateCount=%d",
                trm_slot_calc_type_name(calcType), input->rateCount);
    for (uint8_t i = 0; i < input->rateCount; i++) {
        TRM_LOG_INFO("Rate[%d]: mode=%d, brdBlocks=%d, ulBlocks=%d, dlBlocks=%d", 
                    i, input->rateModes[i], input->brdBlockNums[i], 
                    input->ulBlockNums[i], input->dlBlockNums[i]);
    }
    
    TRM_LOG_INFO("MinGap position config: BCN=%d, BRD=%d, UL=%d, DL=%d", 
                input->minGapPos[0], input->minGapPos[1], input->minGapPos[2], input->minGapPos[3]);
    
    /* 计算每个速率的时隙长度 */
    uint32_t totalRawPeriod = 0;
    output->rateCount = input->rateCount;
    
    for (uint8_t i = 0; i < input->rateCount; i++) {
        uint8_t mode = input->rateModes[i];
        TRM_RateSlotConfig* config = &output->rateConfigs[i];
        
        /* 计算各时隙长度 */
        config->bcnGap = trm_get_bcn_base_gap(calcType, mode);
        config->bcnSlotLen = g_bcnSlotLen[mode] + config->bcnGap;
        config->brdGap = g_brdBaseGap[mode];
        config->ulGap = g_ulBaseGap[mode];
        config->dlGap = dlBaseGap[mode];
        
        /* 如果包块数为0，对应时隙长度为0 */
        if (input->brdBlockNums[i] == 0) {
            config->brdSlotLen = 0;
            config->brdGap = 0;
        } else {
            config->brdSlotLen = INTERVAL_US + g_brdBaseBody[mode] * (input->brdBlockNums[i] * 2 + 1) + g_brdBaseGap[mode];
        }
        
        if (input->ulBlockNums[i] == 0) {
            config->ulSlotLen = 0;
            config->ulGap = 0;
        } else {
            config->ulSlotLen = INTERVAL_US + g_ulBaseBody[mode] * (input->ulBlockNums[i] * 2 + 1) + g_ulBaseGap[mode];
        }
        
        if (input->dlBlockNums[i] == 0) {
            config->dlSlotLen = 0;
            config->dlGap = 0;
        } else {
            config->dlSlotLen = INTERVAL_US + g_dlBaseBody[mode] * (input->dlBlockNums[i] * 2 + 1) + dlBaseGap[mode];
        }
        
        /* 累加到总原始帧周期 */
        uint32_t rateRawPeriod = config->bcnSlotLen + config->brdSlotLen + 
                                 config->ulSlotLen + config->dlSlotLen;
        totalRawPeriod += rateRawPeriod;
        
        TRM_LOG_INFO("Rate[%d] raw period: %u us (BCN:%u + BRD:%u + UL:%u + DL:%u)", 
                    i, rateRawPeriod, config->bcnSlotLen, config->brdSlotLen, 
                    config->ulSlotLen, config->dlSlotLen);
    }
    
    output->totalRawPeriod = totalRawPeriod;
    TRM_LOG_INFO("Total raw frame period: %u us", totalRawPeriod);
    
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
            
            // 只考虑candidatePeriod >= totalRawPeriod的情况
            if (candidatePeriod >= totalRawPeriod) {
                uint32_t gap = candidatePeriod - totalRawPeriod;
                
                // 优先选择间隔最小的解，且N必须是superFrameNum的整数倍
                if (gap < minGap && (N % input->superFrameNum == 0)) {
                    minGap = gap;
                    bestPeriod = candidatePeriod;
                    bestCount = N;
                    
                    TRM_LOG_INFO("Found better solution: M=%ds, N=%u, gap=%u us, period=%u us", 
                           M, N, gap, candidatePeriod);
                    
                    // 如果gap为0，这是最优解，直接退出
                    if (gap == 0) {
                        goto found_multi_rate_solution;
                    }
                }
            }
        }
    }
    
found_multi_rate_solution:
    if (bestPeriod == 0) {
        // 兜底：使用原始周期
        bestPeriod = totalRawPeriod;
        bestCount = 1;
        minGap = 0;
        TRM_LOG_INFO("No solution found, using total raw period");
    }
    
    output->framePeriod = bestPeriod;
    output->frameCount = bestCount;
    output->addedGap = minGap;
    
    /* 在最后一个速率的时隙中增加gap */
    uint8_t lastRateIndex = input->rateCount - 1;
    TRM_RateSlotConfig* lastConfig = &output->rateConfigs[lastRateIndex];
    
    /* 根据输入参数配置minGap位置 */
    if (input->minGapPos[0]) {
        lastConfig->bcnGap = lastConfig->bcnGap + minGap;
        lastConfig->bcnSlotLen = lastConfig->bcnSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to rate[%d] BCN gap", minGap, lastRateIndex);
    }
    if (input->minGapPos[1]) {
        lastConfig->brdGap = lastConfig->brdGap + minGap;
        lastConfig->brdSlotLen = lastConfig->brdSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to rate[%d] BRD gap", minGap, lastRateIndex);
    }
    if (input->minGapPos[2]) {
        lastConfig->ulGap = lastConfig->ulGap + minGap;
        lastConfig->ulSlotLen = lastConfig->ulSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to rate[%d] UL gap", minGap, lastRateIndex);
    }
    if (input->minGapPos[3]) {
        lastConfig->dlGap = lastConfig->dlGap + minGap;
        lastConfig->dlSlotLen = lastConfig->dlSlotLen + minGap;
        TRM_LOG_INFO("Added minGap %u to rate[%d] DL gap", minGap, lastRateIndex);
    }
    
    TRM_LOG_INFO("Multi-rate slot calculation completed:");
    TRM_LOG_INFO("  Frame period: %u us, Frame count: %u (total %u ms)", 
                output->framePeriod, output->frameCount,
                output->framePeriod * output->frameCount / 1000);
    TRM_LOG_INFO("  Total raw period: %u us, Added gap: %u us", 
                output->totalRawPeriod, output->addedGap);
    
    for (uint8_t i = 0; i < input->rateCount; i++) {
        TRM_RateSlotConfig* config = &output->rateConfigs[i];
        TRM_LOG_INFO("  Rate[%d] - Gaps: BCN:%u, BRD:%u, UL:%u, DL:%u us",
                    i, config->bcnGap, config->brdGap, config->ulGap, config->dlGap);
        TRM_LOG_INFO("  Rate[%d] - Slot lengths: BCN:%u, BRD:%u, UL:%u, DL:%u us", 
                    i, config->bcnSlotLen, config->brdSlotLen, 
                    config->ulSlotLen, config->dlSlotLen);
    }
    
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

/**
 * @brief 打印多速率时隙计算结果
 * @param output 计算结果
 */
void trm_print_multi_rate_slot_calc_result(const TRM_MultiRateSlotCalcOutput* output)
{
    if (!output) {
        TRM_LOG_ERROR("Invalid output parameter");
        return;
    }
    
    TRM_LOG_DEBUG("=== TRM Multi-Rate Slot Calculation Result ===");
    TRM_LOG_DEBUG("Rate Count: %d", output->rateCount);
    TRM_LOG_DEBUG("Frame Period: %u us", output->framePeriod);
    TRM_LOG_DEBUG("Frame Count: %u", output->frameCount);
    TRM_LOG_DEBUG("Total Duration: %u ms", output->framePeriod * output->frameCount / 1000);
    TRM_LOG_DEBUG("Total Raw Period: %u us", output->totalRawPeriod);
    TRM_LOG_DEBUG("Added Gap: %u us", output->addedGap);
    
    for (uint8_t i = 0; i < output->rateCount; i++) {
        const TRM_RateSlotConfig* config = &output->rateConfigs[i];
        TRM_LOG_DEBUG("Rate[%d] - Gaps: BCN=%u, BRD=%u, UL=%u, DL=%u us",
                      i, config->bcnGap, config->brdGap, config->ulGap, config->dlGap);
        TRM_LOG_DEBUG("Rate[%d] - Slot lengths: BCN=%u, BRD=%u, UL=%u, DL=%u us", 
                      i, config->bcnSlotLen, config->brdSlotLen, 
                      config->ulSlotLen, config->dlSlotLen);
    }
}
