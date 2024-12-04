#ifndef _FLASH_H_
#define _FLASH_H_


#define OTP_ADDR  ((unsigned int)0x1FFF7000)
#define OTP_SIZE  ((unsigned int)0x400)

extern int flash_erase(unsigned int addr, unsigned int len);
extern int flash_write(unsigned int addr, const unsigned char *data, unsigned int len);
extern int flash_read(unsigned int addr, unsigned char *buf, unsigned int len);
extern int flash_write_otp(const unsigned char *data, unsigned int len);
extern int flash_read_otp(unsigned char *buf, unsigned int len);
extern int flash_program(unsigned int addr, const unsigned char *data, unsigned int len);


#endif
