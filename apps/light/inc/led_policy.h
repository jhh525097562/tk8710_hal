#ifndef LED_POLICY_H
#define LED_POLICY_H
#include <stdint.h>
typedef enum { LED_PREPARING, LED_READY, LED_NETWORK_FAULT, LED_BUSINESS_FAULT,
    LED_GREEN_NS, LED_GREEN_GPS, LED_MAINTENANCE, LED_RED_MQTT, LED_RED_NS, LED_RED_GPS } LedMode;
typedef enum { INPUT_MISSING, INPUT_PREPARING, INPUT_READY, INPUT_FAULT, INPUT_DOWN,
    INPUT_WAIT_NS, INPUT_WAIT_GPS, INPUT_OPERATION, INPUT_MQTT_FAULT, INPUT_NS_FAULT,
    INPUT_GPS_FAULT, INPUT_HW_FAULT } LedInput;
typedef struct {
    uint64_t ready_since, healthy_since, eligible_since;
    uint8_t eligible_seen, ready_seen, fault_latched;
    uint64_t gw_start, restart_since;
    unsigned restarts;
    unsigned fault_class; /* 0 none, 1 generic, 2 NS, 3 MQTT, 4 GPS, 5 hardware */
} LedPolicy;
void LedObserveGateway(LedPolicy *p,uint64_t start,uint64_t now,int maintenance);
LedMode LedDecide(LedPolicy *p, LedInput net, LedInput mqtt, LedInput gw,
                  uint64_t now, uint64_t boot_ms, int maintenance);
void LedLevels(LedMode mode, uint64_t now, int active_low, int levels[2]);
#endif
