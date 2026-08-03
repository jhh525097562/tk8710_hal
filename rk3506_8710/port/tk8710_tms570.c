/**
 * @file tk8710_tms570.c
 * @brief TK8710 port for TMS570LS3137 HALCoGen project.
 */

#include "tk8710_tms570.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_reg_pack.h"

#include "mibspi.h"
#include "gio.h"
#include "rti.h"
#include "sci.h"
#include "sys_core.h"
#include "sys_vim.h"
#include "emif.h"
#include "reg_pinmux.h"
#include "reg_system.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define TK8710_SPI_CMD_RST          0x00U
#define TK8710_SPI_CMD_WR_REG       0x01U
#define TK8710_SPI_CMD_RD_REG       0x02U
#define TK8710_SPI_CMD_WR_BUFF      0x03U
#define TK8710_SPI_CMD_RD_BUFF      0x04U
#define TK8710_SPI_CMD_SET_INFO     0x06U
#define TK8710_SPI_CMD_GET_INFO     0x07U
#define TK8710_REG_SIZE             4U
#define TK8710_NOP_BYTE             0x00U
#define TK8710_SPI_TX_BUF_SIZE      5164U
#define TK8710_SPI_RX_BUF_SIZE      5164U

#define TK8710_MIBSPI3_BANK_A_TG    0U
#define TK8710_MIBSPI3_BANK_B_TG    7U
#define TK8710_MIBSPI3_BANK_A       0U
#define TK8710_MIBSPI3_BANK_B       1U
#define TK8710_MIBSPI3_BANK_A_START 0U
#define TK8710_MIBSPI3_BANK_B_START TK8710_TMS570_SPI_BANK_SIZE
#define TK8710_MIBSPI3_RAM_ENTRIES  TK8710_TMS570_SPI_GROUP_WORDS
#define TK8710_MIBSPI3_TIMEOUT      1000000U
#define TK8710_MIBSPI3_STATUS_OK    0U
#define TK8710_MIBSPI3_STATUS_TIMEOUT 0x80000000U
#define TK8710_MIBSPI3_CS3_MASK     ((uint32)1U << PIN_CS3)
#define TK8710_MIBSPI3_CLK_MASK     ((uint32)1U << PIN_CLK)
#define TK8710_MIBSPI3_SIMO_MASK    ((uint32)1U << PIN_SIMO)
#define TK8710_MIBSPI3_SOMI_MASK    ((uint32)1U << PIN_SOMI)
#define TK8710_GIO_HIGH_CHANNEL     9U

#define TK8710_CAPTURE_ANTENNAS             8U
#define TK8710_CAPTURE_BANKS                2U
#define TK8710_CAPTURE_MAX_RAW_BYTES        32768U
#define TK8710_CAPTURE_MAX_FFT_LEN          8192U
#define TK8710_CAPTURE_FIRST_MD_WAIT_COUNT  1U
#define TK8710_CAPTURE_RX_WAIT_COUNT        2U
#define TK8710_CAPTURE_ANOISE_THE1          4U
#define TK8710_CAPTURE_ANOISE_THE2          5U
#define TK8710_CAPTURE_ANOISE_MIN_VAL       (1.0F / 2048.0F)
#define TK8710_CAPTURE_ANOISE_OFFSET        (-140.0F)
#define TK8710_CAPTURE_PI                   3.14159265358979F
#define TK8710_CAPTURE_ERR_PARAM            (-1)
#define TK8710_CAPTURE_ERR_BUSY             (-2)
#define TK8710_CAPTURE_ERR_ARM              (-3)
#define TK8710_CAPTURE_ERR_SPI              (-4)
#define TK8710_CAPTURE_ERR_PROCESS          (-5)
#define TK8710_CAPTURE_ERR_UNAVAILABLE      (-6)

#ifndef TK8710_TMS570_RTI_TICKS_PER_US
#define TK8710_TMS570_RTI_TICKS_PER_US 10U
#endif

#define TK8710_TMS570_SCILIN_VCLK_HZ 80000000UL
#define TK8710_TMS570_SCILIN_BRS \
    (((TK8710_TMS570_SCILIN_VCLK_HZ + (TK8710_TMS570_UART_BAUD * 8UL)) / \
      (TK8710_TMS570_UART_BAUD * 16UL)) - 1UL)

#define TK8710_MIBSPI_FMT0_VALUE \
    (((uint32)0U << 24U) | \
     ((uint32)0U << 23U) | \
     ((uint32)0U << 22U) | \
     ((uint32)0U << 21U) | \
     ((uint32)0U << 20U) | \
     ((uint32)1U << 17U) | \
     ((uint32)0U << 16U) | \
     ((uint32)TK8710_TMS570_SPI_PRESCALE << 8U) | \
     ((uint32)TK8710_TMS570_SPI_BITS << 0U))

#define TK8710_MIBSPI3_TGCTRL(start) \
    (0xFFFF7FFFU & (((uint32)1U << 30U) | \
     ((uint32)0U << 29U) | \
     ((uint32)TRG_ALWAYS << 20U) | \
     ((uint32)TRG_DISABLED << 16U) | \
     ((uint32)(start) << 8U)))

typedef struct TK8710HeapBlock {
    uint32_t size;
    uint32_t free;
    struct TK8710HeapBlock* next;
} TK8710HeapBlock;

typedef struct {
    float real;
    float imag;
} TK8710CaptureComplex;

typedef struct {
    TK8710CaptureState state;
    uint8_t rateMode;
    uint8_t activeAntenna;
    uint8_t processAntenna;
    uint8_t waitRxCount;
    uint8_t waitRxTarget;
    uint8_t nextWaitRxTarget;
    uint8_t writeBank;
    uint8_t publishedBank;
    uint8_t publishedValid;
    uint8_t validAntennaMask;
    uint32_t rawBytes;
    uint32_t fftLength;
    uint32_t twiddleLength;
    uint32_t generation;
    uint32_t publishedTimestampMs;
    uint32_t publishedRawBytes;
    uint32_t errorCount;
    uint32_t lastSpiUs;
    uint32_t maxSpiUs;
    uint32_t lastFftUs;
    uint32_t maxFftUs;
    int32_t lastError;
    uint8_t publishedRateMode;
    uint8_t publishedValidMask;
    float workingNoiseDbmHz[TK8710_CAPTURE_ANTENNAS];
    float publishedNoiseDbmHz[TK8710_CAPTURE_ANTENNAS];
} TK8710CaptureContext;

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(g_captureBanks, ".tk8710_sdram")
#pragma DATA_SECTION(g_captureFft, ".tk8710_sdram")
#pragma DATA_SECTION(g_capturePower, ".tk8710_sdram")
#pragma DATA_SECTION(g_captureTwiddle, ".tk8710_sdram")
#endif
static uint8_t g_spiTxBuf[TK8710_SPI_TX_BUF_SIZE];
static uint8_t g_spiRxBuf[TK8710_SPI_RX_BUF_SIZE];
static uint8_t g_tk8710Heap[TK8710_TMS570_HEAP_SIZE];
static uint8_t g_captureBanks[TK8710_CAPTURE_BANKS][TK8710_CAPTURE_ANTENNAS]
                             [TK8710_CAPTURE_MAX_RAW_BYTES] TK8710_SECTION_SDRAM;
static TK8710CaptureComplex g_captureFft[TK8710_CAPTURE_MAX_FFT_LEN]
                                                TK8710_SECTION_SDRAM;
static float g_capturePower[TK8710_CAPTURE_MAX_FFT_LEN] TK8710_SECTION_SDRAM;
static TK8710CaptureComplex g_captureTwiddle[TK8710_CAPTURE_MAX_FFT_LEN]
                                                     TK8710_SECTION_SDRAM;

static TK8710HeapBlock* g_heapHead = NULL;
static uint32_t g_heapUsed = 0U;
static uint32_t g_heapPeak = 0U;
static uint32_t g_heapFailCount = 0U;
static uint32_t g_spiErrorCount = 0U;
static uint32_t g_irqCount = 0U;
static uint32_t g_irqEdgeCount = 0U;
static uint32_t g_irqLevelRecoveryCount = 0U;
static uint32_t g_irqStatusPollCount = 0U;
static uint32_t g_spiResetCount = 0U;
static uint32_t g_resetDriveLowCount = 0U;
static uint32_t g_resetPinLowCount = 0U;
static uint32_t g_portInitCount = 0U;
static uint32_t g_spiInitCount = 0U;
static volatile uint32_t g_irqPending = 0U;
static volatile uint32_t g_sdramFailPhase = 0U;
static volatile uint32_t g_sdramFailAddress = 0U;
static volatile uint32_t g_sdramExpected = 0U;
static volatile uint32_t g_sdramActual = 0U;
static volatile uint32_t g_sdramFailIndex = 0U;
static uint8_t g_spiInitialized = 0U;
static uint8_t g_portInitialized = 0U;
static uint8_t g_sdramAvailable = 0U;
static uint32_t g_criticalDepth = 0U;
static uint32_t g_rtiLastFrc = 0U;
static uint64_t g_rtiElapsedTicks = 0U;
static uint8_t g_rtiTimeInitialized = 0U;
static TK8710CaptureContext g_captureContext;

static TK8710GpioIrqCallback g_irqCallback = NULL;
static void* g_irqUser = NULL;
static gioPORT_t* g_irqPort = gioPORTA;
static uint32_t g_irqBit = TK8710_TMS570_IRQ_PIN_DEFAULT;

static uint32_t gio_interrupt_mask(gioPORT_t* port, uint32_t bit);
static void configure_mibspi3_16mhz(void);
static uint32_t transfer_ping_pong(const uint8_t* tx, uint8_t* rx, uint32_t len);
static uint32_t transfer_get_info(uint8_t infoType, uint8_t* data,
                                  uint32_t dataLen);
static void load_bank(uint32_t bank, const uint8_t* tx, uint32_t len);
static void load_get_info_bank(uint32_t bank, uint8_t infoType,
                               uint32_t offset, uint32_t len);
static void start_bank(uint32_t bank);
static uint32_t wait_bank_complete(uint32_t bank);
static uint32_t read_bank(uint32_t bank, uint8_t* rx, uint32_t len);
static uint32_t read_get_info_bank(uint32_t bank, uint8_t* data,
                                   uint32_t dataLen, uint32_t offset,
                                   uint32_t len);
static uint32_t bank_start(uint32_t bank);
static uint32_t bank_group(uint32_t bank);
static void configure_scilin_uart(void);
static void enable_emif_runtime_access(void);
static int capture_get_config(uint8_t rateMode, uint32_t* rawBytes,
                              uint32_t* fftLength);
static int capture_set_enable(uint8_t enable);
static int capture_fail(int error);
static void capture_prepare_twiddles(uint32_t fftLength);
static int capture_process_antenna(uint8_t antenna);
static void capture_publish(void);
void TK8710Tms570GioHighLevelInterrupt(void);

static uint32_t align8(uint32_t value)
{
    return (value + 7U) & ~7U;
}

static void heap_init_once(void)
{
    if (g_heapHead == NULL) {
        uintptr_t start = ((uintptr_t)g_tk8710Heap + 7U) & ~(uintptr_t)7U;
        uintptr_t end = ((uintptr_t)g_tk8710Heap + sizeof(g_tk8710Heap)) & ~(uintptr_t)7U;

        g_heapHead = (TK8710HeapBlock*)start;
        g_heapHead->size = (uint32_t)(end - start - sizeof(TK8710HeapBlock));
        g_heapHead->free = 1U;
        g_heapHead->next = NULL;
        g_heapUsed = 0U;
        g_heapPeak = 0U;
        g_heapFailCount = 0U;
    }
}

void* TK8710PortMalloc(size_t size)
{
    TK8710HeapBlock* block;
    uint32_t need;

    if (size == 0U) {
        return NULL;
    }

    heap_init_once();
    need = align8((uint32_t)size);

    TK8710EnterCritical();
    block = g_heapHead;
    while (block != NULL) {
        if ((block->free != 0U) && (block->size >= need)) {
            if (block->size >= (need + sizeof(TK8710HeapBlock) + 8U)) {
                TK8710HeapBlock* split = (TK8710HeapBlock*)((uint8_t*)(block + 1) + need);
                split->size = block->size - need - (uint32_t)sizeof(TK8710HeapBlock);
                split->free = 1U;
                split->next = block->next;
                block->next = split;
                block->size = need;
            }
            block->free = 0U;
            g_heapUsed += block->size;
            if (g_heapUsed > g_heapPeak) {
                g_heapPeak = g_heapUsed;
            }
            TK8710ExitCritical();
            return (void*)(block + 1);
        }
        block = block->next;
    }

    g_heapFailCount++;
    TK8710ExitCritical();
    return NULL;
}

void TK8710PortFree(void* ptr)
{
    TK8710HeapBlock* block;

    if (ptr == NULL) {
        return;
    }

    TK8710EnterCritical();
    block = ((TK8710HeapBlock*)ptr) - 1;
    if (block->free == 0U) {
        block->free = 1U;
        if (g_heapUsed >= block->size) {
            g_heapUsed -= block->size;
        } else {
            g_heapUsed = 0U;
        }
    }

    block = g_heapHead;
    while ((block != NULL) && (block->next != NULL)) {
        if ((block->free != 0U) && (block->next->free != 0U)) {
            block->size += (uint32_t)sizeof(TK8710HeapBlock) + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
    TK8710ExitCritical();
}

int TK8710PortStorageRead(const char* key, uint32_t offset, void* data, size_t len)
{
    (void)key;
    (void)offset;
    (void)data;
    (void)len;
    return -1;
}

int TK8710PortStorageWrite(const char* key, uint32_t offset, const void* data, size_t len)
{
    (void)key;
    (void)offset;
    (void)data;
    (void)len;
    return -1;
}

int TK8710PortStorageErase(const char* key)
{
    (void)key;
    return -1;
}

int TK8710PortCaptureWrite(const char* stream, const void* data, size_t len)
{
    if ((stream != NULL) && (data == NULL) && (len == 0U)) {
        return 0;
    }
    return -1;
}

static int capture_get_config(uint8_t rateMode, uint32_t* rawBytes,
                              uint32_t* fftLength)
{
    uint32_t rawValues;

    if ((rawBytes == NULL) || (fftLength == NULL)) {
        return TK8710_CAPTURE_ERR_PARAM;
    }

    switch (rateMode) {
        case 5U:
        case 6U:
        case 7U:
        case 8U:
            rawValues = 16384U;
            *fftLength = 8192U;
            break;
        case 9U:
            rawValues = 8192U;
            *fftLength = 4096U;
            break;
        case 10U:
            rawValues = 4096U;
            *fftLength = 2048U;
            break;
        case 11U:
        case 18U:
            rawValues = 2048U;
            *fftLength = 1024U;
            break;
        default:
            return TK8710_CAPTURE_ERR_PARAM;
    }

    *rawBytes = rawValues * 2U;
    return 0;
}

static int capture_set_enable(uint8_t enable)
{
    uint32_t value = TK8710_S_RAM_RD0_CAP_EN_ENCODE((enable != 0U) ? 1U : 0U);

    return TK8710WriteReg(TK8710_REG_TYPE_GLOBAL,
                          RX_MUP_BASE + offsetof(struct rx_mup, ram_rd0),
                          value);
}

static int capture_fail(int error)
{
    (void)capture_set_enable(0U);
    g_captureContext.state = TK8710_CAPTURE_STATE_ERROR;
    g_captureContext.lastError = error;
    g_captureContext.errorCount++;
    return error;
}

static void capture_prepare_twiddles(uint32_t fftLength)
{
    uint32_t index;

    if (g_captureContext.twiddleLength == fftLength) {
        return;
    }

    for (index = 0U; index < (fftLength / 2U); index++) {
        float angle = (-2.0F * TK8710_CAPTURE_PI * (float)index) /
                      (float)fftLength;
        g_captureTwiddle[index].real = cosf(angle);
        g_captureTwiddle[index].imag = sinf(angle);
    }
    g_captureContext.twiddleLength = fftLength;
}

static void capture_fft_radix2(uint32_t fftLength)
{
    uint32_t index;
    uint32_t reversed = 0U;
    uint32_t step;

    for (index = 0U; index < (fftLength - 1U); index++) {
        uint32_t bit;

        if (index < reversed) {
            TK8710CaptureComplex temp = g_captureFft[index];
            g_captureFft[index] = g_captureFft[reversed];
            g_captureFft[reversed] = temp;
        }

        bit = fftLength >> 1U;
        while ((bit != 0U) && ((reversed & bit) != 0U)) {
            reversed &= ~bit;
            bit >>= 1U;
        }
        reversed |= bit;
    }

    for (step = 1U; step < fftLength; step <<= 1U) {
        uint32_t groupSize = step << 1U;
        uint32_t twiddleStride = fftLength / groupSize;
        uint32_t offset;

        for (offset = 0U; offset < step; offset++) {
            TK8710CaptureComplex w = g_captureTwiddle[offset * twiddleStride];
            uint32_t base;

            for (base = offset; base < fftLength; base += groupSize) {
                uint32_t pair = base + step;
                TK8710CaptureComplex upper = g_captureFft[base];
                TK8710CaptureComplex lower = g_captureFft[pair];
                TK8710CaptureComplex product;

                product.real = w.real * lower.real - w.imag * lower.imag;
                product.imag = w.real * lower.imag + w.imag * lower.real;
                g_captureFft[base].real = upper.real + product.real;
                g_captureFft[base].imag = upper.imag + product.imag;
                g_captureFft[pair].real = upper.real - product.real;
                g_captureFft[pair].imag = upper.imag - product.imag;
            }
        }
    }
}

static int16_t capture_decode_le16(const uint8_t* data)
{
    uint16_t value = (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
    return (int16_t)value;
}

static int capture_process_antenna(uint8_t antenna)
{
    const uint8_t* raw;
    uint32_t fftLength = g_captureContext.fftLength;
    uint32_t index;
    float minimum;
    float maximum;
    float noiseFloor;

    if ((antenna >= TK8710_CAPTURE_ANTENNAS) ||
        (fftLength == 0U) || (fftLength > TK8710_CAPTURE_MAX_FFT_LEN)) {
        return TK8710_CAPTURE_ERR_PROCESS;
    }

    raw = g_captureBanks[g_captureContext.writeBank][antenna];
    capture_prepare_twiddles(fftLength);
    for (index = 0U; index < fftLength; index++) {
        int16_t real = capture_decode_le16(&raw[index * 4U]);
        int16_t imag = capture_decode_le16(&raw[index * 4U + 2U]);
        g_captureFft[index].real = (float)real / 32768.0F;
        g_captureFft[index].imag = (float)imag / 32768.0F;
    }
    if (fftLength >= 3U) {
        for (index = fftLength - 3U; index < fftLength; index++) {
            g_captureFft[index].real = 0.0F;
            g_captureFft[index].imag = 0.0F;
        }
    }

    capture_fft_radix2(fftLength);
    g_captureFft[0] = g_captureFft[1];

    for (index = 0U; index < fftLength; index++) {
        float real = g_captureFft[index].real;
        float imag = g_captureFft[index].imag;
        g_capturePower[index] = real * real + imag * imag;
    }

    minimum = g_capturePower[0];
    maximum = g_capturePower[0];
    for (index = 1U; index < fftLength; index++) {
        if (g_capturePower[index] < minimum) {
            minimum = g_capturePower[index];
        }
        if (g_capturePower[index] > maximum) {
            maximum = g_capturePower[index];
        }
    }
    if (minimum < TK8710_CAPTURE_ANOISE_MIN_VAL) {
        minimum = TK8710_CAPTURE_ANOISE_MIN_VAL;
    }
    if (maximum < TK8710_CAPTURE_ANOISE_MIN_VAL) {
        maximum = TK8710_CAPTURE_ANOISE_MIN_VAL;
    }

    if (maximum == minimum) {
        noiseFloor = 1.0F;
    } else {
        uint32_t iteration;

        noiseFloor = sqrtf(minimum * maximum);
        for (iteration = 0U; iteration < 7U; iteration++) {
            float threshold = sqrtf(minimum * maximum);
            uint32_t below = 0U;

            for (index = 0U; index < fftLength; index++) {
                if (g_capturePower[index] < threshold) {
                    below++;
                }
            }
            if (below < (fftLength / TK8710_CAPTURE_ANOISE_THE1)) {
                minimum = threshold;
            } else {
                maximum = threshold;
            }
            noiseFloor = sqrtf(minimum * maximum);
            if ((maximum / minimum) <=
                ((float)TK8710_CAPTURE_ANOISE_THE2 /
                 (float)TK8710_CAPTURE_ANOISE_THE1)) {
                break;
            }
        }
    }

    g_captureContext.workingNoiseDbmHz[antenna] =
        10.0F * log10f(noiseFloor) + TK8710_CAPTURE_ANOISE_OFFSET;
    return 0;
}

static void capture_publish(void)
{
    uint32_t antenna;

    TK8710EnterCritical();
    g_captureContext.publishedBank = g_captureContext.writeBank;
    g_captureContext.publishedValid = 1U;
    g_captureContext.generation++;
    g_captureContext.publishedTimestampMs = TK8710GetTickMs();
    g_captureContext.publishedRawBytes = g_captureContext.rawBytes;
    g_captureContext.publishedRateMode = g_captureContext.rateMode;
    g_captureContext.publishedValidMask = g_captureContext.validAntennaMask;
    for (antenna = 0U; antenna < TK8710_CAPTURE_ANTENNAS; antenna++) {
        g_captureContext.publishedNoiseDbmHz[antenna] =
            g_captureContext.workingNoiseDbmHz[antenna];
    }
    g_captureContext.lastError = 0;
    g_captureContext.state = TK8710_CAPTURE_STATE_READY;
    TK8710ExitCritical();
}

int TK8710CaptureRequest(uint8_t rateMode)
{
    uint32_t rawBytes;
    uint32_t fftLength;

    if (g_sdramAvailable == 0U) {
        return TK8710_CAPTURE_ERR_UNAVAILABLE;
    }
    if (capture_get_config(rateMode, &rawBytes, &fftLength) != 0) {
        return TK8710_CAPTURE_ERR_PARAM;
    }
    if ((g_captureContext.state == TK8710_CAPTURE_STATE_ARMING) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_WAIT_RX) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_CAPTURING) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_PROCESSING)) {
        return TK8710_CAPTURE_ERR_BUSY;
    }

    g_captureContext.rateMode = rateMode;
    g_captureContext.rawBytes = rawBytes;
    g_captureContext.fftLength = fftLength;
    g_captureContext.activeAntenna = 0U;
    g_captureContext.processAntenna = 0U;
    g_captureContext.waitRxCount = 0U;
    g_captureContext.waitRxTarget =
        (g_captureContext.nextWaitRxTarget != 0U) ?
        g_captureContext.nextWaitRxTarget :
        TK8710_CAPTURE_FIRST_MD_WAIT_COUNT;
    g_captureContext.nextWaitRxTarget = TK8710_CAPTURE_RX_WAIT_COUNT;
    g_captureContext.validAntennaMask = 0U;
    g_captureContext.writeBank = (g_captureContext.publishedValid != 0U) ?
                                 (uint8_t)(g_captureContext.publishedBank ^ 1U) : 0U;
    (void)memset(g_captureContext.workingNoiseDbmHz, 0,
                 sizeof(g_captureContext.workingNoiseDbmHz));
    g_captureContext.lastError = 0;
    g_captureContext.state = TK8710_CAPTURE_STATE_ARMING;
    return 0;
}

void TK8710CaptureNotifyMdData(void)
{
    if (g_captureContext.state == TK8710_CAPTURE_STATE_WAIT_RX) {
        if (g_captureContext.waitRxCount < g_captureContext.waitRxTarget) {
            g_captureContext.waitRxCount++;
        }
        if (g_captureContext.waitRxCount >= g_captureContext.waitRxTarget) {
            g_captureContext.state = TK8710_CAPTURE_STATE_CAPTURING;
        }
    }
}

int TK8710CaptureProcess(void)
{
    if (g_sdramAvailable == 0U) {
        return TK8710_CAPTURE_ERR_UNAVAILABLE;
    }

    if (g_captureContext.state == TK8710_CAPTURE_STATE_ARMING) {
        if (capture_set_enable(1U) != TK8710_OK) {
            return capture_fail(TK8710_CAPTURE_ERR_ARM);
        }
        g_captureContext.waitRxCount = 0U;
        g_captureContext.state = TK8710_CAPTURE_STATE_WAIT_RX;
        return 0;
    }

    if (g_captureContext.state == TK8710_CAPTURE_STATE_CAPTURING) {
        uint8_t antenna = g_captureContext.activeAntenna;
        uint8_t* buffer = g_captureBanks[g_captureContext.writeBank][antenna];
        uint32_t sample;
        uint64_t beginUs;
        uint64_t endUs;

        (void)memset(buffer, 0, g_captureContext.rawBytes);
        beginUs = TK8710GetTimeUs();
        if (TK8710SpiGetInfo((uint8_t)(TK8710_GET_INFO_CAPTURE_0 + antenna),
                             buffer,
                             (uint16_t)(g_captureContext.rawBytes - 10U)) != 0) {
            return capture_fail(TK8710_CAPTURE_ERR_SPI);
        }
        endUs = TK8710GetTimeUs();
        g_captureContext.lastSpiUs = (uint32_t)(endUs - beginUs);
        if (g_captureContext.lastSpiUs > g_captureContext.maxSpiUs) {
            g_captureContext.maxSpiUs = g_captureContext.lastSpiUs;
        }
        for (sample = 0U; sample < (g_captureContext.rawBytes / 2U); sample++) {
            uint8_t high = buffer[sample * 2U];
            buffer[sample * 2U] = buffer[sample * 2U + 1U];
            buffer[sample * 2U + 1U] = high;
        }
        g_captureContext.validAntennaMask |= (uint8_t)(1U << antenna);
        g_captureContext.activeAntenna++;
        if (g_captureContext.activeAntenna >= TK8710_CAPTURE_ANTENNAS) {
            if (capture_set_enable(0U) != TK8710_OK) {
                return capture_fail(TK8710_CAPTURE_ERR_ARM);
            }
            g_captureContext.processAntenna = 0U;
            g_captureContext.state = TK8710_CAPTURE_STATE_PROCESSING;
        }
        return 0;
    }

    if (g_captureContext.state == TK8710_CAPTURE_STATE_PROCESSING) {
        uint64_t beginUs = TK8710GetTimeUs();
        int result = capture_process_antenna(g_captureContext.processAntenna);
        uint64_t endUs = TK8710GetTimeUs();
        int32_t noiseCentiDb;
        uint32_t noiseMagnitude;
        const char* noiseSign;
        g_captureContext.lastFftUs = (uint32_t)(endUs - beginUs);
        if (g_captureContext.lastFftUs > g_captureContext.maxFftUs) {
            g_captureContext.maxFftUs = g_captureContext.lastFftUs;
        }
        if (result != 0) {
            return capture_fail(result);
        }
        noiseCentiDb = (int32_t)(
            g_captureContext.workingNoiseDbmHz[g_captureContext.processAntenna] *
            100.0F);
        if (noiseCentiDb < 0) {
            noiseSign = "-";
            noiseMagnitude = (uint32_t)(-(noiseCentiDb + 1)) + 1U;
        } else {
            noiseSign = "";
            noiseMagnitude = (uint32_t)noiseCentiDb;
        }
        TK8710_LOG_CORE_WARN(
            "[CAPTURE] generation=%lu antenna=%u noiseDbmHz=%s%lu.%02lu fftUs=%lu",
            (unsigned long)(g_captureContext.generation + 1U),
            (unsigned int)(g_captureContext.processAntenna + 1U),
            noiseSign,
            (unsigned long)(noiseMagnitude / 100U),
            (unsigned long)(noiseMagnitude % 100U),
            (unsigned long)g_captureContext.lastFftUs);
        g_captureContext.processAntenna++;
        if (g_captureContext.processAntenna >= TK8710_CAPTURE_ANTENNAS) {
            capture_publish();
        }
    }
    return 0;
}

int TK8710CaptureCancel(void)
{
    int result = 0;

    if ((g_captureContext.state == TK8710_CAPTURE_STATE_ARMING) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_WAIT_RX) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_CAPTURING) ||
        (g_captureContext.state == TK8710_CAPTURE_STATE_PROCESSING)) {
        if (capture_set_enable(0U) != TK8710_OK) {
            result = TK8710_CAPTURE_ERR_ARM;
        }
    }
    g_captureContext.state = TK8710_CAPTURE_STATE_IDLE;
    g_captureContext.activeAntenna = 0U;
    g_captureContext.processAntenna = 0U;
    g_captureContext.waitRxCount = 0U;
    g_captureContext.waitRxTarget = 0U;
    g_captureContext.nextWaitRxTarget = TK8710_CAPTURE_FIRST_MD_WAIT_COUNT;
    g_captureContext.lastError = result;
    return result;
}

int TK8710CaptureGetInfo(TK8710CaptureInfo* info)
{
    uint32_t antenna;

    if (info == NULL) {
        return TK8710_CAPTURE_ERR_PARAM;
    }

    TK8710EnterCritical();
    (void)memset(info, 0, sizeof(*info));
    info->generation = g_captureContext.generation;
    info->timestampMs = g_captureContext.publishedTimestampMs;
    info->bytesPerAntenna = g_captureContext.publishedRawBytes;
    info->errorCount = g_captureContext.errorCount;
    info->lastSpiUs = g_captureContext.lastSpiUs;
    info->maxSpiUs = g_captureContext.maxSpiUs;
    info->lastFftUs = g_captureContext.lastFftUs;
    info->maxFftUs = g_captureContext.maxFftUs;
    info->lastError = g_captureContext.lastError;
    info->rateMode = g_captureContext.publishedRateMode;
    info->validAntennaMask = g_captureContext.publishedValidMask;
    info->state = (uint8_t)g_captureContext.state;
    info->activeAntenna = (g_captureContext.state == TK8710_CAPTURE_STATE_PROCESSING) ?
                          g_captureContext.processAntenna :
                          g_captureContext.activeAntenna;
    for (antenna = 0U; antenna < TK8710_CAPTURE_ANTENNAS; antenna++) {
        info->noiseDbmHz[antenna] = g_captureContext.publishedNoiseDbmHz[antenna];
    }
    TK8710ExitCritical();
    return 0;
}

int TK8710CaptureReadAntenna(uint32_t generation, uint8_t antenna,
                             uint32_t offset, void* data, uint32_t len)
{
    if ((g_sdramAvailable == 0U) ||
        (data == NULL) || (antenna >= TK8710_CAPTURE_ANTENNAS) ||
        (g_captureContext.publishedValid == 0U) ||
        (generation != g_captureContext.generation) ||
        (offset > g_captureContext.publishedRawBytes) ||
        (len > (g_captureContext.publishedRawBytes - offset))) {
        return TK8710_CAPTURE_ERR_PARAM;
    }

    (void)memcpy(data,
                 &g_captureBanks[g_captureContext.publishedBank][antenna][offset],
                 len);
    return 0;
}

void TK8710PortLogWrite(const char* text, size_t len)
{
    size_t i;

    if (text == NULL) {
        return;
    }

    for (i = 0U; i < len; i++) {
        uint8 ch = (uint8)text[i];
        while (sciIsTxReady(scilinREG) == 0U) {
        }
        sciSendByte(scilinREG, ch);
    }
}

static gioPORT_t* gpio_port_from_pin(int pin, uint32_t* bit)
{
    if (pin < 0) {
        *bit = TK8710_TMS570_IRQ_PIN_DEFAULT;
        return gioPORTA;
    }

    if (pin < 8) {
        *bit = (uint32_t)pin;
        return gioPORTA;
    }

    *bit = (uint32_t)(pin - 8);
    return gioPORTB;
}

static gioPORT_t* irq_port_from_pin(int pin, uint32_t* bit)
{
    if (pin <= 0) {
        *bit = TK8710_TMS570_IRQ_PIN_DEFAULT;
        return gioPORTA;
    }

    return gpio_port_from_pin(pin, bit);
}

static uint32_t gio_interrupt_mask(gioPORT_t* port, uint32_t bit)
{
    return (port == gioPORTB) ? ((uint32_t)1U << (bit + 8U)) : ((uint32_t)1U << bit);
}

static void configure_mibspi3_16mhz(void)
{
    uint32 i;

    mibspiInit();

    mibspiREG3->GCR1 &= 0xFEFFFFFFU;
    mibspiREG3->GCR1 = (mibspiREG3->GCR1 & 0xFFFFFFFCU) |
                       ((uint32)1U << 1U) |
                       1U;

    mibspiREG3->FMT0 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG3->FMT1 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG3->FMT2 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG3->FMT3 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG3->DEF = (uint32)CS_NONE;

    mibspiREG3->PC1 |= TK8710_MIBSPI3_CS3_MASK;
    mibspiREG3->PC4 = TK8710_MIBSPI3_CS3_MASK;
    mibspiREG3->PC0 = (mibspiREG3->PC0 |
                       TK8710_MIBSPI3_CLK_MASK |
                       TK8710_MIBSPI3_SIMO_MASK |
                       TK8710_MIBSPI3_SOMI_MASK) &
                      ~TK8710_MIBSPI3_CS3_MASK;

    mibspiREG3->TGCTRL[0U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_A_START);
    mibspiREG3->TGCTRL[1U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[2U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[3U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[4U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[5U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[6U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->TGCTRL[7U] = TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
    mibspiREG3->LTGPEND = (mibspiREG3->LTGPEND & 0xFFFF00FFU) |
                          ((uint32)(TK8710_MIBSPI3_RAM_ENTRIES - 1U) << 8U);

    for (i = 0U; i < TK8710_MIBSPI3_RAM_ENTRIES; i++) {
        mibspiRAM3->tx[i].control = ((uint16)4U << 13U) |
                                    ((uint16)DATA_FORMAT0 << 8U) |
                                    (uint16)CS_NONE;
        mibspiRAM3->tx[i].data = (uint16)TK8710_NOP_BYTE;
        mibspiRAM3->rx[i].flags = 0U;
        mibspiRAM3->rx[i].data = 0U;
    }

    mibspiREG3->TGINTFLG = ((uint32)1U << (16U + TK8710_MIBSPI3_BANK_A_TG)) |
                           ((uint32)1U << (16U + TK8710_MIBSPI3_BANK_B_TG));
    mibspiREG3->GCR1 = (mibspiREG3->GCR1 & 0xFEFFFFFFU) | 0x01000000U;
}

static uint32_t transfer_ping_pong(const uint8_t* tx, uint8_t* rx, uint32_t len)
{
    uint32_t offset;
    uint32_t chunk;
    uint32_t status;
    uint32_t currentBank;
    uint32_t nextBank;
    uint32_t bankLength[2U];
    uint32_t bankOffset[2U];
    uint32_t bankValid[2U];

    offset = 0U;
    status = TK8710_MIBSPI3_STATUS_OK;
    bankLength[TK8710_MIBSPI3_BANK_A] = 0U;
    bankLength[TK8710_MIBSPI3_BANK_B] = 0U;
    bankOffset[TK8710_MIBSPI3_BANK_A] = 0U;
    bankOffset[TK8710_MIBSPI3_BANK_B] = 0U;
    bankValid[TK8710_MIBSPI3_BANK_A] = 0U;
    bankValid[TK8710_MIBSPI3_BANK_B] = 0U;

    if (len == 0U) {
        return status;
    }

    chunk = len - offset;
    if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
        chunk = TK8710_TMS570_SPI_BANK_SIZE;
    }
    load_bank(TK8710_MIBSPI3_BANK_A, (tx != NULL) ? &tx[offset] : NULL, chunk);
    bankLength[TK8710_MIBSPI3_BANK_A] = chunk;
    bankOffset[TK8710_MIBSPI3_BANK_A] = offset;
    bankValid[TK8710_MIBSPI3_BANK_A] = 1U;
    offset += chunk;

    if (offset < len) {
        chunk = len - offset;
        if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
            chunk = TK8710_TMS570_SPI_BANK_SIZE;
        }
        load_bank(TK8710_MIBSPI3_BANK_B, (tx != NULL) ? &tx[offset] : NULL, chunk);
        bankLength[TK8710_MIBSPI3_BANK_B] = chunk;
        bankOffset[TK8710_MIBSPI3_BANK_B] = offset;
        bankValid[TK8710_MIBSPI3_BANK_B] = 1U;
        offset += chunk;
    }

    TK8710SpiCsControl(1U);
    currentBank = TK8710_MIBSPI3_BANK_A;
    start_bank(currentBank);

    while (bankValid[currentBank] != 0U) {
        status |= wait_bank_complete(currentBank);
        if ((status & TK8710_MIBSPI3_STATUS_TIMEOUT) != 0U) {
            break;
        }

        nextBank = currentBank ^ 1U;
        if (bankValid[nextBank] != 0U) {
            start_bank(nextBank);
            status |= read_bank(currentBank,
                                (rx != NULL) ? &rx[bankOffset[currentBank]] : NULL,
                                bankLength[currentBank]);
            bankValid[currentBank] = 0U;

            if (offset < len) {
                chunk = len - offset;
                if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
                    chunk = TK8710_TMS570_SPI_BANK_SIZE;
                }

                load_bank(currentBank, (tx != NULL) ? &tx[offset] : NULL, chunk);
                bankLength[currentBank] = chunk;
                bankOffset[currentBank] = offset;
                bankValid[currentBank] = 1U;
                offset += chunk;
            }

            currentBank = nextBank;
        } else {
            status |= read_bank(currentBank,
                                (rx != NULL) ? &rx[bankOffset[currentBank]] : NULL,
                                bankLength[currentBank]);
            bankValid[currentBank] = 0U;

            if (offset < len) {
                chunk = len - offset;
                if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
                    chunk = TK8710_TMS570_SPI_BANK_SIZE;
                }

                load_bank(currentBank, (tx != NULL) ? &tx[offset] : NULL, chunk);
                bankLength[currentBank] = chunk;
                bankOffset[currentBank] = offset;
                bankValid[currentBank] = 1U;
                offset += chunk;
                start_bank(currentBank);
            }
        }
    }

    TK8710SpiCsControl(0U);

    return status;
}

static uint32_t transfer_get_info(uint8_t infoType, uint8_t* data,
                                  uint32_t dataLen)
{
    uint32_t totalLen = dataLen + 3U;
    uint32_t offset = 0U;
    uint32_t chunk;
    uint32_t status = TK8710_MIBSPI3_STATUS_OK;
    uint32_t currentBank;
    uint32_t nextBank;
    uint32_t bankLength[2U] = {0U, 0U};
    uint32_t bankOffset[2U] = {0U, 0U};
    uint32_t bankValid[2U] = {0U, 0U};

    chunk = totalLen;
    if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
        chunk = TK8710_TMS570_SPI_BANK_SIZE;
    }
    load_get_info_bank(TK8710_MIBSPI3_BANK_A, infoType, offset, chunk);
    bankLength[TK8710_MIBSPI3_BANK_A] = chunk;
    bankOffset[TK8710_MIBSPI3_BANK_A] = offset;
    bankValid[TK8710_MIBSPI3_BANK_A] = 1U;
    offset += chunk;

    if (offset < totalLen) {
        chunk = totalLen - offset;
        if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
            chunk = TK8710_TMS570_SPI_BANK_SIZE;
        }
        load_get_info_bank(TK8710_MIBSPI3_BANK_B, infoType, offset, chunk);
        bankLength[TK8710_MIBSPI3_BANK_B] = chunk;
        bankOffset[TK8710_MIBSPI3_BANK_B] = offset;
        bankValid[TK8710_MIBSPI3_BANK_B] = 1U;
        offset += chunk;
    }

    TK8710SpiCsControl(1U);
    currentBank = TK8710_MIBSPI3_BANK_A;
    start_bank(currentBank);

    while (bankValid[currentBank] != 0U) {
        status |= wait_bank_complete(currentBank);
        if ((status & TK8710_MIBSPI3_STATUS_TIMEOUT) != 0U) {
            break;
        }

        nextBank = currentBank ^ 1U;
        if (bankValid[nextBank] != 0U) {
            start_bank(nextBank);
        }

        status |= read_get_info_bank(currentBank, data, dataLen,
                                     bankOffset[currentBank],
                                     bankLength[currentBank]);
        bankValid[currentBank] = 0U;

        if (offset < totalLen) {
            chunk = totalLen - offset;
            if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
                chunk = TK8710_TMS570_SPI_BANK_SIZE;
            }
            load_get_info_bank(currentBank, infoType, offset, chunk);
            bankLength[currentBank] = chunk;
            bankOffset[currentBank] = offset;
            bankValid[currentBank] = 1U;
            offset += chunk;
        }

        if (bankValid[nextBank] != 0U) {
            currentBank = nextBank;
        } else if (bankValid[currentBank] != 0U) {
            start_bank(currentBank);
        }
    }

    TK8710SpiCsControl(0U);
    return status;
}

static void load_get_info_bank(uint32_t bank, uint8_t infoType,
                               uint32_t offset, uint32_t len)
{
    uint8_t tx[TK8710_TMS570_SPI_BANK_SIZE];
    uint32_t i;

    for (i = 0U; i < len; i++) {
        uint32_t position = offset + i;
        if (position == 0U) {
            tx[i] = TK8710_SPI_CMD_GET_INFO;
        } else if (position == 1U) {
            tx[i] = infoType;
        } else {
            tx[i] = TK8710_NOP_BYTE;
        }
    }
    load_bank(bank, tx, len);
}

static uint32_t read_get_info_bank(uint32_t bank, uint8_t* data,
                                   uint32_t dataLen, uint32_t offset,
                                   uint32_t len)
{
    uint8_t rx[TK8710_TMS570_SPI_BANK_SIZE];
    uint32_t i;
    uint32_t status = read_bank(bank, rx, len);

    for (i = 0U; i < len; i++) {
        uint32_t position = offset + i;
        if ((position >= 3U) && ((position - 3U) < dataLen)) {
            data[position - 3U] = rx[i];
        }
    }
    return status;
}

static void load_bank(uint32_t bank, const uint8_t* tx, uint32_t len)
{
    uint32_t i;
    uint32_t start;
    uint16 control;

    start = bank_start(bank);

    for (i = 0U; i < len; i++) {
        control = ((uint16)4U << 13U) |
                  (uint16)(((i + 1U) < len) ? ((uint16)1U << 12U) : 0U) |
                  ((uint16)DATA_FORMAT0 << 8U) |
                  (uint16)CS_3;
        mibspiRAM3->tx[start + i].control = control;
        mibspiRAM3->tx[start + i].data = (tx != NULL) ? (uint16)tx[i] : (uint16)TK8710_NOP_BYTE;
        mibspiRAM3->rx[start + i].flags = 0U;
        mibspiRAM3->rx[start + i].data = 0U;
    }

    if (bank == TK8710_MIBSPI3_BANK_A) {
        mibspiREG3->TGCTRL[TK8710_MIBSPI3_BANK_A_TG] =
            TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_A_START);
        mibspiREG3->TGCTRL[1U] =
            TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_A_START + len);
    } else {
        mibspiREG3->TGCTRL[TK8710_MIBSPI3_BANK_B_TG] =
            TK8710_MIBSPI3_TGCTRL(TK8710_MIBSPI3_BANK_B_START);
        mibspiREG3->LTGPEND = (mibspiREG3->LTGPEND & 0xFFFF00FFU) |
                              ((uint32)(TK8710_MIBSPI3_BANK_B_START + len - 1U) << 8U);
    }
}

static void start_bank(uint32_t bank)
{
    uint32_t group = bank_group(bank);
    mibspiREG3->TGINTFLG = (uint32)((uint32)1U << (16U + group));
    mibspiTransfer(mibspiREG3, group);
}

static uint32_t wait_bank_complete(uint32_t bank)
{
    uint32_t timeout;
    uint32_t group;

    group = bank_group(bank);
    timeout = TK8710_MIBSPI3_TIMEOUT;

    while (mibspiIsTransferComplete(mibspiREG3, group) == FALSE) {
        if (timeout == 0U) {
            return TK8710_MIBSPI3_STATUS_TIMEOUT;
        }
        timeout--;
    }

    return TK8710_MIBSPI3_STATUS_OK;
}

static uint32_t read_bank(uint32_t bank, uint8_t* rx, uint32_t len)
{
    uint32_t i;
    uint32_t start;
    uint32_t status;

    start = bank_start(bank);
    status = TK8710_MIBSPI3_STATUS_OK;

    if (rx != NULL) {
        for (i = 0U; i < len; i++) {
            rx[i] = (uint8_t)(mibspiRAM3->rx[start + i].data & 0x00FFU);
            status |= ((uint32_t)mibspiRAM3->rx[start + i].flags >> 8U) & 0x5FU;
        }
    } else {
        for (i = 0U; i < len; i++) {
            status |= ((uint32_t)mibspiRAM3->rx[start + i].flags >> 8U) & 0x5FU;
        }
    }

    return status;
}

static uint32_t bank_start(uint32_t bank)
{
    return (bank == TK8710_MIBSPI3_BANK_A) ?
           TK8710_MIBSPI3_BANK_A_START :
           TK8710_MIBSPI3_BANK_B_START;
}

static uint32_t bank_group(uint32_t bank)
{
    return (bank == TK8710_MIBSPI3_BANK_A) ?
           TK8710_MIBSPI3_BANK_A_TG :
           TK8710_MIBSPI3_BANK_B_TG;
}

static void sdram_diag_set(uint32_t phase,
                           uint32_t address,
                           uint32_t expected,
                           uint32_t actual,
                           uint32_t index)
{
    g_sdramFailPhase = phase;
    g_sdramFailAddress = address;
    g_sdramExpected = expected;
    g_sdramActual = actual;
    g_sdramFailIndex = index;
}

static void configure_scilin_uart(void)
{
    scilinREG->GCR0 = 0U;
    scilinREG->GCR0 = 1U;
    scilinREG->CLEARINT = 0xFFFFFFFFU;
    scilinREG->CLEARINTLVL = 0xFFFFFFFFU;
    scilinREG->GCR1 = (uint32)((uint32)1U << 25U)
                    | (uint32)((uint32)1U << 24U)
                    | (uint32)((uint32)1U << 5U)
                    | (uint32)((uint32)(2U - 1U) << 4U)
                    | (uint32)((uint32)1U << 1U);
    scilinREG->BRS = (uint32)TK8710_TMS570_SCILIN_BRS;
    scilinREG->FORMAT = 8U - 1U;
    scilinREG->PIO0 = (uint32)((uint32)1U << 2U)
                    | (uint32)((uint32)1U << 1U);
    scilinREG->PIO3 = 0U;
    scilinREG->PIO1 = 0U;
    scilinREG->PIO6 = 0U;
    scilinREG->PIO7 = 0U;
    scilinREG->PIO8 = (uint32)((uint32)1U << 2U)
                    | (uint32)((uint32)1U << 1U);
    scilinREG->SETINTLVL = 0U;
    scilinREG->SETINT = 0U;
    scilinREG->GCR1 |= 0x80U;
}

static void enable_emif_runtime_access(void)
{
    systemREG1->GPREG1 |= 0x80000000U;
    *((volatile unsigned char*)(&emifREG->SDCR) + 0x0U) = 0x00U;
}

int TK8710Tms570Init(void)
{
    if (g_portInitialized == 0U) {
        gioInit();
        configure_scilin_uart();
        rtiInit();
        rtiStartCounter(rtiCOUNTER_BLOCK0);
        g_rtiLastFrc = rtiREG1->CNT[0U].FRCx;
        g_rtiElapsedTicks = 0U;
        g_rtiTimeInitialized = 1U;
        enable_emif_runtime_access();
        g_portInitialized = 1U;
        g_portInitCount++;
    }

    if (g_spiInitialized == 0U) {
        configure_mibspi3_16mhz();
        g_spiInitialized = 1U;
        g_spiInitCount++;
    }

    return 0;
}

int TK8710SpiInit(const SpiConfig* cfg)
{
    (void)cfg;
    return TK8710Tms570Init();
}

int TK8710SpiWrite(const uint8_t* tx, size_t len)
{
    return TK8710SpiTransfer(tx, NULL, len);
}

int TK8710SpiRead(uint8_t* rx, size_t len)
{
    return TK8710SpiTransfer(NULL, rx, len);
}

int TK8710SpiTransfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    uint32_t status;

    if ((len == 0U) || ((tx == NULL) && (rx == NULL))) {
        return -1;
    }

    if (g_spiInitialized == 0U) {
        if (TK8710Tms570Init() != 0) {
            return -1;
        }
    }

    status = transfer_ping_pong(tx, rx, (uint32_t)len);
    if (status != TK8710_MIBSPI3_STATUS_OK) {
        g_spiErrorCount++;
        return -1;
    }

    return 0;
}

void TK8710SpiCsControl(uint8_t active)
{
    if (active != 0U) {
        mibspiREG3->PC5 = TK8710_MIBSPI3_CS3_MASK;
    } else {
        mibspiREG3->PC4 = TK8710_MIBSPI3_CS3_MASK;
    }
}

int TK8710GpioInit(int pin, TK8710GpioEdge edge, TK8710GpioIrqCallback cb, void* user)
{
    uint32_t mask;

    if (g_portInitialized == 0U) {
        (void)TK8710Tms570Init();
    }

    g_irqPort = irq_port_from_pin(pin, &g_irqBit);
    g_irqCallback = cb;
    g_irqUser = user;
    g_irqPending = 0U;

    g_irqPort->DIR &= ~((uint32)1U << g_irqBit);
    mask = gio_interrupt_mask(g_irqPort, g_irqBit);
    gioREG->ENACLR = mask;
    gioREG->FLG = mask;

    if (edge == TK8710_GPIO_EDGE_BOTH) {
        gioREG->INTDET |= mask;
    } else {
        gioREG->INTDET &= ~mask;
        if (edge == TK8710_GPIO_EDGE_FALLING) {
            gioREG->POL &= ~mask;
        } else {
            gioREG->POL |= mask;
        }
    }

    gioREG->LVLSET = mask;
    vimChannelMap(TK8710_GIO_HIGH_CHANNEL,
                  TK8710_GIO_HIGH_CHANNEL,
                  &TK8710Tms570GioHighLevelInterrupt);
    vimEnableInterrupt(TK8710_GIO_HIGH_CHANNEL, SYS_IRQ);
    return 0;
}

int TK8710GpioIrqEnable(uint8_t gpioPin, uint8_t enable)
{
    uint32_t bit;
    gioPORT_t* port = irq_port_from_pin((int)gpioPin, &bit);

    if ((gpioPin == 0U) && (g_irqPort != NULL)) {
        port = g_irqPort;
        bit = g_irqBit;
    }

    if (enable != 0U) {
        gioEnableNotification(port, bit);
    } else {
        gioDisableNotification(port, bit);
    }

    return 0;
}

void TK8710GpioWrite(int pin, uint8_t level)
{
    uint32_t bit;
    gioPORT_t* port = gpio_port_from_pin(pin, &bit);

    port->DIR |= ((uint32)1U << bit);
    gioSetBit(port, bit, (level != 0U) ? 1U : 0U);
    if ((port == gioPORTA) &&
        (bit == (uint32_t)TK8710_TMS570_RST_PIN_DEFAULT) &&
        (level == 0U)) {
        g_resetDriveLowCount++;
    }
}

uint8_t TK8710GpioRead(int pin)
{
    uint32_t bit;
    gioPORT_t* port = gpio_port_from_pin(pin, &bit);

    return (uint8_t)gioGetBit(port, bit);
}

void TK8710DelayUs(uint32_t us)
{
    uint64_t start = TK8710GetTimeUs();
    while ((TK8710GetTimeUs() - start) < (uint64_t)us) {
    }
}

int TK8710SleepUntilUs(uint64_t targetUs)
{
    while (TK8710GetTimeUs() < targetUs) {
    }
    return 0;
}

void TK8710DelayMs(uint32_t ms)
{
    while (ms-- > 0U) {
        TK8710DelayUs(1000U);
    }
}

uint32_t TK8710GetTickMs(void)
{
    return (uint32_t)(TK8710GetTimeUs() / 1000U);
}

uint64_t TK8710GetTimeUs(void)
{
    uint32_t cpsr = _getCPSRValue_();
    uint32_t frc;
    uint64_t elapsedTicks;

    /*
     * RTI FRC0 wraps every 2^32 ticks (about 429.5 seconds at 10 ticks/us).
     * Accumulate the unsigned delta so an FRC wrap does not make the public
     * microsecond clock jump back to zero.  The payload main loop calls this
     * function much more frequently than one wrap period.
     */
    _disable_IRQ_interrupt_();
    frc = rtiREG1->CNT[0U].FRCx;
    if (g_rtiTimeInitialized == 0U) {
        g_rtiLastFrc = frc;
        g_rtiElapsedTicks = 0U;
        g_rtiTimeInitialized = 1U;
    } else {
        g_rtiElapsedTicks += (uint32_t)(frc - g_rtiLastFrc);
        g_rtiLastFrc = frc;
    }
    elapsedTicks = g_rtiElapsedTicks;
    if ((cpsr & 0x80U) == 0U) {
        _enable_interrupt_();
    }

    return elapsedTicks / TK8710_TMS570_RTI_TICKS_PER_US;
}

void TK8710EnterCritical(void)
{
    _disable_IRQ_interrupt_();
    g_criticalDepth++;
}

void TK8710ExitCritical(void)
{
    if (g_criticalDepth > 0U) {
        g_criticalDepth--;
    }
    if (g_criticalDepth == 0U) {
        _enable_interrupt_();
    }
}

void gioNotification(gioPORT_t *port, uint32 bit)
{
    if ((port == g_irqPort) && (bit == g_irqBit)) {
        g_irqCount++;
        g_irqEdgeCount++;
        g_irqPending = 1U;
    }
}

void TK8710Tms570PollIrq(void)
{
    uint32_t pending;
    TK8710GpioIrqCallback callback;
    void* user;

    TK8710EnterCritical();
    pending = g_irqPending;
    g_irqPending = 0U;
    callback = g_irqCallback;
    user = g_irqUser;
    TK8710ExitCritical();

    if ((pending != 0U) && (callback != NULL)) {
        callback(user);
    }
}

#pragma CODE_STATE(TK8710Tms570GioHighLevelInterrupt, 32)
#pragma INTERRUPT(TK8710Tms570GioHighLevelInterrupt, IRQ)
void TK8710Tms570GioHighLevelInterrupt(void)
{
    uint32 offset;

    offset = gioREG->OFF1;
    if (offset != 0U) {
        offset--;
        if (offset >= 8U) {
            gioNotification(gioPORTB, offset - 8U);
        } else {
            gioNotification(gioPORTA, offset);
        }
    }
}

int TK8710SpiReset(uint8_t resetConfig)
{
    uint8_t txBuf[2];

    txBuf[0] = TK8710_SPI_CMD_RST;
    txBuf[1] = resetConfig;
    g_spiResetCount++;

    return TK8710SpiWrite(txBuf, sizeof(txBuf));
}

int TK8710SpiWriteReg(uint16_t addr, const uint32_t* data, uint8_t regCount)
{
    uint32_t txLen;
    uint8_t i;

    if ((data == NULL) || (regCount == 0U)) {
        return -1;
    }

    txLen = 1U + 2U + ((uint32_t)regCount * TK8710_REG_SIZE);
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }

    g_spiTxBuf[0] = TK8710_SPI_CMD_WR_REG;
    g_spiTxBuf[1] = (uint8_t)(addr >> 8);
    g_spiTxBuf[2] = (uint8_t)(addr & 0xFFU);

    for (i = 0U; i < regCount; i++) {
        uint32_t regData = data[i];
        uint32_t offset = 3U + ((uint32_t)i * TK8710_REG_SIZE);
        g_spiTxBuf[offset + 0U] = (uint8_t)(regData >> 24);
        g_spiTxBuf[offset + 1U] = (uint8_t)(regData >> 16);
        g_spiTxBuf[offset + 2U] = (uint8_t)(regData >> 8);
        g_spiTxBuf[offset + 3U] = (uint8_t)(regData & 0xFFU);
    }

    return TK8710SpiWrite(g_spiTxBuf, txLen);
}

int TK8710SpiReadReg(uint16_t addr, uint32_t* data, uint8_t regCount)
{
    uint32_t rxLen;
    uint8_t i;

    if ((data == NULL) || (regCount == 0U)) {
        return -1;
    }

    rxLen = 3U + 1U + ((uint32_t)regCount * TK8710_REG_SIZE);
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
    }

    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_RD_REG;
    g_spiTxBuf[1] = (uint8_t)(addr >> 8);
    g_spiTxBuf[2] = (uint8_t)(addr & 0xFFU);

    if (TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen) != 0) {
        return -1;
    }

    for (i = 0U; i < regCount; i++) {
        uint32_t offset = 4U + ((uint32_t)i * TK8710_REG_SIZE);
        data[i] = ((uint32_t)g_spiRxBuf[offset + 0U] << 24) |
                  ((uint32_t)g_spiRxBuf[offset + 1U] << 16) |
                  ((uint32_t)g_spiRxBuf[offset + 2U] << 8) |
                  ((uint32_t)g_spiRxBuf[offset + 3U]);
    }

    return 0;
}

int TK8710SpiWriteBuffer(uint8_t bufferIndex, const uint8_t* data, uint16_t len)
{
    uint32_t txLen;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    txLen = 2U + (uint32_t)len;
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }

    g_spiTxBuf[0] = TK8710_SPI_CMD_WR_BUFF;
    g_spiTxBuf[1] = bufferIndex;
    memcpy(&g_spiTxBuf[2], data, len);

    return TK8710SpiWrite(g_spiTxBuf, txLen);
}

int TK8710SpiReadBuffer(uint8_t bufferIndex, uint8_t* data, uint16_t len)
{
    uint32_t rxLen;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    rxLen = 3U + (uint32_t)len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
    }

    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_RD_BUFF;
    g_spiTxBuf[1] = bufferIndex;

    if (TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen) != 0) {
        return -1;
    }

    memcpy(data, &g_spiRxBuf[3], len);
    return 0;
}

int TK8710SpiSetInfo(uint8_t infoType, const uint8_t* data, uint16_t len)
{
    uint32_t txLen;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    txLen = 2U + (uint32_t)len;
    if (txLen > TK8710_SPI_TX_BUF_SIZE) {
        return -1;
    }

    g_spiTxBuf[0] = TK8710_SPI_CMD_SET_INFO;
    g_spiTxBuf[1] = infoType;
    memcpy(&g_spiTxBuf[2], data, len);

    return TK8710SpiWrite(g_spiTxBuf, txLen);
}

int TK8710SpiGetInfo(uint8_t infoType, uint8_t* data, uint16_t len)
{
    uint32_t rxLen;
    uint32_t status;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    rxLen = 3U + (uint32_t)len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        if (g_spiInitialized == 0U) {
            if (TK8710Tms570Init() != 0) {
                return -1;
            }
        }
        status = transfer_get_info(infoType, data, (uint32_t)len);
        if (status != TK8710_MIBSPI3_STATUS_OK) {
            g_spiErrorCount++;
            return -1;
        }
        return 0;
    }

    memset(g_spiTxBuf, TK8710_NOP_BYTE, rxLen);
    g_spiTxBuf[0] = TK8710_SPI_CMD_GET_INFO;
    g_spiTxBuf[1] = infoType;

    if (TK8710SpiTransfer(g_spiTxBuf, g_spiRxBuf, rxLen) != 0) {
        return -1;
    }

    memcpy(data, &g_spiRxBuf[3], len);
    return 0;
}

int TK8710GpioSet(const char* chipPath, unsigned int lineOffset, uint8_t level)
{
    (void)chipPath;
    TK8710GpioWrite((int)lineOffset, level);
    return 0;
}

int TK8710GpioGet(const char* chipPath, unsigned int lineOffset)
{
    (void)chipPath;
    return (int)TK8710GpioRead((int)lineOffset);
}

int TK8710Tms570SdramRecoverAtLowClock(void)
{
    uint32_t originalClk2Cntl = systemREG2->CLK2CNTL;
    volatile uint32_t settle;

    /* Reinitialize below 40 MHz as required by the HALCoGen EMIF startup flow. */
    systemREG2->CLK2CNTL = (originalClk2Cntl & 0xFFFFFFF0U) | 0x00000007U;
    settle = systemREG2->CLK2CNTL;
    for (settle = 0U; settle < 1024U; settle++) {
    }

    emif_SDRAM_StartupInit();

    systemREG2->CLK2CNTL = originalClk2Cntl;
    settle = systemREG2->CLK2CNTL;
    for (settle = 0U; settle < 1024U; settle++) {
    }

    systemREG1->GPREG1 |= 0x80000000U;
    *((volatile unsigned char*)(&emifREG->SDCR) + 0x0U) = 0x00U;

    return 0;
}

int TK8710Tms570SdramSelfTest(uint32_t base, uint32_t bytes)
{
    volatile uint16_t* mem = (volatile uint16_t*)base;
    uint32_t halfWords = bytes / sizeof(uint16_t);
    uint32_t i;
    uint32_t walkingFailCount = 0U;
    uint16_t walkingPassMask = 0U;
    uint16_t expected;
    uint16_t actual;

    g_sdramAvailable = 0U;
    sdram_diag_set(0U, 0U, 0U, 0U, 0U);

    if ((base == 0U) || (halfWords == 0U) || ((base & 1U) != 0U)) {
        sdram_diag_set(10U, base, bytes, halfWords, 0U);
        return -1;
    }

    for (i = 0U; i < 16U; i++) {
        expected = (uint16_t)((uint16_t)1U << i);
        mem[0] = expected;
        actual = mem[0];
        if (actual == expected) {
            walkingPassMask = (uint16_t)(walkingPassMask | expected);
        } else {
            walkingFailCount++;
        }
    }
    if (walkingPassMask != 0xFFFFU) {
        sdram_diag_set(1U, base, 0x0000FFFFU,
                       (uint32_t)walkingPassMask, walkingFailCount);
        return -1;
    }

    for (i = 0U; i < halfWords; i++) {
        mem[i] = (uint16_t)(0xA500U | (i & 0x00FFU));
    }
    for (i = 0U; i < halfWords; i++) {
        expected = (uint16_t)(0xA500U | (i & 0x00FFU));
        actual = mem[i];
        if (actual != expected) {
            sdram_diag_set(2U, base + (i * sizeof(uint16_t)),
                           (uint32_t)expected, (uint32_t)actual, i);
            return -1;
        }
    }

    for (i = 0U; i < halfWords; i++) {
        mem[i] = (uint16_t)(0x5A00U | ((~i) & 0x00FFU));
    }
    for (i = 0U; i < halfWords; i++) {
        expected = (uint16_t)(0x5A00U | ((~i) & 0x00FFU));
        actual = mem[i];
        if (actual != expected) {
            sdram_diag_set(3U, base + (i * sizeof(uint16_t)),
                           (uint32_t)expected, (uint32_t)actual, i);
            return -1;
        }
    }

    g_sdramAvailable = 1U;
    return 0;
}

uint8_t TK8710Tms570SdramIsAvailable(void)
{
    return g_sdramAvailable;
}

void TK8710Tms570GetSdramDiag(TK8710Tms570SdramDiag* diag)
{
    if (diag == NULL) {
        return;
    }

    diag->phase = g_sdramFailPhase;
    diag->address = g_sdramFailAddress;
    diag->expected = g_sdramExpected;
    diag->actual = g_sdramActual;
    diag->index = g_sdramFailIndex;
}

void TK8710Tms570GetEmifDiag(TK8710Tms570EmifDiag* diag)
{
    if (diag == NULL) {
        return;
    }

    diag->gpreg1 = systemREG1->GPREG1;
    diag->midr = emifREG->MIDR;
    diag->awcc = emifREG->AWCC;
    diag->sdcr = emifREG->SDCR;
    diag->sdrcr = emifREG->SDRCR;
    diag->sdtimr = emifREG->SDTIMR;
    diag->sdsretr = emifREG->SDSRETR;
    diag->pinmmr29 = pinMuxReg->PINMMR29;
    diag->clk2cntl = systemREG2->CLK2CNTL;
    diag->vclkacon1 = systemREG2->VCLKACON1;
}

void TK8710Tms570GetStats(TK8710Tms570Stats* stats)
{
    if (stats == NULL) {
        return;
    }

    stats->heap_size = (uint32_t)sizeof(g_tk8710Heap);
    stats->heap_used = g_heapUsed;
    stats->heap_peak = g_heapPeak;
    stats->heap_fail_count = g_heapFailCount;
    stats->spi_error_count = g_spiErrorCount;
    stats->irq_count = g_irqCount;
    stats->irq_edge_count = g_irqEdgeCount;
    stats->irq_level_recovery_count = g_irqLevelRecoveryCount;
    stats->irq_status_poll_count = g_irqStatusPollCount;
    stats->spi_reset_count = g_spiResetCount;
    stats->reset_drive_low_count = g_resetDriveLowCount;
    stats->reset_pin_low_count = g_resetPinLowCount;
    stats->port_init_count = g_portInitCount;
    stats->spi_init_count = g_spiInitCount;
    stats->reset_gio_dout = gioPORTA->DOUT;
    stats->reset_gio_dir = gioPORTA->DIR;
    stats->gio_din = g_irqPort->DIN;
    stats->gio_flg = gioREG->FLG;
    stats->gio_enaset = gioREG->ENASET;
    stats->vim_reqmask0 = vimREG->REQMASKSET0;
    stats->irq_pin_level = (uint8_t)gioGetBit(g_irqPort, g_irqBit);
    stats->reset_pin_level = (uint8_t)gioGetBit(
        gioPORTA, TK8710_TMS570_RST_PIN_DEFAULT);
    stats->irq_callback_configured = (g_irqCallback != NULL) ? 1U : 0U;
    stats->sdram_available = g_sdramAvailable;
}
