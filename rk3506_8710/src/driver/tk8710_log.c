/**
 * @file tk8710_log.c
 * @brief TK8710 Driver 日志系统实现
 */

#include "../inc/driver/tk8710_log.h"
#include "../inc/driver/tk8710_driver_api.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_platform.h"
#include "../../port/tk8710_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if TK8710_PLATFORM_HAS_FILE_IO
#include <time.h>
#endif
#include <stdlib.h>
#include <errno.h>
#if defined(PLATFORM_TMS570)
#include "data_transfer.h"
#endif

#if TK8710_PLATFORM_HAS_FILE_IO
#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#define TK8710_LOG_MKDIR(path) _mkdir(path)
#define TK8710_LOG_IS_DIR(mode) (((mode) & _S_IFDIR) != 0)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define TK8710_LOG_MKDIR(path) mkdir(path, 0755)
#define TK8710_LOG_IS_DIR(mode) S_ISDIR(mode)
#endif
#endif

#define TK8710_DEFAULT_LOG_DIR "8710log"

static void sanitize_ascii(char* text)
{
    unsigned char* p = (unsigned char*)text;

    if (text == NULL) {
        return;
    }

    while (*p != '\0') {
        if ((*p < 0x20U) && (*p != '\r') && (*p != '\n') && (*p != '\t')) {
            *p = '?';
        } else if (*p > 0x7EU) {
            *p = '?';
        }
        p++;
    }
}

/* 全局日志配置 */
TK8710LogConfig_t g_logConfig = {
    .level = TK8710_LOG_WARN,           /* 默认警告级别 */
    .module_mask = TK8710_LOG_MODULE_ALL, /* 默认所有模块 */
    .callback = NULL,                    /* 默认无回调 */
    .enable_timestamp = 1,               /* 默认启用时间戳 */
    .enable_module_name = 1,             /* 默认启用模块名 */
    .enable_file_logging = 0,            /* 默认禁用文件日志 */
    .log_file_dir = NULL                 /* 默认使用当前目录 */
};

/* 文件日志相关变量 */
#if TK8710_PLATFORM_HAS_FILE_IO
static FILE* g_logFile = NULL;                      /* 当前日志文件指针 */
#endif
static uint8_t g_fileLoggingEnabled = 0;              /* 文件日志使能标志 */
static int g_currentFileIndex = 0;                  /* 当前日志文件索引 (0-4) */
static char g_logDirectory[256] = {0};              /* 日志文件目录 */
static long g_currentFileSize = 0;                  /* 当前日志文件大小 */

/* 模块名称映射 */
static const char* g_moduleNames[] = {
    [TK8710_LOG_MODULE_CORE]   = "CORE",
    [TK8710_LOG_MODULE_CONFIG] = "CONFIG",
    [TK8710_LOG_MODULE_IRQ]    = "IRQ",
    [TK8710_LOG_MODULE_HAL]    = "HAL",
    [TK8710_LOG_MODULE_ALL]    = "ALL"
};

/* 级别名称映射 */
static const char* g_levelNames[] = {
    [TK8710_LOG_NONE]  = "NONE",
    [TK8710_LOG_ERROR] = "ERROR",
    [TK8710_LOG_WARN]  = "WARN",
    [TK8710_LOG_INFO]  = "INFO",
    [TK8710_LOG_DEBUG] = "DEBUG",
    [TK8710_LOG_TRACE] = "TRACE",
    [TK8710_LOG_ALL]   = "ALL"
};

/**
 * @brief 默认日志输出函数
 */
static void TK8710_UNUSED default_log_output(TK8710LogLevel level, TK8710LogModule module,
                               const char* file, int line, const char* func,
                               const char* fmt, va_list args)
{
    char buffer[512];
    int offset = 0;
    
    /* 时间戳 */
    if (g_logConfig.enable_timestamp) {
#if TK8710_PLATFORM_HAS_FILE_IO
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%04d-%02d-%02d %02d:%02d:%02d", 
                         tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                         tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
#else
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%lu",
                           (unsigned long)TK8710GetTickMs());
#endif
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ");
    }
    
    /* 级别 */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", 
                     TK8710LogGetLevelName(level));
    
    /* 模块名 */
    if (g_logConfig.enable_module_name) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", 
                         TK8710LogGetModuleName(module));
    }
    
    /* 文件和行号 (仅在DEBUG及以上级别) */
    if (level >= TK8710_LOG_DEBUG) {
        const char* filename = strrchr(file, '/');
        if (!filename) filename = strrchr(file, '\\');
        if (!filename) filename = file;
        else filename++;
        
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s:%d:%s] ", 
                         filename, line, func);
    }
    
    /* 用户消息 */
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    
    /* 添加换行符 */
    if (offset < 0) {
        buffer[0] = '\0';
    } else if ((size_t)offset < sizeof(buffer) - 1U) {
        buffer[offset] = '\n';
        buffer[offset + 1] = '\0';
    } else {
        buffer[sizeof(buffer) - 2U] = '\n';
        buffer[sizeof(buffer) - 1U] = '\0';
    }
#if defined(PLATFORM_TMS570)
    size_t length;

    sanitize_ascii(buffer);
    length = strlen(buffer);
    if ((level == TK8710_LOG_ERROR) || (level == TK8710_LOG_WARN)) {
        (void)DataTransfer_AppendRuntimeLog(buffer, (uint16_t)length);
    }
    TK8710PortLogWrite(buffer, length);
#else
    /* 输出到标准输出 */
    sanitize_ascii(buffer);
    printf("%s", buffer);
#endif
}

/*============================================================================
 * 文件日志功能实现
 *============================================================================*/

#if TK8710_PLATFORM_HAS_FILE_IO

/**
 * @brief 生成日志文件路径
 */
static void get_log_file_path(char* path, size_t pathSize, int fileIndex)
{
    if (g_logDirectory[0] != '\0') {
        snprintf(path, pathSize, "%s/%s_%d%s", 
                 g_logDirectory, TK8710_LOG_FILE_NAME_PREFIX, fileIndex, TK8710_LOG_FILE_NAME_EXT);
    } else {
        snprintf(path, pathSize, "%s_%d%s", 
                 TK8710_LOG_FILE_NAME_PREFIX, fileIndex, TK8710_LOG_FILE_NAME_EXT);
    }
}

static int prepare_log_directory(const char* logDir)
{
    struct stat st;
    const char* targetDir = logDir;

    if (targetDir == NULL || targetDir[0] == '\0') {
        targetDir = TK8710_DEFAULT_LOG_DIR;
    }

    strncpy(g_logDirectory, targetDir, sizeof(g_logDirectory) - 1);
    g_logDirectory[sizeof(g_logDirectory) - 1] = '\0';

    if (stat(g_logDirectory, &st) == 0) {
        return TK8710_LOG_IS_DIR(st.st_mode) ? 0 : 1;
    }

    if (TK8710_LOG_MKDIR(g_logDirectory) != 0 && errno != EEXIST) {
        return 1;
    }

    if (stat(g_logDirectory, &st) != 0 || !TK8710_LOG_IS_DIR(st.st_mode)) {
        return 1;
    }

    return 0;
}

/**
 * @brief 获取文件大小
 */
static long get_file_size(const char* path)
{
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return size;
}

/**
 * @brief 关闭当前日志文件
 */
static void close_log_file(void)
{
    if (g_logFile != NULL) {
        fflush(g_logFile);
        fclose(g_logFile);
        g_logFile = NULL;
    }
}

/**
 * @brief 打开新的日志文件
 */
static int open_log_file(int fileIndex)
{
    char path[512];
    
    /* 关闭当前文件 */
    close_log_file();
    
    /* 生成文件路径 */
    get_log_file_path(path, sizeof(path), fileIndex);
    
    /* 打开文件 (追加模式) */
    g_logFile = fopen(path, "a");
    if (g_logFile == NULL) {
        return 1; /* 打开失败 */
    }
    
    /* 获取当前文件大小 */
    g_currentFileSize = get_file_size(path);
    g_currentFileIndex = fileIndex;
    
    return 0; /* 成功 */
}

/**
 * @brief 轮转日志文件
 * @note 当前文件写满2M后, 切换到另一个未满的文件; 如果都满了, 覆盖最小的
 */
static void rotate_log_file(void)
{
    int nextIndex;
    char path[512];
    int foundUnfilled = 0;
    long minSize = -1;
    
    /* 先关闭当前文件 */
    if (g_logFile != NULL) {
        fflush(g_logFile);
        fclose(g_logFile);
        g_logFile = NULL;
    }
    
    /* 扫描所有文件, 查找未满的文件 */
    for (int i = 0; i < TK8710_LOG_FILE_MAX_COUNT; i++) {
        if (i == g_currentFileIndex) {
            continue;  /* 跳过当前文件 */
        }
        
        get_log_file_path(path, sizeof(path), i);
        long size = get_file_size(path);
        
        if (size < TK8710_LOG_FILE_MAX_SIZE) {
            /* 找到未满的文件 */
            if (!foundUnfilled) {
                nextIndex = i;
                foundUnfilled = 1;
            }
        }
        
        /* 记录最小文件大小 */
        if (minSize == -1 || size < minSize) {
            minSize = size;
        }
    }
    
    /* 如果所有其他文件都满了, 则从tk8710_driver_0.log重新开始 */
    if (!foundUnfilled) {
        nextIndex = 0;
        /* 清空tk8710_driver_0.log */
        get_log_file_path(path, sizeof(path), 0);
        remove(path);
    }
    
    /* 打开新的日志文件 */
    if (open_log_file(nextIndex) != 0) {
        /* 打开失败, 禁用文件日志 */
        g_fileLoggingEnabled = 0;
    }
}

/**
 * @brief 写入日志到文件
 */
static void write_log_to_file(const char* buffer, size_t len)
{
    if (!g_fileLoggingEnabled || g_logFile == NULL) {
        return;
    }
    
    /* 检查是否需要轮转 */
    if (g_currentFileSize + (long)len >= TK8710_LOG_FILE_MAX_SIZE) {
        rotate_log_file();
        if (g_logFile == NULL) {
            return;
        }
    }
    
    /* 写入日志 */
    fwrite(buffer, 1, len, g_logFile);
    g_currentFileSize += (long)len;
    
    /* 每10KB刷新一次缓冲区, 平衡性能和可靠性 */
    if (g_currentFileSize % 10240 == 0) {
        fflush(g_logFile);
    }
}

/**
 * @brief 带文件输出的默认日志输出函数
 */
static void default_log_output_with_file(TK8710LogLevel level, TK8710LogModule module,
                                         const char* file, int line, const char* func,
                                         const char* fmt, va_list args)
{
    char buffer[512];
    int offset = 0;
    
    /* 时间戳 */
    if (g_logConfig.enable_timestamp) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%04d-%02d-%02d %02d:%02d:%02d", 
                         tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                         tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ");
    }
    
    /* 级别 */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", 
                     TK8710LogGetLevelName(level));
    
    /* 模块名 */
    if (g_logConfig.enable_module_name) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", 
                         TK8710LogGetModuleName(module));
    }
    
    /* 文件和行号 (仅在DEBUG及以上级别) */
    if (level >= TK8710_LOG_DEBUG) {
        const char* filename = strrchr(file, '/');
        if (!filename) filename = strrchr(file, '\\');
        if (!filename) filename = file;
        else filename++;
        
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s:%d:%s] ", 
                         filename, line, func);
    }
    
    /* 用户消息 */
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    
    /* 添加换行符 */
    if (offset >= 0 && (size_t)offset < sizeof(buffer) - 1U) {
        buffer[offset] = '\n';
        buffer[offset + 1] = '\0';
        offset++;
    }
    sanitize_ascii(buffer);
    
    /* 输出到标准输出 */
    printf("%s", buffer);
    
    /* 输出到文件 */
    if (g_fileLoggingEnabled) {
        write_log_to_file(buffer, offset);
    }
}

#else

static void close_log_file(void)
{
}

static void default_log_output_with_file(TK8710LogLevel level, TK8710LogModule module,
                                         const char* file, int line, const char* func,
                                         const char* fmt, va_list args)
{
    default_log_output(level, module, file, line, func, fmt, args);
}

#endif

/**
 * @brief 初始化日志系统
 */
int TK8710LogInit(const TK8710LogConfig_t* config)
{
    if (config == NULL) {
        return 1; /* 错误：参数为空 */
    }
    
    /* 复制配置 */
    memcpy(&g_logConfig, config, sizeof(TK8710LogConfig_t));

#if !TK8710_PLATFORM_HAS_FILE_IO
    g_logConfig.enable_file_logging = 0;
#endif
    
    /* 如果配置了启用文件日志, 自动启用 */
    if (g_logConfig.enable_file_logging) {
        TK8710LogEnableFileLogging(1, g_logConfig.log_file_dir);
    }
    
    return 0; /* 成功 */
}

/**
 * @brief 设置日志级别
 */
void TK8710LogSetLevel(TK8710LogLevel level)
{
    g_logConfig.level = level;
}

/**
 * @brief 获取当前日志级别
 */
TK8710LogLevel TK8710LogGetLevel(void)
{
    return g_logConfig.level;
}

/**
 * @brief 设置模块掩码
 */
void TK8710LogSetModuleMask(uint32_t module_mask)
{
    g_logConfig.module_mask = module_mask;
}

/**
 * @brief 获取当前模块掩码
 */
uint32_t TK8710LogGetModuleMask(void)
{
    return g_logConfig.module_mask;
}

/**
 * @brief 设置日志回调函数
 */
void TK8710LogSetCallback(TK8710LogCallback callback)
{
    g_logConfig.callback = callback;
}

/**
 * @brief 启用/禁用时间戳
 * @param enable 是否启用时间戳
 */
void TK8710LogEnableTimestamp(uint8_t enable)
{
    g_logConfig.enable_timestamp = enable;
}

/**
 * @brief 启用/禁用模块名
 * @param enable 是否启用模块名
 */
void TK8710LogEnableModuleName(uint8_t enable)
{
    g_logConfig.enable_module_name = enable;
}

/**
 * @brief 输出日志
 */
void TK8710LogOutput(TK8710LogLevel level, TK8710LogModule module,
                     const char* file, int line, const char* func,
                     const char* fmt, ...)
{
    va_list args;
    
    /* 检查级别和模块掩码 */
    if (level > g_logConfig.level || !(g_logConfig.module_mask & module)) {
        return;
    }
    
    va_start(args, fmt);
    
    if (g_logConfig.callback) {
        /* 使用用户回调 */
        g_logConfig.callback(level, module, file, line, func, fmt, args);
    } else {
        /* 使用默认输出 (带文件输出功能) */
        default_log_output_with_file(level, module, file, line, func, fmt, args);
    }
    
    va_end(args);
}

/**
 * @brief 获取模块名称
 */
const char* TK8710LogGetModuleName(TK8710LogModule module)
{
    if (module >= sizeof(g_moduleNames)/sizeof(g_moduleNames[0])) {
        return "UNKNOWN";
    }
    return g_moduleNames[module];
}

/**
 * @brief 获取级别名称
 */
const char* TK8710LogGetLevelName(TK8710LogLevel level)
{
    if (level >= sizeof(g_levelNames)/sizeof(g_levelNames[0])) {
        return "UNKNOWN";
    }
    return g_levelNames[level];
}

/**
 * @brief 简化的日志初始化函数
 * @param level 日志级别
 * @param module_mask 模块掩码
 * @param enable_file_logging 是否启用文件日志
 * @return 0-成功, 1-失败
 */
int TK8710LogConfig(TK8710LogLevel level, uint32_t module_mask, uint8_t enable_file_logging)
{
    TK8710LogConfig_t logConfig = {
        .level = level,
        .module_mask = module_mask,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1,
        .enable_file_logging = enable_file_logging,
        .log_file_dir = NULL
    };
    
    return TK8710LogInit(&logConfig);
}

/*============================================================================
 * 文件日志公共API
 *============================================================================*/

/**
 * @brief 启用/禁用文件日志功能
 * @param enable 是否启用 (1-启用, 0-禁用)
 * @param logDir 日志文件目录路径 (可为NULL, 使用当前目录)
 * @return 0-成功, 1-失败
 */
int TK8710LogEnableFileLogging(uint8_t enable, const char* logDir)
{
#if !TK8710_PLATFORM_HAS_FILE_IO
    (void)logDir;
    g_fileLoggingEnabled = 0;
    return (enable != 0U) ? 1 : 0;
#else
    /* 禁用文件日志 */
    if (!enable) {
        close_log_file();
        g_fileLoggingEnabled = 0;
        return 0;
    }
    
    /* 设置日志目录 */
    if (prepare_log_directory(logDir) != 0) {
        return 1;
    }
    
    /* 扫描所有日志文件，查找未满的文件或需要覆盖的文件 */
    int startIndex = 0;
    int foundUnfilled = 0;
    long minSize = -1;
    
    for (int i = 0; i < TK8710_LOG_FILE_MAX_COUNT; i++) {
        char path[512];
        get_log_file_path(path, sizeof(path), i);
        long size = get_file_size(path);
        
        if (size == 0) {
            /* 空文件，优先使用 */
            if (!foundUnfilled) {
                startIndex = i;
                foundUnfilled = 1;
            }
        } else if (size < TK8710_LOG_FILE_MAX_SIZE) {
            /* 未满的文件，优先使用（选第一个未满的） */
            if (!foundUnfilled) {
                startIndex = i;
                foundUnfilled = 1;
            }
        }
        
        /* 同时记录最小文件大小（用于全部满时覆盖） */
        if (minSize == -1 || size < minSize) {
            minSize = size;
        }
    }
    
    /* 如果所有文件都满了（都>=2M），则从tk8710_driver_0.log重新开始 */
    if (!foundUnfilled) {
        startIndex = 0;
        /* 清空tk8710_driver_0.log重新开始 */
        char path[512];
        get_log_file_path(path, sizeof(path), 0);
        remove(path);
    }
    
    /* 打开日志文件 */
    if (open_log_file(startIndex) != 0) {
        return 1; /* 打开失败 */
    }
    
    g_fileLoggingEnabled = 1;
    return 0;
#endif
}

/**
 * @brief 检查文件日志是否启用
 * @return 1-启用, 0-禁用
 */
uint8_t TK8710LogIsFileLoggingEnabled(void)
{
    return g_fileLoggingEnabled;
}

/**
 * @brief 强制刷新日志文件缓冲区
 * @return 0-成功, 1-失败
 */
int TK8710LogFlushFile(void)
{
#if !TK8710_PLATFORM_HAS_FILE_IO
    return 1;
#else
    if (g_logFile == NULL) {
        return 1;
    }
    
    fflush(g_logFile);
    return 0;
#endif
}

/**
 * @brief 获取当前日志文件索引
 * @return 当前日志文件索引 (0-4)
 */
int TK8710LogGetCurrentFileIndex(void)
{
    return g_currentFileIndex;
}

/**
 * @brief 获取当前日志文件大小
 * @return 当前日志文件大小 (字节)
 */
long TK8710LogGetCurrentFileSize(void)
{
    return g_currentFileSize;
}
