#include "../../src/trm/trm_pps_model.h"
#include <assert.h>
#include <stdio.h>

static const TrmPpsConfig single = {
    .cycle_us = 280000, .cycles = 25, .super_frames = 1, .rates = 1,
    .frame_us = {280000}, .s0_us = {10732}
};
static const uint64_t epoch = 10000000;

static TrmPpsModel ready(void)
{
    TrmPpsModel m;
    assert(TrmPpsModelInit(&m, &single, epoch - 100000) == 0);
    assert(TrmPpsModelEdge(&m, epoch) == 1);
    assert(TrmPpsModelFrame(&m, epoch + 10732, epoch, 0, 0, 1) == 0);
    assert(TrmPpsModelDeadline(&m, epoch + 11000) == epoch + 7000000);
    return m;
}

static void missing_and_recovery(void)
{
    TrmPpsModel m = ready();
    for (unsigned int n = 1; n <= 4; ++n) {
        TrmPpsModelTick(&m, epoch + n * m.period + m.tolerance + 1);
        assert(m.missing == n && !m.fatal);
        assert(!TrmPpsModelDeadline(&m, epoch + n * m.period + 6000));
    }
    /* Pulse at the fifth boundary means four missed pulses, not five. */
    uint64_t recovered = epoch + 5 * m.period;
    m.restart_wait = 1; /* Hardware was armed by ACM and waits across missing pulses. */
    m.restart_from = epoch + 20000;
    assert(TrmPpsModelEdge(&m, recovered) == 1);
    assert(m.missing == 0 && !m.phase_ok);
    assert(TrmPpsModelFrame(&m, recovered + 10732, recovered, 0, 0, 1) == 0);
    assert(TrmPpsModelDeadline(&m, recovered + 11000) == recovered + m.period);
    m = ready();
    TrmPpsModelTick(&m, epoch + 5 * m.period + m.tolerance + 1);
    assert(m.fatal && m.missing == 5);
    assert(TrmPpsModelEdge(&m, epoch + 6 * m.period) == 0);
}

static void superframe_and_rates(void)
{
    TrmPpsConfig c = {.cycle_us = 500000, .cycles = 40, .super_frames = 20,
        .rates = 2, .frame_us = {300000, 200000}, .s0_us = {10000, 8000}};
    TrmPpsModel m;
    assert(TrmPpsModelInit(&m, &c, epoch - 100) == 0);
    assert(TrmPpsModelEdge(&m, epoch) == 1);
    for (uint32_t cycle = 0; cycle < 40; ++cycle) {
        uint64_t start = epoch + cycle * (uint64_t)c.cycle_us;
        assert(TrmPpsModelFrame(&m, start + 10000, epoch,
            cycle, 0, cycle * 2 % 20 + 1) == 0);
        assert(TrmPpsModelFrame(&m, start + 308000, epoch,
            cycle, 1, (cycle * 2 + 1) % 20 + 1) == 0);
    }
    uint64_t next = epoch + m.period;
    assert(TrmPpsModelEdge(&m, next) == 1);
    assert(TrmPpsModelFrame(&m, next + 10000, next, 0, 0, 1) == 0);
    assert(TrmPpsModelFrame(&m, next + 10000, next, 0, 0, 2) == -1);
    assert(TrmPpsModelFrame(&m, next + 510000, next, 0, 0, 1) == -1);
    c.cycles = 25; /* Not a multiple of business superframe length. */
    assert(TrmPpsModelInit(&m, &c, epoch) == -1);
}

static void jitter_glitch_and_order(void)
{
    TrmPpsModel m = ready();
    assert(TrmPpsModelEdge(&m, epoch + 100) == 0);
    assert(m.edge == epoch);
    assert(TrmPpsModelFrame(&m, epoch + 280000 + 10732 + 2000, epoch, 1, 0, 1) == 1);
    assert(!m.fatal && m.phase_ok);
    /* Whole-frame slip must not silently renumber business queues. */
    assert(TrmPpsModelFrame(&m, epoch + 290732, epoch, 0, 0, 1) == -1);
    /* Delayed processing of a previous-cycle S0 after the next GPIO arrives. */
    m = ready();
    for (uint32_t cycle = 1; cycle < 24; ++cycle) {
        assert(TrmPpsModelFrame(&m, epoch + cycle * 280000 + 10732, epoch, cycle, 0, 1) == 0);
    }
    uint64_t next = epoch + m.period;
    assert(TrmPpsModelEdge(&m, next) == 1);
    assert(TrmPpsModelFrame(&m, next - 280000 + 10732, epoch, 24, 0, 1) == 0);
    assert(!m.phase_ok);
    assert(TrmPpsModelFrame(&m, next + 10732, next, 0, 0, 1) == 0);
    /* Missing an entire cycle is still detected even when modulo counters match. */
    assert(TrmPpsModelFrame(&m, next + m.period + 10732, next, 0, 0, 1) == -1);
    m = ready();
    assert(TrmPpsModelEdge(&m, epoch + m.period + 280000) == -1);
    assert(m.fatal);
    m = ready();
    assert(TrmPpsModelEdge(&m, epoch + m.period + 10000) == 1);
    assert(!m.fatal && !m.missing && !m.phase_ok);
}

static void initial_timeout_and_reset(void)
{
    TrmPpsModel m;
    assert(!TrmPpsModelInit(&m, &single, epoch));
    TrmPpsModelTick(&m, epoch + 5 * m.period + 6000);
    assert(m.fatal);
    assert(!TrmPpsModelInit(&m, &single, epoch + 100000000));
    assert(!m.fatal && !m.locked && !m.edge);
    assert(!TrmPpsModelEdge(&m, epoch)); /* Old configuration event. */
}

static void running_frames_during_pulse_loss(void)
{
    TrmPpsModel m = ready();
    /* Hardware keeps running while the GPIO input misses four pulses. No
     * renumbering and no ACM-restart gap exemption is used here. */
    for (uint32_t frame = 1; frame < 125; ++frame) {
        uint64_t stamp = epoch + (uint64_t)frame * 280000 + 10732;
        TrmPpsModelTick(&m, stamp);
        assert(!m.fatal);
        assert(TrmPpsModelFrame(&m, stamp, epoch, frame % 25, 0, 1) == 0);
        if (frame >= 25) assert(!TrmPpsModelDeadline(&m, stamp));
    }
    uint64_t edge = epoch + 5 * m.period;
    assert(TrmPpsModelEdge(&m, edge) == 1);
    assert(TrmPpsModelFrame(&m, edge + 10732, edge, 0, 0, 1) == 0);
    assert(TrmPpsModelDeadline(&m, edge + 11000) == edge + m.period);
}

int main(void)
{
    missing_and_recovery();
    superframe_and_rates();
    jitter_glitch_and_order();
    initial_timeout_and_reset();
    running_frames_during_pulse_loss();
    puts("PPS GPIO timing model tests passed");
    return 0;
}
