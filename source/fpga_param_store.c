#include "fpga_param_store.h"

#include <string.h>

#if !defined(FPGA_PROTOCOL_HOST_TEST)
#include "reg_flash.h"
#include "system.h"
#include "F021.h"
#endif

#define FPGA_PARAM_STORE_MAGIC 0x46504750UL
#define FPGA_PARAM_STORE_VERSION 2U
#define FPGA_PARAM_STORE_VERSION_LEGACY 1U
#define FPGA_PARAM_STORE_RECORD_SIZE 64U
#define FPGA_PARAM_STORE_LEGACY_RECORD_SIZE 32U
#define FPGA_PARAM_STORE_LEGACY_LENGTH 12U
#define FPGA_PARAM_STORE_SECTOR 2U
#define FPGA_BOOT_FLAG_STORE_SECTOR 1U
#define FPGA_BOOT_FLAG_PROGRAM_SIZE 8U
#define FPGA_PARAM_STORE_EMPTY_U32 0xFFFFFFFFUL

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t sequence;
    uint8_t workMode;
    uint8_t rateMode;
    uint8_t slotConfig;
    uint8_t txPower;
    uint32_t centerFreqHz;
    uint8_t rfMask;
    uint8_t reserved[7U];
    uint32_t checksum;
} FpgaParamLegacyRecord;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t sequence;
    uint8_t workMode;
    uint8_t rateMode;
    uint8_t slotConfig;
    uint8_t txPower;
    uint32_t centerFreqHz;
    uint8_t rfMask;
    uint8_t dcValidMask;
    uint8_t reserved0[2U];
    int16_t dcI[8U];
    int16_t dcQ[8U];
    uint8_t reserved1[4U];
    uint32_t checksum;
} FpgaParamRecord;

#if defined(FPGA_PROTOCOL_HOST_TEST)
static FpgaParamRecord g_fpgaParamStoreSector[FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_RECORD_SIZE];
static uint8_t g_fpgaBootFlagStoreSector[FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_SIZE];
static uint8_t g_fpgaParamStoreInitialized;
static uint8_t g_fpgaBootFlagSaveFailure;
static uint8_t g_fpgaBootFlagReadbackOverride;
static uint8_t g_fpgaBootFlagReadbackValue;
#endif

static uint32_t FpgaParamChecksumBytes(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t i;
    uint32_t sum = 0U;

    for (i = 0U; i < (length - 4U); i++)
    {
        sum = (sum << 5U) - sum + bytes[i];
    }
    return ~sum;
}

static uint32_t FpgaParamChecksum(const FpgaParamRecord *record)
{
    return FpgaParamChecksumBytes(record, FPGA_PARAM_STORE_RECORD_SIZE);
}

static uint32_t FpgaParamLegacyChecksum(const FpgaParamLegacyRecord *record)
{
    return FpgaParamChecksumBytes(record, FPGA_PARAM_STORE_LEGACY_RECORD_SIZE);
}

static int FpgaParamRecordIsBlank(const FpgaParamRecord *record)
{
    const uint32_t *words = (const uint32_t *)record;
    uint32_t i;

    for (i = 0U; i < (FPGA_PARAM_STORE_RECORD_SIZE / 4U); i++)
    {
        if (words[i] != FPGA_PARAM_STORE_EMPTY_U32)
        {
            return 0;
        }
    }
    return 1;
}

static int FpgaParamRecordIsValid(const FpgaParamRecord *record)
{
    return ((record->magic == FPGA_PARAM_STORE_MAGIC) &&
            (record->version == FPGA_PARAM_STORE_VERSION) &&
            (record->length == sizeof(FpgaStoredParams)) &&
            (record->checksum == FpgaParamChecksum(record)))
               ? 1
               : 0;
}

static int FpgaParamLegacyRecordIsValid(const FpgaParamLegacyRecord *record)
{
    return ((record->magic == FPGA_PARAM_STORE_MAGIC) &&
            (record->version == FPGA_PARAM_STORE_VERSION_LEGACY) &&
            (record->length == FPGA_PARAM_STORE_LEGACY_LENGTH) &&
            (record->checksum == FpgaParamLegacyChecksum(record)))
               ? 1
               : 0;
}

static void FpgaParamBuildRecord(const FpgaStoredParams *params,
                                 uint32_t sequence,
                                 FpgaParamRecord *record)
{
    (void)memset(record, 0xFF, sizeof(*record));
    record->magic = FPGA_PARAM_STORE_MAGIC;
    record->version = FPGA_PARAM_STORE_VERSION;
    record->length = sizeof(FpgaStoredParams);
    record->sequence = sequence;
    record->workMode = params->workMode;
    record->rateMode = params->rateMode;
    record->slotConfig = params->slotConfig;
    record->txPower = params->txPower;
    record->centerFreqHz = params->centerFreqHz;
    record->rfMask = params->rfMask;
    record->dcValidMask = params->dcValidMask;
    (void)memcpy(record->dcI, params->dcI, sizeof(record->dcI));
    (void)memcpy(record->dcQ, params->dcQ, sizeof(record->dcQ));
    record->checksum = FpgaParamChecksum(record);
}

static void FpgaParamCopyRecord(FpgaStoredParams *params,
                                const FpgaParamRecord *record)
{
    params->workMode = record->workMode;
    params->rateMode = record->rateMode;
    params->slotConfig = record->slotConfig;
    params->txPower = record->txPower;
    params->centerFreqHz = record->centerFreqHz;
    params->rfMask = record->rfMask;
    params->dcValidMask = record->dcValidMask;
    (void)memcpy(params->dcI, record->dcI, sizeof(params->dcI));
    (void)memcpy(params->dcQ, record->dcQ, sizeof(params->dcQ));
}

static void FpgaParamCopyLegacyRecord(FpgaStoredParams *params,
                                      const FpgaParamLegacyRecord *record)
{
    (void)memset(params, 0, sizeof(*params));
    params->workMode = record->workMode;
    params->rateMode = record->rateMode;
    params->slotConfig = record->slotConfig;
    params->txPower = record->txPower;
    params->centerFreqHz = record->centerFreqHz;
    params->rfMask = record->rfMask;
}

static uint8_t *FpgaBootFlagStoreBytes(void);

static uint32_t FpgaBootStoreReadBe32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           data[3];
}

static void FpgaBootStoreWriteBe32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static uint32_t FpgaBootStoreReadResetCount(void)
{
    const uint8_t *bytes = FpgaBootFlagStoreBytes();

    if ((bytes[1] == 0xFFU) && (bytes[2] == 0xFFU) &&
        (bytes[3] == 0xFFU) && (bytes[4] == 0xFFU))
    {
        return 0U;
    }
    return FpgaBootStoreReadBe32(&bytes[1]);
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
static void FpgaParamStoreEnsureInitialized(void)
{
    if (g_fpgaParamStoreInitialized == 0U)
    {
        (void)memset(g_fpgaParamStoreSector, 0xFF, sizeof(g_fpgaParamStoreSector));
        (void)memset(g_fpgaBootFlagStoreSector, 0xFF, sizeof(g_fpgaBootFlagStoreSector));
        g_fpgaParamStoreInitialized = 1U;
    }
}

static FpgaParamRecord *FpgaParamStoreRecords(void)
{
    FpgaParamStoreEnsureInitialized();
    return g_fpgaParamStoreSector;
}

static uint8_t *FpgaBootFlagStoreBytes(void)
{
    FpgaParamStoreEnsureInitialized();
    return g_fpgaBootFlagStoreSector;
}
#else
static FpgaParamRecord *FpgaParamStoreRecords(void)
{
    return (FpgaParamRecord *)FPGA_PARAM_STORE_BANK7_SECTOR2_BASE;
}

static uint8_t *FpgaBootFlagStoreBytes(void)
{
    return (uint8_t *)FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_BASE;
}

static void FpgaParamStoreAllowBlankEepromReads(void)
{
    flashWREG->FSMWRENA = 0x5U;
    flashWREG->EECTRL1 |= 0x00000020U;
    flashWREG->FSMWRENA = 0xAU;
}

Fapi_StatusType Fapi_serviceWatchdogTimer(void)
{
    return Fapi_Status_Success;
}

static int FpgaParamStoreFapiInit(uint32_t sector)
{
    if (Fapi_initializeFlashBanks((uint32_t)HCLK_FREQ) != Fapi_Status_Success)
    {
        return -1;
    }
    FpgaParamStoreAllowBlankEepromReads();
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    if (Fapi_setActiveFlashBank(Fapi_FlashBank7) != Fapi_Status_Success)
    {
        return -1;
    }
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    if (Fapi_enableEepromBankSectors((uint32_t)1U << sector,
                                     0U) != Fapi_Status_Success)
    {
        return -1;
    }
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    return 0;
}

static int FpgaParamStoreEraseSector(void)
{
    if (FpgaParamStoreFapiInit(FPGA_PARAM_STORE_SECTOR) != 0)
    {
        return -1;
    }
    if (Fapi_issueAsyncCommandWithAddress(
            Fapi_EraseSector,
            (uint32_t *)FPGA_PARAM_STORE_BANK7_SECTOR2_BASE) != Fapi_Status_Success)
    {
        return -1;
    }
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    return (FAPI_GET_FSM_STATUS == Fapi_Status_Success) ? 0 : -1;
}

static int FpgaParamStoreProgramRecord(uint32_t offset,
                                       const FpgaParamRecord *record)
{
    uint32_t address = FPGA_PARAM_STORE_BANK7_SECTOR2_BASE + offset;
    uint32_t written = 0U;
    uint8_t chunk;

    if (FpgaParamStoreFapiInit(FPGA_PARAM_STORE_SECTOR) != 0)
    {
        return -1;
    }

    while (written < FPGA_PARAM_STORE_RECORD_SIZE)
    {
        chunk = (uint8_t)((FPGA_PARAM_STORE_RECORD_SIZE - written) > 16U ? 16U : (FPGA_PARAM_STORE_RECORD_SIZE - written));
        if (Fapi_issueProgrammingCommand(
                (uint32_t *)(address + written),
                (uint8_t *)record + written,
                chunk,
                0,
                0,
                Fapi_AutoEccGeneration) != Fapi_Status_Success)
        {
            return -1;
        }
        while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
        {
        }
        if (FAPI_GET_FSM_STATUS != Fapi_Status_Success)
        {
            return -1;
        }
        written += chunk;
    }

    return (memcmp((const void *)address, record, FPGA_PARAM_STORE_RECORD_SIZE) == 0) ? 0 : -1;
}

static int FpgaBootFlagStoreEraseSector(void)
{
    if (FpgaParamStoreFapiInit(FPGA_BOOT_FLAG_STORE_SECTOR) != 0)
    {
        return -1;
    }
    if (Fapi_issueAsyncCommandWithAddress(
            Fapi_EraseSector,
            (uint32_t *)FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_BASE) != Fapi_Status_Success)
    {
        return -1;
    }
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    return (FAPI_GET_FSM_STATUS == Fapi_Status_Success) ? 0 : -1;
}

static int FpgaBootStoreProgram(uint8_t flag, uint32_t resetCount)
{
    uint8_t data[FPGA_BOOT_FLAG_PROGRAM_SIZE];

    (void)memset(data, 0xFF, sizeof(data));
    data[0] = flag;
    FpgaBootStoreWriteBe32(&data[1], resetCount);

    if (FpgaParamStoreFapiInit(FPGA_BOOT_FLAG_STORE_SECTOR) != 0)
    {
        return -1;
    }
    if (Fapi_issueProgrammingCommand(
            (uint32_t *)FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_BASE,
            data,
            sizeof(data),
            0,
            0,
            Fapi_AutoEccGeneration) != Fapi_Status_Success)
    {
        return -1;
    }
    while (FAPI_CHECK_FSM_READY_BUSY == Fapi_Status_FsmBusy)
    {
    }
    if (FAPI_GET_FSM_STATUS != Fapi_Status_Success)
    {
        return -1;
    }
    return (memcmp((const void *)FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_BASE,
                   data,
                   sizeof(data)) == 0) ? 0 : -1;
}
#endif

int FpgaParamStore_Load(FpgaStoredParams *params)
{
    const uint8_t *sector;
    const FpgaParamRecord *newest = 0;
    const FpgaParamLegacyRecord *legacyNewest = 0;
    uint32_t i;
    uint32_t newestSequence = 0U;
    uint32_t count;

    if (params == 0)
    {
        return -1;
    }

#if !defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreAllowBlankEepromReads();
#endif
    sector = (const uint8_t *)FpgaParamStoreRecords();
    count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_RECORD_SIZE;
    for (i = 0U; i < count; i++)
    {
        const FpgaParamRecord *record = (const FpgaParamRecord *)&sector[i * FPGA_PARAM_STORE_RECORD_SIZE];

        if (FpgaParamRecordIsValid(record) != 0)
        {
            if ((newest == 0) || (record->sequence > newestSequence))
            {
                newest = record;
                legacyNewest = 0;
                newestSequence = record->sequence;
            }
        }
    }

    count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_LEGACY_RECORD_SIZE;
    for (i = 0U; i < count; i++)
    {
        const FpgaParamLegacyRecord *record = (const FpgaParamLegacyRecord *)&sector[i * FPGA_PARAM_STORE_LEGACY_RECORD_SIZE];

        if (FpgaParamLegacyRecordIsValid(record) != 0)
        {
            if ((newest == 0) && ((legacyNewest == 0) || (record->sequence > newestSequence)))
            {
                legacyNewest = record;
                newestSequence = record->sequence;
            }
        }
    }

    if ((newest == 0) && (legacyNewest == 0))
    {
        return -1;
    }

    if (newest != 0)
    {
        FpgaParamCopyRecord(params, newest);
    }
    else
    {
        FpgaParamCopyLegacyRecord(params, legacyNewest);
    }
    return 0;
}

int FpgaParamStore_ParamsAreCorrupt(void)
{
    const uint8_t *sector;
    uint32_t i;
    uint32_t count;
    int sawNonBlank = 0;
    int sawValid = 0;

#if !defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreAllowBlankEepromReads();
#endif
    sector = (const uint8_t *)FpgaParamStoreRecords();
    count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_RECORD_SIZE;
    for (i = 0U; i < count; i++)
    {
        const FpgaParamRecord *record =
            (const FpgaParamRecord *)&sector[i * FPGA_PARAM_STORE_RECORD_SIZE];
        if (FpgaParamRecordIsBlank(record) == 0)
        {
            sawNonBlank = 1;
            if (FpgaParamRecordIsValid(record) != 0)
            {
                sawValid = 1;
            }
        }
    }

    count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE /
            FPGA_PARAM_STORE_LEGACY_RECORD_SIZE;
    for (i = 0U; i < count; i++)
    {
        const FpgaParamLegacyRecord *record =
            (const FpgaParamLegacyRecord *)&sector[i * FPGA_PARAM_STORE_LEGACY_RECORD_SIZE];
        if (FpgaParamLegacyRecordIsValid(record) != 0)
        {
            sawValid = 1;
        }
    }

    return ((sawNonBlank != 0) && (sawValid == 0)) ? 1 : 0;
}

int FpgaParamStore_Save(const FpgaStoredParams *params)
{
    uint8_t *sector;
    FpgaParamRecord record;
    uint32_t i;
    uint32_t nextIndex = FPGA_PARAM_STORE_EMPTY_U32;
    uint32_t nextSequence = 1U;
    uint32_t count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_RECORD_SIZE;

    if (params == 0)
    {
        return -1;
    }

    sector = (uint8_t *)FpgaParamStoreRecords();
    for (i = 0U; i < count; i++)
    {
        FpgaParamRecord *current = (FpgaParamRecord *)&sector[i * FPGA_PARAM_STORE_RECORD_SIZE];

        if (FpgaParamRecordIsBlank(current) != 0)
        {
            if (nextIndex == FPGA_PARAM_STORE_EMPTY_U32)
            {
                nextIndex = i;
            }
        }
        else if (FpgaParamRecordIsValid(current) != 0)
        {
            if (current->sequence >= nextSequence)
            {
                nextSequence = current->sequence + 1U;
            }
        }
    }

    count = FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE / FPGA_PARAM_STORE_LEGACY_RECORD_SIZE;
    for (i = 0U; i < count; i++)
    {
        const FpgaParamLegacyRecord *legacy = (const FpgaParamLegacyRecord *)&sector[i * FPGA_PARAM_STORE_LEGACY_RECORD_SIZE];

        if ((FpgaParamLegacyRecordIsValid(legacy) != 0) &&
            (legacy->sequence >= nextSequence))
        {
            nextSequence = legacy->sequence + 1U;
        }
    }

    if (nextIndex == FPGA_PARAM_STORE_EMPTY_U32)
    {
#if defined(FPGA_PROTOCOL_HOST_TEST)
        (void)memset(g_fpgaParamStoreSector, 0xFF, sizeof(g_fpgaParamStoreSector));
        nextIndex = 0U;
#else
        if (FpgaParamStoreEraseSector() != 0)
        {
            return -1;
        }
        nextIndex = 0U;
#endif
    }

    FpgaParamBuildRecord(params, nextSequence, &record);
#if defined(FPGA_PROTOCOL_HOST_TEST)
    (void)memcpy(&sector[nextIndex * FPGA_PARAM_STORE_RECORD_SIZE], &record, sizeof(record));
    return 0;
#else
    return FpgaParamStoreProgramRecord(nextIndex * FPGA_PARAM_STORE_RECORD_SIZE,
                                       &record);
#endif
}

int FpgaParamStore_LoadBootFlag(uint8_t *flag)
{
    if (flag == 0)
    {
        return -1;
    }

#if !defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreAllowBlankEepromReads();
#else
    if (g_fpgaBootFlagReadbackOverride != 0U)
    {
        *flag = g_fpgaBootFlagReadbackValue;
        return 0;
    }
#endif
    *flag = FpgaBootFlagStoreBytes()[0];
    return 0;
}

int FpgaParamStore_SaveBootFlag(uint8_t flag)
{
    uint32_t resetCount = 0U;

    (void)FpgaParamStore_LoadResetCount(&resetCount);
#if defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreEnsureInitialized();
    if (g_fpgaBootFlagSaveFailure != 0U)
    {
        return -1;
    }
    (void)memset(g_fpgaBootFlagStoreSector, 0xFF, sizeof(g_fpgaBootFlagStoreSector));
    g_fpgaBootFlagStoreSector[0] = flag;
    FpgaBootStoreWriteBe32(&g_fpgaBootFlagStoreSector[1], resetCount);
    return 0;
#else
    if (FpgaBootFlagStoreEraseSector() != 0)
    {
        return -1;
    }
    return FpgaBootStoreProgram(flag, resetCount);
#endif
}

int FpgaParamStore_LoadResetCount(uint32_t *count)
{
    if (count == 0)
    {
        return -1;
    }

#if !defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreAllowBlankEepromReads();
#endif
    *count = FpgaBootStoreReadResetCount();
    return 0;
}

int FpgaParamStore_SaveResetCount(uint32_t count)
{
    uint8_t flag = 0xFFU;

    (void)FpgaParamStore_LoadBootFlag(&flag);
#if defined(FPGA_PROTOCOL_HOST_TEST)
    FpgaParamStoreEnsureInitialized();
    (void)memset(g_fpgaBootFlagStoreSector, 0xFF, sizeof(g_fpgaBootFlagStoreSector));
    g_fpgaBootFlagStoreSector[0] = flag;
    FpgaBootStoreWriteBe32(&g_fpgaBootFlagStoreSector[1], count);
    return 0;
#else
    if (FpgaBootFlagStoreEraseSector() != 0)
    {
        return -1;
    }
    return FpgaBootStoreProgram(flag, count);
#endif
}

#if defined(FPGA_PROTOCOL_HOST_TEST)
void FpgaParamStore_TestErase(void)
{
    g_fpgaParamStoreInitialized = 1U;
    (void)memset(g_fpgaParamStoreSector, 0xFF, sizeof(g_fpgaParamStoreSector));
    (void)memset(g_fpgaBootFlagStoreSector, 0xFF, sizeof(g_fpgaBootFlagStoreSector));
    g_fpgaBootFlagSaveFailure = 0U;
    g_fpgaBootFlagReadbackOverride = 0U;
    g_fpgaBootFlagReadbackValue = 0xFFU;
}

void FpgaParamStore_TestCorrupt(void)
{
    g_fpgaParamStoreInitialized = 1U;
    (void)memset(g_fpgaParamStoreSector, 0x00, FPGA_PARAM_STORE_RECORD_SIZE);
}

void FpgaParamStore_TestSetBootFlagSaveFailure(uint8_t enabled)
{
    g_fpgaBootFlagSaveFailure = enabled;
}

void FpgaParamStore_TestSetBootFlagReadback(uint8_t enabled, uint8_t value)
{
    g_fpgaBootFlagReadbackOverride = enabled;
    g_fpgaBootFlagReadbackValue = value;
}

#endif
