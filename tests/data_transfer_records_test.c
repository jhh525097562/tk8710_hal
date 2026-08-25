#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "data_transfer.h"

static uint16_t ReadBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static uint32_t ReadBe32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           data[3];
}

static uint32_t CheckRecordHeader(const uint8_t *data, uint32_t offset,
                                  uint8_t format, uint8_t type,
                                  uint16_t payloadLength)
{
    assert(ReadBe32(&data[offset]) == 0x01020304UL);
    assert(data[offset + 4U] == format);
    assert(data[offset + 5U] == type);
    assert(ReadBe16(&data[offset + 6U]) == payloadLength);
    return offset + 8U;
}

int main(void)
{
    uint8_t records[2048];
    uint32_t offset = 0U;
    uint32_t length;
    DataTransferFirstUserInfo user;
    DataTransferCaptureChunk capture;
    DataTransferSweepPoint point;
    DataTransferSweepChunk sweep;
    static const char logText[] = "[BOOT][INFO] ready";
    static const uint8_t rawData[] = {0x11U, 0x22U, 0x33U};

    DataTransfer_TestReset();
    DataTransfer_Init();
    DataTransfer_TestSetTimestamp(0x01020304UL);

    assert(DataTransfer_AppendRuntimeLog(
               logText, (uint16_t)(sizeof(logText) - 1U)) == 0);
    length = DataTransfer_TestCopyRamBuffer(records, sizeof(records));
    offset = CheckRecordHeader(records, offset,
                               DATA_TRANSFER_RECORD_FORMAT_STRING,
                               DATA_TRANSFER_TYPE_RUNTIME_LOG_STATUS,
                               (uint16_t)(sizeof(logText) - 1U));
    assert(memcmp(&records[offset], logText, sizeof(logText) - 1U) == 0);
    offset += sizeof(logText) - 1U;
    assert(offset == length);

    (void)memset(&user, 0, sizeof(user));
    user.rateMode = 6U;
    user.rssi = -321;
    user.snr = 28U;
    user.dataLen = 52U;
    user.frameNo = 0x11223344UL;
    user.userId = 0x55667788UL;
    user.freqOffset = -128;
    user.frequencyHz = 507800000UL;
    user.pilotPower = 0x0102030405060708ULL;
    user.ahData[0] = 0x00012345UL;
    user.ahData[15] = 0x000ABCDEUL;
    assert(DataTransfer_AppendFirstUserInfo(&user) == 0);
    length = DataTransfer_TestCopyRamBuffer(records, sizeof(records));
    offset = CheckRecordHeader(records, offset,
                               DATA_TRANSFER_RECORD_FORMAT_BINARY,
                               DATA_TRANSFER_TYPE_FIRST_USER_INFO, 96U);
    assert(records[offset] == 1U);
    assert(records[offset + 1U] == 6U);
    assert((int16_t)ReadBe16(&records[offset + 2U]) == -321);
    assert(records[offset + 4U] == 28U);
    assert(ReadBe16(&records[offset + 6U]) == 52U);
    assert(ReadBe32(&records[offset + 8U]) == 0x11223344UL);
    assert(ReadBe32(&records[offset + 12U]) == 0x55667788UL);
    assert((int32_t)ReadBe32(&records[offset + 16U]) == -128);
    assert(ReadBe32(&records[offset + 20U]) == 507800000UL);
    assert(ReadBe32(&records[offset + 24U]) == 0x01020304UL);
    assert(ReadBe32(&records[offset + 28U]) == 0x05060708UL);
    assert(ReadBe32(&records[offset + 32U]) == 0x00012345UL);
    assert(ReadBe32(&records[offset + 92U]) == 0x000ABCDEUL);
    offset += 96U;
    assert(offset == length);

    (void)memset(&capture, 0, sizeof(capture));
    capture.rateMode = 6U;
    capture.antenna = 3U;
    capture.generation = 9U;
    capture.bytesPerAntenna = 32768U;
    capture.offset = 1024U;
    capture.data = rawData;
    capture.length = sizeof(rawData);
    assert(DataTransfer_AppendCaptureRawData(&capture) == 0);
    length = DataTransfer_TestCopyRamBuffer(records, sizeof(records));
    offset = CheckRecordHeader(records, offset,
                               DATA_TRANSFER_RECORD_FORMAT_BINARY,
                               DATA_TRANSFER_TYPE_CAPTURE_RAW_DATA, 23U);
    assert(records[offset] == 1U);
    assert(records[offset + 1U] == 6U);
    assert(records[offset + 2U] == 3U);
    assert(records[offset + 3U] == 1U);
    assert(ReadBe32(&records[offset + 4U]) == 9U);
    assert(ReadBe32(&records[offset + 8U]) == 32768U);
    assert(ReadBe32(&records[offset + 12U]) == 1024U);
    assert(ReadBe16(&records[offset + 16U]) == sizeof(rawData));
    assert(memcmp(&records[offset + 20U], rawData, sizeof(rawData)) == 0);
    offset += 23U;
    assert(offset == length);

    (void)memset(&point, 0, sizeof(point));
    point.frequencyHz = 507800000UL;
    point.noiseDbmHz[0] = 1.0F;
    point.noiseDbmHz[7] = -2.0F;
    (void)memset(&sweep, 0, sizeof(sweep));
    sweep.sweepMode = 1U;
    sweep.rateMode = 6U;
    sweep.generation = 10U;
    sweep.startFrequencyHz = 507800000UL;
    sweep.endFrequencyHz = 508675000UL;
    sweep.stepFrequencyHz = 125000UL;
    sweep.totalPoints = 8U;
    sweep.startIndex = 0U;
    sweep.points = &point;
    sweep.pointCount = 1U;
    assert(DataTransfer_AppendSweepBackgroundNoise(&sweep) == 0);
    length = DataTransfer_TestCopyRamBuffer(records, sizeof(records));
    offset = CheckRecordHeader(records, offset,
                               DATA_TRANSFER_RECORD_FORMAT_BINARY,
                               DATA_TRANSFER_TYPE_SWEEP_BACKGROUND_NOISE, 68U);
    assert(records[offset] == 1U);
    assert(records[offset + 1U] == 1U);
    assert(records[offset + 2U] == 6U);
    assert(records[offset + 3U] == 8U);
    assert(ReadBe32(&records[offset + 4U]) == 10U);
    assert(ReadBe32(&records[offset + 20U]) == 8U);
    assert(ReadBe32(&records[offset + 24U]) == 0U);
    assert(ReadBe16(&records[offset + 28U]) == 1U);
    assert(ReadBe32(&records[offset + 32U]) == 507800000UL);
    assert(ReadBe32(&records[offset + 36U]) == 0x3F800000UL);
    assert(ReadBe32(&records[offset + 64U]) == 0xC0000000UL);
    offset += 68U;
    assert(offset == length);

    puts("data_transfer_records_test: PASS");
    return 0;
}
