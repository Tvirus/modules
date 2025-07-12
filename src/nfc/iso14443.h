#ifndef  _ISO14443_H_
#define  _ISO14443_H_


#define MAX_UID_SIZE 10
typedef struct
{
    unsigned char cid;
    unsigned char nads;  /* sad */
    unsigned char nadd;  /* dad */

    /* 以上由用户设置 */

    unsigned char uid_len;
    unsigned char uid[MAX_UID_SIZE];
    unsigned char support_14443_4;
    unsigned char support_cid;
    unsigned char support_nad;
    unsigned char power_level;
    unsigned char block_num;

    unsigned int fsc;  /* 卡最大接收帧长度 fsc Frame Size for proximity Card */
    unsigned int sfg;  /* ms 定义了在发送了ATS之后，准备接收下一个帧之前PICC所需的特定保护时间 */
    unsigned int fwt;  /* ms 帧等待时间，Frame waiting time，pcd的帧发完后多久内picc需要回复 */

    unsigned char support_diff_div:1;  /* different divisors for each direction */
    unsigned char support_ds8:1;
    unsigned char support_ds4:1;
    unsigned char support_ds2:1;
    unsigned char support_dr8:1;
    unsigned char support_dr4:1;
    unsigned char support_dr2:1;
} card_info_t;


extern int init_card_info(card_info_t *info, unsigned char cid);

extern int typea_activate(card_info_t *info);
extern int typea_halt(void);
extern int typea_rats(card_info_t *info);

extern int typeab_deselect(card_info_t *info);


#endif
