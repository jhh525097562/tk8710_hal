#ifndef TK8710_HAL_INTERNAL_H
#define TK8710_HAL_INTERNAL_H

#include "hal_api.h"
#include "trm/trm_api.h"

typedef enum {
    TK8710_HAL_STATE_UNINIT = 0,
    TK8710_HAL_STATE_INIT,
    TK8710_HAL_STATE_RUNNING,
} TK8710HalState;

void TK8710HalCbStore(const TK8710HalTrmCfg* cfg);
void TK8710HalCbBuildTrmConfig(TRM_InitConfig* trmConfig, const TK8710HalTrmCfg* cfg);

void TK8710HalStatusSetState(TK8710HalState state);
TK8710HalState TK8710HalStatusGetState(void);
int TK8710HalStatusRequireInit(void);
int TK8710HalStatusRequireConfigurable(void);

#endif /* TK8710_HAL_INTERNAL_H */
