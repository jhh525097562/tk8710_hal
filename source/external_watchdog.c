#include "external_watchdog.h"

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "gio.h"
#endif

#define EXTERNAL_WATCHDOG_WDIA_MASK (1UL << EXTERNAL_WATCHDOG_WDIA_BIT)

static uint8_t g_externalWatchdogLevel;
static uint8_t g_externalWatchdogInitialized;

#if defined(FPGA_PROTOCOL_HOST_TEST)
static uint32_t g_externalWatchdogFeedCount;
static uint32_t g_externalWatchdogDirectionMask;
#endif

static void ExternalWatchdogDrive(uint8_t level)
{
#if defined(FPGA_PROTOCOL_HOST_TEST)
    g_externalWatchdogLevel = level;
#else
    gioSetBit(gioPORTA, EXTERNAL_WATCHDOG_WDIA_BIT, level);
#endif
}

void ExternalWatchdog_Init(void)
{
    g_externalWatchdogLevel = 0U;
    g_externalWatchdogInitialized = 1U;
#if defined(FPGA_PROTOCOL_HOST_TEST)
    g_externalWatchdogDirectionMask |= EXTERNAL_WATCHDOG_WDIA_MASK;
#else
    gioSetDirection(gioPORTA, gioPORTA->DIR | EXTERNAL_WATCHDOG_WDIA_MASK);
#endif
    ExternalWatchdogDrive(g_externalWatchdogLevel);
}

void ExternalWatchdog_Process(uint32_t nowMs)
{
    (void)nowMs;

    if (g_externalWatchdogInitialized == 0U)
    {
        ExternalWatchdog_Init();
    }

    g_externalWatchdogLevel = (g_externalWatchdogLevel == 0U) ? 1U : 0U;
    ExternalWatchdogDrive(g_externalWatchdogLevel);
#if defined(FPGA_PROTOCOL_HOST_TEST)
    g_externalWatchdogFeedCount++;
#endif
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
void ExternalWatchdog_TestReset(void)
{
    g_externalWatchdogLevel = 0U;
    g_externalWatchdogInitialized = 0U;
    g_externalWatchdogFeedCount = 0U;
    g_externalWatchdogDirectionMask = 0U;
}

uint32_t ExternalWatchdog_TestGetFeedCount(void)
{
    return g_externalWatchdogFeedCount;
}

uint32_t ExternalWatchdog_TestGetDirectionMask(void)
{
    return g_externalWatchdogDirectionMask;
}

uint8_t ExternalWatchdog_TestGetOutputLevel(void)
{
    return g_externalWatchdogLevel;
}
#endif
