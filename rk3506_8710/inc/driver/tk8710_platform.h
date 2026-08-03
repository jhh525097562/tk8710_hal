/**
 * @file tk8710_platform.h
 * @brief Platform compatibility layer for TK8710 driver sources.
 */

#ifndef TK8710_PLATFORM_H
#define TK8710_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PLATFORM_TMS570)
#define TK8710_PLATFORM_HAS_FILE_IO 0
#define TK8710_PLATFORM_HAS_PTHREAD 0
#else
#define TK8710_PLATFORM_HAS_FILE_IO 1
#define TK8710_PLATFORM_HAS_PTHREAD 1
#endif

#if defined(__TI_COMPILER_VERSION__)
#define TK8710_PACKED
#define TK8710_PACKED_BEGIN _Pragma("pack(1)")
#define TK8710_PACKED_END   _Pragma("pack()")
#define TK8710_UNUSED
#define TK8710_ALIGNED(n)
#define TK8710_SECTION_SDRAM
#elif defined(__GNUC__) || defined(__clang__)
#define TK8710_PACKED __attribute__((packed))
#define TK8710_PACKED_BEGIN
#define TK8710_PACKED_END
#define TK8710_UNUSED __attribute__((unused))
#define TK8710_ALIGNED(n) __attribute__((aligned(n)))
#define TK8710_SECTION_SDRAM __attribute__((section(".tk8710_sdram")))
#else
#define TK8710_PACKED
#define TK8710_PACKED_BEGIN
#define TK8710_PACKED_END
#define TK8710_UNUSED
#define TK8710_ALIGNED(n)
#define TK8710_SECTION_SDRAM
#endif

#if defined(PLATFORM_TMS570)
typedef enum {
    TK8710_CAPTURE_STATE_IDLE = 0,
    TK8710_CAPTURE_STATE_ARMING,
    TK8710_CAPTURE_STATE_WAIT_RX,
    TK8710_CAPTURE_STATE_CAPTURING,
    TK8710_CAPTURE_STATE_PROCESSING,
    TK8710_CAPTURE_STATE_READY,
    TK8710_CAPTURE_STATE_ERROR
} TK8710CaptureState;

typedef struct {
    uint32_t generation;
    uint32_t timestampMs;
    uint32_t bytesPerAntenna;
    uint32_t errorCount;
    uint32_t lastSpiUs;
    uint32_t maxSpiUs;
    uint32_t lastFftUs;
    uint32_t maxFftUs;
    int32_t lastError;
    uint8_t rateMode;
    uint8_t validAntennaMask;
    uint8_t state;
    uint8_t activeAntenna;
    float noiseDbmHz[8];
} TK8710CaptureInfo;

void* TK8710PortMalloc(size_t size);
void TK8710PortFree(void* ptr);
void TK8710PortLogWrite(const char* text, size_t len);
int TK8710PortStorageRead(const char* key, uint32_t offset, void* data, size_t len);
int TK8710PortStorageWrite(const char* key, uint32_t offset, const void* data, size_t len);
int TK8710PortStorageErase(const char* key);
/* data == NULL and len == 0 probes whether a capture backend is available. */
int TK8710PortCaptureWrite(const char* stream, const void* data, size_t len);
int TK8710CaptureRequest(uint8_t rateMode);
int TK8710CaptureProcess(void);
int TK8710CaptureCancel(void);
void TK8710CaptureNotifyMdData(void);
int TK8710CaptureGetInfo(TK8710CaptureInfo* info);
int TK8710CaptureReadAntenna(uint32_t generation, uint8_t antenna,
                             uint32_t offset, void* data, uint32_t len);

#define TK8710_MALLOC(size) TK8710PortMalloc(size)
#define TK8710_FREE(ptr)    TK8710PortFree(ptr)
#else
#include <stdlib.h>
#define TK8710_MALLOC(size) malloc(size)
#define TK8710_FREE(ptr)    free(ptr)
#endif

#ifdef __cplusplus
}
#endif

#endif /* TK8710_PLATFORM_H */
