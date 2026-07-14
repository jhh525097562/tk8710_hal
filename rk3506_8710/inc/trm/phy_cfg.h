#ifndef TRM_PHY_CFG_H
#define TRM_PHY_CFG_H

#include "../driver/tk8710_types.h"
#include "trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int TRM_PhyInit(const ChipConfig* chipConfig, const TRM_InitConfig* trmConfig);
int TRM_PhyConfig(const slotCfg_t* slotConfig);
int TRM_PhyStart(void);
int TRM_PhyReset(void);

#ifdef __cplusplus
}
#endif

#endif /* TRM_PHY_CFG_H */
