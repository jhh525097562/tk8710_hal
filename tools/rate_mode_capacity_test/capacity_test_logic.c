#include "capacity_test_logic.h"

#include <string.h>

#define CAPACITY_TEST_TK_RATE_BASE 6U

uint8_t CapacityTest_MapProtocolRate(uint8_t protocol_rate)
{
    return (protocol_rate <= 2U) ?
           (uint8_t)(protocol_rate + CAPACITY_TEST_TK_RATE_BASE) : 0xFFU;
}

void CapacityTest_Init(CapacityTestStats* stats, uint8_t protocol_rate,
                       uint8_t expected_users, uint32_t expected_frames)
{
    if (stats == NULL) {
        return;
    }
    (void)memset(stats, 0, sizeof(*stats));
    stats->protocol_rate = protocol_rate;
    stats->driver_rate = CapacityTest_MapProtocolRate(protocol_rate);
    stats->expected_users = expected_users;
    stats->expected_frames = expected_frames;
}

void CapacityTest_BeginRxFrame(CapacityTestStats* stats)
{
    if (stats != NULL) {
        (void)memset(stats->frame_seen, 0, sizeof(stats->frame_seen));
    }
}

void CapacityTest_RecordRxUser(CapacityTestStats* stats, uint8_t user_index,
                               uint8_t crc_valid, uint8_t payload_valid)
{
    if (stats == NULL) {
        return;
    }
    if (user_index >= stats->expected_users) {
        stats->rx_out_of_range++;
        return;
    }
    if (stats->frame_seen[user_index] != 0U) {
        stats->rx_duplicates++;
        return;
    }
    stats->frame_seen[user_index] = 1U;
    if (crc_valid == 0U) {
        stats->rx_crc_errors++;
        return;
    }
    if (payload_valid == 0U) {
        stats->rx_payload_errors++;
        return;
    }
    stats->rx_valid_total++;
}

void CapacityTest_EndRxFrame(CapacityTestStats* stats)
{
    uint32_t user;

    if (stats == NULL) {
        return;
    }
    for (user = 0U; user < stats->expected_users; user++) {
        if (stats->frame_seen[user] == 0U) {
            stats->rx_missing++;
        }
    }
    stats->completed_frames++;
}

void CapacityTest_RecordTxSubmitted(CapacityTestStats* stats, uint32_t count)
{
    if (stats != NULL) {
        stats->tx_submitted_total += count;
    }
}

void CapacityTest_RecordTxComplete(CapacityTestStats* stats, uint32_t count)
{
    if (stats == NULL) {
        return;
    }
    stats->tx_completed_total += count;
    if (count < stats->expected_users) {
        stats->tx_missing += (uint32_t)stats->expected_users - count;
    } else if (count > stats->expected_users) {
        stats->tx_failed += count - (uint32_t)stats->expected_users;
    }
}

void CapacityTest_RecordDriverError(CapacityTestStats* stats)
{
    if (stats != NULL) {
        stats->driver_errors++;
    }
}

void CapacityTest_RecordTimeout(CapacityTestStats* stats)
{
    if (stats != NULL) {
        stats->timeouts++;
    }
}

int CapacityTest_Passed(const CapacityTestStats* stats)
{
    uint32_t expected_total;

    if ((stats == NULL) || (stats->driver_rate == 0xFFU) ||
        (stats->expected_users == 0U) ||
        (stats->expected_users > CAPACITY_TEST_MAX_USERS) ||
        (stats->completed_frames != stats->expected_frames)) {
        return 0;
    }
    expected_total = (uint32_t)stats->expected_users * stats->expected_frames;
    return (stats->rx_valid_total == expected_total) &&
           (stats->tx_submitted_total == expected_total) &&
           (stats->tx_completed_total == expected_total) &&
           (stats->rx_crc_errors == 0U) && (stats->rx_missing == 0U) &&
           (stats->rx_duplicates == 0U) &&
           (stats->rx_out_of_range == 0U) &&
           (stats->rx_payload_errors == 0U) &&
           (stats->tx_failed == 0U) && (stats->tx_missing == 0U) &&
           (stats->driver_errors == 0U) && (stats->timeouts == 0U);
}
