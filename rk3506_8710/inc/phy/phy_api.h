#ifndef TK8710_PHY_API_H
#define TK8710_PHY_API_H

#include <stdint.h>
#include "../driver/tk8710_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ChipConfig ChipCfg;
typedef TxToneConfig TxToneCfg;
typedef TK8710IrqResult TK8710PhyIrqResult;
typedef TK8710DriverCallbacks TK8710DriverCbs;
typedef TK8710ConfigType TK8710CfgType;

int TK8710PhyInit(const ChipCfg* initCfg);
int TK8710PhyCfg(TK8710CfgType type, const void* params);
int TK8710PhyStart(uint8_t workType, uint8_t workMode);
int TK8710PhyReset(uint8_t rstType);
int TK8710PhySendData(TK8710DownlinkType downlinkType, uint8_t index, const uint8_t* data,
    uint16_t dataLen, uint8_t txPower, uint8_t beamType);
int TK8710PhyRecvData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen);
int TK8710PhySetBeam(uint8_t userBufferIdx, uint32_t freqInfo, const uint32_t* ahInfo,
    uint64_t pilotPowerInfo);
int TK8710PhyGetBeam(uint8_t userBufferIdx, uint32_t* freqInfo, uint32_t* ahInfo,
    uint64_t* pilotPowerInfo);
int TK8710PhyGetRxQuality(uint8_t userIndex, uint32_t* rssi, uint8_t* snr, uint32_t* freq);
int TK8710PhyDebug(TK8710DebugCtrlType ctrlType, CtrlOptType optType,
    const void* inputParams, void* outputParams);
int TK8710PhyReadReg(uint32_t regAddr, uint32_t* regData);
int TK8710PhyWriteReg(uint32_t regAddr, uint32_t regData);
void TK8710PhySetCbs(const TK8710DriverCbs* cbs);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_PHY_API_H */
