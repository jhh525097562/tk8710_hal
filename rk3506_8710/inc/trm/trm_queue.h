#ifndef TRM_QUEUE_H
#define TRM_QUEUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t TRM_QueueGetCount(void);
uint32_t TRM_QueueGetCapacity(void);

#ifdef __cplusplus
}
#endif

#endif /* TRM_QUEUE_H */
