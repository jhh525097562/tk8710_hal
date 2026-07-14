/**
 * @file tk8710_log.h
 * @brief TK8710 Driver 日志系统
 * 
 * 日志级别定义:
 * - LOG_ERROR: 错误信息，系统异常
 * - LOG_WARN:  警告信息，潜在问题
 * - LOG_INFO:  一般信息，重要事件
 * - LOG_DEBUG: 调试信息，详细执行过程
 * - LOG_TRACE: 跟踪信息，函数调用等
 */

#ifndef TK8710_LOG_H
#define TK8710_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别定义 */
typedef enum {
    TK8710_LOG_NONE  = 0,  /* 不输出日志 */
    TK8710_LOG_ERROR = 1,  /* 错误级别 */
    TK8710_LOG_WARN  = 2,  /* 警告级别 */
    TK8710_LOG_INFO  = 3,  /* 信息级别 */
    TK8710_LOG_DEBUG = 4,  /* 调试级别 */
    TK8710_LOG_TRACE = 5,  /* 跟踪级别 */
    TK8710_LOG_ALL   = 6   /* 所有级别 */
} TK8710LogLevel;

/* 日志模块定义 */
typedef enum {
    TK8710_LOG_MODULE_CORE     = 0x01,  /* 核心模块 */
    TK8710_LOG_MODULE_CONFIG   = 0x02,  /* 配置模块 */
    TK8710_LOG_MODULE_IRQ      = 0x04,  /* 中断模块 */
    TK8710_LOG_MODULE_HAL      = 0x08,  /* HAL层模块 */
    TK8710_LOG_MODULE_ALL      = 0xFF   /* 所有模块 */
} TK8710LogModule;

/* 日志回调函数类型 */
typedef void (*TK8710LogCallback)(TK8710LogLevel level, TK8710LogModule module, 
                                  const char* file, int line, const char* func, 
                                  const char* fmt, ...);

/* 日志控制结构体 */
typedef struct {
    TK8710LogLevel level;           /* 当前日志级别 */
    uint32_t module_mask;           /* 模块掩码 */
    TK8710LogCallback callback;     /* 日志输出回调 */
    uint8_t enable_timestamp;       /* 是否启用时间戳 */
    uint8_t enable_module_name;     /* 是否启用模块名 */
    uint8_t enable_file_logging;    /* 是否启用文件日志 */
    const char* log_file_dir;       /* 日志文件目录 (可为NULL, 使用当前目录) */
} TK8710LogConfig_t;

/* 日志文件配置常量 */
#define TK8710_LOG_FILE_MAX_SIZE    (2 * 1024 * 1024)   /* 单个日志文件最大2MB */
#define TK8710_LOG_FILE_MAX_COUNT   5                   /* 最大日志文件数量 */
#define TK8710_LOG_FILE_NAME_PREFIX "tk8710_driver"      /* 日志文件前缀 */
#define TK8710_LOG_FILE_NAME_EXT    ".log"              /* 日志文件扩展名 */

/**
 * @brief 启用文件日志功能
 * @param enable 是否启用 (1-启用, 0-禁用)
 * @param logDir 日志文件目录路径 (可为NULL, 使用当前目录)
 * @return 0-成功, 1-失败
 * @note 启用后会自动创建日志文件, 达到2MB后自动轮转, 最多保留5个文件
 */
int TK8710LogEnableFileLogging(uint8_t enable, const char* logDir);

/**
 * @brief 检查文件日志是否启用
 * @return 1-启用, 0-禁用
 */
uint8_t TK8710LogIsFileLoggingEnabled(void);

/**
 * @brief 强制刷新日志文件缓冲区
 * @return 0-成功, 1-失败
 */
int TK8710LogFlushFile(void);

/**
 * @brief 获取当前日志文件索引
 * @return 当前日志文件索引 (0-4)
 */
int TK8710LogGetCurrentFileIndex(void);

/**
 * @brief 获取当前日志文件大小
 * @return 当前日志文件大小 (字节)
 */
long TK8710LogGetCurrentFileSize(void);

/* 全局日志配置变量声明 */
extern TK8710LogConfig_t g_logConfig;

/**
 * @brief 初始化日志系统
 * @param config 日志配置
 * @return 0-成功, 1-失败
 */
int TK8710LogInit(const TK8710LogConfig_t* config);

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void TK8710LogSetLevel(TK8710LogLevel level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
TK8710LogLevel TK8710LogGetLevel(void);

/**
 * @brief 设置模块掩码
 * @param module_mask 模块掩码
 */
void TK8710LogSetModuleMask(uint32_t module_mask);

/**
 * @brief 获取当前模块掩码
 * @return 当前模块掩码
 */
uint32_t TK8710LogGetModuleMask(void);

/**
 * @brief 设置日志回调函数
 * @param callback 回调函数
 */
void TK8710LogSetCallback(TK8710LogCallback callback);

/**
 * @brief 启用/禁用时间戳
 * @param enable 是否启用时间戳
 */
void TK8710LogEnableTimestamp(uint8_t enable);

/**
 * @brief 启用/禁用模块名
 * @param enable 是否启用模块名
 */
void TK8710LogEnableModuleName(uint8_t enable);

/**
 * @brief 输出日志
 * @param level 日志级别
 * @param module 模块标识
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void TK8710LogOutput(TK8710LogLevel level, TK8710LogModule module,
                     const char* file, int line, const char* func,
                     const char* fmt, ...);

/**
 * @brief 获取模块名称
 * @param module 模块枚举
 * @return 模块名称字符串
 */
const char* TK8710LogGetModuleName(TK8710LogModule module);

/**
 * @brief 简化初始化日志系统
 * @param level 日志级别
 * @param module_mask 模块掩码
 * @param enable_file_logging 是否启用文件日志
 * @return 0-成功, 1-失败
 */
int TK8710LogConfig(TK8710LogLevel level, uint32_t module_mask, uint8_t enable_file_logging);

/**
 * @brief 获取级别名称
 * @param level 级别枚举
 * @return 级别名称字符串
 */
const char* TK8710LogGetLevelName(TK8710LogLevel level);

/* 日志宏定义 - 简化使用 */
#define TK8710_LOG_ENABLE(module, level) \
    ((TK8710LogGetLevel() >= level) && (TK8710LogGetModuleMask() & (module)))

#define TK8710_LOG_ERROR(module, fmt, ...) \
    do { if (TK8710_LOG_ENABLE(module, TK8710_LOG_ERROR)) \
         TK8710LogOutput(TK8710_LOG_ERROR, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

#define TK8710_LOG_WARN(module, fmt, ...) \
    do { if (TK8710_LOG_ENABLE(module, TK8710_LOG_WARN)) \
         TK8710LogOutput(TK8710_LOG_WARN, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

#define TK8710_LOG_INFO(module, fmt, ...) \
    do { if (TK8710_LOG_ENABLE(module, TK8710_LOG_INFO)) \
         TK8710LogOutput(TK8710_LOG_INFO, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

#define TK8710_LOG_DEBUG(module, fmt, ...) \
    do { if (TK8710_LOG_ENABLE(module, TK8710_LOG_DEBUG)) \
         TK8710LogOutput(TK8710_LOG_DEBUG, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

#define TK8710_LOG_TRACE(module, fmt, ...) \
    do { if (TK8710_LOG_ENABLE(module, TK8710_LOG_TRACE)) \
         TK8710LogOutput(TK8710_LOG_TRACE, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)

/* 模块特定日志宏 - 更简洁 */
#define TK8710_LOG_CORE_ERROR(fmt, ...)   TK8710_LOG_ERROR(TK8710_LOG_MODULE_CORE, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CORE_WARN(fmt, ...)    TK8710_LOG_WARN(TK8710_LOG_MODULE_CORE, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CORE_INFO(fmt, ...)    TK8710_LOG_INFO(TK8710_LOG_MODULE_CORE, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CORE_DEBUG(fmt, ...)   TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CORE, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CORE_TRACE(fmt, ...)   TK8710_LOG_TRACE(TK8710_LOG_MODULE_CORE, fmt, ##__VA_ARGS__)

#define TK8710_LOG_CONFIG_ERROR(fmt, ...) TK8710_LOG_ERROR(TK8710_LOG_MODULE_CONFIG, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CONFIG_WARN(fmt, ...)  TK8710_LOG_WARN(TK8710_LOG_MODULE_CONFIG, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CONFIG_INFO(fmt, ...)  TK8710_LOG_INFO(TK8710_LOG_MODULE_CONFIG, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CONFIG_DEBUG(fmt, ...) TK8710_LOG_DEBUG(TK8710_LOG_MODULE_CONFIG, fmt, ##__VA_ARGS__)
#define TK8710_LOG_CONFIG_TRACE(fmt, ...) TK8710_LOG_TRACE(TK8710_LOG_MODULE_CONFIG, fmt, ##__VA_ARGS__)

#define TK8710_LOG_IRQ_ERROR(fmt, ...)    TK8710_LOG_ERROR(TK8710_LOG_MODULE_IRQ, fmt, ##__VA_ARGS__)
#define TK8710_LOG_IRQ_WARN(fmt, ...)     TK8710_LOG_WARN(TK8710_LOG_MODULE_IRQ, fmt, ##__VA_ARGS__)
#define TK8710_LOG_IRQ_INFO(fmt, ...)     TK8710_LOG_INFO(TK8710_LOG_MODULE_IRQ, fmt, ##__VA_ARGS__)
#define TK8710_LOG_IRQ_DEBUG(fmt, ...)    TK8710_LOG_DEBUG(TK8710_LOG_MODULE_IRQ, fmt, ##__VA_ARGS__)
#define TK8710_LOG_IRQ_TRACE(fmt, ...)    TK8710_LOG_TRACE(TK8710_LOG_MODULE_IRQ, fmt, ##__VA_ARGS__)

#define TK8710_LOG_HAL_ERROR(fmt, ...)    TK8710_LOG_ERROR(TK8710_LOG_MODULE_HAL, fmt, ##__VA_ARGS__)
#define TK8710_LOG_HAL_WARN(fmt, ...)     TK8710_LOG_WARN(TK8710_LOG_MODULE_HAL, fmt, ##__VA_ARGS__)
#define TK8710_LOG_HAL_INFO(fmt, ...)     TK8710_LOG_INFO(TK8710_LOG_MODULE_HAL, fmt, ##__VA_ARGS__)
#define TK8710_LOG_HAL_DEBUG(fmt, ...)    TK8710_LOG_DEBUG(TK8710_LOG_MODULE_HAL, fmt, ##__VA_ARGS__)
#define TK8710_LOG_HAL_TRACE(fmt, ...)    TK8710_LOG_TRACE(TK8710_LOG_MODULE_HAL, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* TK8710_LOG_H */
