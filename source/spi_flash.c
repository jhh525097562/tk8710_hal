#include "spi_flash.h"

#include <string.h>

#include "data_transfer.h"

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "mibspi.h"
#include "spi.h"
#endif

#define SPI_FLASH_CMD_WRITE_ENABLE 0x06U
#define SPI_FLASH_CMD_READ_JEDEC_ID 0x9FU
#define SPI_FLASH_CMD_READ_STATUS 0x05U
#define SPI_FLASH_CMD_READ_STATUS2 0x35U
#define SPI_FLASH_CMD_READ 0x03U
#define SPI_FLASH_CMD_PAGE_PROGRAM 0x02U
#define SPI_FLASH_CMD_SECTOR_ERASE 0xD8U
#define SPI_FLASH_STATUS_WIP 0x01U
#define SPI_FLASH_POLL_LIMIT 1000000UL
#define SPI_FLASH_BAUD_PRESCALE 159U
#define SPI_FLASH_DELAY_VALUE 0x01010101U
#define SPI_FLASH_FMT0_VALUE (((uint32_t)1U << 24U) | \
                              ((uint32_t)SPI_FLASH_BAUD_PRESCALE << 8U) | 8U)

#if defined(FPGA_PROTOCOL_HOST_TEST)
static uint8_t g_spiFlash[DATA_TRANSFER_FLASH_TOTAL_SIZE];
static uint8_t g_spiFlashInitialized;
static int g_spiFlashTestInitResult;
static uint8_t g_spiFlashTestProgramPersists = 1U;
static uint32_t g_spiFlashTestEraseCount;
static uint32_t g_spiFlashTestProgramCount;

static void SpiFlash_TestEnsureInitialized(void)
{
    if (g_spiFlashInitialized == 0U)
    {
        (void)memset(g_spiFlash, 0xFF, sizeof(g_spiFlash));
        g_spiFlashInitialized = 1U;
    }
}

void SpiFlash_TestEraseAll(void)
{
    (void)memset(g_spiFlash, 0xFF, sizeof(g_spiFlash));
    g_spiFlashInitialized = 1U;
    g_spiFlashTestInitResult = 0;
    g_spiFlashTestProgramPersists = 1U;
    g_spiFlashTestEraseCount = 0U;
    g_spiFlashTestProgramCount = 0U;
}

void SpiFlash_TestSetInitResult(int result)
{
    g_spiFlashTestInitResult = result;
}

void SpiFlash_TestSetProgramPersists(uint8_t persists)
{
    g_spiFlashTestProgramPersists = persists;
}

uint32_t SpiFlash_TestGetEraseCount(void)
{
    return g_spiFlashTestEraseCount;
}

uint32_t SpiFlash_TestGetProgramCount(void)
{
    return g_spiFlashTestProgramCount;
}

int SpiFlash_Init(void)
{
    SpiFlash_TestEnsureInitialized();
    return g_spiFlashTestInitResult;
}

int SpiFlash_ReadJedecId(uint8_t id[3])
{
    if (id == 0)
    {
        return -1;
    }
    id[0] = 0xEFU;
    id[1] = 0x40U;
    id[2] = 0x18U;
    return 0;
}

int SpiFlash_ReadStatusRegisters(uint8_t *sr1, uint8_t *sr2)
{
    if ((sr1 == 0) || (sr2 == 0))
    {
        return -1;
    }
    *sr1 = 0U;
    *sr2 = 0U;
    return 0;
}

int SpiFlash_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    SpiFlash_TestEnsureInitialized();
    if ((data == 0) || ((address + length) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }
    (void)memcpy(data, &g_spiFlash[address], length);
    return 0;
}

int SpiFlash_EraseSector(uint32_t address)
{
    SpiFlash_TestEnsureInitialized();
    g_spiFlashTestEraseCount++;
    if (((address % DATA_TRANSFER_FLASH_ERASE_SIZE) != 0U) ||
        ((address + DATA_TRANSFER_FLASH_ERASE_SIZE) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }
    (void)memset(&g_spiFlash[address], 0xFF, DATA_TRANSFER_FLASH_ERASE_SIZE);
    return 0;
}

int SpiFlash_PageProgram(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t i;

    SpiFlash_TestEnsureInitialized();
    g_spiFlashTestProgramCount++;
    if ((data == 0) || (length > DATA_TRANSFER_FLASH_PAGE_SIZE) ||
        ((address + length) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }
    if (g_spiFlashTestProgramPersists == 0U)
    {
        return 0;
    }
    for (i = 0U; i < length; i++)
    {
        g_spiFlash[address + i] &= data[i];
    }
    return 0;
}
#else
static void SpiFlash_WriteBe24(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 16U);
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)value;
}

static uint32_t SpiFlash_Transfer(const uint8_t *tx, uint8_t *rx, uint32_t length)
{
    uint16_t txWords[DATA_TRANSFER_FLASH_PAGE_SIZE + 5U];
    uint16_t rxWords[DATA_TRANSFER_FLASH_PAGE_SIZE + 5U];
    spiDAT1_t config;
    uint32_t i;

    if ((tx == 0) || (length > (DATA_TRANSFER_FLASH_PAGE_SIZE + 5U)))
    {
        return 1U;
    }

    for (i = 0U; i < length; i++)
    {
        txWords[i] = tx[i];
        rxWords[i] = 0U;
    }

    config.CS_HOLD = 1U;
    config.WDEL = 1U;
    config.DFSEL = SPI_FMT_0;
    config.CSNR = SPI_CS_0;
    if (spiTransmitAndReceiveData(spiREG5, &config, length, txWords, rxWords) != 0U)
    {
        return 1U;
    }

    if (rx != 0)
    {
        for (i = 0U; i < length; i++)
        {
            rx[i] = (uint8_t)rxWords[i];
        }
    }
    return 0U;
}

static int SpiFlash_WriteEnable(void)
{
    uint8_t tx[1] = {SPI_FLASH_CMD_WRITE_ENABLE};

    return (SpiFlash_Transfer(tx, 0, sizeof(tx)) == 0U) ? 0 : -1;
}

static int SpiFlash_ReadStatus(uint8_t *status)
{
    uint8_t tx[2] = {SPI_FLASH_CMD_READ_STATUS, 0U};
    uint8_t rx[2];

    if ((status == 0) || (SpiFlash_Transfer(tx, rx, sizeof(tx)) != 0U))
    {
        return -1;
    }
    *status = rx[1];
    return 0;
}

int SpiFlash_ReadJedecId(uint8_t id[3])
{
    uint8_t tx[4] = {SPI_FLASH_CMD_READ_JEDEC_ID, 0U, 0U, 0U};
    uint8_t rx[4];

    if (id == 0)
    {
        return -1;
    }
    if (SpiFlash_Transfer(tx, rx, sizeof(tx)) != 0U)
    {
        return -1;
    }
    id[0] = rx[1];
    id[1] = rx[2];
    id[2] = rx[3];
    return 0;
}

int SpiFlash_ReadStatusRegisters(uint8_t *sr1, uint8_t *sr2)
{
    uint8_t tx1[2] = {SPI_FLASH_CMD_READ_STATUS, 0U};
    uint8_t tx2[2] = {SPI_FLASH_CMD_READ_STATUS2, 0U};
    uint8_t rx[2];

    if ((sr1 == 0) || (sr2 == 0))
    {
        return -1;
    }
    if (SpiFlash_Transfer(tx1, rx, sizeof(tx1)) != 0U)
    {
        return -1;
    }
    *sr1 = rx[1];
    if (SpiFlash_Transfer(tx2, rx, sizeof(tx2)) != 0U)
    {
        return -1;
    }
    *sr2 = rx[1];
    return 0;
}

static int SpiFlash_WaitReady(void)
{
    uint32_t i;
    uint8_t status = 0U;

    for (i = 0U; i < SPI_FLASH_POLL_LIMIT; i++)
    {
        if (SpiFlash_ReadStatus(&status) != 0)
        {
            return -1;
        }
        if (status == 0xFFU)
        {
            return -1;
        }
        if ((status & SPI_FLASH_STATUS_WIP) == 0U)
        {
            return 0;
        }
    }
    return -1;
}

int SpiFlash_Init(void)
{
    mibspiREG5->GCR0 = 0U;
    mibspiREG5->GCR0 = 1U;
    mibspiREG5->MIBSPIE = 0U;
    mibspiREG5->GCR1 = (mibspiREG5->GCR1 & 0xFFFFFFFCU) |
                       ((uint32_t)1U << 1U) | 1U;
    mibspiREG5->DELAY = SPI_FLASH_DELAY_VALUE;
    mibspiREG5->FMT0 = SPI_FLASH_FMT0_VALUE;
    mibspiREG5->FMT1 = SPI_FLASH_FMT0_VALUE;
    mibspiREG5->FMT2 = SPI_FLASH_FMT0_VALUE;
    mibspiREG5->FMT3 = SPI_FLASH_FMT0_VALUE;
    mibspiREG5->DEF = CS_NONE;
    mibspiREG5->PC3 = (uint32_t)1U << 0U;
    mibspiREG5->PC1 = ((uint32_t)1U << 0U) |
                      ((uint32_t)1U << 9U) |
                      ((uint32_t)1U << 10U);
    mibspiREG5->PC0 = ((uint32_t)1U << 0U) |
                      ((uint32_t)1U << 9U) |
                      ((uint32_t)1U << 10U) |
                      ((uint32_t)1U << 11U);
    mibspiREG5->GCR1 = (mibspiREG5->GCR1 & 0xFEFFFFFFU) | 0x01000000U;
    return SpiFlash_WaitReady();
}

int SpiFlash_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    uint8_t tx[DATA_TRANSFER_FLASH_PAGE_SIZE + 5U];
    uint8_t rx[DATA_TRANSFER_FLASH_PAGE_SIZE + 5U];
    uint32_t chunk;
    uint32_t done = 0U;

    if ((data == 0) || ((address + length) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }

    while (done < length)
    {
        chunk = length - done;
        if (chunk > DATA_TRANSFER_FLASH_PAGE_SIZE)
        {
            chunk = DATA_TRANSFER_FLASH_PAGE_SIZE;
        }
        (void)memset(tx, 0, sizeof(tx));
        tx[0] = SPI_FLASH_CMD_READ;
        SpiFlash_WriteBe24(&tx[1], address + done);
        if (SpiFlash_Transfer(tx, rx, chunk + 4U) != 0U)
        {
            return -1;
        }
        (void)memcpy(&data[done], &rx[4], chunk);
        done += chunk;
    }
    return 0;
}

int SpiFlash_EraseSector(uint32_t address)
{
    uint8_t tx[4];
    if (((address % DATA_TRANSFER_FLASH_ERASE_SIZE) != 0U) ||
        ((address + DATA_TRANSFER_FLASH_ERASE_SIZE) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }
    if ((SpiFlash_WriteEnable() != 0) || (SpiFlash_WaitReady() != 0))
    {
        return -1;
    }
    tx[0] = SPI_FLASH_CMD_SECTOR_ERASE;
    SpiFlash_WriteBe24(&tx[1], address);
    if ((SpiFlash_Transfer(tx, 0, sizeof(tx)) != 0U) ||
        (SpiFlash_WaitReady() != 0))
    {
        return -1;
    }
    return 0;
}

int SpiFlash_PageProgram(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint8_t tx[DATA_TRANSFER_FLASH_PAGE_SIZE + 4U];

    if ((data == 0) || (length > DATA_TRANSFER_FLASH_PAGE_SIZE) ||
        ((address + length) > DATA_TRANSFER_FLASH_TOTAL_SIZE))
    {
        return -1;
    }
    if ((SpiFlash_WriteEnable() != 0) || (SpiFlash_WaitReady() != 0))
    {
        return -1;
    }
    tx[0] = SPI_FLASH_CMD_PAGE_PROGRAM;
    SpiFlash_WriteBe24(&tx[1], address);
    (void)memcpy(&tx[4], data, length);
    if (SpiFlash_Transfer(tx, 0, length + 4U) != 0U)
    {
        return -1;
    }
    return SpiFlash_WaitReady();
}
#endif
