#ifndef LOG_EXPORT_H
#define LOG_EXPORT_H
#include "mongoose.h"
/* Called only after the existing Web authentication check. */
void log_export_list(struct mg_connection *, struct mg_http_message *);
void log_export_start(struct mg_connection *, struct mg_http_message *);
void log_export_status(struct mg_connection *, struct mg_http_message *);
void log_export_download(struct mg_connection *, struct mg_http_message *);
#endif
