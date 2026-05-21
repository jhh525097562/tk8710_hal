#ifndef MAC_MSG_PARSER_H
#define MAC_MSG_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定义支持的最大速率配置数量和最大有效载荷长度，避免动态内存碎片
#define MAX_RATE_CFGS 8
#define MAX_PAYLOAD_LEN 512
#define MAX_VER_LEN 64
#define MAX_NET_NAME_LEN 16
#define MAX_UP_USER_NUM 128

// 消息类型枚举
typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_NS_CONFIG_DOWN, // NS配置下行
    MSG_TYPE_NS_DATA_DOWN,   // NS数据下行
    MSG_TYPE_GW_DATA_UP,     // 网关数据上行
    MSG_TYPE_GW_ALIVE_UP,     // 网关心跳上行 (预留)
    MSG_TYPE_CONFIG_FEQ,      // 配置请求
    MSG_TYPE_SWEEP_FREQ,     // 扫频请求 (NS -> 网关)
    MSG_TYPE_SWEEP_RESULT     // 扫频结果上报 (网关 -> NS)
} MacMsgType_e;

// 基础消息结构体（用于类型推断和强制转换）
typedef struct {
    MacMsgType_e msg_type;   // 所有具体结构体的第一个成员必须是它！
} MacMsgBase_t;

// 配置请求结构体
typedef struct {
    MacMsgType_e msg_type;   // 所有具体结构体的第一个成员必须是它！
    int req_data_type;       //1是请求配置，其他后续扩展
} ConfigReq_t;
// ================= 具体消息结构体 =================

// 1. NS配置下行结构体
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_NS_CONFIG_DOWN
    unsigned int freq;
    int nwk_num;
    int tdd_num;
    int slot_cfg;
    int rate_num;
    struct {
        int rate;
        int uplink_pkt;
        int downlink_pkt;
    } rate_cfgs[MAX_RATE_CFGS];
} NsConfigDown_t;

// 2. NS数据下行结构体
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_NS_DATA_DOWN
    int tdd;
    int rate;
    int slot;
    size_t payload_len;  
    unsigned char payload[MAX_PAYLOAD_LEN]; // Base64解码后的二进制数据
                       // 二进制数据的实际长度
} NsDataDown_t;

// 3. 网关数据上行结构体
typedef struct {
    int tdd;
    int rate;
    int slot;
    int rssi;
    int snr;
    // int freq_offset;
    size_t payload_len;                     // 二进制数据的实际长度
    unsigned char payload[MAX_PAYLOAD_LEN]; // Base64解码后的二进制数据
} GwDataUp_t;

// 4. 网关批量数据上行结构体
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_GW_BATCH_DATA_UP
    int total_count;         // 总共有多少个 GwDataUp_t
    GwDataUp_t data_list[MAX_UP_USER_NUM]; // 最多128个 GwDataUp_t
} GwBatchDataUp_t;

typedef struct {
    MacMsgType_e msg_type;
    char ver[MAX_VER_LEN];
    int64_t uptime;
    char net[MAX_NET_NAME_LEN];
    int power;
} GwAliveUp_t;

// 5. 网关扫频请求结构体 (NS -> 网关)
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_SWEEP_FREQ
    unsigned int start_freq; // 起始频率 (Hz)
    unsigned int end_freq;   // 结束频率 (Hz)
    int sweep_mode;          // 扫频模式: 0=62.5kHz(模式5), 1=125kHz(模式6), 2=250kHz(模式7), 3=500kHz(模式8)
} SweepFreqReq_t;

// 6. 网关扫频结果上报结构体 (网关 -> NS)
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_SWEEP_RESULT
    unsigned int freq;       // 扫频频率 (Hz)
    float noise_floor;       // 噪底值 (dBm)
    int sweep_count;         // 扫频次数
    int status;              // 0=成功, 负值=失败
} SweepFreqResult_t;

// ================= 接口 API =================

// 打印开关宏定义，取消注释即可开启打印，或在编译时通过 -DENABLE_MAC_MSG_PRINT 开启
 #define ENABLE_MAC_MSG_PRINT

/**
 * @brief 解析 MAC 层 JSON 字符串
 * @param json_str 原始的 JSON 字符串
 * @return 成功返回动态分配的结构体指针（使用后需调用 free_mac_msg 释放），失败返回 NULL
 */
MacMsgBase_t* parse_mac_msg(const char* json_str);

/**
 * @brief 将网关数据上行结构体打包为 JSON 字符串
 * @param msg 网关数据上行结构体指针
 * @return 成功返回动态分配的 JSON 字符串（使用后需调用 free_json_str 释放），失败返回 NULL
 */
char* pack_gw_data_up_msg(const GwDataUp_t* msg);
char* pack_gw_alive_up_msg(const GwAliveUp_t* msg);

/**
 * @brief 释放由 pack_gw_data_up_msg 分配的 JSON 字符串内存
 * @param json_str JSON 字符串指针
 */
void free_json_str(char* json_str);

/**
 * @brief 释放由 parse_mac_msg 分配的内存
 * @param msg 解析返回的结构体指针
 */
void free_mac_msg(MacMsgBase_t* msg);

/**
 * @brief 打印解析后的结构体内容（按语义输出）
 * @param msg 解析返回的结构体指针
 */
void print_mac_msg(const MacMsgBase_t* msg);

#ifdef __cplusplus
}
#endif

#endif // MAC_MSG_PARSER_H
