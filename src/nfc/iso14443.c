#include "iso14443.h"
#include "log.h"
#include <string.h>
#include "fm17622.h"
#include "stm32u5xx_hal.h"




#define UID_SIZE_4  0x00
#define UID_SIZE_7  0x01
#define UID_SIZE_10 0x02
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char rfu1:4;
    unsigned char coding:4;

    unsigned char uid_size:2;
    unsigned char rfu2:1;
    unsigned char anticollision:5
} ATQA_t;
#else
typedef struct
{
    unsigned char coding:4;
    unsigned char rfu1:4;

    unsigned char anticollision:5;
    unsigned char rfu2:1;
    unsigned char uid_size:2;
} ATQA_t;
#endif

#define SELECT_LEVEL_1  0x93
#define SELECT_LEVEL_2  0x95
#define SELECT_LEVEL_3  0x97
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char level;

    unsigned char bytes:4;
    unsigned char bits:4;

    unsigned char uid[4];
    unsigned char bcc;
} SELECT_t;
#else
typedef struct
{
    unsigned char level;

    unsigned char bits:4;
    unsigned char bytes:4;

    unsigned char uid[4];
    unsigned char bcc;
} SELECT_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char :1;
    unsigned char iso18092:1;
    unsigned char iso14443_4:1;
    unsigned char :2;
    unsigned char uid_not_complete:1;
    unsigned char :2;
} SAK_t;
#else
typedef struct
{
    unsigned char :2;
    unsigned char uid_not_complete:1;
    unsigned char :2;
    unsigned char iso14443_4:1;
    unsigned char iso18092:1;
    unsigned char :1;
} SAK_t;
#endif

#define RATS  0xE0
#define FSI_16   0x00
#define FSI_24   0x01
#define FSI_32   0x02
#define FSI_40   0x03
#define FSI_48   0x04
#define FSI_64   0x05
#define FSI_96   0x06
#define FSI_128  0x07
#define FSI_256  0x08
#define FSI_512  0x09
#define FSI_1024 0x0A
#define FSI_2048 0x0B
#define FSI_4096 0x0C
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char cmd;

    unsigned char fsdi:4;
    unsigned char cid:4;  /* 如果PICC的CID是0，PICC会回应不包含CID的消息 */
} RATS_t;
#else
typedef struct
{
    unsigned char cmd;

    unsigned char cid:4;
    unsigned char fsdi:4;
} RATS_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char tl;  /* 长度，包括TL本身，不包括2字节CRC */

    unsigned char rfu:1;
    unsigned char tc:1;
    unsigned char tb:1;
    unsigned char ta:1;
    unsigned char fsci:4;

    unsigned char data[0];
} ATS_t;
#else
typedef struct
{
    unsigned char tl;

    unsigned char fsci:4;
    unsigned char ta:1;
    unsigned char tb:1;
    unsigned char tc:1;
    unsigned char rfu:1;

    unsigned char data[0];
} ATS_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char diff_div:1;  /* different divisors for each direction */
    unsigned char ds8:1;
    unsigned char ds4:1;
    unsigned char ds2:1;
    unsigned char rfu:1;
    unsigned char dr8:1;
    unsigned char dr4:1;
    unsigned char dr2:1;
} ATS_TA_t;
#else
typedef struct
{
    unsigned char dr2:1;
    unsigned char dr4:1;
    unsigned char dr8:1;
    unsigned char rfu:1;
    unsigned char ds2:1;
    unsigned char ds4:1;
    unsigned char ds8:1;
    unsigned char diff_div:1;
} ATS_TA_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char fwi:4;
    unsigned char sfgi:4;
} ATS_TB_t;
#else
typedef struct
{
    unsigned char sfgi:4;
    unsigned char fwi:4;
} ATS_TB_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char rfu:6;
    unsigned char spt_cid:1;
    unsigned char spt_nad:1;
} ATS_TC_t;
#else
typedef struct
{
    unsigned char spt_nad:1;
    unsigned char spt_cid:1;
    unsigned char rfu:6;
} ATS_TC_t;
#endif


#define BLOCK_TYPE_I  0x00
#define BLOCK_TYPE_R  0x02
#define BLOCK_TYPE_S  0x03
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char block_type:2;
    unsigned char rsv0_set0:1;
    unsigned char chaining:1;
    unsigned char cid:1;
    unsigned char nad:1;
    unsigned char rsv1_set1:1;
    unsigned char block_num:1;  /* 必须从0开始 */
} I_BLOCK_t;
#else
typedef struct
{
    unsigned char block_num:1;
    unsigned char rsv1_set1:1;
    unsigned char nad:1;
    unsigned char cid:1;
    unsigned char chaining:1;
    unsigned char rsv0_set0:1;
    unsigned char block_type:2;
} I_BLOCK_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char block_type:2;
    unsigned char rsv0_set1:1;
    unsigned char nak:1;  /* 1:nak, 0:ack */
    unsigned char cid:1;
    unsigned char rsv1_set0:1;
    unsigned char rsv2_set1:1;
    unsigned char block_num:1;
} R_BLOCK_t;
#else
typedef struct
{
    unsigned char block_num:1;
    unsigned char rsv2_set1:1;
    unsigned char rsv1_set0:1;
    unsigned char cid:1;
    unsigned char nak:1;
    unsigned char rsv0_set1:1;
    unsigned char block_type:2;
} R_BLOCK_t;
#endif

#define S_BLOCK_DESELECT  0x00
#define S_BLOCK_WTX       0x03
#define S_BLOCK_PARAM     0x03
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char block_type:2;
    unsigned char desel_wtx:2;  /* not_param=0时应该设置成11，not_param=1时、00是deselect 11是wtx */
    unsigned char cid:1;
    unsigned char rsv0:1;
    unsigned char not_param:1;  /* 0:parameters  1:deselect or wtx */
    unsigned char rsv1:1;
} S_BLOCK_t;
#else
typedef struct
{
    unsigned char rsv1:1;
    unsigned char not_param:1;
    unsigned char rsv0:1;
    unsigned char cid:1;
    unsigned char desel_wtx:2;
    unsigned char block_type:2;
} S_BLOCK_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char power_level:2;
    unsigned char rfu:2;
    unsigned char cid:4;  /* 如果PICC的CID是0，PICC会回应不包含CID的消息 */
} BLOCK_CID_t;
#else
typedef struct
{
    unsigned char cid:4;
    unsigned char rfu:2;
    unsigned char power_level:2;
} BLOCK_CID_t;
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
typedef struct
{
    unsigned char rsv0:1;
    unsigned char dad:3;
    unsigned char rsv1:1;
    unsigned char sad:3;
} BLOCK_NAD_t;
#else
typedef struct
{
    unsigned char sad:3;
    unsigned char rsv1:1;
    unsigned char dad:3;
    unsigned char rsv0:1;
} BLOCK_NAD_t;
#endif




#define LOGLEVEL_ERROR 1
#define LOGLEVEL_INFO  2
#define LOGLEVEL_DEBUG 3
#define LOG(level, fmt, arg...)  do{if((level)<=iso14443_log_level)log_printf("--TypeAB-- " fmt "\n", ##arg);}while (0)


#define GET_BCC(buf) ((buf)[0] ^ (buf)[1] ^ (buf)[2] ^ (buf)[3])
#define FWI2FWT(fwi) ((1 << (fwi)) * 256 * 16 / 13560)
#define SFGI2SFGT(sfgi) ((1 << (sfgi)) * 256 * 16 / 13560)


unsigned char iso14443_log_level = LOGLEVEL_DEBUG;


static unsigned int fsi2fs[] =
{
    [0x00] = 16,
    [0x01] = 24,
    [0x02] = 32,
    [0x03] = 40,
    [0x04] = 48,
    [0x05] = 64,
    [0x06] = 96,
    [0x07] = 128,
    [0x08] = 256,
    [0x09] = 512,
    [0x0A] = 1024,
    [0x0B] = 2048,
    [0x0C] = 4096
};
#define MAX_FSI ((sizeof(fsi2fs) / sizeof(fsi2fs[0])) - 1)
#define MAX_FS  (fsi2fs[MAX_FSI])
static unsigned int get_fs(unsigned int fsi)
{
    if (MAX_FSI <= fsi)
        return MAX_FS;
    return fsi2fs[fsi];
}
static unsigned char get_fsi(unsigned int fs)
{
    int i;

    for (i = MAX_FSI; i >= 0; i--)
    {
        if (fsi2fs[i] <= fs)
            return (unsigned char)i;
    }

    LOG(LOGLEVEL_ERROR, "get fsi failed, invalid fs(%u) !", fs);
    return 0;
}


int init_card_info(card_info_t *info, unsigned char cid)
{
    if ((NULL == info) || (15 <= cid))
        return -1;

    memset((unsigned char *)info, 0, sizeof(card_info_t));
    info->cid = cid;

    return 0;
}

/* wakeup:是否使用wakeup代替req，和req相比、wakeup还可以多唤醒处于halt状态的卡 */
int typea_request(unsigned char wakeup, ATQA_t *atqa)
{
    unsigned char cmd;
    unsigned char buf[2];
    unsigned char coll_pos;
    int len;


    if (NULL == atqa)
        return -1;

    pcd_disable_crc();
    pcd_set_timer(1);
    //待处理。。。。。。。。。。。。。。。。。。。。。。。。。。。。。。。。。
    //fm17622_set_bits(Status2Reg, Crypto1On, 0);

    if (wakeup)
    {
        LOG(LOGLEVEL_DEBUG, " ==> WAKEUP A");
        cmd = 0x52;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, " ==> REQA");
        cmd = 0x26;
    }

    len = pcd_send(&cmd, 7, buf, sizeof(buf), 0, &coll_pos);
    if ((2 * 8) != len)
    {
        if (wakeup)
            LOG(LOGLEVEL_DEBUG, "WAKEUP A failed(%d)", len);
        else
            LOG(LOGLEVEL_DEBUG, "REQA failed(%d)", len);
        return -1;
    }

    ((unsigned char *)atqa)[0] = buf[1];
    ((unsigned char *)atqa)[1] = buf[0];
    LOG(LOGLEVEL_DEBUG, "<==  ATQA: %02x %02x, coding:%u uid_size:%u(%u)", buf[1], buf[0], atqa->coding, atqa->uid_size, atqa->uid_size * 3 + 4);
    if (coll_pos)
        LOG(LOGLEVEL_DEBUG, "     ATQA collison %u", coll_pos);

    return 0;
}


int typea_halt(void)
{
    int len;

    pcd_enable_crc();
    pcd_set_timer(1);
    len = pcd_send((unsigned char *)"\x50\x00", 2 * 8, NULL, 0, 0, NULL);
    if (0 > len)
    {
        LOG(LOGLEVEL_ERROR, "halt typeA failed(%d) !", len);
        return -1;
    }
    return 0;
}


/* level:串联级别 cascade level, [1,3] */
static int typea_anticoll(unsigned char level, unsigned char *uid, unsigned char *valid_bits)
{
    SELECT_t sel;
    unsigned char buf[5] = {0};
    unsigned char coll_pos;
    int len;
    unsigned char start;
    unsigned char mask;


    if (32 == *valid_bits)
        return 0;
    else if (32 < *valid_bits)
        return -1;

    pcd_disable_crc();
    pcd_set_timer(1);

    if (1 == level)
        sel.level = SELECT_LEVEL_1;
    else if (2 == level)
        sel.level = SELECT_LEVEL_2;
    else if (3 == level)
        sel.level = SELECT_LEVEL_3;
    else
        return -1;
    sel.bytes = (*valid_bits) / 8 + 2;
    sel.bits  = (*valid_bits) & 0x07;
    memcpy(sel.uid, uid, 4);
    LOG(LOGLEVEL_DEBUG, " ==> Anticoll: %02x %02x  %02x %02x %02x %02x, valid_bits:%u",
           ((unsigned char *)&sel)[0],
           ((unsigned char *)&sel)[1],
           ((unsigned char *)&sel)[2],
           ((unsigned char *)&sel)[3],
           ((unsigned char *)&sel)[4],
           ((unsigned char *)&sel)[5],
           *valid_bits);

    len = pcd_send((unsigned char *)&sel, (*valid_bits) + 2*8, buf, sizeof(buf), (*valid_bits) & 0x07, &coll_pos);
    if ((5 * 8 - ((*valid_bits) & 0xF8)) != len)
    {
        LOG(LOGLEVEL_ERROR, "typeA anticoll failed(%d) !", len);
        return -1;
    }
    len /= 8;
    if (32 < (*valid_bits + coll_pos))
    {
        LOG(LOGLEVEL_ERROR, "typeA anticoll failed, wrong coll_pos(%u) !", coll_pos);
        return -1;
    }

    /* 拼接 */
    start = (*valid_bits) / 8;  /* 从uid的第几个字节开始复制 */
    mask = (unsigned char)(0xFF << ((*valid_bits) % 8));  /* 第一个要复制字节的位 */
    uid[start] = (buf[0] & mask) | (uid[start] & ((unsigned char)~mask));
    memcpy(&uid[start + 1], &buf[1], 3 - start);

    LOG(LOGLEVEL_DEBUG, "<==  Anticoll: %02x %02x %02x %02x %02x, len:%d, mask:0x%02x, coll_pos:%u",
           buf[0], buf[1], buf[2], buf[3], buf[4], len, mask, coll_pos);

    if (0 == coll_pos)
    {
        if (GET_BCC(uid) != buf[len - 1])
        {
            LOG(LOGLEVEL_ERROR, "typeA anticoll crc error !");
            return -1;
        }
        *valid_bits = 4 * 8;
    }
    else
    {
        *valid_bits += coll_pos - 1;
    }

    return 0;
}


static int typea_select(unsigned char level, const unsigned char *uid, SAK_t *sak)
{
    SELECT_t sel;
    int len;


    pcd_enable_crc();
    pcd_set_timer(1);

    if (1 == level)
        sel.level = SELECT_LEVEL_1;
    else if (2 == level)
        sel.level = SELECT_LEVEL_2;
    else if (3 == level)
        sel.level = SELECT_LEVEL_3;
    else
        return -1;
    sel.bytes = 7;
    sel.bits  = 0;
    memcpy(sel.uid, uid, 4);
    sel.bcc  = GET_BCC(sel.uid);
    LOG(LOGLEVEL_DEBUG, " ==> SELECT: %02x %02x  %02x %02x %02x %02x  %02x",
           ((unsigned char *)&sel)[0],
           ((unsigned char *)&sel)[1],
           ((unsigned char *)&sel)[2],
           ((unsigned char *)&sel)[3],
           ((unsigned char *)&sel)[4],
           ((unsigned char *)&sel)[5],
           ((unsigned char *)&sel)[6]);

    len = pcd_send((unsigned char *)&sel, sizeof(sel) * 8, (unsigned char *)sak, 1, 0, NULL);
    if (8 != len)
    {
        LOG(LOGLEVEL_ERROR, "typeA select failed(%d) !", len);
        return -1;
    }
    if (sak->uid_not_complete)
        LOG(LOGLEVEL_DEBUG, "<==  SAK: %02x, not complete", *((unsigned char *)sak));
    else
        LOG(LOGLEVEL_DEBUG, "<==  SAK: %02x, ISO/IEC18092:%u ISO/IEC14443-4:%u", *((unsigned char *)sak), sak->iso18092, sak->iso14443_4);

    return 0;
}


int typea_activate(card_info_t *info)
{
    ATQA_t atrq;
    unsigned char tmp_uid[4] = {0};
    unsigned char valid_bits = 0;
    SAK_t sak;
    int i, j;


    if (NULL == info)
        return -1;

    if (typea_request(1, &atrq))
        return -1;

    info->uid_len = 0;
    for (i = 0; i < 3; i++)
    {
        valid_bits = 0;
        for (j = 0; j < 32; j++)
        {
            if (typea_anticoll(i + 1, tmp_uid, &valid_bits))
                return -1;
            if (32 <= valid_bits)
                break;

            tmp_uid[valid_bits / 8] |= 1 << (valid_bits % 8);
            valid_bits++;
            if (32 <= valid_bits)
                break;
        }
        if (32 <= j)
        {
            LOG(LOGLEVEL_ERROR, "typeA activate failed, anticoll infinite loop !");
            return -1;
        }

        if (typea_select(i + 1, tmp_uid, &sak))
            return -1;
        if (!sak.uid_not_complete)
            break;
        memcpy(&info->uid[i * 3], &tmp_uid[1], 3);
        info->uid_len += 3;
    }
    if (3 <= i)
    {
        LOG(LOGLEVEL_ERROR, "typeA activate failed, cascade level err !");
        return -1;
    }

    memcpy(&info->uid[i * 3], tmp_uid, 4);
    info->uid_len += 4;
    info->support_14443_4 = sak.iso14443_4;

    if (4 == info->uid_len)
        LOG(LOGLEVEL_DEBUG, "[UID](%u): %02x %02x %02x %02x", info->uid_len,
               info->uid[0], info->uid[1], info->uid[2], info->uid[3]);
    else if (7 == info->uid_len)
        LOG(LOGLEVEL_DEBUG, "[UID](%u): %02x %02x %02x %02x %02x %02x %02x", info->uid_len,
               info->uid[0], info->uid[1], info->uid[2], info->uid[3],
               info->uid[4], info->uid[5], info->uid[6]);
    else if (10 == info->uid_len)
        LOG(LOGLEVEL_DEBUG, "[UID](%u): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", info->uid_len,
               info->uid[0], info->uid[1], info->uid[2], info->uid[3],
               info->uid[4], info->uid[5], info->uid[6], info->uid[7], info->uid[8], info->uid[9]);

    return 0;
}


int typea_rats(card_info_t *info)
{
    RATS_t rats;
    unsigned char buf[PCD_FIFO_SIZE];
    ATS_t *ats = (ATS_t *)buf;
    int len;
    int i;

    if (NULL == info)
        return -1;

    pcd_enable_crc();
    pcd_set_timer(7);

    /* 设置默认值 */
    info->fsc = get_fs(FSI_16);
    info->sfg = 0;
    info->fwt = 100;
    info->support_diff_div = 0;
    info->support_ds8 = 0;
    info->support_ds4 = 0;
    info->support_ds2 = 0;
    info->support_dr8 = 0;
    info->support_dr4 = 0;
    info->support_dr2 = 0;
    info->support_cid = 0;
    info->support_nad = 0;

    rats.cmd  = RATS;
    rats.fsdi = get_fsi(PCD_FIFO_SIZE);
    rats.cid  = info->cid;
    LOG(LOGLEVEL_DEBUG, " ==> RATS: %02x %02x", ((unsigned char *)&rats)[0], ((unsigned char *)&rats)[1]);
    len = pcd_send((unsigned char *)&rats, sizeof(rats) * 8, buf, sizeof(buf), 0, NULL);
    if ((0 >= len) || (len % 8))
    {
        LOG(LOGLEVEL_ERROR, "rats failed(%d) !", len);
        return -1;
    }
    len /= 8;
    if (ats->tl != len)
    {
        LOG(LOGLEVEL_ERROR, "ats error, TL:%u, recv:%u !", ats->tl, len);
        return -1;
    }
    if (1 == len)
    {
        LOG(LOGLEVEL_DEBUG, "<==  ATS: %02x", buf[0]);
        return 1;
    }
    if (LOGLEVEL_DEBUG <= iso14443_log_level)
    {
        log_printf("--TypeAB-- <==  ATS: ");
        for (i = 0; i < len; i++)
        {
            log_printf("%02x ", buf[i]);
        }
        log_printf("\n");
    }
    if ((ats->ta + ats->tb + ats->tc + sizeof(ATS_t)) > len)
    {
        LOG(LOGLEVEL_ERROR, "ats error, TL:%u, recv:%u, FSCI:%u, TA:%u, TB:%u, TC:%u !",
               ats->tl, len, ats->fsci, ats->ta, ats->tb, ats->tc);
        return -1;
    }
    info->fsc = get_fs(ats->fsci);

    i = 0;
    if (ats->ta)
    {
        info->support_diff_div = !!(((ATS_TA_t *)(&ats->data[i]))->diff_div);
        info->support_ds8 = !!(((ATS_TA_t *)(&ats->data[i]))->ds8);
        info->support_ds4 = !!(((ATS_TA_t *)(&ats->data[i]))->ds4);
        info->support_ds2 = !!(((ATS_TA_t *)(&ats->data[i]))->ds2);
        info->support_dr8 = !!(((ATS_TA_t *)(&ats->data[i]))->dr8);
        info->support_dr4 = !!(((ATS_TA_t *)(&ats->data[i]))->dr4);
        info->support_dr2 = !!(((ATS_TA_t *)(&ats->data[i]))->dr2);
        i++;
    }
    if (ats->tb)
    {
        info->fwt = FWI2FWT(((ATS_TB_t *)(&ats->data[i]))->fwi);
        info->sfg = SFGI2SFGT(((ATS_TB_t *)(&ats->data[i]))->sfgi);
        if (0 == info->fwt)
            info->fwt = 1;
        i++;
    }
    if (ats->tc)
    {
        info->support_cid = !!(((ATS_TC_t *)(&ats->data[i]))->spt_cid);
        info->support_nad = !!(((ATS_TC_t *)(&ats->data[i]))->spt_nad);
        i++;
    }

    LOG(LOGLEVEL_DEBUG, "          FSD:%u FSC:%u SFG:%ums FWT:%ums  diff_div:%u DS8:%u DS4:%u DS2:%u DR8:%u DR4:%u DR2:%u  cid:%u nad:%u",
           PCD_FIFO_SIZE,
           info->fsc,
           info->sfg,
           info->fwt,
           info->support_diff_div,
           info->support_ds8,
           info->support_ds4,
           info->support_ds2,
           info->support_dr8,
           info->support_dr4,
           info->support_dr2,
           info->support_cid,
           info->support_nad);

    HAL_Delay(info->sfg);
    return 0;
}


int typeab_deselect(card_info_t *info)
{
    unsigned char s[2] = {0};
    unsigned char r[2] = {0};
    S_BLOCK_t *send_sblock = (S_BLOCK_t *)s;
    int len;


    pcd_enable_crc();
    pcd_set_timer(5);

    send_sblock->block_type = BLOCK_TYPE_S;
    send_sblock->desel_wtx = S_BLOCK_DESELECT;
    send_sblock->cid = !!(info->support_cid);
    send_sblock->rsv0 = 0;
    send_sblock->not_param = 1;
    send_sblock->rsv1 = 0;
    if (info->support_cid && (0 != info->cid))
    {
        s[1] = info->cid;
        LOG(LOGLEVEL_DEBUG, " ==> DESELECT: %02x %02x", s[0], s[1]);
        len = pcd_send(s, 2 * 8, r, sizeof(r), 0, NULL);
        if ((16 != len) || (s[0] != r[0]) || (s[1] != r[1]))
        {
            LOG(LOGLEVEL_ERROR, "DESELECT failed(%d): %02x %02x !", len, r[0], r[1]);
            return -1;
        }
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, " ==> DESELECT: %02x", s[0]);
        len = pcd_send(s, 1 * 8, r, sizeof(r), 0, NULL);
        if ((8 != len) || (s[0] != r[0]))
        {
            LOG(LOGLEVEL_ERROR, "DESELECT failed(%d): %02x !", len, r[0]);
            return -1;
        }
    }

    LOG(LOGLEVEL_DEBUG, "<==  DESELECT OK");
    return 0;
}
