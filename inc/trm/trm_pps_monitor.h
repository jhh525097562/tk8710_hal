#ifndef TRM_PPS_MONITOR_H
#define TRM_PPS_MONITOR_H
#include <stdint.h>

typedef struct {
    uint32_t cycle_us;
    uint32_t cycles;
    uint32_t super_frames;
    uint8_t rates;
    uint32_t frame_us[4];
    uint32_t s0_us[4];
} TrmPpsConfig;

/* Start/stop only with frame callbacks quiescent. Start before hardware start.
 * GPIO timestamps and all timestamps in this interface are CLOCK_MONOTONIC. */
int TrmPpsMonitorStart(const TrmPpsConfig* config);
void TrmPpsMonitorStop(void);
int TrmPpsMonitorActive(void);
int TrmPpsMonitorFailed(void);
/* Missing GPIO output pulses are governed by the five-cycle policy. */
int TrmPpsMonitorMissing(void);
uint64_t TrmPpsMonotonicUs(void);
void TrmPpsMonitorS0(uint32_t cycle, uint8_t rate, uint32_t super_position);
/* Zero means no trustworthy upcoming PPS: retain pending ACM request. */
uint64_t TrmPpsMonitorDeadline(void);
void TrmPpsMonitorArmRestart(void);
#endif
