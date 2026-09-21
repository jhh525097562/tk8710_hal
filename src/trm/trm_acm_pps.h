#ifndef TRM_ACM_PPS_H
#define TRM_ACM_PPS_H

#include <stdint.h>

/* frameCount counts whole rate cycles, not individual S3 interrupts. */
typedef struct {
    uint32_t cycle_count;
    uint32_t cycle_index;
    uint8_t rate_count;
    uint8_t rate_index;
    uint8_t anchored;
    uint8_t invalid;
} TrmAcmPpsClock;

static inline int TrmAcmPpsHasTime(uint64_t now, uint64_t deadline, uint64_t budget)
{
    return deadline > now && deadline - now > budget;
}

static inline int TrmAcmPpsObserveS0(TrmAcmPpsClock* clock, uint8_t rate)
{
    if (!clock->cycle_count || !clock->rate_count || clock->invalid) return -1;
    if (!clock->anchored) {
        if (rate != 0) { clock->invalid = 1; return -1; }
        clock->anchored = 1;
        clock->cycle_index = 0;
    } else {
        if (rate != (clock->rate_index + 1u) % clock->rate_count) {
            clock->invalid = 1;
            return -1;
        }
        if (rate == 0) {
            clock->cycle_index = (clock->cycle_index + 1u) % clock->cycle_count;
        }
    }
    clock->rate_index = rate;
    return 0;
}

static inline int TrmAcmPpsIsLastFrame(const TrmAcmPpsClock* clock, uint8_t rate)
{
    return clock->anchored && !clock->invalid && clock->cycle_count &&
        clock->cycle_index == clock->cycle_count - 1u &&
        rate == clock->rate_index && rate + 1u == clock->rate_count;
}
#endif
