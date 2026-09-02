/**
 * @file tk8710_api.h
 * @brief TK8710 驱动外部API头文件
 * @note 此文件为旧 Driver 公开入口，新的对外 PHY 入口为 phy/phy_api.h
 */

#ifndef TK8710_DRIVER_API_H
#define TK8710_DRIVER_API_H

#include "tk8710_types.h"
#include "tk8710_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 芯片初始化与控制API
 * ============================================================================
 */

/**
 * @brief 初始化TK8710芯片
 * @param initConfig 初始化配置参数，为NULL时使用默认配置
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Init(const ChipConfig* initConfig);

/**
 * @brief 芯片进入收发状态
 * @param workType 工作类型: 0=Slave, 1=Master
 * @param workMode 工作模式: 1=连续, 2=单次
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Start(uint8_t workType, uint8_t workMode);

/**
 * @brief 快速启动收发状态，仅更新工作模式和触发寄存器
 * @param workType 工作类型: 0=Slave, 1=Master, 2=Loopback
 * @param workMode 工作模式: 1=连续, 2=单次
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710FastStart(uint8_t workType, uint8_t workMode);

/**
 * @brief 初始化芯片连接射频
 * @param initrfConfig 射频初始化配置参数
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710RfConfig(const ChiprfConfig* initrfConfig);

/**
 * @brief 芯片复位
 * @param rstType 复位类型: 1=仅复位状态机, 2=复位状态机+寄存器
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710Reset(uint8_t rstType);

/* ============================================================================
 * 配置管理API
 * ============================================================================
 */

/**
 * @brief 设置芯片配置
 * @param type 配置类型
 * @param params 配置参数指针
 * @return 0-成功, 1-失败, 2-超时
 */
int TK8710SetConfig(TK8710ConfigType type, const void* params);

/* ============================================================================
 * 数据传输API
 * ============================================================================
 */

/**
 * @brief 设置下行发送数据和功率
 * @param downlinkType 下行时隙位置: 0=slot1发送, 1=slot3发送
 * @param index 索引: 下行1时范围(0-15), 下行2时范围(0-127)
 * @param data 数据指针
 * @param dataLen 数据长度
 * @param txPower 发送功率
 * @param beamType 波束类型: 0=广播波束, 1=专用数据波束
 * @return 0-成功, 1-失败
 */
int TK8710SetTxData(TK8710DownlinkType downlinkType, uint8_t index, const uint8_t* data, uint16_t dataLen, uint8_t txPower, uint8_t beamType);

/**
 * @brief 设置发送用用户信息 (指定信息发送模式)
 * @param userBufferIdx 用户索引 (0-127)
 * @param freqInfo 频率
 * @param ahInfo AH数据数组 (16个32位数据)
 * @param pilotPowerInfo Pilot功率
 * @return 0-成功, 1-失败
 */
int TK8710SetTxUserInfo(uint8_t userBufferIdx, uint32_t freqInfo, const uint32_t* ahInfo, uint64_t pilotPowerInfo);

/* ============================================================================
 * 数据接收API
 * ============================================================================
 */

/**
 * @brief 获取接收数据
 * @param userIndex 用户索引 (0-127)
 * @param data 数据指针输出
 * @param dataLen 数据长度输出
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen);

/**
 * @brief 获取接收用户信息 (从MD_UD中断获取的数据)
 * @param userBufferIdx 用户索引 (0-127)
 * @param freqInfo 输出频率指针
 * @param ahInfo 输出AH数据数组 (16个32位数据)
 * @param pilotPowerInfo 输出Pilot功率指针
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserInfo(uint8_t userBufferIdx, uint32_t* freqInfo, uint32_t* ahInfo, uint64_t* pilotPowerInfo);

/**
 * @brief 获取信号质量信息
 * @param userIndex 用户索引 (0-127)
 * @param rssi RSSI值输出
 * @param snr SNR值输出
 * @param freq 频率值输出
 * @return 0-成功, 1-失败
 */
int TK8710GetRxUserSignalQuality(uint8_t userIndex, uint32_t* rssi, uint8_t* snr, uint32_t* freq);

/**
 * @brief 释放接收数据
 * @param userIndex 用户索引 (0-127)
 * @return 0-成功, 1-失败
 */
int TK8710ReleaseRxData(uint8_t userIndex);

/* ============================================================================
 * 状态查询API
 * ============================================================================
 */

/* ============================================================================
 * 回调注册API
 * ============================================================================
 */

/**
 * @brief 注册Driver回调函数
 * @param callbacks 回调函数结构体指针
 */
void TK8710RegisterCallbacks(const TK8710DriverCallbacks* callbacks);

/**
 * @brief 获取ANoise检测到的异常射频通道状态
 * @param channelMask 异常通道位图，bit0-bit7对应RF0-RF7
 * @param channelCount 异常通道数量
 * @return 0-成功，其他值-参数错误
 */
int TK8710GetAbnormalRfChannelStatus(uint8_t* channelMask, uint8_t* channelCount);

/* ============================================================================
 * 中断处理API
 * ============================================================================
 */

/**
 * @brief GPIO中断初始化
 * @param pin GPIO引脚
 * @param edge 中断边沿类型
 * @param cb 中断回调函数
 * @param user 用户数据指针
 * @return 0-成功, 1-失败
 */
int TK8710GpioInit(int pin, TK8710GpioEdge edge, TK8710GpioIrqCallback cb, void* user);

/**
 * @brief 使能GPIO中断
 * @param gpioPin GPIO引脚
 * @param enable 1-使能, 0-禁用
 * @return 0-成功, 1-失败
 */
int TK8710GpioIrqEnable(uint8_t gpioPin, uint8_t enable);

/* ============================================================================
 * 日志系统API
 * ============================================================================
 */

/**
 * @brief 简化的日志初始化函数
 * @param level 日志级别
 * @param module_mask 模块掩码
 * @return 0-成功, 1-失败
 */
int TK8710LogConfig(TK8710LogLevel level, uint32_t module_mask, uint8_t enable_file_logging);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_DRIVER_API_H */
