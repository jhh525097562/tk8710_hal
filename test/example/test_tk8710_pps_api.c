#include "tk8710_pps_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static TK8710PpsStatus healthy_status(void)
{
    TK8710PpsStatus status;

    memset(&status, 0, sizeof(status));
    strcpy(status.state, "RUNNING");
    status.period = 10;
    status.output_enabled = 1;
    status.gps_online = 1;
    status.pps_seen = 1;
    status.fix_valid = 1;
    status.rmc_status = 'A';
    status.aligned = 1;
    return status;
}

int main(void)
{
    TK8710PpsConfig config;
    TK8710PpsStatus status = healthy_status();

    assert(TK8710PpsGetDefaultConfig(&config) == TK8710_PPS_OK);
    assert(strcmp(config.uart_device, "/dev/ttyS4") == 0);
    assert(strcmp(config.reset_gpio_chip, "gpiochip1") == 0);
    assert(config.reset_gpio_line == 19);
    assert(config.reset_low_ms == 100);
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_HEALTHY);

    status.pending = 1;
    strcpy(status.state, "WAIT_PPS");
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_WAITING);
    status.pending = 0;
    strcpy(status.state, "RUNNING");
    status.holdover = 1;
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_HOLDOVER);
    status.holdover = 0;
    status.bad_period = 1;
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_UNHEALTHY);
    status.bad_period = 0;
    strcpy(status.state, "ERROR");
    assert(TK8710PpsEvaluateHealth(&status, 10) == TK8710_PPS_UNHEALTHY);
    assert(TK8710PpsEvaluateHealth(NULL, 10) == TK8710_PPS_UNHEALTHY);

    puts("TK8710 PPS API tests passed");
    return 0;
}
