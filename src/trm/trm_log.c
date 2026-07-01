/**
 * @file trm_log.c
 * @brief TRM independent log system implementation.
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

#if defined(PLATFORM_JTOOL) && defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION trm_mutex_t;
#define TRM_MUTEX_INITIALIZER {0}
static volatile LONG g_trmMutexInitState = 0;

static void trm_mutex_ensure_initialized(trm_mutex_t* mutex)
{
    if (InterlockedCompareExchange(&g_trmMutexInitState, 0, 0) == 2) {
        return;
    }

    if (InterlockedCompareExchange(&g_trmMutexInitState, 1, 0) == 0) {
        InitializeCriticalSection(mutex);
        InterlockedExchange(&g_trmMutexInitState, 2);
        return;
    }

    while (InterlockedCompareExchange(&g_trmMutexInitState, 0, 0) != 2) {
        Sleep(0);
    }
}

static void trm_mutex_lock(trm_mutex_t* mutex)
{
    trm_mutex_ensure_initialized(mutex);
    EnterCriticalSection(mutex);
}

static void trm_mutex_unlock(trm_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
}
#elif defined(PLATFORM_JTOOL)
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

static FILE* g_trmLogFile = NULL;
static int g_trmCurrentFileIndex = 0;
static int g_trmFileLoggingEnabled = 0;
static trm_mutex_t g_trmLogMutex = TRM_MUTEX_INITIALIZER;
static char g_trmLogDirectory[256] = {0};
static long g_trmCurrentFileSize = 0;
static uint8_t g_trmLogConfigured = 0;

TRMLogConfig g_trmLogConfig = {
    .level = TRM_LOG_INFO,
    .enable_timestamp = 1,
    .enable_module_name = 1,
    .enable_file_info = 1,
    .enable_file_logging = 0,
    .log_file_dir = NULL
};

static TRMLogCallback g_trmLogCallback = NULL;

static const char* g_trmLevelNames[] = {
    [TRM_LOG_NONE]  = "NONE",
    [TRM_LOG_ERROR] = "ERROR",
    [TRM_LOG_WARN]  = "WARN",
    [TRM_LOG_INFO]  = "INFO",
    [TRM_LOG_DEBUG] = "DEBUG",
    [TRM_LOG_TRACE] = "TRACE"
};

const char* TRM_LogGetLevelName(TRMLogLevel level)
{
    if ((size_t)level >= sizeof(g_trmLevelNames) / sizeof(g_trmLevelNames[0])) {
        return "UNKNOWN";
    }
    return g_trmLevelNames[level];
}

static int TRM_GetTimestamp(char* buffer, int size)
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if (tm_info == NULL) {
        return snprintf(buffer, size, "0000-00-00 00:00:00");
    }

    return snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

static const char* TRM_GetFilename(const char* filepath)
{
    const char* filename = strrchr(filepath, '/');
    if (filename) {
        return filename + 1;
    }

    filename = strrchr(filepath, '\\');
    if (filename) {
        return filename + 1;
    }

    return filepath;
}

static void get_log_file_path(char* buffer, int size, int index)
{
    const char* dir = g_trmLogConfig.log_file_dir;

    if (dir == NULL || dir[0] == '\0') {
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

    if (strlen(targetDir) >= sizeof(g_trmLogDirectory)) {
        return -1;
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

    strncpy(g_trmLogDirectory, targetDir, sizeof(g_trmLogDirectory) - 1);
    g_trmLogDirectory[sizeof(g_trmLogDirectory) - 1] = '\0';
    g_trmLogConfig.log_file_dir = g_trmLogDirectory;

    return 0;
}

static long get_file_size(const char* filename)
{
    struct stat st;

    if (stat(filename, &st) == 0) {
        return st.st_size;
    }

    return 0;
}

static void close_log_file(void)
{
    if (g_trmLogFile != NULL) {
        fflush(g_trmLogFile);
        fclose(g_trmLogFile);
        g_trmLogFile = NULL;
    }

    g_trmCurrentFileSize = 0;
}

static int open_log_file(int index)
{
    char path[512];

    get_log_file_path(path, sizeof(path), index);

    g_trmLogFile = fopen(path, "a");
    if (g_trmLogFile == NULL) {
        return -1;
    }

    g_trmCurrentFileIndex = index;
    g_trmCurrentFileSize = get_file_size(path);

    return 0;
}

static void rotate_log_file_locked(void)
{
    int nextIndex = (g_trmCurrentFileIndex + 1) % TRM_LOG_FILE_MAX_COUNT;
    char path[512];

    close_log_file();

    get_log_file_path(path, sizeof(path), nextIndex);
    if (get_file_size(path) >= TRM_LOG_FILE_MAX_SIZE) {
        remove(path);
    }

    if (open_log_file(nextIndex) != 0) {
        g_trmFileLoggingEnabled = 0;
        g_trmLogConfig.enable_file_logging = 0;
    }
}

static void write_log_to_file(const char* buffer, size_t len)
{
    if (!g_trmFileLoggingEnabled || g_trmLogFile == NULL || buffer == NULL || len == 0) {
        return;
    }

    trm_mutex_lock(&g_trmLogMutex);

    if (!g_trmFileLoggingEnabled || g_trmLogFile == NULL) {
        trm_mutex_unlock(&g_trmLogMutex);
        return;
    }

    if (g_trmCurrentFileSize + (long)len >= TRM_LOG_FILE_MAX_SIZE) {
        rotate_log_file_locked();
        if (g_trmLogFile == NULL) {
            trm_mutex_unlock(&g_trmLogMutex);
            return;
        }
    }

    size_t written = fwrite(buffer, 1, len, g_trmLogFile);
    g_trmCurrentFileSize += (long)written;

    if (written != len || fflush(g_trmLogFile) != 0) {
        close_log_file();
        g_trmFileLoggingEnabled = 0;
        g_trmLogConfig.enable_file_logging = 0;
    }

    trm_mutex_unlock(&g_trmLogMutex);
}

static int append_log_vformat(char* buffer, size_t size, int offset,
                              const char* fmt, va_list args)
{
    if (buffer == NULL || size == 0 || fmt == NULL) {
        return offset;
    }
    if (offset < 0) {
        offset = 0;
    }
    if ((size_t)offset >= size) {
        return (int)size - 1;
    }

    int remaining = (int)(size - (size_t)offset);
    int written = vsnprintf(buffer + offset, (size_t)remaining, fmt, args);

    if (written < 0) {
        return offset;
    }
    if (written >= remaining) {
        return (int)size - 1;
    }

    return offset + written;
}

static int append_log_format(char* buffer, size_t size, int offset, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    offset = append_log_vformat(buffer, size, offset, fmt, args);
    va_end(args);

    return offset;
}

void TRM_LogOutput(TRMLogLevel level, const char* tag,
                   const char* file, int line, const char* func,
                   const char* fmt, ...)
{
    va_list args;
    char buffer[1024];
    int offset = 0;

    if (level > g_trmLogConfig.level) {
        return;
    }

    buffer[0] = '\0';

    if (g_trmLogConfig.enable_timestamp) {
        char timestamp[64];
        TRM_GetTimestamp(timestamp, sizeof(timestamp));
        offset = append_log_format(buffer, sizeof(buffer), offset, "[%s] ", timestamp);
    }

    offset = append_log_format(buffer, sizeof(buffer), offset, "[%s] ",
                               TRM_LogGetLevelName(level));

    if (g_trmLogConfig.enable_module_name && tag) {
        offset = append_log_format(buffer, sizeof(buffer), offset, "[%s] ", tag);
    }

    if (g_trmLogConfig.enable_file_info && file && func) {
        const char* filename = TRM_GetFilename(file);
        offset = append_log_format(buffer, sizeof(buffer), offset, "[%s:%d:%s] ",
                                   filename, line, func);
    }

    va_start(args, fmt);
    offset = append_log_vformat(buffer, sizeof(buffer), offset, fmt, args);
    va_end(args);

    if (offset >= (int)sizeof(buffer) - 1) {
        offset = (int)sizeof(buffer) - 2;
    }
    buffer[offset++] = '\n';
    buffer[offset] = '\0';

    if (g_trmLogCallback) {
        va_start(args, fmt);
        g_trmLogCallback(level, tag, file, line, func, fmt, args);
        va_end(args);
    } else {
        printf("%s", buffer);
        fflush(stdout);
    }

    if (g_trmFileLoggingEnabled) {
        write_log_to_file(buffer, (size_t)offset);
    }
}

void TRM_LogInit(TRMLogLevel level)
{
    if (!g_trmLogConfigured) {
        g_trmLogConfig.level = level;
    }

    g_trmLogConfig.enable_timestamp = 1;
    g_trmLogConfig.enable_module_name = 1;
    g_trmLogConfig.enable_file_info = 1;
    g_trmLogCallback = NULL;

    TRM_LogOutput(TRM_LOG_INFO, "TRM", __FILE__, __LINE__, __func__,
                  "TRM日志系统初始化完成 - 级别:%s",
                  TRM_LogGetLevelName(g_trmLogConfig.level));
}

int TRM_LogConfig(TRMLogLevel level, uint8_t enable_file_logging)
{
    g_trmLogConfig.level = level;
    g_trmLogConfig.enable_timestamp = 1;
    g_trmLogConfig.enable_module_name = 1;
    g_trmLogConfig.enable_file_info = 1;
    g_trmLogCallback = NULL;
    g_trmLogConfigured = 1;

    if (enable_file_logging) {
        TRM_LogEnableFileLogging(1, NULL);
    } else {
        TRM_LogEnableFileLogging(0, NULL);
    }

    TRM_LogOutput(TRM_LOG_INFO, "TRM", __FILE__, __LINE__, __func__,
                  "TRM日志系统配置完成 - 级别:%s, 文件日志:%s",
                  TRM_LogGetLevelName(level),
                  TRM_LogIsFileLoggingEnabled() ? "启用" : "禁用");

    return (enable_file_logging && !TRM_LogIsFileLoggingEnabled()) ? 1 : 0;
}

void TRM_LogSetLevel(TRMLogLevel level)
{
    g_trmLogConfig.level = level;
    g_trmLogConfigured = 1;
}

void TRM_LogSetCallback(TRMLogCallback callback)
{
    g_trmLogCallback = callback;
}

void TRM_LogEnableTimestamp(uint8_t enable)
{
    g_trmLogConfig.enable_timestamp = enable;
}

void TRM_LogEnableModuleName(uint8_t enable)
{
    g_trmLogConfig.enable_module_name = enable;
}

void TRM_LogEnableFileInfo(uint8_t enable)
{
    g_trmLogConfig.enable_file_info = enable;
}

void TRM_LogEnableFileLogging(uint8_t enable, const char* dir)
{
    if (enable) {
        if (prepare_log_directory(dir) != 0) {
            trm_mutex_lock(&g_trmLogMutex);
            close_log_file();
            g_trmFileLoggingEnabled = 0;
            g_trmLogConfig.enable_file_logging = 0;
            trm_mutex_unlock(&g_trmLogMutex);
            return;
        }

        trm_mutex_lock(&g_trmLogMutex);
        close_log_file();

        char path[512];
        int startIndex = 0;
        int foundUnfilled = 0;

        for (int i = 0; i < TRM_LOG_FILE_MAX_COUNT; i++) {
            get_log_file_path(path, sizeof(path), i);
            if (get_file_size(path) < TRM_LOG_FILE_MAX_SIZE) {
                startIndex = i;
                foundUnfilled = 1;
                break;
            }
        }

        if (!foundUnfilled) {
            startIndex = 0;
            get_log_file_path(path, sizeof(path), startIndex);
            remove(path);
        }

        if (open_log_file(startIndex) == 0) {
            g_trmFileLoggingEnabled = 1;
            g_trmLogConfig.enable_file_logging = 1;
        } else {
            g_trmFileLoggingEnabled = 0;
            g_trmLogConfig.enable_file_logging = 0;
        }

        trm_mutex_unlock(&g_trmLogMutex);
    } else {
        trm_mutex_lock(&g_trmLogMutex);
        close_log_file();
        g_trmFileLoggingEnabled = 0;
        g_trmLogConfig.enable_file_logging = 0;
        trm_mutex_unlock(&g_trmLogMutex);
    }
}

void TRM_LogFlushFile(void)
{
    trm_mutex_lock(&g_trmLogMutex);
    if (g_trmLogFile != NULL) {
        fflush(g_trmLogFile);
    }
    trm_mutex_unlock(&g_trmLogMutex);
}

uint8_t TRM_LogIsFileLoggingEnabled(void)
{
    uint8_t enabled;

    trm_mutex_lock(&g_trmLogMutex);
    enabled = g_trmFileLoggingEnabled ? 1U : 0U;
    trm_mutex_unlock(&g_trmLogMutex);

    return enabled;
}

int TRM_LogGetCurrentFileIndex(void)
{
    int index;

    trm_mutex_lock(&g_trmLogMutex);
    index = g_trmCurrentFileIndex;
    trm_mutex_unlock(&g_trmLogMutex);

    return index;
}

long TRM_LogGetCurrentFileSize(void)
{
    long size;

    trm_mutex_lock(&g_trmLogMutex);
    size = g_trmCurrentFileSize;
    trm_mutex_unlock(&g_trmLogMutex);

    return size;
}
