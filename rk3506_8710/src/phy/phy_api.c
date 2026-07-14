#include "phy/phy_api.h"
#include "driver/tk8710_driver_api.h"

int TK8710PhyInit(const ChipCfg* initCfg)
{
    return TK8710Init(initCfg);
}

int TK8710PhyCfg(TK8710CfgType type, const void* params)
{
    return TK8710SetConfig(type, params);
}

int TK8710PhyStart(uint8_t workType, uint8_t workMode)
{
    return TK8710Start(workType, workMode);
}

int TK8710PhyReset(uint8_t rstType)
{
    return TK8710Reset(rstType);
}

int TK8710PhySendData(TK8710DownlinkType downlinkType, uint8_t index, const uint8_t* data,
    uint16_t dataLen, uint8_t txPower, uint8_t beamType)
{
    return TK8710SetTxData(downlinkType, index, data, dataLen, txPower, beamType);
}

int TK8710PhyRecvData(uint8_t userIndex, uint8_t** data, uint16_t* dataLen)
{
    return TK8710GetRxUserData(userIndex, data, dataLen);
}

int TK8710PhySetBeam(uint8_t userBufferIdx, uint32_t freqInfo, const uint32_t* ahInfo,
    uint64_t pilotPowerInfo)
{
    return TK8710SetTxUserInfo(userBufferIdx, freqInfo, ahInfo, pilotPowerInfo);
}

int TK8710PhyGetBeam(uint8_t userBufferIdx, uint32_t* freqInfo, uint32_t* ahInfo,
    uint64_t* pilotPowerInfo)
{
    return TK8710GetRxUserInfo(userBufferIdx, freqInfo, ahInfo, pilotPowerInfo);
}

int TK8710PhyGetRxQuality(uint8_t userIndex, uint32_t* rssi, uint8_t* snr, uint32_t* freq)
{
    return TK8710GetRxUserSignalQuality(userIndex, rssi, snr, freq);
}
