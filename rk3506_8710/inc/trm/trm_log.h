/**
 * @file trm_log.h
 * @brief TRM模块完全独立日志系统
 * 
 * 完全独立于TK8710日志系统，无任何耦合
 */

#ifndef TRM_LOG_H
#define TRM_LOG_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TRM独立日志级别定义 */
typedef enum {
    TRM_LOG_NONE = 0,    /* 无日志 */
    TRM_LOG_ERROR = 1,   /* 错误 */
    TRM_LOG_WARN = 2,    /* 警告 */
    TRM_LOG_INFO = 3,    /* 信息 */
    TRM_LOG_DEBUG = 4,   /* 调试 */
    TRM_LOG_TRACE = 5    /* 跟踪 */
} TRMLogLevel;

/* TRM日志文件最大大小: 2MB */
#define TRM_LOG_FILE_MAX_SIZE (2 * 1024 * 1024)

/* TRM日志文件最大数量 */
#define TRM_LOG_FILE_MAX_COUNT 5

/* TRM独立日志配置 */
typedef struct {
    TRMLogLevel level;              /* 当前日志级别 */
    uint8_t enable_timestamp;       /* 是否启用时间戳 */
    uint8_t enable_module_name;     /* 是否启用模块名 */
    uint8_t enable_file_info;       /* 是否启用文件信息 */
    uint8_t enable_file_logging;    /* 是否启用文件日志 */
    const char* log_file_dir;       /* 日志文件目录，NULL表示当前目录 */
} TRMLogConfig;

/* TRM日志输出回调函数类型 */
typedef void (*TRMLogCallback)(TRMLogLevel level, const char* tag, 
                              const char* file, int line, const char* func,
                              const char* fmt, va_list args);

/* TRM独立日志宏定义 */
#define TRM_LOG_ERROR(fmt, ...) \
    do { \
        extern TRMLogConfig g_trmLogConfig; \
        if (TRM_LOG_ERROR <= g_trmLogConfig.level) { \
            TRM_LogOutput(TRM_LOG_ERROR, "TRM", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define TRM_LOG_WARN(fmt, ...) \
    do { \
        extern TRMLogConfig g_trmLogConfig; \
        if (TRM_LOG_WARN <= g_trmLogConfig.level) { \
            TRM_LogOutput(TRM_LOG_WARN, "TRM", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define TRM_LOG_INFO(fmt, ...) \
    do { \
        extern TRMLogConfig g_trmLogConfig; \
        if (TRM_LOG_INFO <= g_trmLogConfig.level) { \
            TRM_LogOutput(TRM_LOG_INFO, "TRM", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define TRM_LOG_DEBUG(fmt, ...) \
    do { \
        extern TRMLogConfig g_trmLogConfig; \
        if (TRM_LOG_DEBUG <= g_trmLogConfig.level) { \
            TRM_LogOutput(TRM_LOG_DEBUG, "TRM", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define TRM_LOG_TRACE(fmt, ...) \
    do { \
        extern TRMLogConfig g_trmLogConfig; \
        if (TRM_LOG_TRACE <= g_trmLogConfig.level) { \
            TRM_LogOutput(TRM_LOG_TRACE, "TRM", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/* TRM日志系统初始化和控制函数 */
void TRM_LogInit(TRMLogLevel level);
int TRM_LogConfig(TRMLogLevel level, uint8_t enable_file_logging);
void TRM_LogSetCallback(TRMLogCallback callback);
void TRM_LogEnableTimestamp(uint8_t enable);
void TRM_LogEnableModuleName(uint8_t enable);
void TRM_LogEnableFileInfo(uint8_t enable);
void TRM_LogEnableFileLogging(uint8_t enable, const char* dir);

/* TRM日志输出函数 */
void TRM_LogOutput(TRMLogLevel level, const char* tag, 
                   const char* file, int line, const char* func,
                   const char* fmt, ...);

/* TRM日志级别名称获取函数 */
const char* TRM_LogGetLevelName(TRMLogLevel level);

#ifdef __cplusplus
}
#endif

#endif /* TRM_LOG_H */
