/**
 * @file driver_test_platform.h
 * @brief Platform glue for DriverTest programs.
 */

#ifndef DRIVER_TEST_PLATFORM_H
#define DRIVER_TEST_PLATFORM_H

#include <errno.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <conio.h>
#include <locale.h>
#include <string.h>

#define strcasecmp _stricmp

typedef struct driver_test_timeval {
    long tv_sec;
    long tv_usec;
} driver_test_timeval;

#define timeval driver_test_timeval

static int __attribute__((unused)) driver_test_gettimeofday(driver_test_timeval* tv, void* tz)
{
    FILETIME ft;
    ULARGE_INTEGER value;
    uint64_t us;

    (void)tz;

    if (tv == NULL) {
        return -1;
    }

    GetSystemTimeAsFileTime(&ft);
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    us = value.QuadPart / 10ULL - 11644473600000000ULL;

    tv->tv_sec = (long)(us / 1000000ULL);
    tv->tv_usec = (long)(us % 1000000ULL);
    return 0;
}

#define gettimeofday driver_test_gettimeofday

static void __attribute__((unused)) driver_test_usleep(unsigned int usec)
{
    Sleep((DWORD)((usec + 999U) / 1000U));
}

#define usleep driver_test_usleep

#else

#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

#endif /* _WIN32 */

#endif /* DRIVER_TEST_PLATFORM_H */
