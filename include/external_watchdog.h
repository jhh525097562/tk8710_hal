#ifndef EXTERNAL_WATCHDOG_H
#define EXTERNAL_WATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXTERNAL_WATCHDOG_WDIA_BIT 0U

void ExternalWatchdog_Init(void);
void ExternalWatchdog_Process(uint32_t nowMs);

#if defined(FPGA_PROTOCOL_HOST_TEST)
void ExternalWatchdog_TestReset(void);
uint32_t ExternalWatchdog_TestGetFeedCount(void);
uint32_t ExternalWatchdog_TestGetDirectionMask(void);
uint8_t ExternalWatchdog_TestGetOutputLevel(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXTERNAL_WATCHDOG_H */
