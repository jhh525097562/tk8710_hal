#ifndef TK8710_TIME_UTILS_H
#define TK8710_TIME_UTILS_H

#include <stdint.h>

/* Widen seconds before multiplication, including on 32-bit ARM. */
static inline uint64_t TK8710SecondsToUs(uint64_t seconds, uint32_t microseconds)
{
    return seconds * 1000000ULL + microseconds;
}

/* Reject failed reads (zero) or a backwards time source before subtraction. */
static inline int TK8710TimeIntervalElapsed(uint64_t now, uint64_t previous,
    uint64_t interval)
{
    return now != 0 && now >= previous && now - previous >= interval;
}

#endif
