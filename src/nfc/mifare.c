#include "mifare.h"
#include "fm17622.h"




#define M1_CMD_AUTH_A     0x60
#define M1_CMD_AUTH_B     0x61
#define M1_CMD_READ       0x30
#define M1_CMD_WRITE      0xA0
#define M1_CMD_DECREMENT  0xC0
#define M1_CMD_INCREMENT  0xC1
#define M1_CMD_RESTORE    0xC2
#define M1_CMD_TRANSFER   0xB0


int m1_auth(unsigned char sector, unsigned char key_id, const unsigned char *key, const unsigned char *uid)
{
    m1_auth_t auth;

    if (0 == key_id)
        auth.cmd = M1_CMD_AUTH_A;
    else
        auth.cmd = M1_CMD_AUTH_B;
    auth.block = sector << 2;
    auth.key[0] = key[0];
    auth.key[1] = key[1];
    auth.key[2] = key[2];
    auth.key[3] = key[3];
    auth.key[4] = key[4];
    auth.key[5] = key[5];
    auth.uid[0] = uid[0];
    auth.uid[1] = uid[1];
    auth.uid[2] = uid[2];
    auth.uid[3] = uid[3];

    return pcd_m1_auth(&auth);
}

int m1_read_block(unsigned char block, unsigned char *buf)
{
    unsigned char cmd[2];

    cmd[0] = M1_CMD_READ;
    cmd[1] = block;

    if (16 * 8 != pcd_send(cmd, 16, buf, 16, 0, 0))
        return -1;

    return 0;
}
