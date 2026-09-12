#ifndef TK8710_SCAN_IPC_SERVER_H
#define TK8710_SCAN_IPC_SERVER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int TK8710ScanIpcServerStart(void);
void TK8710ScanSetHandlers(int (*start)(uint32_t, uint32_t, int), int (*restore)(void));
void TK8710ScanConfigLock(void);
void TK8710ScanConfigUnlock(void);
int TK8710ScanTaskIsActive(void);
int TK8710ScanSubmit(uint32_t start, uint32_t end, int mode);
void TK8710ScanIpcServerStop(void);
int TK8710ScanIpcServerIsRunning(void);
void TK8710ScanIpcNotifySweepRunning(void);
void TK8710ScanIpcNotifySweepDone(void);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_SCAN_IPC_SERVER_H */
