/**
 * @file trm_mac_parser.c
 * @brief TRM MAC协议帧解析实现
 * @note 此文件包含TurMass™ Link MAC协议帧的解析函数实现
 */

#include "trm_mac_parser.h"
#include "trm_log.h"

/* =============================================================================
 * MAC协议帧解析辅助函数实现
 * ============================================================================= */

static uint32_t TRM_ExtractUserIdFromDevEui(const uint8_t* devEui)
{
    return ((uint32_t)devEui[3] << 24) | ((uint32_t)devEui[2] << 16) |
           ((uint32_t)devEui[1] << 8) | (uint32_t)devEui[0];
}

int TRM_ParseMacMhdr(const uint8_t* data, uint16_t len, TrmMacMhdr* mhdr)
{
    if (data == NULL || mhdr == NULL || len < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for MHDR parsing");
        return -1;
    }
    
    /* 解析第一个字节 */
    mhdr->frameType = (data[TRM_MHDR_BYTE1_OFFSET] >> TRM_MHDR_FRAMETYPE_SHIFT) & TRM_MHDR_FRAMETYPE_MASK;
    mhdr->devType   = (data[TRM_MHDR_BYTE1_OFFSET] >> TRM_MHDR_DEVTYPE_SHIFT) & TRM_MHDR_DEVTYPE_MASK;
    mhdr->version   = (data[TRM_MHDR_BYTE1_OFFSET] >> TRM_MHDR_VERSION_SHIFT) & TRM_MHDR_VERSION_MASK;
    
    /* 解析第二个字节 (QoS相关) */
    mhdr->qosPri = (data[TRM_MHDR_BYTE2_OFFSET] & TRM_MHDR_QOSPRI_MASK) >> TRM_MHDR_QOSPRI_SHIFT;
    mhdr->qosTtl = (data[TRM_MHDR_BYTE2_OFFSET] & TRM_MHDR_QOSTTL_MASK) >> TRM_MHDR_QOSTTL_SHIFT;
    
    /* 根据网络模式解析HopNum和HopCnt字段 */
    uint8_t nwkMode = (data[TRM_MHDR_BYTE3_OFFSET] & TRM_MHDR_NWKMODE_MASK) >> TRM_MHDR_NWKMODE_SHIFT;
    if (nwkMode == TRM_MAC_NWKMODE_WAN) {
        /* WAN模式：HopNum和HopCnt均为2bit */
        mhdr->hopNum = (data[TRM_MHDR_BYTE2_OFFSET] >> TRM_MHDR_HOPNUM_SHIFT) & TRM_MHDR_HOPNUM_MASK_WAN;
        mhdr->hopCnt = (data[TRM_MHDR_BYTE2_OFFSET] >> TRM_MHDR_HOPCNT_SHIFT) & TRM_MHDR_HOPCNT_MASK_WAN;
    } else {
        /* LAN模式：HopNum和HopCnt均为4bit */
        mhdr->hopNum = (data[TRM_MHDR_BYTE2_OFFSET] >> TRM_MHDR_HOPNUM_SHIFT) & TRM_MHDR_HOPNUM_MASK_LAN;
        mhdr->hopCnt = (data[TRM_MHDR_BYTE2_OFFSET] >> TRM_MHDR_HOPCNT_SHIFT) & TRM_MHDR_HOPCNT_MASK_LAN;
    }
    
    /* 解析第三个字节 */
    mhdr->securityMode = (data[TRM_MHDR_BYTE3_OFFSET] & TRM_MHDR_SECURITYMODE_MASK) >> TRM_MHDR_SECURITYMODE_SHIFT;
    mhdr->addrMode      = (data[TRM_MHDR_BYTE3_OFFSET] & TRM_MHDR_ADDRMODE_MASK) >> TRM_MHDR_ADDRMODE_SHIFT;
    mhdr->nwkMode       = nwkMode;
    mhdr->powerCtrlType = (data[TRM_MHDR_BYTE3_OFFSET] & TRM_MHDR_POWERCTRL_MASK) >> TRM_MHDR_POWERCTRL_SHIFT;
    mhdr->rfu           = (data[TRM_MHDR_BYTE3_OFFSET] & TRM_MHDR_RFU_MASK) >> TRM_MHDR_RFU_SHIFT;
    
    TRM_LOG_DEBUG("TRM: MHDR parsed - FrameType=0x%01X, DevType=%d, QosPri=%d, TTL=%d, AddrMode=%d", 
                  mhdr->frameType, mhdr->devType, mhdr->qosPri, mhdr->qosTtl, mhdr->addrMode);
    
    return 0;
}

int TRM_ExtractUserIdFromMacFrame(const uint8_t* data, uint16_t len, uint32_t* userId)
{
    if (data == NULL || userId == NULL || len < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for user ID extraction");
        return -1;
    }
    
    /* 解析MHDR获取帧类型 */
    TrmMacMhdr mhdr;
    if (TRM_ParseMacMhdr(data, len, &mhdr) != 0) {
        return -1;
    }
    
    /* 根据帧类型提取用户ID */
    switch (mhdr.frameType) {
        case TRM_MAC_FRAMETYPE_JOIN_ACCEPT:
            /* Join Accept帧：MACPayload为JoinResult(1) + DevEUI(8) + ... */
            if (len >= 3 + 1 + 8) {
                *userId = TRM_ExtractUserIdFromDevEui(&data[3 + 1]);
                TRM_LOG_DEBUG("TRM: Extracted user ID from Join Accept DevEUI: 0x%08X", *userId);
                return 0;
            } else {
                TRM_LOG_WARN("TRM: Join Accept frame too short for DevEUI extraction");
                return -1;
            }
            
        case TRM_MAC_FRAMETYPE_JOIN_REQUEST:
            /* Join Request帧：MACPayload为Capacity(1) + DevEUI(8) + DevNonce(2) */
            if (len >= 3 + 1 + 8) {
                *userId = TRM_ExtractUserIdFromDevEui(&data[3 + 1]);
                TRM_LOG_DEBUG("TRM: Extracted user ID from Join Request DevEUI: 0x%08X", *userId);
                return 0;
            } else {
                TRM_LOG_WARN("TRM: Join Request frame too short for DevEUI extraction");
                return -1;
            }

        case TRM_MAC_FRAMETYPE_DISASSOCIATION:
            /* Disassociation帧：MAC Payload为DevEUI(8) + Status(1)，不携带普通FHDR */
            if (len >= 3 + 8 + 1) {
                *userId = TRM_ExtractUserIdFromDevEui(&data[3]);
                TRM_LOG_DEBUG("TRM: Extracted user ID from Disassociation DevEUI: 0x%08X", *userId);
                return 0;
            } else {
                TRM_LOG_WARN("TRM: Disassociation frame too short for DevEUI extraction");
                return -1;
            }
            
        default: {
            /* 其他帧类型：提取FHDR中SrcADDR作为UserID */
            /* FHDR从MHDR后开始，偏移3字节 */
            uint8_t fhdrOffset = 3;
            
            if (mhdr.addrMode == 0) {
                /* 短地址模式 (2字节) */
                if (len >= fhdrOffset + 2) {
                    *userId = (data[fhdrOffset] << 8) | data[fhdrOffset + 1];
                    TRM_LOG_DEBUG("TRM: Extracted user ID from short address: 0x%04X", *userId);
                    return 0;
                } else {
                    TRM_LOG_WARN("TRM: Data too short for short address extraction");
                    return -1;
                }
            } else {
                /* 长地址模式 (4字节: NwkID + NwkAddr) */
                if (len >= fhdrOffset + 4) {
                    *userId = ((uint32_t)data[fhdrOffset + 3] << 24) |
                              ((uint32_t)data[fhdrOffset + 2] << 16) |
                              ((uint32_t)data[fhdrOffset + 1] << 8) |
                              (uint32_t)data[fhdrOffset + 0];
                    TRM_LOG_DEBUG("TRM: Extracted user ID from long address: 0x%08X", *userId);
                    return 0;
                } else {
                    TRM_LOG_WARN("TRM: Data too short for long address extraction");
                    return -1;
                }
            }
        }
    }
}

int TRM_ExtractSrcAddrFromMacFrame(const uint8_t* data, uint16_t len, uint32_t* srcAddr)
{
    TrmMacMhdr mhdr;
    uint8_t fhdrOffset = 3;

    if (data == NULL || srcAddr == NULL || len < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for source address extraction");
        return -1;
    }

    if (TRM_ParseMacMhdr(data, len, &mhdr) != 0) {
        return -1;
    }

    if (mhdr.frameType == TRM_MAC_FRAMETYPE_JOIN_REQUEST ||
        mhdr.frameType == TRM_MAC_FRAMETYPE_JOIN_ACCEPT) {
        /* Join帧没有普通FHDR，data[3]是Capacity/JoinResult，身份字段从DevEUI开始。 */
        return TRM_ExtractUserIdFromMacFrame(data, len, srcAddr);
    }

    if (mhdr.addrMode == 0) {
        if (len < fhdrOffset + 2) {
            return -1;
        }
        *srcAddr = ((uint32_t)data[fhdrOffset] << 8) | data[fhdrOffset + 1];
        return 0;
    }

    if (len < fhdrOffset + 4) {
        return -1;
    }

    *srcAddr = ((uint32_t)data[fhdrOffset + 3] << 24) |
               ((uint32_t)data[fhdrOffset + 2] << 16) |
               ((uint32_t)data[fhdrOffset + 1] << 8) |
               (uint32_t)data[fhdrOffset + 0];
    return 0;
}

int TRM_GetMacFrameQosInfo(const uint8_t* data, uint16_t len, uint8_t* qosPri, uint8_t* qosTtl)
{
    if (data == NULL || qosPri == NULL || qosTtl == NULL || len < 2) {
        TRM_LOG_ERROR("TRM: Invalid parameters for QoS info extraction");
        return -1;
    }
    
    /* 从MHDR第二个字节提取QoS信息 */
    uint8_t mhdrByte2 = data[TRM_MHDR_BYTE2_OFFSET];
    *qosPri = (mhdrByte2 & TRM_MHDR_QOSPRI_MASK) >> TRM_MHDR_QOSPRI_SHIFT;
    *qosTtl = (mhdrByte2 & TRM_MHDR_QOSTTL_MASK) >> TRM_MHDR_QOSTTL_SHIFT;
    
    TRM_LOG_DEBUG("TRM: Extracted QoS info - Pri=%d, TTL=%d", *qosPri, *qosTtl);
    return 0;
}

int TRM_BuildMacMhdr(const TrmMacMhdr* mhdr, uint8_t* data, uint16_t maxLen)
{
    if (mhdr == NULL || data == NULL || maxLen < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for MHDR building");
        return -1;
    }
    
    uint16_t pos = 0;
    
    /* 构建第一个字节 */
    data[pos] = ((mhdr->frameType & TRM_MHDR_FRAMETYPE_MASK) << TRM_MHDR_FRAMETYPE_SHIFT) |
               ((mhdr->devType   & TRM_MHDR_DEVTYPE_MASK)   << TRM_MHDR_DEVTYPE_SHIFT)   |
               ((mhdr->version   & TRM_MHDR_VERSION_MASK)   << TRM_MHDR_VERSION_SHIFT);
    pos++;
    
    /* 构建第二个字节 (QoS相关) */
    data[pos] = ((mhdr->qosPri & 0x03) << TRM_MHDR_QOSPRI_SHIFT) |
               ((mhdr->qosTtl & 0x03) << TRM_MHDR_QOSTTL_SHIFT) |
               ((mhdr->hopNum & 0x03) << TRM_MHDR_HOPNUM_SHIFT) |
               ((mhdr->hopCnt & 0x03) << TRM_MHDR_HOPCNT_SHIFT);
    pos++;
    
    /* 构建第三个字节 */
    data[pos] = ((mhdr->securityMode & 0x07) << TRM_MHDR_SECURITYMODE_SHIFT) |
               ((mhdr->addrMode      & 0x01) << TRM_MHDR_ADDRMODE_SHIFT)     |
               ((mhdr->nwkMode       & 0x03) << TRM_MHDR_NWKMODE_SHIFT)      |
               ((mhdr->powerCtrlType & 0x01) << TRM_MHDR_POWERCTRL_SHIFT)    |
               ((mhdr->rfu           & 0x01) << TRM_MHDR_RFU_SHIFT);
    pos++;
    
    TRM_LOG_DEBUG("TRM: MHDR built - FrameType=0x%01X, DevType=%d, QosPri=%d, TTL=%d", 
                  mhdr->frameType, mhdr->devType, mhdr->qosPri, mhdr->qosTtl);
    
    return 3; /* MHDR固定3字节 */
}

int TRM_GetMacFrameType(const uint8_t* data, uint16_t len, uint8_t* frameType)
{
    if (data == NULL || frameType == NULL || len < 1) {
        TRM_LOG_ERROR("TRM: Invalid parameters for frame type detection");
        return -1;
    }
    
    /* 从第一个字节的高4位提取帧类型 */
    *frameType = (data[0] >> TRM_MHDR_FRAMETYPE_SHIFT) & TRM_MHDR_FRAMETYPE_MASK;
    
    TRM_LOG_DEBUG("TRM: Detected frame type: 0x%01X", *frameType);
    return 0;
}

int TRM_ParseMacFrame(const uint8_t* data, uint16_t len, TrmMacFrame* frame)
{
    if (data == NULL || frame == NULL || len < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for MAC frame parsing");
        return -1;
    }
    
    /* 解析MHDR */
    if (TRM_ParseMacMhdr(data, len, &frame->mhdr) != 0) {
        return -1;
    }
    
    /* 计算FHDR偏移 */
    uint16_t fhdrOffset = 3;
    uint16_t remainingLen = len - fhdrOffset;
    
    if (remainingLen < 2) {
        TRM_LOG_WARN("TRM: Frame too short for FHDR parsing");
        return -1;
    }
    
    /* 解析FHDR - 源地址 */
    uint8_t addrLen = (frame->mhdr.addrMode == 0) ? 2 : 4;
    if (remainingLen < addrLen) {
        TRM_LOG_WARN("TRM: Frame too short for source address");
        return -1;
    }
    
    /* 复制源地址 */
    if (addrLen == 2) {
        frame->payload.fhdr.srcAddr[0] = data[fhdrOffset];
        frame->payload.fhdr.srcAddr[1] = data[fhdrOffset + 1];
        frame->payload.fhdr.srcAddr[2] = 0;
        frame->payload.fhdr.srcAddr[3] = 0;
    } else {
        frame->payload.fhdr.srcAddr[0] = data[fhdrOffset];
        frame->payload.fhdr.srcAddr[1] = data[fhdrOffset + 1];
        frame->payload.fhdr.srcAddr[2] = data[fhdrOffset + 2];
        frame->payload.fhdr.srcAddr[3] = data[fhdrOffset + 3];
    }
    
    /* 构建FCtrl字段 */
    if (remainingLen >= 1) {
        frame->payload.fhdr.fctrl.ack = (data[fhdrOffset + addrLen] >> 7) & 0x01;
        frame->payload.fhdr.fctrl.foptsNum = (data[fhdrOffset + addrLen] >> 3) & 0x0F;
        frame->payload.fhdr.fctrl.retranPkg = (data[fhdrOffset + addrLen] >> 2) & 0x01;
        frame->payload.fhdr.fctrl.subPkg = (data[fhdrOffset + addrLen] >> 1) & 0x01;
        frame->payload.fhdr.fctrl.rfu = data[fhdrOffset + addrLen] & 0x01;
    }
    
    /* 解析可选字段 */
    if (frame->payload.fhdr.fctrl.subPkg && remainingLen >= 2) {
        frame->payload.fhdr.subPkg.subPkgIdx = (data[fhdrOffset + addrLen + 1] >> 4) & 0x0F;
        frame->payload.fhdr.subPkg.subPkgNum = data[fhdrOffset + addrLen + 1] & 0x0F;
    }
    
    if (frame->payload.fhdr.fctrl.retranPkg && remainingLen >= 2) {
        frame->payload.fhdr.retrans.retransIdx = (data[fhdrOffset + addrLen + 1] >> 4) & 0x0F;
        frame->payload.fhdr.retrans.retransNum = data[fhdrOffset + addrLen + 1] & 0x0F;
    }
    
    /* 解析FOpts */
    uint8_t foptsLen = frame->payload.fhdr.fctrl.foptsNum;
    if (foptsLen > 0 && remainingLen >= foptsLen + addrLen + 1) {
        if (foptsLen > 15) foptsLen = 15; /* 限制最大长度 */
        for (uint8_t i = 0; i < foptsLen; i++) {
            frame->payload.fhdr.fopts[i] = data[fhdrOffset + addrLen + 1 + i];
        }
        /* 解析broadcastGroup字段 (WAN模式下的广播组标识) */
        if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN && foptsLen > 0) {
            /* 检查是否有足够的字节来解析broadcastGroup */
            if (remainingLen >= addrLen + 1 + foptsLen + 1) {
                /* broadcastGroup在FOpts的最后一个字节中 */
                frame->payload.fhdr.broadcastGroup = data[fhdrOffset + addrLen + 1 + foptsLen];
            }
        }
    }
    
    /* 功率控制 */
    if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN) {
        /* WAN模式：TxPowerIdx字段存在 */
        if (remainingLen >= addrLen + 1 + foptsLen + 1) {
            frame->payload.fhdr.txPowerIdx = data[fhdrOffset + addrLen + 1 + foptsLen];
        }
    } else {
        /* LAN模式：TxPowerIdx字段也存在，用于开环功控时填入发送功率 */
        if (remainingLen >= addrLen + 1 + foptsLen + 1) {
            frame->payload.fhdr.txPowerIdx = data[fhdrOffset + addrLen + 1 + foptsLen];
        }
    }
    
    /* 解析FPort和负载 */
    if (remainingLen >= addrLen + 1 + foptsLen + 2) {
        frame->payload.fport = data[fhdrOffset + addrLen + 1 + foptsLen + 2];
    }
    
    /* WAN模式有负载长度字段 */
    if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN && remainingLen >= addrLen + 1 + foptsLen + 3) {
        frame->payload.payloadLen = (data[fhdrOffset + addrLen + 1 + foptsLen + 2] << 8) | 
                                    data[fhdrOffset + addrLen + 1 + foptsLen + 3];
    } else {
        frame->payload.payloadLen = 0;
    }
    
    /* 解析负载数据 */
    if (remainingLen > addrLen + 1 + foptsLen + 2) {
        frame->payload.payload = (uint8_t*)&data[fhdrOffset + addrLen + 1 + foptsLen + 2];
        frame->payload.payloadSize = remainingLen - addrLen - 1 - foptsLen - 2;
    } else {
        frame->payload.payload = NULL;
        frame->payload.payloadSize = 0;
    }
    
    /* 解析MIC (如果存在) */
    if (frame->mhdr.securityMode != TRM_MAC_SECURITY_NONE && 
        frame->mhdr.securityMode != TRM_MAC_SECURITY_TEA_ONLY &&
        frame->mhdr.securityMode != TRM_MAC_SECURITY_AES_ONLY) {
        uint16_t micOffset = fhdrOffset + addrLen + 1 + foptsLen + 2 + frame->payload.payloadSize;
        if (len >= micOffset + 4) {
            frame->mic[0] = data[micOffset];
            frame->mic[1] = data[micOffset + 1];
            frame->mic[2] = data[micOffset + 2];
            frame->mic[3] = data[micOffset + 3];
        }
    }
    
    TRM_LOG_DEBUG("TRM: MAC frame parsed successfully - Type=0x%01X, PayloadLen=%d", 
                  frame->mhdr.frameType, frame->payload.payloadSize);
    
    return 0;
}

int TRM_BuildMacFrame(const TrmMacFrame* frame, uint8_t* data, uint16_t maxLen)
{
    if (frame == NULL || data == NULL || maxLen < 3) {
        TRM_LOG_ERROR("TRM: Invalid parameters for MAC frame building");
        return -1;
    }
    
    uint16_t pos = 0;
    
    /* 构建MHDR */
    int mhdrLen = TRM_BuildMacMhdr(&frame->mhdr, &data[pos], maxLen - pos);
    if (mhdrLen < 0) {
        return -1;
    }
    pos += mhdrLen;
    
    /* 构建FHDR */
    uint8_t addrLen = (frame->mhdr.addrMode == 0) ? 2 : 4;
    
    /* 源地址 */
    if (pos + addrLen > maxLen) {
        TRM_LOG_ERROR("TRM: Buffer too small for source address");
        return -1;
    }
    for (uint8_t i = 0; i < addrLen; i++) {
        data[pos + i] = frame->payload.fhdr.srcAddr[i];
    }
    pos += addrLen;
    
    /* FCtrl */
    if (pos + 1 > maxLen) {
        TRM_LOG_ERROR("TRM: Buffer too small for FCtrl");
        return -1;
    }
    data[pos] = ((frame->payload.fhdr.fctrl.ack & 0x01) << 7) |
               ((frame->payload.fhdr.fctrl.foptsNum & 0x0F) << 3) |
               ((frame->payload.fhdr.fctrl.retranPkg & 0x01) << 2) |
               ((frame->payload.fhdr.fctrl.subPkg & 0x01) << 1) |
               ((frame->payload.fhdr.fctrl.nack & 0x01) << 0) |
               ((frame->payload.fhdr.fctrl.rfu & 0x01));
    pos++;
    
    /* FCnt */
    if (pos + 1 > maxLen) {
        TRM_LOG_ERROR("TRM: Buffer too small for FCnt");
        return -1;
    }
    data[pos] = frame->payload.fhdr.fcnt;
    pos++;
    
    /* SubPkg */
    if (frame->payload.fhdr.fctrl.subPkg) {
        if (pos + 1 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for SubPkg");
            return -1;
        }
        data[pos] = ((frame->payload.fhdr.subPkg.subPkgIdx & 0x0F) << 4) | 
                    (frame->payload.fhdr.subPkg.subPkgNum & 0x0F);
        pos++;
    }
    
    /* Retrans */
    if (frame->payload.fhdr.fctrl.retranPkg) {
        if (pos + 1 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for Retrans");
            return -1;
        }
        data[pos] = ((frame->payload.fhdr.retrans.retransIdx & 0x0F) << 4) | 
                    (frame->payload.fhdr.retrans.retransNum & 0x0F);
        pos++;
    }
    
    /* FOpts */
    uint8_t foptsLen = frame->payload.fhdr.fctrl.foptsNum;
    if (foptsLen > 0) {
        if (pos + foptsLen > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for FOpts");
            return -1;
        }
        for (uint8_t i = 0; i < foptsLen; i++) {
            data[pos + i] = frame->payload.fhdr.fopts[i];
        }
        pos += foptsLen;
    }
    
    /* 解析broadcastGroup字段 (WAN模式下的广播组标识) */
    if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN && foptsLen > 0) {
        /* 检查是否有足够的字节来解析broadcastGroup */
        if (pos + 1 <= maxLen) {
            /* broadcastGroup在FOpts的最后一个字节中 */
            data[pos] = frame->payload.fhdr.broadcastGroup;
            pos++;
        }
    }
    
    /* 功率控制 */
    if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN) {
        if (pos + 1 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for TxPowerIdx");
            return -1;
        }
        data[pos] = frame->payload.fhdr.txPowerIdx;
        pos++;
    } else {
        /* LAN模式：TxPowerIdx字段也存在，用于开环功控时填入发送功率 */
        if (pos + 1 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for TxPowerIdx");
            return -1;
        }
        data[pos] = frame->payload.fhdr.txPowerIdx;
        pos++;
    }
    
    /* FPort */
    if (pos + 1 > maxLen) {
        TRM_LOG_ERROR("TRM: Buffer too small for FPort");
        return -1;
    }
    data[pos] = frame->payload.fport;
    pos++;
    
    /* 负载长度 (WAN模式) */
    if (frame->mhdr.nwkMode == TRM_MAC_NWKMODE_WAN) {
        if (pos + 2 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for PayloadLen");
            return -1;
        }
        data[pos] = (frame->payload.payloadLen >> 8) & 0xFF;
        data[pos + 1] = frame->payload.payloadLen & 0xFF;
        pos += 2;
    }
    
    /* 负载数据 */
    if (frame->payload.payloadSize > 0) {
        if (pos + frame->payload.payloadSize > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for payload data");
            return -1;
        }
        for (uint16_t i = 0; i < frame->payload.payloadSize; i++) {
            data[pos + i] = frame->payload.payload[i];
        }
        pos += frame->payload.payloadSize;
    }
    
    /* MIC */
    if (frame->mhdr.securityMode != TRM_MAC_SECURITY_NONE && 
        frame->mhdr.securityMode != TRM_MAC_SECURITY_TEA_ONLY &&
        frame->mhdr.securityMode != TRM_MAC_SECURITY_AES_ONLY) {
        if (pos + 4 > maxLen) {
            TRM_LOG_ERROR("TRM: Buffer too small for MIC");
            return -1;
        }
        data[pos] = frame->mic[0];
        data[pos + 1] = frame->mic[1];
        data[pos + 2] = frame->mic[2];
        data[pos + 3] = frame->mic[3];
        pos += 4;
    }
    
    TRM_LOG_DEBUG("TRM: MAC frame built successfully - Total length=%d", pos);
    return pos;
}

/**
 * @brief 在广播帧中配置TDD周期
 * @param broadcastData 广播帧数据（直接修改）
 * @param dataLen 数据长度
 * @param tddPeriod 要配置的TDD周期值
 * @return 成功返回0，失败返回负数
 */
int TRM_ConfigureTddPeriodInBroadcast(uint8_t* broadcastData, uint16_t dataLen, uint8_t tddPeriod)
{
    if (broadcastData == NULL || dataLen < 4) {
        TRM_LOG_ERROR("TRM: Invalid parameters for TDD period configuration");
        return -1;
    }
    
    /* 检查最小广播帧长度：MHDR(3) + BroadcastGroup(13) + BrocastPayloadLen(1) */
    if (dataLen < 17) {
        TRM_LOG_ERROR("TRM: Broadcast frame too short for TDD configuration");
        return -1;
    }
    
    /* 配置TDD周期：在BroadcastGroup的第2字节（索引4）设置TDD周期 */
    /* 广播帧结构：MHDR(0-2) + BroadcastGroup(3-15) + BrocastPayloadLen(16) + BrocastPayload(17-) */
    /* BroadcastGroup的第2字节位置：3 + 1 = 4 */
    broadcastData[4] = tddPeriod;
    
    TRM_LOG_DEBUG("TRM: TDD period configured in broadcast frame - TDD=%d, FrameLen=%d", 
                  tddPeriod, dataLen);
    
    return 0;
}
