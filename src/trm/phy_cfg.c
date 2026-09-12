#include "trm/phy_cfg.h"
#include "phy/phy_api.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_internal.h"
#include "tk8710_hal.h"

#define TRM_BROADCAST_BUFFER_BASE 128U
#define TRM_BROADCAST_BUFFER_COUNT 16U
#define TRM_BROADCAST_RAM_CLEAR_LEN 128U

static int _trm_clear_broadcast_tx_ram(void)
{
    uint8_t clearData[TRM_BROADCAST_RAM_CLEAR_LEN] = {0};
    int result = TK8710_OK;

    for (uint8_t brdIndex = 0; brdIndex < TRM_BROADCAST_BUFFER_COUNT; brdIndex++) {
        uint8_t bufferIndex = (uint8_t)(TRM_BROADCAST_BUFFER_BASE + brdIndex);
        int ret = TK8710WriteBuffer(bufferIndex, clearData, sizeof(clearData));
        if (ret != TK8710_OK) {
            TK8710_LOG_CORE_ERROR("Failed to clear broadcast TX RAM: brdIndex=%u, ret=%d",
                                  brdIndex, ret);
            result = ret;
        }
    }

    return result;
}

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
    int clearRet;
    phyRet = TK8710PhyReset(TK8710_RST_STATE_MACHINE);
    phyRet = TK8710PhyReset(TK8710_RST_ALL);
    clearRet = _trm_clear_broadcast_tx_ram();
    TK8710GpioIrqEnable(0, 0);
#ifdef PLATFORM_RK3506
    TK8710Rk3506Cleanup();
#endif
    if (trmRet != TRM_OK) {
        return trmRet;
    }
    return phyRet == TK8710_OK && clearRet == TK8710_OK ? TRM_OK : TRM_ERR_DRIVER;
}
