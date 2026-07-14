#include "phy/phy_irq.h"
#include "driver/tk8710_driver_api.h"

void TK8710PhySetCbs(const TK8710DriverCbs* cbs)
{
    TK8710RegisterCallbacks(cbs);
}
