/**
 * @file rate_mode_capacity_test.c
 * @brief Standalone TMS570/TK8710 loopback capacity test firmware.
 *
 * This file intentionally does not depend on the satellite payload application,
 * FPGA telecommand/telemetry protocol, HAL, or TRM public interfaces.
 */

#include "sys_common.h"

#if defined(PLATFORM_TMS570)
#include <stdio.h>
#include <string.h>

#include "sci.h"
#include "sys_core.h"
#include "system.h"
#include "tk8710_tms570.h"
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"
#include "capacity_test_logic.h"
#include "capacity_sim_vectors.h"

#ifndef CAPACITY_TEST_FRAMES
#define CAPACITY_TEST_FRAMES 10U
#endif

#ifndef CAPACITY_TEST_TIMEOUT_MS
#define CAPACITY_TEST_TIMEOUT_MS 5000U
#endif

#ifndef CAPACITY_TEST_PROTOCOL_RATE
#define CAPACITY_TEST_PROTOCOL_RATE 0xFFU
#endif

#ifndef CAPACITY_TEST_USER_COUNT
#define CAPACITY_TEST_USER_COUNT 0U
#endif

#define CAPACITY_TEST_PAYLOAD_LEN 26U
#define CAPACITY_TEST_CENTER_FREQ_HZ 509100000UL
#define CAPACITY_TEST_VERSION_REG (MAC_BASE + 0x0110U)
#define CAPACITY_TEST_TX_POWER 32U
#define CAPACITY_TEST_RESULT_COUNT 6U

typedef struct {
    uint8_t protocol_rate;
    uint8_t user_count;
} CapacityTestCase;

typedef struct {
    CapacityTestStats stats;
    volatile uint32_t tx_frames;
    volatile uint32_t rx_frames;
    volatile uint8_t running;
    uint8_t next_tx_sequence;
} CapacityTestRuntime;

static const CapacityTestCase g_cases[CAPACITY_TEST_RESULT_COUNT] = {
    {0U, 16U}, {0U, 128U}, {1U, 16U},
    {1U, 128U}, {2U, 16U}, {2U, 128U}
};

static CapacityTestRuntime g_runtime;
static uint8_t g_payload[CAPACITY_TEST_MAX_USERS][CAPACITY_TEST_PAYLOAD_LEN];
static uint8_t g_case_passed[CAPACITY_TEST_RESULT_COUNT];
static uint8_t g_payloadDiagnosticPrinted;

volatile uint32_t g_capacityTestExitCode = 0xFFFFFFFFU;
volatile uint32_t g_capacityTestCompletedCases = 0U;
/* Consumed by the shared HALCoGen startup object. */
volatile uint32 g_tk8710ResetCause = 0U;

static void CapacityLog(const char* text)
{
    while ((text != NULL) && (*text != '\0')) {
        while (sciIsTxReady(scilinREG) == 0U) {
        }
        sciSendByte(scilinREG, (uint8)*text++);
    }
}

static void CapacityLogU32(uint32_t value)
{
    char text[16];
    (void)snprintf(text, sizeof(text), "%lu", (unsigned long)value);
    CapacityLog(text);
}

static void CapacityBuildPayload(uint8_t user, uint8_t sequence, uint8_t driver_rate)
{
    uint32_t pos;

    g_payload[user][0] = user;
    g_payload[user][1] = sequence;
    g_payload[user][2] = driver_rate;
    g_payload[user][3] = (uint8_t)(user ^ sequence ^ driver_rate);
    for (pos = 4U; pos < CAPACITY_TEST_PAYLOAD_LEN; pos++) {
        g_payload[user][pos] = (uint8_t)(0x5AU ^ user ^ sequence ^ (uint8_t)pos);
    }
}

static uint8_t CapacityPayloadValid(uint8_t user, const uint8_t* data,
                                    uint16_t length, uint8_t driver_rate)
{
    uint32_t pos;
    uint8_t sequence;

    if ((data == NULL) || (length != CAPACITY_TEST_PAYLOAD_LEN) ||
        (data[0] != user) || (data[2] != driver_rate)) {
        return 0U;
    }
    sequence = data[1];
    if (data[3] != (uint8_t)(user ^ sequence ^ driver_rate)) {
        return 0U;
    }
    for (pos = 4U; pos < CAPACITY_TEST_PAYLOAD_LEN; pos++) {
        if (data[pos] != (uint8_t)(0x5AU ^ user ^ sequence ^ (uint8_t)pos)) {
            return 0U;
        }
    }
    return 1U;
}

static int CapacitySubmitUsers(uint8_t sequence)
{
    uint32_t user;
    uint32_t submitted = 0U;
    uint32_t user_bits[4] = {0U, 0U, 0U, 0U};

    for (user = 0U; user < g_runtime.stats.expected_users; user++) {
        s_tx_pow_ctrl power;

        CapacityBuildPayload((uint8_t)user, sequence, g_runtime.stats.driver_rate);
        if (TK8710TestRxInjectionSetUser((uint8_t)user, g_payload[user],
                                         CAPACITY_TEST_PAYLOAD_LEN) != TK8710_OK) {
            g_runtime.stats.driver_errors++;
            g_runtime.stats.tx_failed++;
            continue;
        }
        power.data = 0U;
        power.b.UserIndex = (uint8_t)user;
        power.b.power = g_capacityTxPower[user];
        if ((TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                            MAC_BASE + offsetof(struct mac, tx_pow_ctrl),
                            power.data) != TK8710_OK) ||
            (TK8710WriteBuffer((uint8_t)user, g_payload[user],
                               CAPACITY_TEST_PAYLOAD_LEN) != TK8710_OK)) {
            g_runtime.stats.tx_failed++;
        } else {
            user_bits[user / 32U] |= (uint32_t)1U << (user % 32U);
            submitted++;
        }
    }
    for (user = 0U; user < 4U; user++) {
        if (TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                           MAC_BASE + 0x3CU + user * 4U,
                           user_bits[user]) != TK8710_OK) {
            g_runtime.stats.tx_failed++;
        }
    }
    CapacityTest_RecordTxSubmitted(&g_runtime.stats, submitted);
    return (submitted == g_runtime.stats.expected_users) ? 0 : -1;
}

static int CapacityLoadSimulationVectors(void)
{
    if ((TK8710SpiSetInfo(TK8710_SET_INFO_ANOISE, g_capacityAnoise,
                          sizeof(g_capacityAnoise)) != TK8710_OK) ||
        (TK8710SpiSetInfo(TK8710_SET_INFO_AH, g_capacityAh,
                          sizeof(g_capacityAh)) != TK8710_OK) ||
        (TK8710SpiSetInfo(TK8710_SET_INFO_PILOT_POW, g_capacityPilotPower,
                          sizeof(g_capacityPilotPower)) != TK8710_OK) ||
        (TK8710SpiSetInfo(TK8710_SET_INFO_TX_FREQ, g_capacityTxFreq,
                          sizeof(g_capacityTxFreq)) != TK8710_OK)) {
        return -1;
    }
    return 0;
}

static void CapacityOnRxData(TK8710IrqResult* irq_result)
{
    uint32_t user;

    if ((g_runtime.running == 0U) || (irq_result == NULL) ||
        (irq_result->irq_type != TK8710_IRQ_MD_DATA)) {
        return;
    }

    CapacityTest_BeginRxFrame(&g_runtime.stats);
    for (user = 0U; user < TK8710_MAX_DATA_USERS; user++) {
        TK8710CrcResult* crc = &irq_result->crcResults[user];
        uint8_t* data = NULL;
        uint16_t length = 0U;
        uint8_t payload_valid = 0U;

        if ((crc->crcValid == 0U) && (crc->dataValid == 0U)) {
            continue;
        }
        if ((crc->crcValid != 0U) &&
            (TK8710GetRxUserData(crc->userIndex, &data, &length) == TK8710_OK)) {
            payload_valid = CapacityPayloadValid(crc->userIndex, data, length,
                                                 g_runtime.stats.driver_rate);
            if ((payload_valid == 0U) && (g_payloadDiagnosticPrinted == 0U)) {
                uint32_t byte;
                char hex[8];
                CapacityLog("RX_SAMPLE user="); CapacityLogU32(crc->userIndex);
                CapacityLog(" len="); CapacityLogU32(length); CapacityLog(" data=");
                for (byte = 0U; (byte < length) && (byte < 12U); byte++) {
                    (void)snprintf(hex, sizeof(hex), "%02X", data[byte]);
                    CapacityLog(hex);
                }
                CapacityLog(" expected=");
                for (byte = 0U; byte < 12U; byte++) {
                    (void)snprintf(hex, sizeof(hex), "%02X",
                                   g_payload[crc->userIndex][byte]);
                    CapacityLog(hex);
                }
                CapacityLog("\r\n");
                g_payloadDiagnosticPrinted = 1U;
            }
        }
        CapacityTest_RecordRxUser(&g_runtime.stats, crc->userIndex,
                                  crc->crcValid, payload_valid);
        if (data != NULL) {
            (void)TK8710ReleaseRxData(crc->userIndex);
        }
    }
    if (irq_result->crcErrorCount > g_runtime.stats.rx_crc_errors) {
        g_runtime.stats.rx_crc_errors +=
            (uint32_t)irq_result->crcErrorCount - g_runtime.stats.rx_crc_errors;
    }
    CapacityTest_EndRxFrame(&g_runtime.stats);
    g_runtime.rx_frames++;
}

static void CapacityOnTxSlot(TK8710IrqResult* irq_result)
{
    if ((g_runtime.running == 0U) || (irq_result == NULL)) {
        return;
    }
    /* In Driver loopback specified-info mode, one S1 completion means the
     * preloaded and user_val-selected set was consumed for this frame. */
    CapacityTest_RecordTxComplete(&g_runtime.stats,
                                  g_runtime.stats.expected_users);
    g_runtime.tx_frames++;

    if (g_runtime.tx_frames < g_runtime.stats.expected_frames) {
        g_runtime.next_tx_sequence++;
        (void)CapacitySubmitUsers(g_runtime.next_tx_sequence);
    }
}

static void CapacityOnSlotEnd(TK8710IrqResult* irq_result)
{
    (void)irq_result;
}

static void CapacityOnError(TK8710IrqResult* irq_result)
{
    (void)irq_result;
    CapacityTest_RecordDriverError(&g_runtime.stats);
}

static uint32_t CapacityRateGap(uint8_t driver_rate)
{
    switch (driver_rate) {
        case 6U: return 65536U;
        case 7U: return 32768U;
        case 8U: return 8000U;
        default: return 0U;
    }
}

static int CapacityConfigure(uint8_t protocol_rate, uint8_t users)
{
    ChipConfig chip;
    slotCfg_t slot;
    TK8710DriverCallbacks callbacks;
    uint32_t antenna;
    uint32_t gap;

    CapacityTest_Init(&g_runtime.stats, protocol_rate, users,
                      (uint32_t)CAPACITY_TEST_FRAMES);
    if (g_runtime.stats.driver_rate == 0xFFU) {
        return -1;
    }

    (void)memset(&chip, 0, sizeof(chip));
    chip.bcn_agc = 32U;
    chip.interval = 32U;
    chip.conti_mode = 1U;
    chip.ant_en = 0xFFU;
    chip.rf_sel = 0xFFU;
    chip.tx_bcn_en = 1U;
    chip.rf_model = 1U;
    chip.irq_ctrl0 = 0x7FFU;

    callbacks.onRxData = CapacityOnRxData;
    callbacks.onTxSlot = CapacityOnTxSlot;
    callbacks.onSlotEnd = CapacityOnSlotEnd;
    callbacks.onError = CapacityOnError;
    TK8710RegisterCallbacks(&callbacks);

    if (TK8710Init(&chip) != TK8710_OK) {
        return -1;
    }

    (void)memset(&slot, 0, sizeof(slot));
    slot.msMode = TK8710_MODE_LOOPBACK;
    slot.rateCount = 1U;
    slot.rateModes[0] = (rateMode_e)g_runtime.stats.driver_rate;
    slot.antEn = 0xFFU;
    slot.rfSel = 0xFFU;
    slot.txBeamCtrlMode = 1U;
    slot.txBcnAntEn = 0x7FU;
    slot.md_agc = 1024U;
    slot.local_sync = 0U;
    slot.brdFreq[0] = 20000.0;
    for (antenna = 0U; antenna < TK8710_MAX_ANTENNAS; antenna++) {
        slot.bcnRotation[antenna] = (uint8_t)antenna;
    }
    gap = CapacityRateGap(g_runtime.stats.driver_rate);
    slot.s0Cfg[0].centerFreq = CAPACITY_TEST_CENTER_FREQ_HZ;
    slot.s1Cfg[0].byteLen = 1U;
    slot.s1Cfg[0].centerFreq = CAPACITY_TEST_CENTER_FREQ_HZ;
    slot.s1Cfg[0].da_m = 60000U;
    slot.s2Cfg[0].centerFreq = CAPACITY_TEST_CENTER_FREQ_HZ;
    slot.s3Cfg[0].byteLen = CAPACITY_TEST_PAYLOAD_LEN;
    slot.s3Cfg[0].centerFreq = CAPACITY_TEST_CENTER_FREQ_HZ;
    slot.s3Cfg[0].da_m = gap;

    if (TK8710SetConfig(TK8710_CFG_TYPE_SLOT_CFG, &slot) != TK8710_OK) {
        return -1;
    }
    if (CapacityLoadSimulationVectors() != 0) {
        return -1;
    }
    g_runtime.tx_frames = 0U;
    g_runtime.rx_frames = 0U;
    g_runtime.next_tx_sequence = 0U;
    g_runtime.running = 1U;
    if (TK8710TestRxInjectionEnable(users) != TK8710_OK) {
        return -1;
    }
    if (CapacitySubmitUsers(0U) != 0) {
        TK8710TestRxInjectionDisable();
        return -1;
    }
    TK8710SetSimulationDataLoaded(1U);
    if (TK8710Start(TK8710_MODE_LOOPBACK,
                    TK8710_WORK_MODE_CONTINUOUS) != TK8710_OK) {
        TK8710SetSimulationDataLoaded(0U);
        TK8710TestRxInjectionDisable();
        return -1;
    }
    return 0;
}

static int CapacityRunCase(uint8_t protocol_rate, uint8_t users)
{
    uint64_t start_ms;
    uint64_t now_ms;
    TK8710Tms570Stats port_stats_before;
    TK8710Tms570Stats port_stats;
    uint32_t spi_error_delta;

    (void)memset(&g_runtime, 0, sizeof(g_runtime));
    g_payloadDiagnosticPrinted = 0U;
    CapacityLog("CASE rate=");
    CapacityLogU32(protocol_rate);
    CapacityLog(" driver_rate=");
    CapacityLogU32(CapacityTest_MapProtocolRate(protocol_rate));
    CapacityLog(" users=");
    CapacityLogU32(users);
    CapacityLog(" frames=");
    CapacityLogU32(CAPACITY_TEST_FRAMES);
    CapacityLog(" START\r\n");

    TK8710Tms570GetStats(&port_stats_before);
    (void)TK8710Reset(TK8710_RST_ALL);
    TK8710DelayMs(20U);
    if (CapacityConfigure(protocol_rate, users) != 0) {
        g_runtime.stats.driver_errors++;
        CapacityLog("CASE setup failed\r\n");
        return 0;
    }

    start_ms = TK8710GetTimeUs() / 1000U;
    while ((g_runtime.rx_frames < CAPACITY_TEST_FRAMES) ||
           (g_runtime.tx_frames < CAPACITY_TEST_FRAMES)) {
        TK8710Tms570PollIrq();
        TK8710ProcessRuntimeWatchdog();
        now_ms = TK8710GetTimeUs() / 1000U;
        if ((now_ms - start_ms) >
            ((uint64_t)CAPACITY_TEST_TIMEOUT_MS * CAPACITY_TEST_FRAMES)) {
            CapacityTest_RecordTimeout(&g_runtime.stats);
            break;
        }
    }
    g_runtime.running = 0U;
    TK8710TestRxInjectionDisable();
    TK8710SetSimulationDataLoaded(0U);
    (void)TK8710Reset(TK8710_RST_STATE_MACHINE);
    TK8710Tms570GetStats(&port_stats);
    spi_error_delta = port_stats.spi_error_count - port_stats_before.spi_error_count;
    if (spi_error_delta != 0U) {
        g_runtime.stats.driver_errors += spi_error_delta;
    }

    CapacityLog("CASE rx_frames="); CapacityLogU32(g_runtime.rx_frames);
    CapacityLog(" tx_frames="); CapacityLogU32(g_runtime.tx_frames);
    CapacityLog(" rx_valid="); CapacityLogU32(g_runtime.stats.rx_valid_total);
    CapacityLog(" rx_crc_error="); CapacityLogU32(g_runtime.stats.rx_crc_errors);
    CapacityLog(" rx_missing="); CapacityLogU32(g_runtime.stats.rx_missing);
    CapacityLog(" rx_duplicate="); CapacityLogU32(g_runtime.stats.rx_duplicates);
    CapacityLog(" rx_range="); CapacityLogU32(g_runtime.stats.rx_out_of_range);
    CapacityLog(" rx_payload="); CapacityLogU32(g_runtime.stats.rx_payload_errors);
    CapacityLog(" tx_submit="); CapacityLogU32(g_runtime.stats.tx_submitted_total);
    CapacityLog(" tx_complete="); CapacityLogU32(g_runtime.stats.tx_completed_total);
    CapacityLog(" tx_failed="); CapacityLogU32(g_runtime.stats.tx_failed);
    CapacityLog(" tx_missing="); CapacityLogU32(g_runtime.stats.tx_missing);
    CapacityLog(" driver_error="); CapacityLogU32(g_runtime.stats.driver_errors);
    CapacityLog(" timeout="); CapacityLogU32(g_runtime.stats.timeouts);
    CapacityLog(" spi_error_delta="); CapacityLogU32(spi_error_delta);
    CapacityLog(" spi_error_total="); CapacityLogU32(port_stats.spi_error_count);
    CapacityLog(" RESULT=");
    CapacityLog(CapacityTest_Passed(&g_runtime.stats) != 0 ? "PASS\r\n" : "FAIL\r\n");
    return CapacityTest_Passed(&g_runtime.stats);
}

int main(void)
{
    uint32_t version = 0U;
    uint32_t test;
    uint32_t passed = 0U;

    if (TK8710Tms570Init() != 0) {
        g_capacityTestExitCode = 2U;
        for (;;) {
        }
    }
    CapacityLog("\r\nTK8710 RATE MODE CAPACITY TEST\r\n");
    CapacityLog("NOTE: driver loopback capacity test; no real RF terminal is used.\r\n");
    if ((TK8710SpiReset(TK8710_RST_SM_AND_REG) != 0) ||
        (TK8710SpiReadReg((uint16_t)CAPACITY_TEST_VERSION_REG,
                         &version, 1U) != 0) ||
        (version == 0U) || (version == 0xFFFFFFFFU)) {
        CapacityLog("TK8710 bring-up failed\r\n");
        g_capacityTestExitCode = 3U;
        for (;;) {
        }
    }
    _enable_interrupt_();

    for (test = 0U; test < CAPACITY_TEST_RESULT_COUNT; test++) {
        if ((CAPACITY_TEST_PROTOCOL_RATE <= 2U) &&
            (g_cases[test].protocol_rate != CAPACITY_TEST_PROTOCOL_RATE)) {
            continue;
        }
        if ((CAPACITY_TEST_USER_COUNT != 0U) &&
            (g_cases[test].user_count != CAPACITY_TEST_USER_COUNT)) {
            continue;
        }
        g_case_passed[test] = (uint8_t)CapacityRunCase(
            g_cases[test].protocol_rate, g_cases[test].user_count);
        if (g_case_passed[test] != 0U) {
            passed++;
        }
        g_capacityTestCompletedCases++;
    }

    CapacityLog("SUMMARY completed="); CapacityLogU32(g_capacityTestCompletedCases);
    CapacityLog(" passed="); CapacityLogU32(passed);
    CapacityLog(" failed="); CapacityLogU32(g_capacityTestCompletedCases - passed);
    if ((g_capacityTestCompletedCases != 0U) &&
        (passed == g_capacityTestCompletedCases)) {
        CapacityLog(" RESULT=PASS\r\n");
        g_capacityTestExitCode = 0U;
    } else {
        CapacityLog(" RESULT=FAIL\r\n");
        g_capacityTestExitCode = 1U;
    }
    for (;;) {
    }
}

#else
int main(void)
{
    return 1;
}
#endif
