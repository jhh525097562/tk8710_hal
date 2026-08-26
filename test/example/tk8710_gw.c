/**
 * @file test8710main.c
 * @brief TK8710 主测试程序
 * @note 完整的初始化、配置、工作和中断处理流程
 * 
 * RK3506 编译方法:
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port test8710main.c \
 *       ../port/tk8710_rk3506.c ../src/tk8710_irq.c ../src/tk8710_core.c \
 *       ../src/tk8710_config.c ../src/tk8710_log.c \
 *       -o test8710main -lgpiod -lpthread
 */

#define _GNU_SOURCE  /* 必须在所有头文件之前定义，用于CPU_ZERO/CPU_SET等宏 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include "tk8710_hal.h"
#include "hal_api.h"   /* HAL API接口 */
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_regs.h"
#include "trm/trm_api.h"        /* 添加TRM头文件 */
#include "trm/trm_log.h"     /* 添加TRM日志头文件 */
#include "trm_tx_validator.h"  /* 添加发送验证模块 */
#include "tk8710_ipc_comm.h"  /* 核间通信模块 */
#include "tk8710_scan_ipc_server.h"  /* Web扫频IPC服务 */
#include "tk8710_noise_api.h"           /* 噪底能量计算 API */

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <sched.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <direct.h>
#include <locale.h>
#define TK8710_CHDIR(path) _chdir(path)
#define TK8710_GETCWD(buf, size) _getcwd(buf, size)
#define TK8710_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#define TK8710_CHDIR(path) chdir(path)
#define TK8710_GETCWD(buf, size) getcwd(buf, size)
#define TK8710_MKDIR(path) mkdir(path, 0755)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/*============================================================================
 * 全局变量和配置
 *============================================================================*/

/* 运行标志 */
/* Version info */
#define TK8710_GW_VERSION          "1.0.0"
#define TK8710_GW_VERSION_PLATFORM "RK3506"
#define TK8710_GW_VERSION_STRING   TK8710_GW_VERSION " (" TK8710_GW_VERSION_PLATFORM ")"

/* Running flag */
static volatile int g_running = 1;
typedef enum {
    GW_INIT_IDLE = 0,
    GW_INIT_IN_PROGRESS,
    GW_INIT_SUCCESS,
    GW_INIT_FAILED
} GwInitState;

static volatile GwInitState g_init_state = GW_INIT_IDLE;
static volatile int g_init_error = 0;
static volatile int g_fatal_error = 0;
static uint8_t g_rf_tx_gain = 0x2a;
static uint32_t g_reg_a064_value = 0x00044003u;
static TK8710LogLevel g_driver_log_level = TK8710_LOG_WARN;
static TRMLogLevel g_trm_log_level = TRM_LOG_WARN;

#define TK8710_TX_DC_DIR  "TxDC"
#define TK8710_TX_DC_FILE TK8710_TX_DC_DIR "/txadc.txt"
#define TK8710_RF_I2C_ADDR_BASE 0x50u
#define TK8710_RF_DC_RAM_OFFSET 0x00u
#define TK8710_I2C_BUS_MAX      31
#define TK8710_CORRELATION_MAX_TERMINALS 128u
#define TK8710_CORRELATION_LOG_DIR       "/userdata/8710log"
#define TK8710_CORRELATION_LOG_FILE      TK8710_CORRELATION_LOG_DIR "/correlation.txt"

typedef struct {
    uint32_t terminal_id;
    uint32_t ah_data[16];
} GwCorrelationTerminal;

static int32_t DecodeAhComponent(uint32_t value)
{
    uint32_t raw = value & 0xFFFFFu;

    return (raw & 0x80000u) ? (int32_t)(raw - 0x100000u) : (int32_t)raw;
}

static int CompareCorrelationTerminal(const void* lhs, const void* rhs)
{
    const GwCorrelationTerminal* left = (const GwCorrelationTerminal*)lhs;
    const GwCorrelationTerminal* right = (const GwCorrelationTerminal*)rhs;

    if (left->terminal_id < right->terminal_id) {
        return -1;
    }
    if (left->terminal_id > right->terminal_id) {
        return 1;
    }
    return 0;
}

static int CalculateChannelCorrelation(const uint32_t* first_ah, const uint32_t* second_ah,
                                       double* correlation)
{
    double inner_real = 0.0;
    double inner_imag = 0.0;
    double first_energy = 0.0;
    double second_energy = 0.0;

    if (first_ah == NULL || second_ah == NULL || correlation == NULL) {
        return -1;
    }

    for (uint8_t antenna = 0; antenna < 8; antenna++) {
        double first_i = (double)DecodeAhComponent(first_ah[antenna * 2]);
        double first_q = (double)DecodeAhComponent(first_ah[antenna * 2 + 1]);
        double second_i = (double)DecodeAhComponent(second_ah[antenna * 2]);
        double second_q = (double)DecodeAhComponent(second_ah[antenna * 2 + 1]);

        /* (first_i + j*first_q) * conj(second_i + j*second_q) */
        inner_real += first_i * second_i + first_q * second_q;
        inner_imag += first_q * second_i - first_i * second_q;
        first_energy += first_i * first_i + first_q * first_q;
        second_energy += second_i * second_i + second_q * second_q;
    }

    if (first_energy == 0.0 || second_energy == 0.0) {
        return 1;
    }

    *correlation = hypot(inner_real, inner_imag) / sqrt(first_energy * second_energy);
    if (*correlation > 1.0) {
        *correlation = 1.0;
    }
    return 0;
}

static void ProcessChannelCorrelations(const TRM_RxDataList* rx_data_list,
                                       uint32_t system_frame)
{
    GwCorrelationTerminal terminals[TK8710_CORRELATION_MAX_TERMINALS];
    uint8_t terminal_count = 0;
    FILE* log_file = NULL;

    if (rx_data_list == NULL ||
        (rx_data_list->userCount > 0 && rx_data_list->users == NULL)) {
        printf("Channel correlation skipped: invalid RX data list\n");
        return;
    }

    for (uint8_t user_index = 0; user_index < rx_data_list->userCount; user_index++) {
        const TRM_RxUserData* user = &rx_data_list->users[user_index];
        uint32_t terminal_id = user->userId >> 8;
        uint8_t terminal_index;

        for (terminal_index = 0; terminal_index < terminal_count; terminal_index++) {
            if (terminals[terminal_index].terminal_id == terminal_id) {
                break;
            }
        }
        if (terminal_index == terminal_count) {
            if (terminal_count >= TK8710_CORRELATION_MAX_TERMINALS) {
                printf("Channel correlation terminal limit reached: %u\n",
                       TK8710_CORRELATION_MAX_TERMINALS);
                break;
            }
            terminals[terminal_count].terminal_id = terminal_id;
            terminal_count++;
        }
        memcpy(terminals[terminal_index].ah_data, user->beam.ahData,
               sizeof(terminals[terminal_index].ah_data));
    }

    qsort(terminals, terminal_count, sizeof(terminals[0]), CompareCorrelationTerminal);

    if (TK8710_MKDIR(TK8710_CORRELATION_LOG_DIR) != 0 && errno != EEXIST) {
        printf("Failed to create %s: %s\n", TK8710_CORRELATION_LOG_DIR, strerror(errno));
    } else {
        log_file = fopen(TK8710_CORRELATION_LOG_FILE, "a");
        if (log_file == NULL) {
            printf("Failed to open %s: %s\n", TK8710_CORRELATION_LOG_FILE, strerror(errno));
        }
    }

    if (log_file == NULL) {
        return;
    }

    fprintf(log_file,
            "=== Channel correlation: super_frame=%u system_frame=%u "
            "rx_users=%u terminals=%u ===\n",
            rx_data_list->frameNo, system_frame,
            rx_data_list->userCount, terminal_count);
    fprintf(log_file, "%-12s", "terminal_id");
    for (uint8_t column = 0; column < terminal_count; column++) {
        fprintf(log_file, "0x%06X    ", terminals[column].terminal_id);
    }
    fprintf(log_file, "\n");

    for (uint8_t row = 0; row < terminal_count; row++) {
        fprintf(log_file, "0x%06X    ", terminals[row].terminal_id);
        for (uint8_t column = 0; column < terminal_count; column++) {
            if (column < row) {
                fprintf(log_file, "%-12s", "");
            } else if (column == row) {
                fprintf(log_file, "%-12s", "1.000000");
            } else {
                double correlation = 0.0;
                int result = CalculateChannelCorrelation(terminals[row].ah_data,
                                                         terminals[column].ah_data,
                                                         &correlation);

                if (result == 0) {
                    fprintf(log_file, "%-12.6f", correlation);
                } else {
                    fprintf(log_file, "%-12s", "N/A");
                }
            }
        }
        fprintf(log_file, "\n");
    }
    fprintf(log_file, "\n");

    if (ferror(log_file)) {
        printf("Failed to write channel correlation log: %s\n", strerror(errno));
    }
    if (fclose(log_file) != 0) {
        printf("Failed to close %s: %s\n", TK8710_CORRELATION_LOG_FILE, strerror(errno));
    }
}

/**
 * @brief Read one uint32_t TX DC word from an RF module over I2C.
 */
#ifndef _WIN32
static int ReadRfTxDcWord(int fd, uint8_t slave_addr, uint32_t* value)
{
    uint8_t offset = TK8710_RF_DC_RAM_OFFSET;
    uint8_t data[sizeof(uint32_t)];
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data transfer;

    if (value == NULL) {
        return -1;
    }

    messages[0].addr = slave_addr;
    messages[0].flags = 0;
    messages[0].len = sizeof(offset);
    messages[0].buf = &offset;
    messages[1].addr = slave_addr;
    messages[1].flags = I2C_M_RD;
    messages[1].len = sizeof(data);
    messages[1].buf = data;

    transfer.msgs = messages;
    transfer.nmsgs = 2;
    if (ioctl(fd, I2C_RDWR, &transfer) < 0) {
        return -1;
    }

    /* RF RAM stores the uint32_t word in little-endian byte order. */
    *value = (uint32_t)data[0] |
             ((uint32_t)data[1] << 8) |
             ((uint32_t)data[2] << 16) |
             ((uint32_t)data[3] << 24);
    return 0;
}

static int OpenRfI2cBus(char* device_path, size_t path_size)
{
    int bus;

    for (bus = 0; bus <= TK8710_I2C_BUS_MAX; bus++) {
        uint32_t value;
        int antenna;
        int fd;

        snprintf(device_path, path_size, "/dev/i2c-%d", bus);
        fd = open(device_path, O_RDWR);
        if (fd < 0) {
            continue;
        }

        for (antenna = 0; antenna < TK8710_MAX_ANTENNAS; antenna++) {
            if (ReadRfTxDcWord(fd,
                               (uint8_t)(TK8710_RF_I2C_ADDR_BASE + antenna),
                               &value) != 0) {
                break;
            }
        }

        if (antenna == TK8710_MAX_ANTENNAS) {
            return fd;
        }
        close(fd);
    }

    device_path[0] = '\0';
    return -1;
}
#endif

/**
 * @brief Read eight RF TX DC words over I2C and save I/Q values to txadc.txt.
 */
static int SaveTxDcFromRfRam(void)
{
#ifdef _WIN32
    printf("I2C TX DC loading is only supported on RK3506 Linux\n");
    return -1;
#else
    uint32_t tx_dc_data[TK8710_MAX_ANTENNAS];
    char device_path[32];
    FILE* file;
    int fd;
    int i;

    fd = OpenRfI2cBus(device_path, sizeof(device_path));
    if (fd < 0) {
        printf("Failed to find an I2C bus containing RF modules 0x50~0x57\n");
        return -1;
    }

    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        uint8_t slave_addr = (uint8_t)(TK8710_RF_I2C_ADDR_BASE + i);

        if (ReadRfTxDcWord(fd, slave_addr, &tx_dc_data[i]) != 0) {
            printf("Failed to read RF TX DC: device=%s, slave=0x%02X, offset=0x%02X: %s\n",
                   device_path, slave_addr, TK8710_RF_DC_RAM_OFFSET, strerror(errno));
            close(fd);
            return -1;
        }
    }
    close(fd);

    if (TK8710_MKDIR(TK8710_TX_DC_DIR) != 0 && errno != EEXIST) {
        printf("Failed to create %s: %s\n", TK8710_TX_DC_DIR, strerror(errno));
        return -1;
    }

    file = fopen(TK8710_TX_DC_FILE, "w");
    if (file == NULL) {
        printf("Failed to open %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
        return -1;
    }

    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        uint16_t dc_i = (uint16_t)(tx_dc_data[i] >> 16);
        uint16_t dc_q = (uint16_t)(tx_dc_data[i] & 0xFFFFu);

        if (dc_i == UINT16_MAX) {
            printf("RF%d TX DC I is invalid (0xFFFF), using 0x0000\n", i);
            dc_i = 0;
        }
        if (dc_q == UINT16_MAX) {
            printf("RF%d TX DC Q is invalid (0xFFFF), using 0x0000\n", i);
            dc_q = 0;
        }

        if (fprintf(file, "0x%04X, 0x%04X\n", dc_i, dc_q) < 0) {
            printf("Failed to write %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
            fclose(file);
            return -1;
        }
        printf("RF%d TX DC: raw=0x%08X, I=0x%04X, Q=0x%04X\n",
               i, tx_dc_data[i], dc_i, dc_q);
    }

    if (fclose(file) != 0) {
        printf("Failed to close %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
        return -1;
    }

    printf("TX DC values read from %s and saved to %s\n",
           device_path, TK8710_TX_DC_FILE);
    return 0;
#endif
}

#ifndef _WIN32
/**
 * @brief Linux信号处理函数
 */
static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
    printf("\nReceived signal, exiting...\n");
}
#endif

 int set_cpu_affinity(int cpu_core) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_core, &cpu_set);
    
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) < 0) {
        perror("Failed to set CPU affinity");
        return -1;
    }
    
    printf("Process bound to CPU core %d\n", cpu_core);
    return 0;
}

/* 下行发送状态跟踪 */
static volatile bool g_hasValidUsers = false;     /* 是否有有效用户 */
static volatile bool g_txBeamCtrlMode = false;         /* 波束控制模式 */

/* TRM相关变量 */
static uint32_t g_trmSendCount = 0;               /* TRM发送计数 */
static uint32_t g_trmRxCount = 0;                 /* TRM接收计数 */

#define DRIVER_IRQ_STALL_TIMEOUT_SEC 120
#define CONSOLE_POLL_INTERVAL_SEC 10
#define CONFIG_APPLY_TIMEOUT_TICKS 600  /* 600 * 100 ms = 60 s */

/* 核间通信上下文由 src/tk8710_ipc_comm.c 定义 */

/* TRM回调函数声明 */
static void OnTrmRxData(const TRM_RxDataList* rxDataList);
static void OnTrmTxComplete(const TRM_TxCompleteResult* txResult);

/* 配置处理函数声明 */
static int HandleNsConfig(const NsConfigDown_t* config);
static int ApplyNsConfig(const NsConfigDown_t* config);

/* HAL是否已初始化标志 */
static int g_hal_initialized = 0;
static volatile uint8_t g_ns_config_started = 0;

/* 采集数据控制变量 */
static volatile uint8_t g_captureDataPending = 0;         /* 采集数据待执行标志 */
static volatile uint8_t g_captureDataPendingNum = 0;         /* 采集数据等待次数 */



void read_register(void)
{
    uint32_t addr, value;
    int ret;
    
    printf("\n=== 读取寄存器 ===\n");
    printf("输入寄存器地址 (十六进制，如 0xc030): ");
    
    if (scanf("%x", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr, &value);
    if (ret == TK8710_OK) {
        printf("寄存器 0x%08X = 0x%08X (%u)\n", addr, value, value);
    } else {
        printf("读取失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}

/**
 * @brief 写入寄存器
 */
void write_register(void)
{
    uint32_t addr, value;
    int ret;
    
    printf("\n=== 写入寄存器 ===\n");
    printf("输入寄存器地址 (十六进制，如 0xc030): ");
    
    if (scanf("%x", &addr) != 1) {
        printf("无效的地址格式\n");
        return;
    }
    
    printf("输入写入值 (十六进制，如 0x8): ");
    
    if (scanf("%x", &value) != 1) {
        printf("无效的值格式\n");
        return;
    }
    
    ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, addr, value);
    if (ret == TK8710_OK) {
        printf("写入成功: 0x%08X = 0x%08X (%u)\n", addr, value, value);
    } else {
        printf("写入失败: 错误码=%d\n", ret);
    }
    printf("==================\n\n");
}


/* 扫频功能已移至TRM层，现在使用TRM_SweepState结构体 */

/* NS速率索引到TK8710速率模式转换函数 */
static int StringEqualsIgnoreCase(const char* left, const char* right)
{
    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static int ParseDriverLogLevel(const char* text, TK8710LogLevel* level)
{
    if (StringEqualsIgnoreCase(text, "none"))  *level = TK8710_LOG_NONE;
    else if (StringEqualsIgnoreCase(text, "error")) *level = TK8710_LOG_ERROR;
    else if (StringEqualsIgnoreCase(text, "warn"))  *level = TK8710_LOG_WARN;
    else if (StringEqualsIgnoreCase(text, "info"))  *level = TK8710_LOG_INFO;
    else if (StringEqualsIgnoreCase(text, "debug")) *level = TK8710_LOG_DEBUG;
    else if (StringEqualsIgnoreCase(text, "trace")) *level = TK8710_LOG_TRACE;
    else if (StringEqualsIgnoreCase(text, "all"))   *level = TK8710_LOG_ALL;
    else return -1;
    return 0;
}

static int ParseTrmLogLevel(const char* text, TRMLogLevel* level)
{
    if (StringEqualsIgnoreCase(text, "none"))  *level = TRM_LOG_NONE;
    else if (StringEqualsIgnoreCase(text, "error")) *level = TRM_LOG_ERROR;
    else if (StringEqualsIgnoreCase(text, "warn"))  *level = TRM_LOG_WARN;
    else if (StringEqualsIgnoreCase(text, "info"))  *level = TRM_LOG_INFO;
    else if (StringEqualsIgnoreCase(text, "debug")) *level = TRM_LOG_DEBUG;
    else if (StringEqualsIgnoreCase(text, "trace")) *level = TRM_LOG_TRACE;
    else return -1;
    return 0;
}

static const char* DriverLogLevelName(TK8710LogLevel level)
{
    static const char* names[] = {"none", "error", "warn", "info", "debug", "trace", "all"};
    return (level >= TK8710_LOG_NONE && level <= TK8710_LOG_ALL) ? names[level] : "unknown";
}

static void ApplyRuntimeLogLevels(void)
{
    TK8710LogConfig(g_driver_log_level, TK8710_LOG_MODULE_ALL, 1);
    TRM_LogConfig(g_trm_log_level, 1);
}

static void PrintUsage(const char* prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --work-dir <dir>, -w <dir> : Set runtime directory for generated files\n");
    printf("  --rf-gain <gain>, -g <gain>: Set RF TX gain (0x00~0xFF, default: 0x2a)\n");
    printf("  --reg-a064 <value>          : Set register 0xA064 (default: 0x00045003)\n");
    printf("  --driver-log-level <level>  : Driver log: none|error|warn|info|debug|trace|all\n");
    printf("  --trm-log-level <level>     : TRM log: none|error|warn|info|debug|trace\n");
    printf("  --help, -h                 : Show this help\n");
}

static int ConfigureRuntimeDirectory(const char* path)
{
    char cwd[PATH_MAX];

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (TK8710_MKDIR(path) != 0 && errno != EEXIST) {
        printf("Failed to create runtime directory %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (TK8710_CHDIR(path) != 0) {
        printf("Failed to enter runtime directory %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (TK8710_GETCWD(cwd, sizeof(cwd)) != NULL) {
        printf("Runtime directory: %s\n", cwd);
    }

    return 0;
}

static int NormalizeRuntimeArgs(int* argc, char* argv[], const char** work_dir)
{
    int compact_argc = 1;
    int arg_index;

    if (argc == NULL || argv == NULL || work_dir == NULL) {
        return -1;
    }

    *work_dir = NULL;
    for (arg_index = 1; arg_index < *argc; arg_index++) {
        if (strcmp(argv[arg_index], "--help") == 0 || strcmp(argv[arg_index], "-h") == 0) {
            PrintUsage(argv[0]);
            return 1;
        }

        if (strcmp(argv[arg_index], "--work-dir") == 0 || strcmp(argv[arg_index], "-w") == 0) {
            if (arg_index + 1 >= *argc) {
                printf("Error: %s requires a directory path\n", argv[arg_index]);
                PrintUsage(argv[0]);
                return -1;
            }

            *work_dir = argv[++arg_index];
            continue;
        }

        if (strcmp(argv[arg_index], "--rf-gain") == 0 || strcmp(argv[arg_index], "-g") == 0) {
            char* end = NULL;
            unsigned long gain;

            if (arg_index + 1 >= *argc) {
                printf("Error: %s requires a gain value\n", argv[arg_index]);
                PrintUsage(argv[0]);
                return -1;
            }

            errno = 0;
            gain = strtoul(argv[++arg_index], &end, 0);
            if (errno != 0 || end == argv[arg_index] || *end != '\0' || gain > 0xFF) {
                printf("Error: Invalid RF gain '%s' (expected 0x00~0xFF)\n", argv[arg_index]);
                PrintUsage(argv[0]);
                return -1;
            }

            g_rf_tx_gain = (uint8_t)gain;
            continue;
        }

        if (strcmp(argv[arg_index], "--reg-a064") == 0) {
            char* end = NULL;
            unsigned long value;

            if (arg_index + 1 >= *argc) {
                printf("Error: %s requires a register value\n", argv[arg_index]);
                PrintUsage(argv[0]);
                return -1;
            }

            errno = 0;
            value = strtoul(argv[++arg_index], &end, 0);
            if (errno != 0 || argv[arg_index][0] == '-' || end == argv[arg_index] ||
                *end != '\0' || value > UINT32_MAX) {
                printf("Error: Invalid 0xA064 value '%s' (expected 0x00000000~0xFFFFFFFF)\n",
                       argv[arg_index]);
                PrintUsage(argv[0]);
                return -1;
            }

            g_reg_a064_value = (uint32_t)value;
            continue;
        }

        if (strcmp(argv[arg_index], "--driver-log-level") == 0) {
            if (arg_index + 1 >= *argc ||
                ParseDriverLogLevel(argv[++arg_index], &g_driver_log_level) != 0) {
                printf("Error: --driver-log-level expects "
                       "none|error|warn|info|debug|trace|all\n");
                PrintUsage(argv[0]);
                return -1;
            }
            continue;
        }

        if (strcmp(argv[arg_index], "--trm-log-level") == 0) {
            if (arg_index + 1 >= *argc ||
                ParseTrmLogLevel(argv[++arg_index], &g_trm_log_level) != 0) {
                printf("Error: --trm-log-level expects none|error|warn|info|debug|trace\n");
                PrintUsage(argv[0]);
                return -1;
            }
            continue;
        }

        printf("Error: Unknown argument %s\n", argv[arg_index]);
        PrintUsage(argv[0]);
        return -1;
    }

    *argc = compact_argc;
    return 0;
}

static int ConfigureRegA064(void)
{
    int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xA064u, g_reg_a064_value);

    if (ret != TK8710_OK) {
        printf("Failed to configure register 0xA064 = 0x%08X: ret=%d\n",
               g_reg_a064_value, ret);
        return ret;
    }

    printf("Register 0xA064 configured: 0x%08X\n", g_reg_a064_value);
    return TK8710_OK;
}

static uint8_t ConvertNsRateToTk8710Rate(uint8_t ns_rate) {
    switch (ns_rate) {
        case 0: return TK8710_RATE_MODE_5;
        case 1: return TK8710_RATE_MODE_6;
        case 2: return TK8710_RATE_MODE_7;
        case 3: return TK8710_RATE_MODE_8;
        case 4: return TK8710_RATE_MODE_9;
        case 5: return TK8710_RATE_MODE_10;
        case 6: return TK8710_RATE_MODE_11;
        case 7: return TK8710_RATE_MODE_18;
        default: 
            printf("⚠️  未知的NS速率索引: %d，使用默认速率7\n", ns_rate);
            return TK8710_RATE_MODE_7;
    }
}

/* 扫频模式到TK8710速率模式转换函数 */
static uint8_t ConvertSweepModeToTk8710Rate(int sweep_mode) {
    switch (sweep_mode) {
        case 0: return TK8710_RATE_MODE_5;   /* 62.5kHz */
        case 1: return TK8710_RATE_MODE_6;   /* 125kHz */
        case 2: return TK8710_RATE_MODE_7;   /* 250kHz */
        case 3: return TK8710_RATE_MODE_8;   /* 500kHz */
        default:
            printf("⚠️  未知的扫频模式: %d，使用默认模式8\n", sweep_mode);
            return TK8710_RATE_MODE_8;
    }
}

static uint8_t ConvertNsNetworkIdToBcnBits(int nwk_num)
{
    if (nwk_num < 0 || nwk_num > 0x1F) {
        uint8_t bcnbits = (uint8_t)(nwk_num & 0x1F);
        printf("⚠️  NS network id超出bcnbits范围: nwk_num=%d, 将截断为 %u\n",
               nwk_num, bcnbits);
        return bcnbits;
    }

    return (uint8_t)nwk_num;
}

static uint32_t GetDriverWatchdogIrqCount(uint8_t irq_type)
{
    uint32_t counters[10] = {0};

    TK8710GetAllIrqCounters(counters);
    if (irq_type >= 10) {
        return 0;
    }

    return counters[irq_type];
}

#ifndef _WIN32
static int ReadConsoleCommandWithIrqWatchdog(char* input, uint8_t watchdog_enabled)
{
    static uint8_t initialized = 0;
    static uint8_t prompt_shown = 0;
    static uint8_t console_checked = 0;
    static uint8_t console_input_enabled = 0;
    static uint32_t last_md_data_irq_count = 0;
    static uint32_t last_s3_irq_count = 0;
    static uint64_t last_md_data_change_sec = 0;
    static uint64_t last_s3_change_sec = 0;
    fd_set read_fds;
    struct timeval timeout;
    int select_ret;
    uint64_t now_sec;
    uint32_t current_md_data_irq_count;
    uint32_t current_s3_irq_count;

    if (!input) {
        return -1;
    }

    if (!console_checked) {
        console_input_enabled = isatty(STDIN_FILENO) ? 1 : 0;
        console_checked = 1;
    }

    if (!watchdog_enabled) {
        initialized = 0;
    } else if (!initialized) {
        last_md_data_irq_count = GetDriverWatchdogIrqCount(TK8710_IRQ_MD_DATA);
        last_s3_irq_count = GetDriverWatchdogIrqCount(TK8710_IRQ_S3);
        last_md_data_change_sec = (uint64_t)time(NULL);
        last_s3_change_sec = last_md_data_change_sec;
        initialized = 1;
    }

    if (console_input_enabled && !prompt_shown) {
        printf("TK8710> ");
        fflush(stdout);
        prompt_shown = 1;
    }

    FD_ZERO(&read_fds);
    if (console_input_enabled) {
        FD_SET(STDIN_FILENO, &read_fds);
    }
    timeout.tv_sec = CONSOLE_POLL_INTERVAL_SEC;
    timeout.tv_usec = 0;

    select_ret = select(console_input_enabled ? STDIN_FILENO + 1 : 0,
                        console_input_enabled ? &read_fds : NULL,
                        NULL, NULL, &timeout);
    if (select_ret < 0) {
        if (errno == EINTR) {
            return 0;
        }

        perror("select");
        return -1;
    }

    if (watchdog_enabled) {
        current_md_data_irq_count = GetDriverWatchdogIrqCount(TK8710_IRQ_MD_DATA);
        current_s3_irq_count = GetDriverWatchdogIrqCount(TK8710_IRQ_S3);
        now_sec = (uint64_t)time(NULL);

        if (current_md_data_irq_count != last_md_data_irq_count) {
            last_md_data_irq_count = current_md_data_irq_count;
            last_md_data_change_sec = now_sec;
        } else if (now_sec >= last_md_data_change_sec + DRIVER_IRQ_STALL_TIMEOUT_SEC) {
            printf("\nDriver MD_DATA interrupt count unchanged for %u seconds, exiting.\n",
                   DRIVER_IRQ_STALL_TIMEOUT_SEC);
            return -1;
        }

        if (current_s3_irq_count != last_s3_irq_count) {
            last_s3_irq_count = current_s3_irq_count;
            last_s3_change_sec = now_sec;
        } else if (now_sec >= last_s3_change_sec + DRIVER_IRQ_STALL_TIMEOUT_SEC) {
            printf("\nDriver S3 interrupt count unchanged for %u seconds, exiting.\n",
                   DRIVER_IRQ_STALL_TIMEOUT_SEC);
            return -1;
        }
    }

    if (select_ret == 0 || !console_input_enabled) {
        return 0;
    }

    if (scanf(" %c", input) != 1) {
        if (feof(stdin)) {
            printf("\nstdin closed, console input disabled.\n");
            console_input_enabled = 0;
            prompt_shown = 0;
            clearerr(stdin);
            return 0;
        }

        clearerr(stdin);
        return 0;
    }

    prompt_shown = 0;
    return 1;
}
#endif

/*============================================================================
 * 扫频函数实现
 *============================================================================*/

/**
 * @brief 扫频函数 - 设置扫频参数并启动扫频
 * @param start_freq 起始频率 (Hz), 例如 470000000 (470MHz)
 * @param end_freq 结束频率 (Hz), 例如 510000000 (510MHz)
 * @param sweep_mode 扫频模式: 0=62.5kHz(模式5), 1=125kHz(模式6), 2=250kHz(模式7), 3=500kHz(模式8)
 * @return 0成功, 负值失败
 * @note 参考 HandleNsConfig 实现 (步骤1-12), 仅设置扫频状态位
 *       实际扫频在中断中进行: 检测状态位 -> 采数计算噪底 -> 检测结束状态 -> 切换下一个频点
 */
static int DoFrequencySweep(uint32_t start_freq, uint32_t end_freq, int sweep_mode) {
    if (start_freq == 0 || end_freq == 0 || start_freq > end_freq) {
        printf("[扫频] 错误: 无效的频率参数\n");
        return -1;
    }
    
    if (sweep_mode < 0 || sweep_mode > 3) {
        printf("[扫频] 错误: 无效的扫频模式 %d\n", sweep_mode);
        return -1;
    }
    uint8_t rate_mode = ConvertSweepModeToTk8710Rate(sweep_mode);
    printf("[扫频] 扫频模式: %d, 速率模式: %d\n", sweep_mode, rate_mode);
    
    /* 扫频步进 - 根据扫频模式的带宽确定间隔 */
    static const uint32_t sweep_bandwidth_table[] = {
        62500,   /* 模式0: 62.5kHz (mode5) */
        125000,  /* 模式1: 125kHz (mode6) */
        250000,  /* 模式2: 250kHz (mode7) */
        500000   /* 模式3: 500kHz (mode8) */
    };
    uint32_t step_freq = (sweep_mode < 4) ? sweep_bandwidth_table[sweep_mode] : 62500;
    printf("[扫频] 扫频间隔: %u Hz\n", step_freq);
    
    printf("[扫频] ===== 配置步骤 1-12 (参考 HandleNsConfig) =====\n");
    
    printf("[扫频] 🔄 配置更新: 复位HAL并重新配置...\n");
    
    int ret;
    int trmRet;
    
    /* 1. 清理TRM系统资源 */
    trmRet = TRM_Deinit();
    if (trmRet != TRM_OK) {
        return TK8710_HAL_ERROR_RESET;
    }
    printf("[扫频] 步骤1: TRM系统资源已清理\n");
    
    /* 2. 复位TK8710芯片 */
    ret = TK8710Reset(TK8710_RST_STATE_MACHINE);
    ret = TK8710Reset(TK8710_RST_ALL);
    if (ret != TK8710_OK) {
        return TK8710_HAL_ERROR_RESET;
    }
    printf("[扫频] 步骤2: TK8710芯片已复位\n");
    
    /* 3. 准备RF配置 - 设置当前频率 */
    ChiprfConfig rfConfig;
    rfConfig.rftype = TK8710_RF_TYPE_1255_1M;
    rfConfig.Freq = start_freq;  /* 从起始频率开始 */
    rfConfig.rxgain = 0x7e;
    rfConfig.txgain = g_rf_tx_gain;
    uint16_t txadc_data[][2] = {
        {0x0450, 0x0450}, {0x0a00, 0x1080}, {0x0750, 0x1500}, {0x0400, 0x0b00},
        {0x08a0, 0x07a0}, {0x0990, 0xff00}, {0x0850, 0x08c8}, {0x0950, 0x0a00}
    };
    for (int i = 0; i < 8; i++) {
        rfConfig.txadc[i].i = (int16_t)txadc_data[i][0];
        rfConfig.txadc[i].q = (int16_t)txadc_data[i][1];
    }
    printf("[扫频] 步骤3: RF配置已准备 (频率=%u Hz)\n", rfConfig.Freq);
    
    /* 4. 准备芯片配置 */
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 0xff,
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = 0,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig
    };
    printf("[扫频] 步骤4: 芯片配置已准备\n");
    
    /* 5. 准备TRM配置 */
    TRM_InitConfig trmConfig;
    memset(&trmConfig, 0, sizeof(trmConfig));
    trmConfig.beamMode = TRM_BEAM_MODE_FULL_STORE;
    trmConfig.beamMaxUsers = 3000;
    trmConfig.beamTimeoutMs = 20000;
    trmConfig.callbacks.onRxData = OnTrmRxData;
    trmConfig.callbacks.onTxComplete = OnTrmTxComplete;
    trmConfig.maxFrameCount = 0;  /* 扫频模式: 持续运行 */
    printf("[扫频] 步骤5: TRM配置已准备\n");
    
    /* 6. 准备HAL初始化配置 */
    TK8710HalInitCfg halConfig = {
        .chipInitCfg = &chipConfig,
        .trmCfg = {
            .beamMaxUsers = trmConfig.beamMaxUsers,
            .beamTimeoutMs = trmConfig.beamTimeoutMs,
            .maxFrameCount = trmConfig.maxFrameCount,
            .onRxData = trmConfig.callbacks.onRxData,
            .onTxComplete = trmConfig.callbacks.onTxComplete
        }
    };
    printf("[扫频] 步骤6: HAL初始化配置已准备\n");
    
    /* 7. 调用 TK8710HalInit 完成初始化 */
    printf("[扫频] 步骤7: 调用 TK8710HalInit...\n");
    TK8710HalError halRet = TK8710HalInit(&halConfig);
    ApplyRuntimeLogLevels();
    if (halRet != TK8710_HAL_OK) {
        printf("[扫频] HAL初始化失败: %d\n", halRet);
        return -1;
    }
    printf("[扫频] 步骤7: HAL初始化完成\n");

    if (ConfigureRegA064() != TK8710_OK) {
        return -1;
    }
    
    /* 8-11. 配置时隙参数 (扫频模式简化配置) */
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    slotCfg.msMode = TK8710_MODE_MASTER;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 1;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
    slotCfg.txBcnAntEn = 0xff;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;
    
    /* 扫频模式使用单速率配置 */
    slotCfg.rateCount = 1;
    slotCfg.rateModes[0] = rate_mode;
    
    /* 根据速率模式设置da_m参数 */
    switch (slotCfg.rateModes[0]) {
        case TK8710_RATE_MODE_5:
            slotCfg.s1Cfg[0].da_m = 21492;
            slotCfg.s2Cfg[0].da_m = 21492;
            slotCfg.s3Cfg[0].da_m = 21492;
            break;
        case TK8710_RATE_MODE_6:
            slotCfg.s1Cfg[0].da_m = 19728;
            slotCfg.s2Cfg[0].da_m = 19728;
            slotCfg.s3Cfg[0].da_m = 19728;
            break;
        case TK8710_RATE_MODE_7:
            slotCfg.s1Cfg[0].da_m = 12000;
            slotCfg.s2Cfg[0].da_m = 12000;
            slotCfg.s3Cfg[0].da_m = 12000;
            break;
        case TK8710_RATE_MODE_8:
            slotCfg.s1Cfg[0].da_m = 5600;
            slotCfg.s2Cfg[0].da_m = 5600;
            slotCfg.s3Cfg[0].da_m = 5600;
            break;
        default:
            slotCfg.s1Cfg[0].da_m = 12000;
            slotCfg.s2Cfg[0].da_m = 12000;
            slotCfg.s3Cfg[0].da_m = 12000;
            break;
    }

    /* 配置时隙长度和频率 */
    slotCfg.s0Cfg[0].byteLen = 0;
    slotCfg.s0Cfg[0].centerFreq = start_freq;
    slotCfg.s1Cfg[0].byteLen = 26;   /* 2 * 26 */
    slotCfg.s1Cfg[0].centerFreq = start_freq;
    slotCfg.s2Cfg[0].byteLen = 26;
    slotCfg.s2Cfg[0].centerFreq = start_freq;
    slotCfg.s3Cfg[0].byteLen = 26;
    slotCfg.s3Cfg[0].centerFreq = start_freq;

    printf("[扫频] 步骤8-11: 时隙参数已配置 (rate_mode=%d)\n", rate_mode);
    
    /* 9. 调用 TK8710HalCfg 配置时隙 */
    TK8710HalError halRet_config = TK8710HalCfg(&slotCfg);
    if (halRet_config != TK8710_HAL_OK) {
        printf("[扫频] HAL config (slot) failed: %d\n", halRet_config);
        return -1;
    }
    printf("[扫频] 步骤9: TK8710HalCfg 完成\n");
    
    /* 10. 调用 TK8710HalStart 启动工作 */
    TK8710HalError halRet_start = TK8710HalStart();
    if (halRet_start != TK8710_HAL_OK) {
        printf("[扫频] HAL start failed: %d\n", halRet_start);
        return -1;
    }
    printf("[扫频] 步骤10: TK8710HalStart 完成\n");
    
    /* 12. 使用TRM API启动扫频 */
    ret = TRM_StartFrequencySweep(start_freq, end_freq, (uint8_t)sweep_mode, (uint8_t)rate_mode);
    if (ret != TRM_OK) {
        printf("[扫频] 错误: TRM_StartFrequencySweep 失败, 错误码=%d\n", ret);
        return ret;
    }
    
    printf("[扫频] ===== 步骤12: TRM扫频API调用完成 =====\n");
    printf("[扫频] 扫频已启动: 起始=%u Hz, 结束=%u Hz, 模式=%d\n",
           start_freq, end_freq, sweep_mode);
    printf("[扫频] 提示: TRM中断中会检测扫频状态位, 进行采数计算噪底, 检测结束状态后切换频点\n");
    
    return 0;
}

/*============================================================================
 * 配置处理函数实现
 *============================================================================*/

/**
 * @brief 处理NS配置消息
 * @param config NS配置数据
 * @return 0成功，负数失败
 */
static int ApplyNsConfig(const NsConfigDown_t* config) {
    uint8_t network_id;

    if (!config) {
        return -1;
    }

    g_ns_config_started = 0;
    network_id = ConvertNsNetworkIdToBcnBits(config->nwk_num);
    
    printf("开始处理NS配置 (HAL已初始化=%d)...\n", g_hal_initialized);
    printf("NS配置详情: freq=%u, nwk_num=%d, tdd_num=%d, slot_cfg=%d, rate_num=%d, bcnbits=%u\n",
           config->freq, config->nwk_num, config->tdd_num, config->slot_cfg,
           config->rate_num, network_id);
    
    /* 如果HAL已初始化，需要先复位再重新配置 */
    if (g_hal_initialized) {
        printf("🔄 配置更新: 复位HAL并重新配置...\n");
        
        // /* 1. 复位HAL */
        // TK8710HalError halRet_reset = TK8710HalReset();
        // if (halRet_reset != TK8710_HAL_OK) {
        //     printf("HAL复位失败: %d\n", halRet_reset);
        //     return -1;
        // }
        int ret;
        int trmRet;
        
        // 1. 清理TRM系统资源
        trmRet = TRM_Deinit();
        if (trmRet != TRM_OK) {
            printf("TRM deinitialization failed before reconfiguration: %d\n", trmRet);
            return TK8710_HAL_ERROR_RESET;
        }
        g_hal_initialized = 0;

        // 2. 复位TK8710芯片（复位状态机+寄存器）
        ret = TK8710Reset(TK8710_RST_STATE_MACHINE);
        if (ret != TK8710_OK) {
            printf("TK8710 state-machine reset failed before reconfiguration: %d\n", ret);
            return TK8710_HAL_ERROR_RESET;
        }

        ret = TK8710Reset(TK8710_RST_ALL);
        if (ret != TK8710_OK) {
            printf("TK8710 full reset failed before reconfiguration: %d\n", ret);
            return TK8710_HAL_ERROR_RESET;
        }

        // TK8710GpioIrqEnable(0, 0);
        
        // TK8710Rk3506Cleanup();
        printf("HAL复位完成\n");
        
        /* 短暂等待确保硬件稳定 */
        usleep(100000);  // 100ms
    }
    
    /* ========== 使用 HAL API 进行初始化 ========== */
    /* 1. 准备RF配置 */
    static ChiprfConfig rfConfig = {
        .rftype = TK8710_RF_TYPE_1255_1M,
        .Freq = 503100000,
        .rxgain = 0x7e,
        .txgain = 0x2a
        // .txadc = {//D号板
        //     {0x0350, 0x0490}, {0x0150, 0x0500}, {0x0450, 0x0490}, {0x0190, 0x0850},
        //     {0x0500, 0x0300}, {0xfe50, 0x0200}, {0x0190, 0x0550}, {0x03c0, 0x0400}
        // }
    };
    rfConfig.Freq = config->freq;
    rfConfig.txgain = g_rf_tx_gain;
    /* 2. 准备芯片配置 (与原 init_tk8710_chip 配置一致) */
    ChipConfig chipConfig = {
        .bcn_agc     = 32,
        .interval    = 32,
        .tx_dly      = 0,
        .tx_fix_info = 0,
        .offset_adj  = 0,
        .tx_pre      = 0,
        .conti_mode  = 1,
        .bcn_scan    = 0,
        .ant_en      = 0xFF,
        .rf_sel      = 0xFF,
        .tx_bcn_en   = 0xff,//0xff（8天线轮流发送bcn）
        .ts_sync     = 0,
        .rf_model    = 1,
        .bcnbits     = network_id,
        .anoiseThe1  = 0,
        .power2rssi  = 0,
        .irq_ctrl0   = 0x7FF,
        .irq_ctrl1   = 0,
        .spiConfig   = NULL,
        .rfConfig    = (struct ChiprfConfig_s*)&rfConfig  /* RF配置在TK8710Init中自动调用 */
    };
    
    /* 3. 准备TRM配置 (与原 init_trm_system 配置一致) */
    TRM_InitConfig trmConfig;
    memset(&trmConfig, 0, sizeof(trmConfig));
    trmConfig.beamMode = TRM_BEAM_MODE_FULL_STORE;
    trmConfig.beamMaxUsers = 3000;
    trmConfig.beamTimeoutMs = 20000;
    trmConfig.callbacks.onRxData = OnTrmRxData;
    trmConfig.callbacks.onTxComplete = OnTrmTxComplete;
    trmConfig.maxFrameCount = config->tdd_num;
    /* 4. 准备HAL初始化配置 */
    TK8710HalInitCfg halConfig = {
        .chipInitCfg = &chipConfig,
        .trmCfg = {
            .beamMaxUsers = trmConfig.beamMaxUsers,
            .beamTimeoutMs = trmConfig.beamTimeoutMs,
            .maxFrameCount = trmConfig.maxFrameCount,
            .onRxData = trmConfig.callbacks.onRxData,
            .onTxComplete = trmConfig.callbacks.onTxComplete
        }
    };
    /* 5. 调用 TK8710HalInit 完成芯片、RF、日志、TRM初始化 */
    printf("Initializing HAL (chip + RF + log + TRM)...\n");
    TK8710HalError halRet = TK8710HalInit(&halConfig);
    ApplyRuntimeLogLevels();
    if (halRet != TK8710_HAL_OK) {
        printf("HAL initialization failed: %d\n", halRet);
        return -1;
    }
    g_hal_initialized = 1;

    printf("HAL initialization completed (including RF)\n");

    if (ConfigureRegA064() != TK8710_OK) {
        return -1;
    }

    // 根据收到的配置重新配置时隙参数
    slotCfg_t slotCfg;
    memset(&slotCfg, 0, sizeof(slotCfg_t));
    
    // 配置基本参数
    slotCfg.msMode = TK8710_MODE_MASTER;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 1;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
    slotCfg.txBcnAntEn = 0xff;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;
    
    // 配置BCN轮流发送
    for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        slotCfg.bcnRotation[i] = i;
    }
    
    // 根据NS配置设置速率模式
    slotCfg.rateCount = config->rate_num;
    
    // 准备多速率时隙计算参数
    TRM_MultiRateSlotCalcInput multiSlotInput = {0};
    multiSlotInput.rateCount = config->rate_num;
    multiSlotInput.superFrameNum = config->tdd_num;
    multiSlotInput.calcType = TRM_SLOT_CALC_TYPE_GROUND_WAN;
    
    // 设置minGap位置：多速率时在最后一个速率的DL时隙添加gap，单速率时在DL时隙添加gap
    if (config->rate_num > 1) {
        multiSlotInput.minGapPos[0] = 0;  // BCN
        multiSlotInput.minGapPos[1] = 0;  // BRD
        multiSlotInput.minGapPos[2] = 0;  // UL
        multiSlotInput.minGapPos[3] = 1;  // DL (在最后一个速率的DL时隙添加gap)
    } else {
        multiSlotInput.minGapPos[0] = 0;  // BCN
        multiSlotInput.minGapPos[1] = 0;  // BRD
        multiSlotInput.minGapPos[2] = 0;  // UL
        multiSlotInput.minGapPos[3] = 1;  // DL
    }
    
    // 收集所有速率的参数
    for (int i = 0; i < config->rate_num && i < MAX_RATE_CFGS; i++) {
        slotCfg.rateModes[i] = ConvertNsRateToTk8710Rate(config->rate_cfgs[i].rate);
        multiSlotInput.rateModes[i] = slotCfg.rateModes[i];
        multiSlotInput.brdBlockNums[i] = 2;  // 广播包块数固定为2
        multiSlotInput.ulBlockNums[i] = config->rate_cfgs[i].uplink_pkt;     // 上行包块数
        multiSlotInput.dlBlockNums[i] = config->rate_cfgs[i].downlink_pkt;   // 下行包块数
    }
    
    // 使用多速率时隙计算函数计算gap参数
    TRM_MultiRateSlotCalcOutput multiSlotOutput;
    if (trm_calc_multi_rate_slot_config(&multiSlotInput, &multiSlotOutput) == 0) {
        printf("✅ 多速率时隙计算成功！总原始周期: %u us, 调整后周期: %u us, 添加gap: %u us\n", 
               multiSlotOutput.totalRawPeriod, multiSlotOutput.framePeriod, multiSlotOutput.addedGap);
        
        // 为每个速率配置gap参数
        for (int i = 0; i < config->rate_num && i < MAX_RATE_CFGS; i++) {
            slotCfg.s1Cfg[i].da_m = multiSlotOutput.rateConfigs[i].brdGap;
            slotCfg.s2Cfg[i].da_m = multiSlotOutput.rateConfigs[i].ulGap;
            slotCfg.s3Cfg[i].da_m = multiSlotOutput.rateConfigs[i].dlGap;
            printf("  速率[%d] 模式%d gap参数: BRD=%u, UL=%u, DL=%u\n", 
                   i, slotCfg.rateModes[i], 
                   multiSlotOutput.rateConfigs[i].brdGap, 
                   multiSlotOutput.rateConfigs[i].ulGap, 
                   multiSlotOutput.rateConfigs[i].dlGap);
        }
    } else {
        printf("❌ 多速率时隙计算失败，使用默认参数\n");
        // 使用默认参数
        for (int i = 0; i < config->rate_num && i < MAX_RATE_CFGS; i++) {
            slotCfg.s1Cfg[i].da_m = 12000;
            slotCfg.s2Cfg[i].da_m = 12000;
            slotCfg.s3Cfg[i].da_m = 12000;
        }
    }
    
    // 配置时隙长度和频点
    for (int i = 0; i < config->rate_num && i < MAX_RATE_CFGS; i++) {
        slotCfg.s0Cfg[i].byteLen = 0;
        slotCfg.s0Cfg[i].centerFreq = config->freq;
        
        // 根据模式确定包块长度：模式18使用40，其他模式使用26
        int blockSize = (slotCfg.rateModes[i] == 18) ? 40 : 26;
        
        slotCfg.s1Cfg[i].byteLen = blockSize * 2;
        slotCfg.s1Cfg[i].centerFreq = config->freq;
        slotCfg.s2Cfg[i].byteLen = config->rate_cfgs[i].uplink_pkt * blockSize;
        slotCfg.s2Cfg[i].centerFreq = config->freq;
        slotCfg.s3Cfg[i].byteLen = config->rate_cfgs[i].downlink_pkt * blockSize;
        slotCfg.s3Cfg[i].centerFreq = config->freq;
    }
    
    // 调用 TK8710HalCfg 配置时隙
    TK8710HalError halRet_config = TK8710HalCfg(&slotCfg);
    if (halRet_config != TK8710_HAL_OK) {
        printf("HAL config (slot) failed: %d\n", halRet_config);
        return -1;
    }
    
    printf("✅ 根据NS配置完成时隙参数配置\n");

    // printf("配置为单天线接收模式...\n");
    // int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, 0xc02c, 0x00010101);
    // if (ret == TK8710_OK) {
    //     printf("单天线接收模式配置成功 (0xc02c = 0x00010101)\n");
    // } else {
    //     printf("单天线接收模式配置失败: ret=%d\n", ret);
    // }

    /* 12. 调用 TK8710HalStart 启动工作 */
    TK8710HalError halRet_start = TK8710HalStart();
    if (halRet_start != TK8710_HAL_OK) {
        printf("HAL start failed: %d\n", halRet_start);
        return -1;
    }
    printf("HAL started successfully (Master mode, Continuous work)\n");
    g_ns_config_started = 1;

    if (!TK8710ScanIpcServerIsRunning()) {
        if (TK8710ScanIpcServerStart() == 0) {
            printf("Web扫频IPC服务已启动: /tmp/data_collect.sock\n");
        } else {
            printf("Web扫频IPC服务启动失败，Web侧无法触发扫频\n");
        }
    }
    
    /* 标记HAL已初始化 */
    g_hal_initialized = 1;

    return 0;
}

/* IPC接收线程通过此包装函数把配置结果同步给main。 */
static int HandleNsConfig(const NsConfigDown_t* config)
{
    int ret;

    g_init_state = GW_INIT_IN_PROGRESS;
    g_init_error = 0;

    ret = ApplyNsConfig(config);
    if (ret != 0) {
        g_init_error = ret;
        g_init_state = GW_INIT_FAILED;
        g_fatal_error = 1;
        g_running = 0;
        printf("Fatal: NS configuration failed: %d; requesting program shutdown.\n", ret);
        return ret;
    }

    g_init_state = GW_INIT_SUCCESS;
    return 0;
}

/*============================================================================
 * TRM回调函数实现
 *============================================================================*/

/**
 * @brief TRM接收数据回调
 * @param rxDataList 接收数据列表
 */
static void OnTrmRxData(const TRM_RxDataList* rxDataList)
{
    static uint8_t max_rx_user_count = 0;
    uint32_t system_frame;

    if (rxDataList == NULL) {
        printf("TRM RX callback received a NULL data list\n");
        return;
    }

    system_frame = TRM_GetCurrentFrame();
    printf("=== TRM接收数据事件 (超帧号：%u), (系统帧号：%u) ===\n", rxDataList->frameNo, system_frame);
    printf("时隙: 用户数=%d\n", 
           rxDataList->userCount);
    
    /* 打印第一个用户的速率模式信息（如果有用户数据） */
    if (rxDataList->userCount > 0 && rxDataList->users) {
        TRM_RxUserData* firstUser = &rxDataList->users[0];
        printf("第一个用户信息: ID=0x%08X, 速率模式=%d, 数据长度=%u\n", 
               firstUser->userId, firstUser->rateMode, firstUser->dataLen);
    }
    
    g_trmRxCount += rxDataList->userCount;
    if (rxDataList->userCount > max_rx_user_count) {
        max_rx_user_count = rxDataList->userCount;
        ProcessChannelCorrelations(rxDataList, system_frame);
    }
    
    // for (uint8_t i = 0; i < rxDataList->userCount; i++) {
    //     TRM_RxUserData* user = &rxDataList->users[i];
    //     printf("  用户[%d]: ID=0x%08X, 长度=%d, RSSI=%d, SNR=%d, Freq=%d Hz\n", 
    //            i, user->userId, user->dataLen, user->rssi, user->snr, user->freq/128);
        
    //     /* 显示数据内容 */
    //     if (user->data != NULL && user->dataLen > 0) {
    //         printf("    数据: ");
    //         for (int k = 0; k < user->dataLen && k < 8; k++) {
    //             printf("%02X ", user->data[k]);
    //         }
    //         if (user->dataLen > 8) printf("...");
    //         printf("\n");
    //     }
    // }
    
    // /* 调用发送验证器 */
    // int ret = TRM_TxValidatorOnRxData(rxDataList);
    // if (ret != TRM_OK) {
    //     printf("  验证器处理失败: 错误码=%d\n", ret);
    // }
    // /* 显示验证统计信息 */
    // TRM_TxValidatorStats stats;
    // if (TRM_TxValidatorGetStats(&stats) == TRM_OK) {
    //     printf("  发送统计: 总触发=%u, 成功=%u, 失败=%u, 接收总次数=%u\n", 
    //            stats.totalTriggerCount, stats.successSendCount, stats.failedSendCount, g_trmRxCount);
    // }
    
    /* 发送上行数据给NS（通过核间通信） */
    IpcSendUplinkData(&g_ipc_ctx, rxDataList);
    
    /* 检查是否需要执行采集数据 */
    if (g_captureDataPending && g_captureDataPendingNum == 2) {
        printf("执行采集数据功能...\n");
        int captureRet = TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA, TK8710_DBG_OPT_GET, NULL, NULL);
        if (captureRet == TK8710_OK) {
            printf("采集数据功能执行成功\n");
            /* 采集数据成功后计算噪底能量 */
            tk8710_noise_process("8710CaptureData", 6, "ANoise.txt");
        } else {
            printf("采集数据功能执行失败: ret=%d\n", captureRet);
        }
        g_captureDataPending = 0;  /* 清除待执行标志 */
        g_captureDataPendingNum = 0;
    }
    g_captureDataPendingNum++;
    printf("==================\n");
}


/**
 * @brief TRM发送完成回调
 * @param txResult 发送完成结果
 */
static void OnTrmTxComplete(const TRM_TxCompleteResult* txResult)
{
    if (!txResult) return;

    printf("=== TRM发送完成事件,(超帧号: %u),(系统帧号：%u) ===\n",txResult->superFrameNo, TRM_GetCurrentFrame());
    printf("发送用户总数: %u, 剩余队列: %u\n", txResult->totalUsers, txResult->remainingQueue);
    g_trmSendCount += txResult->userCount;
    /* 打印每个用户的发送结果 */
    // const char* resultStr[] = {"OK", "NO_BEAM", "TIMEOUT", "ERROR"};
    // for (uint32_t i = 0; i < txResult->userCount; i++) {
    //     const TRM_TxUserResult* userResult = &txResult->users[i];
    //     printf("  用户ID: 0x%08X, 结果: %s\n", userResult->userId, resultStr[userResult->result]);
    // }
    printf("==================\n");
}

/**
 * @brief 显示TRM统计信息
 */
void show_trm_statistics(void)
{
    printf("\n=== TRM统计信息 ===\n");
    printf("发送计数: %u\n", g_trmSendCount);
    printf("接收计数: %u\n", g_trmRxCount);
    
    /* 获取TRM内部统计 */
    TRM_Stats stats;
    if (TRM_GetStats(&stats) == TRM_OK) {
        printf("TRM内部统计:\n");
        printf("  发送次数: %u\n", stats.txCount);
        printf("  发送成功: %u\n", stats.txSuccessCount);
        printf("  接收次数: %u\n", stats.rxCount);
        printf("  波束数量: %u\n", stats.beamCount);
        printf("当前帧号: %u\n", TRM_GetCurrentFrame());
    }
    
    printf("==================\n\n");
}

/*============================================================================
 * 主程序
 *============================================================================*/

/**
 * @brief 显示帮助信息
 */
void show_help(void)
{
    printf("\n=== TK8710 Driver + TRM Test Commands ===\n");
    printf("Driver Commands:\n");
    printf("  h/H - Show this help information\n");
    printf("  s/S - Show system status\n");
    printf("  i/I - Show interrupt statistics\n");
    printf("  c/C - Clear screen\n");
    printf("  q/Q - Quit program\n");
    printf("\nTRM Commands (when TRM enabled):\n");
    printf("  t/T - Show TRM statistics\n");
    printf("  a/A - Start frequency sweep\n");
    printf("\nDriver Test Commands:\n");
    printf("  m/M - Manual downlink test (single user)\n");
    printf("  r/R - Reset chip\n");
    printf("+--------------------------------------+\n");
}
void show_system_status(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();
    uint8_t rateMode = TK8710GetRateMode();
    uint8_t workType = TK8710GetWorkType();
    uint8_t brdUserNum = TK8710GetBrdUserNum();
    
    printf("\n=== System Status ===\n");
    printf("Version: %s\n", TK8710_GW_VERSION_STRING);
    printf("Work mode: %s\n", workType == TK8710_MODE_MASTER ? "Master" : "Slave");
    printf("Rate mode: %d\n", rateMode);
    printf("Broadcast users: %d\n", brdUserNum);
    printf("Antenna enable: 0x%02X\n", slotCfg->antEn);
    printf("RF select: 0x%02X\n", slotCfg->rfSel);
    printf("Transmit mode: %s\n", slotCfg->txBeamCtrlMode ? "Specified info transmit" : "Auto transmit");
    printf("BCN antenna enable: %s\n", slotCfg->txBcnAntEn ? "Yes" : "No");
    printf("Slot configuration:\n");
    printf("  S1(FDL): %d bytes, freq: %u\n", slotCfg->s1Cfg[0].byteLen, slotCfg->s1Cfg[0].centerFreq);
    printf("  S2(ADL): %d bytes, freq: %u\n", slotCfg->s2Cfg[0].byteLen, slotCfg->s2Cfg[0].centerFreq);
    printf("  S3(UL):  %d bytes, freq: %u\n", slotCfg->s3Cfg[0].byteLen, slotCfg->s3Cfg[0].centerFreq);
    printf("=== Status End ===\n\n");
}

/**
 * @brief 显示中断统计
 */
void show_irq_statistics(void)
{
    uint32_t counters[10];
    uint32_t irqStatus;
    
    printf("\n=== Interrupt Statistics ===\n");
    
    /* 获取所有中断计数器 */
    TK8710GetAllIrqCounters(counters);
    
    /* 获取当前中断状态 */
    irqStatus = TK8710GetIrqStatus();
    
    printf("Interrupt counters:\n");
    printf("  RX_BCN (0):    %u\n", counters[0]);
    printf("  BRD_UD (1):    %u\n", counters[1]);
    printf("  BRD_DATA (2):  %u\n", counters[2]);
    printf("  MD_UD (3):      %u\n", counters[3]);
    printf("  MD_DATA (4):   %u\n", counters[4]);
    printf("  S0 (5):        %u\n", counters[5]);
    printf("  S1 (6):        %u\n", counters[6]);
    printf("  S2 (7):        %u\n", counters[7]);
    printf("  S3 (8):        %u\n", counters[8]);
    printf("  ACM (9):       %u\n", counters[9]);
    
    printf("Current interrupt status: 0x%08X\n", irqStatus);
    printf("=== Statistics End ===\n\n");
}

static int read_sweep_frequency_hz(const char* prompt, uint32_t* freq_hz)
{
    char text[32];
    char* end_ptr;
    double value;
    double hz;

    if (prompt == NULL || freq_hz == NULL) {
        return -1;
    }

    printf("%s", prompt);
    fflush(stdout);

    if (scanf("%31s", text) != 1) {
        clearerr(stdin);
        return -1;
    }

    errno = 0;
    value = strtod(text, &end_ptr);
    if (end_ptr == text || errno != 0 || value <= 0.0) {
        return -1;
    }

    if (*end_ptr == '\0') {
        hz = (value < 1000000.0) ? value * 1000000.0 : value;
    } else if (strcmp(end_ptr, "M") == 0 || strcmp(end_ptr, "m") == 0 ||
               strcmp(end_ptr, "MHz") == 0 || strcmp(end_ptr, "mhz") == 0) {
        hz = value * 1000000.0;
    } else if (strcmp(end_ptr, "K") == 0 || strcmp(end_ptr, "k") == 0 ||
               strcmp(end_ptr, "kHz") == 0 || strcmp(end_ptr, "khz") == 0) {
        hz = value * 1000.0;
    } else {
        return -1;
    }

    if (hz <= 0.0 || hz > (double)UINT32_MAX) {
        return -1;
    }

    *freq_hz = (uint32_t)(hz + 0.5);
    return 0;
}

static void start_frequency_sweep_from_console(void)
{
    uint32_t start_freq;
    uint32_t end_freq;
    const int sweep_mode = 3;
    int ret;

    printf("\n=== Frequency Sweep ===\n");
    printf("Input format: Hz or MHz, for example 470000000 or 470.5M\n");

    if (read_sweep_frequency_hz("Start frequency: ", &start_freq) != 0 ||
        read_sweep_frequency_hz("End frequency: ", &end_freq) != 0) {
        printf("[Sweep] Invalid frequency input\n");
        return;
    }

    if (start_freq > end_freq) {
        printf("[Sweep] Invalid range: start=%u Hz, end=%u Hz\n", start_freq, end_freq);
        return;
    }

    printf("[Sweep] Starting: start=%u Hz, end=%u Hz, mode=%d (500 kHz step)\n",
           start_freq, end_freq, sweep_mode);
    ret = DoFrequencySweep(start_freq, end_freq, sweep_mode);
    if (ret != 0) {
        printf("[Sweep] Start failed: ret=%d\n", ret);
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[])
{
    char input;
    const char* work_dir = NULL;
    int exit_code = 0;
    int ipc_started = 0;
    int arg_ret = NormalizeRuntimeArgs(&argc, argv, &work_dir);
    if (arg_ret != 0) {
        return arg_ret > 0 ? 0 : 1;
    }

    if (ConfigureRuntimeDirectory(work_dir) != 0) {
        return 1;
    }
    printf("RF TX gain: 0x%02X\n", g_rf_tx_gain);
    printf("Register 0xA064 value: 0x%08X\n", g_reg_a064_value);
    printf("Driver log level: %s\n", DriverLogLevelName(g_driver_log_level));
    printf("TRM log level: %s\n", TRM_LogGetLevelName(g_trm_log_level));
    // if (SaveTxDcFromRfRam() != 0) {
    //     printf("Warning: failed to refresh %s from RF RAM\n", TK8710_TX_DC_FILE);
    // }

    // Set CPU affinity to core 2
    if (set_cpu_affinity(2) < 0) {
        return 1;
    }
#ifdef _WIN32
    /* 设置控制台编码为UTF-8 */
    SetConsoleOutputCP(65001);  // UTF-8
    SetConsoleCP(65001);       // UTF-8
    setlocale(LC_ALL, ".UTF8");
#endif
    
    printf("\n");
    printf("+======================================+\n");
    printf("|   TK8710 Main Test Program           |\n");
    printf("|   Version: %-25s|\n", TK8710_GW_VERSION_STRING);
    printf("|   Complete Init, Config, Workflow   |\n");
    printf("+======================================+\n");
    
#ifndef _WIN32
    /* 注册Linux信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    
    /* 6. 启动核间通信 */
    printf("启动核间通信...\n");
    
    // 设置配置处理回调函数
    IpcCommSetConfigHandler(HandleNsConfig);
    
    if (IpcCommInit(&g_ipc_ctx) != 0) {
        printf("核间通信初始化失败\n");
        return -1;
    }
    if (IpcCommStart(&g_ipc_ctx) != 0) {
        printf("核间通信启动失败\n");
        IpcCommCleanup(&g_ipc_ctx);
        return -1;
    }
    ipc_started = 1;
    printf("核间通信已启动，等待配置消息...\n");
    
    /* 7. 等待配置消息 */
    printf("等待来自NS的配置消息...\n");
    
    // 每隔10秒发送一次配置请求，最多请求3次
    int request_count = 0;
    while (!IpcCommIsConfigReceived() && request_count < 3) {
        // 发送配置请求
        printf("📤 发送第%d次配置请求...\n", request_count + 1);
        if (IpcCommSendConfigRequest(&g_ipc_ctx) != 0) {
            printf("❌ 配置请求发送失败\n");
        }
        
        // 等待10秒
        printf("⏳ 等待10秒以接收配置...\n");
        for (int i = 0; i < 100 && !IpcCommIsConfigReceived(); i++) {
            usleep(100000); // 等待100ms
        }
        
        request_count++;
    }
    
    if (IpcCommIsConfigReceived()) {
        int wait_ticks = 0;

        /* IPC marks the message received before HandleNsConfig returns. */
        while (g_init_state != GW_INIT_SUCCESS &&
               g_init_state != GW_INIT_FAILED &&
               wait_ticks < CONFIG_APPLY_TIMEOUT_TICKS) {
            usleep(100000);
            wait_ticks++;
        }

        if (g_init_state != GW_INIT_SUCCESS) {
            if (g_init_state == GW_INIT_FAILED) {
                printf("Fatal: received NS configuration but applying it failed: %d.\n",
                       g_init_error);
            } else {
                printf("Fatal: timed out waiting for NS configuration to finish.\n");
                g_init_error = -1;
                g_init_state = GW_INIT_FAILED;
                g_fatal_error = 1;
                g_running = 0;
            }
            exit_code = 1;
            goto shutdown;
        }
    }

    if (!IpcCommIsConfigReceived()) {
        g_init_state = GW_INIT_IN_PROGRESS;
        printf("⚠️  已请求10次仍未收到配置消息，使用默认配置继续\n");
        // 使用默认配置进行时隙配置
            /* ========== 使用 HAL API 进行初始化 ========== */
        /* 1. 准备RF配置 */
        static ChiprfConfig rfConfig = {
            .rftype = TK8710_RF_TYPE_1255_1M,
            .Freq = 483800000,
            .rxgain = 0x7e,
            .txgain = 0x2a,
            // .txadc = {//C号板
            //     {0x0bc0, 0x04a0}, {0x0a50, 0x0780}, {0x0750, 0x0820}, {0x0bc3, 0x0940},
            //     {0x0e83, 0x05e0}, {0xfbff, 0x0850}, {0x0880, 0x0500}, {0x02a0, 0x06ff}
            // }
            // .txadc = {//2号板
            //     {0x0c90, 0x1190}, {0xfe30, 0x0220}, {0x0210, 0x01a0}, {0x0b70, 0x07b0},
            //     {0x03ae, 0x0980}, {0x0740, 0x0990}, {0x0930, 0x0680}, {0x0df0, 0x0190}
            // }
        };
        rfConfig.txgain = g_rf_tx_gain;
        
        /* 2. 准备芯片配置 (与原 init_tk8710_chip 配置一致) */
        ChipConfig chipConfig = {
            .bcn_agc     = 32,
            .interval    = 32,
            .tx_dly      = 0,
            .tx_fix_info = 0,
            .offset_adj  = 0,
            .tx_pre      = 0,
            .conti_mode  = 1,
            .bcn_scan    = 0,
            .ant_en      = 0xFF,
            .rf_sel      = 0xFF,
            .tx_bcn_en   = 1,
            .ts_sync     = 0,
            .rf_model    = 1,
            .bcnbits     = 0,
            .anoiseThe1  = 0,
            .power2rssi  = 0,
            .irq_ctrl0   = 0x7FF,
            .irq_ctrl1   = 0,
            .spiConfig   = NULL,
            .rfConfig    = (struct ChiprfConfig_s*)&rfConfig  /* RF配置在TK8710Init中自动调用 */
        };
        
        /* 3. 准备TRM配置 (与原 init_trm_system 配置一致) */
        TRM_InitConfig trmConfig;
        memset(&trmConfig, 0, sizeof(trmConfig));
        trmConfig.beamMode = TRM_BEAM_MODE_FULL_STORE;
        trmConfig.beamMaxUsers = 3000;
        trmConfig.beamTimeoutMs = 10000;
        trmConfig.callbacks.onRxData = OnTrmRxData;
        trmConfig.callbacks.onTxComplete = OnTrmTxComplete;
        trmConfig.maxFrameCount = 2;
        /* 4. 准备HAL初始化配置 */
        TK8710HalInitCfg halConfig = {
            .chipInitCfg = &chipConfig,
            .trmCfg = {
                .beamMaxUsers = trmConfig.beamMaxUsers,
                .beamTimeoutMs = trmConfig.beamTimeoutMs,
                .maxFrameCount = trmConfig.maxFrameCount,
                .onRxData = trmConfig.callbacks.onRxData,
                .onTxComplete = trmConfig.callbacks.onTxComplete
            }
        };
        /* 5. 调用 TK8710HalInit 完成芯片、RF、日志、TRM初始化 */
        printf("Initializing HAL (chip + RF + log + TRM)...\n");
        TK8710HalError halRet = TK8710HalInit(&halConfig);
        ApplyRuntimeLogLevels();
        if (halRet != TK8710_HAL_OK) {
            printf("HAL initialization failed: %d\n", halRet);
            g_init_error = halRet;
            g_init_state = GW_INIT_FAILED;
            exit_code = 1;
            goto shutdown;
        }
        g_hal_initialized = 1;

        printf("HAL initialization completed (including RF)\n");

        if (ConfigureRegA064() != TK8710_OK) {
            g_init_error = -1;
            g_init_state = GW_INIT_FAILED;
            exit_code = 1;
            goto shutdown;
        }
        slotCfg_t slotCfg;
        memset(&slotCfg, 0, sizeof(slotCfg_t));
        
        // 配置基本参数 (使用默认值)
        slotCfg.msMode = TK8710_MODE_MASTER;
        slotCfg.plCrcEn = 0;
        slotCfg.brdUserNum = 1;
        slotCfg.antEn = 0xFF;
        slotCfg.rfSel = 0xFF;
        slotCfg.txBeamCtrlMode = 1;
        g_txBeamCtrlMode = (slotCfg.txBeamCtrlMode == 0);
        slotCfg.txBcnAntEn = 0xff;
        slotCfg.rx_delay = 0;
        slotCfg.md_agc = 1024;
        slotCfg.brdFreq[0] = 20000.0;
        slotCfg.frameTimeLen = 0;
        
        // 配置BCN轮流发送
        for (int i = 0; i < TK8710_MAX_ANTENNAS; i++) {
            slotCfg.bcnRotation[i] = i;
        }
        
        // 使用默认单速率配置
        slotCfg.rateCount = 1;
        slotCfg.rateModes[0] = TK8710_RATE_MODE_8;
        slotCfg.s0Cfg[0].byteLen = 0;
        slotCfg.s0Cfg[0].centerFreq = 483800000;
        slotCfg.s1Cfg[0].byteLen = 52;
        slotCfg.s1Cfg[0].centerFreq = 483800000;
        slotCfg.s2Cfg[0].byteLen = 26;
        slotCfg.s2Cfg[0].centerFreq = 483800000;
        slotCfg.s3Cfg[0].byteLen = 26;
        slotCfg.s3Cfg[0].centerFreq = 483800000;
        switch (slotCfg.rateModes[0]) {
            case TK8710_RATE_MODE_5:
                slotCfg.s1Cfg[0].da_m = 21492;
                slotCfg.s2Cfg[0].da_m = 21492;
                slotCfg.s3Cfg[0].da_m = 21492;
                break;
            case TK8710_RATE_MODE_6:
                slotCfg.s1Cfg[0].da_m = 19728;
                slotCfg.s2Cfg[0].da_m = 19728;
                slotCfg.s3Cfg[0].da_m = 19728;
                break;
            case TK8710_RATE_MODE_7:
                slotCfg.s1Cfg[0].da_m = 12000;
                slotCfg.s2Cfg[0].da_m = 12000;
                slotCfg.s3Cfg[0].da_m = 12000;
                break;
            case TK8710_RATE_MODE_8:
                slotCfg.s1Cfg[0].da_m = 5600;
                slotCfg.s2Cfg[0].da_m = 5600;
                slotCfg.s3Cfg[0].da_m = 5600;
                break;
            default:
                slotCfg.s1Cfg[0].da_m = 12000;
                slotCfg.s2Cfg[0].da_m = 12000;
                slotCfg.s3Cfg[0].da_m = 12000;
                break;
        }
        
        TK8710HalError halRet_config = TK8710HalCfg(&slotCfg);
        if (halRet_config != TK8710_HAL_OK) {
            printf("HAL config (slot) failed: %d\n", halRet_config);
            g_init_error = halRet_config;
            g_init_state = GW_INIT_FAILED;
            exit_code = 1;
            goto shutdown;
        }
        printf("使用默认配置完成时隙参数配置\n");

        /* 12. 调用 TK8710HalStart 启动工作 */
        TK8710HalError halRet_start = TK8710HalStart();
        if (halRet_start != TK8710_HAL_OK) {
            printf("HAL start failed: %d\n", halRet_start);
            g_init_error = halRet_start;
            g_init_state = GW_INIT_FAILED;
            exit_code = 1;
            goto shutdown;
        }
        printf("HAL started successfully (Master mode, Continuous work)\n");
        g_ns_config_started = 1;
        g_init_state = GW_INIT_SUCCESS;
        if (!TK8710ScanIpcServerIsRunning()) {
            if (TK8710ScanIpcServerStart() == 0) {
                printf("Web扫频IPC服务已启动: /tmp/data_collect.sock\n");
            } else {
                printf("Web扫频IPC服务启动失败，Web侧无法触发扫频\n");
            }
        }

    } else {
        printf("✅ 已收到并处理配置消息\n");
    }
    
    /* 8. 主循环 - 等待中断并进行中断处理 */
    while (g_running) {
        if (TRM_IsShutdownRequested()) {
            uint32_t failure_count = TRM_GetAcmConsecutiveFailureCount();

            fprintf(stderr,
                    "FATAL: ACM calibration failed %u consecutive times; "
                    "stopping gateway safely.\n",
                    failure_count);
            g_fatal_error = 1;
            exit_code = 1;
            g_running = 0;
            break;
        }
#ifdef _WIN32
        printf("TK8710> ");
        input = _getch();
        if (input != '\r') {
            printf("%c\n", input);
        } else {
            printf("\n");
            continue;
        }
#else
        int read_ret = ReadConsoleCommandWithIrqWatchdog(&input, g_ns_config_started);
        // int read_ret = ReadConsoleCommandWithIrqWatchdog(&input, 0);
        if (read_ret < 0) {
            exit_code = 1;
            g_running = 0;
            break;
        }
        if (read_ret == 0) {
            continue;
        }
#endif
        
        switch (input) {
            case 'h':
            case 'H':
                show_help();
                break;
                
            case 's':
            case 'S':
                show_system_status();
                break;
                
            case 'i':
            case 'I':
                show_irq_statistics();
                break;
                
            case 'c':
            case 'C':
#ifdef _WIN32
                system("cls");
#else
                system("clear");
#endif
                break;
                
            case 't':
            case 'T':
                show_trm_statistics();
                break;

            case 'r':
            case 'R':
                read_register();
                break;
                
            case 'w':
            case 'W':
                write_register();
                break;            
            case 'a':
            case 'A':
                start_frequency_sweep_from_console();
                break;

            case 'd':
            case 'D':
                printf("采集数据功能已设置，将在下次MD_DATA中断时执行\n");
                s_ram_rd0 ramRd0;
                ramRd0.data = 0;
                ramRd0.b.cap_en = 1;  // 启用采集
                int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0), ramRd0.data);
                if (ret != TK8710_OK) {
                    TK8710_LOG_CONFIG_ERROR("配置s_ram_rd0寄存器失败: ret=%d\n", ret);
                    return ret;
                }
                g_captureDataPending = 1;
                g_captureDataPendingNum = 0;
                break;

            case 'q':
            case 'Q':
                printf("Exiting program...\n");
                g_running = 0;
                break;
                
            default:
                printf("Unknown command: %c (enter 'h' for help)\n", input);
                break;
        }
    }
    
shutdown:
    ; /* A label must precede a statement before the declaration below. */
    if (g_fatal_error) {
        exit_code = 1;
    }

    TK8710LogConfig_t defaultLogConfig = {
        .level = TK8710_LOG_ALL,
        .module_mask = TK8710_LOG_MODULE_ALL,
        .callback = NULL,
        .enable_timestamp = 1,
        .enable_module_name = 1
    };
    TK8710LogInit(&defaultLogConfig);
    /* 打印最终的中断时间统计报告 */
    printf("\n=== 最终中断处理时间统计报告 ===\n");
    if (g_hal_initialized) {
        TK8710PrintIrqTimeStats();
    }

    /* 清理发送验证器 */
    // TRM_TxValidatorDeinit();

    /* 停止Web扫频IPC服务 */
    TK8710ScanIpcServerStop();

    /* 停止核间通信 */
    printf("停止核间通信...\n");
    if (ipc_started) {
        IpcCommStop(&g_ipc_ctx);
        IpcCommCleanup(&g_ipc_ctx);
    }
    printf("核间通信已停止\n");

    TK8710HalReset();
    
    printf("Program ended\n");
    return exit_code;
}

/*============================================================================
 * 编译说明
 *============================================================================
 * 
 * RK3506 编译命令 (在开发板上):
 *   gcc -I../inc -I../port test8710main.c ../port/tk8710_rk3506.c \
 *       ../src/tk8710_irq.c ../src/tk8710_core.c ../src/tk8710_config.c \
 *       ../src/tk8710_log.c -o test8710main -lgpiod -lpthread
 * 
 * RK3506 交叉编译命令 (在PC上):
 *   arm-buildroot-linux-gnueabihf-gcc -I../inc -I../port test8710main.c \
 *       ../port/tk8710_rk3506.c ../src/tk8710_irq.c ../src/tk8710_core.c \
 *       ../src/tk8710_config.c ../src/tk8710_log.c \
 *       -o test8710main -lgpiod -lpthread
 * 
 * 运行:
 *   ./test8710main      (RK3506 Linux)
 * 
 * 功能说明:
 * 1. 完整的TK8710初始化流程
 * 2. RK3506 SPI接口初始化 (/dev/spidev0.0)
 * 3. GPIO中断初始化 (libgpiod)
 * 4. 射频系统配置
 * 5. 8710芯片初始化
 * 6. 时隙参数配置 (TK8710_CFG_TYPE_SLOT_CFG)
 * 7. 启动主模式连续工作
 * 8. 实时中断处理和状态监控
 * 9. 交互式命令行界面
 * 
 * 注意事项:
 * 1. 需要TK8710硬件连接到RK3506的SPI接口
 * 2. 确保/dev/spidev0.0存在且有访问权限
 * 3. 确保libgpiod已安装
 * 4. 可能需要root权限运行
 * 5. 按Ctrl+C可安全退出程序
 */
