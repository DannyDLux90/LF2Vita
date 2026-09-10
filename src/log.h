#ifndef LF2_LOG_H
#define LF2_LOG_H
#include <stddef.h>
void lf2_log_init(void);
void lf2_log_shutdown(int clean);
void lf2_logf(const char *level, const char *fmt, ...);
void lf2_log_stage(const char *stage, const char *fmt, ...);
void lf2_log_memory(const char *tag);
void lf2_log_install_signal_handlers(void);
const char *lf2_log_path(void);
#endif
