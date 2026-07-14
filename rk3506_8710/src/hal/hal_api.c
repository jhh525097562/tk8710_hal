#include <string.h>
#include "hal_internal.h"
#include "phy/phy_api.h"
#include "phy/phy_log.h"
#include "trm/phy_cfg.h"
#include "trm/phy_data.h"
#include "trm/phy_stat.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_internal.h"
#include "trm/trm_log.h"

TK8710HalError TK8710HalInit(const TK8710HalInitCfg* cfg)
{
    TRM_InitConfig trmConfig;
    const ChipConfig* chipConfig = NULL;

    TK8710HalCbStore(cfg != NULL ? &cfg->trmCfg : NULL);
    TK8710HalCbBuildTrmConfig(&trmConfig, cfg != NULL ? &cfg->trmCfg : NULL);
    if (cfg != NULL) {
        chipConfig = (const ChipConfig*)cfg->chipInitCfg;
    }

    TK8710PhyLogConfig(TK8710_LOG_WARN, 0xFFFFFFFFu, 1);
    TRM_LogConfig(TRM_LOG_DEBUG, 1);//TRM_LOG_INFO

    if (TRM_PhyInit(chipConfig, &trmConfig) != TRM_OK) {
        return TK8710_HAL_ERROR_INIT;
    }

    TK8710HalStatusSetState(TK8710_HAL_STATE_INIT);
    return TK8710_HAL_OK;
}

TK8710HalError TK8710HalCfg(const void* slotCfg)
{
    const slotCfg_t* configToUse = (const slotCfg_t*)slotCfg;

    if (!TK8710HalStatusRequireConfigurable()) {
        return TK8710_HAL_ERROR_CONFIG;
    }

    if (configToUse == NULL) {
        configToUse = TK8710GetSlotConfig();
    }
    if (configToUse == NULL) {
        return TK8710_HAL_ERROR_PARAM;
    }

    return TRM_PhyConfig(configToUse) == TRM_OK ? TK8710_HAL_OK : TK8710_HAL_ERROR_CONFIG;
}

TK8710HalError TK8710HalStart(void)
{
    if (!TK8710HalStatusRequireInit()) {
        return TK8710_HAL_ERROR_START;
    }
    if (TRM_PhyStart() != TRM_OK) {
        return TK8710_HAL_ERROR_START;
    }
    TK8710HalStatusSetState(TK8710_HAL_STATE_RUNNING);
    return TK8710_HAL_OK;
}

TK8710HalError TK8710HalReset(void)
{
    if (TRM_PhyReset() != TRM_OK) {
        return TK8710_HAL_ERROR_RESET;
    }
    TK8710HalStatusSetState(TK8710_HAL_STATE_UNINIT);
    return TK8710_HAL_OK;
}

TK8710HalError TK8710HalSendData(TK8710DownlinkType downlinkType,
    uint32_t userId_brdIndex,
    const uint8_t* data,
    uint16_t len,
    uint8_t txPower,
    uint32_t frameNo,
    uint8_t targetRateMode,
    uint8_t beamType)
{
    if (!TK8710HalStatusRequireInit()) {
        return TK8710_HAL_ERROR_SEND;
    }
    if (data == NULL || len == 0) {
        return TK8710_HAL_ERROR_PARAM;
    }

    return TRM_PhySendData(downlinkType, userId_brdIndex, data, len, txPower,
        frameNo, targetRateMode, beamType) == TRM_OK ? TK8710_HAL_OK : TK8710_HAL_ERROR_SEND;
}

TK8710HalError TK8710HalGetBeam(uint32_t userId, TrmBeamInfo* beamInfo)
{
    if (beamInfo == NULL) {
        return TK8710_HAL_ERROR_PARAM;
    }
    return TRM_PhyGetBeam(userId, beamInfo) == TRM_OK ? TK8710_HAL_OK : TK8710_HAL_ERROR_STATUS;
}

TK8710HalError TK8710HalGetStatus(TrmStatus* status)
{
    if (status == NULL) {
        return TK8710_HAL_ERROR_PARAM;
    }
    return TRM_PhyGetStats(status) == TRM_OK ? TK8710_HAL_OK : TK8710_HAL_ERROR_STATUS;
}

TK8710HalError TK8710HalDebug(uint32_t type, void* param1, void* param2)
{
    if (type == 1U) {
        return TK8710PhyReadReg((uint32_t)(uintptr_t)param1, (uint32_t*)param2) == TK8710_OK ?
            TK8710_HAL_OK : TK8710_HAL_ERROR_STATUS;
    }
    if (type == 2U) {
        return TK8710PhyWriteReg((uint32_t)(uintptr_t)param1, (uint32_t)(uintptr_t)param2) == TK8710_OK ?
            TK8710_HAL_OK : TK8710_HAL_ERROR_STATUS;
    }
    return TK8710_HAL_ERROR_PARAM;
}
