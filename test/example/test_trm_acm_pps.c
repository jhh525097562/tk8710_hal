#include "../../src/trm/trm_acm_pps.h"
#include <assert.h>
#include <stdio.h>

static void run_cycles(uint32_t cycles, uint8_t rates)
{
    TrmAcmPpsClock clock = {.cycle_count = cycles, .rate_count = rates};
    assert(!TrmAcmPpsIsLastFrame(&clock, 0));
    /* Includes the cycle after a virtual S3: no extra S0 may be counted. */
    for (unsigned int repeat = 0; repeat < 3; ++repeat) {
        for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
            for (uint8_t rate = 0; rate < rates; ++rate) {
                assert(TrmAcmPpsObserveS0(&clock, rate) == 0);
                assert(clock.cycle_index == cycle);
                assert(!!TrmAcmPpsIsLastFrame(&clock, rate) ==
                       (cycle + 1 == cycles && rate + 1 == rates));
            }
        }
    }
}

int main(void)
{
    TrmAcmPpsClock invalid = {.cycle_count = 25, .rate_count = 4};
    run_cycles(25, 1); /* 7 s / 280 ms, tdd_num=1 */
    run_cycles(40, 1); /* tdd_num=20, PPS every two business superframes */
    run_cycles(25, 4);
    run_cycles(1, 1);
    run_cycles(1, 3);
    assert(TrmAcmPpsHasTime(100, 92280, 29400));
    assert(!TrmAcmPpsHasTime(100, 29500, 29400));
    assert(!TrmAcmPpsHasTime(92280, 92280, 0));
    assert(!TrmAcmPpsHasTime(92281, 92280, 1000));
    assert(TrmAcmPpsHasTime(UINT64_MAX - 2000, UINT64_MAX, 1000));
    assert(TrmAcmPpsObserveS0(&invalid, 1) == -1);
    assert(!TrmAcmPpsIsLastFrame(&invalid, 3));
    invalid = (TrmAcmPpsClock){.cycle_count = 2, .rate_count = 2};
    assert(TrmAcmPpsObserveS0(&invalid, 0) == 0);
    assert(TrmAcmPpsObserveS0(&invalid, 0) == -1);
    invalid = (TrmAcmPpsClock){0};
    assert(TrmAcmPpsObserveS0(&invalid, 0) == -1);
    puts("ACM PPS scheduling tests passed");
    return 0;
}
