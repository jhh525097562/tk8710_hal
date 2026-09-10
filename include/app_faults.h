#ifndef APP_FAULTS_H
#define APP_FAULTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define APP_FAULT_SELFTEST_PERIPHERAL 0U
#define APP_FAULT_PARAM_CHECKSUM 1U
#define APP_FAULT_ECC_WARNING 2U
#define APP_FAULT_TK8710_COMM 3U
#define APP_FAULT_TK8710_WORK_MODE 4U
#define APP_FAULT_TK8710_PARAMS 5U
#define APP_FAULT_TK8710_SLOT 6U
#define APP_FAULT_TK8710_TX_POWER 7U
#define APP_FAULT_TK8710_DC 8U
#define APP_FAULT_ADC1 9U
#define APP_FAULT_ADC2 10U
#define APP_FAULT_ADC3 11U
#define APP_FAULT_ADC4 12U
#define APP_FAULT_BOOT_FLAG_VERIFY 13U

void AppFaults_Init(void);
void AppFaults_Set(uint32_t bit);
uint32_t AppFaults_GetBitmap(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULTS_H */
