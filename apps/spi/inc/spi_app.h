/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-06 14:18:22
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-26 11:47:44
 * @FilePath: \spi\spi_app.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SPI_APP_H
#define SPI_APP_H

#include <stdint.h>
#include <stddef.h>
#include <ipc_smp.h>

// IPC Channel Configurations
// (Moved to after GwBatchDataUp_t definition to allow sizeof)

// Message Types
#define IPC_MSG_TYPE_MQTT_TO_SPI 0x1001u
#define IPC_MSG_TYPE_SPI_TO_MQTT 0x2001u

// MAX_PAYLOAD_LEN definition
#define MAX_PAYLOAD_LEN 520

// Message Types Enum
typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_NS_CONFIG_DOWN, // NS配置下行
    MSG_TYPE_NS_DATA_DOWN,   // NS数据下行
    MSG_TYPE_GW_DATA_UP,     // 网关数据上行
    MSG_TYPE_GW_ALIVE_UP,     // 网关心跳上行 (预留)
    MSG_TYPE_CONFIG_REQ      // 配置请求
} MacMsgType_e;

#define MAX_RATE_CFGS 8
#define MAX_UP_USER_NUM 128

// Up Data Item Structure
typedef struct {
    int tdd;
    int rate;
    int slot;
    int rssi;
    int snr;
    int freq_offset;//跳频通信频点
    size_t payload_len;                     // 二进制数据的实际长度
    unsigned char payload[MAX_PAYLOAD_LEN]; // 二进制数据
} GwDataUp_t;

// 4. 网关批量数据上行结构体
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_GW_DATA_UP
    int total_count;         // 总共有多少个 GwDataUp_t
    GwDataUp_t data_list[MAX_UP_USER_NUM]; // 最多128个数据
} GwBatchDataUp_t;

// IPC Channel Configurations
#define IPC_CH_MQTT_TO_SPI "/ipc_mqtt_to_spi"
#define IPC_CH_SPI_TO_MQTT "/ipc_spi_to_mqtt"
#define IPC_RING_SIZE (4u * 1024u * 1024u) // 扩大到4MB以支持批量数据
#define IPC_MAX_MSG_SIZE (sizeof(GwBatchDataUp_t) + 256) // 动态适配结构体大小
#define IPC_BATCH_COUNT 8u
#define IPC_FLUSH_TIMEOUT_MS 20u

typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_NS_DATA_DOWN
    int tdd;
    int rate;
    int slot;
    size_t payload_len; 
    unsigned char payload[MAX_PAYLOAD_LEN]; // Base64解码后的二进制数据
                        // 二进制数据的实际长度
} NsDataDown_t;

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

// 配置请求结构体
typedef struct {
    MacMsgType_e msg_type;   // 必须为第一成员，值为 MSG_TYPE_CONFIG_REQ
    int req_data_type;       // 1是请求配置，其它后续扩展
} ConfigReq_t;
// SPI Device Path
#define SPI_DEV_PATH "/dev/spidev0.0"

// Function Prototypes
int spi_app_init(void);
void spi_app_run(void);
void spi_app_cleanup(void);

#endif // SPI_APP_H
