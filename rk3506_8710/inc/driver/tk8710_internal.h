/**
 * @file tk8710_internal.h
 * @brief TK8710 驱动内部函数和调试接口头文件
 * @note 此文件包含驱动内部使用的函数和调试接口，不建议应用层直接调用
 * 
 * 内容分类:
 *   - 内部控制函数 (原 tk8710_internal.h)
 *   - 调试和测试接口 (原 tk8710_debug.h)
 *   - 从api.h移入的内部函数
 * 
 * 建议:
 *   - 应用层只需包含 tk8710_driver_api.h
 *   - 需要内部功能和调试时包含此文件
 *   - 包含 tk8710.h 可获得所有功能 (向后兼容)
 */

#ifndef TK8710_INTERNAL_H
#define TK8710_INTERNAL_H

#include "tk8710_types.h"
#include "tk8710_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 内部控制函数
 * ============================================================================
 */

/**
 * @brief 控制发送BCN的天线和bcnbits
 * @param bcnantennasel 发送BCN天线配置
 * @param bcnbits 发送BCN信号中的bcnbits
 * @return 0-成功, 1-失败, 2-超时
 * @note 内部函数，不建议应用层直接调用
 */
int TK8710Txbcnctl(uint8_t bcnantennasel, uint8_t bcnbits);

/* ============================================================================
 * 芯片控制API (内部使用)
 * ============================================================================
 */

/**
 * @brief 获取芯片配置
 * @param type 配置类型
 * @param params 配置参数输出指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710GetConfig(TK8710ConfigType type, void* params);

/**
 * @brief 芯片控制命令
 * @param type 控制类型
 * @param params 控制参数指针
 * @return 0-成功, 非0-失败(ACM时表示异常天线位)
 */
int TK8710Ctrl(TK8710CtrlType type, const void* params);
int TK8710FastStartPrepare(uint8_t workType, uint8_t workMode);
int TK8710FastStartTrigger(uint8_t workType);

/* ============================================================================
 * 寄存器读写API (内部使用)
 * ============================================================================
 */

/**
 * @brief 写寄存器
 * @param regType 寄存器类型: 0=全局, 非0=RF寄存器
 * @param addr 寄存器地址
 * @param data 写入数据
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710WriteReg(uint8_t regType, uint16_t addr, uint32_t data);

/**
 * @brief 读寄存器
 * @param regType 寄存器类型: 0=全局, 非0=RF寄存器
 * @param addr 寄存器地址
 * @param data 读取数据输出指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710ReadReg(uint8_t regType, uint16_t addr, uint32_t* data);
int TK8710CheckAndRestoreInit10(void);
void TK8710NotifySlaveBcnReceived(void);
void TK8710StartSlaveBcnWatchdog(void);
void TK8710ProcessRuntimeWatchdog(void);

/* ============================================================================
 * Buffer读写API (内部使用)
 * ============================================================================
 */

/**
 * @brief 写发送数据缓冲区
 * @param start_index 数据开始index (0-127数据用户, 128-143广播用户)
 * @param data 用户数据指针
 * @param len 数据长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710WriteBuffer(uint8_t start_index, const uint8_t* data, size_t len);

/**
 * @brief 读接收数据缓冲区
 * @param start_index 数据开始index (0-127数据用户, 128-143广播用户)
 * @param data_out 数据输出指针
 * @param len 数据长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710ReadBuffer(uint8_t start_index, uint8_t* data_out, size_t len);

/* ============================================================================
 * Efuse读写API (内部使用)
 * ============================================================================
 */

/**
 * @brief 写Efuse
 * @param start_bit 起始bit
 * @param data_bits 写入数据
 * @param len_bits 写入长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710EfuseWrite(uint16_t start_bit, const uint8_t* data_bits, size_t len_bits);

/**
 * @brief 读Efuse
 * @param start_bit 起始bit
 * @param data_bits 读取数据输出
 * @param len_bits 读取长度
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710EfuseRead(uint16_t start_bit, uint8_t* data_bits, size_t len_bits);

/* ============================================================================
 * 数据发送API (内部使用)
 * ============================================================================
 */

/**
 * @brief 获取发送用用户信息
 * @param userIndex 用户索引 (0-127)
 * @param freq 输出频率指针
 * @param ahData 输出AH数据数组 (16个32位数据)
 * @param pilotPower 输出Pilot功率指针
 * @return 0-成功, 1-失败
 */
int TK8710GetTxUserInfo(uint8_t userIndex, uint32_t* freq, uint32_t* ahData, uint64_t* pilotPower);

/**
 * @brief 清除发送用用户信息
 * @param userIndex 用户索引 (0-127), 255表示清除所有
 * @return 0-成功, 1-失败
 */
int TK8710ClearTxUserInfo(uint8_t userIndex);

/**
 * @brief 清除自动发送用户数据
 * @param userIndex 用户索引 (0-127), 255表示清除所有
 * @return 0-成功, 1-失败
 */
int TK8710ClearTxUserData(uint8_t userIndex);

/* ============================================================================
 * 数据接收API (内部使用)
 * ============================================================================
 */

/**
 * @brief 获取发送数据
 * @param userIndex 用户索引 (0-127)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @param txPower 发送功率输出
 * @return 0-成功, 1-失败
 */
int TK8710GetTxData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen, uint8_t* txPower);

/**
 * @brief 获取广播数据
 * @param brdIndex 广播用户索引 (0-15)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @param txPower 发送功率输出
 * @return 0-成功, 1-失败
 */
int TK8710GetBrdData(uint8_t brdIndex, uint8_t** data, uint16_t* dataLen, uint8_t* txPower);

/* ============================================================================
 * 中断管理API (内部使用)
 * ============================================================================
 */

/**
 * @brief 获取当前中断状态
 * @return 中断状态位图
 */
uint32_t TK8710GetIrqStatus(void);

/**
 * @brief 清除中断状态
 * @param mask 要清除的中断位
 */
void TK8710ClearIrqStatus(uint32_t mask);

/**
 * @brief 使能指定中断
 * @param mask 要使能的中断位
 */
void TK8710EnableIrq(uint32_t mask);

/**
 * @brief 禁用指定中断
 * @param mask 要禁用的中断位
 */
void TK8710DisableIrq(uint32_t mask);

/**
 * @brief 获取中断结果
 * @param irqResult 中断结果输出指针
 * @return 0-成功, 1-失败
 */
int TK8710GetIrqResult(TK8710IrqResult* irqResult);

/**
 * @brief Driver层中断处理入口
 */
void TK8710_IRQHandler(void);

/**
 * @brief 获取中断计数器
 * @param irqType 中断类型 (0-9)
 * @return 中断发生次数
 */
uint32_t TK8710GetIrqCounter(uint8_t irqType);

/**
 * @brief 获取所有中断计数器
 * @param counters 输出数组指针，至少10个元素
 */
void TK8710GetAllIrqCounters(uint32_t* counters);
void TK8710GetS0PeriodStats(uint64_t* lastTimeUs, uint32_t* lastPeriodUs, uint32_t* count);

/**
 * @brief 重置中断计数器
 */
void TK8710ResetIrqCounters(void);

/* ============================================================================
 * 日志管理API (内部使用)
 * ============================================================================
 */

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void TK8710LogSetLevel(TK8710LogLevel level);

/**
 * @brief 设置模块掩码
 * @param module_mask 模块掩码
 */
void TK8710LogSetModuleMask(uint32_t module_mask);

/**
 * @brief 设置日志回调函数
 * @param callback 回调函数指针
 */
void TK8710LogSetCallback(TK8710LogCallback callback);

/* ============================================================================
 * 状态查询API (内部使用)
 * ============================================================================
 */

/**
 * @brief 获取时隙配置
 * @return 时隙配置指针
 * @note 内部函数，不建议应用层直接调用
 */
const slotCfg_t* TK8710GetSlotConfig(void);

/**
 * @brief 获取当前速率模式
 * @return 当前速率模式
 */
uint8_t TK8710GetRateMode(void);

/**
 * @brief 获取广播用户数
 * @return 当前广播用户数
 */
uint8_t TK8710GetBrdUserNum(void);

/**
 * @brief 获取速率模式参数
 * @param rateMode 速率模式
 * @param params 输出参数结构体指针
 * @return 0-成功, 1-失败
 */
int TK8710GetRateModeParams(uint8_t rateMode, RateModeParams* params);

/**
 * @brief 获取当前工作类型
 * @return 当前工作类型
 */
uint8_t TK8710GetWorkType(void);

/**
 * @brief 获取当前下行发送模式
 * @return 0=自动发送, 1=指定信息发送
 */
uint8_t TK8710GetTxBeamCtrlMode(void);

/**
 * @brief 获取ACM校准因子
 * @param calFactors 输出校准因子结构体指针
 * @return 0-成功, 1-失败
 * @note 内部函数，不建议应用层直接调用
 */
int TK8710GetAcmCalibrationFactors(AcmCalibrationFactors* calFactors);

/**
 * @brief 补偿一次S3结束后的速率状态更新
 * @return 0-成功, 非0-失败
 * @note ACM校准打断真实S3中断后，由TRM在恢复时隙前调用
 */
int TK8710AdvanceRateAfterS3(void);

/* ============================================================================
 * RF寄存器读写API (内部使用)
 * ============================================================================
 */

/**
 * @brief 读RF寄存器
 * @param rfSel RF选择 (bit0-7对应RF0-7)
 * @param addr RF寄存器地址 (7位)
 * @param data 读取数据输出
 * @return 0-成功, 非0-失败
 */
int tk8710_rf_read(uint8_t rfSel, uint16_t addr, uint32_t* data);

/**
 * @brief 写RF寄存器
 * @param rfSel RF选择 (bit0-7对应RF0-7)
 * @param addr RF寄存器地址 (7位)
 * @param data 写入数据
 * @return 0-成功, 非0-失败
 */
int tk8710_rf_write(uint8_t rfSel, uint16_t addr, uint32_t data);

/* ============================================================================
 * 测试控制接口
 * ============================================================================
 */

/**
 * @brief 设置是否强制处理所有用户数据（测试接口）
 * @param enable 1-强制处理所有用户数据，0-按CRC结果正常处理
 * @note 此函数仅用于测试中断处理能力
 */
void TK8710SetForceProcessAllUsers(uint8_t enable);

/**
 * @brief 获取当前是否强制处理所有用户数据
 * @return 1-强制处理，0-正常处理
 */
uint8_t TK8710GetForceProcessAllUsers(void);

/**
 * @brief 设置是否强制按最大用户数发送（测试接口）
 * @param enable 1-强制按最大用户数发送，0-按实际输入用户数发送
 * @note 此函数仅用于测试中断处理能力
 */
void TK8710SetForceMaxUsersTx(uint8_t enable);

/**
 * @brief 获取当前是否强制按最大用户数发送
 * @return 1-强制发送，0-正常发送
 */
uint8_t TK8710GetForceMaxUsersTx(void);

/* ============================================================================
 * 中断时间统计接口
 * ============================================================================
 */

/**
 * @brief 获取中断处理时间统计
 * @param irqType 中断类型 (0-9)
 * @param totalTime 总处理时间 (输出参数)
 * @param maxTime 最大处理时间 (输出参数)
 * @param minTime 最小处理时间 (输出参数)
 * @param count 中断次数 (输出参数)
 * @return 0-成功, 1-失败
 */
int TK8710GetIrqTimeStats(uint8_t irqType, uint32_t* totalTime, 
                          uint32_t* maxTime, uint32_t* minTime, uint32_t* count);

/**
 * @brief 重置中断处理时间统计
 * @param irqType 中断类型 (0-9)，255表示重置所有
 * @return 0-成功, 1-失败
 */
int TK8710ResetIrqTimeStats(uint8_t irqType);

/**
 * @brief 打印中断处理时间统计报告
 */
void TK8710PrintIrqTimeStats(void);

/* ============================================================================
 * 测试控制API
 * ============================================================================
 */

/**
 * @brief 设置仿真数据加载状态
 * @param loaded 是否已加载仿真数据 (1-已加载, 0-未加载)
 * @note 用于测试时控制中断处理行为
 */
void TK8710SetSimulationDataLoaded(uint8_t loaded);

/**
 * @brief 获取仿真数据加载状态
 * @return 1-已加载仿真数据, 0-未加载仿真数据
 */
uint8_t TK8710GetSimulationDataLoaded(void);

/* ============================================================================
 * 调试控制接口
 * ============================================================================
 */

/**
 * @brief 调试控制接口
 * @param ctrlType 控制类型
 * @param optType 操作类型: 0=Set, 1=Get, 2=Exe
 * @param inputParams 输入参数指针
 * @param outputParams 输出参数指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710DebugCtrl(TK8710DebugCtrlType ctrlType, CtrlOptType optType,
                    const void* inputParams, void* outputParams);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_INTERNAL_H */
