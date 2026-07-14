#ifndef TK8710_PHY_LOG_H
#define TK8710_PHY_LOG_H

#include <stdint.h>
#include "../driver/tk8710_log.h"

#ifdef __cplusplus
extern "C" {
#endif

int TK8710PhyLogConfig(TK8710LogLevel level, uint32_t moduleMask, uint8_t enableFileLogging);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_PHY_LOG_H */
