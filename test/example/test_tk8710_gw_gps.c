#include "tk8710_gw_gps.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static TK8710PpsStatus make_status(void)
{
    TK8710PpsStatus status;

    memset(&status, 0, sizeof(status));
    strcpy(status.state, "RUNNING");
    status.period = 4;
    status.output_enabled = 1;
    status.gps_online = 1;
    status.pps_seen = 1;
    status.fix_valid = 1;
    status.rmc_status = 'A';
    status.aligned = 1;
    return status;
}

int main(void)
{
    GwGpsPolicy policy;
    GwGpsManager manager;
    TK8710PpsStatus status = make_status();
    TRM_MultiRateSlotCalcOutput slot_output;
    uint32_t period_s = 0;

    GwGpsGetDefaultPolicy(&policy);
    assert(TK8710_SYNC_MODE_EXTERNAL == 0);
    assert(TK8710_SYNC_MODE_LOCAL == 1);
    assert(policy.detect_attempts == 2);
    assert(policy.search_timeout_ms == 300000);
    assert(policy.poll_interval_ms == 10000);
    assert(policy.align_timeout_ms == 60000);
    assert(policy.consecutive_limit == 3);

    assert(GwGpsBasicReady(&status) == 1);
    assert(GwGpsFullHealthy(&status, 4) == 1);
    status.rmc_status = 'V';
    assert(GwGpsBasicReady(&status) == 0);
    assert(GwGpsFullHealthy(&status, 4) == 0);
    status = make_status();

    memset(&slot_output, 0, sizeof(slot_output));
    slot_output.framePeriod = 400000;
    slot_output.frameCount = 10;
    assert(GwGpsPeriodFromSlotCalc(&slot_output, &period_s) == 0);
    assert(period_s == 4);
    slot_output.framePeriod = 333333;
    slot_output.frameCount = 3;
    assert(GwGpsPeriodFromSlotCalc(&slot_output, &period_s) != 0);

    memset(&manager, 0, sizeof(manager));
    manager.policy = policy;
    manager.mode = GW_GPS_MODE_EXTERNAL_ACTIVE;
    manager.expected_period_s = 4;
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    status.bad_period = 1;
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_FATAL_EXIT);
    status = make_status();
    manager.mode = GW_GPS_MODE_EXTERNAL_ACTIVE;
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(manager.consecutive_abnormal == 0);

    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_NONE);
    assert(manager.consecutive_abnormal == 1);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(manager.consecutive_abnormal == 0);
    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_FATAL_EXIT);

    manager.mode = GW_GPS_MODE_LOCAL_DEGRADED;
    manager.consecutive_abnormal = 0;
    manager.consecutive_recovered = 0;
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_NONE);
    assert(manager.consecutive_recovered == 0);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_NONE);
    assert(GwGpsUpdateMonitor(&manager, 1, &status) == GW_GPS_ACTION_RESTART_EXIT);

    manager.mode = GW_GPS_MODE_LOCAL_STATIC;
    assert(GwGpsUpdateMonitor(&manager, 0, NULL) == GW_GPS_ACTION_NONE);
    assert(GwGpsCanConfigurePeriod(&manager) == 1);
    manager.mode = GW_GPS_MODE_LOCAL_DEGRADED;
    assert(GwGpsCanConfigurePeriod(&manager) == 1);
    manager.mode = GW_GPS_MODE_EXTERNAL_READY;
    assert(GwGpsCanConfigurePeriod(&manager) == 1);
    manager.mode = GW_GPS_MODE_EXTERNAL_ACTIVE;
    assert(GwGpsCanConfigurePeriod(&manager) == 1);
    manager.mode = GW_GPS_MODE_ABSENT;
    assert(GwGpsCanConfigurePeriod(&manager) == 0);
    manager.mode = GW_GPS_MODE_FAULT;
    assert(GwGpsCanConfigurePeriod(&manager) == 0);
    puts("TK8710 gateway GPS policy tests passed");
    return 0;
}
