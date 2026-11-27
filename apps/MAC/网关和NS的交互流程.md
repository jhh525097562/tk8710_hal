<!--
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-21 08:39:57
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-21 08:54:47
 * @FilePath: \MAC\网关和NS的交互流程.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->
# 网关和NS的交互流程

网关请求

{"get":{"config":true}}

NS配置下行

{"set":{"config":{"freq":470300000,"nwk_num":4,"tdd_num":1,"slot_cfg":0,"rate_num":4,"rate_cfgs":[{"rate":0,"uplink_pkt":1,"downlink_pkt":2},{"rate":1,"uplink_pkt":3,"downlink_pkt":4},{"rate":2,"uplink_pkt":5,"downlink_pkt":6},{"rate":3,"uplink_pkt":7,"downlink_pkt":8}]}}}

NS数据下行

{"tdd":0,"rate":2,"slot":3,"msg":”QIAAAQAAAAAAEQETAQIDBAUGBwgJEA==”}

网关数据上行

{"tdd":0,"rate":2,"slot":3,”rssi”:-45,”snr”:18,"msg":”QIAAAQAAAAAAEQETAQIDBAUGBwgJEA==”}

网关上行

{alive:{"ver":"V1.0","uptime":3600}}

tkg1234567887654321/msg/up
tkg1234567887654321/msg/down
