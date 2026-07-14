#ifndef TK8710_IPC_COMM_H
#define TK8710_IPC_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "ipc_smp.h"
#include "mac_msg_parser.h"
#include "trm/trm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// 核间通信状态
typedef struct {
    pthread_t thread;
    volatile int running;
    ipc_smp_handle_t *ch_ns_to_gw;  // NS->网关 通道
    ipc_smp_handle_t *ch_gw_to_ns;  // 网关->NS 通道
} IpcCommContext;

/* 核间通信上下文全局变量声明 */
extern IpcCommContext g_ipc_ctx;

// 配置处理回调函数类型
typedef int (*ConfigHandler_t)(const NsConfigDown_t* config);

// 扫频处理回调函数类型
typedef int (*SweepFreqHandler_t)(const SweepFreqReq_t* req);

// 核间通信接口函数
int IpcCommInit(IpcCommContext *ctx);
int IpcCommStart(IpcCommContext *ctx);
int IpcCommStop(IpcCommContext *ctx);
void IpcCommCleanup(IpcCommContext *ctx);
int IpcSendUplinkData(IpcCommContext *ctx, const TRM_RxDataList* rxDataList);

// 配置相关接口函数
void IpcCommSetConfigHandler(ConfigHandler_t handler);
int IpcCommIsConfigReceived(void);
void IpcCommClearConfigReceived(void);
int IpcCommGetReceivedConfig(NsConfigDown_t* config);
int IpcCommSendConfigRequest(IpcCommContext *ctx);

// 扫频相关接口函数
void IpcCommSetSweepFreqHandler(SweepFreqHandler_t handler);
int IpcCommSendSweepFreqResult(IpcCommContext *ctx, const SweepFreqResult_t* result);

#ifdef __cplusplus
}
#endif

#endif // TK8710_IPC_COMM_H
