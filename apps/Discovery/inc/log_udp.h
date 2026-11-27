#ifndef LOG_UDP_H
#define LOG_UDP_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_OK 0
#define LOG_ERR_NETWORK -1
#define LOG_ERR_TOO_LONG -2
#define LOG_ERR_PARAM -3

int log_send(int level, const char *content, time_t timestamp);
int log_flush(void);
void log_close(void);

#ifdef __cplusplus
}
#endif

#endif
