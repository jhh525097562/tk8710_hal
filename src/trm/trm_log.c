/**
 * @file trm_log.c
 * @brief TRM模块完全独立日志系统实现
 * 
 * 完全独立于TK8710日志系统，无任何耦合
 */

#include "../inc/trm/trm_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define TRM_LOG_MKDIR(path) _mkdir(path)
#define TRM_LOG_IS_DIR(mode) (((mode) & _S_IFDIR) != 0)
#else
#include <sys/types.h>
#define TRM_LOG_MKDIR(path) mkdir(path, 0755)
#define TRM_LOG_IS_DIR(mode) S_ISDIR(mode)
#endif

#define TRM_DEFAULT_LOG_DIR "8710log"

#ifdef PLATFORM_JTOOL
typedef int trm_mutex_t;
#define TRM_MUTEX_INITIALIZER 0
static void trm_mutex_lock(trm_mutex_t* mutex)
{
    (void)mutex;
}
static void trm_mutex_unlock(trm_mutex_t* mutex)
{
    (void)mutex;
}
#else
#include <pthread.h>
typedef pthread_mutex_t trm_mutex_t;
#define TRM_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
static void trm_mutex_lock(trm_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);
}
static void trm_mutex_unlock(trm_mutex_t* mutex)
{
    pthread_mutex_unlock(mutex);
}
#endif

/* TRM日志文件相关变量 */
static FILE* g_trmLogFile = NULL;           /* 当前日志文件指针 */
static int g_trmCurrentFileIndex = 0;       /* 当前日志文件索引 */
static int g_trmFileLoggingEnabled = 0;     /* 文件日志是否启用 */
static trm_mutex_t g_trmLogMutex = TRM_MUTEX_INITIALIZER;  /* 日志互斥锁 */

/* TRM独立日志配置 */
TRMLogConfig g_trmLogConfig = {
    .level = TRM_LOG_INFO,          /* 默认信息级别 */
    .enable_timestamp = 1,          /* 默认启用时间戳 */
    .enable_module_name = 1,        /* 默认启用模块名 */
    .enable_file_info = 1,          /* 默认启用文件信息（包含函数名和行数） */
    .enable_file_logging = 0,       /* 默认禁用文件日志 */
    .log_file_dir = NULL            /* 默认当前目录 */
};

/* TRM日志输出回调函数 */
static TRMLogCallback g_trmLogCallback = NULL;

/* TRM日志级别名称映射 */
static const char* g_trmLevelNames[] = {
    [TRM_LOG_NONE]  = "NONE",
    [TRM_LOG_ERROR] = "ERROR",
    [TRM_LOG_WARN]  = "WARN",
    [TRM_LOG_INFO]  = "INFO",
    [TRM_LOG_DEBUG] = "DEBUG",
    [TRM_LOG_TRACE] = "TRACE"
};

/**
 * @brief 获取TRM日志级别名称
 * @param level 日志级别
 * @return 级别名称字符串
 */
const char* TRM_LogGetLevelName(TRMLogLevel level)
{
    if (level >= sizeof(g_trmLevelNames)/sizeof(g_trmLevelNames[0])) {
        return "UNKNOWN";
    }
    return g_trmLevelNames[level];
}

/**
 * @brief 获取当前时间戳字符串
 * @param buffer 缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字符数
 */
static int TRM_GetTimestamp(char* buffer, int size)
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    return snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
                   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                   tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

/**
 * @brief 获取文件名（去除路径）
 * @param filepath 完整文件路径
 * @return 文件名指针
 */
static const char* TRM_GetFilename(const char* filepath)
{
    const char* filename = strrchr(filepath, '/');
    if (filename) {
        return filename + 1; /* 跳过'/' */
    }
    filename = strrchr(filepath, '\\');
    if (filename) {
        return filename + 1; /* 跳过'\' */
    }
    return filepath;
}

/**
 * @brief 获取日志文件路径
 * @param buffer 缓冲区
 * @param size 缓冲区大小
 * @param index 文件索引
 */
static void get_log_file_path(char* buffer, int size, int index)
{
    const char* dir = g_trmLogConfig.log_file_dir;
    if (dir == NULL || strlen(dir) == 0) {
        snprintf(buffer, size, "trm_log_%d.log", index);
    } else {
        snprintf(buffer, size, "%s/trm_log_%d.log", dir, index);
    }
}

static int prepare_log_directory(const char* dir)
{
    struct stat st;
    const char* targetDir = dir;

    if (targetDir == NULL || targetDir[0] == '\0') {
        targetDir = TRM_DEFAULT_LOG_DIR;
    }

    if (stat(targetDir, &st) == 0) {
        if (!TRM_LOG_IS_DIR(st.st_mode)) {
            return -1;
        }
    } else {
        if (TRM_LOG_MKDIR(targetDir) != 0 && errno != EEXIST) {
            return -1;
        }

        if (stat(targetDir, &st) != 0 || !TRM_LOG_IS_DIR(st.st_mode)) {
            return -1;
        }
    }

    g_trmLogConfig.log_file_dir = targetDir;
    return 0;
}

/**
 * @brief 获取文件大小
 * @param filename 文件名
 * @return 文件大小（字节），-1表示文件不存在
 */
static long get_file_size(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

/**
 * @brief 关闭当前日志文件
 */
static void close_log_file(void)
{
    if (g_trmLogFile != NULL) {
        fflush(g_trmLogFile);
        fclose(g_trmLogFile);
        g_trmLogFile = NULL;
    }
}

/**
 * @brief 打开指定索引的日志文件
 * @param index 文件索引
 * @return 0-成功, -1-失败
 */
static int open_log_file(int index)
{
    char path[512];
    get_log_file_path(path, sizeof(path), index);
    
    g_trmLogFile = fopen(path, "a+");
    if (g_trmLogFile == NULL) {
        return -1;
    }
    
    g_trmCurrentFileIndex = index;
    return 0;
}

/**
 * @brief 轮转日志文件
 * @note 当前文件写满2M后，切换到另一个未满的文件；如果都满了，覆盖第0个文件
 */
static void rotate_log_file(void)
{
    int nextIndex = 0;
    char path[512];
    int foundUnfilled = 0;
    
    trm_mutex_lock(&g_trmLogMutex);
    
    /* 先关闭当前文件 */
    close_log_file();
    
    /* 扫描所有文件，查找未满的文件 */
    for (int i = 0; i < TRM_LOG_FILE_MAX_COUNT; i++) {
        if (i == g_trmCurrentFileIndex) {
            continue;  /* 跳过当前文件 */
        }
        
        get_log_file_path(path, sizeof(path), i);
        long size = get_file_size(path);
        
        if (size < TRM_LOG_FILE_MAX_SIZE) {
            /* 找到未满的文件 */
            nextIndex = i;
            foundUnfilled = 1;
            break;
        }
    }
    
    /* 如果所有文件都满了，覆盖第0个文件 */
    if (!foundUnfilled) {
        nextIndex = 0;
        get_log_file_path(path, sizeof(path), 0);
        remove(path);  /* 删除旧文件 */
    }
    
    /* 打开新的日志文件 */
    if (open_log_file(nextIndex) != 0) {
        g_trmFileLoggingEnabled = 0;
    }
    
    trm_mutex_unlock(&g_trmLogMutex);
}

/**
 * @brief 写入日志到文件
 * @param buffer 日志内容
 */
static void write_log_to_file(const char* buffer)
{
    if (!g_trmFileLoggingEnabled || g_trmLogFile == NULL) {
        return;
    }
    
    trm_mutex_lock(&g_trmLogMutex);
    
    /* 检查当前文件大小 */
    long currentSize = ftell(g_trmLogFile);
    if (currentSize >= TRM_LOG_FILE_MAX_SIZE) {
        trm_mutex_unlock(&g_trmLogMutex);
        rotate_log_file();
        trm_mutex_lock(&g_trmLogMutex);
    }
    
    /* 写入日志 */
    if (g_trmLogFile != NULL) {
        fprintf(g_trmLogFile, "%s", buffer);
        fflush(g_trmLogFile);
    }
    
    trm_mutex_unlock(&g_trmLogMutex);
}

/**
 * @brief TRM日志输出函数
 * @param level 日志级别
 * @param tag 模块标签
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
void TRM_LogOutput(TRMLogLevel level, const char* tag, 
                   const char* file, int line, const char* func,
                   const char* fmt, ...)
{
    va_list args;
    char buffer[1024];
    int offset = 0;
    
    /* 检查日志级别 */
    if (level > g_trmLogConfig.level) {
        return;
    }
    
    /* 时间戳 */
    if (g_trmLogConfig.enable_timestamp) {
        char timestamp[64];
        TRM_GetTimestamp(timestamp, sizeof(timestamp));
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", timestamp);
    }
    
    /* 日志级别 */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", TRM_LogGetLevelName(level));
    
    /* 模块名 */
    if (g_trmLogConfig.enable_module_name && tag) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s] ", tag);
    }
    
    /* 文件信息 */
    if (g_trmLogConfig.enable_file_info && file && func) {
        const char* filename = TRM_GetFilename(file);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[%s:%d:%s] ", filename, line, func);
    }
    
    /* 用户消息 */
    va_start(args, fmt);
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);
    
    /* 添加换行符 */
    if (offset < (int)sizeof(buffer) - 1) {
        buffer[offset] = '\n';
        buffer[offset + 1] = '\0';
    }
    
    /* 输出日志 */
    if (g_trmLogCallback) {
        /* 使用用户回调 */
        va_start(args, fmt);
        g_trmLogCallback(level, tag, file, line, func, fmt, args);
        va_end(args);
    } else {
        /* 默认输出到标准输出 */
        printf("%s", buffer);
        fflush(stdout);
    }
    
    /* 同时写入文件日志 */
    if (g_trmFileLoggingEnabled) {
        write_log_to_file(buffer);
    }
}

/**
 * @brief 初始化TRM日志系统
 * @param level 日志级别
 */
void TRM_LogInit(TRMLogLevel level)
{
    g_trmLogConfig.level = level;
    g_trmLogConfig.enable_timestamp = 1;
    g_trmLogConfig.enable_module_name = 1;
    g_trmLogConfig.enable_file_info = 1;  /* 启用文件信息（包含函数名和行数） */
    g_trmLogCallback = NULL;
    
    TRM_LogOutput(TRM_LOG_INFO, "TRM", __FILE__, __LINE__, __func__, 
                  "TRM日志系统初始化完成 - 级别:%s", TRM_LogGetLevelName(level));
}

/**
 * @brief 配置TRM日志系统
 * @param level 日志级别
 * @param enable_file_logging 是否启用文件日志
 * @return 0-成功, 1-失败
 */
int TRM_LogConfig(TRMLogLevel level, uint8_t enable_file_logging)
{
    g_trmLogConfig.level = level;
    g_trmLogConfig.enable_timestamp = 1;
    g_trmLogConfig.enable_module_name = 1;
    g_trmLogConfig.enable_file_info = 1;
    g_trmLogCallback = NULL;
    
    /* 配置并启用文件日志 */
    if (enable_file_logging) {
        TRM_LogEnableFileLogging(1, NULL);
    }
    
    TRM_LogOutput(TRM_LOG_INFO, "TRM", __FILE__, __LINE__, __func__, 
                  "TRM日志系统配置完成 - 级别:%s, 文件日志:%s", 
                  TRM_LogGetLevelName(level),
                  enable_file_logging ? "启用" : "禁用");
    
    return 0;
}


/**
 * @brief 设置TRM日志级别
 * @param level 日志级别
 */
void TRM_LogSetLevel(TRMLogLevel level)
{
    g_trmLogConfig.level = level;
}

/**
 * @brief 设置TRM日志输出回调
 * @param callback 回调函数
 */
void TRM_LogSetCallback(TRMLogCallback callback)
{
    g_trmLogCallback = callback;
}

/**
 * @brief 启用/禁用时间戳
 * @param enable 是否启用
 */
void TRM_LogEnableTimestamp(uint8_t enable)
{
    g_trmLogConfig.enable_timestamp = enable;
}

/**
 * @brief 启用/禁用模块名
 * @param enable 是否启用
 */
void TRM_LogEnableModuleName(uint8_t enable)
{
    g_trmLogConfig.enable_module_name = enable;
}

/**
 * @brief 启用/禁用文件信息
 * @param enable 是否启用
 */
void TRM_LogEnableFileInfo(uint8_t enable)
{
    g_trmLogConfig.enable_file_info = enable;
}

/**
 * @brief 启用/禁用文件日志
 * @param enable 是否启用
 * @param dir 日志文件目录，NULL表示当前目录
 */
void TRM_LogEnableFileLogging(uint8_t enable, const char* dir)
{
    g_trmLogConfig.enable_file_logging = enable;
    
    if (enable) {
        if (prepare_log_directory(dir) != 0) {
            g_trmFileLoggingEnabled = 0;
            return;
        }
        /* 初始化文件日志系统 */
        trm_mutex_lock(&g_trmLogMutex);
        
        /* 关闭之前的日志文件 */
        close_log_file();
        
        /* 查找第一个未满的文件 */
        char path[512];
        int startIndex = 0;
        int foundUnfilled = 0;
        
        for (int i = 0; i < TRM_LOG_FILE_MAX_COUNT; i++) {
            get_log_file_path(path, sizeof(path), i);
            long size = get_file_size(path);
            if (size < TRM_LOG_FILE_MAX_SIZE && size >= 0) {
                /* 找到未满的文件 */
                startIndex = i;
                foundUnfilled = 1;
                break;
            }
        }
        
        /* 如果所有文件都满了，从第0个文件重新开始 */
        if (!foundUnfilled) {
            startIndex = 0;
            get_log_file_path(path, sizeof(path), 0);
            remove(path);
        }
        
        /* 打开日志文件 */
        if (open_log_file(startIndex) == 0) {
            g_trmFileLoggingEnabled = 1;
        } else {
            g_trmFileLoggingEnabled = 0;
        }
        
        trm_mutex_unlock(&g_trmLogMutex);
    } else {
        /* 禁用文件日志 */
        trm_mutex_lock(&g_trmLogMutex);
        close_log_file();
        g_trmFileLoggingEnabled = 0;
        trm_mutex_unlock(&g_trmLogMutex);
    }
}
