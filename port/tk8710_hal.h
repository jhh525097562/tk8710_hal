/**
 * @file tk8710_hal.h
 * @brief TK8710 硬件抽象层接口定义
 * @note 用户需要根据不同MCU平台实现这些接口
 */

#ifndef TK8710_HAL_H
#define TK8710_HAL_H

#include <stdint.h>
#include <stddef.h>
#include "../inc/driver/tk8710_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化SPI接口
 * @param cfg SPI配置参数
 * @return 0-成功, 非0-失败
 */
int TK8710SpiInit(const SpiConfig* cfg);

/**
 * @brief SPI写数据
 * @param tx 待写数据
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiWrite(const uint8_t* tx, size_t len);

/**
 * @brief SPI读数据
 * @param rx 接收缓冲区
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiRead(uint8_t* rx, size_t len);

/**
 * @brief SPI读写数据 (全双工)
 * @param tx 待写数据
 * @param rx 接收缓冲区
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiTransfer(const uint8_t* tx, uint8_t* rx, size_t len);

/**
 * @brief SPI片选控制
 * @param active 1=选中, 0=释放
 */
void TK8710SpiCsControl(uint8_t active);

/**
 * @brief 初始化GPIO中断
 * @param pin GPIO引脚号
 * @param edge 触发边沿
 * @param cb 中断回调函数
 * @param user 用户上下文数据
 * @return 0-成功, 非0-失败
 */
int TK8710GpioInit(int pin, TK8710GpioEdge edge, TK8710GpioIrqCallback cb, void* user);

/**
 * @brief 使能GPIO中断
 * @param gpioPin GPIO引脚号
 * @param enable 1-使能, 0-禁用
 * @return 0-成功, 1-失败
 */
int TK8710GpioIrqEnable(uint8_t gpioPin, uint8_t enable);

/**
 * @brief GPIO输出控制
 * @param pin GPIO引脚号
 * @param level 输出电平 (0或1)
 */
void TK8710GpioWrite(int pin, uint8_t level);

/**
 * @brief GPIO输入读取
 * @param pin GPIO引脚号
 * @return 引脚电平 (0或1)
 */
uint8_t TK8710GpioRead(int pin);

/**
 * @brief 毫秒延时
 * @param ms 延时毫秒数
 */
void TK8710DelayMs(uint32_t ms);

/**
 * @brief 微秒延时
 * @param us 延时微秒数
 */
void TK8710DelayUs(uint32_t us);

/**
 * @brief Sleep until an absolute monotonic timestamp.
 * @param targetUs Target timestamp in us, using the same time base as TK8710GetTimeUs.
 * @return 0 on success, non-zero on failure or unsupported platform.
 */
int TK8710SleepUntilUs(uint64_t targetUs);

/**
 * @brief 获取系统时间戳 (毫秒)
 * @return 系统时间戳
 */
uint32_t TK8710GetTickMs(void);

/**
 * @brief 进入临界区 (禁用中断)
 */
void TK8710EnterCritical(void);

/**
 * @brief 退出临界区 (恢复中断)
 */
void TK8710ExitCritical(void);

/*============================================================================
 * SPI 协议层接口 (8710 SPI 协议命令)
 * 这些函数在具体的移植层文件中实现 (tk8710_jtool.c / tk8710_stm32.c 等)
 *============================================================================*/

/**
 * @brief 芯片复位命令
 * @param resetConfig 复位配置: bit0=SM复位, bit1=REG复位
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReset(uint8_t resetConfig);

/**
 * @brief 写寄存器 (支持批量写)
 * @param addr 起始寄存器地址
 * @param data 寄存器数据数组
 * @param regCount 寄存器数量
 * @return 0-成功, 非0-失败
 */
int TK8710SpiWriteReg(uint16_t addr, const uint32_t* data, uint8_t regCount);

/**
 * @brief 读寄存器 (支持批量读)
 * @param addr 起始寄存器地址
 * @param data 寄存器数据输出数组
 * @param regCount 寄存器数量
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReadReg(uint16_t addr, uint32_t* data, uint8_t regCount);

/**
 * @brief 写Buffer
 * @param bufferIndex Buffer索引
 * @param data 数据
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiWriteBuffer(uint8_t bufferIndex, const uint8_t* data, uint16_t len);

/**
 * @brief 读Buffer
 * @param bufferIndex Buffer索引
 * @param data 数据输出
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiReadBuffer(uint8_t bufferIndex, uint8_t* data, uint16_t len);

/**
 * @brief 设置信息
 * @param infoType 信息类型 (0-4)
 * @param data 设置数据
 * @param len 数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiSetInfo(uint8_t infoType, const uint8_t* data, uint16_t len);

/**
 * @brief 获取信息
 * @param infoType 信息类型 (0-4)
 * @param data 读取数据缓冲区
 * @param len 要读取的数据长度
 * @return 0-成功, 非0-失败
 */
int TK8710SpiGetInfo(uint8_t infoType, uint8_t* data, uint16_t len);

/**
 * @brief RK3506移植层资源清理
 */
void TK8710Rk3506Cleanup(void);

/**
 * @brief 获取当前时间戳 (微秒)
 * @return 当前时间戳 (微秒)
 * @note 平台相关，需要在不同平台实现
 */
uint64_t TK8710GetTimeUs(void);

/**
 * @brief 获取平台系统随机字节
 * @param data 输出缓冲区
 * @param len 随机字节数
 * @return 0-成功, 非0-失败
 */
int TK8710GetRandomBytes(uint8_t* data, size_t len);

/**
 * @brief GPIO control function (similar to gpioset command)
 * @param chipPath GPIO chip path (e.g., "gpiochip0")
 * @param lineOffset GPIO line offset (e.g., 9)
 * @param level GPIO level (0=low, 1=high)
 * @return 0- success, -1- failure
 */
int TK8710GpioSet(const char* chipPath, unsigned int lineOffset, uint8_t level);

/**
 * @brief GPIO read function (similar to gpioget command)
 * @param chipPath GPIO chip path (e.g., "gpiochip0")
 * @param lineOffset GPIO line offset (e.g., 9)
 * @return GPIO level (0=low, 1=high), -1- error
 */
int TK8710GpioGet(const char* chipPath, unsigned int lineOffset);

/**
 * @brief JTOOL power-cycle reset using VCC/VIO rails
 * @return 0- success, -1- failure
 */
int TK8710JtoolPowerReset(void);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_HAL_H */
