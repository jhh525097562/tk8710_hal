#include "app_faults.h"

static uint32_t g_appFaultBitmap;

void AppFaults_Init(void)
{
    g_appFaultBitmap = 0U;
}

void AppFaults_Set(uint32_t bit)
{
    if (bit < 32U)
    {
        g_appFaultBitmap |= (uint32_t)(1UL << bit);
    }
}

uint32_t AppFaults_GetBitmap(void)
{
    return g_appFaultBitmap;
}
