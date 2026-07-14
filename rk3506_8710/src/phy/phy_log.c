#include "phy/phy_log.h"
#include "driver/tk8710_driver_api.h"

int TK8710PhyLogConfig(TK8710LogLevel level, uint32_t moduleMask, uint8_t enableFileLogging)
{
    return TK8710LogConfig(level, moduleMask, enableFileLogging);
}
