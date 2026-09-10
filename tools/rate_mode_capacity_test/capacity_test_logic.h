#ifndef CAPACITY_TEST_LOGIC_H
#define CAPACITY_TEST_LOGIC_H

#include <stdint.h>

#define CAPACITY_TEST_MAX_USERS 128U

typedef struct {
    uint8_t protocol_rate;
    uint8_t driver_rate;
    uint8_t expected_users;
    uint32_t expected_frames;
    uint32_t completed_frames;
    uint32_t rx_valid_total;
    uint32_t rx_crc_errors;
    uint32_t rx_missing;
    uint32_t rx_duplicates;
    uint32_t rx_out_of_range;
    uint32_t rx_payload_errors;
    uint32_t tx_submitted_total;
    uint32_t tx_completed_total;
    uint32_t tx_failed;
    uint32_t tx_missing;
    uint32_t driver_errors;
    uint32_t timeouts;
    uint8_t frame_seen[CAPACITY_TEST_MAX_USERS];
} CapacityTestStats;

uint8_t CapacityTest_MapProtocolRate(uint8_t protocol_rate);
void CapacityTest_Init(CapacityTestStats* stats, uint8_t protocol_rate,
                       uint8_t expected_users, uint32_t expected_frames);
void CapacityTest_BeginRxFrame(CapacityTestStats* stats);
void CapacityTest_RecordRxUser(CapacityTestStats* stats, uint8_t user_index,
                               uint8_t crc_valid, uint8_t payload_valid);
void CapacityTest_EndRxFrame(CapacityTestStats* stats);
void CapacityTest_RecordTxSubmitted(CapacityTestStats* stats, uint32_t count);
void CapacityTest_RecordTxComplete(CapacityTestStats* stats, uint32_t count);
void CapacityTest_RecordDriverError(CapacityTestStats* stats);
void CapacityTest_RecordTimeout(CapacityTestStats* stats);
int CapacityTest_Passed(const CapacityTestStats* stats);

#endif
