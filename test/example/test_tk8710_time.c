#include "../../port/tk8710_time_utils.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const uint64_t hour = 3600000000ULL;
    /* Both signed and unsigned 32-bit microsecond wrap boundaries. */
    assert(TK8710SecondsToUs(2147, 483647) == 2147483647ULL);
    assert(TK8710SecondsToUs(2147, 483648) == 2147483648ULL);
    assert(TK8710SecondsToUs(4294, 967295) == 4294967295ULL);
    assert(TK8710SecondsToUs(4294, 967296) == 4294967296ULL);
    assert(TK8710SecondsToUs(1790000000, 123456) == 1790000000123456ULL);
    uint64_t last = 1000000ULL;
    unsigned int requests = 0;
    /* Six hours of simulated monotonic time includes several former wrap points.
     * Wall-clock values never enter the interval predicate. */
    for (uint64_t now = last; now <= 1000000ULL + 6 * hour; now += 1000000ULL) {
        if (TK8710TimeIntervalElapsed(now, last, hour)) {
            assert(now - last == hour);
            last = now;
            ++requests;
        }
    }
    assert(requests == 6);
    assert(!TK8710TimeIntervalElapsed(last + hour - 1, last, hour));
    assert(TK8710TimeIntervalElapsed(last + hour, last, hour));
    assert(!TK8710TimeIntervalElapsed(last + 694967296ULL, last, hour));
    assert(!TK8710TimeIntervalElapsed(last - 1, last, hour));
    assert(!TK8710TimeIntervalElapsed(0, last, hour));
    puts("64-bit clock conversion and ACM interval tests passed");
    return 0;
}
