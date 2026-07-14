#include <stdint.h>
#include "trm/trm_queue.h"

extern uint32_t TRM_GetTxQueueCount(void);
extern uint32_t TRM_GetTxQueueCapacity(void);

uint32_t TRM_QueueGetCount(void)
{
    return TRM_GetTxQueueCount();
}

uint32_t TRM_QueueGetCapacity(void)
{
    return TRM_GetTxQueueCapacity();
}
