#ifndef TK8710_SCAN_IPC_SERVER_H
#define TK8710_SCAN_IPC_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

int TK8710ScanIpcServerStart(void);
void TK8710ScanIpcServerStop(void);
int TK8710ScanIpcServerIsRunning(void);
void TK8710ScanIpcNotifySweepRunning(void);
void TK8710ScanIpcNotifySweepDone(void);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_SCAN_IPC_SERVER_H */
