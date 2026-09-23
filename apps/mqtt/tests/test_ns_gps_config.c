#include "mac_msg_parser.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char *values[] = {"0", "1", "2", "-1", "true", "null", "\"1\"", "1.0"};
    for (int i = 0; i < 8; ++i) {
        char json[128];
        snprintf(json, sizeof(json), "{\"set\":{\"config\":{\"gps_enable\":%s}}}", values[i]);
        MacMsgBase_t *msg = parse_mac_msg(json);
        if (i < 2) {
            assert(msg && msg->msg_type == MSG_TYPE_NS_CONFIG_DOWN);
            assert(((NsConfigDown_t *)msg)->gps_enable == i);
        } else assert(msg == NULL);
        free_mac_msg(msg);
    }
    assert(parse_mac_msg("{\"set\":{\"config\":{}}}") == NULL);
    puts("PASS NS gps_enable: 0/1 accepted, missing and invalid types rejected");
    return 0;
}
