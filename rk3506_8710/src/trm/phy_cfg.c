#include "trm/phy_cfg.h"
#include "phy/phy_api.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_internal.h"
#include "tk8710_hal.h"

int TRM_PhyInit(const ChipConfig* chipConfig, const TRM_InitConfig* trmConfig)
{
    int ret = TK8710PhyInit(chipConfig);
    if (ret != TK8710_OK) {
        return TRM_ERR_DRIVER;
    }
    return TRM_Init(trmConfig);
}

int TRM_PhyConfig(const slotCfg_t* slotConfig)
{
    if (slotConfig == NULL) {
        return TRM_ERR_PARAM;
    }
    return TK8710PhyCfg(TK8710_CFG_TYPE_SLOT_CFG, slotConfig) == TK8710_OK ?
        TRM_OK : TRM_ERR_DRIVER;
}

int TRM_PhyStart(void)
{
    return TK8710PhyStart(TK8710_MODE_MASTER, TK8710_WORK_MODE_CONTINUOUS) == TK8710_OK ?
        TRM_OK : TRM_ERR_DRIVER;
}

int TRM_PhyReset(void)
{
    int trmRet = TRM_Deinit();
    int phyRet;
    phyRet = TK8710PhyReset(TK8710_RST_STATE_MACHINE);
    phyRet = TK8710PhyReset(TK8710_RST_ALL);
    TK8710GpioIrqEnable(0, 0);
#ifdef PLATFORM_RK3506
    TK8710Rk3506Cleanup();
#endif
    if (trmRet != TRM_OK) {
        return trmRet;
    }
    return phyRet == TK8710_OK ? TRM_OK : TRM_ERR_DRIVER;
}
