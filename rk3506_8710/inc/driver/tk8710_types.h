/**
 * @file tk8710_types.h
 * @brief TK8710 数据类型定义
 */

#ifndef TK8710_TYPES_H
#define TK8710_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include "tk8710_platform.h"

/* SPI 配置结构体 */
typedef struct {
    uint32_t speed;         /* SPI时钟频率 */
    uint8_t  mode;          /* SPI模式 (0-3) */
    uint8_t  bits;          /* 数据位宽 */
    uint8_t  lsb_first;     /* 0=MSB优先, 1=LSB优先 */
    uint8_t  cs_pin;        /* CS引脚号 */
} SpiConfig;

#ifdef __cplusplus
extern "C" {
#endif

/* 返回值定义 */
#define TK8710_OK           0
#define TK8710_ERR          1
#define TK8710_ERR_PARAM    2
#define TK8710_ERR_STATE    3
#define TK8710_TIMEOUT      4

/* 最大用户数定义 */
#define TK8710_MAX_DATA_USERS   128
#define TK8710_MAX_BRD_USERS    16
#define TK8710_MAX_ANTENNAS     8

/* ACM校准因子结构体 */
typedef struct {
    uint32_t i_factor;  /* I路校准因子 */
    uint32_t q_factor;  /* Q路校准因子 */
} AcmCalibrationFactor;

/* ACM校准因子集合结构体 */
typedef struct {
    AcmCalibrationFactor channels[TK8710_MAX_ANTENNAS];  /* 8个射频通道的校准因子 */
} AcmCalibrationFactors;

/* 速率模式枚举 */
typedef enum {
    TK8710_RATE_MODE_5  = 5,
    TK8710_RATE_MODE_6  = 6,
    TK8710_RATE_MODE_7  = 7,
    TK8710_RATE_MODE_8  = 8,
    TK8710_RATE_MODE_9  = 9,
    TK8710_RATE_MODE_10 = 10,
    TK8710_RATE_MODE_11 = 11,
    TK8710_RATE_MODE_18 = 18,
} rateMode_e;

/* 速率模式参数结构体 */
typedef struct {
    uint8_t  mode;           /* 模式编号 */
    uint8_t  signalBwKHz;    /* 信号带宽 (KHz): 2/4/8/16/32/64/128 */
    uint32_t systemBwKHz;    /* 系统带宽 (KHz): */
    uint8_t  maxUsers;       /* 最大并发用户数: 128/64/32/16 */
    uint16_t maxPayloadLen;  /* 最大载荷长度: mode5-8=260, mode9/10/11/18=520 */
} RateModeParams;

/* 速率模式参数查找表 (定义在tk8710_core.c中) */
/* 模式 | 信号带宽(KHz) | 系统带宽(KHz) | 最大用户数 */
/*  5   |      2        |     62.5      |    128     */
/*  6   |      4        |     125       |    128     */
/*  7   |      8        |     250       |    128     */
/*  8   |     16        |     500       |    128     */
/*  9   |     32        |     500       |     64     */
/* 10   |     64        |     500       |     32     */
/* 11   |    128        |     500       |     16     */
/* 18   |    128        |     500       |     16     */

/* 主从模式枚举 */
typedef enum {
    TK8710_MODE_SLAVE    = 0,
    TK8710_MODE_MASTER   = 1,
    TK8710_MODE_LOOPBACK = 2,
} msMode_e;

/* 工作模式枚举 */
typedef enum {
    TK8710_WORK_MODE_CONTINUOUS = 1,
    TK8710_WORK_MODE_SINGLE     = 2,
} workMode_e;

/* 复位类型枚举 */
typedef enum {
    TK8710_RST_STATE_MACHINE = 1,
    TK8710_RST_ALL           = 2,
} rstType_e;

/* 射频类型枚举 */
typedef enum {
    TK8710_RF_TYPE_1255_1M  = 0,
    TK8710_RF_TYPE_1255_32M = 1,
    TK8710_RF_TYPE_1257_32M = 2,
    TK8710_RF_TYPE_OTHER    = 3,
} rfType_e;

/* 中断类型枚举 */
typedef enum {
    TK8710_IRQ_RX_BCN       = 0,
    TK8710_IRQ_BRD_UD       = 1,
    TK8710_IRQ_BRD_DATA     = 2,
    TK8710_IRQ_MD_UD        = 3,
    TK8710_IRQ_MD_DATA      = 4,
    TK8710_IRQ_S0           = 5,
    TK8710_IRQ_S1           = 6,
    TK8710_IRQ_S2           = 7,
    TK8710_IRQ_S3           = 8,
    TK8710_IRQ_ACM          = 9,
} irqType_e;

/* SPI SetInfo类型枚举 (CMD=0x06) */
typedef enum {
    TK8710_SET_INFO_TX_FREQ     = 0,  /* 指定128个用户TX频点 */
    TK8710_SET_INFO_AH          = 1,  /* 指定128个用户AH */
    TK8710_SET_INFO_ANOISE      = 2,  /* 指定8天线的anoise */
    TK8710_SET_INFO_PILOT_POW   = 3,  /* 指定128个用户pilot pow */
} setInfoType_e;

/* SPI GetInfo类型枚举 (CMD=0x07) */
typedef enum {
    TK8710_GET_INFO_FREQ        = 0,  /* 获取128个用户频点 */
    TK8710_GET_INFO_AH          = 1,  /* 获取128个用户AH */
    TK8710_GET_INFO_ANOISE      = 2,  /* 获取8天线anoise */
    TK8710_GET_INFO_PILOT_POW   = 3,  /* 获取128个用户pilot pow */
    TK8710_GET_INFO_FFT_OUT_0   = 4,  /* 天线0 fft out */
    TK8710_GET_INFO_FFT_OUT_1   = 5,  /* 天线1 fft out */
    TK8710_GET_INFO_FFT_OUT_2   = 6,  /* 天线2 fft out */
    TK8710_GET_INFO_FFT_OUT_3   = 7,  /* 天线3 fft out */
    TK8710_GET_INFO_FFT_OUT_4   = 8,  /* 天线4 fft out */
    TK8710_GET_INFO_FFT_OUT_5   = 9,  /* 天线5 fft out */
    TK8710_GET_INFO_FFT_OUT_6   = 10, /* 天线6 fft out */
    TK8710_GET_INFO_FFT_OUT_7   = 11, /* 天线7 fft out */
    TK8710_GET_INFO_CAPTURE_0   = 12, /* 天线0 capture data */
    TK8710_GET_INFO_CAPTURE_1   = 13, /* 天线1 capture data */
    TK8710_GET_INFO_CAPTURE_2   = 14, /* 天线2 capture data */
    TK8710_GET_INFO_CAPTURE_3   = 15, /* 天线3 capture data */
    TK8710_GET_INFO_CAPTURE_4   = 16, /* 天线4 capture data */
    TK8710_GET_INFO_CAPTURE_5   = 17, /* 天线5 capture data */
    TK8710_GET_INFO_CAPTURE_6   = 18, /* 天线6 capture data */
    TK8710_GET_INFO_CAPTURE_7   = 19, /* 天线7 capture data */
    TK8710_GET_INFO_SNR_RSSI    = 20, /* 获取128个数据用户和16个广播用户的SNR和RSSI */
} getInfoType_e;

/* 配置类型枚举 */
typedef enum {
    TK8710_CFG_TYPE_CHIP_INFO,
    TK8710_CFG_TYPE_SLOT_CFG,
    TK8710_CFG_TYPE_ADDTL,
} TK8710ConfigType;

/* 控制类型枚举 */
typedef enum {
    TK8710_CTRL_TYPE_ACM_START,
    TK8710_CTRL_TYPE_ACM_CALIBRATE_ONLY,
    TK8710_CTRL_TYPE_SEND_WAKEUP,
} TK8710CtrlType;

/* 调试控制类型枚举 */
typedef enum {
    TK8710_DBG_TYPE_FFT_OUT,
    TK8710_DBG_TYPE_CAPTURE_DATA,
    TK8710_DBG_TYPE_ACM_CAL_FACTOR,
    TK8710_DBG_TYPE_ACM_SNR,
    TK8710_DBG_TYPE_ACM_AUTO_GAIN,  /* 校准信号增益自动获取 */
    TK8710_DBG_TYPE_ACM_CALIBRATE,   /* 校准 */
    TK8710_DBG_TYPE_TX_TONE,
    TK8710_DBG_TYPE_REG_RW,  /* 读写寄存器测试类型 */
    
} TK8710DebugCtrlType;

/* ACM校准参数结构体 */
typedef struct {
    uint8_t calibCount;    /* 校准次数 */
    uint8_t snrThreshold;  /* SNR门限值 */
} AcmCalibParams;

/* 调试操作类型枚举 */
typedef enum {
    TK8710_DBG_OPT_SET = 0,
    TK8710_DBG_OPT_GET = 1,
    TK8710_DBG_OPT_EXE = 2,
} CtrlOptType;

/* 唤醒模式枚举 */
typedef enum {
    TK8710_WAKEUP_MODE_1 = 1,
    TK8710_WAKEUP_MODE_2 = 2,
    TK8710_WAKEUP_MODE_3 = 3,
} wakeUpMode_e;

/* 芯片初始化配置结构体 */
typedef struct {
    uint32_t bcn_agc;       /* BCN AGC长度, 默认32 */
    uint32_t interval;      /* Intval长度, 默认32 */
    uint8_t  tx_dly;        /* 下行发送时使用前几个上行ram中的信息, 默认1 */
    uint8_t  tx_fix_info;   /* TX是否固定频点, 默认0 */
    int32_t  offset_adj;    /* BCN sync窗口微调, 默认0, 单位us */
    int32_t  tx_pre;        /* 发送数据窗口调整, 默认0, 单位us */
    uint8_t  conti_mode;    /* 连续工作模式使能, 默认1 */
    uint8_t  bcn_scan;      /* BCN SCAN模式使能, 默认0 */
    uint8_t  ant_en;        /* 天线使能, 默认0xFF */
    uint8_t  rf_sel;        /* 射频使能, 默认0xFF */
    uint8_t  tx_bcn_en;     /* 发送BCN使能, 默认1 */
    uint8_t  ts_sync;       /* 本地同步, 默认0 */
    uint8_t  rf_model;      /* 射频芯片型号: 1=SX1255, 2=SX1257 */
    uint8_t  bcnbits;       /* 信标标识位, 共5bit */
    uint32_t anoiseThe1;    /* 用户检测anoiseTh1门限, 默认0 */
    uint32_t power2rssi;    /* RSSI换算 */
    uint32_t irq_ctrl0;     /* 中断使能 */
    uint32_t irq_ctrl1;     /* 中断清理 */
    
    /* SPI配置 (可选, 为NULL时使用默认配置) */
    SpiConfig* spiConfig;   /* SPI接口配置, 为NULL时使用默认16MHz/Mode0 */
    
    /* 射频配置 (可选, 为NULL时不配置射频) */
    struct ChiprfConfig_s* rfConfig;  /* 射频初始化配置, 为NULL时跳过射频配置 */
} ChipConfig;

/* 射频发送直流配置 (每天线i/q两路) */
typedef struct {
    int16_t i;              /* I路直流, 16bit */
    int16_t q;              /* Q路直流, 16bit */
} TxAdcConfig;

/* 射频初始化配置结构体 */
typedef struct ChiprfConfig_s {
    rfType_e rftype;        /* 射频类型 */
    uint32_t Freq;          /* 射频中心频率 */
    uint8_t  rxgain;        /* 射频接收增益 */
    uint8_t  txgain;        /* 射频发送增益 */
    TxAdcConfig txadc[TK8710_MAX_ANTENNAS]; /* 射频发送直流, 8天线×(i,q)×16bit */
} ChiprfConfig;

/* MD_DATA中断CRC结果结构体 */
typedef struct {
    uint8_t  userIndex;     /* 用户索引 (0-127) */
    uint8_t  crcValid;      /* CRC有效性: 1=正确, 0=错误 */
    uint8_t  dataValid;     /* 数据有效性: 1=已读取, 0=未读取 */
    uint8_t  reserved;      /* 保留字段 */
} TK8710CrcResult;

/* 用户信息结构体 (用于MD_UD中断) */
typedef struct {
    uint32_t freq;        /* 用户频率 */
    uint32_t pilotPower;  /* Pilot功率 */
    uint32_t ahData[16];  /* AH数据: 8天线 × 2(I/Q) = 16个uint32_t */
    uint8_t  valid;       /* 数据有效性标志 */
} TK8710UserInfo;

/* 自动发送数据结构体 */
typedef struct {
    uint8_t  userIndex;     /* 用户索引 (0-127) */
    uint8_t* data;         /* 用户数据指针 */
    uint16_t dataLen;      /* 数据长度 */
    uint8_t  valid;        /* 数据有效性: 1=有效, 0=无效 */
    uint8_t  txPower;      /* 发送功率 */
} TK8710TxUserData;

/* 广播发送数据结构体 */
typedef struct {
    uint8_t  brdIndex;     /* 广播用户索引 (0-15) */
    uint8_t* data;         /* 广播数据指针 */
    uint16_t dataLen;      /* 数据长度 */
    uint8_t  valid;        /* 数据有效性: 1=有效, 0=无效 */
    uint8_t  txPower;      /* 发送功率 */
} TK8710TxBrdData;

/* 中断结果结构体 */
typedef struct {
    irqType_e irq_type;             /* 中断类型 */
    int32_t   bcn_freq_offset;      /* BCN offset */
    uint8_t   rx_bcnbits;           /* 接收BCN bit数 */
    uint8_t   rxbcn_status;         /* 接收BCN状态 */
    
    /* MD_UD中断专用信息 */
    uint8_t  mdUserDataValid; /* MD_UD用户波束信息有效性 */
    
    /* BRD_UD中断专用信息 （8710做为slave时，接收BRD_DATA）*/
    uint8_t  brdUserDataValid; /* BRD_UD用户波束信息有效性 */

    /* MD_DATA中断专用信息 */
    uint8_t  mdDataValid;         /* MD_DATA数据有效性 */
    uint8_t  crcValidCount;       /* CRC正确的用户数量 */
    uint8_t  crcErrorCount;       /* CRC错误的用户数量 */
    uint8_t  maxUsers;            /* 当前速率模式最大用户数 */
    TK8710CrcResult crcResults[128]; /* CRC结果数组 (最多128个用户) */
 
    /* BRD_DATA中断专用信息（8710做为slave时，接收BRD_DATA） */
    uint8_t  brdDataValid;         /* BRD_DATA数据有效性 */
    uint8_t  brdCrcValidCount;       /* CRC正确的用户数量 */
    uint8_t  brdCrcErrorCount;       /* CRC错误的用户数量 */
    uint8_t  brdMaxUsers;            /* 当前速率模式最大用户数 */
    TK8710CrcResult brdCrcResults[16]; /* CRC结果数组 (最多16个用户) */

    /* S1时隙自动发送信息 */
    uint8_t  autoTxValid;         /* 自动发送数据有效性 */
    uint8_t  autoTxCount;         /* 自动发送用户数量 */
    
    /* 广播发送信息 */
    uint8_t  brdTxValid;          /* 广播发送数据有效性 */
    uint8_t  brdTxCount;          /* 广播发送用户数量 */
    
    /* 信号质量信息 */
    uint8_t  signalInfoValid;     /* 信号信息有效性 */
    uint8_t  currentRateIndex;    /* 当前速率序号 (0-based) */
    uint32_t ANoiseInfo[TK8710_MAX_ANTENNAS];           /* 8根天线的ANoise信息 */
} TK8710IrqResult;

/* 专用回调函数类型 */
typedef void (*TK8710RxDataCallback)(TK8710IrqResult* irqResult);
typedef void (*TK8710TxSlotCallback)(TK8710IrqResult* irqResult);
typedef void (*TK8710SlotEndCallback)(TK8710IrqResult* irqResult);
typedef void (*TK8710ErrorCallback)(TK8710IrqResult* irqResult);

/* Driver回调结构体 */
typedef struct {
    TK8710RxDataCallback onRxData;
    TK8710TxSlotCallback onTxSlot;
    TK8710SlotEndCallback onSlotEnd;
    TK8710ErrorCallback onError;
} TK8710DriverCallbacks;

/* 接收数据Buffer结构体 */
typedef struct {
    uint8_t* data;        /* 数据指针 */
    uint16_t dataLen;     /* 数据长度 */
    uint8_t  valid;       /* 数据有效性: 1=有效, 0=无效 */
    uint8_t  userIndex;   /* 用户索引 */
} TK8710RxBuffer;

/* 下行类型枚举 */
typedef enum {
    TK8710_DOWNLINK_A = 0,  /* 下行A */
    TK8710_DOWNLINK_B = 1,  /* 下行B */
} TK8710DownlinkType;

/* 数据类型 */
#define TK8710_DATA_TYPE_BRD     0   /* 广播波束 - 使用广播式波束发送数据 */
#define TK8710_DATA_TYPE_DED     1   /* 指定波束 - 使用针对性的波束 */

/* 发送数据Buffer结构体 */
typedef struct {
    uint8_t* data;        /* 数据指针 */
    uint16_t dataLen;     /* 数据长度 */
    uint8_t  valid;       /* 数据有效性: 1=有效, 0=无效 */
    uint8_t  userIndex;   /* 用户索引 */
    uint8_t  txPower;     /* 发送功率 */
    uint8_t  beamType;    /* 波束类型: 0=广播数据, 1=专用数据 */
} TK8710TxBuffer;

/* 广播数据Buffer结构体 */
typedef struct {
    uint8_t* data;        /* 数据指针 */
    uint16_t dataLen;     /* 数据长度 */
    uint8_t  valid;       /* 数据有效性: 1=有效, 0=无效 */
    uint8_t  brdIndex;    /* 广播用户索引 (0-15) */
    uint8_t  txPower;     /* 发送功率 */
    uint8_t  beamType;    /* 波束类型: 0=广播数据, 1=专用数据 */
} TK8710BrdBuffer;

/* 信号质量信息结构体 */
typedef struct {
    uint32_t rssi;        /* RSSI值 */
    uint8_t  snr;         /* SNR值 */
    uint32_t freq;        /* 频率值 */
    uint8_t  valid;       /* 数据有效性: 1=有效, 0=无效 */
} TK8710SignalInfo;

/* 时隙配置结构体 */
typedef struct {
    uint16_t byteLen;       /* 时隙字节数 */
    uint32_t timeLen;       /* 时隙时间长度, 单位us (仅查询有效) */
    uint32_t centerFreq;    /* 时隙中心频点 */
    uint32_t FreqOffset;    /*相较于中心频点的偏移*/
    uint32_t da_m;          /* 内部DAC参数，用于配置时隙末尾的空闲长度 */
} SlotConfig;

typedef struct {
    msMode_e   msMode;          /* 主从模式 */
    uint8_t    plCrcEn;         /* Payload CRC使能 */
    uint8_t    rateCount;       /* 速率个数，范围1~4，支持最多4个速率模式 */
    rateMode_e rateModes[4];    /* 各时隙速率模式数组，对应s0Cfg-s3Cfg */
    uint8_t    brdUserNum;      /* 广播时隙用户数, 范围1~16 */
    float      brdFreq[TK8710_MAX_BRD_USERS]; /*广播时隙发送用户的频率*/
    uint8_t    antEn;           /* 天线使能, 默认0xFF(8天线) */
    uint8_t    rfSel;           /* RF天线选择, 默认0xFF(8天线) */
    uint8_t    txBeamCtrlMode;  /* 下行波束控制模式: 0=芯片自动控制波束, 1=外部指定信息控制 */
    uint8_t    txBcnAntEn;      /* 发送BCN天线使能 */
    uint8_t    bcnRotation[TK8710_MAX_ANTENNAS];  /* BCN发送使能为0xff时，从bcnRotation中轮流获取当前发送bcn天线*/
    uint32_t   rx_delay;        /* RX delay, 默认0 */
    uint32_t   md_agc;          /* DATA AGC长度, 默认1024 */
    uint8_t    local_sync;      /* 本地同步: 1=产生本地同步信号, 0=接收外部同步脉冲 */
    
    SlotConfig s0Cfg[4];           /* 时隙0(BCN)配置 */
    SlotConfig s1Cfg[4];           /* 时隙1配置 */
    SlotConfig s2Cfg[4];           /* 时隙2配置 */
    SlotConfig s3Cfg[4];           /* 时隙3配置 */
    uint32_t   frameTimeLen;    /* TDD周期总时间长度, 单位us */
} slotCfg_t;

/* 波束资源配置结构体 */
typedef struct {
    uint32_t* freqList;     /* 频点列表, 128个uint32_t */
    uint32_t* ahList;       /* AH数据列表, 2048个数据点 (128用户 × 8天线 × 2(I/Q)) */
    uint32_t* pilotPwr;     /* Pilot功率, 128个数据点 */
    uint32_t* anoise;       /* Anoise数据, 8个数据点 */
    uint8_t   dataUserNum;  /* 数据用户数, 范围1~128 */
} fixInfo_t;

/* 功率配置结构体 */
typedef struct {
    uint8_t   start_index;  /* 用户起始索引 */
    uint8_t*  tx_power;     /* 功率数组 */
    size_t    count;        /* 写入数量 */
} pwrCfg_t;

/* 附加位配置联合体 */
typedef union {
    struct {
        uint8_t lenEn : 1;  /* 物理层携带数据包长度 */
        uint8_t rfu   : 7;  /* 保留 */
    } bits;
    uint8_t value;
} addtlCfg_u;

/* 唤醒参数结构体 */
typedef struct {
    wakeUpMode_e wakeUpMode;    /* 唤醒模式, 范围1~3 */
    uint8_t      wakeUpId;      /* 唤醒ID */
    uint32_t     wakeUpLen;     /* 唤醒信号时长, 单位ms */
} wakeUpParam_t;

/* ACM校准参数结构体 */
typedef struct {
    uint8_t  calibCount;        /* 连续校准次数 */
    uint8_t  snrThreshold;      /* 校准信号SNR门限 */
    uint8_t  snrPassCount;      /* SNR大于门限的个数 */
} acmParam_t;

/* 信号质量结构体 */
typedef struct {
    uint8_t snr_data[TK8710_MAX_DATA_USERS];    /* 数据用户SNR */
    uint8_t snr_brd[TK8710_MAX_BRD_USERS];      /* 广播用户SNR */
    uint8_t rssi_data[TK8710_MAX_DATA_USERS];   /* 数据用户RSSI */
    uint8_t rssi_brd[TK8710_MAX_BRD_USERS];     /* 广播用户RSSI */
} SignalQuality;

/* TX Tone配置结构体 */
typedef struct {
    uint32_t freq;          /* Tone频点 */
    uint8_t  gain;          /* Tone增益 */
} TxToneConfig;

/* GPIO中断回调函数类型 */
typedef void (*TK8710GpioIrqCallback)(void* user);

/* GPIO中断边沿类型枚举 */
typedef enum {
    TK8710_GPIO_EDGE_RISING  = 0,
    TK8710_GPIO_EDGE_FALLING = 1,
    TK8710_GPIO_EDGE_BOTH    = 2,
} TK8710GpioEdge;

#ifdef __cplusplus
}
#endif

#endif /* TK8710_TYPES_H */
