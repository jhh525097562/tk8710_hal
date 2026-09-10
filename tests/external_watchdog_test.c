#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "external_watchdog.h"

int main(void)
{
    ExternalWatchdog_TestReset();
    ExternalWatchdog_Init();
    assert((ExternalWatchdog_TestGetDirectionMask() &
            (1UL << EXTERNAL_WATCHDOG_WDIA_BIT)) != 0U);
    assert(ExternalWatchdog_TestGetOutputLevel() == 0U);

    ExternalWatchdog_Process(0U);
    assert(ExternalWatchdog_TestGetFeedCount() == 1U);
    assert(ExternalWatchdog_TestGetOutputLevel() == 1U);

    ExternalWatchdog_Process(1U);
    assert(ExternalWatchdog_TestGetFeedCount() == 2U);
    assert(ExternalWatchdog_TestGetOutputLevel() == 0U);

    puts("external_watchdog_test: PASS");
    return 0;
}
