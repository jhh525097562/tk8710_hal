#ifndef DATA_TRANSFER_H
#define DATA_TRANSFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DATA_TRANSFER_SPI_FLASH_ENABLED
#if defined(FPGA_PROTOCOL_HOST_TEST) || defined(PLATFORM_TMS570)
#define DATA_TRANSFER_SPI_FLASH_ENABLED 1U
#else
#define DATA_TRANSFER_SPI_FLASH_ENABLED 0U
#endif
#endif

#ifndef DATA_TRANSFER_FLASH_SECTOR_SIZE
#define DATA_TRANSFER_FLASH_SECTOR_SIZE (64UL * 1024UL)
#endif

/*
 * The SPI flash is optional at run time.  Keep enough SDRAM-backed staging
 * space for one complete 8-antenna capture when the flash probe fails; mode 0
 * capture uses about 256 KiB before record headers are added.
 */
#ifndef DATA_TRANSFER_RAM_CAPACITY
#define DATA_TRANSFER_RAM_CAPACITY (512UL * 1024UL)
#endif

#ifndef DATA_TRANSFER_FLASH_ERASE_SIZE
#define DATA_TRANSFER_FLASH_ERASE_SIZE (256UL * 1024UL)
#endif

#ifndef DATA_TRANSFER_FLASH_PAGE_SIZE
#define DATA_TRANSFER_FLASH_PAGE_SIZE 256UL
#endif

#ifndef DATA_TRANSFER_FLASH_TOTAL_SIZE
#if defined(FPGA_PROTOCOL_HOST_TEST)
#define DATA_TRANSFER_FLASH_TOTAL_SIZE (4UL * DATA_TRANSFER_FLASH_ERASE_SIZE)
#else
#define DATA_TRANSFER_FLASH_TOTAL_SIZE (64UL * 1024UL * 1024UL)
#endif
#endif

#define DATA_TRANSFER_FLASH_METADATA_START 0UL
#define DATA_TRANSFER_FLASH_DATA_START DATA_TRANSFER_FLASH_ERASE_SIZE

#define DATA_TRANSFER_FRAME_SYNC 0x1ACFFC1DUL
#define DATA_TRANSFER_FRAME_LEN 512U
#define DATA_TRANSFER_FRAME_DATA_LEN 502U
#define DATA_TRANSFER_RECORD_FORMAT_STRING 0x01U
#define DATA_TRANSFER_RECORD_FORMAT_BINARY 0x02U
#define DATA_TRANSFER_AH_VALUE_COUNT 16U
#define DATA_TRANSFER_SWEEP_ANTENNA_COUNT 8U

    typedef enum
    {
        DATA_TRANSFER_TYPE_RUNTIME_LOG_STATUS = 0x01U,
        DATA_TRANSFER_TYPE_FIRST_USER_INFO = 0x02U,
        DATA_TRANSFER_TYPE_CAPTURE_RAW_DATA = 0x03U,
        DATA_TRANSFER_TYPE_SWEEP_BACKGROUND_NOISE = 0x04U
    } DataTransferRecordType;

    typedef struct
    {
        uint8_t rateMode;
        int16_t rssi;
        uint8_t snr;
        uint16_t dataLen;
        uint32_t frameNo;
        uint32_t userId;
        int32_t freqOffset;
        uint32_t frequencyHz;
        uint64_t pilotPower;
        uint32_t ahData[DATA_TRANSFER_AH_VALUE_COUNT];
    } DataTransferFirstUserInfo;

    typedef struct
    {
        uint8_t rateMode;
        uint8_t antenna;
        uint32_t generation;
        uint32_t bytesPerAntenna;
        uint32_t offset;
        const uint8_t *data;
        uint16_t length;
    } DataTransferCaptureChunk;

    typedef struct
    {
        uint32_t frequencyHz;
        float noiseDbmHz[DATA_TRANSFER_SWEEP_ANTENNA_COUNT];
    } DataTransferSweepPoint;

    typedef struct
    {
        uint8_t sweepMode;
        uint8_t rateMode;
        uint32_t generation;
        uint32_t startFrequencyHz;
        uint32_t endFrequencyHz;
        uint32_t stepFrequencyHz;
        uint32_t totalPoints;
        uint32_t startIndex;
        const DataTransferSweepPoint *points;
        uint16_t pointCount;
    } DataTransferSweepChunk;

    typedef struct
    {
        uint32_t head;
        uint32_t tail;
        uint32_t ramLength;
        uint32_t metadataSequence;
        uint32_t txStartCount;
        uint32_t txFramesBuilt;
        uint32_t txFramesSent;
        uint32_t txSendErrors;
        uint32_t flashLastError;
        uint32_t flashLastOffset;
        int32_t lastBuildResult;
        int32_t lastSendResult;
        uint16_t lastFrameLength;
        uint16_t lastPacketSeq;
        uint32_t spi2SlaveProcessCount;
        uint32_t spi2SlaveRxReadyCount;
        uint32_t spi2SlavePc2ChangeCount;
        uint32_t spi2SlaveFlg;
        uint32_t spi2SlavePc0;
        uint32_t spi2SlavePc1;
        uint32_t spi2SlavePc2;
        uint32_t spi2SlaveGcr1;
        uint32_t spi2DmaStatus;
        uint32_t spi2DmaPending;
        uint32_t spi2DmaHwEnable;
        uint32_t spi2DmaBtcFlag;
        uint32_t spi2DmaLastRxChecksum;
        uint16_t spi2DmaRxRemaining;
        uint16_t spi2DmaTxRemaining;
        uint8_t spi2DmaLastRxFirst;
        uint8_t spi2DmaLastRxLast;
        uint8_t spi2DmaStarted;
        uint16_t spi2SlaveOffset;
        uint8_t transmitActive;
        uint8_t spi2SlaveReady;
    } DataTransferSnapshot;

    void DataTransfer_Init(void);
    void DataTransfer_Process(void);
    int DataTransfer_AppendBytes(uint8_t type, const uint8_t *data, uint16_t length);
    int DataTransfer_AppendAlphabetPattern64K(uint8_t type);
    int DataTransfer_ClearPending(void);
    int DataTransfer_AppendString(uint8_t type, const char *data, uint16_t length);
    int DataTransfer_AppendRuntimeLog(const char *text, uint16_t length);
    int DataTransfer_AppendFirstUserInfo(const DataTransferFirstUserInfo *info);
    int DataTransfer_AppendCaptureRawData(const DataTransferCaptureChunk *chunk);
    int DataTransfer_AppendSweepBackgroundNoise(const DataTransferSweepChunk *chunk);
    int DataTransfer_StartTransmit(void);
    void DataTransfer_StopTransmit(void);
    void DataTransfer_SetUtcSeconds(uint32_t utcSeconds);
    uint32_t DataTransfer_GetPendingLength(void);
    uint32_t DataTransfer_ReadPending(uint32_t offset, uint8_t *data, uint32_t length);
    void DataTransfer_GetSnapshot(DataTransferSnapshot *snapshot);

#if defined(FPGA_PROTOCOL_HOST_TEST)
    void DataTransfer_TestReset(void);
    void DataTransfer_TestResetRuntimeOnly(void);
    void DataTransfer_TestSetTimestamp(uint32_t timestamp);
    uint32_t DataTransfer_TestCopyRamBuffer(uint8_t *dest, uint32_t capacity);
    void DataTransfer_TestReadFlash(uint32_t address, uint8_t *dest, uint32_t length);
    int DataTransfer_TestBuildNextFrame(uint8_t *frame);
    void DataTransfer_TestProcessOneFrameWithSendResult(int sendResult);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DATA_TRANSFER_H */
