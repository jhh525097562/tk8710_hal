#include "sys_common.h"

/* Application reset vector, placed at 0x00020020 by sys_link.cmd. */
extern void resetEntry(void);

#pragma DATA_SECTION(g_appStatus, ".app_status")
#pragma RETAIN(g_appStatus)
const uint32_t g_appStatus[8] =
{
    0x5A5A5A5AU,
    (uint32_t)&resetEntry,
    0x00000000U,
    0x20260729U,
    0x00000000U,
    0x00000000U,
    0x00000000U,
    0x00000000U
};
