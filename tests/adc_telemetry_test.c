#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "adc_telemetry.h"

int main(void)
{
    assert(AdcTelemetry_RawToPinMv(0U) == 0U);
    assert(AdcTelemetry_RawToPinMv(4095U) == 4999U);
    assert(AdcTelemetry_RawToDividedSupplyMv(0U) == 0U);
    assert(AdcTelemetry_RawToDividedSupplyMv(1352U) == 3300U);
    assert(AdcTelemetry_RawToDividedSupplyMv(4095U) == 5000U);
    assert(AdcTelemetry_RawToThermistorC(0U) == 125);
    assert(AdcTelemetry_RawToThermistorC(2703U) == -55);
    assert(AdcTelemetry_RawToThermistorC(1352U) == 25);

    puts("adc_telemetry_test: PASS");
    return 0;
}
