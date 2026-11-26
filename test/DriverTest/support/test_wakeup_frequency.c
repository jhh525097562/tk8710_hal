/* Host-only frequency-offset regression against current wakeup configuration. */
#include "../../../src/driver/tk8710_wakeup.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t regs[65536];
static uint32_t writes[65536];
static int fail_address = -1;
static unsigned int depth;
void TK8710EnterCritical(void) { assert(depth == 0); depth++; }
void TK8710ExitCritical(void) { assert(depth == 1); depth--; }
uint64_t TK8710GetMonotonicTimeUs(void) { return 1000000; }
void TK8710DelayMs(uint32_t ms) { (void)ms; }
int TK8710SpiReset(uint8_t type) { assert(type == TK8710_RST_STATE_MACHINE); return 0; }
int TK8710WriteReg(uint8_t type, uint16_t address, uint32_t value)
{
    assert(type == TK8710_REG_TYPE_GLOBAL);
    if (address == fail_address) return -7;
    regs[address] = value;
    writes[address]++;
    return 0;
}
int TK8710ReadReg(uint8_t type, uint16_t address, uint32_t* value)
{
    (void)type;
    *value = regs[address];
    return 0;
}
TK8710LogLevel TK8710LogGetLevel(void) { return TK8710_LOG_NONE; }
uint32_t TK8710LogGetModuleMask(void) { return 0; }
void TK8710LogOutput(TK8710LogLevel level, TK8710LogModule module,
                     const char* file, int line, const char* func, const char* fmt, ...)
{
    (void)level; (void)module; (void)file; (void)line; (void)func; (void)fmt;
}

int main(void)
{
    const uint32_t expected[2][8] = {
        {0x01f2b020, 0, 0x01fb645a, 0, 0x00041893, 0, 0x000ccccc, 0},
        {0x01e56041, 0, 0x01f6c8b4, 0, 0x0008b439, 0, 0x00199999, 0}
    };
    TK8710WakeupStatus status;
    unsigned int mode, id, ant;
    /* Acknowledge the user's deliberately disabled preparation helper. */
    (void)_wakeup_prepare_antennas;
    assert(_wakeup_bcn_phase_word(1000) == 33554u);
    assert(_wakeup_bcn_phase_word(-1000) == 33520877u);
    assert(_wakeup_bcn_phase_word(0) == 0);
    assert(TK8710WakeupPrepareInit() == 0);
    TK8710WakeupInitComplete();
    for (mode = 1; mode <= 3; mode++) {
        for (id = 0; id <= 255; id++) {
            for (ant = 0; ant < 8; ant++) {
                regs[0x0830 + ant * 0x1000] = 0xdeadbeef;
                writes[0x0830 + ant * 0x1000] = 0;
            }
            assert(TK8710SendWakeup((wakeUpMode_e)mode, 5000, (uint8_t)id) == 0);
            for (ant = 0; ant < 8; ant++) {
                uint32_t word = regs[0x0830 + ant * 0x1000];
                assert(word == expected[mode == 3 ? 1 : 0][ant]);
                assert(word < (1u << 25));
                assert(writes[0x0830 + ant * 0x1000] == 1);
                assert(regs[0x0878 + ant * 0x1000] == (ant % 2 == 0 ? 0x60040u : 0));
            }
            assert(TK8710CancelWakeup() == 0);
        }
    }
    /* Each per-path write failure must abort before triggering RF. */
    for (ant = 0; ant < 8; ant++) {
        memset(writes, 0, sizeof(writes));
        fail_address = 0x0830 + ant * 0x1000;
        assert(TK8710SendWakeup(3, 5000, 47) == -7);
        assert(writes[0xc074] == 0);
        assert(TK8710GetWakeupStatus(&status) == 0);
        assert(status.state == TK8710_WAKEUP_ERROR && status.result == -7 && status.stopped);
    }
    puts("PASS: 768 mode/ID starts x 8 phase words; +/-1k/zero encoding; 8 frequency-write faults; existing amplitude settings preserved");
    return 0;
}
