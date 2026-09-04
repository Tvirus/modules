#ifndef _N32_FLASH_H_
#define _N32_FLASH_H_


int flash_erase(unsigned int addr, unsigned int len);
int flash_write(unsigned int addr, const void *data, unsigned int len);
int flash_program(unsigned int addr, const void *data, unsigned int len);
void flash_switch_bank(void);


#endif
