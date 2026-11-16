#ifndef _NORLOG_H_
#define _NORLOG_H_

#include <stdint.h>

int nlog_init(void);
int nlog_write(unsigned int log_id, const unsigned char *data, unsigned int len);
uint32_t nlog_get_len(unsigned int log_id);
int nlog_read(unsigned int log_id, uint32_t offset, unsigned char *buf, unsigned int len);


#endif
