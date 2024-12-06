#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>


extern uint16_t crc16_xmodem(const uint8_t *data, uint32_t len);
extern uint32_t crc32_cksum(const uint8_t *data, uint32_t len);


#endif
