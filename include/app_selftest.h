#ifndef APP_SELFTEST_H
#define APP_SELFTEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t AppSelfTest_Run(void);

#if defined(FPGA_PROTOCOL_HOST_TEST)
void AppSelfTest_TestInjectPeripheralFault(uint8_t enabled);
void AppSelfTest_TestInjectEccFault(uint8_t enabled);
void AppSelfTest_TestInjectAdcFault(uint32_t adcIndex, uint8_t enabled);
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_SELFTEST_H */
