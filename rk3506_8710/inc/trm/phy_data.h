#ifndef TRM_PHY_DATA_H
#define TRM_PHY_DATA_H

#include <stdint.h>
#include "../driver/tk8710_types.h"
#include "trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int TRM_PhySendData(TK8710DownlinkType downlinkType, uint32_t userId_brdIndex,
    const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo,
    uint8_t targetRateMode, uint8_t beamType);
int TRM_PhyGetBeam(uint32_t userId, TRM_BeamInfo* beamInfo);

#ifdef __cplusplus
}
#endif

#endif /* TRM_PHY_DATA_H */
