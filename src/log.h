#ifndef _LOG_H_
#define _LOG_H_

#define LOG_BUFFER_ENABLE

extern int log_init(void);
extern void log_printf(const char *fmt, ...);

#endif
