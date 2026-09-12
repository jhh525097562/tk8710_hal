#ifndef TK8710_GW_RF_STATUS_H
#define TK8710_GW_RF_STATUS_H

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Called from process context only; mask bit n denotes a fault on antenna n.
 * The directory must already have an existing parent. */
static inline int GwWriteRfStatus(const char* directory, uint8_t abnormal_mask)
{
    char target[512];
    char temporary[512];
    char bits[9];
    int length;
    int fd;
    int saved_errno;
    int ok;
    FILE* file;

    length = snprintf(target, sizeof(target), "%s/RFstatus.txt", directory);
    if (length < 0 || (size_t)length >= sizeof(target)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    length = snprintf(temporary, sizeof(temporary), "%s/.RFstatus.XXXXXX", directory);
    if (length < 0 || (size_t)length >= sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) return -1;
    if (chmod(directory, 0755) != 0) return -1;
    fd = mkstemp(temporary);
    if (fd < 0) return -1;
    file = fdopen(fd, "w");
    if (file == NULL) {
        saved_errno = errno;
        close(fd);
        unlink(temporary);
        errno = saved_errno;
        return -1;
    }
    for (unsigned int index = 0; index < 8; ++index) {
        bits[index] = (abnormal_mask & (1u << (7u - index))) ? '0' : '1';
    }
    bits[8] = '\0';
    ok = fchmod(fd, 0644) == 0 && fprintf(file, "rfstatus=%s\n", bits) > 0 &&
        fflush(file) == 0;
    saved_errno = errno;
    if (fclose(file) != 0) {
        ok = 0;
        saved_errno = errno;
    }
    if (ok && rename(temporary, target) == 0) return 0;
    if (ok) saved_errno = errno;
    unlink(temporary);
    errno = saved_errno;
    return -1;
}

#endif
