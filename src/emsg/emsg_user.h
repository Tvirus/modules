#ifndef _EMSG_USER_H_
#define _EMSG_USER_H_

#include <stdint.h>


extern uint8_t uart3_conn_id;

extern int emsg_user_init(void);
extern void emsg_user_task(void);

extern uint32_t uart3_sender(const uint8_t *data, uint32_t len);
extern void uart3_irq_handler(void);


#endif
