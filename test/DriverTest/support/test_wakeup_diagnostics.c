/* Diagnostic regression: normal initialization plus injected invalid state.
 * No hardware access. Include implementation for diagnostic fault injection.
 */
#include "../../../src/driver/tk8710_wakeup.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint32_t regs[65536];
static uint64_t ticks = 1000000;
static int depth;
static int fail_read_addr = -1;
static char output[131072];
static size_t output_len;

void TK8710EnterCritical(void) { assert(depth == 0); depth = 1; }
void TK8710ExitCritical(void) { assert(depth == 1); depth = 0; }
uint64_t TK8710GetMonotonicTimeUs(void) { return ticks; }
void TK8710DelayMs(uint32_t ms) { ticks += (uint64_t)ms * 1000u; }
TK8710LogLevel TK8710LogGetLevel(void) { return TK8710_LOG_DEBUG; }
uint32_t TK8710LogGetModuleMask(void) { return TK8710_LOG_MODULE_ALL; }
void TK8710LogOutput(TK8710LogLevel level, TK8710LogModule module,
                     const char* file, int line, const char* func, const char* fmt, ...)
{
    va_list args;
    int length;
    (void)level; (void)module; (void)file; (void)line; (void)func;
    va_start(args, fmt);
    length = vsnprintf(output + output_len, sizeof(output) - output_len, fmt, args);
    va_end(args);
    assert(length >= 0 && (size_t)length < sizeof(output) - output_len - 2);
    output_len += (size_t)length;
    output[output_len++] = '\n'; output[output_len] = 0;
}
int TK8710WriteReg(uint8_t type, uint16_t addr, uint32_t value)
{
    (void)type;
    regs[addr] = value;
    return 0;
}
int TK8710ReadReg(uint8_t type, uint16_t addr, uint32_t* value)
{
    (void)type;
    if (addr == fail_read_addr) return -9;
    *value = regs[addr];
    return 0;
}
int TK8710SpiReset(uint8_t type) { assert(type == 1); return 0; }

int main(void)
{
    s_init_9 init9 = { .data = 0 };
    TK8710WakeupStatus status;
    init9.b.ant_en = init9.b.rf_sel = 0xff;
    regs[MAC_BASE + offsetof(struct mac, init_9)] = init9.data;
    assert(TK8710WakeupPrepareInit() == 0);
    TK8710WakeupInitComplete();

    /* Normal startup must prepare the mask and select one path before trigger. */
    assert(TK8710SendWakeup(3, 100, 1) == 0);
    assert(strstr(output, "STARTED prepared=1 mask=0xFF current=0"));
    assert(!strstr(output, "PREP_SKIPPED"));
    assert(g_current_antenna == 0 && g_antenna_mask == 0xff);
    /* Inject the zero-mask regression from the supplied board log. */
    g_antenna_mask = 0;
    g_s0_count = 15;
    regs[MAC_BASE + offsetof(struct mac, irq_res)] = 1u << TK8710_IRQ_S0;
    assert(TK8710WakeupProcessIrq() == 1);
    TK8710WakeupDispatchCompletion();
    assert(TK8710GetWakeupStatus(&status) == 0);
    assert(status.state == TK8710_WAKEUP_ERROR && status.stopped);
    assert(strstr(output, "EMPTY_MASK"));
    assert(strstr(output, "INVALID_ANT"));

    /* Restart resets dwell count; switch only at S0 16. */
    assert(TK8710SendWakeup(3, 100, 1) == 0);
    regs[MAC_BASE + offsetof(struct mac, irq_res)] = 1u << TK8710_IRQ_S0;
    assert(TK8710WakeupProcessIrq() == 1);
    assert(g_s0_count == 1 && g_current_antenna == 0);
    for (unsigned int step = 2; step <= 16; step++) {
        regs[MAC_BASE + offsetof(struct mac, irq_res)] = 1u << TK8710_IRQ_S0;
        assert(TK8710WakeupProcessIrq() == 1);
    }
    assert(strstr(output, "HOLD s0=1 prev=0 requested=0 sw_current=0"));
    assert(strstr(output, "SWITCH s0=16 prev=0 requested=1 sw_current=1"));
    assert(!strstr(output, "TRACKING_MISMATCH"));
    assert(strstr(output, "active=0x02"));
    assert(strstr(output, "addr=0x1878"));
    assert(strstr(output, "write=0x00060040 read=0x00060040"));
    assert(g_current_antenna == 1);
    /* Verify the diagnostic still detects deliberately corrupted tracking. */
    g_current_antenna = 0;
    _wakeup_trace_antennas("injected-tracking-error", 1);
    assert(strstr(output, "TRACKING_MISMATCH selected=1 sw_current=0"));
    g_current_antenna = 1;
    fail_read_addr = 0x1878;
    _wakeup_trace_antennas("test-read-error", 1);
    assert(strstr(output, "READ_FAIL phase=test-read-error"));
    fail_read_addr = -1;
    assert(TK8710CancelWakeup() == 0);
    puts(output);
    puts("PASS: prepared startup, advancing antenna state, injected empty mask/tracking/read faults");
    return 0;
}
