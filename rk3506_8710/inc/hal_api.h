#ifndef TK8710_HAL_API_H
#define TK8710_HAL_API_H

#include <stdint.h>
#include "driver/tk8710_types.h"
#include "trm/trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TK8710_HAL_OK = 0,
    TK8710_HAL_ERROR_PARAM = -1,
    TK8710_HAL_ERROR_INIT = -2,
    TK8710_HAL_ERROR_CONFIG = -3,
    TK8710_HAL_ERROR_START = -4,
    TK8710_HAL_ERROR_SEND = -5,
    TK8710_HAL_ERROR_STATUS = -6,
    TK8710_HAL_ERROR_RESET = -7,
} TK8710HalError;

typedef TRM_BeamInfo TrmBeamInfo;
typedef TRM_RxUserData TrmRxUserData;
typedef TRM_RxDataList TrmRxDataList;
typedef TRM_TxResult TrmTxResult;
typedef TRM_TxUserResult TrmTxUserResult;
typedef TRM_TxCompleteResult TrmTxCompleteResult;
typedef TRM_Stats TrmStatus;

typedef TRM_OnRxData TrmOnRxData;
typedef TRM_OnTxComplete TrmOnTxComplete;

typedef struct {
    uint32_t beamMaxUsers;
    uint32_t beamTimeoutMs;
    uint32_t maxFrameCount;
    TrmOnRxData onRxData;
    TrmOnTxComplete onTxComplete;
} TK8710HalTrmCfg;

typedef struct {
    void* chipInitCfg;
    TK8710HalTrmCfg trmCfg;
} TK8710HalInitCfg;

TK8710HalError TK8710HalInit(const TK8710HalInitCfg* cfg);
TK8710HalError TK8710HalCfg(const void* slotCfg);
TK8710HalError TK8710HalStart(void);
TK8710HalError TK8710HalReset(void);
TK8710HalError TK8710HalSendData(TK8710DownlinkType downlinkType,
    uint32_t userId_brdIndex,
    const uint8_t* data,
    uint16_t len,
    uint8_t txPower,
    uint32_t frameNo,
    uint8_t targetRateMode,
    uint8_t beamType);
TK8710HalError TK8710HalGetBeam(uint32_t userId, TrmBeamInfo* beamInfo);
TK8710HalError TK8710HalGetStatus(TrmStatus* status);
TK8710HalError TK8710HalDebug(uint32_t type, void* param1, void* param2);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_HAL_API_H */
