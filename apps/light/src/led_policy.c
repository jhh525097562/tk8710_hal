#include "led_policy.h"
static LedMode fault_mode(unsigned type)
{
    if(type==2)return LED_RED_NS;
    if(type==3)return LED_RED_MQTT;
    if(type==4)return LED_RED_GPS;
    return LED_BUSINESS_FAULT;
}
static unsigned fault_type(LedInput input)
{
    switch(input) {
    case INPUT_HW_FAULT:return 5;
    case INPUT_GPS_FAULT:return 4;
    case INPUT_MQTT_FAULT:return 3;
    case INPUT_NS_FAULT:return 2;
    case INPUT_FAULT:return 1;
    default:return 0;
    }
}
void LedObserveGateway(LedPolicy *p,uint64_t start,uint64_t now,int maintenance)
{
    if (!start || start==p->gw_start) return;
    if (p->gw_start && !maintenance) {
        if (!p->restarts || now-p->restart_since>300000) {p->restarts=0;p->restart_since=now;}
        if (++p->restarts>=3) p->fault_latched=1;
    }
    p->gw_start=start;
}
LedMode LedDecide(LedPolicy *p, LedInput net, LedInput mqtt, LedInput gw,
                  uint64_t now, uint64_t boot_ms, int maintenance)
{
    int healthy = net == INPUT_READY && mqtt == INPUT_READY && gw == INPUT_READY;
    if (net == INPUT_READY && !p->eligible_seen) {
        p->eligible_seen=1; p->eligible_since=now;
    }
    if (!healthy) { p->ready_since=0; p->healthy_since=0; }
    if (net == INPUT_DOWN) return LED_NETWORK_FAULT;
    if (maintenance) return LED_MAINTENANCE;
    unsigned cause=fault_type(net), mc=fault_type(mqtt), gc=fault_type(gw);
    if(mc>cause)cause=mc;
    if(gc>cause)cause=gc;
    if (cause) {
        p->fault_latched=1;
        if(cause>p->fault_class)p->fault_class=cause;
        return fault_mode(p->fault_class);
    }
    if (healthy) {
        if (!p->healthy_since) p->healthy_since=now;
        if (!p->ready_since) p->ready_since=now;
        if (p->fault_latched && now-p->healthy_since < 30000) return fault_mode(p->fault_class);
        p->fault_latched=0; p->fault_class=0;
        if (now-p->healthy_since>=30000) p->restarts=0;
        if (now-p->ready_since < 3000) return LED_PREPARING;
        p->ready_seen=1;
        return LED_READY;
    }
    if (p->fault_latched) return fault_mode(p->fault_class);
    if (net == INPUT_MISSING || mqtt == INPUT_MISSING || gw == INPUT_MISSING) {
        if (p->ready_seen || (p->eligible_seen && now-p->eligible_since>30000) ||
            (!p->eligible_seen && boot_ms>180000)) {
            p->fault_latched=1;
            return LED_BUSINESS_FAULT;
        }
    }
    if (gw==INPUT_WAIT_GPS) return LED_GREEN_GPS;
    if (gw==INPUT_WAIT_NS) return LED_GREEN_NS;
    if (gw==INPUT_OPERATION) return LED_MAINTENANCE;
    return LED_PREPARING;
}
void LedLevels(LedMode mode, uint64_t now, int active_low, int levels[2])
{
    int green=0, red=0;
    if (mode==LED_READY) green=1;
    else if (mode==LED_NETWORK_FAULT) red=1;
    else if(mode==LED_MAINTENANCE)green=(now/1000)%2==0;
    else {
        unsigned count=1;
        if(mode==LED_GREEN_NS || mode==LED_RED_NS)count=2;
        else if(mode==LED_GREEN_GPS || mode==LED_RED_GPS)count=3;
        else if(mode==LED_BUSINESS_FAULT)count=4;
        /* Last pulse's falling edge to next first pulse: exactly 2000ms. */
        uint64_t pos=now%((count*2-1)*250+2000);
        int on=pos<(count*2-1)*250 && (pos/250)%2==0;
        if(mode==LED_PREPARING || mode==LED_GREEN_NS || mode==LED_GREEN_GPS)green=on;
        else red=on;
    }
    levels[0]=active_low ? !green : green;
    levels[1]=active_low ? !red : red;
}
