#ifndef _FM17622_H_
#define _FM17622_H_


#define PCD_FIFO_SIZE  64

typedef struct
{
    unsigned char cmd;
    unsigned char block;
    unsigned char key[6];
    unsigned char uid[4];
} m1_auth_t;

extern int pcd_soft_reset(void);
#define PCD_MODE_POWERDOWN  0
#define PCD_MODE_LPCD       1
#define PCD_MODE_TYPEA      2
#define PCD_MODE_TYPEB      3
extern int pcd_set_mode(int mode);
extern int pcd_enable_crc(void);
extern int pcd_disable_crc(void);
extern int pcd_set_timer(unsigned int ms);
extern int pcd_send(unsigned char *send, unsigned int send_bits, unsigned char *recv, unsigned char recv_len, unsigned char recv_pos, unsigned char *coll);
extern int pcd_m1_auth(m1_auth_t *auth);
extern int pcd_disable_m1_auth(void);


#endif
