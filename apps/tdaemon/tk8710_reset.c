#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <gpiod.h>
#include <stdio.h>
#include <time.h>

/* RK3506 only. Matches TK8710HardwareResetPulse: gpiochip0/13, low 10ms, high.
 * Keep the line requested for the entire pulse (two gpioset invocations do not).
 * Caller must stop tk8710_gw and its supervisor before invoking this utility.
 */
int main(void)
{
    struct gpiod_chip *chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) { perror("TK8710 reset: open gpiochip0"); return 1; }
    struct gpiod_line *line = gpiod_chip_get_line(chip, 13);
    if (!line || gpiod_line_request_output(line, "tk8710-stop-reset", 0)) {
        perror("TK8710 reset: request GPIO0_B5 low");
        gpiod_chip_close(chip);
        return 1;
    }
    int failed = 0;
    struct timespec delay = {0, 10000000};
    while (nanosleep(&delay, &delay)) {
        if (errno != EINTR) { perror("TK8710 reset: delay"); failed = 1; break; }
    }
    /* Attempt to deassert reset even if the delay failed. */
    if (gpiod_line_set_value(line, 1)) {
        perror("TK8710 reset: set GPIO0_B5 high");
        failed = 1;
    }
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    if (!failed) puts("TK8710 hardware reset: gpiochip0 line 13, low >=10ms then high");
    return failed;
}
