/** Standalone test runtime adapted from tk8710_gw.c; production source is unchanged. */
#define _GNU_SOURCE
#include "tk8710_gw_freq_runtime.h"
#include "tk8710_hal.h"
#include "hal_api.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"
#include "driver/tk8710_log.h"
#include "trm/trm_log.h"
#include "trm/trm_internal.h"
#include "trm/trm_pps_monitor.h"
#include "tk8710_ipc_comm.h"
#include "tk8710_gw_gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#define TK8710_CHDIR(path) chdir(path)
#define TK8710_GETCWD(buf, size) getcwd(buf, size)
#define TK8710_MKDIR(path) mkdir(path, 0755)
#define TK8710_TX_DC_DIR "TxDC"
#define TK8710_TX_DC_FILE TK8710_TX_DC_DIR "/txadc.txt"
#define TK8710_RF_I2C_ADDR_BASE 0x50u
#define TK8710_RF_DC_RAM_OFFSET 0x00u
#define TK8710_I2C_BUS_MAX 31
static volatile int g_running = 1;
static uint8_t g_rf_tx_gain = 0x2a;
static uint32_t g_reg_a064_value = 0x00044003u;
static GwGpsManager g_gps_manager;
static int g_hal_initialized;
static pthread_mutex_t g_freq_ipc_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_freq_ipc_started;
static int g_freq_ns_mismatch;
static NsConfigDown_t g_freq_config;
static uint32_t g_freq_first_s3, g_freq_last_s3;
static uint64_t g_freq_last_slot_ms;
static int g_freq_hw_touched;
static void OnTrmRxData(const TRM_RxDataList* data)
{
    pthread_mutex_lock(&g_freq_ipc_lock);
    if (g_freq_ipc_started && data) IpcSendUplinkData(&g_ipc_ctx, data);
    pthread_mutex_unlock(&g_freq_ipc_lock);
}
static void OnTrmTxComplete(const TRM_TxCompleteResult* result) { (void)result; }
static void ApplyRuntimeLogLevels(void)
{
    TK8710LogSetLevel(TK8710_LOG_WARN);
    TRM_LogConfig(TRM_LOG_WARN, 1);
}

static int FreqCheckNsConfig(const NsConfigDown_t* config)
{
    /* NS provides data but cannot replace test frequency/bcnbits. */
    if (config->gps_enable != 1 || config->tdd_num != g_freq_config.tdd_num ||
        config->rate_num != 1 || config->rate_cfgs[0].rate != g_freq_config.rate_cfgs[0].rate ||
        config->rate_cfgs[0].uplink_pkt != g_freq_config.rate_cfgs[0].uplink_pkt ||
        config->rate_cfgs[0].downlink_pkt != g_freq_config.rate_cfgs[0].downlink_pkt) {
        fprintf(stderr, "FREQ_TEST: NS GPS/TDD/rate/block configuration mismatch\n");
        pthread_mutex_lock(&g_freq_ipc_lock);
        g_freq_ns_mismatch = 1;
        pthread_mutex_unlock(&g_freq_ipc_lock);
        return -1;
    }
    return 0;
}

static void FreqStopIpc(void)
{
    pthread_mutex_lock(&g_freq_ipc_lock);
    int started = g_freq_ipc_started;
    g_freq_ipc_started = 0;
    pthread_mutex_unlock(&g_freq_ipc_lock);
    if (started) {
        IpcCommStop(&g_ipc_ctx);
        IpcCommCleanup(&g_ipc_ctx);
    }
}

static int ApplyNsConfig(const NsConfigDown_t* config);
static int ConfigureRuntimeDirectory(const char* path);
static int SaveTxDcFromRfRam(void);
int set_cpu_affinity(int cpu_core);
static uint32_t GetDriverWatchdogIrqCount(uint8_t irq_type);

int GwFreqOpen(const char* work_dir, uint8_t tx_gain)
{
    g_running = 1;
    g_rf_tx_gain = tx_gain;
    if (ConfigureRuntimeDirectory(work_dir) != 0 || set_cpu_affinity(2) < 0) return -1;
    GwGpsUseLocalWithoutRecovery(&g_gps_manager);
    /* Match the reference gateway: EEPROM availability is not a startup gate. */
    int dc_ret = SaveTxDcFromRfRam();
    struct stat info;
    if (stat(TK8710_TX_DC_FILE, &info) == 0 && info.st_size > 0) {
        printf("FREQ_TEST: TXDC calibration_file_available=yes refresh_status=%d; "
                "actual source depends on driver file validation\n", dc_ret);
    } else {
        fprintf(stderr, "FREQ_TEST: TXDC source=zero_txadc; "
                "no board calibration file available, refresh_status=%d\n", dc_ret);
    }
    return 0;
}

int GwFreqConfigure(const NsConfigDown_t* config)
{
    FreqStopIpc();
    if (g_hal_initialized) {
        /* RK3506 disable joins the IRQ thread, draining callbacks before TRM teardown. */
        if (TK8710GpioIrqEnable(0, 0) != 0 ||
            TK8710Reset(TK8710_RST_STATE_MACHINE) != TK8710_OK) return -1;
        /* HAL reset also releases requested GPIO lines; a stopped IRQ thread alone
         * does not release gpiod ownership and the next GpioInit would fail EBUSY. */
        if (TK8710HalReset() != TK8710_HAL_OK) return -1;
        g_hal_initialized = 0;
        g_freq_hw_touched = 0;
        TK8710ResetIrqCounters();
    }
    g_freq_config = *config;
    g_freq_ns_mismatch = 0;
    if (!g_running || ApplyNsConfig(config) != 0) return -1;
    g_freq_first_s3 = GetDriverWatchdogIrqCount(TK8710_IRQ_S3);
    g_freq_last_s3 = g_freq_first_s3;
    g_freq_last_slot_ms = TK8710GetTimeUs() / 1000;
    IpcCommClearConfigReceived();
    IpcCommSetConfigHandler(FreqCheckNsConfig);
    if (IpcCommInit(&g_ipc_ctx) != 0) {
        IpcCommCleanup(&g_ipc_ctx);
        return -1;
    }
    if (IpcCommStart(&g_ipc_ctx) != 0) {
        IpcCommCleanup(&g_ipc_ctx);
        return -1;
    }
    pthread_mutex_lock(&g_freq_ipc_lock);
    g_freq_ipc_started = 1;
    pthread_mutex_unlock(&g_freq_ipc_lock);
    return IpcCommSendConfigRequest(&g_ipc_ctx);
}

int GwFreqPoll(uint8_t check_gps)
{
    uint8_t mask = 0, count = 0, valid = 0;
    uint32_t slots = GetDriverWatchdogIrqCount(TK8710_IRQ_S3);
    uint64_t now_ms = TK8710GetTimeUs() / 1000;
    pthread_mutex_lock(&g_freq_ipc_lock);
    int mismatch = g_freq_ns_mismatch;
    pthread_mutex_unlock(&g_freq_ipc_lock);
    if (!g_running || mismatch) return -1;
    if (TrmPpsMonitorFailed()) return -2;
    if (TRM_IsShutdownRequested() || TrmPpsMonitorMissing()) return -1;
    if (check_gps) {
        if (GwGpsPoll(&g_gps_manager) != GW_GPS_ACTION_NONE ||
            g_gps_manager.consecutive_abnormal != 0 ||
            g_gps_manager.mode != GW_GPS_MODE_EXTERNAL_ACTIVE) return -1;
        if (TK8710GetAbnormalRfChannelStatus(&mask, &count) != TK8710_OK || count >= 3)
            return -1;
    }
    if (slots != g_freq_last_s3) {
        g_freq_last_s3 = slots;
        g_freq_last_slot_ms = now_ms;
    } else if (now_ms-g_freq_last_slot_ms > 60000) return -1;
    if (TRM_GetBroadcastStatus(NULL, &valid) != TRM_OK) return -1;
    return valid && IpcCommIsConfigReceived() && slots-g_freq_first_s3 >= 2;
}

void GwFreqCancel(void) { g_running = 0; }

int GwFreqClose(void)
{
    int ret = 0;
    FreqStopIpc();
    if (g_freq_hw_touched && TK8710GpioIrqEnable(0, 0) != 0) ret = -1;
    TrmPpsMonitorStop();
    GwGpsClose(&g_gps_manager);
    if (g_freq_hw_touched && TK8710HalReset() != TK8710_HAL_OK) ret = -1;
    g_hal_initialized = 0;
    g_freq_hw_touched = 0;
    return ret;
}

static int ReadRfTxDcWord(int fd, uint8_t slave_addr, uint32_t* value)
{
    uint8_t offset = TK8710_RF_DC_RAM_OFFSET;
    uint8_t data[sizeof(uint32_t) + 1U];
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

    /* Skip the first returned byte. Q and I are each little-endian uint16_t. */
    *value = (uint32_t)data[1] |
             ((uint32_t)data[2] << 8) |
             ((uint32_t)data[3] << 16) |
             ((uint32_t)data[4] << 24);
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

static int SaveTxDcFromRfRam(void)
{
#ifdef _WIN32
    printf("I2C TX DC loading is only supported on RK3506 Linux\n");
    return -1;
#else
    uint32_t tx_dc_data[TK8710_MAX_ANTENNAS];
    char device_path[32];
    char temporary_path[] = TK8710_TX_DC_FILE ".XXXXXX";
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

    /* Check raw words before sanitizing components or opening any output file.
     * Either unprogrammed marker selects the existing calibration for all RFs. */
    for (i = 0; i < TK8710_MAX_ANTENNAS; i++) {
        if (tx_dc_data[i] == 0U || tx_dc_data[i] == UINT32_MAX) {
            printf("RF%d TX DC is not stored (raw=0x%08X); keeping board calibration %s\n",
                   i, tx_dc_data[i], TK8710_TX_DC_FILE);
            return 0;
        }
    }

    if (TK8710_MKDIR(TK8710_TX_DC_DIR) != 0 && errno != EEXIST) {
        printf("Failed to create %s: %s\n", TK8710_TX_DC_DIR, strerror(errno));
        return -1;
    }

    /* Commit all eight channels together; preserve the old file on failure. */
    fd = mkstemp(temporary_path);
    if (fd < 0) {
        printf("Failed to create TX DC temporary file: %s\n", strerror(errno));
        return -1;
    }
    file = fdopen(fd, "w");
    if (file == NULL) {
        printf("Failed to open %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
        close(fd);
        unlink(temporary_path);
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
            unlink(temporary_path);
            return -1;
        }
        printf("RF%d TX DC: raw=0x%08X, I=0x%04X, Q=0x%04X\n",
               i, tx_dc_data[i], dc_i, dc_q);
    }

    int write_ok = fflush(file) == 0 && fchmod(fd, 0644) == 0 && fsync(fd) == 0;
    if (fclose(file) != 0) {
        write_ok = 0;
    }
    if (!write_ok) {
        printf("Failed to close %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
        unlink(temporary_path);
        return -1;
    }
    if (rename(temporary_path, TK8710_TX_DC_FILE) != 0) {
        printf("Failed to replace %s: %s\n", TK8710_TX_DC_FILE, strerror(errno));
        unlink(temporary_path);
        return -1;
    }

    printf("TX DC values read from %s and saved to %s\n",
           device_path, TK8710_TX_DC_FILE);
    return 0;
#endif
}

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

static int RandomizeBcnRotation(slotCfg_t* slot_config)
{
    unsigned int seed;
    unsigned int random_state;

    if (TK8710GetRandomBytes((uint8_t*)&seed, sizeof(seed)) != 0) {
        fprintf(stderr, "Failed to obtain BCN rotation random seed\n");
        return -1;
    }
    random_state = seed;
    for (unsigned int index = 0; index < TK8710_MAX_ANTENNAS; ++index) {
        slot_config->bcnRotation[index] = (uint8_t)index;
    }
    /* Local PRNG state avoids changing random sequences in other threads. */
    for (unsigned int count = TK8710_MAX_ANTENNAS; count > 1; --count) {
        unsigned int value;
        unsigned int range = (unsigned int)RAND_MAX + 1u;
        unsigned int limit = range - range % count;
        do {
            value = (unsigned int)rand_r(&random_state);
        } while (value >= limit);
        unsigned int selected = value % count;
        uint8_t antenna = slot_config->bcnRotation[count - 1];
        slot_config->bcnRotation[count - 1] = slot_config->bcnRotation[selected];
        slot_config->bcnRotation[selected] = antenna;
    }
    printf("BCN rotation seed=%u table[0..7]=", seed);
    for (unsigned int index = 0; index < TK8710_MAX_ANTENNAS; ++index) {
        printf("%s%u", index == 0 ? "" : ",", slot_config->bcnRotation[index]);
    }
    printf("\n");
    return 0;
}

static int ApplyNsConfig(const NsConfigDown_t* config) {
    uint8_t network_id;
    uint8_t use_external_sync = 0;
    int slot_calc_ok;
    uint32_t pps_period_s = 0;

    if (!config) {
        return -1;
    }
    if (config->gps_enable != 0 && config->gps_enable != 1) return -1;
    TrmPpsMonitorStop();
    GwGpsClose(&g_gps_manager);
    GwGpsUseLocalWithoutRecovery(&g_gps_manager);
    if (config->gps_enable) {
        GwGpsPolicy policy;
        TK8710PpsConfig pps;
        char version[TK8710_PPS_DIAG_TEXT_MAX];
        GwGpsGetDefaultPolicy(&policy);
        if (TK8710PpsGetDefaultConfig(&pps) != TK8710_PPS_OK ||
            GwGpsStartup(&g_gps_manager, &pps, &policy, &g_running, version, sizeof(version)) != 0 ||
            g_gps_manager.mode != GW_GPS_MODE_EXTERNAL_READY) {
            fprintf(stderr, "FATAL: NS gps_enable=1 but GPS is unavailable/unhealthy; refusing business startup\n");
            return -1;
        }
    } else {
        printf("NS gps_enable=0: GPS disabled, local synchronization required\n");
    }

    network_id = ConvertNsNetworkIdToBcnBits(config->nwk_num);

    printf("开始处理NS配置 (HAL已初始化=%d)...\n", g_hal_initialized);
    printf("NS配置详情: freq=%u, nwk_num=%d, tdd_num=%d, slot_cfg=%d, rate_num=%d, bcnbits=%u\n",
           config->freq, config->nwk_num, config->tdd_num, config->slot_cfg,
           config->rate_num, network_id);

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
    g_freq_hw_touched = 1;
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
    slotCfg.local_sync = TK8710_SYNC_MODE_LOCAL;
    slotCfg.plCrcEn = 0;
    slotCfg.brdUserNum = 1;
    slotCfg.antEn = 0xFF;
    slotCfg.rfSel = 0xFF;
    slotCfg.txBeamCtrlMode = 1;
    slotCfg.txBcnAntEn = 0xff;
    slotCfg.rx_delay = 0;
    slotCfg.md_agc = 1024;
    slotCfg.brdFreq[0] = 20000.0;
    slotCfg.frameTimeLen = 0;

    if (RandomizeBcnRotation(&slotCfg) != 0) {
        return -1;
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
    slot_calc_ok = trm_calc_multi_rate_slot_config(&multiSlotInput, &multiSlotOutput) == 0;
    if (slot_calc_ok) {
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

    if (slot_calc_ok && GwGpsPeriodFromSlotCalc(&multiSlotOutput, &pps_period_s) != 0) {
        fprintf(stderr,
                "Invalid PPS period from slot calculator: framePeriod=%u frameCount=%u\n",
                multiSlotOutput.framePeriod, multiSlotOutput.frameCount);
        slot_calc_ok = 0;
    }
    printf("GPS/PPS mode before NS synchronization setup: %s\n",
           GwGpsModeName(g_gps_manager.mode));
    if (config->gps_enable && slot_calc_ok && GwGpsCanConfigurePeriod(&g_gps_manager)) {
        printf("Configuring GPS/PPS period from slot calculator: %u s\n", pps_period_s);
        if (GwGpsConfigurePeriod(&g_gps_manager, pps_period_s) == 0) {
            use_external_sync = 1;
        } else {
            if (!g_running) {
                return -1;
            }
            fprintf(stderr, "FATAL: mandatory GPS/PPS alignment failed\n");
            return -1;
        }
    } else if (config->gps_enable) {
        fprintf(stderr, "FATAL: mandatory GPS/PPS period unavailable\n");
        return -1;
    }
    slotCfg.local_sync = use_external_sync ?
        TK8710_SYNC_MODE_EXTERNAL : TK8710_SYNC_MODE_LOCAL;
    printf("TK8710 synchronization source: %s\n",
           use_external_sync ? "external GPS/PPS" : "local");

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

    if (TRM_SetAcmPpsSchedule(use_external_sync ? multiSlotOutput.frameCount : 0,
            use_external_sync ? slotCfg.rateCount : 0) != TRM_OK) {
        fprintf(stderr, "Failed to configure ACM PPS schedule\n");
        return -1;
    }
    if (use_external_sync) {
        TrmPpsConfig pps_monitor = {
            .cycle_us = multiSlotOutput.framePeriod,
            .cycles = multiSlotOutput.frameCount,
            .super_frames = (uint32_t)config->tdd_num,
            .rates = slotCfg.rateCount
        };
        for (uint8_t i = 0; i < pps_monitor.rates; ++i) {
            const TRM_RateSlotConfig* rate = &multiSlotOutput.rateConfigs[i];
            pps_monitor.s0_us[i] = rate->bcnSlotLen;
            pps_monitor.frame_us[i] = rate->bcnSlotLen + rate->brdSlotLen +
                                      rate->ulSlotLen + rate->dlSlotLen;
        }
        if (TrmPpsMonitorStart(&pps_monitor) != 0) {
            fprintf(stderr, "FATAL: cannot monitor AG32 PPS on gpiochip1/18\n");
            return -1;
        }
    } else {
        TrmPpsMonitorStop();
    }
    /* 12. 调用 TK8710HalStart 启动工作 */
    TK8710HalError halRet_start = TK8710HalStart();
    if (halRet_start != TK8710_HAL_OK) {
        printf("HAL start failed: %d\n", halRet_start);
        return -1;
    }
    printf("HAL started successfully (Master mode, Continuous work)\n");

    g_hal_initialized = 1;

    return 0;
}
