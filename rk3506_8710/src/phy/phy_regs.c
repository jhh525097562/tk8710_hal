#include "phy/phy_regs.h"
#include "driver/tk8710_log.h"
#include "driver/tk8710_internal.h"
#include "driver/tk8710_regs.h"

int TK8710PhyDebug(TK8710DebugCtrlType ctrlType, CtrlOptType optType,
    const void* inputParams, void* outputParams)
{
    return TK8710DebugCtrl(ctrlType, optType, inputParams, outputParams);
}

int TK8710PhyReadReg(uint32_t regAddr, uint32_t* regData)
{
    if (regData == NULL) {
        return TK8710_ERR_PARAM;
    }
    return TK8710ReadReg(TK8710_REG_TYPE_GLOBAL, (uint16_t)regAddr, regData);
}

int TK8710PhyWriteReg(uint32_t regAddr, uint32_t regData)
{
    return TK8710WriteReg(TK8710_REG_TYPE_GLOBAL, (uint16_t)regAddr, regData);
}
