#include "adc_telemetry.h"

#include <math.h>
#include <stddef.h>

#ifndef ADC_TELEMETRY_HOST_TEST
#include "adc.h"
#include "trm/trm_log.h"
#endif

#ifdef ADC_TELEMETRY_HOST_TEST
#define ADC_TELEMETRY_PRIVATE
#else
#define ADC_TELEMETRY_PRIVATE static
#endif

#define ADC_TELEMETRY_CHANNEL_COUNT 4U
#define ADC_TELEMETRY_NORMAL_CHANNEL_COUNT 4U
#define ADC_TELEMETRY_CHANNEL_MASK 0x0000000FU
#define ADC_TELEMETRY_CHANNEL_ID_ENABLE 0x00000010U
#define ADC_TELEMETRY_GROUP1_FIFO_SIZE 16U
#define ADC_TELEMETRY_SAMPLE_WINDOW 64U
#define ADC_TELEMETRY_ADC_LEVEL_COUNT 4096U
#define ADC_TELEMETRY_REFERENCE_MV 5000U
#define ADC_TELEMETRY_THERMISTOR_SUPPLY_MV 3300U
#define ADC_TELEMETRY_SUPPLY_MAX_MV 5000U
#define ADC_TELEMETRY_THERMISTOR_PULLUP_OHM 10000.0
#define ADC_TELEMETRY_THERMISTOR_R0_OHM 10000.0
#define ADC_TELEMETRY_THERMISTOR_BETA 4100.0
#define ADC_TELEMETRY_THERMISTOR_T0_K 298.15
#define ADC_TELEMETRY_TEMP_MIN_C (-55)
#define ADC_TELEMETRY_TEMP_MAX_C 125
#define ADC_TELEMETRY_TIMEOUT_LOOPS 100000U
#define ADC_TELEMETRY_LOG_LIMIT 8U

#ifndef ADC_TELEMETRY_HOST_TEST
static uint32_t g_adcTelemetryLogCount;
#endif

ADC_TELEMETRY_PRIVATE uint16_t AdcTelemetry_RawToPinMv(uint16_t raw)
{
    uint32_t value = ((uint32_t)raw * ADC_TELEMETRY_REFERENCE_MV) +
                     (ADC_TELEMETRY_ADC_LEVEL_COUNT / 2U);
    return (uint16_t)(value / ADC_TELEMETRY_ADC_LEVEL_COUNT);
}

ADC_TELEMETRY_PRIVATE uint16_t AdcTelemetry_RawToDividedSupplyMv(uint16_t raw)
{
    uint32_t supplyMv = (uint32_t)AdcTelemetry_RawToPinMv(raw) * 2U;

    if (supplyMv > ADC_TELEMETRY_SUPPLY_MAX_MV)
    {
        supplyMv = ADC_TELEMETRY_SUPPLY_MAX_MV;
    }
    return (uint16_t)supplyMv;
}

ADC_TELEMETRY_PRIVATE int16_t AdcTelemetry_RawToThermistorC(uint16_t raw)
{
    double pinMv;
    double thermistorOhm;
    double temperatureK;
    double temperatureC;

    if (raw == 0U)
    {
        return ADC_TELEMETRY_TEMP_MAX_C;
    }

    pinMv = (double)raw * (double)ADC_TELEMETRY_REFERENCE_MV /
            (double)ADC_TELEMETRY_ADC_LEVEL_COUNT;
    if (pinMv >= (double)ADC_TELEMETRY_THERMISTOR_SUPPLY_MV)
    {
        return ADC_TELEMETRY_TEMP_MIN_C;
    }
    thermistorOhm = (ADC_TELEMETRY_THERMISTOR_PULLUP_OHM * pinMv) /
                    ((double)ADC_TELEMETRY_THERMISTOR_SUPPLY_MV - pinMv);
    temperatureK = 1.0 /
                   ((1.0 / ADC_TELEMETRY_THERMISTOR_T0_K) +
                    (log(thermistorOhm / ADC_TELEMETRY_THERMISTOR_R0_OHM) /
                     ADC_TELEMETRY_THERMISTOR_BETA));
    temperatureC = temperatureK - 273.15;
    if (temperatureC >= 0.0)
    {
        temperatureC += 0.5;
    }
    else
    {
        temperatureC -= 0.5;
    }
    if (temperatureC > (double)ADC_TELEMETRY_TEMP_MAX_C)
    {
        return ADC_TELEMETRY_TEMP_MAX_C;
    }
    if (temperatureC < (double)ADC_TELEMETRY_TEMP_MIN_C)
    {
        return ADC_TELEMETRY_TEMP_MIN_C;
    }
    return (int16_t)temperatureC;
}

#ifndef ADC_TELEMETRY_HOST_TEST
static int AdcTelemetry_ShouldLog(void)
{
    if (g_adcTelemetryLogCount < ADC_TELEMETRY_LOG_LIMIT)
    {
        g_adcTelemetryLogCount++;
        return 1;
    }
    return 0;
}

static void AdcTelemetry_LogReadFailure(const char *reason,
                                        uint32_t count,
                                        uint8_t seenMask,
                                        const uint16_t *raw)
{
    if (AdcTelemetry_ShouldLog() == 0)
    {
        return;
    }

    TRM_LOG_WARN("ADC telemetry %s: count=%lu seen=0x%02X raw=[%u,%u,%u,%u] G1SR=0x%08lX INTFLG=0x%08lX INTCR=0x%08lX SEL=0x%08lX",
                 reason,
                 (unsigned long)count,
                 (unsigned int)seenMask,
                 (unsigned int)raw[0],
                 (unsigned int)raw[1],
                 (unsigned int)raw[2],
                 (unsigned int)raw[3],
                 (unsigned long)adcREG1->G1SR,
                 (unsigned long)adcREG1->GxINTFLG[adcGROUP1],
                 (unsigned long)adcREG1->GxINTCR[adcGROUP1],
                 (unsigned long)adcREG1->GxSEL[adcGROUP1]);
}

void AdcTelemetry_Init(void)
{
    adcInit();
    adcREG1->G1SAMP = ADC_TELEMETRY_SAMPLE_WINDOW;
}

int AdcTelemetry_Read(AdcTelemetryValues *values)
{
    adcData_t samples[ADC_TELEMETRY_GROUP1_FIFO_SIZE];
    uint16_t raw[ADC_TELEMETRY_CHANNEL_COUNT] = {0U, 0U, 0U, 0U};
    uint8_t seenMask = 0U;
    uint32_t count;
    uint32_t i;
    uint32_t guard = ADC_TELEMETRY_TIMEOUT_LOOPS;

    if (values == NULL)
    {
        return -1;
    }

    adcStopConversion(adcREG1, adcGROUP1);
    adcResetFiFo(adcREG1, adcGROUP1);
    adcREG1->G1SAMP = ADC_TELEMETRY_SAMPLE_WINDOW;
    adcREG1->GxMODECR[adcGROUP1] =
        (adcREG1->GxMODECR[adcGROUP1] & ~ADC_TELEMETRY_CHANNEL_ID_ENABLE) |
        ADC_TELEMETRY_CHANNEL_ID_ENABLE;
    adcREG1->GxINTCR[adcGROUP1] = ADC_TELEMETRY_GROUP1_FIFO_SIZE;
    adcREG1->GxINTFLG[adcGROUP1] = 9U;
    adcREG1->GxSEL[adcGROUP1] = ADC_TELEMETRY_CHANNEL_MASK;
    while ((adcIsConversionComplete(adcREG1, adcGROUP1) == 0U) && (guard > 0U))
    {
        guard--;
    }
    if (guard == 0U)
    {
        AdcTelemetry_LogReadFailure("timeout", 0U, seenMask, raw);
        adcStopConversion(adcREG1, adcGROUP1);
        return -1;
    }

    count = adcGetData(adcREG1, adcGROUP1, samples);
    adcStopConversion(adcREG1, adcGROUP1);
    if (count < ADC_TELEMETRY_NORMAL_CHANNEL_COUNT)
    {
        AdcTelemetry_LogReadFailure("short", count, seenMask, raw);
        return -1;
    }

    for (i = 0U; i < count; i++)
    {
        uint32_t channel = samples[i].id;
        if (channel < ADC_TELEMETRY_CHANNEL_COUNT)
        {
            raw[channel] = samples[i].value;
            seenMask = (uint8_t)(seenMask | (uint8_t)(1U << channel));
        }
    }
    if (seenMask != ADC_TELEMETRY_CHANNEL_MASK)
    {
        AdcTelemetry_LogReadFailure("channel", count, seenMask, raw);
        return -1;
    }

    values->basebandTempC = AdcTelemetry_RawToThermistorC(raw[0]);
    values->rfTempC = AdcTelemetry_RawToThermistorC(raw[1]);
    values->rf3v3Mv = AdcTelemetry_RawToDividedSupplyMv(raw[2]);
    values->rf1v2Mv = AdcTelemetry_RawToDividedSupplyMv(raw[3]);
    return 0;
}
#endif
