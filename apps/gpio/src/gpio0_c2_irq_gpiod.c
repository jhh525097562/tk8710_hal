#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *chip_path = "/dev/gpiochip0";
    unsigned int line_offset = 18;
    const char *consumer = "gpio0_c2_irq_listener";
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    struct gpiod_line_event event;
    int ret;

    printf("libgpiod version: %s\n", gpiod_version_string());

    chip = gpiod_chip_open(chip_path);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    line = gpiod_chip_get_line(chip, line_offset);
    if (!line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return 1;
    }

    ret = gpiod_line_request_rising_edge_events(line, consumer);
    if (ret < 0) {
        perror("gpiod_line_request_rising_edge_events");
        gpiod_chip_close(chip);
        return 1;
    }

    for (;;) {
        ret = gpiod_line_event_wait(line, NULL);
        if (ret < 0) {
            perror("gpiod_line_event_wait");
            break;
        }
        if (ret == 0) {
            continue;
        }

        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("gpiod_line_event_read");
            break;
        }

        printf("event: %ld.%09ld type=%d\n",
               event.ts.tv_sec, event.ts.tv_nsec, event.event_type);
        fflush(stdout);
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}
