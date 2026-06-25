/**
 * @file trm_satellite.c
 * @brief Satellite payload and ground-station role state for TRM.
 */

#include "../inc/trm/trm_satellite.h"
#include "../inc/trm/trm_internal.h"
#include "../inc/trm/trm_mac_parser.h"
#include "../inc/trm/trm_log.h"
#include "../inc/driver/tk8710_internal.h"
#include "../inc/driver/tk8710_types.h"
#include "../port/tk8710_hal.h"
#include <string.h>

#define TRM_SAT_GS_BEAM_MAX        5
#define TRM_SAT_UPLINK_CACHE_MAX   128
#define TRM_SAT_ROUTE_MAX          2048
#define TRM_SAT_PACKET_MAX_LEN     520
#define TRM_SAT_MAX_FORWARD_USERS  16
#define TRM_SAT_DEFAULT_TX_POWER   35
#define TRM_SAT_JOIN_TIMEOUT_MS    5000
#define TRM_SAT_JOIN_FRAME_LEN     16
#define TRM_SAT_DEFAULT_GS_ADDR    0x00000001u
#define TRM_SAT_DEFAULT_FREQ_OFFSET 20000
#define TRM_SAT_DEFAULT_PILOT_PWR  1000000ULL
#define TRM_SAT_DEFAULT_AH_BASE    8192u

typedef struct {
    uint8_t frameType;
    uint8_t devType;
    uint8_t nwkMode;
    uint8_t targetRateMode;
    uint32_t srcAddr;
    uint8_t valid;
} TRM_SatMacInfo;

typedef struct {
    uint32_t addr;
    TRM_BeamInfo beam;
    uint32_t lastUpdateMs;
    uint8_t valid;
} TRM_SatBeamEntry;

typedef struct {
    uint32_t terminalAddr;
    uint32_t groundStationAddr;
    uint32_t lastUpdateMs;
    uint8_t valid;
} TRM_SatRouteEntry;

typedef struct {
    uint32_t terminalAddr;
    uint8_t data[TRM_SAT_PACKET_MAX_LEN];
    uint16_t len;
    uint8_t txPower;
    uint8_t targetRateMode;
    uint32_t frameNo;
    uint32_t timestampMs;
    uint8_t valid;
} TRM_SatCacheItem;

typedef struct {
    uint32_t terminalAddr;
    uint32_t lastUpdateMs;
    uint8_t valid;
} TRM_SatTerminalBeamEntry;

typedef struct {
    TRM_NodeRole role;
    uint32_t localAddr;
    uint32_t gsBeamMax;
    uint32_t gsBeamTimeoutMs;
    uint32_t uplinkCacheMax;
    uint32_t gsTerminalBeamMax;
    uint32_t joinMaintainMs;
    uint32_t bcnThreshold;
    uint32_t beamMissCount;
    uint32_t gsBeamCount;
    uint32_t routeCount;
    uint32_t gsTerminalBeamCount;
    uint8_t gsOnline;
    uint8_t bcnOkCount;
    uint8_t joinPending;
    uint32_t lastGsTxMs;
    uint32_t lastJoinReqMs;
    TRM_SatBeamEntry gsBeams[TRM_SAT_GS_BEAM_MAX];
    TRM_SatRouteEntry routes[TRM_SAT_ROUTE_MAX];
    TRM_SatCacheItem cache[TRM_SAT_UPLINK_CACHE_MAX];
    TRM_SatTerminalBeamEntry terminalBeams[TRM_SAT_ROUTE_MAX];
    TRM_BeamInfo satelliteBeam;
    uint32_t cacheHead;
    uint32_t cacheCount;
    uint32_t randState;
    uint8_t satelliteBeamValid;
    uint8_t autoForwarding;
} TRM_SatelliteContext;

static TRM_SatelliteContext g_satCtx;

static uint32_t trm_sat_now_ms(void)
{
    return (uint32_t)(TK8710GetTimeUs() / 1000);
}

static uint32_t trm_sat_default_u32(uint32_t value, uint32_t def, uint32_t max)
{
    if (value == 0) {
        value = def;
    }
    if (max > 0 && value > max) {
        value = max;
    }
    return value;
}

static int trm_sat_is_enabled(void)
{
    return g_satCtx.role == TRM_NODE_ROLE_SAT_PAYLOAD ||
           g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION;
}

static uint8_t trm_sat_default_rate_mode(void)
{
    const slotCfg_t* slotCfg = TK8710GetSlotConfig();

    if (slotCfg != NULL && slotCfg->rateCount > 0) {
        return slotCfg->rateModes[0];
    }
    return TK8710_RATE_MODE_8;
}

static uint32_t trm_sat_to_spi_u32(uint32_t value)
{
    uint32_t encoded = 0;
    uint8_t* bytes = (uint8_t*)&encoded;

    bytes[0] = (uint8_t)((value >> 24) & 0xFF);
    bytes[1] = (uint8_t)((value >> 16) & 0xFF);
    bytes[2] = (uint8_t)((value >> 8) & 0xFF);
    bytes[3] = (uint8_t)(value & 0xFF);
    return encoded;
}

static uint32_t trm_sat_freq_offset_to_raw(int32_t freqOffset)
{
    uint32_t magnitude;

    if (freqOffset >= 0) {
        return ((uint32_t)freqOffset << 7) & 0x03FFFFFFu;
    }

    magnitude = ((uint32_t)(-freqOffset) << 7) & 0x03FFFFFFu;
    return ((~magnitude) + 1u) & 0x03FFFFFFu;
}

static uint64_t trm_sat_to_spi_u40(uint64_t value)
{
    uint64_t encoded = 0;
    uint8_t* bytes = (uint8_t*)&encoded;

    bytes[0] = (uint8_t)((value >> 32) & 0xFF);
    bytes[1] = (uint8_t)((value >> 24) & 0xFF);
    bytes[2] = (uint8_t)((value >> 16) & 0xFF);
    bytes[3] = (uint8_t)((value >> 8) & 0xFF);
    bytes[4] = (uint8_t)(value & 0xFF);
    return encoded;
}

static uint32_t trm_sat_rand_u32(void)
{
    uint64_t now;

    if (g_satCtx.randState == 0) {
        now = TK8710GetTimeUs();
        g_satCtx.randState = (uint32_t)now ^ (uint32_t)(now >> 32) ^
                             g_satCtx.localAddr ^ 0xA5A55A5Au;
        if (g_satCtx.randState == 0) {
            g_satCtx.randState = 1;
        }
    }

    g_satCtx.randState = g_satCtx.randState * 1664525u + 1013904223u;
    return g_satCtx.randState;
}

static int32_t trm_sat_join_freq_limit(uint8_t rateMode)
{
    switch (rateMode) {
        case TK8710_RATE_MODE_5:
            return 30000;
        case TK8710_RATE_MODE_6:
            return 60000;
        case TK8710_RATE_MODE_7:
            return 120000;
        case TK8710_RATE_MODE_8:
            return 240000;
        case TK8710_RATE_MODE_9:
            return 230000;
        case TK8710_RATE_MODE_10:
            return 220000;
        case TK8710_RATE_MODE_11:
        case TK8710_RATE_MODE_18:
            return 210000;
        default:
            return 240000;
    }
}

static int32_t trm_sat_random_join_freq_offset(uint8_t rateMode)
{
    int32_t limit = trm_sat_join_freq_limit(rateMode);
    uint32_t span = (uint32_t)(limit * 2 + 1);

    return (int32_t)(trm_sat_rand_u32() % span) - limit;
}

static void trm_sat_set_cached_ah(TRM_BeamInfo* beam, uint8_t ant, uint32_t iData,
    uint32_t qData)
{
    uint64_t ah40;
    uint64_t encoded;

    if (beam == NULL || ant >= 8) {
        return;
    }

    ah40 = (((uint64_t)iData & 0xFFFFFULL) << 20) | ((uint64_t)qData & 0xFFFFFULL);
    encoded = trm_sat_to_spi_u40(ah40);
    beam->ahData[ant * 2] = (uint32_t)((encoded >> 20) & 0xFFFFF);
    beam->ahData[ant * 2 + 1] = (uint32_t)(encoded & 0xFFFFF);
}

static void trm_sat_fill_default_beam(uint32_t userId, TRM_BeamInfo* beam)
{
    if (beam == NULL) {
        return;
    }

    memset(beam, 0, sizeof(*beam));
    beam->userId = userId;
    beam->freq = trm_sat_to_spi_u32(trm_sat_freq_offset_to_raw(TRM_SAT_DEFAULT_FREQ_OFFSET));
    beam->pilotPower = trm_sat_to_spi_u40(TRM_SAT_DEFAULT_PILOT_PWR);
    beam->timestamp = trm_sat_now_ms();
    beam->valid = 1;
    trm_sat_set_cached_ah(beam, 0, TRM_SAT_DEFAULT_AH_BASE, 0);
}

static void trm_sat_randomize_join_beam(uint8_t rateMode, int32_t* freqOffset,
    uint32_t* freqRaw)
{
    int32_t offset = trm_sat_random_join_freq_offset(rateMode);
    uint32_t raw = trm_sat_freq_offset_to_raw(offset);

    if (!g_satCtx.satelliteBeamValid) {
        trm_sat_fill_default_beam(g_satCtx.localAddr, &g_satCtx.satelliteBeam);
        g_satCtx.satelliteBeamValid = 1;
    }
    g_satCtx.satelliteBeam.freq = trm_sat_to_spi_u32(raw);
    g_satCtx.satelliteBeam.timestamp = trm_sat_now_ms();

    if (freqOffset != NULL) {
        *freqOffset = offset;
    }
    if (freqRaw != NULL) {
        *freqRaw = raw;
    }
}

static void trm_sat_update_satellite_beam_from_bcn(const TK8710IrqResult* irqResult)
{
    if (irqResult == NULL || g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION) {
        return;
    }

    if (!g_satCtx.satelliteBeamValid) {
        trm_sat_fill_default_beam(g_satCtx.localAddr, &g_satCtx.satelliteBeam);
        g_satCtx.satelliteBeamValid = 1;
    } else {
        g_satCtx.satelliteBeam.timestamp = trm_sat_now_ms();
    }
    TRM_LOG_DEBUG("TRM SAT: satellite beam refreshed by BCN offset=%d bits=%u",
                  irqResult->bcn_freq_offset, irqResult->rx_bcnbits);
}

static int trm_sat_parse_mac(const uint8_t* data, uint16_t len, TRM_SatMacInfo* info)
{
    TrmMacMhdr mhdr;

    if (data == NULL || info == NULL || len < 3) {
        return TRM_ERR_PARAM;
    }

    memset(info, 0, sizeof(*info));
    if (TRM_ParseMacMhdr(data, len, &mhdr) != 0) {
        return TRM_ERR_PARAM;
    }
    if (TRM_ExtractSrcAddrFromMacFrame(data, len, &info->srcAddr) != 0) {
        return TRM_ERR_PARAM;
    }

    info->frameType = mhdr.frameType;
    info->devType = mhdr.devType;
    info->nwkMode = mhdr.nwkMode;
    info->targetRateMode = 0;
    info->valid = 1;
    return TRM_OK;
}

static int trm_sat_is_data_frame(const TRM_SatMacInfo* info)
{
    return info->frameType == TRM_MAC_FRAMETYPE_CONFIRM_DATA ||
           info->frameType == TRM_MAC_FRAMETYPE_UNCONFIRM_DATA;
}

static int trm_sat_is_terminal_uplink_frame(const TRM_SatMacInfo* info)
{
    return info->devType == TRM_MAC_DEVTYPE_TERMINAL &&
           (info->frameType == TRM_MAC_FRAMETYPE_JOIN_REQUEST ||
            trm_sat_is_data_frame(info));
}

static int trm_sat_is_ground_station_downlink_frame(const TRM_SatMacInfo* info)
{
    return info->frameType == TRM_MAC_FRAMETYPE_JOIN_ACCEPT ||
           trm_sat_is_data_frame(info);
}

static int trm_sat_is_ground_station(const TRM_SatMacInfo* info)
{
    return info->devType == TRM_MAC_DEVTYPE_MULTI_CHANNEL_GW ||
           info->devType == TRM_MAC_DEVTYPE_MULTI_ANTENNA_GW;
}

static void trm_sat_route_put(uint32_t terminalAddr, uint32_t groundStationAddr)
{
    int freeIndex = -1;
    uint32_t now = trm_sat_now_ms();

    for (uint32_t i = 0; i < TRM_SAT_ROUTE_MAX; i++) {
        if (g_satCtx.routes[i].valid &&
            g_satCtx.routes[i].terminalAddr == terminalAddr) {
            g_satCtx.routes[i].groundStationAddr = groundStationAddr;
            g_satCtx.routes[i].lastUpdateMs = now;
            TRM_LOG_INFO("TRM SAT: route update terminal=0x%08X -> gs=0x%08X",
                         terminalAddr, groundStationAddr);
            return;
        }
        if (!g_satCtx.routes[i].valid && freeIndex < 0) {
            freeIndex = (int)i;
        }
    }

    if (freeIndex < 0) {
        uint32_t oldest = 0;
        uint32_t oldestMs = g_satCtx.routes[0].lastUpdateMs;
        for (uint32_t i = 1; i < TRM_SAT_ROUTE_MAX; i++) {
            if (g_satCtx.routes[i].lastUpdateMs < oldestMs) {
                oldestMs = g_satCtx.routes[i].lastUpdateMs;
                oldest = i;
            }
        }
        freeIndex = (int)oldest;
    } else {
        g_satCtx.routeCount++;
    }

    g_satCtx.routes[freeIndex].terminalAddr = terminalAddr;
    g_satCtx.routes[freeIndex].groundStationAddr = groundStationAddr;
    g_satCtx.routes[freeIndex].lastUpdateMs = now;
    g_satCtx.routes[freeIndex].valid = 1;
    TRM_LOG_INFO("TRM SAT: route add terminal=0x%08X -> gs=0x%08X",
                 terminalAddr, groundStationAddr);
}

static int trm_sat_route_get(uint32_t terminalAddr, uint32_t* groundStationAddr)
{
    if (groundStationAddr == NULL) {
        return TRM_ERR_PARAM;
    }

    for (uint32_t i = 0; i < TRM_SAT_ROUTE_MAX; i++) {
        if (g_satCtx.routes[i].valid &&
            g_satCtx.routes[i].terminalAddr == terminalAddr) {
            *groundStationAddr = g_satCtx.routes[i].groundStationAddr;
            return TRM_OK;
        }
    }
    return TRM_ERR_NO_BEAM;
}

static int trm_sat_terminal_beam_find(uint32_t terminalAddr)
{
    for (uint32_t i = 0; i < g_satCtx.gsTerminalBeamMax; i++) {
        if (g_satCtx.terminalBeams[i].valid &&
            g_satCtx.terminalBeams[i].terminalAddr == terminalAddr) {
            return (int)i;
        }
    }
    return -1;
}

static int trm_sat_terminal_beam_free_index(void)
{
    for (uint32_t i = 0; i < g_satCtx.gsTerminalBeamMax; i++) {
        if (!g_satCtx.terminalBeams[i].valid) {
            return (int)i;
        }
    }
    return -1;
}

static int trm_sat_terminal_beam_oldest_index(void)
{
    uint32_t oldest = 0;
    uint32_t oldestMs = g_satCtx.terminalBeams[0].lastUpdateMs;

    for (uint32_t i = 1; i < g_satCtx.gsTerminalBeamMax; i++) {
        if (g_satCtx.terminalBeams[i].lastUpdateMs < oldestMs) {
            oldestMs = g_satCtx.terminalBeams[i].lastUpdateMs;
            oldest = i;
        }
    }
    return (int)oldest;
}

static int trm_sat_is_ground_station_terminal_data(const uint8_t* data, uint16_t len)
{
    TRM_SatMacInfo info;

    if (g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION ||
        trm_sat_parse_mac(data, len, &info) != TRM_OK ||
        info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return 0;
    }

    return trm_sat_is_terminal_uplink_frame(&info);
}

static void trm_sat_gs_beam_put(uint32_t gsAddr, const TRM_BeamInfo* beam)
{
    int freeIndex = -1;
    uint32_t now = trm_sat_now_ms();

    if (beam == NULL) {
        return;
    }

    for (uint32_t i = 0; i < g_satCtx.gsBeamMax; i++) {
        if (g_satCtx.gsBeams[i].valid && g_satCtx.gsBeams[i].addr == gsAddr) {
            g_satCtx.gsBeams[i].beam = *beam;
            g_satCtx.gsBeams[i].beam.userId = gsAddr;
            g_satCtx.gsBeams[i].lastUpdateMs = now;
            TRM_LOG_INFO("TRM SAT: ground-station beam update addr=0x%08X", gsAddr);
            return;
        }
        if (!g_satCtx.gsBeams[i].valid && freeIndex < 0) {
            freeIndex = (int)i;
        }
    }

    if (freeIndex < 0) {
        uint32_t oldest = 0;
        uint32_t oldestMs = g_satCtx.gsBeams[0].lastUpdateMs;
        for (uint32_t i = 1; i < g_satCtx.gsBeamMax; i++) {
            if (g_satCtx.gsBeams[i].lastUpdateMs < oldestMs) {
                oldestMs = g_satCtx.gsBeams[i].lastUpdateMs;
                oldest = i;
            }
        }
        freeIndex = (int)oldest;
    } else {
        g_satCtx.gsBeamCount++;
    }

    g_satCtx.gsBeams[freeIndex].addr = gsAddr;
    g_satCtx.gsBeams[freeIndex].beam = *beam;
    g_satCtx.gsBeams[freeIndex].beam.userId = gsAddr;
    g_satCtx.gsBeams[freeIndex].lastUpdateMs = now;
    g_satCtx.gsBeams[freeIndex].valid = 1;
    TRM_LOG_INFO("TRM SAT: ground-station beam add addr=0x%08X", gsAddr);
}

static int trm_sat_get_latest_gs(uint32_t* gsAddr, TRM_BeamInfo* beam)
{
    int found = -1;
    uint32_t latestMs = 0;
    uint32_t now = trm_sat_now_ms();

    for (uint32_t i = 0; i < g_satCtx.gsBeamMax; i++) {
        TRM_SatBeamEntry* entry = &g_satCtx.gsBeams[i];
        if (!entry->valid) {
            continue;
        }
        if (g_satCtx.gsBeamTimeoutMs > 0 &&
            (now - entry->lastUpdateMs) > g_satCtx.gsBeamTimeoutMs) {
            entry->valid = 0;
            if (g_satCtx.gsBeamCount > 0) {
                g_satCtx.gsBeamCount--;
            }
            continue;
        }
        if (found < 0 || entry->lastUpdateMs >= latestMs) {
            latestMs = entry->lastUpdateMs;
            found = (int)i;
        }
    }

    if (found < 0) {
        return TRM_ERR_NO_BEAM;
    }
    if (gsAddr != NULL) {
        *gsAddr = g_satCtx.gsBeams[found].addr;
    }
    if (beam != NULL) {
        *beam = g_satCtx.gsBeams[found].beam;
    }
    return TRM_OK;
}

static int trm_sat_cache_push(uint32_t terminalAddr, const uint8_t* data, uint16_t len,
                              uint8_t txPower, uint8_t targetRateMode, uint32_t frameNo)
{
    TRM_SatCacheItem* item;
    uint32_t index;

    if (data == NULL || len == 0 || len > TRM_SAT_PACKET_MAX_LEN) {
        return TRM_ERR_PARAM;
    }

    if (g_satCtx.cacheCount >= g_satCtx.uplinkCacheMax) {
        g_satCtx.cache[g_satCtx.cacheHead].valid = 0;
        g_satCtx.cacheHead = (g_satCtx.cacheHead + 1) % g_satCtx.uplinkCacheMax;
        g_satCtx.cacheCount--;
        TRM_LOG_WARN("TRM SAT: uplink cache full, dropped oldest item");
    }

    index = (g_satCtx.cacheHead + g_satCtx.cacheCount) % g_satCtx.uplinkCacheMax;
    item = &g_satCtx.cache[index];
    memset(item, 0, sizeof(*item));
    item->terminalAddr = terminalAddr;
    memcpy(item->data, data, len);
    item->len = len;
    item->txPower = txPower;
    item->targetRateMode = targetRateMode;
    item->frameNo = frameNo;
    item->timestampMs = trm_sat_now_ms();
    item->valid = 1;
    g_satCtx.cacheCount++;
    TRM_LOG_INFO("TRM SAT: cache terminal uplink terminal=0x%08X cache=%u/%u",
                 terminalAddr, g_satCtx.cacheCount, g_satCtx.uplinkCacheMax);
    return TRM_OK;
}

static int trm_sat_forward_to_beam(uint32_t beamAddr, const uint8_t* data, uint16_t len,
                                   uint8_t txPower, uint8_t targetRateMode)
{
    uint8_t oldAuto = g_satCtx.autoForwarding;
    int ret;

    if (targetRateMode == 0) {
        targetRateMode = trm_sat_default_rate_mode();
    }
    g_satCtx.autoForwarding = 1;
    ret = TRM_SendData(beamAddr, data, len, txPower, 0xFF, targetRateMode,
                       TK8710_DATA_TYPE_DED);
    g_satCtx.autoForwarding = oldAuto;
    return ret;
}

static void trm_sat_flush_cache(uint8_t maxUserCount)
{
    uint8_t sent = 0;

    if (g_satCtx.cacheCount == 0) {
        // TRM_LOG_INFO("TRM SAT: uplink cache flush skipped empty");
        return;
    }

    TRM_LOG_INFO("TRM SAT: uplink cache flush start cache=%u/%u",
                 g_satCtx.cacheCount, g_satCtx.uplinkCacheMax);

    while (g_satCtx.cacheCount > 0 && sent < maxUserCount) {
        TRM_SatCacheItem item = g_satCtx.cache[g_satCtx.cacheHead];
        uint32_t gsAddr;
        int ret;

        if (!item.valid) {
            g_satCtx.cacheHead = (g_satCtx.cacheHead + 1) % g_satCtx.uplinkCacheMax;
            g_satCtx.cacheCount--;
            continue;
        }

        if (trm_sat_get_latest_gs(&gsAddr, NULL) != TRM_OK) {
            TRM_LOG_WARN("TRM SAT: uplink cache flush deferred, no ground-station beam cache=%u/%u",
                         g_satCtx.cacheCount, g_satCtx.uplinkCacheMax);
            return;
        }

        ret = trm_sat_forward_to_beam(gsAddr, item.data, item.len, item.txPower,
                                      item.targetRateMode);
        if (ret != TRM_OK) {
            g_satCtx.beamMissCount++;
            TRM_LOG_WARN("TRM SAT: cached uplink forward failed terminal=0x%08X ret=%d",
                         item.terminalAddr, ret);
            return;
        }

        trm_sat_route_put(item.terminalAddr, gsAddr);
        g_satCtx.cache[g_satCtx.cacheHead].valid = 0;
        g_satCtx.cacheHead = (g_satCtx.cacheHead + 1) % g_satCtx.uplinkCacheMax;
        g_satCtx.cacheCount--;
        sent++;
        TRM_LOG_INFO("TRM SAT: cached uplink queued terminal=0x%08X gs=0x%08X",
                     item.terminalAddr, gsAddr);
    }
}

static void trm_sat_build_join_frame(uint8_t frameType, uint32_t srcAddr,
                                     uint8_t* data, uint16_t* len)
{
    TrmMacMhdr mhdr;

    memset(data, 0, TRM_SAT_JOIN_FRAME_LEN);
    memset(&mhdr, 0, sizeof(mhdr));
    mhdr.frameType = frameType;
    mhdr.devType = TRM_MAC_DEVTYPE_MULTI_ANTENNA_GW;
    mhdr.version = 0;
    mhdr.addrMode = 1;
    mhdr.nwkMode = TRM_MAC_NWKMODE_SATELLITE;
    (void)TRM_BuildMacMhdr(&mhdr, data, TRM_SAT_JOIN_FRAME_LEN);

    data[3] = 0;
    data[4] = (uint8_t)(srcAddr & 0xFF);
    data[5] = (uint8_t)((srcAddr >> 8) & 0xFF);
    data[6] = (uint8_t)((srcAddr >> 16) & 0xFF);
    data[7] = (uint8_t)((srcAddr >> 24) & 0xFF);
    *len = TRM_SAT_JOIN_FRAME_LEN;
}

static int trm_sat_queue_join_request(void)
{
    uint8_t data[TRM_SAT_JOIN_FRAME_LEN];
    uint16_t len = 0;
    uint32_t now = trm_sat_now_ms();
    uint8_t oldAuto = g_satCtx.autoForwarding;
    uint8_t rateMode = trm_sat_default_rate_mode();
    int32_t freqOffset = 0;
    uint32_t freqRaw = 0;
    int ret;

    trm_sat_build_join_frame(TRM_MAC_FRAMETYPE_JOIN_REQUEST,
                             g_satCtx.localAddr, data, &len);
    trm_sat_randomize_join_beam(rateMode, &freqOffset, &freqRaw);
    g_satCtx.autoForwarding = 1;
    ret = TRM_SendData(g_satCtx.localAddr, data, len, TRM_SAT_DEFAULT_TX_POWER,
                       0xFF, rateMode, TK8710_DATA_TYPE_DED);
    g_satCtx.autoForwarding = oldAuto;
    if (ret == TRM_OK) {
        g_satCtx.joinPending = 1;
        g_satCtx.lastJoinReqMs = now;
        g_satCtx.lastGsTxMs = now;
        TRM_LOG_INFO("TRM SAT: ground-station join request queued addr=0x%08X mode=%u freqOffset=%d freqRaw=0x%08X",
                     g_satCtx.localAddr, rateMode, freqOffset, freqRaw);
    } else {
        TRM_LOG_WARN("TRM SAT: ground-station join request queue failed ret=%d", ret);
    }
    return ret;
}

static int trm_sat_queue_join_accept(uint32_t gsAddr)
{
    uint8_t data[TRM_SAT_JOIN_FRAME_LEN];
    uint16_t len = 0;

    trm_sat_build_join_frame(TRM_MAC_FRAMETYPE_JOIN_ACCEPT,
                             g_satCtx.localAddr, data, &len);
    return trm_sat_forward_to_beam(gsAddr, data, len,
                                   TRM_SAT_DEFAULT_TX_POWER, 0);
}

static void trm_sat_payload_process_terminal(const TRM_RxUserData* user,
                                             const TRM_SatMacInfo* info)
{
    uint32_t gsAddr;
    int ret;

    ret = trm_sat_get_latest_gs(&gsAddr, NULL);
    if (ret != TRM_OK) {
        g_satCtx.beamMissCount++;
        (void)trm_sat_cache_push(info->srcAddr, user->data, user->dataLen,
                                 TRM_SAT_DEFAULT_TX_POWER, info->targetRateMode,
                                 user->rateMode);
        return;
    }

    ret = trm_sat_forward_to_beam(gsAddr, user->data, user->dataLen,
                                  TRM_SAT_DEFAULT_TX_POWER, info->targetRateMode);
    if (ret == TRM_OK) {
        trm_sat_route_put(info->srcAddr, gsAddr);
        TRM_LOG_INFO("TRM SAT: terminal uplink queued terminal=0x%08X gs=0x%08X",
                     info->srcAddr, gsAddr);
    } else {
        g_satCtx.beamMissCount++;
        TRM_LOG_WARN("TRM SAT: terminal uplink queue failed terminal=0x%08X ret=%d",
                     info->srcAddr, ret);
    }
}

static void trm_sat_payload_process_ground_station(const TRM_RxUserData* user,
                                                   const TRM_SatMacInfo* info)
{
    uint32_t routeGsAddr;
    int ret;

    if (info->frameType == TRM_MAC_FRAMETYPE_JOIN_REQUEST) {
        trm_sat_gs_beam_put(info->srcAddr, &user->beam);
        trm_sat_flush_cache(TRM_SAT_MAX_FORWARD_USERS);
        ret = trm_sat_queue_join_accept(info->srcAddr);
        TRM_LOG_INFO("TRM SAT: join request from gs=0x%08X, accept ret=%d",
                     info->srcAddr, ret);
        return;
    }

    if (!trm_sat_is_ground_station_downlink_frame(info)) {
        return;
    }

    if (trm_sat_route_get(info->srcAddr, &routeGsAddr) == TRM_OK) {
        trm_sat_gs_beam_put(routeGsAddr, &user->beam);
        trm_sat_flush_cache(TRM_SAT_MAX_FORWARD_USERS);
    } else {
        g_satCtx.beamMissCount++;
        TRM_LOG_WARN("TRM SAT: ground-station route miss for terminal=0x%08X frame=%u",
                     info->srcAddr, info->frameType);
        return;
    }

    ret = trm_sat_forward_to_beam(info->srcAddr, user->data, user->dataLen,
                                  TRM_SAT_DEFAULT_TX_POWER, info->targetRateMode);
    if (ret == TRM_OK) {
        TRM_LOG_INFO("TRM SAT: downlink response queued terminal=0x%08X frame=%u",
                     info->srcAddr, info->frameType);
    } else {
        g_satCtx.beamMissCount++;
        TRM_LOG_WARN("TRM SAT: downlink response queue failed terminal=0x%08X ret=%d",
                     info->srcAddr, ret);
    }
}

static void trm_sat_ground_station_process_rx(const TRM_RxUserData* user,
                                              const TRM_SatMacInfo* info)
{
    if (trm_sat_is_ground_station(info) &&
        info->frameType == TRM_MAC_FRAMETYPE_JOIN_ACCEPT) {
        g_satCtx.gsOnline = 1;
        g_satCtx.joinPending = 0;
        TRM_LOG_INFO("TRM SAT: ground-station online by join accept");
        return;
    }

    if (info->devType == TRM_MAC_DEVTYPE_TERMINAL && trm_sat_is_data_frame(info)) {
        TRM_LOG_INFO("TRM SAT: ground-station terminal beam updated terminal=0x%08X",
                     info->srcAddr);
    }
}

void TRM_SatelliteInit(const TRM_InitConfig* config)
{
    memset(&g_satCtx, 0, sizeof(g_satCtx));
    if (config == NULL) {
        return;
    }

    g_satCtx.role = config->nodeRole;
    g_satCtx.localAddr = config->localAddr;
    if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION && g_satCtx.localAddr == 0) {
        g_satCtx.localAddr = TRM_SAT_DEFAULT_GS_ADDR;
    }
    g_satCtx.joinMaintainMs = trm_sat_default_u32(config->groundStationJoinMaintainMs,
        TRM_GS_JOIN_MAINTAIN_DEFAULT, 0);
    g_satCtx.gsBeamMax = trm_sat_default_u32(config->groundStationBeamMax,
        TRM_GS_BEAM_MAX_DEFAULT, TRM_SAT_GS_BEAM_MAX);
    g_satCtx.gsBeamTimeoutMs = trm_sat_default_u32(config->groundStationBeamTimeoutMs,
        g_satCtx.joinMaintainMs, 0);
    g_satCtx.uplinkCacheMax = trm_sat_default_u32(config->satelliteUplinkCacheSize,
        TRM_SAT_UPLINK_CACHE_DEFAULT, TRM_SAT_UPLINK_CACHE_MAX);
    g_satCtx.gsTerminalBeamMax = trm_sat_default_u32(config->groundStationTerminalBeamMax,
        TRM_GS_TERMINAL_BEAM_DEFAULT, TRM_SAT_ROUTE_MAX);
    g_satCtx.bcnThreshold = trm_sat_default_u32(config->groundStationBcnThreshold,
        TRM_GS_BCN_THRESHOLD_DEFAULT, 0);

    TRM_LOG_INFO("TRM SAT: role=%d local=0x%08X gsBeamMax=%u gsBeamTimeoutMs=%u joinMaintainMs=%u cache=%u",
                 g_satCtx.role, g_satCtx.localAddr,
                 g_satCtx.gsBeamMax, g_satCtx.gsBeamTimeoutMs,
                 g_satCtx.joinMaintainMs, g_satCtx.uplinkCacheMax);
}

void TRM_SatelliteDeinit(void)
{
    memset(&g_satCtx, 0, sizeof(g_satCtx));
}

void TRM_SatelliteReset(void)
{
    TRM_NodeRole role = g_satCtx.role;
    uint32_t localAddr = g_satCtx.localAddr;
    uint32_t gsBeamMax = g_satCtx.gsBeamMax;
    uint32_t gsBeamTimeoutMs = g_satCtx.gsBeamTimeoutMs;
    uint32_t uplinkCacheMax = g_satCtx.uplinkCacheMax;
    uint32_t gsTerminalBeamMax = g_satCtx.gsTerminalBeamMax;
    uint32_t joinMaintainMs = g_satCtx.joinMaintainMs;
    uint32_t bcnThreshold = g_satCtx.bcnThreshold;

    memset(&g_satCtx, 0, sizeof(g_satCtx));
    g_satCtx.role = role;
    g_satCtx.localAddr = localAddr;
    g_satCtx.gsBeamMax = gsBeamMax;
    g_satCtx.gsBeamTimeoutMs = gsBeamTimeoutMs;
    g_satCtx.uplinkCacheMax = uplinkCacheMax;
    g_satCtx.gsTerminalBeamMax = gsTerminalBeamMax;
    g_satCtx.joinMaintainMs = joinMaintainMs;
    g_satCtx.bcnThreshold = bcnThreshold;
}

int TRM_SatelliteAllowRxBeamStore(const uint8_t* data, uint16_t len)
{
    TRM_SatMacInfo info;

    if (g_satCtx.role != TRM_NODE_ROLE_SAT_PAYLOAD ||
        trm_sat_parse_mac(data, len, &info) != TRM_OK ||
        info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return 1;
    }

    return trm_sat_is_ground_station(&info) ? 0 : 1;
}

uint8_t TRM_SatelliteKeepRxBeam(const uint8_t* data, uint16_t len)
{
    TRM_SatMacInfo info;

    if (!trm_sat_is_enabled() ||
        trm_sat_parse_mac(data, len, &info) != TRM_OK ||
        info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return 0;
    }

    if (g_satCtx.role == TRM_NODE_ROLE_SAT_PAYLOAD) {
        return trm_sat_is_terminal_uplink_frame(&info);
    }
    if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION) {
        return trm_sat_is_terminal_uplink_frame(&info);
    }
    return 0;
}

uint8_t TRM_SatelliteKeepTxBeam(uint32_t userId)
{
    uint32_t gsAddr;

    if (g_satCtx.role != TRM_NODE_ROLE_SAT_PAYLOAD || userId == 0) {
        return 0;
    }

    return trm_sat_route_get(userId, &gsAddr) == TRM_OK ? 1 : 0;
}

uint8_t TRM_SatelliteIsGroundStationTx(void)
{
    return g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION ? 1 : 0;
}

int TRM_SatelliteGetTxBeam(uint32_t userId, TRM_BeamInfo* beamInfo)
{
    if (beamInfo == NULL) {
        return TRM_ERR_NO_BEAM;
    }

    if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION) {
        if (userId == g_satCtx.localAddr && g_satCtx.satelliteBeamValid) {
            *beamInfo = g_satCtx.satelliteBeam;
            return TRM_OK;
        }
        return TRM_ERR_NO_BEAM;
    }

    if (g_satCtx.role != TRM_NODE_ROLE_SAT_PAYLOAD) {
        return TRM_ERR_NO_BEAM;
    }

    for (uint32_t i = 0; i < g_satCtx.gsBeamMax; i++) {
        if (g_satCtx.gsBeams[i].valid && g_satCtx.gsBeams[i].addr == userId) {
            uint32_t now = trm_sat_now_ms();
            if (g_satCtx.gsBeamTimeoutMs > 0 &&
                (now - g_satCtx.gsBeams[i].lastUpdateMs) > g_satCtx.gsBeamTimeoutMs) {
                g_satCtx.gsBeams[i].valid = 0;
                if (g_satCtx.gsBeamCount > 0) {
                    g_satCtx.gsBeamCount--;
                }
                return TRM_ERR_NO_BEAM;
            }
            *beamInfo = g_satCtx.gsBeams[i].beam;
            return TRM_OK;
        }
    }
    return TRM_ERR_NO_BEAM;
}

void TRM_SatelliteAdjustForwardBeam(uint8_t userIndex, uint8_t userCount,
    uint8_t rateMode, TRM_BeamInfo* beamInfo)
{
    RateModeParams rateParams;
    uint8_t effectiveCount;
    uint64_t stepHz128;
    int64_t startOffsetHz128;
    int64_t offsetHz128;

    if (beamInfo == NULL || g_satCtx.role != TRM_NODE_ROLE_SAT_PAYLOAD) {
        return;
    }
    if (rateMode != TK8710_RATE_MODE_6 &&
        rateMode != TK8710_RATE_MODE_7 &&
        rateMode != TK8710_RATE_MODE_8) {
        return;
    }
    if (TK8710GetRateModeParams(rateMode, &rateParams) != TK8710_OK ||
        rateParams.systemBwKHz == 0) {
        return;
    }

    (void)userCount;
    effectiveCount = TRM_SAT_MAX_FORWARD_USERS;
    if (userIndex >= TRM_SAT_MAX_FORWARD_USERS) {
        return;
    }

    stepHz128 = ((uint64_t)rateParams.systemBwKHz * 128ULL) / effectiveCount;
    if (stepHz128 == 0) {
        return;
    }
    startOffsetHz128 = -((int64_t)stepHz128 * (int64_t)(effectiveCount - 1)) / 2;
    offsetHz128 = startOffsetHz128 + ((int64_t)stepHz128 * userIndex);
    uint32_t freqRaw = (uint32_t)((uint64_t)offsetHz128 & 0x03FFFFFFu);
    beamInfo->freq = trm_sat_to_spi_u32(freqRaw);
    TRM_LOG_DEBUG("TRM SAT: forward freq assign idx=%u/%u mode=%u offset=%d freqRaw=0x%08X",
                  userIndex, effectiveCount, rateMode, (int)(offsetHz128 / 128), freqRaw);
}

void TRM_SatelliteAdjustGroundStationTxBeam(uint8_t rateMode, TRM_BeamInfo* beamInfo)
{
    int32_t freqOffset;
    uint32_t freqRaw;

    if (beamInfo == NULL || g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION) {
        return;
    }

    freqOffset = trm_sat_random_join_freq_offset(rateMode);
    freqRaw = trm_sat_freq_offset_to_raw(freqOffset);
    beamInfo->freq = trm_sat_to_spi_u32(freqRaw);

    TRM_LOG_INFO("TRM SAT: ground-station tx freq hop mode=%u offset=%d freqRaw=0x%08X",
                 rateMode, freqOffset, freqRaw);
}

void TRM_SatelliteBeforeRxBeamStore(uint32_t userId, const uint8_t* data, uint16_t len)
{
    int index;

    if (g_satCtx.gsTerminalBeamMax == 0 ||
        !trm_sat_is_ground_station_terminal_data(data, len)) {
        return;
    }
    if (trm_sat_terminal_beam_find(userId) >= 0) {
        return;
    }
    index = trm_sat_terminal_beam_free_index();
    if (index >= 0) {
        return;
    }

    index = trm_sat_terminal_beam_oldest_index();
    TRM_LOG_INFO("TRM SAT: ground-station terminal beam FIFO evict terminal=0x%08X",
                 g_satCtx.terminalBeams[index].terminalAddr);
    (void)TRM_ClearBeamInfo(g_satCtx.terminalBeams[index].terminalAddr);
    g_satCtx.terminalBeams[index].valid = 0;
    if (g_satCtx.gsTerminalBeamCount > 0) {
        g_satCtx.gsTerminalBeamCount--;
    }
}

void TRM_SatelliteAfterRxBeamStore(uint32_t userId, const uint8_t* data, uint16_t len)
{
    int index;

    if (g_satCtx.gsTerminalBeamMax == 0 ||
        !trm_sat_is_ground_station_terminal_data(data, len)) {
        return;
    }

    index = trm_sat_terminal_beam_find(userId);
    if (index < 0) {
        index = trm_sat_terminal_beam_free_index();
        if (index < 0) {
            return;
        }
        g_satCtx.terminalBeams[index].terminalAddr = userId;
        g_satCtx.terminalBeams[index].valid = 1;
        g_satCtx.gsTerminalBeamCount++;
    }
    g_satCtx.terminalBeams[index].lastUpdateMs = trm_sat_now_ms();
}

void TRM_SatelliteProcessRxUser(const TRM_RxUserData* user)
{
    TRM_SatMacInfo info;

    if (!trm_sat_is_enabled() || user == NULL || user->data == NULL || user->dataLen == 0) {
        return;
    }
    if (trm_sat_parse_mac(user->data, user->dataLen, &info) != TRM_OK) {
        return;
    }
    if (info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return;
    }

    TRM_LOG_INFO("TRM SAT: rx role=%d frame=%u dev=%u src=0x%08X",
                 g_satCtx.role, info.frameType, info.devType, info.srcAddr);

    if (g_satCtx.role == TRM_NODE_ROLE_SAT_PAYLOAD) {
        if (trm_sat_is_terminal_uplink_frame(&info)) {
            trm_sat_payload_process_terminal(user, &info);
        } else if (trm_sat_is_ground_station(&info)) {
            trm_sat_payload_process_ground_station(user, &info);
        }
    } else if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION) {
        trm_sat_ground_station_process_rx(user, &info);
    }
}

uint8_t TRM_SatelliteShouldDeliverRxUser(const uint8_t* data, uint16_t len)
{
    TRM_SatMacInfo info;

    if (!trm_sat_is_enabled() || data == NULL || len == 0) {
        return 1;
    }
    if (trm_sat_parse_mac(data, len, &info) != TRM_OK ||
        info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return 1;
    }

    if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION) {
        return trm_sat_is_terminal_uplink_frame(&info) ? 1 : 0;
    }
    return 1;
}

void TRM_SatelliteProcessIrq(const TK8710IrqResult* irqResult)
{
    uint32_t now;

    if (g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION || irqResult == NULL) {
        return;
    }

    now = trm_sat_now_ms();
    if (irqResult->irq_type == TK8710_IRQ_RX_BCN) {
        if (irqResult->rxbcn_status) {
            trm_sat_update_satellite_beam_from_bcn(irqResult);
            if (g_satCtx.bcnOkCount < 0xFF) {
                g_satCtx.bcnOkCount++;
            }
            if (!g_satCtx.gsOnline && !g_satCtx.joinPending &&
                g_satCtx.bcnOkCount >= g_satCtx.bcnThreshold) {
                (void)trm_sat_queue_join_request();
            }
        } else {
            g_satCtx.bcnOkCount = 0;
        }
    }

    if (g_satCtx.joinPending && (now - g_satCtx.lastJoinReqMs) >= TRM_SAT_JOIN_TIMEOUT_MS) {
        (void)trm_sat_queue_join_request();
    }
}

void TRM_SatelliteProcessTxSlot(uint8_t maxUserCount, const TK8710IrqResult* irqResult)
{
    uint32_t now;

    (void)irqResult;

    if (g_satCtx.role == TRM_NODE_ROLE_SAT_PAYLOAD) {
        trm_sat_flush_cache(maxUserCount > TRM_SAT_MAX_FORWARD_USERS ?
                            TRM_SAT_MAX_FORWARD_USERS : maxUserCount);
        return;
    }

    if (g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION) {
        return;
    }

    now = trm_sat_now_ms();
    if (g_satCtx.gsOnline &&
        (now - g_satCtx.lastGsTxMs) >= g_satCtx.joinMaintainMs) {
        (void)trm_sat_queue_join_request();
    }
}

uint8_t TRM_SatelliteLimitTxUserCount(uint8_t maxUserCount)
{
    if (g_satCtx.role == TRM_NODE_ROLE_SAT_PAYLOAD &&
        maxUserCount > TRM_SAT_MAX_FORWARD_USERS) {
        return TRM_SAT_MAX_FORWARD_USERS;
    }
    return maxUserCount;
}

int TRM_SatelliteBeforeTxData(uint32_t userId, const uint8_t* data, uint16_t len)
{
    TRM_SatMacInfo info;

    if (!trm_sat_is_enabled() || g_satCtx.autoForwarding || data == NULL || len == 0) {
        return TRM_OK;
    }
    if (trm_sat_parse_mac(data, len, &info) != TRM_OK ||
        info.nwkMode != TRM_MAC_NWKMODE_SATELLITE) {
        return TRM_OK;
    }

    if (g_satCtx.role == TRM_NODE_ROLE_GROUND_STATION && trm_sat_is_data_frame(&info)) {
        TRM_BeamInfo beam;
        if (TRM_GetBeamInfo(info.srcAddr, &beam) != TRM_OK) {
            g_satCtx.beamMissCount++;
            TRM_LOG_WARN("TRM SAT: ground-station tx beam miss terminal=0x%08X",
                         info.srcAddr);
            return TRM_ERR_NO_BEAM;
        }
        (void)userId;
    }

    return TRM_OK;
}

void TRM_SatelliteAfterTxData(uint32_t userId, int result)
{
    if (g_satCtx.role != TRM_NODE_ROLE_GROUND_STATION) {
        return;
    }
    if (result == TRM_OK) {
        g_satCtx.lastGsTxMs = trm_sat_now_ms();
        TRM_LOG_DEBUG("TRM SAT: ground-station tx time updated user=0x%08X", userId);
    }
}

void TRM_SatelliteUpdateStats(TRM_Stats* stats)
{
    if (stats == NULL) {
        return;
    }
    stats->satelliteCacheCount = g_satCtx.cacheCount;
    stats->satelliteGroundStationBeamCount = g_satCtx.gsBeamCount;
    stats->satelliteRouteCount = g_satCtx.routeCount;
    stats->satelliteBeamMissCount = g_satCtx.beamMissCount;
    stats->groundStationOnline = g_satCtx.gsOnline;
    stats->groundStationBeamCount = g_satCtx.gsTerminalBeamCount;
}
