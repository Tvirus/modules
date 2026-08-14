#ifndef _BQ27220_H_
#define _BQ27220_H_

#include <stdint.h>

typedef struct
{
    uint16_t addr;
    uint16_t size;
    uint16_t value;
} bq27220_reg_t;

extern int bq27220_modify_ram(const bq27220_reg_t *list, unsigned int count);


#endif
