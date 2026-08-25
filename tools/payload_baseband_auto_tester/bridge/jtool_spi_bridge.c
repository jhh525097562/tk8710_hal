#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BOOL_T;
typedef enum { DEV_ALL = -1, DEV_I2C = 0, DEV_IO, DEV_SPI } DevType;
typedef enum { ERR_NONE = 0 } ErrorType;
typedef enum { LOW_1EDG = 0, LOW_2EDG = 1, HIGH_1EDG = 2, HIGH_2EDG = 3 } SpiMode;
typedef enum { ENDIAN_MSB = 0, ENDIAN_LSB = 1 } SpiFirstBit;

typedef char *(*DevicesScanFn)(int, int *);
typedef void *(*DevOpenFn)(int, char *, int);
typedef BOOL_T (*DevCloseFn)(void *);
typedef ErrorType (*SpiWriteReadFn)(void *, SpiMode, SpiFirstBit, uint32_t,
                                    uint8_t *, uint8_t *);
typedef ErrorType (*SetU8Fn)(void *, uint8_t);

static HMODULE g_dll;
static DevicesScanFn g_scan;
static DevOpenFn g_open;
static DevCloseFn g_close;
static SpiWriteReadFn g_xfer;
static SetU8Fn g_vio;
static SetU8Fn g_speed;
static void *g_spi1;
static void *g_spi2;

#define DT_FRAME_LEN 512U
#define DT_SYNC 0x1ACFFC1DUL

static void json_string(const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    putchar('"');
    while (*p) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20U) printf("\\u%04x", *p);
            else putchar(*p);
            break;
        }
        ++p;
    }
    putchar('"');
}

static void emit_error(long id, const char *message)
{
    printf("{\"id\":%ld,\"ok\":false,\"error\":", id);
    json_string(message);
    puts("}");
    fflush(stdout);
}

static void emit_ok(long id, const char *extra)
{
    printf("{\"id\":%ld,\"ok\":true", id);
    if (extra && *extra) printf(",%s", extra);
    puts("}");
    fflush(stdout);
}

static int json_long(const char *line, const char *key, long *value)
{
    char needle[64];
    const char *p;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(line, needle);
    if (!p) return -1;
    p = strchr(p + strlen(needle), ':');
    if (!p) return -1;
    *value = strtol(p + 1, NULL, 0);
    return 0;
}

static int json_text(const char *line, const char *key, char *out, size_t cap)
{
    char needle[64];
    const char *p;
    size_t used = 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(line, needle);
    if (!p) return -1;
    p = strchr(p + strlen(needle), ':');
    if (!p) return -1;
    while (*++p && isspace((unsigned char)*p)) {}
    if (*p != '"') return -1;
    ++p;
    while (*p && *p != '"' && used + 1 < cap) {
        if (*p == '\\') {
            ++p;
            if (!*p) break;
            if (*p == 'n') out[used++] = '\n';
            else if (*p == 'r') out[used++] = '\r';
            else if (*p == 't') out[used++] = '\t';
            else out[used++] = *p;
        } else out[used++] = *p;
        ++p;
    }
    out[used] = '\0';
    return (*p == '"') ? 0 : -1;
}

static int hex_value(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int parse_hex(const char *text, uint8_t *out, size_t cap, size_t *length)
{
    int high = -1;
    size_t used = 0;
    while (*text) {
        int value = hex_value((unsigned char)*text++);
        if (value < 0) {
            if (isspace((unsigned char)text[-1]) || text[-1] == ':' || text[-1] == '-') continue;
            return -1;
        }
        if (high < 0) high = value;
        else {
            if (used >= cap) return -1;
            out[used++] = (uint8_t)((high << 4) | value);
            high = -1;
        }
    }
    if (high >= 0) return -1;
    *length = used;
    return 0;
}

static void print_hex(const uint8_t *data, size_t length)
{
    size_t i;
    putchar('"');
    for (i = 0; i < length; ++i) printf("%02X", data[i]);
    putchar('"');
}

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8U) | p[1]);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | p[3];
}

static uint16_t dt_checksum(const uint8_t *frame)
{
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i < DT_FRAME_LEN - 2U; i += 2U)
        sum += ((uint16_t)frame[i] << 8U) | frame[i + 1U];
    return (uint16_t)sum;
}

static int valid_dt(const uint8_t *frame)
{
    return be32(frame) == DT_SYNC && be16(frame + 4) <= 502U &&
           dt_checksum(frame) == be16(frame + DT_FRAME_LEN - 2U);
}

static int load_api(void)
{
    g_dll = LoadLibraryA("jtool.dll");
    if (!g_dll) return -1;
    g_scan = (DevicesScanFn)GetProcAddress(g_dll, "DevicesScan");
    g_open = (DevOpenFn)GetProcAddress(g_dll, "DevOpen");
    g_close = (DevCloseFn)GetProcAddress(g_dll, "DevClose");
    g_xfer = (SpiWriteReadFn)GetProcAddress(g_dll, "SPIWriteRead");
    g_vio = (SetU8Fn)GetProcAddress(g_dll, "JSPISetVio");
    g_speed = (SetU8Fn)GetProcAddress(g_dll, "JSPISetSpeed");
    return g_scan && g_open && g_close && g_xfer && g_vio && g_speed ? 0 : -1;
}

static void close_devices(void)
{
    if (g_spi1) { g_close(g_spi1); g_spi1 = NULL; }
    if (g_spi2) { g_close(g_spi2); g_spi2 = NULL; }
}

static void command_scan(long id)
{
    int count = 0;
    char *raw = g_scan(DEV_SPI, &count);
    printf("{\"id\":%ld,\"ok\":true,\"count\":%d,\"raw\":", id, count);
    json_string(raw);
    puts("}");
    fflush(stdout);
}

static void command_open(long id, const char *line)
{
    char spi1[128], spi2[128];
    if (json_text(line, "spi1_sn", spi1, sizeof(spi1)) != 0 ||
        json_text(line, "spi2_sn", spi2, sizeof(spi2)) != 0) {
        emit_error(id, "spi1_sn/spi2_sn required");
        return;
    }
    close_devices();
    g_spi1 = g_open(DEV_SPI, spi1, 0);
    g_spi2 = g_open(DEV_SPI, spi2, 0);
    if (!g_spi1 || !g_spi2) {
        close_devices();
        emit_error(id, "failed to open both SPI devices by SN");
        return;
    }
    (void)g_vio(g_spi1, 33U); (void)g_speed(g_spi1, 0U);
    (void)g_vio(g_spi2, 33U); (void)g_speed(g_spi2, 0U);
    emit_ok(id, "\"spi1_mode\":0,\"spi2_mode\":2");
}

static void command_spi1(long id, const char *line)
{
    char hex[4096];
    uint8_t tx[512], rx[512];
    size_t length = 0;
    if (!g_spi1) { emit_error(id, "SPI1 not open"); return; }
    if (json_text(line, "tx_hex", hex, sizeof(hex)) != 0 ||
        parse_hex(hex, tx, sizeof(tx), &length) != 0 || length == 0U) {
        emit_error(id, "invalid tx_hex"); return;
    }
    memset(rx, 0, length);
    if (g_xfer(g_spi1, LOW_1EDG, ENDIAN_MSB, (uint32_t)length, tx, rx) != ERR_NONE) {
        emit_error(id, "SPI1 transfer failed"); return;
    }
    printf("{\"id\":%ld,\"ok\":true,\"rx_hex\":", id);
    print_hex(rx, length);
    puts("}"); fflush(stdout);
}

static void emit_dt_frame(long id, unsigned index, const uint8_t *frame)
{
    printf("{\"event\":\"dt_frame\",\"request_id\":%ld,\"index\":%u,"
           "\"seq\":%u,\"length\":%u,\"frame_hex\":",
           id, index, be16(frame + 6), be16(frame + 4));
    print_hex(frame, DT_FRAME_LEN);
    puts("}"); fflush(stdout);
}

static void command_spi2_capture(long id, const char *line)
{
    long max_frames = 4096, idle_ms = 2000, arm_ms = 100;
    uint8_t frame[DT_FRAME_LEN];
    uint8_t tx[DT_FRAME_LEN], rx[DT_FRAME_LEN];
    uint32_t sync = 0;
    size_t offset = 0;
    size_t byte_index;
    unsigned frames = 0, bad = 0;
    int final_frame = 0;
    DWORD last_data;
    (void)json_long(line, "max_frames", &max_frames);
    (void)json_long(line, "idle_timeout_ms", &idle_ms);
    (void)json_long(line, "arm_delay_ms", &arm_ms);
    if (!g_spi2) { emit_error(id, "SPI2 not open"); return; }
    if (max_frames < 1) max_frames = 1;
    if (idle_ms < 100) idle_ms = 100;
    memset(tx, 0xFF, sizeof(tx));
    Sleep((DWORD)arm_ms);
    last_data = GetTickCount();
    while (frames < (unsigned)max_frames &&
           GetTickCount() - last_data < (DWORD)idle_ms && final_frame == 0) {
        if (g_xfer(g_spi2, HIGH_1EDG, ENDIAN_MSB, DT_FRAME_LEN, tx, rx) != ERR_NONE) {
            emit_error(id, "SPI2 512-byte transfer failed"); return;
        }
        for (byte_index = 0U; byte_index < DT_FRAME_LEN; byte_index++) {
            uint8_t value = rx[byte_index];
            if (offset == 0U) {
                sync = (sync << 8U) | value;
                if (sync == DT_SYNC) {
                    frame[0] = 0x1A; frame[1] = 0xCF; frame[2] = 0xFC; frame[3] = 0x1D;
                    offset = 4U;
                    last_data = GetTickCount();
                }
                continue;
            }
            frame[offset++] = value;
            if (offset == DT_FRAME_LEN) {
                if (valid_dt(frame)) {
                    emit_dt_frame(id, frames++, frame);
                    if (be16(frame + 4) < 502U) final_frame = 1;
                } else {
                    ++bad;
                }
                offset = 0U;
                sync = 0U;
                last_data = GetTickCount();
                if (final_frame != 0) break;
            }
        }
        Sleep(100U);
    }
    printf("{\"id\":%ld,\"ok\":true,\"frames\":%u,\"bad_frames\":%u,"
           "\"partial_bytes\":%u}\n", id, frames, bad, (unsigned)offset);
    fflush(stdout);
}

static int selftest(void)
{
    static const uint8_t expected[] = {0x1A, 0xCF, 0xFC, 0x1D};
    uint8_t bytes[8];
    size_t length = 0;
    char text[32];
    long value = 0;
    if (parse_hex("1a cf fc 1d", bytes, sizeof(bytes), &length) != 0 ||
        length != 4U || memcmp(bytes, expected, 4U) != 0) return 1;
    if (json_text("{\"op\":\"open\",\"spi1_sn\":\"ABC\"}", "spi1_sn",
                  text, sizeof(text)) != 0 || strcmp(text, "ABC") != 0) return 1;
    if (json_long("{\"id\":17}", "id", &value) != 0 || value != 17) return 1;
    puts("SELFTEST PASS");
    return 0;
}

int main(int argc, char **argv)
{
    char line[16384];
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return selftest();
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (load_api() != 0) {
        fputs("{\"event\":\"fatal\",\"error\":\"cannot load jtool.dll API\"}\n", stdout);
        return 2;
    }
    puts("{\"event\":\"ready\",\"protocol\":1}");
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        char op[64] = "";
        long id = 0;
        (void)json_long(line, "id", &id);
        if (json_text(line, "op", op, sizeof(op)) != 0) { emit_error(id, "op required"); continue; }
        if (strcmp(op, "scan") == 0) command_scan(id);
        else if (strcmp(op, "open") == 0) command_open(id, line);
        else if (strcmp(op, "spi1") == 0) command_spi1(id, line);
        else if (strcmp(op, "spi2_capture") == 0) command_spi2_capture(id, line);
        else if (strcmp(op, "close") == 0) { close_devices(); emit_ok(id, NULL); }
        else if (strcmp(op, "quit") == 0) { close_devices(); emit_ok(id, NULL); break; }
        else emit_error(id, "unknown op");
    }
    close_devices();
    if (g_dll) FreeLibrary(g_dll);
    return 0;
}
