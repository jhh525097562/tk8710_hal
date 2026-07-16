/**
 * @file trm_satellite.h
 * @brief TRM satellite payload and ground-station role helpers.
 */

#ifndef TRM_SATELLITE_H
#define TRM_SATELLITE_H

#include <stdint.h>
#include "trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void TRM_SatelliteInit(const TRM_InitConfig* config);
void TRM_SatelliteDeinit(void);
void TRM_SatelliteReset(void);

int TRM_SatelliteAllowRxBeamStore(const uint8_t* data, uint16_t len);
uint8_t TRM_SatelliteKeepRxBeam(const uint8_t* data, uint16_t len);
uint8_t TRM_SatelliteKeepTxBeam(uint32_t userId);
uint8_t TRM_SatelliteIsPayloadTx(void);
uint8_t TRM_SatelliteIsPayloadGroundStationTx(uint32_t userId);
uint8_t TRM_SatelliteIsGroundStationTx(void);
int TRM_SatelliteGetTxBeam(uint32_t userId, TRM_BeamInfo* beamInfo);
void TRM_SatelliteAdjustForwardBeam(uint8_t userIndex, uint8_t userCount,
    uint8_t rateMode, TRM_BeamInfo* beamInfo);
void TRM_SatelliteAdjustGroundStationTxBeam(uint8_t rateMode, TRM_BeamInfo* beamInfo);
void TRM_SatelliteBeforeRxBeamStore(uint32_t userId, const uint8_t* data, uint16_t len);
void TRM_SatelliteAfterRxBeamStore(uint32_t userId, const uint8_t* data, uint16_t len);
void TRM_SatelliteProcessRxUser(const TRM_RxUserData* user);
uint8_t TRM_SatelliteShouldDeliverRxUser(const uint8_t* data, uint16_t len);
void TRM_SatelliteProcessIrq(const TK8710IrqResult* irqResult);
void TRM_SatelliteProcessTxSlot(uint8_t maxUserCount, const TK8710IrqResult* irqResult);
uint8_t TRM_SatelliteLimitTxUserCount(uint8_t maxUserCount);
int TRM_SatelliteBeforeTxData(uint32_t userId, const uint8_t* data, uint16_t len);
void TRM_SatelliteAfterTxData(uint32_t userId, int result);
void TRM_SatelliteUpdateStats(TRM_Stats* stats);

#ifdef __cplusplus
}
#endif

#endif /* TRM_SATELLITE_H */
