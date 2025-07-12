#ifndef  _MIFARE_H_
#define  _MIFARE_H_


extern int m1_auth(unsigned char sector, unsigned char key_id, unsigned char *key, unsigned char *uid);
extern int m1_read_block(unsigned char block, unsigned char *buf);


#endif
