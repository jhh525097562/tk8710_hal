#ifndef TK8710_SCAN_SERVICE_H
#define TK8710_SCAN_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int TK8710ScanStart(uint32_t start_freq, uint32_t end_freq, int sweep_mode);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_SCAN_SERVICE_H */
