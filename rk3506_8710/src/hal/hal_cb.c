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
    trmConfig->nodeRole = cfg->nodeRole;
    trmConfig->localAddr = cfg->localAddr;
    trmConfig->groundStationBeamMax = cfg->groundStationBeamMax;
    trmConfig->groundStationBeamTimeoutMs = cfg->groundStationBeamTimeoutMs;
    trmConfig->satelliteUplinkCacheSize = cfg->satelliteUplinkCacheSize;
    trmConfig->groundStationTerminalBeamMax = cfg->groundStationTerminalBeamMax;
    trmConfig->groundStationJoinMaintainMs = cfg->groundStationJoinMaintainMs;
    trmConfig->groundStationBcnThreshold = cfg->groundStationBcnThreshold;
    trmConfig->groundStationJoinResponseStatus = cfg->groundStationJoinResponseStatus;
    trmConfig->groundStationJoinResponseStatusSet = cfg->groundStationJoinResponseStatusSet;
    trmConfig->callbacks.onRxData = cfg->onRxData;
    trmConfig->callbacks.onTxComplete = cfg->onTxComplete;
}
