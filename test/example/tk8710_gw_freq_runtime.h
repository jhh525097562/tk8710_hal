#ifndef TK8710_GW_FREQ_RUNTIME_H
#define TK8710_GW_FREQ_RUNTIME_H
#include "mac_msg_parser.h"
#include <stdint.h>

int GwFreqOpen(const char* work_dir, uint8_t tx_gain);
int GwFreqConfigure(const NsConfigDown_t* config);
/* 1: GPS + broadcast + advancing slots ready; 0: waiting;
 * -1: fault; -2: PPS frame/edge validation fault (startup retry candidate). */
int GwFreqPoll(uint8_t check_gps);
int GwFreqClose(void);
void GwFreqCancel(void);
#endif
