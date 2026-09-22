#ifndef TRM_PPS_MODEL_H
#define TRM_PPS_MODEL_H
#include "trm/trm_pps_monitor.h"
#include <string.h>

typedef struct {
    TrmPpsConfig config;
    uint64_t period;
    uint64_t start;
    uint64_t edge;
    uint64_t epoch;
    uint64_t last_frame;
    uint32_t tolerance;
    uint32_t half_frame;
    uint32_t missing;
    uint8_t locked;
    uint8_t phase_ok;
    uint8_t fatal;
    uint8_t last_rate;
    uint8_t restart_wait;
    uint64_t restart_from;
} TrmPpsModel;

static inline uint64_t TrmPpsDistance(uint64_t a, uint64_t b)
{
    return a > b ? a - b : b - a;
}

static inline int TrmPpsModelInit(TrmPpsModel* m, const TrmPpsConfig* c, uint64_t now)
{
    uint64_t sum = 0;
    uint32_t minimum = UINT32_MAX;
    if (!c || !c->cycles || !c->cycle_us || !c->super_frames ||
        !c->rates || c->rates > 4 || c->cycles % c->super_frames) return -1;
    for (uint8_t i = 0; i < c->rates; ++i) {
        if (!c->s0_us[i] || c->s0_us[i] >= c->frame_us[i]) return -1;
        sum += c->frame_us[i];
        if (c->frame_us[i] < minimum) minimum = c->frame_us[i];
    }
    if (sum != c->cycle_us || minimum < 4000) return -1;
    memset(m, 0, sizeof(*m));
    m->config = *c;
    m->period = sum * c->cycles;
    if (m->period % 1000000) return -1;
    m->start = now;
    m->half_frame = minimum / 2;
    m->tolerance = minimum / 4 < 5000 ? minimum / 4 : 5000;
    return 0;
}

static inline void TrmPpsModelTick(TrmPpsModel* m, uint64_t now)
{
    uint64_t anchor = m->edge ? m->edge : m->start;
    uint64_t count = now > anchor + m->tolerance ?
        (now - anchor - m->tolerance) / m->period : 0;
    m->missing = count > UINT32_MAX ? UINT32_MAX : (uint32_t)count;
    if (m->missing) m->phase_ok = 0;
    if (m->missing >= 5) m->fatal = 1;
}

/* Returns 0 for a rejected edge, 1 for accepted, -1 for lost grid alignment.
 * Small glitches never reset the missing-pulse timer. */
static inline int TrmPpsModelEdge(TrmPpsModel* m, uint64_t stamp)
{
    if (stamp < m->start || m->fatal) return 0;
    TrmPpsModelTick(m, stamp);
    if (m->fatal) return -1;
    if (m->edge) {
        if (stamp <= m->edge || stamp - m->edge < m->period / 2) return 0;
        uint64_t delta = stamp - m->edge;
        uint64_t periods = (delta + m->period / 2) / m->period;
        uint64_t error = TrmPpsDistance(delta, periods * m->period);
        if (error >= m->half_frame) { m->fatal = 1; return -1; }
        /* Sub-frame jitter is observed, not counted as a missing pulse.
         * Subsequent frame correlation controls whether ACM may resume. */
    }
    m->edge = stamp;
    m->missing = 0;
    /* Require a frame correlated against this edge before resuming ACM. */
    m->phase_ok = 0;
    return 1;
}

/* The caller waits briefly for the GPIO event thread to drain the kernel queue.
 * edge_at_frame is the newest accepted edge at/before this frame start. */
static inline int TrmPpsModelFrame(TrmPpsModel* m, uint64_t stamp,
    uint64_t edge_at_frame, uint32_t cycle, uint8_t rate, uint32_t super_position)
{
    uint64_t start, offset, phase, error;
    if (m->fatal || rate >= m->config.rates || cycle >= m->config.cycles) return -1;
    if (stamp < m->config.s0_us[rate]) return -1;
    start = stamp - m->config.s0_us[rate];
    int restarting = m->restart_wait && stamp >= m->restart_from;
    if (m->last_frame && !restarting) {
        uint64_t expected_delta = m->config.frame_us[m->last_rate] -
            m->config.s0_us[m->last_rate] + m->config.s0_us[rate];
        if (stamp <= m->last_frame ||
            TrmPpsDistance(stamp - m->last_frame, expected_delta) >= m->half_frame) return -1;
    }
    if (restarting && (!edge_at_frame || cycle || rate ||
        TrmPpsDistance(start, edge_at_frame) > m->tolerance)) return -1;
    if (!m->locked) {
        if (!edge_at_frame || cycle || rate ||
            TrmPpsDistance(start, edge_at_frame) > m->tolerance) return -1;
        m->epoch = edge_at_frame;
        m->locked = 1;
    }
    offset = (uint64_t)cycle * m->config.cycle_us;
    for (uint8_t i = 0; i < rate; ++i) offset += m->config.frame_us[i];
    /* A late-delivered pulse may be just after the estimated frame start. */
    uint64_t base = edge_at_frame ? edge_at_frame : m->epoch;
    phase = start >= base ? (start - base) % m->period :
        (m->period - (base - start) % m->period) % m->period;
    error = TrmPpsDistance(phase, offset);
    if (error > m->period / 2) error = m->period - error;
    uint32_t expected_super = ((uint64_t)cycle * m->config.rates + rate) %
        m->config.super_frames + 1;
    if (error >= m->half_frame || super_position != expected_super) return -1;
    m->last_frame = stamp;
    m->last_rate = rate;
    if (restarting) m->restart_wait = 0;
    if (edge_at_frame == m->edge && !m->missing) m->phase_ok = error <= m->tolerance;
    return error > 1000 ? 1 : 0;
}

static inline uint64_t TrmPpsModelDeadline(TrmPpsModel* m, uint64_t now)
{
    TrmPpsModelTick(m, now);
    if (!m->locked || !m->phase_ok || m->fatal || m->missing || !m->edge ||
        now >= m->edge + m->period) return 0;
    return m->edge + m->period;
}
#endif
