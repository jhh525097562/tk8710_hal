#include "trm/phy_data.h"

int TRM_PhySendData(TK8710DownlinkType downlinkType, uint32_t userId_brdIndex,
    const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo,
    uint8_t targetRateMode, uint8_t beamType)
{
    return TRM_SetTxData(downlinkType, userId_brdIndex, data, len, txPower,
        frameNo, targetRateMode, beamType);
}

int TRM_PhyGetBeam(uint32_t userId, TRM_BeamInfo* beamInfo)
{
    return TRM_GetBeamInfo(userId, beamInfo);
}
