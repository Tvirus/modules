#ifndef _LOG_H_
#define _LOG_H_


#define ENABLE_LOG_BUFFER

extern int log_init(void);
extern void log_printf(const char *fmt, ...);

#endif
