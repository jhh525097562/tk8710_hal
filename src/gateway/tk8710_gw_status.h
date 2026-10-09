/** Gateway lifecycle snapshot for local LED/Web consumers; not an NS protocol. */
#ifndef TK8710_GW_STATUS_H
#define TK8710_GW_STATUS_H

typedef enum {
    GW_STATUS_STARTING = 0,
    GW_STATUS_WAIT_NS,
    GW_STATUS_CONFIGURING,
    GW_STATUS_WAIT_GPS,
    GW_STATUS_WAIT_PPS,
    GW_STATUS_RUNNING,
    GW_STATUS_DEGRADED,
    GW_STATUS_SCANNING,
    GW_STATUS_STOPPING,
    GW_STATUS_STOPPED,
    GW_STATUS_FAULT
} GwStatusPhase;

/* Linux /run directory, must already have a writable parent. Init failure is
 * observational only and must not change the gateway synchronization policy. */
int GwStatusInit(const char* directory);
void GwStatusSet(GwStatusPhase phase, const char* reason, int error_code);
void GwStatusSetGpsPolicy(int enabled); /* -1 unknown, 0 disabled, 1 required */
void GwStatusTouch(void);              /* Main-loop progress, rate limited to 1s */
void GwStatusFinish(int exit_code);    /* Preserve first fault and mark exited */

#endif
