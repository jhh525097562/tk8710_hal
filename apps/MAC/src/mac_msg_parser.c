#include "mac_msg_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "mbedtls/base64.h"

// 内部辅助函数：解码 Base64 并填充到 buffer 中
static int decode_base64_payload(const char* base64_str, unsigned char* out_buf, size_t max_len, size_t* out_len) {
    size_t slen = strlen(base64_str);
    size_t olen = 0;
    int ret = mbedtls_base64_decode(out_buf, max_len, &olen, (const unsigned char*)base64_str, slen);
    if (ret == 0) {
        *out_len = olen;
        return 0;
    }
    return -1; // 解码失败
}

// 内部辅助函数：编码 Base64
static int encode_base64_payload(const unsigned char* in_buf, size_t in_len, char* out_str, size_t max_len, size_t* out_len) {
    size_t olen = 0;
    int ret = mbedtls_base64_encode((unsigned char*)out_str, max_len, &olen, in_buf, in_len);
    if (ret == 0) {
        *out_len = olen;
        return 0;
    }
    return -1; // 编码失败
}

MacMsgBase_t* parse_mac_msg(const char* json_str) {
    if (!json_str) return NULL;

    struct json_object *root = json_tokener_parse(json_str);
    if (!root) {
        return NULL; // JSON 格式错误
    }

    MacMsgBase_t* result = NULL;
    struct json_object *set_obj, *cfg_obj, *item;

    // 1. 判断是否为 "NS配置下行" (包含 "set" 对象)
    if (json_object_object_get_ex(root, "set", &set_obj)) {
        if (json_object_object_get_ex(set_obj, "config", &cfg_obj)) {
            NsConfigDown_t* msg = (NsConfigDown_t*)calloc(1, sizeof(NsConfigDown_t));
            if (!msg) goto end;
            msg->msg_type = MSG_TYPE_NS_CONFIG_DOWN;
            
            if (json_object_object_get_ex(cfg_obj, "freq", &item)) msg->freq = (unsigned int)json_object_get_int64(item);
            if (json_object_object_get_ex(cfg_obj, "nwk_num", &item)) msg->nwk_num = json_object_get_int(item);
            if (json_object_object_get_ex(cfg_obj, "tdd_num", &item)) msg->tdd_num = json_object_get_int(item);
            if (json_object_object_get_ex(cfg_obj, "slot_cfg", &item)) msg->slot_cfg = json_object_get_int(item);
            
            struct json_object *rate_num_obj, *rate_cfgs_arr;
            if (json_object_object_get_ex(cfg_obj, "rate_num", &rate_num_obj)) {
                msg->rate_num = json_object_get_int(rate_num_obj);
            }
            
            if (json_object_object_get_ex(cfg_obj, "rate_cfgs", &rate_cfgs_arr) && json_object_is_type(rate_cfgs_arr, json_type_array)) {
                int arr_size = json_object_array_length(rate_cfgs_arr);
                int limit = (arr_size < MAX_RATE_CFGS) ? arr_size : MAX_RATE_CFGS;
                
                for (int i = 0; i < limit; i++) {
                    struct json_object *rate_item = json_object_array_get_idx(rate_cfgs_arr, i);
                    if (!rate_item) continue;
                    
                    if (json_object_object_get_ex(rate_item, "rate", &item)) msg->rate_cfgs[i].rate = json_object_get_int(item);
                    if (json_object_object_get_ex(rate_item, "uplink_pkt", &item)) msg->rate_cfgs[i].uplink_pkt = json_object_get_int(item);
                    if (json_object_object_get_ex(rate_item, "downlink_pkt", &item)) msg->rate_cfgs[i].downlink_pkt = json_object_get_int(item);
                }
            }
            result = (MacMsgBase_t*)msg;
            goto end;
        }
    }

    // 2. 区分是 "网关数据上行" 还是 "NS数据下行"
    // 都有 tdd, rate, slot, msg，但网关上行独有 rssi 和 snr
    struct json_object *tdd_obj, *msg_obj;
    if (json_object_object_get_ex(root, "tdd", &tdd_obj) && 
        json_object_object_get_ex(root, "msg", &msg_obj) && 
        json_object_is_type(msg_obj, json_type_string)) {
        
        struct json_object *rssi_obj;
        if (json_object_object_get_ex(root, "rssi", &rssi_obj)) {
            GwBatchDataUp_t* msg = (GwBatchDataUp_t*)calloc(1, sizeof(GwBatchDataUp_t));
            if (!msg) goto end;
            msg->msg_type = MSG_TYPE_GW_DATA_UP;
            msg->total_count = 1;
            msg->data_list[0].tdd = json_object_get_int(tdd_obj);
            msg->data_list[0].rssi = json_object_get_int(rssi_obj);
            
            if (json_object_object_get_ex(root, "rate", &item)) msg->data_list[0].rate = json_object_get_int(item);
            if (json_object_object_get_ex(root, "slot", &item)) msg->data_list[0].slot = json_object_get_int(item);
            if (json_object_object_get_ex(root, "snr", &item)) msg->data_list[0].snr = json_object_get_int(item);
            
            if (decode_base64_payload(json_object_get_string(msg_obj), msg->data_list[0].payload, MAX_PAYLOAD_LEN, &msg->data_list[0].payload_len) != 0) {
                fprintf(stderr, "decode base64 failed for gw_data_up\n");
            }
            result = (MacMsgBase_t*)msg;
        } else {
            // 这是：NS数据下行
            NsDataDown_t* msg = (NsDataDown_t*)calloc(1, sizeof(NsDataDown_t));
            if (!msg) goto end;
            msg->msg_type = MSG_TYPE_NS_DATA_DOWN;
            msg->tdd = json_object_get_int(tdd_obj);
            
            if (json_object_object_get_ex(root, "rate", &item)) msg->rate = json_object_get_int(item);
            if (json_object_object_get_ex(root, "slot", &item)) msg->slot = json_object_get_int(item);
            
            if (decode_base64_payload(json_object_get_string(msg_obj), msg->payload, MAX_PAYLOAD_LEN, &msg->payload_len) != 0) {
                fprintf(stderr, "decode base64 failed for ns_data_down\n");
            }
            result = (MacMsgBase_t*)msg;
        }
        goto end;
    }

end:
    json_object_put(root); // json-c 中释放 json 对象的标准接口
    return result;
}

void free_mac_msg(MacMsgBase_t* msg) {
    if (msg) {
        free(msg);
    }
}

char* pack_gw_batch_data_up_msg(const GwBatchDataUp_t* msg, int start_index, int count) {
    if (!msg || msg->msg_type != MSG_TYPE_GW_DATA_UP) return NULL;
    if (start_index < 0 || count <= 0 || start_index >= msg->total_count) return NULL;
    if (start_index + count > msg->total_count) return NULL;

    struct json_object *arr = json_object_new_array();
    if (!arr) {
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const GwDataUp_t *item = &msg->data_list[start_index + i];
        struct json_object *obj = json_object_new_object();
        if (!obj) continue;
        json_object_object_add(obj, "tdd", json_object_new_int(item->tdd));
        json_object_object_add(obj, "rate", json_object_new_int(item->rate));
        json_object_object_add(obj, "slot", json_object_new_int(item->slot));
        json_object_object_add(obj, "rssi", json_object_new_int(item->rssi));
        json_object_object_add(obj, "snr", json_object_new_int(item->snr));

        char b64_str[4 * ((MAX_PAYLOAD_LEN + 2) / 3) + 1];
        size_t out_len = 0;
        if (encode_base64_payload(item->payload, item->payload_len, b64_str, sizeof(b64_str), &out_len) == 0) {
            json_object_object_add(obj, "msg", json_object_new_string(b64_str));
        }
        json_object_array_add(arr, obj);
    }

    const char* json_str_temp = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    char* result = NULL;
    if (json_str_temp) {
        result = strdup(json_str_temp);
    }
    json_object_put(arr);
    return result;
}

char* pack_gw_alive_up_msg(const GwAliveUp_t* msg) {
    if (!msg || msg->msg_type != MSG_TYPE_GW_ALIVE_UP) return NULL;

    struct json_object *root = json_object_new_object();
    struct json_object *alive = json_object_new_object();
    if (!root || !alive) {
        if (alive) json_object_put(alive);
        if (root) json_object_put(root);
        return NULL;
    }

    json_object_object_add(alive, "ver", json_object_new_string(msg->ver));
    json_object_object_add(alive, "uptime", json_object_new_int64(msg->uptime));
    json_object_object_add(alive, "net", json_object_new_string(msg->net));
    json_object_object_add(alive, "power", json_object_new_int(msg->power));
    json_object_object_add(root, "alive", alive);

    const char* json_str_temp = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char* result = NULL;
    if (json_str_temp) {
        result = strdup(json_str_temp);
    }
    json_object_put(root);
    return result;
}

void free_json_str(char* json_str) {
    if (json_str) {
        free(json_str);
    }
}

// 辅助打印二进制为 Hex 格式
static void print_hex(const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
}

void print_mac_msg(const MacMsgBase_t* msg) {
#ifdef ENABLE_MAC_MSG_PRINT
    if (!msg) return;

    printf("\n--- 开始解析结构体语义 ---\n");
    switch (msg->msg_type) {
        case MSG_TYPE_NS_CONFIG_DOWN: {
            NsConfigDown_t* m = (NsConfigDown_t*)msg;
            printf("[类型] NS配置下行 (NS Config Downlink)\n");
            printf("  - 频点 (freq)    : %u Hz\n", m->freq);
            printf("  - 网络编号 (nwk) : %d\n", m->nwk_num);
            printf("  - TDD 编号       : %d\n", m->tdd_num);
            printf("  - 时隙配置       : %d\n", m->slot_cfg);
            printf("  - 速率配置数量   : %d\n", m->rate_num);
            for (int i = 0; i < m->rate_num; i++) {
                printf("    * 速率 %d: 上行包数=%d, 下行包数=%d\n", 
                       m->rate_cfgs[i].rate, m->rate_cfgs[i].uplink_pkt, m->rate_cfgs[i].downlink_pkt);
            }
            break;
        }
        case MSG_TYPE_NS_DATA_DOWN: {
            NsDataDown_t* m = (NsDataDown_t*)msg;
            printf("[类型] NS数据下行 (NS Data Downlink)\n");
            printf("  - TDD          : %d\n", m->tdd);
            printf("  - Rate         : %d\n", m->rate);
            printf("  - Slot         : %d\n", m->slot);
            printf("  - Payload 长度 : %zu bytes\n", m->payload_len);
            printf("  - Payload(Hex) : ");
            print_hex(m->payload, m->payload_len);
            printf("\n");
            break;
        }
        case MSG_TYPE_GW_DATA_UP: {
            GwBatchDataUp_t* m = (GwBatchDataUp_t*)msg;
            printf("[类型] 网关批量数据上行 (Gateway Batch Data Uplink)\n");
            printf("  - Total Count  : %d\n", m->total_count);
            for (int i = 0; i < m->total_count && i < MAX_UP_USER_NUM; i++) {
                const GwDataUp_t *item = &m->data_list[i];
                printf("  - Item[%d] TDD:%d Rate:%d Slot:%d RSSI:%d SNR:%d PayloadLen:%zu\n",
                       i, item->tdd, item->rate, item->slot, item->rssi, item->snr, item->payload_len);
                printf("    Payload(Hex) : ");
                print_hex(item->payload, item->payload_len);
                printf("\n");
            }
            break;
        }
        case MSG_TYPE_GW_ALIVE_UP: {
            GwAliveUp_t* m = (GwAliveUp_t*)msg;
            printf("[类型] 网关心跳上行 (Gateway Alive Uplink)\n");
            printf("  - Ver          : %s\n", m->ver);
            printf("  - Uptime       : %lld\n", (long long)m->uptime);
            printf("  - Net          : %s\n", m->net);
            printf("  - Power        : %d\n", m->power);
            break;
        }
        default:
            printf("[类型] 未知消息类型\n");
            break;
    }
    printf("----------------------------\n");
#endif
}
