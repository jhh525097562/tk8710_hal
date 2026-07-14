#include <string.h>
#include "hal_internal.h"

static TK8710HalTrmCfg g_halTrmCfg;

void TK8710HalCbStore(const TK8710HalTrmCfg* cfg)
{
    memset(&g_halTrmCfg, 0, sizeof(g_halTrmCfg));
    if (cfg != NULL) {
        g_halTrmCfg = *cfg;
    }
}

void TK8710HalCbBuildTrmConfig(TRM_InitConfig* trmConfig, const TK8710HalTrmCfg* cfg)
{
    if (trmConfig == NULL) {
        return;
    }

    memset(trmConfig, 0, sizeof(*trmConfig));
    if (cfg == NULL) {
        return;
    }

    trmConfig->beamMaxUsers = cfg->beamMaxUsers;
    trmConfig->beamTimeoutMs = cfg->beamTimeoutMs;
    trmConfig->maxFrameCount = cfg->maxFrameCount;
    trmConfig->callbacks.onRxData = cfg->onRxData;
    trmConfig->callbacks.onTxComplete = cfg->onTxComplete;
}
