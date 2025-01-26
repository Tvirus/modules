#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>
#include <stddef.h>


extern uint16_t crc16_xmodem(uint16_t *prev_crc, const void *data, size_t len);
extern uint32_t crc32_cksum(uint32_t *prev_crc, const void *data, size_t len);


#endif
