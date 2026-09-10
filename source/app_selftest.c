#include "app_selftest.h"

#include "app_faults.h"
#include "fpga_param_store.h"

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "adc.h"
#include "reg_adc.h"
#include "reg_esm.h"
#endif

#if defined(FPGA_PROTOCOL_HOST_TEST)
static uint8_t g_testPeripheralFault;
static uint8_t g_testEccFault;
static uint8_t g_testAdcFault[4U];
#endif

static void AppSelfTest_CheckParams(void)
{
    if (FpgaParamStore_ParamsAreCorrupt() != 0)
    {
        AppFaults_Set(APP_FAULT_PARAM_CHECKSUM);
    }
}

static void AppSelfTest_CheckEcc(void)
{
#if defined(FPGA_PROTOCOL_HOST_TEST)
    if (g_testEccFault != 0U)
    {
        AppFaults_Set(APP_FAULT_ECC_WARNING);
    }
#else
    if (((esmREG->SR1[0] | esmREG->SR1[1] | esmREG->SR1[2]) != 0U) ||
        ((esmREG->SR4[0] | esmREG->SR4[1] | esmREG->SR4[2]) != 0U) ||
        (esmREG->SSR2 != 0U))
    {
        AppFaults_Set(APP_FAULT_ECC_WARNING);
    }
#endif
}

static void AppSelfTest_CheckAdc(void)
{
#if defined(FPGA_PROTOCOL_HOST_TEST)
    uint32_t i;
    for (i = 0U; i < 4U; i++)
    {
        if (g_testAdcFault[i] != 0U)
        {
            AppFaults_Set(APP_FAULT_ADC1 + i);
        }
    }
#else
    if ((adcREG1->G1SR & 0x7U) != 0U)
    {
        AppFaults_Set(APP_FAULT_ADC1);
    }
    if ((adcREG1->G2SR & 0x7U) != 0U)
    {
        AppFaults_Set(APP_FAULT_ADC2);
    }
    if ((adcREG2->G1SR & 0x7U) != 0U)
    {
        AppFaults_Set(APP_FAULT_ADC3);
    }
    if ((adcREG2->G2SR & 0x7U) != 0U)
    {
        AppFaults_Set(APP_FAULT_ADC4);
    }
#endif
}

uint32_t AppSelfTest_Run(void)
{
#if defined(FPGA_PROTOCOL_HOST_TEST)
    if (g_testPeripheralFault != 0U)
    {
        AppFaults_Set(APP_FAULT_SELFTEST_PERIPHERAL);
    }
#endif
    AppSelfTest_CheckParams();
    AppSelfTest_CheckEcc();
    AppSelfTest_CheckAdc();
    return AppFaults_GetBitmap();
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
void AppSelfTest_TestInjectPeripheralFault(uint8_t enabled)
{
    g_testPeripheralFault = enabled;
}

void AppSelfTest_TestInjectEccFault(uint8_t enabled)
{
    g_testEccFault = enabled;
}

void AppSelfTest_TestInjectAdcFault(uint32_t adcIndex, uint8_t enabled)
{
    if (adcIndex < 4U)
    {
        g_testAdcFault[adcIndex] = enabled;
    }
}
#endif
