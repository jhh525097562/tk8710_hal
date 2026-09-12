#define _POSIX_C_SOURCE 200809L
#include "tk8710_gw_rf_status.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    char directory[] = "/tmp/tk8710-rf-status-XXXXXX";
    char target[512];
    char actual[64];
    const uint8_t masks[] = {0, 1, 128, 5, 255, 0};
    const char* expected[] = {"11111111", "11111110", "01111111",
                             "11111010", "00000000", "11111111"};
    struct stat info;

    assert(mkdtemp(directory) != NULL);
    snprintf(target, sizeof(target), "%s/RFstatus.txt", directory);
    umask(0077);
    for (unsigned int index = 0; index < sizeof(masks); ++index) {
        FILE* file;
        char line[64];
        assert(GwWriteRfStatus(directory, masks[index]) == 0);
        file = fopen(target, "r");
        assert(file != NULL);
        assert(fgets(actual, sizeof(actual), file) != NULL);
        snprintf(line, sizeof(line), "rfstatus=%s\n", expected[index]);
        assert(strcmp(actual, line) == 0);
        assert(fgetc(file) == EOF);
        assert(fclose(file) == 0);
        assert(stat(target, &info) == 0 && (info.st_mode & 0777) == 0644);
    }
    /* Failed replacement must preserve the last complete snapshot. */
    assert(GwWriteRfStatus(target, 255) == -1);
    assert(stat(target, &info) == 0 && info.st_size == 18);
    assert(unlink(target) == 0);
    assert(rmdir(directory) == 0); /* No leaked temporary files. */
    puts("RF status file tests passed");
    return 0;
}
