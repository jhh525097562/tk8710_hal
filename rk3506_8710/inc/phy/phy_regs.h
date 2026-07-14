#ifndef TK8710_PHY_REGS_H
#define TK8710_PHY_REGS_H

#include "phy_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int TK8710PhyDebug(TK8710DebugCtrlType ctrlType, CtrlOptType optType,
    const void* inputParams, void* outputParams);
int TK8710PhyReadReg(uint32_t regAddr, uint32_t* regData);
int TK8710PhyWriteReg(uint32_t regAddr, uint32_t regData);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_PHY_REGS_H */
