#include "data_transfer.h"

#include <string.h>

#include "spi_flash.h"

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "spi.h"
#include "tk8710_hal.h"
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
#include "sys_dma.h"
#endif
#endif

#define DATA_TRANSFER_METADATA_MAGIC 0x44545846UL
#define DATA_TRANSFER_METADATA_VERSION 1U
#define DATA_TRANSFER_METADATA_LEN 24U
#define DATA_TRANSFER_RECORD_HEADER_LEN 8U
#define DATA_TRANSFER_RUNTIME_MAGIC 0x44545254UL
#define DATA_TRANSFER_PAYLOAD_VERSION 1U
#define DATA_TRANSFER_FIRST_USER_PAYLOAD_LEN 96U
#define DATA_TRANSFER_CAPTURE_HEADER_LEN 20U
#define DATA_TRANSFER_SWEEP_HEADER_LEN 32U
#define DATA_TRANSFER_SWEEP_POINT_LEN 36U
#define DATA_TRANSFER_SPI2_WAIT_LIMIT 100000UL
#define DATA_TRANSFER_SPI2_IDLE_BYTE 0xFFU
#define DATA_TRANSFER_ALPHABET_PATTERN_TOTAL (64UL * 1024UL)
#define DATA_TRANSFER_ALPHABET_PATTERN_CHUNK 512U

#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
#define DATA_TRANSFER_SPI2_DMA_RX_CHANNEL DMA_CH2
#define DATA_TRANSFER_SPI2_DMA_TX_CHANNEL DMA_CH3
#define DATA_TRANSFER_SPI2_DMA_RX_REQUEST 2U
#define DATA_TRANSFER_SPI2_DMA_TX_REQUEST 3U
#define DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK                                      \
    (((uint32_t)1U << DATA_TRANSFER_SPI2_DMA_RX_CHANNEL) |                       \
     ((uint32_t)1U << DATA_TRANSFER_SPI2_DMA_TX_CHANNEL))
#define DATA_TRANSFER_SPI2_DMA_REQUEST_ENABLE ((uint32_t)1U << 16U)
#define DATA_TRANSFER_SPI2_DMA_PORT_B_ASSIGNMENT 4U
#define DATA_TRANSFER_SPI2_DMA_PORT_B_BYPASS ((uint32_t)1U << 18U)
#define DATA_TRANSFER_SPI2_DMA_COUNT_MASK 0x1FFFU
#endif

#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST) && defined(FPGA_PROTOCOL_HOST_TEST)
#error DATA_TRANSFER_SPI2_SLAVE_TEST is not supported in host tests
#endif

typedef struct
{
    uint32_t runtimeMagic;
    uint32_t head;
    uint32_t tail;
    uint32_t metadataSequence;
    uint32_t metadataNext;
    uint32_t ramLength;
    uint32_t utcBaseSeconds;
    uint32_t utcSyncLocalSeconds;
    uint32_t txFlashRead;
    uint32_t txFlashEnd;
    uint32_t txRamLength;
    uint32_t txRamRead;
    uint32_t txStartTail;
    uint32_t txStartHead;
    uint32_t txStartRamLength;
    uint32_t txStartCount;
    uint32_t txFramesBuilt;
    uint32_t txFramesSent;
    uint32_t txSendErrors;
    uint32_t flashLastError;
    uint32_t flashLastOffset;
    int32_t lastBuildResult;
    int32_t lastSendResult;
    uint16_t txPacketSeq;
    uint16_t lastFrameLength;
    uint16_t lastPacketSeq;
    uint8_t initialized;
    uint8_t transmitActive;
    uint8_t txHadError;
    uint8_t utcValid;
    uint8_t flashAvailable;
    uint8_t ram[DATA_TRANSFER_FLASH_SECTOR_SIZE];
    uint8_t txRam[DATA_TRANSFER_FLASH_SECTOR_SIZE];
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
    uint8_t slaveFrame[DATA_TRANSFER_FRAME_LEN];
    uint32_t slaveTxWords[DATA_TRANSFER_FRAME_LEN];
    uint32_t slaveRxWords[DATA_TRANSFER_FRAME_LEN];
    uint32_t slaveProcessCount;
    uint32_t slaveRxReadyCount;
    uint32_t slavePc2ChangeCount;
    uint32_t slaveLastPc2;
    uint32_t slaveLastFlg;
    uint32_t slaveLastPc0;
    uint32_t slaveLastPc1;
    uint32_t slaveLastGcr1;
    uint32_t slaveDmaStatus;
    uint32_t slaveDmaPending;
    uint32_t slaveDmaHwEnable;
    uint32_t slaveDmaBtcFlag;
    uint32_t slaveLastRxChecksum;
    uint16_t slaveDmaRxRemaining;
    uint16_t slaveDmaTxRemaining;
    uint16_t slaveLastTransferred;
    uint8_t slaveLastRxFirst;
    uint8_t slaveLastRxLast;
    uint8_t slaveDmaStarted;
    uint16_t slaveOffset;
    uint8_t slaveFrameReady;
#endif
} DataTransferContext;

#if !defined(FPGA_PROTOCOL_HOST_TEST) && defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(g_dataTransfer, ".tk8710_sdram")
#endif
static DataTransferContext g_dataTransfer;

#if defined(FPGA_PROTOCOL_HOST_TEST)
static uint32_t g_testTimestamp;
#endif

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint32_t DataTransferReadBe32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           data[3];
}
#endif

static void DataTransferWriteBe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static void DataTransferWriteBe32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static void DataTransferWriteBe64(uint8_t *data, uint64_t value)
{
    DataTransferWriteBe32(&data[0], (uint32_t)(value >> 32U));
    DataTransferWriteBe32(&data[4], (uint32_t)value);
}

static void DataTransferWriteFloatBe32(uint8_t *data, float value)
{
    uint32_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    DataTransferWriteBe32(data, bits);
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint32_t DataTransferChecksum32(const uint8_t *data, uint32_t length)
{
    uint32_t i;
    uint32_t sum = 0U;

    for (i = 0U; i < length; i++)
    {
        sum = (sum << 5U) - sum + data[i];
    }
    return ~sum;
}
#endif

static uint16_t DataTransferFrameChecksum(const uint8_t *frame)
{
    uint32_t i;
    uint32_t sum = 0U;

    for (i = 0U; i < (DATA_TRANSFER_FRAME_LEN - 2U); i += 2U)
    {
        sum += ((uint16_t)frame[i] << 8U) | frame[i + 1U];
    }
    return (uint16_t)sum;
}

static uint32_t DataTransferNowSeconds(void)
{
    uint32_t localSeconds;

#if defined(FPGA_PROTOCOL_HOST_TEST)
    localSeconds = g_testTimestamp;
#else
    localSeconds = (uint32_t)(TK8710GetTimeUs() / 1000000ULL);
#endif
    if (g_dataTransfer.utcValid != 0U)
    {
        return g_dataTransfer.utcBaseSeconds + (localSeconds - g_dataTransfer.utcSyncLocalSeconds);
    }
    return localSeconds;
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint32_t DataTransferAdvanceSector(uint32_t address)
{
    address += DATA_TRANSFER_FLASH_SECTOR_SIZE;
    if (address >= DATA_TRANSFER_FLASH_TOTAL_SIZE)
    {
        address = DATA_TRANSFER_FLASH_DATA_START;
    }
    return address;
}
#endif

static void DataTransferSetEmptyState(void)
{
    g_dataTransfer.head = DATA_TRANSFER_FLASH_DATA_START;
    g_dataTransfer.tail = DATA_TRANSFER_FLASH_DATA_START;
    g_dataTransfer.metadataSequence = 0U;
    g_dataTransfer.metadataNext = 0U;
}

static void DataTransferSetEmptyPending(void)
{
    g_dataTransfer.head = DATA_TRANSFER_FLASH_DATA_START;
    g_dataTransfer.tail = DATA_TRANSFER_FLASH_DATA_START;
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint8_t DataTransferAddressInDataRegion(uint32_t address)
{
    return ((address >= DATA_TRANSFER_FLASH_DATA_START) &&
            (address < DATA_TRANSFER_FLASH_TOTAL_SIZE) &&
            ((address % DATA_TRANSFER_FLASH_SECTOR_SIZE) == 0U))
               ? 1U
               : 0U;
}

static void DataTransferBuildMetadata(uint8_t *record, uint32_t sequence,
                                      uint32_t head, uint32_t tail)
{
    (void)memset(record, 0xFF, DATA_TRANSFER_METADATA_LEN);
    DataTransferWriteBe32(&record[0], DATA_TRANSFER_METADATA_MAGIC);
    DataTransferWriteBe16(&record[4], DATA_TRANSFER_METADATA_VERSION);
    DataTransferWriteBe16(&record[6], DATA_TRANSFER_METADATA_LEN);
    DataTransferWriteBe32(&record[8], sequence);
    DataTransferWriteBe32(&record[12], head);
    DataTransferWriteBe32(&record[16], tail);
    DataTransferWriteBe32(&record[20], DataTransferChecksum32(record, 20U));
}

static uint8_t DataTransferMetadataIsBlank(const uint8_t *record)
{
    uint32_t i;

    for (i = 0U; i < DATA_TRANSFER_METADATA_LEN; i++)
    {
        if (record[i] != 0xFFU)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t DataTransferMetadataIsValid(const uint8_t *record)
{
    uint32_t head;
    uint32_t tail;

    if ((DataTransferReadBe32(&record[0]) != DATA_TRANSFER_METADATA_MAGIC) ||
        (((uint16_t)record[4] << 8U | record[5]) != DATA_TRANSFER_METADATA_VERSION) ||
        (((uint16_t)record[6] << 8U | record[7]) != DATA_TRANSFER_METADATA_LEN) ||
        (DataTransferReadBe32(&record[20]) != DataTransferChecksum32(record, 20U)))
    {
        return 0U;
    }
    head = DataTransferReadBe32(&record[12]);
    tail = DataTransferReadBe32(&record[16]);
    return ((DataTransferAddressInDataRegion(head) != 0U) &&
            (DataTransferAddressInDataRegion(tail) != 0U))
               ? 1U
               : 0U;
}

static void DataTransferLoadMetadata(void)
{
    uint8_t record[DATA_TRANSFER_METADATA_LEN];
    uint32_t offset;
    uint8_t found = 0U;

    DataTransferSetEmptyState();
    for (offset = DATA_TRANSFER_FLASH_METADATA_START;
         (offset + DATA_TRANSFER_METADATA_LEN) <= DATA_TRANSFER_FLASH_SECTOR_SIZE;
         offset += DATA_TRANSFER_FLASH_PAGE_SIZE)
    {
        if (SpiFlash_Read(offset, record, sizeof(record)) != 0)
        {
            break;
        }
        if (DataTransferMetadataIsBlank(record) != 0U)
        {
            g_dataTransfer.metadataNext = offset;
            return;
        }
        if (DataTransferMetadataIsValid(record) != 0U)
        {
            g_dataTransfer.metadataSequence = DataTransferReadBe32(&record[8]);
            g_dataTransfer.head = DataTransferReadBe32(&record[12]);
            g_dataTransfer.tail = DataTransferReadBe32(&record[16]);
            g_dataTransfer.metadataNext = offset + DATA_TRANSFER_FLASH_PAGE_SIZE;
            found = 1U;
        }
    }
    if (found == 0U)
    {
        DataTransferSetEmptyState();
    }
}
#endif

static int DataTransferSaveMetadata(void)
{
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    uint8_t record[DATA_TRANSFER_METADATA_LEN];

    if ((g_dataTransfer.metadataNext + DATA_TRANSFER_METADATA_LEN) > DATA_TRANSFER_FLASH_SECTOR_SIZE)
    {
        if (SpiFlash_EraseSector(DATA_TRANSFER_FLASH_METADATA_START) != 0)
        {
            return -1;
        }
        g_dataTransfer.metadataNext = DATA_TRANSFER_FLASH_METADATA_START;
    }
    g_dataTransfer.metadataSequence++;
    DataTransferBuildMetadata(record, g_dataTransfer.metadataSequence,
                              g_dataTransfer.head, g_dataTransfer.tail);
    if (SpiFlash_PageProgram(g_dataTransfer.metadataNext, record, sizeof(record)) != 0)
    {
        return -1;
    }
    g_dataTransfer.metadataNext += DATA_TRANSFER_FLASH_PAGE_SIZE;
#endif
    return 0;
}

static void DataTransferEnsureInitialized(void)
{
    if ((g_dataTransfer.runtimeMagic != DATA_TRANSFER_RUNTIME_MAGIC) ||
        (g_dataTransfer.initialized == 0U))
    {
        DataTransfer_Init();
    }
}

static uint8_t DataTransferIsInitialized(void)
{
    return ((g_dataTransfer.runtimeMagic == DATA_TRANSFER_RUNTIME_MAGIC) && (g_dataTransfer.initialized != 0U)) ? 1U : 0U;
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static int DataTransferFlushSector(void)
{
    uint8_t verify[DATA_TRANSFER_FLASH_PAGE_SIZE];
    uint32_t offset;
    uint32_t chunk;
    uint32_t nextHead;
    uint32_t eraseAddress;

    if (g_dataTransfer.ramLength != DATA_TRANSFER_FLASH_SECTOR_SIZE)
    {
        return 0;
    }
    g_dataTransfer.flashLastError = 0U;
    g_dataTransfer.flashLastOffset = 0U;
    if (g_dataTransfer.flashAvailable == 0U)
    {
        g_dataTransfer.flashLastError = 1U;
        return -1;
    }
    eraseAddress = g_dataTransfer.head - (g_dataTransfer.head % DATA_TRANSFER_FLASH_ERASE_SIZE);
    if (((g_dataTransfer.head % DATA_TRANSFER_FLASH_ERASE_SIZE) == 0U) &&
        (SpiFlash_EraseSector(eraseAddress) != 0))
    {
        g_dataTransfer.flashLastError = 2U;
        return -1;
    }
    for (offset = 0U; offset < DATA_TRANSFER_FLASH_SECTOR_SIZE; offset += DATA_TRANSFER_FLASH_PAGE_SIZE)
    {
        chunk = DATA_TRANSFER_FLASH_SECTOR_SIZE - offset;
        if (chunk > DATA_TRANSFER_FLASH_PAGE_SIZE)
        {
            chunk = DATA_TRANSFER_FLASH_PAGE_SIZE;
        }
        if (SpiFlash_PageProgram(g_dataTransfer.head + offset,
                                 &g_dataTransfer.ram[offset], chunk) != 0)
        {
            g_dataTransfer.flashLastError = 3U;
            g_dataTransfer.flashLastOffset = offset;
            return -1;
        }
        if (SpiFlash_Read(g_dataTransfer.head + offset, verify, chunk) != 0)
        {
            g_dataTransfer.flashLastError = 4U;
            g_dataTransfer.flashLastOffset = offset;
            return -1;
        }
        if (memcmp(verify, &g_dataTransfer.ram[offset], chunk) != 0)
        {
            g_dataTransfer.flashLastError = 5U;
            g_dataTransfer.flashLastOffset = offset;
            return -1;
        }
    }
    nextHead = DataTransferAdvanceSector(g_dataTransfer.head);
    if (nextHead == g_dataTransfer.tail)
    {
        g_dataTransfer.tail = DataTransferAdvanceSector(g_dataTransfer.tail);
    }
    g_dataTransfer.head = nextHead;
    g_dataTransfer.ramLength = 0U;
    if (DataTransferSaveMetadata() != 0)
    {
        g_dataTransfer.flashLastError = 6U;
        return -1;
    }
    return 0;
}
#endif

static int DataTransferAppendRaw(const uint8_t *data, uint32_t length)
{
    uint32_t chunk;
    uint32_t copied = 0U;

    while (copied < length)
    {
        chunk = DATA_TRANSFER_FLASH_SECTOR_SIZE - g_dataTransfer.ramLength;
        if (chunk > (length - copied))
        {
            chunk = length - copied;
        }
        (void)memcpy(&g_dataTransfer.ram[g_dataTransfer.ramLength], &data[copied], chunk);
        g_dataTransfer.ramLength += chunk;
        copied += chunk;
#if DATA_TRANSFER_SPI_FLASH_ENABLED
        if (g_dataTransfer.ramLength == DATA_TRANSFER_FLASH_SECTOR_SIZE)
        {
            if (DataTransferFlushSector() != 0)
            {
                return -1;
            }
        }
#endif
    }
    return 0;
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint32_t DataTransferFlashBytesAvailable(void)
{
    if (g_dataTransfer.txFlashRead == g_dataTransfer.txFlashEnd)
    {
        return 0U;
    }
    if (g_dataTransfer.txFlashRead < g_dataTransfer.txFlashEnd)
    {
        return g_dataTransfer.txFlashEnd - g_dataTransfer.txFlashRead;
    }
    return DATA_TRANSFER_FLASH_TOTAL_SIZE - g_dataTransfer.txFlashRead;
}
#endif

static uint32_t DataTransferPersistedLength(void)
{
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    if (g_dataTransfer.head == g_dataTransfer.tail)
    {
        return 0U;
    }
    if (g_dataTransfer.head > g_dataTransfer.tail)
    {
        return g_dataTransfer.head - g_dataTransfer.tail;
    }
    return (DATA_TRANSFER_FLASH_TOTAL_SIZE - g_dataTransfer.tail) +
           (g_dataTransfer.head - DATA_TRANSFER_FLASH_DATA_START);
#else
    return 0U;
#endif
}

#if DATA_TRANSFER_SPI_FLASH_ENABLED
static uint32_t DataTransferAddressFromPendingOffset(uint32_t offset)
{
    uint32_t address = g_dataTransfer.tail + offset;

    if (address >= DATA_TRANSFER_FLASH_TOTAL_SIZE)
    {
        address = DATA_TRANSFER_FLASH_DATA_START + (address - DATA_TRANSFER_FLASH_TOTAL_SIZE);
    }
    return address;
}
#endif

static int DataTransferReadTransmitBytes(uint8_t *dest, uint32_t capacity)
{
    uint32_t copied = 0U;
    uint32_t chunk;

#if DATA_TRANSFER_SPI_FLASH_ENABLED
    while (copied < capacity)
    {
        chunk = DataTransferFlashBytesAvailable();
        if (chunk == 0U)
        {
            break;
        }
        if (chunk > (capacity - copied))
        {
            chunk = capacity - copied;
        }
        if (SpiFlash_Read(g_dataTransfer.txFlashRead, &dest[copied], chunk) != 0)
        {
            return -1;
        }
        g_dataTransfer.txFlashRead += chunk;
        if (g_dataTransfer.txFlashRead >= DATA_TRANSFER_FLASH_TOTAL_SIZE)
        {
            g_dataTransfer.txFlashRead = DATA_TRANSFER_FLASH_DATA_START;
        }
        copied += chunk;
    }
#endif

    if ((copied < capacity) && (g_dataTransfer.txRamRead < g_dataTransfer.txRamLength))
    {
        chunk = g_dataTransfer.txRamLength - g_dataTransfer.txRamRead;
        if (chunk > (capacity - copied))
        {
            chunk = capacity - copied;
        }
        (void)memcpy(&dest[copied], &g_dataTransfer.txRam[g_dataTransfer.txRamRead], chunk);
        g_dataTransfer.txRamRead += chunk;
        copied += chunk;
    }
    return (int)copied;
}

static int DataTransferCommitTransmitCleanup(void)
{
    uint32_t oldTail;
    uint32_t remaining;

    if (g_dataTransfer.txHadError != 0U)
    {
        return 0;
    }

    if (g_dataTransfer.txStartTail != g_dataTransfer.txStartHead)
    {
        oldTail = g_dataTransfer.tail;
        g_dataTransfer.tail = g_dataTransfer.txStartHead;
        if (DataTransferSaveMetadata() != 0)
        {
            g_dataTransfer.tail = oldTail;
            return -1;
        }
    }

    if (g_dataTransfer.txStartRamLength != 0U)
    {
        if (g_dataTransfer.ramLength > g_dataTransfer.txStartRamLength)
        {
            remaining = g_dataTransfer.ramLength - g_dataTransfer.txStartRamLength;
            (void)memmove(g_dataTransfer.ram,
                          &g_dataTransfer.ram[g_dataTransfer.txStartRamLength],
                          remaining);
            g_dataTransfer.ramLength = remaining;
        }
        else
        {
            g_dataTransfer.ramLength = 0U;
        }
    }
    return 0;
}

static int DataTransferBuildNextFrame(uint8_t *frame)
{
    int length;
    uint16_t checksum;

    if ((frame == 0) || (g_dataTransfer.transmitActive == 0U))
    {
        g_dataTransfer.lastBuildResult = 0;
        return 0;
    }

    (void)memset(frame, 0x5A, DATA_TRANSFER_FRAME_LEN);
    length = DataTransferReadTransmitBytes(&frame[8], DATA_TRANSFER_FRAME_DATA_LEN);
    if (length < 0)
    {
        g_dataTransfer.lastBuildResult = -1;
        g_dataTransfer.lastFrameLength = 0U;
        g_dataTransfer.transmitActive = 0U;
        return -1;
    }
    if (length == 0)
    {
        g_dataTransfer.lastBuildResult = 0;
        g_dataTransfer.lastFrameLength = 0U;
        if (DataTransferCommitTransmitCleanup() != 0)
        {
            g_dataTransfer.lastBuildResult = -1;
            g_dataTransfer.transmitActive = 0U;
            return -1;
        }
        g_dataTransfer.transmitActive = 0U;
        return 0;
    }

    DataTransferWriteBe32(&frame[0], DATA_TRANSFER_FRAME_SYNC);
    DataTransferWriteBe16(&frame[4], (uint16_t)length);
    g_dataTransfer.lastPacketSeq = g_dataTransfer.txPacketSeq;
    DataTransferWriteBe16(&frame[6], g_dataTransfer.lastPacketSeq);
    g_dataTransfer.txPacketSeq++;
    checksum = DataTransferFrameChecksum(frame);
    DataTransferWriteBe16(&frame[DATA_TRANSFER_FRAME_LEN - 2U], checksum);
    g_dataTransfer.txFramesBuilt++;
    g_dataTransfer.lastBuildResult = 1;
    g_dataTransfer.lastFrameLength = (uint16_t)length;
    return 1;
}

static void DataTransferHandleSendResult(int sendResult)
{
    g_dataTransfer.lastSendResult = (int32_t)sendResult;
    if (sendResult == 0)
    {
        g_dataTransfer.txFramesSent++;
    }
    else
    {
        g_dataTransfer.txSendErrors++;
        g_dataTransfer.txHadError = 1U;
        g_dataTransfer.transmitActive = 0U;
    }
}

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
static uint16_t DataTransferSpi2DmaRemaining(uint32_t channel)
{
    uint32_t remaining;

    /* WCP is loaded by hardware only after the first request is accepted. */
    if ((dmaRAMREG->WCP[channel].CSADDR == 0U) &&
        (dmaRAMREG->WCP[channel].CDADDR == 0U))
    {
        return DATA_TRANSFER_FRAME_LEN;
    }
    remaining = dmaRAMREG->WCP[channel].CTCOUNT;
    return (uint16_t)((remaining >> 16U) & DATA_TRANSFER_SPI2_DMA_COUNT_MASK);
}

static void DataTransferStopSpi2SlaveDma(void)
{
    spiREG2->INT0 &= ~DATA_TRANSFER_SPI2_DMA_REQUEST_ENABLE;
    dmaREG->HWCHENAR = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
}

static void DataTransferConfigSpi2DmaPacket(uint32_t channel,
                                            uint32_t request,
                                            uint32_t source,
                                            uint32_t destination,
                                            uint32_t portAssignment,
                                            uint32_t sourceMode,
                                            uint32_t destinationMode)
{
    g_dmaCTRL packet;

    (void)memset(&packet, 0, sizeof(packet));
    packet.SADD = source;
    packet.DADD = destination;
    packet.FRCNT = DATA_TRANSFER_FRAME_LEN;
    packet.ELCNT = 1U;
    packet.PORTASGN = portAssignment;
    packet.RDSIZE = ACCESS_32_BIT;
    packet.WRSIZE = ACCESS_32_BIT;
    packet.TTYPE = FRAME_TRANSFER;
    packet.ADDMODERD = sourceMode;
    packet.ADDMODEWR = destinationMode;
    packet.AUTOINIT = AUTOINIT_OFF;

    dmaReqAssign(channel, request);
    dmaSetCtrlPacket(channel, packet);
    dmaSetPriority(channel, HIGHPRIORITY);
}

static void DataTransferConfigureSpi2SlaveDma(void)
{
    DataTransferConfigSpi2DmaPacket(
        DATA_TRANSFER_SPI2_DMA_RX_CHANNEL,
        DATA_TRANSFER_SPI2_DMA_RX_REQUEST,
        (uint32_t)&spiREG2->BUF,
        (uint32_t)g_dataTransfer.slaveRxWords,
        DATA_TRANSFER_SPI2_DMA_PORT_B_ASSIGNMENT,
        ADDR_FIXED,
        ADDR_INC1);
    DataTransferConfigSpi2DmaPacket(
        DATA_TRANSFER_SPI2_DMA_TX_CHANNEL,
        DATA_TRANSFER_SPI2_DMA_TX_REQUEST,
        (uint32_t)g_dataTransfer.slaveTxWords,
        (uint32_t)&spiREG2->DAT1,
        DATA_TRANSFER_SPI2_DMA_PORT_B_ASSIGNMENT,
        ADDR_INC1,
        ADDR_FIXED);

    /* The SPI registers are single-word endpoints; bypass the four-deep DMA FIFO. */
    dmaREG->PTCRL |= DATA_TRANSFER_SPI2_DMA_PORT_B_BYPASS;
}

static void DataTransferConfigSpi2SlaveTest(void)
{
    DataTransferStopSpi2SlaveDma();
    /*
     * Startup initializes DMA RAM but does not enable the DMA controller.
     * Without this call WCP remains at reset value zero and looks like a
     * completed transfer to polling code even though no SPI clocks occurred.
     */
    dmaEnable();
    spiREG2->GCR1 &= 0xFEFFFFFFU;
    spiREG2->GCR0 = 0U;
    spiREG2->GCR0 = 1U;
    spiREG2->GCR1 = (spiREG2->GCR1 & 0xFFFFFFFCU); /* slave, external clock */
    spiREG2->INT0 = 0U;
    spiREG2->DELAY = 0U;
    spiREG2->FMT0 = (spiREG2->FMT0 & 0xFFFFFFE0U) | 8U;
    spiREG2->FMT1 = spiREG2->FMT0;
    spiREG2->FMT2 = spiREG2->FMT0;
    spiREG2->FMT3 = spiREG2->FMT0;
    spiREG2->DEF = SPI_CS_NONE;
    spiREG2->LVL = 0U;
    spiREG2->FLG |= 0xFFFFU;
    spiREG2->PC3 = (uint32_t)((uint32_t)1U << 0U) |
                  (uint32_t)((uint32_t)1U << 1U) |
                  (uint32_t)((uint32_t)0U << 8U) |
                  (uint32_t)((uint32_t)0U << 9U) |
                  (uint32_t)((uint32_t)0U << 10U) |
                  (uint32_t)((uint32_t)1U << 11U);
    spiREG2->PC1 = (uint32_t)((uint32_t)0U << 0U) |
                  (uint32_t)((uint32_t)0U << 1U) |
                  (uint32_t)((uint32_t)0U << 8U) |
                  (uint32_t)((uint32_t)0U << 9U) |
                  (uint32_t)((uint32_t)0U << 10U) |
                  (uint32_t)((uint32_t)1U << 11U);
    spiREG2->PC6 = 0U;
    spiREG2->PC8 = (uint32_t)((uint32_t)1U << 0U) |
                  (uint32_t)((uint32_t)1U << 1U) |
                  (uint32_t)((uint32_t)1U << 8U) |
                  (uint32_t)((uint32_t)1U << 9U) |
                  (uint32_t)((uint32_t)1U << 10U) |
                  (uint32_t)((uint32_t)1U << 11U);
    spiREG2->PC7 = 0U;
    spiREG2->PC0 = (uint32_t)((uint32_t)1U << 0U) |
                  (uint32_t)((uint32_t)1U << 1U) |
                  (uint32_t)((uint32_t)1U << 8U) |
                  (uint32_t)((uint32_t)1U << 9U) |
                  (uint32_t)((uint32_t)1U << 10U) |
                  (uint32_t)((uint32_t)1U << 11U);
    spiREG2->GCR1 = (spiREG2->GCR1 & 0xFEFFFFFFU) | 0x01000000U;
    DataTransferConfigureSpi2SlaveDma();
    g_dataTransfer.slaveLastPc2 = spiREG2->PC2;
    g_dataTransfer.slaveLastFlg = spiREG2->FLG;
    g_dataTransfer.slaveLastPc0 = spiREG2->PC0;
    g_dataTransfer.slaveLastPc1 = spiREG2->PC1;
    g_dataTransfer.slaveLastGcr1 = spiREG2->GCR1;
}

static void DataTransferPrimeSpi2SlaveByte(uint8_t value)
{
    spiREG2->DAT1 = ((uint32_t)SPI_FMT_0 << 24U) |
                   ((uint32_t)SPI_CS_0 << 16U) |
                   (uint32_t)value;
}

static void DataTransferPrimeSpi2SlaveIdle(void)
{
    DataTransferPrimeSpi2SlaveByte(DATA_TRANSFER_SPI2_IDLE_BYTE);
}

static void DataTransferArmSpi2SlaveDma(void)
{
    uint32_t discardedRx;
    uint32_t i;

    DataTransferStopSpi2SlaveDma();
    discardedRx = spiREG2->BUF;
    (void)discardedRx;
    for (i = 0U; i < DATA_TRANSFER_FRAME_LEN; i++)
    {
        g_dataTransfer.slaveTxWords[i] =
            ((uint32_t)SPI_FMT_0 << 24U) |
            ((uint32_t)SPI_CS_0 << 16U) |
            (uint32_t)g_dataTransfer.slaveFrame[i];
        g_dataTransfer.slaveRxWords[i] = 0U;
    }

    DataTransferConfigureSpi2SlaveDma();
    dmaREG->FTCFLAG = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
    dmaREG->LFSFLAG = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
    dmaREG->HBCFLAG = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
    dmaREG->BTCFLAG = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
    g_dataTransfer.slaveDmaBtcFlag = 0U;
    spiREG2->FLG |= 0xFFFFU;
    g_dataTransfer.slaveOffset = 0U;
    g_dataTransfer.slaveDmaRxRemaining = DATA_TRANSFER_FRAME_LEN;
    g_dataTransfer.slaveDmaTxRemaining = DATA_TRANSFER_FRAME_LEN;
    g_dataTransfer.slaveLastTransferred = 0U;
    g_dataTransfer.slaveDmaStarted = 0U;
    g_dataTransfer.slaveFrameReady = 1U;

    /* Enabling SPI DMA creates the first TX request; both DMA channels must be ready. */
    dmaSetChEnable(DATA_TRANSFER_SPI2_DMA_RX_CHANNEL, DMA_HW);
    dmaSetChEnable(DATA_TRANSFER_SPI2_DMA_TX_CHANNEL, DMA_HW);
    spiREG2->INT0 |= DATA_TRANSFER_SPI2_DMA_REQUEST_ENABLE;
}

static void DataTransferPrepareSpi2SlaveFrame(void)
{
    int result;

    result = DataTransferBuildNextFrame(g_dataTransfer.slaveFrame);
    if (result == 1)
    {
        DataTransferArmSpi2SlaveDma();
    }
    else
    {
        DataTransferStopSpi2SlaveDma();
        g_dataTransfer.slaveFrameReady = 0U;
        g_dataTransfer.slaveOffset = 0U;
        g_dataTransfer.slaveDmaRxRemaining = 0U;
        g_dataTransfer.slaveDmaTxRemaining = 0U;
        DataTransferPrimeSpi2SlaveIdle();
    }
}

static void DataTransferCaptureSpi2SlaveRxDiagnostics(void)
{
    uint32_t checksum = 0U;
    uint32_t i;

    for (i = 0U; i < DATA_TRANSFER_FRAME_LEN; i++)
    {
        checksum += (uint8_t)g_dataTransfer.slaveRxWords[i];
    }
    g_dataTransfer.slaveLastRxFirst = (uint8_t)g_dataTransfer.slaveRxWords[0];
    g_dataTransfer.slaveLastRxLast =
        (uint8_t)g_dataTransfer.slaveRxWords[DATA_TRANSFER_FRAME_LEN - 1U];
    g_dataTransfer.slaveLastRxChecksum = checksum & 0xFFFFU;
}

static void DataTransferProcessSpi2SlaveTest(void)
{
    uint16_t transferred;
    uint16_t rxRemaining;
    uint16_t txRemaining;
    uint32_t ftcFlag;
    uint32_t pc2;

    g_dataTransfer.slaveProcessCount++;
    pc2 = spiREG2->PC2;
    if (pc2 != g_dataTransfer.slaveLastPc2)
    {
        g_dataTransfer.slavePc2ChangeCount++;
        g_dataTransfer.slaveLastPc2 = pc2;
    }
    g_dataTransfer.slaveLastFlg = spiREG2->FLG;
    g_dataTransfer.slaveLastPc0 = spiREG2->PC0;
    g_dataTransfer.slaveLastPc1 = spiREG2->PC1;
    g_dataTransfer.slaveLastGcr1 = spiREG2->GCR1;
    g_dataTransfer.slaveDmaStatus = dmaREG->DMASTAT;
    g_dataTransfer.slaveDmaPending = dmaREG->PEND;
    g_dataTransfer.slaveDmaHwEnable = dmaREG->HWCHENAS;
    if (g_dataTransfer.slaveFrameReady != 0U)
    {
        /* Retain the last active-frame flags for post-transfer diagnostics. */
        g_dataTransfer.slaveDmaBtcFlag = dmaREG->BTCFLAG;
    }

    if ((spiREG2->FLG & 0x000000FFU) != 0U)
    {
        DataTransferStopSpi2SlaveDma();
        spiREG2->FLG |= 0xFFFFU;
        g_dataTransfer.slaveFrameReady = 0U;
        g_dataTransfer.slaveOffset = 0U;
        g_dataTransfer.slaveDmaRxRemaining = 0U;
        g_dataTransfer.slaveDmaTxRemaining = 0U;
        DataTransferHandleSendResult(-1);
        DataTransferPrimeSpi2SlaveIdle();
        return;
    }

    if (g_dataTransfer.slaveFrameReady != 0U)
    {
        ftcFlag = dmaREG->FTCFLAG;
        if ((ftcFlag &
             ((uint32_t)1U << DATA_TRANSFER_SPI2_DMA_RX_CHANNEL)) != 0U)
        {
            g_dataTransfer.slaveDmaStarted = 1U;
        }
        rxRemaining = (g_dataTransfer.slaveDmaStarted != 0U)
                          ? DataTransferSpi2DmaRemaining(
                                DATA_TRANSFER_SPI2_DMA_RX_CHANNEL)
                          : DATA_TRANSFER_FRAME_LEN;
        txRemaining = ((ftcFlag &
                        ((uint32_t)1U <<
                         DATA_TRANSFER_SPI2_DMA_TX_CHANNEL)) != 0U)
                          ? DataTransferSpi2DmaRemaining(
                                DATA_TRANSFER_SPI2_DMA_TX_CHANNEL)
                          : DATA_TRANSFER_FRAME_LEN;
        g_dataTransfer.slaveDmaRxRemaining = rxRemaining;
        g_dataTransfer.slaveDmaTxRemaining = txRemaining;
        transferred = (uint16_t)(DATA_TRANSFER_FRAME_LEN - rxRemaining);
        g_dataTransfer.slaveOffset = transferred;
        if (transferred > g_dataTransfer.slaveLastTransferred)
        {
            g_dataTransfer.slaveRxReadyCount +=
                (uint32_t)(transferred - g_dataTransfer.slaveLastTransferred);
            g_dataTransfer.slaveLastTransferred = transferred;
        }
        if ((g_dataTransfer.slaveDmaStarted != 0U) &&
            ((g_dataTransfer.slaveDmaBtcFlag &
              DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK) ==
             DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK))
        {
            /*
             * BTC is authoritative.  On LS3137 the polled WCP count can still
             * expose the final in-flight count after both channels have
             * completed, so do not require CTCOUNT to reach zero here.
             */
            if (g_dataTransfer.slaveLastTransferred < DATA_TRANSFER_FRAME_LEN)
            {
                g_dataTransfer.slaveRxReadyCount +=
                    DATA_TRANSFER_FRAME_LEN -
                    g_dataTransfer.slaveLastTransferred;
                g_dataTransfer.slaveLastTransferred = DATA_TRANSFER_FRAME_LEN;
            }
            DataTransferStopSpi2SlaveDma();
            dmaREG->BTCFLAG = DATA_TRANSFER_SPI2_DMA_CHANNEL_MASK;
            DataTransferCaptureSpi2SlaveRxDiagnostics();
            DataTransferHandleSendResult(0);
            g_dataTransfer.slaveFrameReady = 0U;
            g_dataTransfer.slaveOffset = 0U;
            g_dataTransfer.slaveDmaRxRemaining = 0U;
            g_dataTransfer.slaveDmaTxRemaining = 0U;
            g_dataTransfer.slaveDmaStarted = 0U;
            DataTransferPrepareSpi2SlaveFrame();
        }
    }
    else if (g_dataTransfer.transmitActive != 0U)
    {
        DataTransferPrepareSpi2SlaveFrame();
    }
}
#else
static int DataTransferWaitSpi2RxReady(spiBASE_t *spi)
{
    uint32_t guard = DATA_TRANSFER_SPI2_WAIT_LIMIT;

    while ((spi->FLG & 0x00000100U) != 0x00000100U)
    {
        if ((spi->FLG & 0x000000FFU) != 0U)
        {
            return -1;
        }
        if (guard == 0U)
        {
            return -2;
        }
        guard--;
    }
    return 0;
}

static int DataTransferSpi2TransmitWords(const spiDAT1_t *config,
                                         uint32_t blocksize,
                                         const uint16_t *src)
{
    volatile uint32_t spiBuf;
    uint32_t chipSelectHold = (config->CS_HOLD != 0U) ? 0x10000000U : 0U;
    uint32_t wDelay = (config->WDEL != 0U) ? 0x04000000U : 0U;
    uint32_t dataFormat = (uint32_t)config->DFSEL;
    uint32_t chipSelect = (uint32_t)config->CSNR;

    while (blocksize != 0U)
    {
        if ((spiREG2->FLG & 0x000000FFU) != 0U)
        {
            return -1;
        }
        if (blocksize == 1U)
        {
            chipSelectHold = 0U;
        }
        spiREG2->DAT1 = (dataFormat << 24U) |
                       (chipSelect << 16U) |
                       wDelay |
                       chipSelectHold |
                       (uint32_t)(*src);
        src++;
        if (DataTransferWaitSpi2RxReady(spiREG2) != 0)
        {
            return -2;
        }
        spiBuf = spiREG2->BUF;
        (void)spiBuf;
        blocksize--;
    }
    return ((spiREG2->FLG & 0x000000FFU) == 0U) ? 0 : -1;
}
static int DataTransferSendFrame(const uint8_t *frame)
{
    uint16_t words[DATA_TRANSFER_FRAME_LEN];
    spiDAT1_t config;
    uint32_t i;

    for (i = 0U; i < DATA_TRANSFER_FRAME_LEN; i++)
    {
        words[i] = frame[i];
    }
    config.CS_HOLD = 1U;
    config.WDEL = 0U;
    config.DFSEL = SPI_FMT_0;
    config.CSNR = SPI_CS_0;
    return DataTransferSpi2TransmitWords(&config, DATA_TRANSFER_FRAME_LEN, words);
}
#endif
#endif

void DataTransfer_Init(void)
{
    g_dataTransfer.runtimeMagic = 0U;
    g_dataTransfer.initialized = 0U;
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    if (SpiFlash_Init() != 0)
    {
        DataTransferSetEmptyState();
        g_dataTransfer.flashAvailable = 0U;
    }
    else
    {
        DataTransferLoadMetadata();
        g_dataTransfer.flashAvailable = 1U;
    }
#else
    DataTransferSetEmptyState();
#endif
    g_dataTransfer.ramLength = 0U;
    g_dataTransfer.utcBaseSeconds = 0U;
    g_dataTransfer.utcSyncLocalSeconds = 0U;
    g_dataTransfer.txFlashRead = 0U;
    g_dataTransfer.txFlashEnd = 0U;
    g_dataTransfer.txRamLength = 0U;
    g_dataTransfer.txRamRead = 0U;
    g_dataTransfer.txStartTail = 0U;
    g_dataTransfer.txStartHead = 0U;
    g_dataTransfer.txStartRamLength = 0U;
    g_dataTransfer.txStartCount = 0U;
    g_dataTransfer.txFramesBuilt = 0U;
    g_dataTransfer.txFramesSent = 0U;
    g_dataTransfer.txSendErrors = 0U;
    g_dataTransfer.lastBuildResult = 0;
    g_dataTransfer.lastSendResult = 0;
    g_dataTransfer.txPacketSeq = 0U;
    g_dataTransfer.lastFrameLength = 0U;
    g_dataTransfer.lastPacketSeq = 0U;
    g_dataTransfer.transmitActive = 0U;
    g_dataTransfer.txHadError = 0U;
    g_dataTransfer.utcValid = 0U;
    g_dataTransfer.initialized = 1U;
    g_dataTransfer.runtimeMagic = DATA_TRANSFER_RUNTIME_MAGIC;
#if !defined(FPGA_PROTOCOL_HOST_TEST)
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
    g_dataTransfer.slaveProcessCount = 0U;
    g_dataTransfer.slaveRxReadyCount = 0U;
    g_dataTransfer.slavePc2ChangeCount = 0U;
    g_dataTransfer.slaveLastPc2 = 0U;
    g_dataTransfer.slaveLastFlg = 0U;
    g_dataTransfer.slaveLastPc0 = 0U;
    g_dataTransfer.slaveLastPc1 = 0U;
    g_dataTransfer.slaveLastGcr1 = 0U;
    g_dataTransfer.slaveDmaStatus = 0U;
    g_dataTransfer.slaveDmaPending = 0U;
    g_dataTransfer.slaveDmaHwEnable = 0U;
    g_dataTransfer.slaveDmaBtcFlag = 0U;
    g_dataTransfer.slaveLastRxChecksum = 0U;
    g_dataTransfer.slaveDmaRxRemaining = 0U;
    g_dataTransfer.slaveDmaTxRemaining = 0U;
    g_dataTransfer.slaveLastTransferred = 0U;
    g_dataTransfer.slaveLastRxFirst = 0U;
    g_dataTransfer.slaveLastRxLast = 0U;
    g_dataTransfer.slaveDmaStarted = 0U;
    dmaEnable();
    DataTransferConfigSpi2SlaveTest();
    DataTransferPrimeSpi2SlaveIdle();
    g_dataTransfer.slaveFrameReady = 0U;
    g_dataTransfer.slaveOffset = 0U;
#else
    spiREG2->FMT0 = (spiREG2->FMT0 & 0xFFFFFFE0U) | 8U;
#endif
#endif
}

void DataTransfer_Process(void)
{
#if !defined(FPGA_PROTOCOL_HOST_TEST)
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
    DataTransferProcessSpi2SlaveTest();
#else
    uint8_t frame[DATA_TRANSFER_FRAME_LEN];
    int buildResult;

    buildResult = DataTransferBuildNextFrame(frame);
    if (buildResult == 1)
    {
        DataTransferHandleSendResult(DataTransferSendFrame(frame));
    }
#endif
#endif
}

static int DataTransferAppendRecord(uint8_t format, uint8_t type,
                                    const uint8_t *data, uint16_t length)
{
    uint8_t header[DATA_TRANSFER_RECORD_HEADER_LEN];
    uint32_t recordLength = (uint32_t)DATA_TRANSFER_RECORD_HEADER_LEN +
                            (uint32_t)length;

    if ((data == 0) && (length != 0U))
    {
        return -1;
    }
    DataTransferEnsureInitialized();
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    if ((g_dataTransfer.transmitActive != 0U) &&
        (g_dataTransfer.ramLength >= g_dataTransfer.txStartRamLength) &&
        ((DATA_TRANSFER_FLASH_SECTOR_SIZE - g_dataTransfer.ramLength) < recordLength))
    {
        return -1;
    }
#else
    if (recordLength >
        (DATA_TRANSFER_FLASH_SECTOR_SIZE - g_dataTransfer.ramLength))
    {
        return -1;
    }
#endif
    DataTransferWriteBe32(&header[0], DataTransferNowSeconds());
    header[4] = format;
    header[5] = type;
    DataTransferWriteBe16(&header[6], length);
    if (DataTransferAppendRaw(header, sizeof(header)) != 0)
    {
        return -1;
    }
    if (length != 0U)
    {
        return DataTransferAppendRaw(data, length);
    }
    return 0;
}

static int DataTransferAppendStructuredHeader(uint8_t type, uint16_t length)
{
    uint8_t header[DATA_TRANSFER_RECORD_HEADER_LEN];
    uint32_t recordLength = (uint32_t)DATA_TRANSFER_RECORD_HEADER_LEN +
                            (uint32_t)length;

    DataTransferEnsureInitialized();
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    if ((g_dataTransfer.transmitActive != 0U) &&
        (g_dataTransfer.ramLength >= g_dataTransfer.txStartRamLength) &&
        ((DATA_TRANSFER_FLASH_SECTOR_SIZE - g_dataTransfer.ramLength) <
         recordLength))
    {
        return -1;
    }
#else
    if (recordLength >
        (DATA_TRANSFER_FLASH_SECTOR_SIZE - g_dataTransfer.ramLength))
    {
        return -1;
    }
#endif
    DataTransferWriteBe32(&header[0], DataTransferNowSeconds());
    header[4] = DATA_TRANSFER_RECORD_FORMAT_BINARY;
    header[5] = type;
    DataTransferWriteBe16(&header[6], length);
    return DataTransferAppendRaw(header, sizeof(header));
}

int DataTransfer_AppendBytes(uint8_t type, const uint8_t *data, uint16_t length)
{
    return DataTransferAppendRecord(DATA_TRANSFER_RECORD_FORMAT_BINARY, type, data, length);
}

int DataTransfer_AppendAlphabetPattern64K(uint8_t type)
{
    uint8_t chunk[DATA_TRANSFER_ALPHABET_PATTERN_CHUNK];
    uint32_t offset;
    uint32_t i;

    for (offset = 0U; offset < DATA_TRANSFER_ALPHABET_PATTERN_TOTAL;
         offset += DATA_TRANSFER_ALPHABET_PATTERN_CHUNK)
    {
        for (i = 0U; i < DATA_TRANSFER_ALPHABET_PATTERN_CHUNK; i++)
        {
            chunk[i] = (uint8_t)('A' + ((offset + i) % 26U));
        }
        if (DataTransfer_AppendBytes(type, chunk,
                                     DATA_TRANSFER_ALPHABET_PATTERN_CHUNK) != 0)
        {
            return -1;
        }
    }
    return 0;
}

int DataTransfer_ClearPending(void)
{
    DataTransferEnsureInitialized();
    g_dataTransfer.ramLength = 0U;
    g_dataTransfer.txFlashRead = 0U;
    g_dataTransfer.txFlashEnd = 0U;
    g_dataTransfer.txRamLength = 0U;
    g_dataTransfer.txRamRead = 0U;
    g_dataTransfer.transmitActive = 0U;
    g_dataTransfer.txHadError = 0U;
    DataTransferSetEmptyPending();
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    if (g_dataTransfer.flashAvailable != 0U)
    {
        return DataTransferSaveMetadata();
    }
#endif
    return 0;
}

int DataTransfer_AppendString(uint8_t type, const char *data, uint16_t length)
{
    return DataTransferAppendRecord(DATA_TRANSFER_RECORD_FORMAT_STRING, type,
                                    (const uint8_t *)data, length);
}

int DataTransfer_AppendRuntimeLog(const char *text, uint16_t length)
{
    if ((text == 0) || (length == 0U) ||
        (DataTransferIsInitialized() == 0U))
    {
        return -1;
    }
    return DataTransferAppendRecord(DATA_TRANSFER_RECORD_FORMAT_STRING,
                                    DATA_TRANSFER_TYPE_RUNTIME_LOG_STATUS,
                                    (const uint8_t *)text, length);
}

int DataTransfer_AppendFirstUserInfo(const DataTransferFirstUserInfo *info)
{
    uint8_t payload[DATA_TRANSFER_FIRST_USER_PAYLOAD_LEN];
    uint32_t i;

    if (info == 0)
    {
        return -1;
    }
    (void)memset(payload, 0, sizeof(payload));
    payload[0] = DATA_TRANSFER_PAYLOAD_VERSION;
    payload[1] = info->rateMode;
    DataTransferWriteBe16(&payload[2], (uint16_t)info->rssi);
    payload[4] = info->snr;
    DataTransferWriteBe16(&payload[6], info->dataLen);
    DataTransferWriteBe32(&payload[8], info->frameNo);
    DataTransferWriteBe32(&payload[12], info->userId);
    DataTransferWriteBe32(&payload[16], (uint32_t)info->freqOffset);
    DataTransferWriteBe32(&payload[20], info->frequencyHz);
    DataTransferWriteBe64(&payload[24], info->pilotPower);
    for (i = 0U; i < DATA_TRANSFER_AH_VALUE_COUNT; i++)
    {
        DataTransferWriteBe32(&payload[32U + (i * 4U)], info->ahData[i]);
    }
    return DataTransferAppendRecord(DATA_TRANSFER_RECORD_FORMAT_BINARY,
                                    DATA_TRANSFER_TYPE_FIRST_USER_INFO,
                                    payload, sizeof(payload));
}

int DataTransfer_AppendCaptureRawData(const DataTransferCaptureChunk *chunk)
{
    uint8_t header[DATA_TRANSFER_CAPTURE_HEADER_LEN];
    uint32_t payloadLength;

    if ((chunk == 0) || (chunk->antenna >= 8U) ||
        ((chunk->data == 0) && (chunk->length != 0U)) ||
        (chunk->offset > chunk->bytesPerAntenna) ||
        ((uint32_t)chunk->length >
         (chunk->bytesPerAntenna - chunk->offset)))
    {
        return -1;
    }
    payloadLength = DATA_TRANSFER_CAPTURE_HEADER_LEN + chunk->length;
    if (payloadLength > 0xFFFFU)
    {
        return -1;
    }
    (void)memset(header, 0, sizeof(header));
    header[0] = DATA_TRANSFER_PAYLOAD_VERSION;
    header[1] = chunk->rateMode;
    header[2] = chunk->antenna;
    header[3] = 1U;
    DataTransferWriteBe32(&header[4], chunk->generation);
    DataTransferWriteBe32(&header[8], chunk->bytesPerAntenna);
    DataTransferWriteBe32(&header[12], chunk->offset);
    DataTransferWriteBe16(&header[16], chunk->length);
    if (DataTransferAppendStructuredHeader(
            DATA_TRANSFER_TYPE_CAPTURE_RAW_DATA,
            (uint16_t)payloadLength) != 0)
    {
        return -1;
    }
    if (DataTransferAppendRaw(header, sizeof(header)) != 0)
    {
        return -1;
    }
    return (chunk->length == 0U) ? 0 : DataTransferAppendRaw(chunk->data, chunk->length);
}

int DataTransfer_AppendSweepBackgroundNoise(const DataTransferSweepChunk *chunk)
{
    uint8_t header[DATA_TRANSFER_SWEEP_HEADER_LEN];
    uint8_t point[DATA_TRANSFER_SWEEP_POINT_LEN];
    uint32_t payloadLength;
    uint32_t pointIndex;
    uint32_t antenna;

    if ((chunk == 0) ||
        ((chunk->points == 0) && (chunk->pointCount != 0U)) ||
        (chunk->startIndex > chunk->totalPoints) ||
        ((uint32_t)chunk->pointCount >
         (chunk->totalPoints - chunk->startIndex)))
    {
        return -1;
    }
    payloadLength = DATA_TRANSFER_SWEEP_HEADER_LEN +
                    ((uint32_t)chunk->pointCount *
                     DATA_TRANSFER_SWEEP_POINT_LEN);
    if (payloadLength > 0xFFFFU)
    {
        return -1;
    }
    (void)memset(header, 0, sizeof(header));
    header[0] = DATA_TRANSFER_PAYLOAD_VERSION;
    header[1] = chunk->sweepMode;
    header[2] = chunk->rateMode;
    header[3] = DATA_TRANSFER_SWEEP_ANTENNA_COUNT;
    DataTransferWriteBe32(&header[4], chunk->generation);
    DataTransferWriteBe32(&header[8], chunk->startFrequencyHz);
    DataTransferWriteBe32(&header[12], chunk->endFrequencyHz);
    DataTransferWriteBe32(&header[16], chunk->stepFrequencyHz);
    DataTransferWriteBe32(&header[20], chunk->totalPoints);
    DataTransferWriteBe32(&header[24], chunk->startIndex);
    DataTransferWriteBe16(&header[28], chunk->pointCount);
    if (DataTransferAppendStructuredHeader(
            DATA_TRANSFER_TYPE_SWEEP_BACKGROUND_NOISE,
            (uint16_t)payloadLength) != 0)
    {
        return -1;
    }
    if (DataTransferAppendRaw(header, sizeof(header)) != 0)
    {
        return -1;
    }
    for (pointIndex = 0U; pointIndex < chunk->pointCount; pointIndex++)
    {
        DataTransferWriteBe32(&point[0],
                              chunk->points[pointIndex].frequencyHz);
        for (antenna = 0U; antenna < DATA_TRANSFER_SWEEP_ANTENNA_COUNT;
             antenna++)
        {
            DataTransferWriteFloatBe32(
                &point[4U + (antenna * 4U)],
                chunk->points[pointIndex].noiseDbmHz[antenna]);
        }
        if (DataTransferAppendRaw(point, sizeof(point)) != 0)
        {
            return -1;
        }
    }
    return 0;
}

int DataTransfer_StartTransmit(void)
{
    DataTransferEnsureInitialized();
    g_dataTransfer.txFlashRead = g_dataTransfer.tail;
    g_dataTransfer.txFlashEnd = g_dataTransfer.head;
    g_dataTransfer.txRamLength = g_dataTransfer.ramLength;
    g_dataTransfer.txRamRead = 0U;
    g_dataTransfer.txStartTail = g_dataTransfer.tail;
    g_dataTransfer.txStartHead = g_dataTransfer.head;
    g_dataTransfer.txStartRamLength = g_dataTransfer.ramLength;
    g_dataTransfer.txPacketSeq = 0U;
    g_dataTransfer.txHadError = 0U;
    g_dataTransfer.txStartCount++;
    g_dataTransfer.lastBuildResult = 0;
    g_dataTransfer.lastSendResult = 0;
    g_dataTransfer.lastFrameLength = 0U;
    g_dataTransfer.lastPacketSeq = 0U;
    if (g_dataTransfer.txRamLength != 0U)
    {
        (void)memcpy(g_dataTransfer.txRam, g_dataTransfer.ram, g_dataTransfer.txRamLength);
    }
    g_dataTransfer.transmitActive = 1U;
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
    DataTransferConfigSpi2SlaveTest();
    g_dataTransfer.slaveFrameReady = 0U;
    g_dataTransfer.slaveOffset = 0U;
    DataTransferPrepareSpi2SlaveFrame();
#endif
    return 0;
}

void DataTransfer_SetUtcSeconds(uint32_t utcSeconds)
{
    DataTransferEnsureInitialized();
    g_dataTransfer.utcBaseSeconds = utcSeconds;
#if defined(FPGA_PROTOCOL_HOST_TEST)
    g_dataTransfer.utcSyncLocalSeconds = g_testTimestamp;
#else
    g_dataTransfer.utcSyncLocalSeconds = (uint32_t)(TK8710GetTimeUs() / 1000000ULL);
#endif
    g_dataTransfer.utcValid = 1U;
}

uint32_t DataTransfer_GetPendingLength(void)
{
    DataTransferEnsureInitialized();
    return DataTransferPersistedLength() + g_dataTransfer.ramLength;
}

uint32_t DataTransfer_ReadPending(uint32_t offset, uint8_t *data, uint32_t length)
{
    uint32_t total;
    uint32_t flashLen;
    uint32_t copied = 0U;
    uint32_t chunk;
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    uint32_t address;
#endif

    if ((data == 0) || (length == 0U))
    {
        return 0U;
    }
    DataTransferEnsureInitialized();
    total = DataTransfer_GetPendingLength();
    if (offset >= total)
    {
        return 0U;
    }
    if (length > (total - offset))
    {
        length = total - offset;
    }

    flashLen = 0U;
#if DATA_TRANSFER_SPI_FLASH_ENABLED
    flashLen = DataTransferPersistedLength();
    if (offset < flashLen)
    {
        chunk = flashLen - offset;
        if (chunk > length)
        {
            chunk = length;
        }
        address = DataTransferAddressFromPendingOffset(offset);
        if ((address + chunk) > DATA_TRANSFER_FLASH_TOTAL_SIZE)
        {
            chunk = DATA_TRANSFER_FLASH_TOTAL_SIZE - address;
        }
        if (SpiFlash_Read(address, data, chunk) != 0)
        {
            return 0U;
        }
        copied += chunk;
        if ((copied < length) && ((offset + copied) < flashLen))
        {
            chunk = flashLen - offset - copied;
            if (chunk > (length - copied))
            {
                chunk = length - copied;
            }
            address = DataTransferAddressFromPendingOffset(offset + copied);
            if (SpiFlash_Read(address, &data[copied], chunk) != 0)
            {
                return copied;
            }
            copied += chunk;
        }
    }
#endif

    if (copied < length)
    {
        uint32_t ramOffset = offset + copied - flashLen;
        chunk = length - copied;
        if (chunk > (g_dataTransfer.ramLength - ramOffset))
        {
            chunk = g_dataTransfer.ramLength - ramOffset;
        }
        (void)memcpy(&data[copied], &g_dataTransfer.ram[ramOffset], chunk);
        copied += chunk;
    }
    return copied;
}

void DataTransfer_GetSnapshot(DataTransferSnapshot *snapshot)
{
    if (snapshot == 0)
    {
        return;
    }
    snapshot->head = g_dataTransfer.head;
    snapshot->tail = g_dataTransfer.tail;
    snapshot->ramLength = g_dataTransfer.ramLength;
    snapshot->metadataSequence = g_dataTransfer.metadataSequence;
    snapshot->txStartCount = g_dataTransfer.txStartCount;
    snapshot->txFramesBuilt = g_dataTransfer.txFramesBuilt;
    snapshot->txFramesSent = g_dataTransfer.txFramesSent;
    snapshot->txSendErrors = g_dataTransfer.txSendErrors;
    snapshot->flashLastError = g_dataTransfer.flashLastError;
    snapshot->flashLastOffset = g_dataTransfer.flashLastOffset;
    snapshot->lastBuildResult = g_dataTransfer.lastBuildResult;
    snapshot->lastSendResult = g_dataTransfer.lastSendResult;
    snapshot->lastFrameLength = g_dataTransfer.lastFrameLength;
    snapshot->lastPacketSeq = g_dataTransfer.lastPacketSeq;
    snapshot->transmitActive = g_dataTransfer.transmitActive;
#if defined(DATA_TRANSFER_SPI2_SLAVE_TEST)
    snapshot->spi2SlaveProcessCount = g_dataTransfer.slaveProcessCount;
    snapshot->spi2SlaveRxReadyCount = g_dataTransfer.slaveRxReadyCount;
    snapshot->spi2SlavePc2ChangeCount = g_dataTransfer.slavePc2ChangeCount;
    snapshot->spi2SlaveFlg = g_dataTransfer.slaveLastFlg;
    snapshot->spi2SlavePc0 = g_dataTransfer.slaveLastPc0;
    snapshot->spi2SlavePc1 = g_dataTransfer.slaveLastPc1;
    snapshot->spi2SlavePc2 = g_dataTransfer.slaveLastPc2;
    snapshot->spi2SlaveGcr1 = g_dataTransfer.slaveLastGcr1;
    snapshot->spi2DmaStatus = g_dataTransfer.slaveDmaStatus;
    snapshot->spi2DmaPending = g_dataTransfer.slaveDmaPending;
    snapshot->spi2DmaHwEnable = g_dataTransfer.slaveDmaHwEnable;
    snapshot->spi2DmaBtcFlag = g_dataTransfer.slaveDmaBtcFlag;
    snapshot->spi2DmaLastRxChecksum = g_dataTransfer.slaveLastRxChecksum;
    snapshot->spi2DmaRxRemaining = g_dataTransfer.slaveDmaRxRemaining;
    snapshot->spi2DmaTxRemaining = g_dataTransfer.slaveDmaTxRemaining;
    snapshot->spi2DmaLastRxFirst = g_dataTransfer.slaveLastRxFirst;
    snapshot->spi2DmaLastRxLast = g_dataTransfer.slaveLastRxLast;
    snapshot->spi2DmaStarted = g_dataTransfer.slaveDmaStarted;
    snapshot->spi2SlaveOffset = g_dataTransfer.slaveOffset;
    snapshot->spi2SlaveReady = g_dataTransfer.slaveFrameReady;
#else
    snapshot->spi2SlaveProcessCount = 0U;
    snapshot->spi2SlaveRxReadyCount = 0U;
    snapshot->spi2SlavePc2ChangeCount = 0U;
    snapshot->spi2SlaveFlg = 0U;
    snapshot->spi2SlavePc0 = 0U;
    snapshot->spi2SlavePc1 = 0U;
    snapshot->spi2SlavePc2 = 0U;
    snapshot->spi2SlaveGcr1 = 0U;
    snapshot->spi2DmaStatus = 0U;
    snapshot->spi2DmaPending = 0U;
    snapshot->spi2DmaHwEnable = 0U;
    snapshot->spi2DmaBtcFlag = 0U;
    snapshot->spi2DmaLastRxChecksum = 0U;
    snapshot->spi2DmaRxRemaining = 0U;
    snapshot->spi2DmaTxRemaining = 0U;
    snapshot->spi2DmaLastRxFirst = 0U;
    snapshot->spi2DmaLastRxLast = 0U;
    snapshot->spi2DmaStarted = 0U;
    snapshot->spi2SlaveOffset = 0U;
    snapshot->spi2SlaveReady = 0U;
#endif
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
void DataTransfer_TestResetRuntimeOnly(void)
{
    uint32_t timestamp = g_testTimestamp;

    (void)memset(&g_dataTransfer, 0, sizeof(g_dataTransfer));
    g_testTimestamp = timestamp;
}

void DataTransfer_TestReset(void)
{
    (void)memset(&g_dataTransfer, 0, sizeof(g_dataTransfer));
    g_testTimestamp = 0U;
    SpiFlash_TestEraseAll();
}

void DataTransfer_TestSetTimestamp(uint32_t timestamp)
{
    g_testTimestamp = timestamp;
}

uint32_t DataTransfer_TestCopyRamBuffer(uint8_t *dest, uint32_t capacity)
{
    uint32_t copyLen = g_dataTransfer.ramLength;

    if ((dest != 0) && (capacity != 0U))
    {
        if (copyLen > capacity)
        {
            copyLen = capacity;
        }
        (void)memcpy(dest, g_dataTransfer.ram, copyLen);
    }
    return g_dataTransfer.ramLength;
}

void DataTransfer_TestReadFlash(uint32_t address, uint8_t *dest, uint32_t length)
{
    (void)SpiFlash_Read(address, dest, length);
}

int DataTransfer_TestBuildNextFrame(uint8_t *frame)
{
    return DataTransferBuildNextFrame(frame);
}

void DataTransfer_TestProcessOneFrameWithSendResult(int sendResult)
{
    uint8_t frame[DATA_TRANSFER_FRAME_LEN];

    if (DataTransferBuildNextFrame(frame) == 1)
    {
        DataTransferHandleSendResult(sendResult);
    }
}
#endif
