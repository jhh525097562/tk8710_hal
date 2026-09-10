#ifndef ADC_TELEMETRY_H
#define ADC_TELEMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int16_t basebandTempC;
    int16_t rfTempC;
    uint16_t rf3v3Mv;
    uint16_t rf1v2Mv;
} AdcTelemetryValues;

void AdcTelemetry_Init(void);
int AdcTelemetry_Read(AdcTelemetryValues *values);

#ifdef ADC_TELEMETRY_HOST_TEST
uint16_t AdcTelemetry_RawToPinMv(uint16_t raw);
uint16_t AdcTelemetry_RawToDividedSupplyMv(uint16_t raw);
int16_t AdcTelemetry_RawToThermistorC(uint16_t raw);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ADC_TELEMETRY_H */
