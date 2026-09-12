#define _DEFAULT_SOURCE
#include "tk8710_pps_api.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_RK3506
#include "tk8710_hal.h"
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#define PPS_COMMAND_MAX 128
#define PPS_RESPONSE_MAX 1024

static int copy_string(char* destination, size_t size, const char* source)
{
    size_t length;

    if (destination == NULL || source == NULL || size == 0) {
        return -1;
    }
    length = strlen(source);
    if (length >= size) {
        return -1;
    }
    memcpy(destination, source, length + 1);
    return 0;
}

static int parse_u32(const char* value, uint32_t* result)
{
    char* end = NULL;
    unsigned long parsed;

    if (value == NULL || result == NULL || value[0] == '\0' || value[0] == '-') {
        return -1;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *result = (uint32_t)parsed;
    return 0;
}

static int parse_bool(const char* value, uint8_t* result)
{
    uint32_t parsed;

    if (parse_u32(value, &parsed) != 0 || parsed > 1) {
        return -1;
    }
    *result = (uint8_t)parsed;
    return 0;
}

static int parse_coordinate(const char* response, const char* key,
    double maximum, char positive_hemisphere, char negative_hemisphere,
    double* coordinate, uint8_t* valid)
{
    const char* position;
    char* end = NULL;
    double parsed;
    char hemisphere;

    if (response == NULL || key == NULL || coordinate == NULL || valid == NULL) {
        return -1;
    }
    position = strstr(response, key);
    if (position == NULL || (position != response && position[-1] != ' ')) {
        return -1;
    }
    position += strlen(key);
    if (strncmp(position, "NA", 2) == 0 &&
        (position[2] == ' ' || position[2] == '\0')) {
        position += 2;
        while (*position == ' ') {
            ++position;
        }
        if (*position != '?') {
            return -1;
        }
        *coordinate = 0.0;
        *valid = 0;
        return 0;
    }
    errno = 0;
    parsed = strtod(position, &end);
    if (errno != 0 || end == position || parsed != parsed ||
        parsed < 0.0 || parsed > maximum) {
        return -1;
    }
    while (*end == ' ') {
        ++end;
    }
    hemisphere = *end;
    if (hemisphere != positive_hemisphere && hemisphere != negative_hemisphere) {
        return -1;
    }
    if (hemisphere == negative_hemisphere) {
        parsed = -parsed;
    }
    *coordinate = parsed;
    *valid = 1;
    return 0;
}

static const char* find_value(const char* response, const char* key,
    char* value, size_t value_size)
{
    size_t key_length = strlen(key);
    const char* position = response;

    while ((position = strstr(position, key)) != NULL) {
        const char* end;
        size_t length;

        if ((position == response || position[-1] == ' ') &&
            position[key_length] == '=') {
            position += key_length + 1;
            end = position;
            while (*end != '\0' && *end != ' ' && *end != '\r' && *end != '\n') {
                ++end;
            }
            length = (size_t)(end - position);
            if (length == 0 || length >= value_size) {
                return NULL;
            }
            memcpy(value, position, length);
            value[length] = '\0';
            return value;
        }
        position += key_length;
    }
    return NULL;
}

static int copy_utc_value(const char* response, char* utc, size_t utc_size)
{
    const char* position = strstr(response, " UTC=");
    size_t length;

    if (position == NULL) {
        return -1;
    }
    position += 5;
    length = strcspn(position, "\r\n");
    if (length == 0 || length >= utc_size) {
        return -1;
    }
    memcpy(utc, position, length);
    utc[length] = '\0';
    return 0;
}

#ifdef PLATFORM_RK3506
static uint64_t monotonic_ms(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
}

static TK8710PpsError open_uart(TK8710PpsContext* context)
{
    struct termios settings;
    int fd;

    fd = open(context->uart_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0 || tcgetattr(fd, &settings) != 0) {
        if (fd >= 0) {
            close(fd);
        }
        return TK8710_PPS_ERROR_OPEN;
    }
    cfmakeraw(&settings);
    cfsetispeed(&settings, B115200);
    cfsetospeed(&settings, B115200);
    settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
    settings.c_cflag |= CS8 | CLOCAL | CREAD;
#ifdef CRTSCTS
    settings.c_cflag &= ~CRTSCTS;
#endif
    settings.c_iflag &= ~(IXON | IXOFF | IXANY);
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &settings) != 0) {
        close(fd);
        return TK8710_PPS_ERROR_OPEN;
    }
    tcflush(fd, TCIOFLUSH);
    context->fd = fd;
    context->initialized = 1;
    return TK8710_PPS_OK;
}

static TK8710PpsError write_all(int fd, const char* data, size_t length)
{
    while (length > 0) {
        ssize_t written = write(fd, data, length);
        if (written > 0) {
            data += written;
            length -= (size_t)written;
        } else if (written < 0 && (errno == EINTR || errno == EAGAIN ||
                errno == EWOULDBLOCK)) {
            usleep(1000);
        } else {
            return TK8710_PPS_ERROR_WRITE;
        }
    }
    return tcdrain(fd) == 0 ? TK8710_PPS_OK : TK8710_PPS_ERROR_WRITE;
}

static TK8710PpsError send_command(TK8710PpsContext* context,
    const char* command, char* response, size_t response_size)
{
    char tx[PPS_COMMAND_MAX];
    char line[PPS_RESPONSE_MAX];
    size_t used = 0;
    int length;
    uint64_t deadline;

    if (context == NULL || !context->initialized) {
        return TK8710_PPS_ERROR_NOT_INITIALIZED;
    }
    length = snprintf(tx, sizeof(tx), "%s\n", command);
    if (length <= 0 || (size_t)length >= sizeof(tx)) {
        return TK8710_PPS_ERROR_OVERFLOW;
    }
    tcflush(context->fd, TCIFLUSH);
    if (write_all(context->fd, tx, (size_t)length) != TK8710_PPS_OK) {
        return TK8710_PPS_ERROR_WRITE;
    }

    deadline = monotonic_ms() + context->config.command_timeout_ms;
    while (monotonic_ms() < deadline) {
        struct pollfd pfd = { .fd = context->fd, .events = POLLIN };
        uint64_t current = monotonic_ms();
        uint64_t remaining;

        if (current >= deadline) {
            break;
        }
        remaining = deadline - current;
        int result = poll(&pfd, 1, (int)(remaining > INT_MAX ? INT_MAX : remaining));

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return TK8710_PPS_ERROR_READ;
        }
        if (result == 0) {
            break;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return TK8710_PPS_ERROR_READ;
        }
        if ((pfd.revents & POLLIN) != 0) {
            char chunk[128];
            ssize_t count = read(context->fd, chunk, sizeof(chunk));
            size_t index;

            if (count < 0 && (errno == EINTR || errno == EAGAIN ||
                    errno == EWOULDBLOCK)) {
                continue;
            }
            if (count <= 0) {
                return TK8710_PPS_ERROR_READ;
            }
            for (index = 0; index < (size_t)count; ++index) {
                char character = chunk[index];
                if (character == '\r' || character == '\n') {
                    if (used == 0) {
                        continue;
                    }
                    line[used] = '\0';
                    if (strncmp(line, "EVENT ", 6) == 0) {
                        copy_string(context->last_event,
                            sizeof(context->last_event), line);
                    } else if (strncmp(line, "OK", 2) == 0 &&
                            (line[2] == '\0' || line[2] == ' ')) {
                        if (copy_string(response, response_size, line) != 0) {
                            return TK8710_PPS_ERROR_OVERFLOW;
                        }
                        return TK8710_PPS_OK;
                    } else if (strncmp(line, "ERR", 3) == 0 &&
                            (line[3] == '\0' || line[3] == ' ')) {
                        copy_string(response, response_size, line);
                        return TK8710_PPS_ERROR_PROTOCOL;
                    }
                    used = 0;
                } else if (used + 1 >= sizeof(line)) {
                    return TK8710_PPS_ERROR_OVERFLOW;
                } else {
                    line[used++] = character;
                }
            }
        }
    }
    return TK8710_PPS_ERROR_TIMEOUT;
}
#else
static TK8710PpsError send_command(TK8710PpsContext* context,
    const char* command, char* response, size_t response_size)
{
    (void)context;
    (void)command;
    (void)response;
    (void)response_size;
    return TK8710_PPS_ERROR_UNSUPPORTED;
}
#endif

TK8710PpsError TK8710PpsGetDefaultConfig(TK8710PpsConfig* config)
{
    if (config == NULL) {
        return TK8710_PPS_ERROR_PARAM;
    }
    config->uart_device = "/dev/ttyS4";
    config->baud_rate = 115200;
    config->command_timeout_ms = 2000;
    config->reset_gpio_chip = "gpiochip1";
    config->reset_gpio_line = 19;
    config->reset_low_ms = 100;
    config->reset_boot_wait_ms = 2000;
    return TK8710_PPS_OK;
}

TK8710PpsError TK8710PpsInit(TK8710PpsContext* context,
    const TK8710PpsConfig* config)
{
    if (context == NULL || config == NULL || config->uart_device == NULL ||
        config->reset_gpio_chip == NULL || config->baud_rate != 115200 ||
        config->command_timeout_ms == 0) {
        return TK8710_PPS_ERROR_PARAM;
    }
    memset(context, 0, sizeof(*context));
    context->fd = -1;
    if (copy_string(context->uart_device, sizeof(context->uart_device),
            config->uart_device) != 0 ||
        copy_string(context->reset_gpio_chip, sizeof(context->reset_gpio_chip),
            config->reset_gpio_chip) != 0) {
        return TK8710_PPS_ERROR_PARAM;
    }
    context->config = *config;
    context->config.uart_device = context->uart_device;
    context->config.reset_gpio_chip = context->reset_gpio_chip;
#ifdef PLATFORM_RK3506
    return open_uart(context);
#else
    return TK8710_PPS_ERROR_UNSUPPORTED;
#endif
}

void TK8710PpsClose(TK8710PpsContext* context)
{
    if (context == NULL) {
        return;
    }
#ifdef PLATFORM_RK3506
    if (context->fd >= 0) {
        close(context->fd);
    }
#endif
    context->fd = -1;
    context->initialized = 0;
}

TK8710PpsError TK8710PpsSetOutput(TK8710PpsContext* context, uint8_t enable)
{
    char response[PPS_RESPONSE_MAX];
    char value[16];
    uint8_t actual;
    TK8710PpsError error;

    if (enable > 1) {
        return TK8710_PPS_ERROR_PARAM;
    }
    error = send_command(context, enable ? "OUT ON" : "OUT OFF",
        response, sizeof(response));
    if (error != TK8710_PPS_OK) {
        return error;
    }
    if (find_value(response, "EN", value, sizeof(value)) == NULL ||
        parse_bool(value, &actual) != 0 || actual != enable) {
        return TK8710_PPS_ERROR_PARSE;
    }
    return TK8710_PPS_OK;
}

TK8710PpsError TK8710PpsGetVersion(TK8710PpsContext* context,
    char* version, uint32_t version_size)
{
    if (version == NULL || version_size == 0) {
        return TK8710_PPS_ERROR_PARAM;
    }
    version[0] = '\0';
    return send_command(context, "GET VERSION", version, version_size);
}

TK8710PpsError TK8710PpsSetPeriod(TK8710PpsContext* context, uint32_t period)
{
    char command[PPS_COMMAND_MAX];
    char response[PPS_RESPONSE_MAX];
    char value[16];
    uint32_t actual;
    TK8710PpsError error;

    if (period == 0) {
        return TK8710_PPS_ERROR_PARAM;
    }
    snprintf(command, sizeof(command), "SET PERIOD %u", period);
    error = send_command(context, command, response, sizeof(response));
    if (error != TK8710_PPS_OK) {
        return error;
    }
    if (find_value(response, "PERIOD", value, sizeof(value)) == NULL ||
        parse_u32(value, &actual) != 0 || actual != period) {
        return TK8710_PPS_ERROR_PARSE;
    }
    return TK8710_PPS_OK;
}

TK8710PpsError TK8710PpsGetPeriod(TK8710PpsContext* context,
    TK8710PpsPeriodInfo* info)
{
    char response[PPS_RESPONSE_MAX];
    char value[32];
    TK8710PpsError error;

    if (info == NULL) {
        return TK8710_PPS_ERROR_PARAM;
    }
    memset(info, 0, sizeof(*info));
    error = send_command(context, "GET PERIOD", response, sizeof(response));
    if (error != TK8710_PPS_OK) {
        return error;
    }
#define PARSE_PERIOD_FIELD(key, field, parser) \
    if (find_value(response, key, value, sizeof(value)) == NULL || \
        parser(value, &info->field) != 0) return TK8710_PPS_ERROR_PARSE
    PARSE_PERIOD_FIELD("PERIOD", period, parse_u32);
    PARSE_PERIOD_FIELD("PENDING", pending, parse_bool);
    if (find_value(response, "PPAUSE", value, sizeof(value)) != NULL &&
        parse_bool(value, &info->pause) != 0) return TK8710_PPS_ERROR_PARSE;
    if (find_value(response, "PPERIOD", value, sizeof(value)) != NULL &&
        parse_u32(value, &info->pending_period) != 0) return TK8710_PPS_ERROR_PARSE;
    if (find_value(response, "PCONF", value, sizeof(value)) != NULL &&
        parse_bool(value, &info->confirm_pending) != 0) return TK8710_PPS_ERROR_PARSE;
#undef PARSE_PERIOD_FIELD
    return TK8710_PPS_OK;
}

TK8710PpsError TK8710PpsGetStatus(TK8710PpsContext* context,
    TK8710PpsStatus* status)
{
    char response[PPS_RESPONSE_MAX];
    char value[TK8710_PPS_UTC_MAX];
    TK8710PpsError error;

    if (status == NULL) return TK8710_PPS_ERROR_PARAM;
    memset(status, 0, sizeof(*status));
    error = send_command(context, "GET STATUS", response, sizeof(response));
    if (error != TK8710_PPS_OK) return error;
#define GET_STR(key, field) \
    if (find_value(response, key, value, sizeof(value)) == NULL || \
        copy_string(status->field, sizeof(status->field), value) != 0) \
        return TK8710_PPS_ERROR_PARSE
#define GET_U32(key, field) \
    if (find_value(response, key, value, sizeof(value)) == NULL || \
        parse_u32(value, &status->field) != 0) return TK8710_PPS_ERROR_PARSE
#define GET_BOOL(key, field) \
    if (find_value(response, key, value, sizeof(value)) == NULL || \
        parse_bool(value, &status->field) != 0) return TK8710_PPS_ERROR_PARSE
    GET_STR("STATE", state);
    GET_U32("PERIOD", period);
    GET_U32("WIDTH", width_ms);
    GET_BOOL("OUT", output_enabled);
    GET_BOOL("PENDING", pending);
    GET_BOOL("GPS", gps_online);
    GET_BOOL("PPS", pps_seen);
    GET_BOOL("FIX", fix_valid);
    if (find_value(response, "RMC", value, sizeof(value)) == NULL ||
        (strcmp(value, "A") != 0 && strcmp(value, "V") != 0))
        return TK8710_PPS_ERROR_PARSE;
    status->rmc_status = value[0];
    GET_BOOL("ALIGN", aligned);
    GET_BOOL("HOLD", holdover);
    GET_BOOL("BAD", bad_period);
    GET_BOOL("RESYNC", resync_pending);
    GET_U32("SATS", satellites);
    GET_U32("SNR", snr);
    if (copy_utc_value(response, status->utc, sizeof(status->utc)) != 0)
        return TK8710_PPS_ERROR_PARSE;
#undef GET_STR
#undef GET_U32
#undef GET_BOOL
    return TK8710_PPS_OK;
}

TK8710PpsError TK8710PpsGetPosition(TK8710PpsContext* context,
    TK8710PpsPosition* position)
{
    char response[PPS_RESPONSE_MAX];
    uint8_t latitude_valid;
    uint8_t longitude_valid;
    TK8710PpsError error;

    if (position == NULL) {
        return TK8710_PPS_ERROR_PARAM;
    }
    memset(position, 0, sizeof(*position));
    error = send_command(context, "GET POS", response, sizeof(response));
    if (error != TK8710_PPS_OK) {
        return error;
    }
    if (strncmp(response, "OK POS ", 7) != 0 ||
        parse_coordinate(response, "LAT=", 90.0, 'N', 'S', &position->latitude,
            &latitude_valid) != 0 ||
        parse_coordinate(response, "LON=", 180.0, 'E', 'W', &position->longitude,
            &longitude_valid) != 0) {
        return TK8710_PPS_ERROR_PARSE;
    }
    if (latitude_valid != longitude_valid) {
        memset(position, 0, sizeof(*position));
        return TK8710_PPS_ERROR_PARSE;
    }
    position->valid = latitude_valid;
    return TK8710_PPS_OK;
}

TK8710PpsHealth TK8710PpsEvaluateHealth(const TK8710PpsStatus* status,
    uint32_t expected_period)
{
    if (status == NULL || expected_period == 0) return TK8710_PPS_UNHEALTHY;
    if (status->bad_period || strcmp(status->state, "ERROR") == 0)
        return TK8710_PPS_UNHEALTHY;
    if (status->output_enabled && status->aligned && status->holdover)
        return TK8710_PPS_HOLDOVER;
    if (strcmp(status->state, "RUNNING") == 0 &&
        status->period == expected_period && status->output_enabled &&
        !status->pending && status->gps_online && status->pps_seen &&
        status->fix_valid && status->rmc_status == 'A' && status->aligned &&
        !status->holdover && !status->resync_pending) return TK8710_PPS_HEALTHY;
    if (status->output_enabled && !status->bad_period &&
        (status->pending || strcmp(status->state, "WAIT_GPS") == 0 ||
         strcmp(status->state, "WAIT_TARGET") == 0 ||
         strcmp(status->state, "WAIT_PPS") == 0)) return TK8710_PPS_WAITING;
    return TK8710_PPS_UNHEALTHY;
}

TK8710PpsError TK8710PpsGetDiagnostics(TK8710PpsContext* context,
    TK8710PpsDiagnostics* diagnostics)
{
    static const char* commands[] = {
        "GET PPS", "GET OUT", "GET SYNC", "GET GPS", "GET RMC DIAG"
    };
    char* outputs[5];
    TK8710PpsError first_error = TK8710_PPS_OK;
    size_t index;

    if (diagnostics == NULL) return TK8710_PPS_ERROR_PARAM;
    memset(diagnostics, 0, sizeof(*diagnostics));
    outputs[0] = diagnostics->pps;
    outputs[1] = diagnostics->output;
    outputs[2] = diagnostics->sync;
    outputs[3] = diagnostics->gps;
    outputs[4] = diagnostics->rmc;
    for (index = 0; index < 5; ++index) {
        TK8710PpsError error = send_command(context, commands[index], outputs[index],
            TK8710_PPS_DIAG_TEXT_MAX);
        if (error == TK8710_PPS_OK) diagnostics->success_mask |= (1U << index);
        else if (first_error == TK8710_PPS_OK) first_error = error;
    }
    return diagnostics->success_mask != 0 ? TK8710_PPS_OK : first_error;
}

TK8710PpsError TK8710PpsResetDevice(TK8710PpsContext* context)
{
    if (context == NULL || context->uart_device[0] == '\0')
        return TK8710_PPS_ERROR_PARAM;
#ifdef PLATFORM_RK3506
    TK8710PpsClose(context);
    if (TK8710GpioSet(context->reset_gpio_chip,
            context->config.reset_gpio_line, 0) != 0) return TK8710_PPS_ERROR_GPIO;
    usleep(context->config.reset_low_ms * 1000U);
    if (TK8710GpioSet(context->reset_gpio_chip,
            context->config.reset_gpio_line, 1) != 0) return TK8710_PPS_ERROR_GPIO;
    usleep(context->config.reset_boot_wait_ms * 1000U);
    return open_uart(context);
#else
    return TK8710_PPS_ERROR_UNSUPPORTED;
#endif
}

const char* TK8710PpsGetLastEvent(const TK8710PpsContext* context)
{
    return context == NULL ? "" : context->last_event;
}
