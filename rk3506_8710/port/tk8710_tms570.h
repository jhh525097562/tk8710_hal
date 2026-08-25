/**
 * @file tk8710_tms570.h
 * @brief TMS570LS3137 port definitions for TK8710.
 */

#ifndef TK8710_TMS570_H
#define TK8710_TMS570_H

#include <stdint.h>
#include <stddef.h>
#include "tk8710_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TK8710_TMS570_SPI_SPEED_HZ     16000000UL
#define TK8710_TMS570_SPI_BITS         8U
#define TK8710_TMS570_SPI_MODE         2U
#define TK8710_TMS570_SPI_PRESCALE     4U
#define TK8710_TMS570_SPI_BANK_SIZE    64U
#define TK8710_TMS570_SPI_GROUP_WORDS  (TK8710_TMS570_SPI_BANK_SIZE * 2U)
#define TK8710_TMS570_UART_BAUD        1000000UL
#define TK8710_TMS570_IRQ_PIN_DEFAULT  3
#define TK8710_TMS570_RST_PIN_DEFAULT  1
#define TK8710_TMS570_SDRAM_BASE       0x80000000UL
#define TK8710_TMS570_SDRAM_SIZE       (8UL * 1024UL * 1024UL)
#define TK8710_TMS570_SDRAM_TEST_SIZE  (64UL * 1024UL)
#define TK8710_TMS570_SDRAM_TEST_BASE  (TK8710_TMS570_SDRAM_BASE + TK8710_TMS570_SDRAM_SIZE - TK8710_TMS570_SDRAM_TEST_SIZE)
#define TK8710_TMS570_HEAP_SIZE        (8UL * 1024UL)

typedef struct {
    uint32_t heap_size;
    uint32_t heap_used;
    uint32_t heap_peak;
    uint32_t heap_fail_count;
    uint32_t spi_error_count;
    uint32_t irq_count;
    uint32_t irq_edge_count;
    uint32_t irq_level_recovery_count;
    uint32_t irq_status_poll_count;
    uint32_t spi_reset_count;
    uint32_t reset_drive_low_count;
    uint32_t reset_pin_low_count;
    uint32_t port_init_count;
    uint32_t spi_init_count;
    uint32_t reset_gio_dout;
    uint32_t reset_gio_dir;
    uint32_t gio_din;
    uint32_t gio_flg;
    uint32_t gio_enaset;
    uint32_t vim_reqmask0;
    uint8_t irq_pin_level;
    uint8_t reset_pin_level;
    uint8_t irq_callback_configured;
    uint8_t sdram_available;
} TK8710Tms570Stats;

typedef struct {
    uint32_t phase;
    uint32_t address;
    uint32_t expected;
    uint32_t actual;
    uint32_t index;
} TK8710Tms570SdramDiag;

typedef struct {
    uint32_t gpreg1;
    uint32_t midr;
    uint32_t awcc;
    uint32_t sdcr;
    uint32_t sdrcr;
    uint32_t sdtimr;
    uint32_t sdsretr;
    uint32_t pinmmr29;
    uint32_t clk2cntl;
    uint32_t vclkacon1;
} TK8710Tms570EmifDiag;

int TK8710Tms570Init(void);
void TK8710Tms570PollIrq(void);
int TK8710Tms570SdramRecoverAtLowClock(void);
int TK8710Tms570SdramSelfTest(uint32_t base, uint32_t bytes);
uint8_t TK8710Tms570SdramIsAvailable(void);
void TK8710Tms570GetStats(TK8710Tms570Stats* stats);
void TK8710Tms570GetSdramDiag(TK8710Tms570SdramDiag* diag);
void TK8710Tms570GetEmifDiag(TK8710Tms570EmifDiag* diag);

#ifdef __cplusplus
}
#endif

#endif /* TK8710_TMS570_H */
