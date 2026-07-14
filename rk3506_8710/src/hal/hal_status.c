#include "hal_internal.h"

static TK8710HalState g_halState = TK8710_HAL_STATE_UNINIT;

void TK8710HalStatusSetState(TK8710HalState state)
{
    g_halState = state;
}

TK8710HalState TK8710HalStatusGetState(void)
{
    return g_halState;
}

int TK8710HalStatusRequireInit(void)
{
    return g_halState == TK8710_HAL_STATE_UNINIT ? 0 : 1;
}

int TK8710HalStatusRequireConfigurable(void)
{
    return g_halState == TK8710_HAL_STATE_UNINIT ? 0 : 1;
}
