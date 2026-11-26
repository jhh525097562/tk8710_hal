/* Host test: link with src/driver/tk8710_wakeup.c; no hardware access. */
#include "driver/tk8710_driver_api.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"
#include "tk8710_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t registers[65536];
static uint32_t register_writes[65536];
static uint64_t now_us = 1000000;
static unsigned int reset_count, toggle_count, callback_count;
static unsigned int write_count, fail_write_at;
static unsigned int antenna_writes;
static int reset_failure, lock_depth, read_failure;
static TK8710WakeupStatus event;

/* Keep the functional fixture independent of production logging/output files. */
TK8710LogLevel TK8710LogGetLevel(void) { return TK8710_LOG_NONE; }
uint32_t TK8710LogGetModuleMask(void) { return 0; }
void TK8710LogOutput(TK8710LogLevel level, TK8710LogModule module,
                     const char* file, int line, const char* func, const char* fmt, ...)
{
    (void)level; (void)module; (void)file; (void)line; (void)func; (void)fmt;
}

void TK8710EnterCritical(void) { assert(lock_depth == 0); lock_depth = 1; }
void TK8710ExitCritical(void) { assert(lock_depth == 1); lock_depth = 0; }
uint64_t TK8710GetMonotonicTimeUs(void) { return now_us; }
void TK8710DelayMs(uint32_t ms) { now_us += (uint64_t)ms * 1000; }
int TK8710SpiReset(uint8_t type)
{
    assert(type == TK8710_RST_STATE_MACHINE);
    reset_count++;
    return reset_failure ? -1 : 0;
}
int TK8710WriteReg(uint8_t type, uint16_t address, uint32_t value)
{
    assert(type == TK8710_REG_TYPE_GLOBAL);
    write_count++;
    if (write_count == fail_write_at) return -7;
    registers[address] = value;
    register_writes[address]++;
    if (address >= TX_FE_BASE + 0x78u &&
        (address - TX_FE_BASE - 0x78u) % 0x1000u == 0 &&
        (address - TX_FE_BASE - 0x78u) / 0x1000u < TK8710_MAX_ANTENNAS) {
        unsigned int active = 0;
        unsigned int ant;
        antenna_writes++;
        for (ant = 0; ant < TK8710_MAX_ANTENNAS; ant++) {
            s_tx_config_28 gain;
            gain.data = registers[TX_FE_BASE + 0x78u + ant * 0x1000u];
            if (gain.b.sc_bcn != 0) active++;
        }
        assert(active <= 1); /* No transient overlap during rotation. */
    }
    if (address == 0xc008) toggle_count++;
    if (address == MAC_BASE + offsetof(struct mac, irq_ctrl1)) {
        registers[MAC_BASE + offsetof(struct mac, irq_res)] &= ~value;
    }
    return 0;
}
int TK8710ReadReg(uint8_t type, uint16_t address, uint32_t* value)
{
    (void)type;
    if (read_failure) return -8;
    *value = registers[address];
    return 0;
}
static void completed(const TK8710WakeupStatus* status, void* user)
{
    TK8710WakeupStatus copy;
    assert(user == &event);
    assert(lock_depth == 0); /* Callback can query without deadlock. */
    assert(TK8710GetWakeupStatus(&copy) == 0);
    event = *status;
    callback_count++;
}
static void initialize(void)
{
    s_init_9 init9;
    reset_failure = read_failure = 0;
    fail_write_at = 0;
    assert(TK8710WakeupPrepareInit() == 0);
    TK8710WakeupInitComplete();
    assert(TK8710RegisterWakeupCallback(completed, &event) == 0);
    memset(registers, 0, sizeof(registers));
    memset(register_writes, 0, sizeof(register_writes));
    init9.data = 0;
    init9.b.ant_en = 0xff;
    init9.b.rf_sel = 0xff;
    init9.b.tx_bcn_ant_en = 1;
    registers[MAC_BASE + offsetof(struct mac, init_9)] = init9.data;
    for (unsigned int ant = 0; ant < TK8710_MAX_ANTENNAS; ant++) {
        registers[TX_FE_BASE + 0x78u + ant * 0x1000u] = 0x011000a5u;
    }
    now_us = 1000000;
}
static void expect_antenna(unsigned int selected)
{
    for (unsigned int ant = 0; ant < TK8710_MAX_ANTENNAS; ant++) {
        s_tx_config_28 gain;
        gain.data = registers[TX_FE_BASE + 0x78u + ant * 0x1000u];
        assert(gain.b.sc_bcn == (ant == selected ? 0x60 : 0));
        assert(gain.b.sc_da == 0x40);
        assert((gain.data & ~0x000ff0ffu) == (0x011000a5u & ~0x000ff0ffu));
    }
}
static void irq(uint32_t bits)
{
    registers[MAC_BASE + offsetof(struct mac, irq_res)] = bits;
    assert(TK8710WakeupProcessIrq() == 1);
    TK8710WakeupDispatchCompletion();
}
int main(void)
{
    TK8710WakeupStatus status;
    unsigned int callbacks, resets, toggles;
    unsigned int mode, id, failure;
    unsigned int startup_writes;

    assert(TK8710SendWakeup(TK8710_WAKEUP_MODE_3, 100, 32) != 0); /* No Init. */
    initialize();
    assert(TK8710WakeupProcessIrq() == 0); /* Normal IRQ bypasses wakeup. */
    assert(TK8710SendWakeup(0, 100, 0) != 0);
    assert(TK8710SendWakeup(4, 100, 0) != 0);
    assert(TK8710SendWakeup(3, 0, 0) != 0);
    assert(TK8710SendWakeup(3, 0x1000000, 0) != 0);
    assert(TK8710GetWakeupStatus(NULL) != 0);

    startup_writes = write_count;
    assert(TK8710SendWakeup(3, 1000, 32) == 0);
    startup_writes = write_count - startup_writes;
    expect_antenna(0);
    assert(TK8710SendWakeup(3, 100, 33) != 0);
    assert(TK8710WakeupPrepareInit() != 0);
    assert(TK8710RegisterWakeupCallback(NULL, NULL) != 0);
    callbacks = callback_count;
    irq(1u << TK8710_IRQ_S0); /* Before deadline: toggle, no completion. */
    expect_antenna(0);
    for (unsigned int step = 2; step <= 256; step++) {
        unsigned int old_writes = antenna_writes;
        irq(1u << TK8710_IRQ_S0);
        expect_antenna((step / 16u) % 8u);
        assert(antenna_writes == old_writes + (step % 16u == 0 ? 9u : 0u));
    }
    assert(callback_count == callbacks && registers[0xc008] == 0x80);
    now_us = 2000000;
    resets = reset_count;
    assert(TK8710GetWakeupStatus(&status) == 0 && status.state == TK8710_WAKEUP_WAIT_BCN);
    irq(1u << TK8710_IRQ_S1); /* Deadline alone/non-S0 cannot complete. */
    assert(reset_count == resets && callback_count == callbacks);
    toggles = toggle_count;
    unsigned int last_antenna_writes = antenna_writes;
    irq(1u << TK8710_IRQ_S0);
    assert(event.state == TK8710_WAKEUP_DONE && event.stopped && event.result == 0);
    assert(callback_count == callbacks + 1 && toggle_count == toggles);
    assert(antenna_writes == last_antenna_writes);
    irq(1u << TK8710_IRQ_S0); /* Stale IRQ cannot toggle or duplicate completion. */
    assert(callback_count == callbacks + 1 && toggle_count == toggles);
    TK8710CancelWakeup();
    assert(callback_count == callbacks + 1);

    /* Reuse stopped configuration for every mode/ID; verify deadline at equality. */
    for (mode = 1; mode <= 3; mode++) {
        for (id = 0; id <= 255; id++) {
            uint32_t group = id / 32;
            const uint16_t antenna0_addresses[] = {0x081c, 0x0820, 0x0814, 0x0818, 0x082c, 0x0824};
            const uint32_t expected[] = {
                0, 0, 0x100000u | ((id % 32u) << 4), 0x0006008c, 0x20,
                (mode == 3 ? 0x4d0000u : 0x8d0000u) + group * 0x100000u
            };
            for (unsigned int ant = 0; ant < 8; ant++) {
                for (unsigned int reg = 0; reg < 6; reg++) {
                    unsigned int address = antenna0_addresses[reg] + ant * 0x1000u;
                    registers[address] = 0xdeadbeef;
                    register_writes[address] = 0;
                }
            }
            register_writes[0xc000] = register_writes[0xc074] = 0;
            assert(TK8710SendWakeup((wakeUpMode_e)mode, 1, (uint8_t)id) == 0);
            for (unsigned int ant = 0; ant < 8; ant++) {
                for (unsigned int reg = 0; reg < 6; reg++) {
                    unsigned int address = antenna0_addresses[reg] + ant * 0x1000u;
                    assert(registers[address] == expected[reg]);
                    assert(register_writes[address] == 1);
                }
            }
            assert(register_writes[0xc000] == 1 && register_writes[0xc074] == 1);
            assert(registers[0xc054] == group * (mode == 1 ? 0x800u : 0x400u));
            assert(registers[0xc000] == (mode + 6u) << 20);
            now_us += 1000;
            irq(1u << TK8710_IRQ_S0);
            assert(event.state == TK8710_WAKEUP_DONE);
        }
    }

    assert(TK8710SendWakeup(3, 0xffffff, 255) == 0);
    callbacks = callback_count;
    assert(TK8710CancelWakeup() == 0);
    assert(event.state == TK8710_WAKEUP_CANCELLED && event.stopped);
    assert(callback_count == callbacks + 1);

    /* Every startup write failure rolls back; no false successful launch/event. */
    for (failure = 1; failure <= startup_writes; failure++) {
        initialize();
        callbacks = callback_count;
        fail_write_at = write_count + failure;
        assert(TK8710SendWakeup(3, 100, 7) != 0);
        assert(TK8710GetWakeupStatus(&status) == 0 && status.state == TK8710_WAKEUP_ERROR);
        assert(status.stopped);
        TK8710WakeupDispatchCompletion();
        assert(callback_count == callbacks);
    }
    /* Sparse Init mask: 0,2,4 only; independent of the ordinary slot table. */
    initialize();
    s_init_9 sparse;
    sparse.data = registers[MAC_BASE + offsetof(struct mac, init_9)];
    sparse.b.ant_en = 0x95;
    sparse.b.rf_sel = 0x1f;
    registers[MAC_BASE + offsetof(struct mac, init_9)] = sparse.data;
    assert(TK8710SendWakeup(3, 1000, 1) == 0);
    assert(registers[0x7814] == 0x00100010u); /* Program disabled paths too. */
    assert(registers[0x7824] == 0x004d0000u);
    for (unsigned int step = 0; step < 144; step++) {
        expect_antenna(((step / 16u) % 3) * 2);
        irq(1u << TK8710_IRQ_S0);
    }
    assert(TK8710CancelWakeup() == 0);
    initialize();
    sparse.b.ant_en = 0;
    registers[MAC_BASE + offsetof(struct mac, init_9)] = sparse.data;
    assert(TK8710SendWakeup(3, 100, 1) != 0);

    /* A rotation write failure must stop and report one error event. */
    for (failure = 1; failure <= 9; failure++) {
        initialize();
        assert(TK8710SendWakeup(3, 100, 1) == 0);
        callbacks = callback_count;
        for (unsigned int step = 0; step < 15; step++) irq(1u << TK8710_IRQ_S0);
        fail_write_at = write_count + 2 + failure; /* IRQ clear, c008, antenna write. */
        irq(1u << TK8710_IRQ_S0);
        assert(event.state == TK8710_WAKEUP_ERROR && event.stopped);
        assert(callback_count == callbacks + 1);
    }
    initialize();
    assert(TK8710SendWakeup(3, 100, 1) == 0);
    for (unsigned int step = 0; step < 15; step++) irq(1u << TK8710_IRQ_S0);
    unsigned int boundary_writes = antenna_writes;
    now_us += 100000;
    irq(1u << TK8710_IRQ_S0);
    assert(event.state == TK8710_WAKEUP_DONE && antenna_writes == boundary_writes);
    initialize();
    assert(TK8710SendWakeup(3, 10, 1) == 0);
    reset_failure = 1;
    now_us += 10000;
    irq(1u << TK8710_IRQ_S0);
    assert(event.state == TK8710_WAKEUP_ERROR && !event.stopped);
    assert(TK8710WakeupBusy());
    assert(TK8710SendWakeup(3, 10, 2) != 0);
    initialize(); /* Explicit recovery via Init can clear stop failure. */
    assert(TK8710SendWakeup(3, 10, 1) == 0);
    read_failure = 1;
    irq(1u << TK8710_IRQ_S0);
    assert(event.state == TK8710_WAKEUP_ERROR && event.stopped);
    read_failure = 0;
    initialize();
    now_us = 0;
    assert(TK8710SendWakeup(3, 10, 1) != 0);
    printf("PASS: lifecycle, 768 mode/ID mappings, %u startup faults, antenna rotation/wrap/masks/9 rotation faults\n", startup_writes);
    return 0;
}
