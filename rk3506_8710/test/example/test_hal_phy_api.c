#if defined(PLATFORM_TMS570)
/* PC/RK test source is intentionally empty for the TMS570 target build. */
#else

#include <stdio.h>
#include "hal_api.h"
#include "phy/phy_api.h"
#include "phy/phy_test.h"

int main(void)
{
    TrmStatus status;
    uint32_t regValue = 0;

    if (TK8710PhySelfTest() != TK8710_OK) {
        return 1;
    }

    if (TK8710HalGetStatus(NULL) != TK8710_HAL_ERROR_PARAM) {
        return 2;
    }

    if (TK8710HalGetStatus(&status) == TK8710_HAL_ERROR_PARAM) {
        return 3;
    }

    if (TK8710PhyReadReg(0x0, NULL) != TK8710_ERR_PARAM) {
        return 4;
    }

    printf("HAL/PHY API headers and wrappers compiled. reg=%u\n", regValue);
    return 0;
}

#endif /* PLATFORM_TMS570 */
