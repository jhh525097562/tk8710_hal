#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "app_faults.h"
#include "fpga_protocol.h"

static uint8_t checksum(const uint8_t *frame)
{
    uint32_t i;
    uint8_t sum = 0U;

    for (i = 2U; i <= 142U; i++)
    {
        sum = (uint8_t)(sum + frame[i]);
    }
    return sum;
}

static void test_v05_telemetry_fields(void)
{
    uint8_t frame[FPGA_PROTOCOL_TM_FRAME_LEN];

    FpgaProtocol_TestReset();
    AppFaults_Set(APP_FAULT_ADC3);
    FpgaProtocol_TestBuildTelemetry(frame);

    assert(frame[0] == 0xEBU);
    assert(frame[1] == 0x90U);
    assert(frame[18] == 0U && frame[19] == 17U);
    assert(frame[49] == 0U && frame[50] == 0U);
    assert(frame[51] == 0x08U && frame[52] == 0U);
    assert(frame[53] == 0xD6U);
    assert(frame[54] == 0x25U);
    assert(frame[55] == 0x34U && frame[56] == 0x56U);
    assert(frame[57] == 0xA5U);
    assert(frame[58] == 0U && frame[59] == 0U && frame[60] == 0U);
    assert(frame[135] == 0U && frame[136] == 25U);
    assert(frame[137] == 0U && frame[138] == 30U);
    assert(frame[139] == 0x0CU && frame[140] == 0xE4U);
    assert(frame[141] == 0x04U && frame[142] == 0xB0U);
    assert(frame[143] == checksum(frame));
}

static void test_dc_antenna_is_one_based(void)
{
    uint8_t frame[FPGA_PROTOCOL_RC_FRAME_LEN] =
        {0x76U, 0x25U, 0x0EU, 1U, 0U, 1U, 0U, 2U, 0U, 0U};
    uint32_t errors = 0U;
    uint8_t antenna = 0xFFU;
    int16_t iDc = 0;
    int16_t qDc = 0;

    frame[9] = (uint8_t)(frame[2] + frame[3] + frame[4] + frame[5] +
                         frame[6] + frame[7] + frame[8]);
    assert(FpgaProtocol_TestHandleRxFrame(frame, &errors) == 0);
    assert(errors == 0U);
    assert(FpgaProtocol_TestGetAppliedDc(0U, &antenna, &iDc, &qDc) == 0);
    assert(antenna == 0U);
    assert(iDc == 1 && qDc == 2);
}

static void test_reserved_write_register_is_rejected(void)
{
    uint8_t frame[FPGA_PROTOCOL_RC_FRAME_LEN] =
        {0x76U, 0x25U, 0x0AU, 0U, 1U, 0U, 0U, 0U, 2U, 0U};
    uint32_t errors = 0U;

    frame[9] = (uint8_t)(frame[2] + frame[3] + frame[4] + frame[5] +
                         frame[6] + frame[7] + frame[8]);
    assert(FpgaProtocol_TestHandleRxFrame(frame, &errors) < 0);
    assert(errors == 1U);
}

int main(void)
{
    test_v05_telemetry_fields();
    test_dc_antenna_is_one_based();
    test_reserved_write_register_is_rejected();
    puts("fpga_protocol_v05_test: PASS");
    return 0;
}
