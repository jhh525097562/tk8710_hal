/**
 * @file tk8710_wakeup.c
 * @brief 唤醒配置、S0边界定时停止和完成通知。频率由外部Init配置。
 */
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"
#include "tk8710_hal.h"

#include <stddef.h>

#define WAKEUP_S0_PER_ANTENNA 16u
#define WAKEUP_BCN_SAMPLE_HZ 1000000u
#define WAKEUP_BCN_PHASE_SCALE (1u << 25)

/* Physical antennas 1~8, represented by array indices 0~7. */
static const int32_t g_bcn_offset_hz[2][TK8710_MAX_ANTENNAS] = {
    // {-26000, 0, -9000, 0, 8000, 0, 25000, 0}, /* Modes 1/2 */
    // {-52000, 0, -18000, 0, 17000, 0, 50000, 0} /* Mode 3 */
    {0, 0, 0, 0, 0, 0, 0, 0}, /* Modes 1/2 */
    {0, 0, 0, 0, 0, 0, 0, 0} /* Mode 3 */
};

static uint32_t _wakeup_bcn_phase_word(int32_t offset_hz)
{
    /* Normalize negative offsets first, then truncate the unsigned phase word.
     * Use 64-bit integer arithmetic: (Fs + f) / Fs * 2^25 for f < 0.
     */
    uint32_t wrapped_hz = (uint32_t)(offset_hz < 0 ?
                                   (int32_t)WAKEUP_BCN_SAMPLE_HZ + offset_hz : offset_hz);
    return (uint32_t)((uint64_t)wrapped_hz * WAKEUP_BCN_PHASE_SCALE / WAKEUP_BCN_SAMPLE_HZ);
}

static TK8710WakeupStatus g_status = { .state = TK8710_WAKEUP_IDLE };
static TK8710WakeupCallback g_callback;
static void* g_callback_user;
static uint64_t g_started_us;
static uint8_t g_ready;
static uint8_t g_owns_irq;
static uint8_t g_completion_pending;
static uint8_t g_antenna_mask;
static uint8_t g_current_antenna;
static uint32_t g_antenna_config[TK8710_MAX_ANTENNAS];
static uint8_t g_antennas_prepared;
static uint32_t g_s0_count;
static uint64_t g_last_s0_us;
static uint32_t g_last_antenna_values[TK8710_MAX_ANTENNAS];

/* Sample first 16 S0s, every antenna switch and every 100 S0s.
 * Readback/printing happens after c008=0x80, outside the 1ms toggle window.
 */
static uint8_t _wakeup_log_sample(void)
{
    return TK8710_LOG_ENABLE(TK8710_LOG_MODULE_CORE, TK8710_LOG_INFO) &&
           (g_s0_count <= 16u || g_s0_count % WAKEUP_S0_PER_ANTENNA == 0 ||
            g_s0_count % 100u == 0);
}

static void _wakeup_trace_antennas(const char* phase, uint8_t requested)
{
    uint32_t active = 0;
    uint32_t mismatches = 0;
    uint32_t read_errors = 0;
    uint8_t ant;
    for (ant = 0; ant < TK8710_MAX_ANTENNAS; ant++) {
        uint16_t addr = TX_FE_BASE + 0x78u + 0x1000u * ant;
        s_tx_config_28 value;
        int ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, addr, &value.data);
        if (ret != TK8710_OK) {
            read_errors |= 1u << ant;
            TK8710_LOG_CORE_WARN("[WAKEUP] READ_FAIL phase=%s s0=%u ant=%u addr=0x%04X ret=%d",
                                phase, g_s0_count, ant, addr, ret);
            continue;
        }
        if (value.b.sc_bcn != 0) active |= 1u << ant;
        if (value.data != g_last_antenna_values[ant]) mismatches |= 1u << ant;
        TK8710_LOG_CORE_DEBUG("[WAKEUP] REG phase=%s s0=%u ant=%u addr=0x%04X "
                             "saved=0x%08X write=0x%08X read=0x%08X bcn=0x%02X da=0x%02X",
                             phase, g_s0_count, ant, addr, g_antenna_config[ant],
                             g_last_antenna_values[ant], value.data, value.b.sc_bcn, value.b.sc_da);
    }
    TK8710_LOG_CORE_INFO("[WAKEUP] ANT phase=%s s0=%u requested=%u sw_current=%u "
                        "eligible=0x%02X active=0x%02X mismatch=0x%02X read_errors=0x%02X",
                        phase, g_s0_count, requested, g_current_antenna,
                        g_antenna_mask, active, mismatches, read_errors);
    if (read_errors == 0 && requested < TK8710_MAX_ANTENNAS && active != (1u << requested)) {
        TK8710_LOG_CORE_WARN("[WAKEUP] ACTIVE_MISMATCH requested=%u expected=0x%02X actual=0x%02X",
                            requested, 1u << requested, active);
    }
    if (requested < TK8710_MAX_ANTENNAS && g_current_antenna != requested) {
        TK8710_LOG_CORE_WARN("[WAKEUP] TRACKING_MISMATCH selected=%u sw_current=%u; next uses sw_current",
                            requested, g_current_antenna);
    }
}

/* All helpers below run under the Port critical section. SPI does not take it. */
static uint8_t _wakeup_busy(void)
{
    return g_status.state == TK8710_WAKEUP_SENDING ||
           g_status.state == TK8710_WAKEUP_WAIT_BCN;
}

static int _wakeup_write(uint16_t address, uint32_t value)
{
    int ret = TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, address, value);
    if (ret != TK8710_OK) {
        TK8710_LOG_CORE_ERROR("[WAKEUP] WRITE_FAIL s0=%u addr=0x%04X value=0x%08X ret=%d",
                             g_s0_count, address, value, ret);
    }
    return ret;
}

/* Disable all paths before enabling the next one, including on 7 -> 0 wrap.
 * Commit the software antenna only after its enable write succeeds.
 */
static int _wakeup_select_antenna(uint8_t antenna)
{
    s_tx_config_28 config;
    uint8_t index;
    int ret;

    if (antenna >= TK8710_MAX_ANTENNAS) {
        TK8710_LOG_CORE_ERROR("[WAKEUP] INVALID_ANT requested=%u mask=0x%02X prepared=%u",
                             antenna, g_antenna_mask, g_antennas_prepared);
        return TK8710_ERR_PARAM;
    }

    for (index = 0; index < TK8710_MAX_ANTENNAS; index++) {
        config.data = g_antenna_config[index];
        config.b.sc_da = 0x40;
        config.b.sc_bcn = 0;
        g_last_antenna_values[index] = config.data;
        ret = _wakeup_write(TX_FE_BASE + 0x78u + 0x1000u * index, config.data);
        if (ret != TK8710_OK) return ret;
    }
    config.data = g_antenna_config[antenna];
    config.b.sc_da = 0x40;
    config.b.sc_bcn = 0x60;
    g_last_antenna_values[antenna] = config.data;
    ret = _wakeup_write(TX_FE_BASE + 0x78u + 0x1000u * antenna, config.data);
    if (ret == TK8710_OK) g_current_antenna = antenna;
    return ret;
}

static uint8_t _wakeup_next_antenna(uint8_t current)
{
    if (g_antenna_mask == 0) {
        TK8710_LOG_CORE_ERROR("[WAKEUP] EMPTY_MASK prepared=%u current=%u s0=%u; skip endless search",
                             g_antennas_prepared, current, g_s0_count);
        return 0xff;
    }
    do {
        current = (uint8_t)((current + 1u) % TK8710_MAX_ANTENNAS);
    } while ((g_antenna_mask & (1u << current)) == 0);
    return current;
}

static int _wakeup_prepare_antennas(void)
{
    s_init_9 init9;
    uint8_t index;
    int ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                           MAC_BASE + offsetof(struct mac, init_9), &init9.data);
    if (ret != TK8710_OK) return ret;
    g_antenna_mask = init9.b.ant_en & init9.b.rf_sel;
    TK8710_LOG_CORE_INFO("[WAKEUP] PREP init9=0x%08X ant_en=0x%02X rf_sel=0x%02X "
                        "bcn_en=0x%02X eligible=0x%02X",
                        init9.data, init9.b.ant_en, init9.b.rf_sel,
                        init9.b.tx_bcn_ant_en, g_antenna_mask);
    if (g_antenna_mask == 0) return TK8710_ERR_PARAM;
    for (index = 0; index < TK8710_MAX_ANTENNAS; index++) {
        ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                           TX_FE_BASE + 0x78u + 0x1000u * index, &g_antenna_config[index]);
        if (ret != TK8710_OK) return ret;
    }
    /* Init may select only BCN antenna0. Enable eligible paths; the per-path
     * sc_bcn gates above ensure that only one transmits at a time.
     */
    init9.b.tx_bcn_ant_en = g_antenna_mask;
    ret = _wakeup_write(MAC_BASE + offsetof(struct mac, init_9), init9.data);
    if (ret != TK8710_OK) return ret;
    ret = _wakeup_select_antenna(_wakeup_next_antenna(TK8710_MAX_ANTENNAS - 1u));
    if (ret == TK8710_OK) g_antennas_prepared = 1;
    if (ret == TK8710_OK && _wakeup_log_sample()) {
        _wakeup_trace_antennas("prepared", g_current_antenna);
    }
    return ret;
}

static int _wakeup_elapsed(void)
{
    uint64_t now = TK8710GetMonotonicTimeUs();
    if (_wakeup_busy() && (now == 0 || now < g_started_us)) return TK8710_ERR;
    if (_wakeup_busy() && now >= g_started_us) {
        g_status.elapsed_us = now - g_started_us;
        if (g_status.elapsed_us >= (uint64_t)g_status.duration_ms * 1000u) {
            g_status.state = TK8710_WAKEUP_WAIT_BCN;
        }
    }
    return TK8710_OK;
}

static int _wakeup_stop(TK8710WakeupState state, int reason)
{
    int reset_ret;
    int mask_ret;
    int clear_ret;

    _wakeup_elapsed();
    reset_ret = TK8710SpiReset(TK8710_RST_STATE_MACHINE);
    mask_ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl0), 0xffffu);
    clear_ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl1), 0x1ffu);
    g_status.stopped = reset_ret == 0 && mask_ret == 0 && clear_ret == 0;
    g_status.result = reason != 0 ? reason :
                      (reset_ret != 0 ? reset_ret : (mask_ret != 0 ? mask_ret : clear_ret));
    g_status.state = g_status.result == 0 ? state : TK8710_WAKEUP_ERROR;
    TK8710_LOG_CORE_INFO("[WAKEUP] STOP state=%u reason=%d result=%d stopped=%u "
                        "reset=%d mask=%d clear=%d s0=%u current=%u elapsed_us=%llu",
                        g_status.state, reason, g_status.result, g_status.stopped,
                        reset_ret, mask_ret, clear_ret, g_s0_count, g_current_antenna,
                        (unsigned long long)g_status.elapsed_us);
    /* Even after completion, suppress stale normal IRQs until external Init. */
    g_completion_pending = 1;
    return g_status.result;
}

uint8_t TK8710WakeupBusy(void)
{
    uint8_t busy;
    TK8710EnterCritical();
    busy = _wakeup_busy() || g_completion_pending ||
           (g_status.state == TK8710_WAKEUP_ERROR && !g_status.stopped);
    TK8710ExitCritical();
    return busy;
}

uint8_t TK8710WakeupOwnsIrq(void)
{
    uint8_t owns;
    TK8710EnterCritical();
    owns = g_owns_irq;
    TK8710ExitCritical();
    return owns;
}

int TK8710WakeupPrepareInit(void)
{
    TK8710EnterCritical();
    if (_wakeup_busy() || g_completion_pending) {
        TK8710ExitCritical();
        return TK8710_ERR;
    }
    g_ready = 0;
    g_status.state = TK8710_WAKEUP_IDLE;
    /* Gate IRQs until initialization successfully completes. */
    g_owns_irq = 1;
    TK8710ExitCritical();
    return TK8710_OK;
}

void TK8710WakeupInitComplete(void)
{
    TK8710EnterCritical();
    g_ready = 1;
    g_owns_irq = 0;
    g_status = (TK8710WakeupStatus){ .state = TK8710_WAKEUP_IDLE, .stopped = 1 };
    TK8710ExitCritical();
}

void TK8710WakeupResetComplete(void)
{
    TK8710EnterCritical();
    g_ready = 0;
    TK8710ExitCritical();
}

int TK8710RegisterWakeupCallback(TK8710WakeupCallback callback, void* user_data)
{
    TK8710EnterCritical();
    if (_wakeup_busy() || g_completion_pending) {
        TK8710ExitCritical();
        return TK8710_ERR;
    }
    g_callback = callback;
    g_callback_user = user_data;
    TK8710ExitCritical();
    return TK8710_OK;
}

void TK8710WakeupDispatchCompletion(void)
{
    TK8710WakeupCallback callback;
    TK8710WakeupStatus status;
    void* user;

    TK8710EnterCritical();
    if (!g_completion_pending) {
        TK8710ExitCritical();
        return;
    }
    status = g_status;
    callback = g_callback;
    user = g_callback_user;
    g_completion_pending = 0;
    TK8710ExitCritical();
    if (callback != NULL) {
        callback(&status, user);
    }
}

int TK8710SendWakeup(wakeUpMode_e mode, uint32_t duration_ms, uint8_t wakeup_id)
{
    uint32_t group = wakeup_id / 32u;
    s_irq_ctrl0 irq_mask;
    uint64_t now;
    int ret;
    size_t i;
    uint8_t antenna;
    const int32_t* offset_hz = g_bcn_offset_hz[mode == TK8710_WAKEUP_MODE_3 ? 1 : 0];
    uint32_t bcn_ampc[8] = {0x00060040, 0x0, 0x00060040, 0x0, 0x00060040, 0x0, 0x00060040, 0x0};
    // uint32_t bcn_ampc[8] = {0x00060040, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    const struct {
        uint16_t address;
        uint32_t value;
    } antenna_config[] = {
        {0x081c, 0}, {0x0820, 0},{0x0878, 0x00060040},
        {0x0814, 0x00100000u | ((uint32_t)(wakeup_id % 32u) << 4)},
        {0x0818, 0x0006008c}, {0x082c, 0x20},//{0x0818, 0x0006008c}
        {0x0824, (mode == TK8710_WAKEUP_MODE_3 ? 0x004d0000u : 0x008d0000u)
                 + group * 0x00100000u},
        {0x0830, 0} /* Per-antenna BCN frequency offset, filled below. */
    };
    const struct {
        uint16_t address;
        uint32_t value;
    } mac_config[] = {
        {0xc008, 1},
        {0xc000, (uint32_t)(mode + 6u) << 20},
        {0xc054, group * (mode == TK8710_WAKEUP_MODE_1 ? 0x800u : 0x400u)},
        {0xc01c, 0x10000000}
    };

    if (mode < TK8710_WAKEUP_MODE_1 || mode > TK8710_WAKEUP_MODE_3 ||
        duration_ms == 0 || duration_ms > 0xffffffu) {
        return TK8710_ERR_PARAM;
    }
    TK8710EnterCritical();
    if (!g_ready || _wakeup_busy() || g_completion_pending ||
        (g_status.state == TK8710_WAKEUP_ERROR && !g_status.stopped)) {
        TK8710ExitCritical();
        return TK8710_ERR;
    }
    g_owns_irq = 1;
    g_s0_count = 0;
    g_last_s0_us = 0;
    g_antennas_prepared = 0;
    TK8710_LOG_CORE_INFO("[WAKEUP] START mode=%u ms=%u id=%u group=%u bcnbits=%u "
                        "retained_mask=0x%02X retained_current=%u",
                        mode, duration_ms, wakeup_id, group, wakeup_id % 32u,
                        g_antenna_mask, g_current_antenna);
    g_status = (TK8710WakeupStatus){
        .state = TK8710_WAKEUP_SENDING, .mode = mode,
        .wakeup_id = wakeup_id, .duration_ms = duration_ms
    };
    ret = TK8710SpiReset(TK8710_RST_STATE_MACHINE);
    if (ret == 0) ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl0), 0xffffu);
    if (ret == 0) ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl1), 0x1ffu);
    /* TX FE registers are per antenna: 0x08xx, 0x18xx, ... 0x78xx.
     * Program all eight paths, even when Init enables only a subset.
     */
    for (antenna = 0; ret == 0 && antenna < TK8710_MAX_ANTENNAS; antenna++) {
        for (i = 0; ret == 0 && i < sizeof(antenna_config) / sizeof(antenna_config[0]); i++) {
            uint16_t address = antenna_config[i].address + 0x1000u * antenna;
            uint32_t value = antenna_config[i].value;
            if (antenna_config[i].address == 0x0878) {
                value = bcn_ampc[antenna];
            } else if (antenna_config[i].address == 0x0830) {
                value = _wakeup_bcn_phase_word(offset_hz[antenna]);
            }
            ret = _wakeup_write(address, value);
            TK8710_LOG_CORE_DEBUG("[WAKEUP] CFG ant=%u addr=0x%04X value=0x%08X ret=%d",
                                 antenna, address, value, ret);
            if (antenna_config[i].address == 0x0830) {
                TK8710_LOG_CORE_INFO("[WAKEUP] BCN_FREQ ant=%u offset_hz=%ld "
                                    "addr=0x%04X value=0x%08X ret=%d",
                                    antenna, (long)offset_hz[antenna], address, value, ret);
            }
        }
    }
    for (i = 0; ret == 0 && i < sizeof(mac_config) / sizeof(mac_config[0]); i++) {
        ret = _wakeup_write(mac_config[i].address, mac_config[i].value);
    }
    // if (ret == 0) ret = _wakeup_prepare_antennas();
    irq_mask.data = 0xffffu;
    irq_mask.b.s0_irq_mask = 0;
    if (ret == 0) ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl0), irq_mask.data);
    now = TK8710GetMonotonicTimeUs();
    if (ret == 0 && now == 0) ret = TK8710_ERR;
    g_started_us = now;
    if (ret == 0) ret = _wakeup_write(0xc074, 1);
    if (ret == 0) ret = _wakeup_write(0xc02c, 0x0ffffff);//多天线发送bcn
    if (ret == TK8710_OK) {
        uint32_t init9;
        int read_ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                                   MAC_BASE + offsetof(struct mac, init_9), &init9);
        TK8710_LOG_CORE_INFO("[WAKEUP] STARTED prepared=%u mask=0x%02X current=%u "
                            "init9_read_ret=%d init9=0x%08X",
                            g_antennas_prepared, g_antenna_mask, g_current_antenna,
                            read_ret, read_ret == TK8710_OK ? init9 : 0);
        if (!g_antennas_prepared) {
            TK8710_LOG_CORE_WARN("[WAKEUP] PREP_SKIPPED: antenna mask/saved gains may be uninitialized; "
                                "check _wakeup_prepare_antennas call");
        }
    }
    if (ret != 0) {
        _wakeup_stop(TK8710_WAKEUP_ERROR, ret);
        g_completion_pending = 0; /* Synchronous launch failure, no completion event. */
    }
    TK8710ExitCritical();
    return ret;
}

int TK8710GetWakeupStatus(TK8710WakeupStatus* status)
{
    if (status == NULL) return TK8710_ERR_PARAM;
    TK8710EnterCritical();
    if (_wakeup_elapsed() != TK8710_OK) {
        TK8710ExitCritical();
        return TK8710_ERR;
    }
    *status = g_status;
    TK8710ExitCritical();
    return TK8710_OK;
}

int TK8710CancelWakeup(void)
{
    int ret = TK8710_OK;
    TK8710EnterCritical();
    if (_wakeup_busy()) {
        ret = _wakeup_stop(TK8710_WAKEUP_CANCELLED, 0);
    } else if (g_status.state == TK8710_WAKEUP_ERROR && !g_status.stopped) {
        ret = g_status.result;
    }
    TK8710ExitCritical();
    TK8710WakeupDispatchCompletion();
    return ret;
}

uint8_t TK8710WakeupProcessIrq(void)
{
    uint32_t irq_status;
    int ret;

    TK8710EnterCritical();
    if (!g_owns_irq) {
        TK8710ExitCritical();
        return 0;
    }
    ret = TK8710ReadReg(TK8710_REG_TYPE_GLOBAL,
                      MAC_BASE + offsetof(struct mac, irq_res), &irq_status);
    if (ret == 0) ret = _wakeup_write(MAC_BASE + offsetof(struct mac, irq_ctrl1), irq_status & 0x1ffu);
    if (ret != 0 && _wakeup_busy()) {
        _wakeup_stop(TK8710_WAKEUP_ERROR, ret);
    } else if (ret == 0 && _wakeup_busy() && (irq_status & (1u << TK8710_IRQ_S0))) {
        uint64_t entered_us = TK8710GetMonotonicTimeUs();
        uint64_t period_us = g_last_s0_us == 0 ? 0 : entered_us - g_last_s0_us;
        uint8_t previous = g_current_antenna;
        uint8_t next = previous;
        uint8_t switch_due;
        g_last_s0_us = entered_us;
        g_s0_count++;
        switch_due = g_s0_count % WAKEUP_S0_PER_ANTENNA == 0;
        if (_wakeup_log_sample()) {
            TK8710_LOG_CORE_INFO("[WAKEUP] S0 count=%u irq=0x%08X period_us=%llu "
                                "elapsed_us=%llu target_ms=%u current=%u mask=0x%02X",
                                g_s0_count, irq_status, (unsigned long long)period_us,
                                (unsigned long long)(entered_us >= g_started_us ? entered_us - g_started_us : 0),
                                g_status.duration_ms, previous, g_antenna_mask);
        }
        if (_wakeup_elapsed() != TK8710_OK) {
            _wakeup_stop(TK8710_WAKEUP_ERROR, TK8710_ERR);
        } else if (g_status.state == TK8710_WAKEUP_WAIT_BCN) {
            _wakeup_stop(TK8710_WAKEUP_DONE, 0);
        } else {
            ret = _wakeup_write(0xc008, 1);
            // if (ret == TK8710_OK && switch_due) {
            //     next = _wakeup_next_antenna(g_current_antenna);
            //     ret = _wakeup_select_antenna(next);
            // }
            if (ret == 0) {
                TK8710DelayMs(1);
                ret = _wakeup_write(0xc008, 0x80);
            }
            if (ret != 0) _wakeup_stop(TK8710_WAKEUP_ERROR, ret);
            if (_wakeup_log_sample()) {
                TK8710_LOG_CORE_INFO("[WAKEUP] %s s0=%u prev=%u requested=%u "
                                    "sw_current=%u ret=%d handler_us=%llu dwell_s0=%u/16",
                                    switch_due ? "SWITCH" : "HOLD",
                                    g_s0_count, previous, next, g_current_antenna, ret,
                                    (unsigned long long)(TK8710GetMonotonicTimeUs() - entered_us),
                                    g_s0_count % WAKEUP_S0_PER_ANTENNA);
                // _wakeup_trace_antennas("after-toggle", next);
            }
        }
    }
    TK8710ExitCritical();
    return 1;
}
