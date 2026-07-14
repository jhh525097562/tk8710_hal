/**
 * @file tk8710_tms570.c
 * @brief TK8710 port for TMS570LS3137 HALCoGen project.
 */

#include "tk8710_tms570.h"
#include "../inc/driver/tk8710_regs.h"
#include "../inc/driver/tk8710_internal.h"

#include "mibspi.h"
#include "gio.h"
#include "rti.h"
#include "sci.h"
#include "sys_core.h"
#include "sys_vim.h"
#include "emif.h"
#include "reg_system.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TK8710_SPI_CMD_RST          0x00U
#define TK8710_SPI_CMD_WR_REG       0x01U
#define TK8710_SPI_CMD_RD_REG       0x02U
#define TK8710_SPI_CMD_WR_BUFF      0x03U
#define TK8710_SPI_CMD_RD_BUFF      0x04U
#define TK8710_SPI_CMD_SET_INFO     0x06U
#define TK8710_SPI_CMD_GET_INFO     0x07U
#define TK8710_REG_SIZE             4U
#define TK8710_NOP_BYTE             0x00U
#define TK8710_SPI_TX_BUF_SIZE      (16384U * 2U + 4U)
#define TK8710_SPI_RX_BUF_SIZE      (16384U * 2U + 4U)

#define TK8710_MIBSPI1_BANK_A_TG    0U
#define TK8710_MIBSPI1_BANK_B_TG    7U
#define TK8710_MIBSPI1_BANK_A       0U
#define TK8710_MIBSPI1_BANK_B       1U
#define TK8710_MIBSPI1_BANK_A_START 0U
#define TK8710_MIBSPI1_BANK_B_START TK8710_TMS570_SPI_BANK_SIZE
#define TK8710_MIBSPI1_RAM_ENTRIES  TK8710_TMS570_SPI_GROUP_WORDS
#define TK8710_MIBSPI1_TIMEOUT      1000000U
#define TK8710_MIBSPI1_STATUS_OK    0U
#define TK8710_MIBSPI1_STATUS_TIMEOUT 0x80000000U
#define TK8710_MIBSPI1_CS0_MASK     ((uint32)1U << PIN_CS0)
#define TK8710_MIBSPI1_CLK_MASK     ((uint32)1U << PIN_CLK)
#define TK8710_MIBSPI1_SIMO_MASK    ((uint32)1U << PIN_SIMO)
#define TK8710_MIBSPI1_SOMI_MASK    ((uint32)1U << PIN_SOMI)
#define TK8710_GIO_HIGH_CHANNEL     9U

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

#define TK8710_MIBSPI1_TGCTRL(start) \
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

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(g_spiTxBuf, ".tk8710_sdram")
#pragma DATA_SECTION(g_spiRxBuf, ".tk8710_sdram")
#pragma DATA_SECTION(g_tk8710Heap, ".tk8710_sdram")
#endif
static uint8_t g_spiTxBuf[TK8710_SPI_TX_BUF_SIZE] TK8710_SECTION_SDRAM;
static uint8_t g_spiRxBuf[TK8710_SPI_RX_BUF_SIZE] TK8710_SECTION_SDRAM;
static uint8_t g_tk8710Heap[TK8710_TMS570_HEAP_SIZE] TK8710_SECTION_SDRAM;

static TK8710HeapBlock* g_heapHead = NULL;
static uint32_t g_heapUsed = 0U;
static uint32_t g_heapPeak = 0U;
static uint32_t g_heapFailCount = 0U;
static uint32_t g_spiErrorCount = 0U;
static uint32_t g_irqCount = 0U;
static volatile uint32_t g_irqPending = 0U;
static volatile uint32_t g_sdramFailPhase = 0U;
static volatile uint32_t g_sdramFailAddress = 0U;
static volatile uint32_t g_sdramExpected = 0U;
static volatile uint32_t g_sdramActual = 0U;
static volatile uint32_t g_sdramFailIndex = 0U;
static uint8_t g_spiInitialized = 0U;
static uint8_t g_portInitialized = 0U;
static uint32_t g_criticalDepth = 0U;

static TK8710GpioIrqCallback g_irqCallback = NULL;
static void* g_irqUser = NULL;
static gioPORT_t* g_irqPort = gioPORTA;
static uint32_t g_irqBit = TK8710_TMS570_IRQ_PIN_DEFAULT;

static uint32_t gio_interrupt_mask(gioPORT_t* port, uint32_t bit);
static void configure_mibspi1_16mhz(void);
static uint32_t transfer_ping_pong(const uint8_t* tx, uint8_t* rx, uint32_t len);
static void load_bank(uint32_t bank, const uint8_t* tx, uint32_t len);
static void start_bank(uint32_t bank);
static uint32_t wait_bank_complete(uint32_t bank);
static uint32_t read_bank(uint32_t bank, uint8_t* rx, uint32_t len);
static uint32_t bank_start(uint32_t bank);
static uint32_t bank_group(uint32_t bank);
static void configure_scilin_uart(void);
static void enable_emif_runtime_access(void);
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

static void configure_mibspi1_16mhz(void)
{
    uint32 i;

    mibspiInit();

    mibspiREG1->GCR1 &= 0xFEFFFFFFU;
    mibspiREG1->GCR1 = (mibspiREG1->GCR1 & 0xFFFFFFFCU) |
                       ((uint32)1U << 1U) |
                       1U;

    mibspiREG1->FMT0 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG1->FMT1 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG1->FMT2 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG1->FMT3 = TK8710_MIBSPI_FMT0_VALUE;
    mibspiREG1->DEF = (uint32)CS_NONE;

    mibspiREG1->PC1 |= TK8710_MIBSPI1_CS0_MASK;
    mibspiREG1->PC4 = TK8710_MIBSPI1_CS0_MASK;
    mibspiREG1->PC0 = (mibspiREG1->PC0 |
                       TK8710_MIBSPI1_CLK_MASK |
                       TK8710_MIBSPI1_SIMO_MASK |
                       TK8710_MIBSPI1_SOMI_MASK) &
                      ~TK8710_MIBSPI1_CS0_MASK;

    mibspiREG1->TGCTRL[0U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_A_START);
    mibspiREG1->TGCTRL[1U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[2U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[3U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[4U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[5U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[6U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->TGCTRL[7U] = TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
    mibspiREG1->LTGPEND = (mibspiREG1->LTGPEND & 0xFFFF00FFU) |
                          ((uint32)(TK8710_MIBSPI1_RAM_ENTRIES - 1U) << 8U);

    for (i = 0U; i < TK8710_MIBSPI1_RAM_ENTRIES; i++) {
        mibspiRAM1->tx[i].control = ((uint16)4U << 13U) |
                                    ((uint16)DATA_FORMAT0 << 8U) |
                                    (uint16)CS_NONE;
        mibspiRAM1->tx[i].data = (uint16)TK8710_NOP_BYTE;
        mibspiRAM1->rx[i].flags = 0U;
        mibspiRAM1->rx[i].data = 0U;
    }

    mibspiREG1->TGINTFLG = ((uint32)1U << (16U + TK8710_MIBSPI1_BANK_A_TG)) |
                           ((uint32)1U << (16U + TK8710_MIBSPI1_BANK_B_TG));
    mibspiREG1->GCR1 = (mibspiREG1->GCR1 & 0xFEFFFFFFU) | 0x01000000U;
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
    status = TK8710_MIBSPI1_STATUS_OK;
    bankLength[TK8710_MIBSPI1_BANK_A] = 0U;
    bankLength[TK8710_MIBSPI1_BANK_B] = 0U;
    bankOffset[TK8710_MIBSPI1_BANK_A] = 0U;
    bankOffset[TK8710_MIBSPI1_BANK_B] = 0U;
    bankValid[TK8710_MIBSPI1_BANK_A] = 0U;
    bankValid[TK8710_MIBSPI1_BANK_B] = 0U;

    if (len == 0U) {
        return status;
    }

    chunk = len - offset;
    if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
        chunk = TK8710_TMS570_SPI_BANK_SIZE;
    }
    load_bank(TK8710_MIBSPI1_BANK_A, (tx != NULL) ? &tx[offset] : NULL, chunk);
    bankLength[TK8710_MIBSPI1_BANK_A] = chunk;
    bankOffset[TK8710_MIBSPI1_BANK_A] = offset;
    bankValid[TK8710_MIBSPI1_BANK_A] = 1U;
    offset += chunk;

    if (offset < len) {
        chunk = len - offset;
        if (chunk > TK8710_TMS570_SPI_BANK_SIZE) {
            chunk = TK8710_TMS570_SPI_BANK_SIZE;
        }
        load_bank(TK8710_MIBSPI1_BANK_B, (tx != NULL) ? &tx[offset] : NULL, chunk);
        bankLength[TK8710_MIBSPI1_BANK_B] = chunk;
        bankOffset[TK8710_MIBSPI1_BANK_B] = offset;
        bankValid[TK8710_MIBSPI1_BANK_B] = 1U;
        offset += chunk;
    }

    TK8710SpiCsControl(1U);
    currentBank = TK8710_MIBSPI1_BANK_A;
    start_bank(currentBank);

    while (bankValid[currentBank] != 0U) {
        status |= wait_bank_complete(currentBank);
        if ((status & TK8710_MIBSPI1_STATUS_TIMEOUT) != 0U) {
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
                  (uint16)CS_0;
        mibspiRAM1->tx[start + i].control = control;
        mibspiRAM1->tx[start + i].data = (tx != NULL) ? (uint16)tx[i] : (uint16)TK8710_NOP_BYTE;
        mibspiRAM1->rx[start + i].flags = 0U;
        mibspiRAM1->rx[start + i].data = 0U;
    }

    if (bank == TK8710_MIBSPI1_BANK_A) {
        mibspiREG1->TGCTRL[TK8710_MIBSPI1_BANK_A_TG] =
            TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_A_START);
        mibspiREG1->TGCTRL[1U] =
            TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_A_START + len);
    } else {
        mibspiREG1->TGCTRL[TK8710_MIBSPI1_BANK_B_TG] =
            TK8710_MIBSPI1_TGCTRL(TK8710_MIBSPI1_BANK_B_START);
        mibspiREG1->LTGPEND = (mibspiREG1->LTGPEND & 0xFFFF00FFU) |
                              ((uint32)(TK8710_MIBSPI1_BANK_B_START + len - 1U) << 8U);
    }
}

static void start_bank(uint32_t bank)
{
    uint32_t group = bank_group(bank);
    mibspiREG1->TGINTFLG = (uint32)((uint32)1U << (16U + group));
    mibspiTransfer(mibspiREG1, group);
}

static uint32_t wait_bank_complete(uint32_t bank)
{
    uint32_t timeout;
    uint32_t group;

    group = bank_group(bank);
    timeout = TK8710_MIBSPI1_TIMEOUT;

    while (mibspiIsTransferComplete(mibspiREG1, group) == FALSE) {
        if (timeout == 0U) {
            return TK8710_MIBSPI1_STATUS_TIMEOUT;
        }
        timeout--;
    }

    return TK8710_MIBSPI1_STATUS_OK;
}

static uint32_t read_bank(uint32_t bank, uint8_t* rx, uint32_t len)
{
    uint32_t i;
    uint32_t start;
    uint32_t status;

    start = bank_start(bank);
    status = TK8710_MIBSPI1_STATUS_OK;

    if (rx != NULL) {
        for (i = 0U; i < len; i++) {
            rx[i] = (uint8_t)(mibspiRAM1->rx[start + i].data & 0x00FFU);
            status |= ((uint32_t)mibspiRAM1->rx[start + i].flags >> 8U) & 0x5FU;
        }
    } else {
        for (i = 0U; i < len; i++) {
            status |= ((uint32_t)mibspiRAM1->rx[start + i].flags >> 8U) & 0x5FU;
        }
    }

    return status;
}

static uint32_t bank_start(uint32_t bank)
{
    return (bank == TK8710_MIBSPI1_BANK_A) ?
           TK8710_MIBSPI1_BANK_A_START :
           TK8710_MIBSPI1_BANK_B_START;
}

static uint32_t bank_group(uint32_t bank)
{
    return (bank == TK8710_MIBSPI1_BANK_A) ?
           TK8710_MIBSPI1_BANK_A_TG :
           TK8710_MIBSPI1_BANK_B_TG;
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
        enable_emif_runtime_access();
        g_portInitialized = 1U;
    }

    if (g_spiInitialized == 0U) {
        configure_mibspi1_16mhz();
        g_spiInitialized = 1U;
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
    if (status != TK8710_MIBSPI1_STATUS_OK) {
        g_spiErrorCount++;
        return -1;
    }

    return 0;
}

void TK8710SpiCsControl(uint8_t active)
{
    if (active != 0U) {
        mibspiREG1->PC5 = TK8710_MIBSPI1_CS0_MASK;
    } else {
        mibspiREG1->PC4 = TK8710_MIBSPI1_CS0_MASK;
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
    uint32_t frc = rtiREG1->CNT[0U].FRCx;
    return (uint64_t)(frc / TK8710_TMS570_RTI_TICKS_PER_US);
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

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    rxLen = 3U + (uint32_t)len;
    if (rxLen > TK8710_SPI_RX_BUF_SIZE) {
        return -1;
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

int TK8710Tms570SdramSelfTest(uint32_t base, uint32_t bytes)
{
    volatile uint16_t* mem = (volatile uint16_t*)base;
    uint32_t halfWords = bytes / sizeof(uint16_t);
    uint32_t i;
    uint16_t expected;
    uint16_t actual;

    sdram_diag_set(0U, 0U, 0U, 0U, 0U);

    if ((base == 0U) || (halfWords == 0U) || ((base & 1U) != 0U)) {
        sdram_diag_set(10U, base, bytes, halfWords, 0U);
        return -1;
    }

    for (i = 0U; i < 16U; i++) {
        expected = (uint16_t)((uint16_t)1U << i);
        mem[0] = expected;
        actual = mem[0];
        if (actual != expected) {
            sdram_diag_set(1U, base, (uint32_t)expected, (uint32_t)actual, i);
            return -1;
        }
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

    return 0;
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
}
