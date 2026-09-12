/**
 * @file tk8710_gw_gps.h
 * @brief Gateway-specific GPS/PPS lifecycle management
 */

#ifndef TK8710_GW_GPS_H
#define TK8710_GW_GPS_H

#include "tk8710_pps_api.h"
#include "trm/trm_api.h"

#include <stdint.h>

typedef enum {
    GW_GPS_MODE_ABSENT = 0,
    GW_GPS_MODE_SEARCHING,
    GW_GPS_MODE_LOCAL_DEGRADED,
    GW_GPS_MODE_EXTERNAL_READY,
    GW_GPS_MODE_EXTERNAL_ACTIVE,
    GW_GPS_MODE_LOCAL_STATIC,
    GW_GPS_MODE_FAULT
} GwGpsMode;

typedef enum {
    GW_GPS_ACTION_NONE = 0,
    GW_GPS_ACTION_FATAL_EXIT,
    GW_GPS_ACTION_RESTART_EXIT
} GwGpsAction;

typedef struct {
    uint32_t detect_attempts;
    uint32_t search_timeout_ms;
    uint32_t poll_interval_ms;
    uint32_t align_timeout_ms;
    uint32_t consecutive_limit;
} GwGpsPolicy;

typedef struct {
    TK8710PpsContext pps;
    TK8710PpsConfig pps_config;
    GwGpsPolicy policy;
    GwGpsMode mode;
    uint32_t expected_period_s;
    uint32_t consecutive_abnormal;
    uint32_t consecutive_recovered;
    uint64_t consecutive_healthy_status;
    uint8_t uart_initialized;
    const volatile int* running;
} GwGpsManager;

void GwGpsGetDefaultPolicy(GwGpsPolicy* policy);
uint8_t GwGpsBasicReady(const TK8710PpsStatus* status);
uint8_t GwGpsFullHealthy(const TK8710PpsStatus* status, uint32_t expected_period_s);
int GwGpsPeriodFromSlotCalc(const TRM_MultiRateSlotCalcOutput* output,
    uint32_t* period_s);
GwGpsAction GwGpsUpdateMonitor(GwGpsManager* manager, uint8_t query_ok,
    const TK8710PpsStatus* status);

int GwGpsStartup(GwGpsManager* manager, const TK8710PpsConfig* pps_config,
    const GwGpsPolicy* policy, const volatile int* running,
    char* version, uint32_t version_size);
uint8_t GwGpsCanConfigurePeriod(const GwGpsManager* manager);
int GwGpsConfigurePeriod(GwGpsManager* manager, uint32_t period_s);
GwGpsAction GwGpsPoll(GwGpsManager* manager);
void GwGpsUseLocalWithoutRecovery(GwGpsManager* manager);
void GwGpsPrintDiagnostics(GwGpsManager* manager, const char* reason);
void GwGpsClose(GwGpsManager* manager);
const char* GwGpsModeName(GwGpsMode mode);

#ifdef TK8710_GPS_TEST_HOOKS
#define GW_GPS_TEST_RUN_ID_MAX 48

typedef enum {
    GW_GPS_TEST_PASSTHROUGH = 0,
    GW_GPS_TEST_NO_MODULE,
    GW_GPS_TEST_NO_PPS,
    GW_GPS_TEST_NO_PPS_THEN_RECOVER
} GwGpsTestScenario;

int GwGpsTestParseScenario(const char* name, GwGpsTestScenario* scenario);
int GwGpsTestSetScenario(GwGpsTestScenario scenario, const char* run_id);
GwGpsTestScenario GwGpsTestGetScenario(void);
const char* GwGpsTestScenarioName(GwGpsTestScenario scenario);
int GwGpsTestClearMarker(const char* run_id);
void GwGpsTestReset(void);
#endif

#endif /* TK8710_GW_GPS_H */
