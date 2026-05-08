/**
 * @file trm_api.h
 * @brief TRM (Transmission Resource Management) 公共API头文件
 * @note 此文件包含应用层可调用的TRM API接口函数
 */

#ifndef TRM_API_H
#define TRM_API_H

#include <stdint.h>
#include <stddef.h>
#include "../driver/tk8710_types.h"
#include "trm_log.h"
#include "trm_mac_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 错误码定义
 * ============================================================================= */
#define TRM_OK                  0
#define TRM_ERR_PARAM           (-1)
#define TRM_ERR_STATE           (-2)
#define TRM_ERR_TIMEOUT         (-3)
#define TRM_ERR_NO_MEM          (-4)
#define TRM_ERR_NOT_INIT        (-5)
#define TRM_ERR_QUEUE_FULL      (-6)
#define TRM_ERR_NO_BEAM         (-7)
#define TRM_ERR_DRIVER          (-8)

/* =============================================================================
 * 常量定义
 * ============================================================================= */
#define TRM_BEAM_MAX_USERS_DEFAULT  3000    /* 默认最大用户数 */
#define TRM_BEAM_TIMEOUT_DEFAULT    10000    /* 默认波束超时时间(ms) */

/* =============================================================================
 * 类型定义
 * ============================================================================= */

/* 波束存储模式 */
typedef enum {
    TRM_BEAM_MODE_FULL_STORE = 0,   /* 完整波束存储模式(CPU RAM) */
    TRM_BEAM_MODE_MAPPING    = 1,   /* 波束映射模式(8710 RAM) */
} TRM_BeamMode;

/* 发送结果 */
typedef enum {
    TRM_TX_OK = 0,
    TRM_TX_NO_BEAM,
    TRM_TX_TIMEOUT,
    TRM_TX_ERROR,
} TRM_TxResult;

/* TRM状态 */
typedef enum {
    TRM_STATE_UNINIT = 0,
    TRM_STATE_INIT,
} TrmState;

/* 速率模式 */
typedef enum {
    TRM_RATE_2M = 0,
    TRM_RATE_4M = 1,
    TRM_RATE_8M = 2,
} TRM_RateMode;

/* 波束信息 */
typedef struct {
    uint32_t userId;            /* 用户ID */
    uint32_t freq;              /* 频率 (26位格式) */
    uint32_t ahData[16];        /* AH数据: 8天线×2(I/Q) */
    uint64_t pilotPower;        /* Pilot功率 */
    uint32_t timestamp;         /* 更新时间戳*/
    uint8_t  valid;             /* 有效标志 */
} TRM_BeamInfo;

/* 接收用户数据 */
typedef struct {
    uint32_t userId;            /* 用户ID */
    uint16_t dataLen;          /* 数据长度 */
    uint8_t  rateMode;          /* 接收速率模式 */
    int16_t  rssi;              /* 信号强度 */
    uint8_t  snr;               /* 信噪比 */
    uint8_t  reserved;
    uint8_t* data;              /* 数据指针 */
    int32_t  freq;              /* 频率 */
    uint32_t timestamp;         /* 更新时间戳*/
    uint8_t  valid;             /* 有效标志 */
    TRM_BeamInfo beam;          /* 波束信息 */
} TRM_RxUserData;

/* 接收数据列表 */
typedef struct {
    uint8_t  userCount;         /* 用户数量 */
    uint16_t reserved;
    uint32_t frameNo;           /* 帧号 */
    TRM_RxUserData* users;      /* 用户数据数组 */
} TRM_RxDataList;

/* 统计信息 */
typedef struct {
    TrmState    state;             /* TRM运行状态 */
    uint32_t    txCount;           /* 发送次数 */
    uint32_t    txSuccessCount;    /* 发送成功次数 */
    uint32_t    rxCount;           /* 接收次数 */
    uint32_t    beamCount;         /* 当前波束数量 */
    uint32_t    memAllocCount;     /* 内存分配次数 */
    uint32_t    memFreeCount;      /* 内存释放次数 */
    uint32_t    txQueueRemaining;   /* 剩余发送队列数量 */
} TRM_Stats;

/* 时隙计算器输入参数 */
typedef struct {
    uint8_t  rateMode;       /**< 速率模式: 5-11, 18 */
    uint8_t  brdBlockNum;    /**< slot1包块数（广播） */
    uint8_t  ulBlockNum;     /**< 上行包块数 */
    uint8_t  dlBlockNum;     /**< 下行包块数 */
    uint8_t  superFrameNum;  /**< 超帧数 */
    uint8_t  minGapPos[4];   /**< minGap位置指示: [0]=BCN, [1]=BRD, [2]=UL, [3]=DL */
} TRM_SlotCalcInput;

/* 时隙计算器输出结果 */
typedef struct {
    uint32_t bcnSlotLen;     /**< BCN时隙长度(us) */
    uint32_t brdSlotLen;     /**< 广播时隙长度(us) */
    uint32_t ulSlotLen;      /**< 上行时隙长度(us) */
    uint32_t dlSlotLen;      /**< 下行时隙长度(us) */
    uint32_t bcnGap;         /**< BCN间隔(us) */
    uint32_t brdGap;         /**< 广播间隔(us) */
    uint32_t ulGap;          /**< 上行间隔(us) */
    uint32_t dlGap;          /**< 下行间隔(us)，用于调整帧周期 */
    uint32_t framePeriod;    /**< 调整后帧周期(us) */
    uint32_t frameCount;     /**< 帧数(framePeriod * frameCount = 1s的倍数) */
} TRM_SlotCalcOutput;

/* =============================================================================
 * TRM上层回调接口类型定义
 * ============================================================================= */

/**
 * @brief 接收数据回调函数类型
 * @param rxDataList 接收数据列表
 */
typedef void (*TRM_OnRxData)(const TRM_RxDataList* rxDataList);

/* 单个用户发送结果 */
typedef struct {
    uint32_t userId;         /* 用户ID */
    TRM_TxResult result;     /* 发送结果 */
} TRM_TxUserResult;

/* 发送完成回调结果 */
typedef struct {
    uint32_t totalUsers;           /* 发送用户总数 */
    uint8_t  superFrameNo;         /* 当前超帧号 */
    uint32_t remainingQueue;        /* 剩余发送队列数量 */
    uint32_t userCount;             /* 结果数组中的用户数量 */
    const TRM_TxUserResult* users;  /* 用户结果数组指针 */
} TRM_TxCompleteResult;

/**
 * @brief 发送完成回调函数类型
 * @param txResult 发送完成结果
 */
typedef void (*TRM_OnTxComplete)(const TRM_TxCompleteResult* txResult);

/* 初始化配置 */
typedef struct {
    /* 波束配置 */
    TRM_BeamMode beamMode;          /* 波束存储模式 */
    uint32_t     beamMaxUsers;      /* 最大用户数(使用默认?) */
    uint32_t     beamTimeoutMs;     /* 波束超时时间戳使用默认?) */
    
    /* 帧管理配置 */
    uint32_t     maxFrameCount;     /* 最大帧数 */
    
    /* 回调函数 */
    struct {
        TRM_OnRxData      onRxData;
        TRM_OnTxComplete  onTxComplete;
    } callbacks;
    
    /* 平台配置(预留) */
    void* platformConfig;
} TRM_InitConfig;

/* =============================================================================
 * 系统初始化与控制API
 * ============================================================================= */

/**
 * @brief 初始化TRM系统
 * @param config 初始化配置参数
 * @return TRM_OK成功，其他失败
 */
int TRM_Init(const TRM_InitConfig* config);

/**
 * @brief 清理TRM系统资源
 * @return TRM_OK成功，其他失败
 */
int TRM_Deinit(void);

/* =============================================================================
 * 数据发送API
 * ============================================================================= */

/**
 * @brief 统一发送数据接口（支持用户数据和广播数据）
 * @param downlinkType 下行位置 (TK8710_DOWNLINK_A=slot1发送, TK8710_DOWNLINK_B=slot3)
 * @param userId_brdIndex 用户ID或广播索引
 * @param data 数据指针
 * @param len 数据长度
 * @param txPower 发送功率
 * @param frameNo 帧号 (仅用户数据使用，广播时忽略)
 * @param targetRateMode 目标速率模式 (仅用户数据使用，广播时忽略)
 * @param BeamType 波束类型 (0=广播波束, 1=指定波束)
 * @return TRM_OK成功，其他失败
 */
int TRM_SetTxData(TK8710DownlinkType downlinkType, uint32_t userId_brdIndex, const uint8_t* data, uint16_t len, uint8_t txPower, uint32_t frameNo, uint8_t targetRateMode, uint8_t BeamType);

/* =============================================================================
 * 波束获取API
 * ============================================================================= */

/**
 * @brief 获取用户波束信息
 * @param userId 用户ID
 * @param beamInfo 波束信息输出指针
 * @return TRM_OK成功，其他失败
 */
int TRM_GetBeamInfo(uint32_t userId, TRM_BeamInfo* beamInfo);

/* =============================================================================
 * 状态查询API
 * ============================================================================= */

/**
 * @brief 获取TRM统计信息
 * @param stats 统计信息输出
 * @return TRM_OK成功，其他失败
 */
int TRM_GetStats(TRM_Stats* stats);

/**
 * @brief 获取当前系统帧号
 * @return 当前系统帧号
 */
uint32_t TRM_GetCurrentFrame(void);

/* =============================================================================
 * TRM日志系统API
 * =============================================================================
 */

/**
 * @brief 配置TRM日志系统
 * @param level 日志级别
 * @param enable_file_logging 是否启用文件日志
 * @return TRM_OK成功，其他失败
 */
int TRM_LogConfig(TRMLogLevel level, uint8_t enable_file_logging);

/* =============================================================================
 * 时隙计算API
 * ============================================================================= */

/**
 * @brief 计算时隙配置参数
 * @param input 输入参数
 * @param output 输出结果
 * @return 0-成功, 非0-失败
 * @note 基于8710_HAL用户指南v1.0 7.2.4章节实现
 */
int trm_calc_slot_config(const TRM_SlotCalcInput* input, TRM_SlotCalcOutput* output);

#ifdef __cplusplus
}
#endif

#endif /* TRM_API_H */
