#include <stdio.h>
#include "mac_msg_parser.h"

int main() {
    // 1. 测试: NS配置下行
    const char* json_ns_config = "{\"set\":{\"config\":{\"freq\":470300000,\"nwk_num\":4,\"tdd_num\":1,\"slot_cfg\":0,\"rate_num\":4,\"rate_cfgs\":[{\"rate\":0,\"uplink_pkt\":1,\"downlink_pkt\":2},{\"rate\":1,\"uplink_pkt\":3,\"downlink_pkt\":4},{\"rate\":2,\"uplink_pkt\":5,\"downlink_pkt\":6},{\"rate\":3,\"uplink_pkt\":7,\"downlink_pkt\":8}]}}}";
    
    // 2. 测试: NS数据下行 (使用合法的标准双引号)
    const char* json_ns_data = "{\"tdd\":0,\"rate\":2,\"slot\":3,\"msg\":\"QIAAAQAAAAAAEQETAQIDBAUGBwgJEA==\"}";

    // 3. 测试: 网关数据上行
    const char* json_gw_data = "{\"tdd\":0,\"rate\":2,\"slot\":3,\"rssi\":-45,\"snr\":18,\"msg\":\"QIAAAQAAAAAAEQETAQIDBAUGBwgJEA==\"}";

    printf("解析测试 1...\n");
    MacMsgBase_t* msg1 = parse_mac_msg(json_ns_config);
    print_mac_msg(msg1);
    free_mac_msg(msg1);

    printf("解析测试 2...\n");
    MacMsgBase_t* msg2 = parse_mac_msg(json_ns_data);
    print_mac_msg(msg2);
    free_mac_msg(msg2);

    printf("解析测试 3...\n");
    MacMsgBase_t* msg3 = parse_mac_msg(json_gw_data);
    print_mac_msg(msg3);
    
    // 演示多态用法：调用方如何自己处理数据
    if (msg3 && msg3->msg_type == MSG_TYPE_GW_DATA_UP) {
        GwDataUp_t* gw_msg = (GwDataUp_t*)msg3; // 强转为具体子类
        printf("\n【业务逻辑处理】成功获取网关上报数据, RSSI为: %d\n", gw_msg->rssi);

        // 演示打包回 JSON
        printf("\n【打包测试】将结构体打包回 JSON 字符串...\n");
        char* packed_json = pack_gw_data_up_msg(gw_msg);
        if (packed_json) {
            printf("打包结果: %s\n", packed_json);
            free_json_str(packed_json);
        } else {
            printf("打包失败!\n");
        }
    }
    free_mac_msg(msg3);

    return 0;
}
