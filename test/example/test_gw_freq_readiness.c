/* Host-only test of the actual runtime poll with hardware observations stubbed.
 * Link with -ffunction-sections -fdata-sections -Wl,--gc-sections. */
#include "tk8710_gw_freq_runtime.c"
#include <assert.h>

static uint64_t test_now_us = 1000000;
static int test_pps_fault;
static int test_broadcast_result;
static uint8_t test_broadcast_valid;
static int test_ns_received;
static uint32_t test_s3;

uint64_t TK8710GetTimeUs(void) { return test_now_us; }
void TK8710GetAllIrqCounters(uint32_t* counters)
{
    memset(counters, 0, 10*sizeof(*counters));
    counters[TK8710_IRQ_S3]=test_s3;
}
uint8_t TRM_IsShutdownRequested(void) { return 0; }
int TrmPpsMonitorFailed(void) { return test_pps_fault; }
int TrmPpsMonitorMissing(void) { return 0; }
GwGpsAction GwGpsPoll(GwGpsManager* manager) { (void)manager; return GW_GPS_ACTION_NONE; }
const char* GwGpsModeName(GwGpsMode mode) { (void)mode; return "TEST"; }
int TK8710GetAbnormalRfChannelStatus(uint8_t* mask, uint8_t* count)
{
    *mask=0; *count=0; return TK8710_OK;
}
int IpcCommIsConfigReceived(void) { return test_ns_received; }
int TRM_GetBroadcastStatus(uint8_t* payload, uint8_t* valid)
{
    (void)payload; *valid=test_broadcast_valid; return test_broadcast_result;
}

int main(void)
{
    g_running=1;
    g_gps_manager.mode=GW_GPS_MODE_EXTERNAL_ACTIVE;
    g_gps_manager.consecutive_abnormal=0;
    g_freq_ns_verified=0;
    g_freq_last_slot_ms=1000;
    /* Healthy GPS alone is insufficient. */
    assert(GwFreqPoll(1)==0);
    test_ns_received=1;
    assert(GwFreqPoll(1)==0);
    g_freq_ns_verified=1;
    assert(GwFreqPoll(1)==0);
    test_broadcast_valid=1;
    assert(GwFreqPoll(1)==0);
    test_s3=1;
    assert(GwFreqPoll(1)==0);
    test_s3=2;
    assert(GwFreqPoll(1)==1);
    test_ns_received=0;
    assert(GwFreqPoll(1)==0);
    test_ns_received=1;
    g_freq_ns_verified=0;
    assert(GwFreqPoll(1)==0);
    g_freq_ns_verified=1;
    test_broadcast_valid=0;
    assert(GwFreqPoll(1)==0);
    test_broadcast_valid=1;
    test_broadcast_result=TRM_ERR_PARAM;
    assert(GwFreqPoll(1)==-1);
    test_broadcast_result=TRM_OK;
    g_gps_manager.mode=GW_GPS_MODE_EXTERNAL_READY;
    assert(GwFreqPoll(0)==0); /* Not yet PPS-aligned. */
    g_gps_manager.mode=GW_GPS_MODE_EXTERNAL_ACTIVE;
    g_gps_manager.consecutive_abnormal=1;
    assert(GwFreqPoll(1)==-1);
    g_gps_manager.consecutive_abnormal=0;
    test_pps_fault=1;
    assert(GwFreqPoll(1)==-2);
    test_pps_fault=0;
    g_freq_ns_mismatch=1;
    assert(GwFreqPoll(1)==-1); /* Explicit incompatible config is still a fault. */
    g_freq_ns_mismatch=0;
    test_now_us=62000000;
    assert(GwFreqPoll(1)==-1); /* Preserve runtime IRQ stall protection. */
    puts("PASS: actual GwFreqPoll requires GPS, verified NS, broadcast and S3; fault protection preserved");
    return 0;
}
