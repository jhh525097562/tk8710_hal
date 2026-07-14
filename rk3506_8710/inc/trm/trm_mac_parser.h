/**
 * @file trm_mac_parser.h
 * @brief TRM MAC协议帧解析头文件
 * @note 此文件包含TurMass™ Link MAC协议帧的结构体定义和解析函数
 */

#ifndef TRM_MAC_PARSER_H
#define TRM_MAC_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "../driver/tk8710_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * MAC协议帧结构体定义 (参考协议规范第6章)
 * ============================================================================= */

/* MHDR - MAC帧头 (3字节) */
TK8710_PACKED_BEGIN
typedef struct TK8710_PACKED {
    /* 第一个字节 */
    uint8_t frameType : 4;      /* 帧类型 (0000-0110) */
    uint8_t devType   : 2;      /* 设备类型 (00=终端,01=多通道网关,10=多天线网关,11=中继) */
    uint8_t version   : 2;      /* 协议版本 (00=MAC 3.0) */
    
    /* 第二个字节 */
    uint8_t qosPri    : 2;      /* QoS优先级 (0-3) */
    uint8_t qosTtl    : 2;      /* QoS生存时间等级 (0-3) */
    uint8_t hopNum    : 2;      /* 最大跳数 (0-3) */
    uint8_t hopCnt    : 2;      /* 当前跳数/设备层级 (0-3) */
    
    /* 第三个字节 */
    uint8_t securityMode : 3;   /* 安全模式 (000-101) */
    uint8_t addrMode      : 1;  /* 地址模式 (0=Short,1=Long) */
    uint8_t nwkMode       : 2;  /* 网络模式 (00=LAN,01=WAN,10=Satellite,11=Mesh) */
    uint8_t powerCtrlType : 1;  /* 功控类型 (0=开环,1=闭环) */
    uint8_t rfu           : 1;  /* 保留位 */
} TrmMacMhdr;

/* FCtrl - 帧控制字段 (1字节) */
typedef struct TK8710_PACKED {
    uint8_t ack       : 1;      /* 确认标识 (0=非确认,1=确认) */
    uint8_t foptsNum  : 4;      /* 扩展信息数量 (0-15) */
    uint8_t nack      : 1;      /* NACK标识 (0=无NACK,1=有NACK) */
    uint8_t retranPkg : 1;      /* 重传域存在标识 */
    uint8_t subPkg    : 1;      /* 分包域存在标识 */
    uint8_t rfu       : 1;      /* 预留位 */
} TrmMacFCtrl;

/* SubPkg Field - 分包域 (1字节) */
typedef struct TK8710_PACKED {
    uint8_t subPkgIdx : 4;      /* 当前分包序号 (1-15) */
    uint8_t subPkgNum : 4;      /* 总分包数 (1-15) */
} TrmMacSubPkgField;

/* Retrans Field - 重传域 (1字节) */
typedef struct TK8710_PACKED {
    uint8_t retransIdx : 4;     /* 当前重传序号 (0-15) */
    uint8_t retransNum : 4;     /* 总重传数 (0-15) */
} TrmMacRetransField;

/* OptsCtrl - 扩展信息控制字段 (2字节) */
typedef struct TK8710_PACKED {
    uint16_t optsType : 7;      /* 扩展信息类型 */
    uint16_t optsLen  : 9;      /* 扩展信息长度 */
} TrmMacOptsCtrl;

/* WakeUpCtrl - 唤醒控制字段 (2字节) */
typedef struct TK8710_PACKED {
    uint16_t wakeUpMode : 4;     /* 唤醒模式 (0-3) */
    uint16_t wakeUpId   : 10;    /* 唤醒ID */
    uint16_t rfu        : 2;     /* 保留位 */
} TrmMacWakeUpCtrl;
TK8710_PACKED_END

/* FHDR - 帧头 (6-27字节) */
typedef struct {
    uint8_t  srcAddr[4];         /* 源地址 (2或4字节) */
    uint8_t  dstAddr[4];         /* 目标地址 (0或4字节) */
    TrmMacFCtrl fctrl;              /* 帧控制 */
    uint8_t  fcnt;               /* 帧序号 */
    TrmMacSubPkgField subPkg;       /* 分包域 (可选) */
    TrmMacRetransField retrans;     /* 重传域 (可选) */
    uint8_t  fopts[15];          /* 扩展域 (0-15字节) */
    uint8_t  txPowerIdx;         /* 发送功率索引 (闭环功控) */
    uint8_t  nack;               /* NACK标识 (0=无NACK,1=有NACK) */
    uint8_t  broadcastGroup;      /* 广播组标识 (WAN模式) */
} TrmMacFhdr;

/* MACPayload - MAC负载 (变长) */
typedef struct {
    TrmMacFhdr fhdr;                /* 帧头 */
    uint8_t  fport;              /* 端口 */
    uint16_t payloadLen;         /* 负载长度 (WAN模式) */
    uint8_t* payload;            /* 负载数据指针 */
    uint16_t payloadSize;        /* 负载数据大小 */
} TrmMacPayload;

/* MAC协议帧结构 */
typedef struct {
    TrmMacMhdr mhdr;                /* MAC帧头 */
    TrmMacPayload payload;          /* MAC负载 */
    uint8_t mic[4];              /* MIC校验 (4字节) */
} TrmMacFrame;

/* Join Request帧结构 */
typedef struct {
    uint8_t  capacity;           /* 设备能力 */
    uint8_t  powerClass : 2;      /* 功率等级 (00=正常,01=+4dB,10=+8dB,11=+12dB) */
    uint8_t  devEui[8];          /* 设备唯一标识号 */
    uint16_t devNonce;           /* 入网随机数 */
} TrmMacJoinRequest;

/* Join Accept帧结构 */
typedef struct {
    uint32_t devAddr;            /* 设备地址 */
    uint8_t  dlSettings;          /* 下行设置 */
    uint8_t  rxDelay;             /* 接收延迟 */
    uint8_t  cfList[16];          /* 通道频率列表 */
    uint8_t  wrsbGroup;           /* 时隙资源块组，LAN：0-30，WAN：7 */
    uint8_t  gwId[8];             /* 网关唯一标识号，仅LAN有 */
    uint8_t  wakeUpCfg[8];         /* 低功耗设备携带，标识当前休眠唤醒配置信息，仅LAN有 */
} TrmMacJoinAccept;

/* =============================================================================
 * MAC协议字段偏移和掩码定义
 * ============================================================================= */

/* MHDR字节偏移 */
#define TRM_MHDR_BYTE1_OFFSET       0     /* MHDR第一个字节偏移 */
#define TRM_MHDR_BYTE2_OFFSET       1     /* MHDR第二个字节偏移 */
#define TRM_MHDR_BYTE3_OFFSET       2     /* MHDR第三个字节偏移 */

/* MHDR第一个字节字段定义 */
#define TRM_MHDR_FRAMETYPE_SHIFT    4     /* FrameType位偏移 (bit 7:4) */
#define TRM_MHDR_FRAMETYPE_MASK     0x0F  /* FrameType掩码 (4位) */
#define TRM_MHDR_DEVTYPE_SHIFT      2     /* DevType位偏移 (bit 3:2) */
#define TRM_MHDR_DEVTYPE_MASK       0x03  /* DevType掩码 (2位) */
#define TRM_MHDR_VERSION_SHIFT      0     /* Version位偏移 (bit 1:0) */
#define TRM_MHDR_VERSION_MASK       0x03  /* Version掩码 (2位) */

/* MHDR第二个字节字段定义 (QoS相关) */
#define TRM_MHDR_QOSPRI_SHIFT       6     /* QosPri位偏移 (bit 7:6) */
#define TRM_MHDR_QOSPRI_MASK        0xC0  /* QosPri掩码 (2位) */
#define TRM_MHDR_QOSTTL_SHIFT       4     /* QosTTL位偏移 (bit 5:4) */
#define TRM_MHDR_QOSTTL_MASK        0x30  /* QosTTL掩码 (2位) */
#define TRM_MHDR_HOPNUM_SHIFT       2     /* HopNum位偏移 (bit 3:2) */
#define TRM_MHDR_HOPNUM_MASK_LAN     0x0F  /* HopNum掩码 (4位) - LAN模式 */
#define TRM_MHDR_HOPNUM_MASK_WAN     0x03  /* HopNum掩码 (2位) - WAN模式 */
#define TRM_MHDR_HOPCNT_SHIFT       0     /* HopCnt位偏移 (bit 1:0) */
#define TRM_MHDR_HOPCNT_MASK_LAN     0x0F  /* HopCnt掩码 (4位) - LAN模式 */
#define TRM_MHDR_HOPCNT_MASK_WAN     0x03  /* HopCnt掩码 (2位) - WAN模式 */

/* MHDR第三个字节字段定义 */
#define TRM_MHDR_SECURITYMODE_SHIFT 5     /* SecurityMode位偏移 (bit 7:5) */
#define TRM_MHDR_SECURITYMODE_MASK  0xE0 /* SecurityMode掩码 (3位) */
#define TRM_MHDR_ADDRMODE_SHIFT     4     /* AddrMode位偏移 (bit 4) */
#define TRM_MHDR_ADDRMODE_MASK      0x10  /* AddrMode掩码 (1位) */
#define TRM_MHDR_NWKMODE_SHIFT      2     /* NwkMode位偏移 (bit 3:2) */
#define TRM_MHDR_NWKMODE_MASK       0x0C  /* NwkMode掩码 (2位) */
#define TRM_MHDR_POWERCTRL_SHIFT    1     /* PowerControlType位偏移 (bit 1) */
#define TRM_MHDR_POWERCTRL_MASK     0x02  /* PowerControlType掩码 (1位) */
#define TRM_MHDR_RFU_SHIFT          0     /* RFU位偏移 (bit 0) */
#define TRM_MHDR_RFU_MASK           0x01  /* RFU掩码 (1位) */

/* 帧类型定义 */
#define TRM_MAC_FRAMETYPE_JOIN_REQUEST     0x00  /* Join Request */
#define TRM_MAC_FRAMETYPE_JOIN_ACCEPT      0x01  /* Join Accept */
#define TRM_MAC_FRAMETYPE_DISASSOCIATION   0x02  /* Disassociation */
#define TRM_MAC_FRAMETYPE_CONFIRM_DATA     0x03  /* Confirm Data Up/Down */
#define TRM_MAC_FRAMETYPE_UNCONFIRM_DATA   0x04  /* UnConfirm Data Up/Down */
#define TRM_MAC_FRAMETYPE_BROADCAST        0x05  /* Broadcast */
#define TRM_MAC_FRAMETYPE_MAC_COMMAND      0x06  /* MAC Command */

/* 设备类型定义 */
#define TRM_MAC_DEVTYPE_TERMINAL           0x00  /* 终端 */
#define TRM_MAC_DEVTYPE_MULTI_CHANNEL_GW   0x01  /* 多通道网关 */
#define TRM_MAC_DEVTYPE_MULTI_ANTENNA_GW   0x02  /* 多天线网关 */
#define TRM_MAC_DEVTYPE_RELAY              0x03  /* 中继 */

/* 安全模式定义 */
#define TRM_MAC_SECURITY_NONE              0x00  /* 不带MIC且不加密 */
#define TRM_MAC_SECURITY_MIC_ONLY          0x01  /* MIC校验且不加密 */
#define TRM_MAC_SECURITY_TEA_ONLY          0x02  /* 不带MIC且TEA加密 */
#define TRM_MAC_SECURITY_MIC_TEA           0x03  /* MIC校验且TEA加密 */
#define TRM_MAC_SECURITY_AES_ONLY          0x04  /* 不带MIC且AES128加密 */
#define TRM_MAC_SECURITY_MIC_AES           0x05  /* MIC校验且AES128加密 */

/* 网络模式定义 */
#define TRM_MAC_NWKMODE_LAN                0x00  /* LAN Network */
#define TRM_MAC_NWKMODE_WAN                0x01  /* WAN Network */
#define TRM_MAC_NWKMODE_SATELLITE          0x02  /* Satellite Network */
#define TRM_MAC_NWKMODE_MESH               0x03  /* Mesh Network */

/* FPort端口定义 */
#define TRM_MAC_FPORT_APPLICATION_MIN     0     /* 应用端口最小值 */
#define TRM_MAC_FPORT_APPLICATION_MAX     201   /* 应用端口最大值 */
#define TRM_MAC_FPORT_TIME_SYNC          202   /* 时间同步端口 */
#define TRM_MAC_FPORT_MAC_TEST           224   /* MAC协议测试模式端口 */

/* =============================================================================
 * MAC协议帧解析辅助函数声明
 * ============================================================================= */

/**
 * @brief 解析MAC帧头MHDR
 * @param data 原始数据指针
 * @param len 数据长度
 * @param mhdr 输出的MHDR结构体
 * @return 成功返回0，失败返回负数
 */
int TRM_ParseMacMhdr(const uint8_t* data, uint16_t len, TrmMacMhdr* mhdr);

/**
 * @brief 从MAC帧中提取用户ID
 * @param data MAC帧数据指针
 * @param len 数据长度
 * @param userId 输出的用户ID
 * @return 成功返回0，失败返回负数
 */
int TRM_ExtractUserIdFromMacFrame(const uint8_t* data, uint16_t len, uint32_t* userId);

/**
 * @brief 获取MAC帧的QoS信息
 * @param data MAC帧数据指针
 * @param len 数据长度
 * @param qosPri 输出的QoS优先级
 * @param qosTtl 输出的QoS生存时间
 * @return 成功返回0，失败返回负数
 */
int TRM_GetMacFrameQosInfo(const uint8_t* data, uint16_t len, uint8_t* qosPri, uint8_t* qosTtl);

/**
 * @brief 构建MAC帧头MHDR
 * @param mhdr MHDR结构体
 * @param data 输出的数据缓冲区
 * @param maxLen 缓冲区最大长度
 * @return 成功返回写入的字节数，失败返回负数
 */
int TRM_BuildMacMhdr(const TrmMacMhdr* mhdr, uint8_t* data, uint16_t maxLen);

/**
 * @brief 检查MAC帧类型
 * @param data MAC帧数据指针
 * @param len 数据长度
 * @param frameType 输出的帧类型
 * @return 成功返回0，失败返回负数
 */
int TRM_GetMacFrameType(const uint8_t* data, uint16_t len, uint8_t* frameType);

/**
 * @brief 解析完整的MAC帧
 * @param data MAC帧数据指针
 * @param len 数据长度
 * @param frame 输出的MAC帧结构体
 * @return 成功返回0，失败返回负数
 */
int TRM_ParseMacFrame(const uint8_t* data, uint16_t len, TrmMacFrame* frame);

/**
 * @brief 构建MAC帧
 * @param frame MAC帧结构体
 * @param data 输出的数据缓冲区
 * @param maxLen 缓冲区最大长度
 * @return 成功返回构建的帧长度，失败返回负数
 */
int TRM_BuildMacFrame(const TrmMacFrame* frame, uint8_t* data, uint16_t maxLen);

/**
 * @brief 在广播帧中配置TDD周期
 * @param broadcastData 广播帧数据（直接修改）
 * @param dataLen 数据长度
 * @param tddPeriod 要配置的TDD周期值
 * @return 成功返回0，失败返回负数
 */
int TRM_ConfigureTddPeriodInBroadcast(uint8_t* broadcastData, uint16_t dataLen, uint8_t tddPeriod);

#ifdef __cplusplus
}
#endif

#endif /* TRM_MAC_PARSER_H */
