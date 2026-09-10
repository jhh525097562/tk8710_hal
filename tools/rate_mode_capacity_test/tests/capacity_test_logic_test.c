#include "capacity_test_logic.h"

#include <assert.h>
#include <stdio.h>

static void record_full_frame(CapacityTestStats* stats)
{
    uint32_t user;

    CapacityTest_BeginRxFrame(stats);
    for (user = 0U; user < stats->expected_users; user++) {
        CapacityTest_RecordRxUser(stats, (uint8_t)user, 1U, 1U);
    }
    CapacityTest_EndRxFrame(stats);
    CapacityTest_RecordTxSubmitted(stats, stats->expected_users);
    CapacityTest_RecordTxComplete(stats, stats->expected_users);
}

int main(void)
{
    CapacityTestStats stats;
    uint32_t frame;

    assert(CapacityTest_MapProtocolRate(0U) == 6U);
    assert(CapacityTest_MapProtocolRate(1U) == 7U);
    assert(CapacityTest_MapProtocolRate(2U) == 8U);
    assert(CapacityTest_MapProtocolRate(3U) == 0xFFU);

    CapacityTest_Init(&stats, 2U, 128U, 10U);
    for (frame = 0U; frame < 10U; frame++) {
        record_full_frame(&stats);
    }
    assert(CapacityTest_Passed(&stats) != 0);

    CapacityTest_Init(&stats, 0U, 16U, 1U);
    CapacityTest_BeginRxFrame(&stats);
    CapacityTest_RecordRxUser(&stats, 0U, 1U, 1U);
    CapacityTest_RecordRxUser(&stats, 0U, 1U, 1U);
    CapacityTest_RecordRxUser(&stats, 16U, 1U, 1U);
    CapacityTest_EndRxFrame(&stats);
    CapacityTest_RecordTxSubmitted(&stats, 16U);
    CapacityTest_RecordTxComplete(&stats, 15U);
    assert(stats.rx_duplicates == 1U);
    assert(stats.rx_out_of_range == 1U);
    assert(stats.rx_missing == 15U);
    assert(stats.tx_missing == 1U);
    assert(CapacityTest_Passed(&stats) == 0);

    CapacityTest_Init(&stats, 1U, 16U, 1U);
    record_full_frame(&stats);
    CapacityTest_RecordDriverError(&stats);
    assert(CapacityTest_Passed(&stats) == 0);

    CapacityTest_Init(&stats, 1U, 16U, 1U);
    record_full_frame(&stats);
    CapacityTest_RecordTimeout(&stats);
    assert(CapacityTest_Passed(&stats) == 0);

    puts("capacity_test_logic_test: PASS");
    return 0;
}
