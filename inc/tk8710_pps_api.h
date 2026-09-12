/**
 * @file tk8710_pps_api.h
 * @brief RK3506 external PPS device UART API
 */

#ifndef TK8710_PPS_API_H
#define TK8710_PPS_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TK8710_PPS_PATH_MAX 128
#define TK8710_PPS_STATE_MAX 16
#define TK8710_PPS_UTC_MAX 32
#define TK8710_PPS_DIAG_TEXT_MAX 512

typedef enum {
    TK8710_PPS_OK = 0,
    TK8710_PPS_ERROR_PARAM = -1,
    TK8710_PPS_ERROR_NOT_INITIALIZED = -2,
    TK8710_PPS_ERROR_UNSUPPORTED = -3,
    TK8710_PPS_ERROR_OPEN = -4,
    TK8710_PPS_ERROR_WRITE = -5,
    TK8710_PPS_ERROR_READ = -6,
    TK8710_PPS_ERROR_TIMEOUT = -7,
    TK8710_PPS_ERROR_PROTOCOL = -8,
    TK8710_PPS_ERROR_PARSE = -9,
    TK8710_PPS_ERROR_OVERFLOW = -10,
    TK8710_PPS_ERROR_GPIO = -11
} TK8710PpsError;

typedef enum {
    TK8710_PPS_HEALTHY = 0,
    TK8710_PPS_WAITING,
    TK8710_PPS_HOLDOVER,
    TK8710_PPS_UNHEALTHY
} TK8710PpsHealth;

typedef struct {
    const char* uart_device;
    uint32_t baud_rate;
    uint32_t command_timeout_ms;
    const char* reset_gpio_chip;
    uint32_t reset_gpio_line;
    uint32_t reset_low_ms;
    uint32_t reset_boot_wait_ms;
} TK8710PpsConfig;

typedef struct {
    int fd;
    uint8_t initialized;
    TK8710PpsConfig config;
    char uart_device[TK8710_PPS_PATH_MAX];
    char reset_gpio_chip[TK8710_PPS_PATH_MAX];
    char last_event[TK8710_PPS_DIAG_TEXT_MAX];
} TK8710PpsContext;

typedef struct {
    uint32_t period;
    uint32_t pending_period;
    uint8_t pending;
    uint8_t pause;
    uint8_t confirm_pending;
} TK8710PpsPeriodInfo;

typedef struct {
    char state[TK8710_PPS_STATE_MAX];
    uint32_t period;
    uint32_t width_ms;
    uint32_t satellites;
    uint32_t snr;
    uint8_t output_enabled;
    uint8_t pending;
    uint8_t gps_online;
    uint8_t pps_seen;
    uint8_t fix_valid;
    char rmc_status;
    uint8_t aligned;
    uint8_t holdover;
    uint8_t bad_period;
    uint8_t resync_pending;
    char utc[TK8710_PPS_UTC_MAX];
} TK8710PpsStatus;

typedef struct {
    double latitude;
    double longitude;
    uint8_t valid;
} TK8710PpsPosition;

typedef struct {
    char pps[TK8710_PPS_DIAG_TEXT_MAX];
    char output[TK8710_PPS_DIAG_TEXT_MAX];
    char sync[TK8710_PPS_DIAG_TEXT_MAX];
    char gps[TK8710_PPS_DIAG_TEXT_MAX];
    char rmc[TK8710_PPS_DIAG_TEXT_MAX];
    uint32_t success_mask;
} TK8710PpsDiagnostics;

TK8710PpsError TK8710PpsGetDefaultConfig(TK8710PpsConfig* config);
TK8710PpsError TK8710PpsInit(TK8710PpsContext* context,
    const TK8710PpsConfig* config);
void TK8710PpsClose(TK8710PpsContext* context);
TK8710PpsError TK8710PpsSetOutput(TK8710PpsContext* context, uint8_t enable);
TK8710PpsError TK8710PpsGetVersion(TK8710PpsContext* context,
    char* version, uint32_t version_size);
TK8710PpsError TK8710PpsSetPeriod(TK8710PpsContext* context, uint32_t period);
TK8710PpsError TK8710PpsGetPeriod(TK8710PpsContext* context,
    TK8710PpsPeriodInfo* info);
TK8710PpsError TK8710PpsGetStatus(TK8710PpsContext* context,
    TK8710PpsStatus* status);
/**
 * Gets the current decimal-degree position from `GET POS`.
 * Southern and western coordinates are returned as negative values. When the
 * device reports `NA ?`, the call succeeds with `valid` set to zero.
 */
TK8710PpsError TK8710PpsGetPosition(TK8710PpsContext* context,
    TK8710PpsPosition* position);
TK8710PpsHealth TK8710PpsEvaluateHealth(const TK8710PpsStatus* status,
    uint32_t expected_period);
TK8710PpsError TK8710PpsGetDiagnostics(TK8710PpsContext* context,
    TK8710PpsDiagnostics* diagnostics);
TK8710PpsError TK8710PpsResetDevice(TK8710PpsContext* context);
const char* TK8710PpsGetLastEvent(const TK8710PpsContext* context);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_PPS_API_H */
