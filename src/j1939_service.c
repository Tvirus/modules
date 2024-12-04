#include "j1939_service.h"
#include "uart.h"
#include "stm32l4xx_hal.h"
#include <string.h>




#define LOGLEVEL_ERROR  1
#define LOGLEVEL_INFO   2
#define LOGLEVEL_DEBUG  3
#define LOG(level, fmt, arg...)  do{if((level) <= j1939_log_level)debug_printf("--J1939-- " fmt, ##arg);}while(0)


#ifndef CAN_MSG_COUNT_MAX
#define CAN_MSG_COUNT_MAX       64
#endif
#ifndef J1939_MSG_CB_MAX
#define J1939_MSG_CB_MAX        32
#endif
#ifndef J1939_LARGE_MSG_CB_MAX
#define J1939_LARGE_MSG_CB_MAX  16
#endif
#ifndef J1939_LARGE_MSG_TX_MAX
#define J1939_LARGE_MSG_TX_MAX   8
#endif


typedef struct
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    unsigned char rsv:3;
    unsigned char priority:3;
    unsigned char edp:1;
    unsigned char dp:1;
#else
    unsigned char dp:1;
    unsigned char edp:1;
    unsigned char priority:3;
    unsigned char rsv:3;
#endif

    unsigned char pf;
    unsigned char ps;
    unsigned char sa;
}j1939_pdu_t;

typedef struct
{
    j1939_msg_header_t header;
    j1939_msg_cb_t cb;  /* 同时用来标记已用 */
}j1939_msg_cb_info_t;

typedef struct
{
    j1939_msg_header_t header;
    j1939_msg_cb_t cb;  /* 同时用来标记已用 */
    unsigned char *buf;
    unsigned int buf_size;
    unsigned char start;
    unsigned char rsv;
    unsigned char total_packets;
    unsigned char cur_packets;
    unsigned int total_bytes;
    unsigned int cur_bytes;
}j1939_large_msg_cb_info_t;

typedef struct
{
    j1939_msg_header_t header;
    unsigned char *data;  /* 同时用来标记已用 */
    unsigned char state;
    unsigned char rsv;
    unsigned char total_packets;
    unsigned char cur_packets;
    unsigned int total_bytes;
    unsigned int interval;  /* 发送间隔，ms */
    unsigned int ts;  /* 上一次的发送时间，ms */
}j1939_large_msg_tx_info_t;


unsigned char j1939_log_level = LOGLEVEL_ERROR;

static unsigned int can_msg_head = 0;
static unsigned int can_msg_tail = 0;  /* 永远指向空节点 */
static can_msg_t can_msg_ring_buf[CAN_MSG_COUNT_MAX] = {0};
static j1939_msg_cb_info_t j1939_msg_cb_list[J1939_MSG_CB_MAX] = {0};
static j1939_large_msg_cb_info_t j1939_large_msg_cb_list[J1939_LARGE_MSG_CB_MAX] = {0};
static j1939_large_msg_tx_info_t j1939_large_msg_tx_list[J1939_LARGE_MSG_TX_MAX] = {0};


int j1939_register_msg_cb(const j1939_msg_header_t *header, j1939_msg_cb_t cb)
{
    int i;

    if ((NULL == header) || (NULL == cb))
        return -1;

    for (i = 0; i < J1939_MSG_CB_MAX; i++)
    {
        if (NULL != j1939_msg_cb_list[i].cb)
            continue;

        memset(&j1939_msg_cb_list[i], 0, sizeof(j1939_msg_cb_list[i]));
        j1939_msg_cb_list[i].header = *header;
        j1939_msg_cb_list[i].header.priority = j1939_msg_cb_list[i].header.priority & 0x07;
        j1939_msg_cb_list[i].header.ext_data_page = !!j1939_msg_cb_list[i].header.ext_data_page;
        if (0xf000 <= (j1939_msg_cb_list[i].header.pgn & 0xff00))
            j1939_msg_cb_list[i].header.dst_addr = 0;
        j1939_msg_cb_list[i].cb = cb;

        return 0;
    }

    LOG(LOGLEVEL_ERROR, "register msg cb failed !\n");
    return -1;
}

int j1939_register_large_msg_cb(const j1939_msg_header_t *header, unsigned char *buf, unsigned int size, j1939_msg_cb_t cb)
{
    int i;

    if ((NULL == header) || (NULL == buf) || (NULL == cb) || (8 >= size))
        return -1;

    for (i = 0; i < J1939_LARGE_MSG_CB_MAX; i++)
    {
        if (NULL != j1939_large_msg_cb_list[i].cb)
            continue;

        j1939_large_msg_cb_list[i].header = *header;
        j1939_large_msg_cb_list[i].header.priority = j1939_large_msg_cb_list[i].header.priority & 0x07;
        j1939_large_msg_cb_list[i].header.ext_data_page = !!j1939_msg_cb_list[i].header.ext_data_page;
        j1939_large_msg_cb_list[i].buf = buf;
        j1939_large_msg_cb_list[i].buf_size = size;
        j1939_large_msg_cb_list[i].start = 0;
        j1939_large_msg_cb_list[i].cb = cb;

        return 0;
    }

    LOG(LOGLEVEL_ERROR, "register large msg cb failed !\n");
    return -1;
}

int j1939_put_can_msg(const can_msg_t *msg)
{
    if ((NULL == msg) || (8 < msg->dlc))
        return -1;

    __disable_irq();
    if ((can_msg_tail + 1 == can_msg_head) || ((0 == can_msg_head) && (CAN_MSG_COUNT_MAX - 1 == can_msg_tail)))
    {
        __enable_irq();
        LOG(LOGLEVEL_ERROR, "put can msg failed !\n");
        return -1;
    }

    can_msg_ring_buf[can_msg_tail] = *msg;

    can_msg_tail++;
    if (CAN_MSG_COUNT_MAX <= can_msg_tail)
        can_msg_tail = 0;
    __enable_irq();

    return 0;
}

extern CAN_HandleTypeDef hcan1;
int j1939_send_msg(const j1939_msg_header_t *header, const unsigned char *data, unsigned char len)
{
    unsigned char pdu_buf[4];
    j1939_pdu_t *pdu = (j1939_pdu_t *)pdu_buf;
    CAN_TxHeaderTypeDef can_header;
    uint32_t mail_box;
    int i;


    if ((NULL == header) || (NULL == data) || (8 < len))
        return -1;

    pdu->rsv = 0;
    pdu->priority = header->priority & 0x07;
    pdu->edp = !!header->ext_data_page;
    pdu->dp = (header->pgn >> 16) & 0x01;
    pdu->pf = (header->pgn >> 8) & 0xff;
    if (0xf0 > pdu->pf)
        pdu->ps = header->dst_addr;
    else
        pdu->ps = header->pgn & 0xff;
    pdu->sa = header->src_addr;

    can_header.ExtId = (pdu_buf[0] << 24) | (pdu_buf[1] << 16) | (pdu_buf[2] << 8) | pdu_buf[3];
    can_header.IDE = CAN_ID_EXT;
    can_header.RTR = CAN_RTR_DATA;
    can_header.DLC = len;

    LOG(LOGLEVEL_DEBUG, "send can msg: [%02X->%02X] PGN:%06x extid:%x, len:%u data:%02x %02x %02x %02x %02x %02x %02x %02x\n",
                         header->src_addr, header->dst_addr, header->pgn, can_header.ExtId, len,
                         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    /* 发送邮箱需要配置为fifo模式! */
    for (i = 0; i < 500; i++)
    {
        if (0 == HAL_CAN_GetTxMailboxesFreeLevel(&hcan1))
            continue;
        if (HAL_OK != HAL_CAN_AddTxMessage(&hcan1, &can_header, data, &mail_box))
        {
            LOG(LOGLEVEL_ERROR, "send msg failed: [%02X->%02X] PGN:%06x, len:%u\n", header->src_addr, header->dst_addr, header->pgn, len);
            return -1;
        }
        return 0;
    }

    LOG(LOGLEVEL_ERROR, "send msg timeout: [%02X->%02X] PGN:%06x, len:%u\n", header->src_addr, header->dst_addr, header->pgn, len);
    return -1;
}

int j1939_send_rqst(const j1939_msg_header_t *header)
{
    j1939_msg_header_t _header;
    j1939_rqst_t rqst;

    if (NULL == header)
        return -1;

    _header.pgn = J1939_PGN_RQST;
    _header.priority = J1939_DEFUALT_PRIORITY;
    _header.ext_data_page = 0;
    _header.dst_addr = header->dst_addr;
    _header.src_addr = header->src_addr;

    rqst.pgn[0] = header->pgn & 0xff;
    rqst.pgn[1] = (header->pgn >> 8) & 0xff;
    rqst.pgn[2] = (header->pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send rqst: [%02X->%02X] PGN:%06x\n", header->src_addr, header->dst_addr, header->pgn);
    return j1939_send_msg(&_header, (unsigned char *)&rqst, sizeof(rqst));
}

int j1939_send_ackm(const j1939_msg_header_t *header, unsigned char ack, unsigned char group_fun, unsigned char ori_addr)
{
    j1939_msg_header_t _header;
    j1939_ackm_t ackm;

    if (NULL == header)
        return -1;

    _header.pgn = J1939_PGN_ACKM;
    _header.priority = J1939_DEFUALT_PRIORITY;
    _header.ext_data_page = 0;
    _header.dst_addr = header->dst_addr;
    _header.src_addr = header->src_addr;

    ackm.control = ack;
    ackm.group_fun = group_fun;
    ackm.rsv[0] = 0xff;
    ackm.rsv[1] = 0xff;
    ackm.ori_addr = ori_addr;
    ackm.pgn[0] = header->pgn & 0xff;
    ackm.pgn[1] = (header->pgn >> 8) & 0xff;
    ackm.pgn[2] = (header->pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send ackm: [%02X->%02X] PGN:%06x, ack:%u group:%02x ori:%20x\n",
        header->src_addr, header->dst_addr, header->pgn, ack, group_fun, ori_addr);
    return j1939_send_msg(&_header, (unsigned char *)&ackm, sizeof(ackm));
}

int j1939_send_tpcm_rts(unsigned int pgn, unsigned char dst, unsigned char src, unsigned int total_bytes)
{
    j1939_msg_header_t header;
    j1939_tpcm_rts_t rts;

    header.pgn = J1939_PGN_TPCM;
    header.priority = J1939_DEFUALT_PRIORITY;
    header.ext_data_page = 0;
    header.dst_addr = dst;
    header.src_addr = src;

    rts.control = J1939_TPCM_CONTROL_RTS;
    rts.total_bytes[0] = total_bytes & 0xff;
    rts.total_bytes[1] = (total_bytes >> 8) & 0xff;
    rts.total_packets = (total_bytes + 6) / 7;
    rts.max_packets = 0xff;
    rts.pgn[0] = pgn & 0xff;
    rts.pgn[1] = (pgn >> 8) & 0xff;
    rts.pgn[2] = (pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send tpcm_rts: [%02X->%02X] PGN:%06x, size:%u packets:%u\n", src, dst, pgn, total_bytes, rts.total_packets);
    return j1939_send_msg(&header, (unsigned char *)&rts, sizeof(rts));
}

int j1939_send_tpcm_cts(unsigned int pgn, unsigned char dst, unsigned char src, unsigned char max_packets, unsigned char next_packet)
{
    j1939_msg_header_t header;
    j1939_tpcm_cts_t cts;

    header.pgn = J1939_PGN_TPCM;
    header.priority = J1939_DEFUALT_PRIORITY;
    header.ext_data_page = 0;
    header.dst_addr = dst;
    header.src_addr = src;

    cts.control = J1939_TPCM_CONTROL_CTS;
    cts.max_packets = max_packets;
    cts.next_packet = next_packet;
    cts.rsv[0] = 0xff;
    cts.rsv[1] = 0xff;
    cts.pgn[0] = pgn & 0xff;
    cts.pgn[1] = (pgn >> 8) & 0xff;
    cts.pgn[2] = (pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send tpcm_cts: [%02X->%02X] PGN:%06x, max:%u next:%u\n", src, dst, pgn, max_packets, next_packet);
    return j1939_send_msg(&header, (unsigned char *)&cts, sizeof(cts));
}

int j1939_send_tpcm_ack(unsigned int pgn, unsigned char dst, unsigned char src, unsigned char packets, unsigned int bytes)
{
    j1939_msg_header_t header;
    j1939_tpcm_ack_t ack;

    header.pgn = J1939_PGN_TPCM;
    header.priority = J1939_DEFUALT_PRIORITY;
    header.ext_data_page = 0;
    header.dst_addr = dst;
    header.src_addr = src;

    ack.control = J1939_TPCM_CONTROL_ACK;
    ack.total_bytes[0] = bytes & 0xff;
    ack.total_bytes[1] = (bytes >> 8) & 0xff;
    ack.total_packets = packets;
    ack.rsv = 0xff;
    ack.pgn[0] = pgn & 0xff;
    ack.pgn[1] = (pgn >> 8) & 0xff;
    ack.pgn[2] = (pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send tpcm_ack: [%02X->%02X] PGN:%06x, size:%u packet:%u\n", src, dst, pgn, bytes, packets);
    return j1939_send_msg(&header, (unsigned char *)&ack, sizeof(ack));
}

int j1939_send_tpcm_abort(unsigned int pgn, unsigned char dst, unsigned char src, unsigned char reason, unsigned char role)
{
    j1939_msg_header_t header;
    j1939_tpcm_abort_t abort;

    header.pgn = J1939_PGN_TPCM;
    header.priority = J1939_DEFUALT_PRIORITY;
    header.ext_data_page = 0;
    header.dst_addr = dst;
    header.src_addr = src;

    abort.control = J1939_TPCM_CONTROL_ABORT;
    abort.reason = reason;
    abort.role = role & 0x03;
    abort.pgn[0] = pgn & 0xff;
    abort.pgn[1] = (pgn >> 8) & 0xff;
    abort.pgn[2] = (pgn >> 16) & 0xff;
    LOG(LOGLEVEL_INFO, "send tpcm_abort: [%02X->%02X] PGN:%06x, reason:%u role:%u\n", src, dst, pgn, reason, role);
    return j1939_send_msg(&header, (unsigned char *)&abort, sizeof(abort));
}

int j1939_send_tpdt(unsigned char dst, unsigned char src, unsigned char seq, const unsigned char *data, unsigned char len)
{
    j1939_msg_header_t header;
    j1939_tpdt_t tpdt;

    if (7 < len)
        return -1;

    header.pgn = J1939_PGN_TPDT;
    header.priority = J1939_DEFUALT_PRIORITY;
    header.ext_data_page = 0;
    header.dst_addr = dst;
    header.src_addr = src;

    tpdt.seq_num = seq;
    memcpy(tpdt.data, data, len);
    memset(&tpdt.data[len], 0xff, 7 - len);
    LOG(LOGLEVEL_INFO, "send tpdt: [%02X->%02X], seq:%u len:%u\n", src, dst, seq, len);
    return j1939_send_msg(&header, (unsigned char *)&tpdt, sizeof(tpdt));
}

int j1939_create_large_msg_sending(const j1939_msg_header_t *header, unsigned char *data, unsigned int len, unsigned int interval)
{
    int i;

    if ((NULL == header) || (NULL == data) || (8 >= len) || (255 * 7 < len))
        return -1;

    for (i = 0; i < J1939_LARGE_MSG_TX_MAX; i++)
    {
        if (NULL != j1939_large_msg_tx_list[i].data)
            continue;

        j1939_large_msg_tx_list[i].header = *header;
        j1939_large_msg_tx_list[i].header.priority = header->priority & 0x07;
        j1939_large_msg_tx_list[i].header.ext_data_page = !!header->ext_data_page;
        j1939_large_msg_tx_list[i].data = data;
        j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_WAIT_RTS;
        j1939_large_msg_tx_list[i].total_packets = (len + 6) / 7;
        j1939_large_msg_tx_list[i].cur_packets = 0;
        j1939_large_msg_tx_list[i].total_bytes = len;
        j1939_large_msg_tx_list[i].interval = interval;
        j1939_large_msg_tx_list[i].ts = 0;

        LOG(LOGLEVEL_INFO, "create large sending: [%02X->%02X] PGN:%06x, len:%u\n", header->src_addr, header->dst_addr, header->pgn, len);
        return i;
    }

    LOG(LOGLEVEL_ERROR, "create large sending failed: [%02X->%02X] PGN:%06x, len:%u\n", header->src_addr, header->dst_addr, header->pgn, len);
    return -1;
}
int j1939_destroy_large_msg_sending(int handle)
{
    if ((J1939_LARGE_MSG_TX_MAX <= handle) || (0 > handle))
        return -1;

    LOG(LOGLEVEL_INFO, "destroy large sending: [%02X->%02X] PGN:%06x\n",
                        j1939_large_msg_tx_list[handle].header.src_addr,
                        j1939_large_msg_tx_list[handle].header.dst_addr,
                        j1939_large_msg_tx_list[handle].header.pgn);

    if (J1939_LARGE_MSG_TX_STARTED(j1939_large_msg_tx_list[handle].state))
        j1939_send_tpcm_abort(j1939_large_msg_tx_list[handle].header.pgn,
                              j1939_large_msg_tx_list[handle].header.dst_addr,
                              j1939_large_msg_tx_list[handle].header.src_addr,
                              J1939_TPCM_ABORT_REASON_SYSTEM,
                              J1939_TPCM_ABORT_ROLE_ORI);

    j1939_large_msg_tx_list[handle].data = NULL;
    return 0;
}
int j1939_get_large_msg_sending_state(int handle)
{
    if ((J1939_LARGE_MSG_TX_MAX <= handle) || (0 > handle) || (NULL == j1939_large_msg_tx_list[handle].data))
        return -1;
    return j1939_large_msg_tx_list[handle].state;
}

static void do_msg_cb(const j1939_msg_header_t *header, const unsigned char *data, unsigned int len)
{
    int i;

    for (i = 0; i < J1939_MSG_CB_MAX; i++)
    {
        if (NULL == j1939_msg_cb_list[i].cb)
            break;
        if (   (header->pgn           != j1939_msg_cb_list[i].header.pgn)
            || (header->dst_addr      != j1939_msg_cb_list[i].header.dst_addr)
            || (header->src_addr      != j1939_msg_cb_list[i].header.src_addr)
            || (header->ext_data_page != j1939_msg_cb_list[i].header.ext_data_page))
            continue;

        j1939_msg_cb_list[i].cb(header, data, len);
    }
}

static void handle_tpdt(const j1939_msg_header_t *header, unsigned char *data, unsigned int len)
{
    j1939_tpdt_t *tpdt = (j1939_tpdt_t *)data;
    int i;
    unsigned char total_packets;
    unsigned char cur_packets;
    unsigned int total_bytes;
    unsigned int cur_bytes;
    unsigned char *buf;


    if ((J1939_PGN_TPDT != header->pgn) || (8 != len))
        return;

    for (i = 0; i < J1939_LARGE_MSG_CB_MAX; i++)
    {
        if (NULL == j1939_large_msg_cb_list[i].cb)
            break;
        if (   (j1939_large_msg_cb_list[i].header.dst_addr != header->dst_addr)
            || (j1939_large_msg_cb_list[i].header.src_addr != header->src_addr)
            || (0 == j1939_large_msg_cb_list[i].start))
            continue;

        total_packets = j1939_large_msg_cb_list[i].total_packets;
        cur_packets   = j1939_large_msg_cb_list[i].cur_packets + 1;
        total_bytes   = j1939_large_msg_cb_list[i].total_bytes;
        cur_bytes     = j1939_large_msg_cb_list[i].cur_bytes;
        buf           = j1939_large_msg_cb_list[i].buf;
        if (cur_packets != tpdt->seq_num)
        {
            LOG(LOGLEVEL_ERROR, "recv large msg error: [%02X->%02X] PGN:%06x, seq:%u expact_seq:%u\n",
                header->src_addr, header->dst_addr, j1939_large_msg_cb_list[i].header.pgn, tpdt->seq_num, cur_packets);
            continue;
        }

        j1939_large_msg_cb_list[i].cur_packets++;
        if (cur_packets < total_packets)
        {
            memcpy(&buf[cur_bytes], tpdt->data, 7);
            j1939_large_msg_cb_list[i].cur_bytes += 7;
            LOG(LOGLEVEL_DEBUG, "recv large msg: [%02X->%02X] PGN:%06x, seq:%u/%u\n",
                header->src_addr, header->dst_addr, j1939_large_msg_cb_list[i].header.pgn, tpdt->seq_num, total_packets);
        }
        else if (cur_packets == total_packets)
        {
            memcpy(&buf[cur_bytes], tpdt->data, total_bytes - cur_bytes);
            LOG(LOGLEVEL_INFO, "recv large msg OK: [%02X->%02X] PGN:%06x, bytes:%u packets:%u\n",
                header->src_addr, header->dst_addr, j1939_large_msg_cb_list[i].header.pgn, total_bytes, total_packets);
            j1939_send_tpcm_ack(j1939_large_msg_cb_list[i].header.pgn,
                                j1939_large_msg_cb_list[i].header.src_addr,
                                j1939_large_msg_cb_list[i].header.dst_addr,
                                total_packets, total_bytes);
            j1939_large_msg_cb_list[i].cb(&j1939_large_msg_cb_list[i].header, buf, total_bytes);
            j1939_large_msg_cb_list[i].start = 0;
        }
    }
}

static void handle_tpcm_rts(const j1939_msg_header_t *header, unsigned char *data, unsigned int len)
{
    j1939_tpcm_rts_t *tpcm_rts = (j1939_tpcm_rts_t *)data;
    unsigned int pgn;
    unsigned int total_bytes;
    unsigned char cts_sent = 0;
    int i;


    if ((J1939_PGN_TPCM != header->pgn) || (8 != len) || (J1939_TPCM_CONTROL_RTS != data[0]))
        return;

    pgn = tpcm_rts->pgn[0] | (tpcm_rts->pgn[1] << 8) | (tpcm_rts->pgn[2] << 16);
    total_bytes = tpcm_rts->total_bytes[0] | (tpcm_rts->total_bytes[1] << 8);
    if (   (8 >= total_bytes) || (255 * 7 < total_bytes)
        || (tpcm_rts->total_packets != (total_bytes + 6) / 7)
        || (tpcm_rts->total_packets > tpcm_rts->max_packets))
        return;

    for (i = 0; i < J1939_LARGE_MSG_CB_MAX; i++)
    {
        if (   (NULL == j1939_large_msg_cb_list[i].cb)
            || (j1939_large_msg_cb_list[i].header.dst_addr != header->dst_addr)
            || (j1939_large_msg_cb_list[i].header.src_addr != header->src_addr))
            continue;

        /* 相同的目的地址和源地址之间只能有一个连接 */
        if (j1939_large_msg_cb_list[i].header.pgn != pgn)
        {
            if (j1939_large_msg_cb_list[i].start)
            {
                j1939_large_msg_cb_list[i].start = 0;
                LOG(LOGLEVEL_INFO, "recv tpcm_rts, stop old conn: [%02X->%02X] PGN:%06x\n",
                    header->src_addr, header->dst_addr, j1939_large_msg_cb_list[i].header.pgn);
            }
            continue;
        }
        if (j1939_large_msg_cb_list[i].buf_size < total_bytes)
        {
            LOG(LOGLEVEL_INFO, "recv tpcm_rts [%02X->%02X] PGN:%06x, total size(%u) exceed cb buf size(%u)\n",
                header->src_addr, header->dst_addr, pgn, total_bytes, j1939_large_msg_cb_list[i].buf_size);
            j1939_large_msg_cb_list[i].start = 0;
            continue;
        }

        j1939_large_msg_cb_list[i].total_packets = tpcm_rts->total_packets;
        j1939_large_msg_cb_list[i].cur_packets = 0;
        j1939_large_msg_cb_list[i].total_bytes = total_bytes;
        j1939_large_msg_cb_list[i].cur_bytes = 0;
        j1939_large_msg_cb_list[i].start = 1;
        if (0 == cts_sent)
        {
            LOG(LOGLEVEL_INFO, "recv tpcm_rts: [%02X->%02X] PGN:%06x, size:%u packets:%u\n",
                header->src_addr, header->dst_addr, pgn, total_bytes, tpcm_rts->total_packets);
            cts_sent = 1;
            j1939_send_tpcm_cts(pgn, header->src_addr, header->dst_addr, tpcm_rts->total_packets, 1);
        }
    }
}

static void handle_tpcm_cts(const j1939_msg_header_t *header, const unsigned char *data, unsigned int len)
{
    j1939_tpcm_cts_t *tpcm_cts = (j1939_tpcm_cts_t *)data;
    unsigned int pgn;
    int i;


    if ((J1939_PGN_TPCM != header->pgn) || (8 != len) || (J1939_TPCM_CONTROL_CTS != data[0]))
        return;

    pgn = tpcm_cts->pgn[0] | (tpcm_cts->pgn[1] << 8) | (tpcm_cts->pgn[2] << 16);
    for (i = 0; i < J1939_LARGE_MSG_TX_MAX; i++)
    {
        if (   (NULL == j1939_large_msg_tx_list[i].data)
            || (j1939_large_msg_tx_list[i].header.src_addr != header->dst_addr)
            || (j1939_large_msg_tx_list[i].header.dst_addr != header->src_addr)
            || (j1939_large_msg_tx_list[i].header.pgn != pgn))
            continue;

        /* 简化协议 */
        LOG(LOGLEVEL_INFO, "recv tpcm_cts: [%02X->%02X] PGN:%06x, max:%u next:%u, state:%u\n",
            header->src_addr, header->dst_addr, pgn, tpcm_cts->max_packets, tpcm_cts->next_packet, j1939_large_msg_tx_list[i].state);
        if (J1939_LARGE_MSG_TX_STATE_WAIT_CTS != j1939_large_msg_tx_list[i].state)
            continue;

        j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_SENDING;
        j1939_large_msg_tx_list[i].ts = HAL_GetTick();
    }
}
static void handle_tpcm_ack(const j1939_msg_header_t *header, const unsigned char *data, unsigned int len)
{
    j1939_tpcm_ack_t *tpcm_ack = (j1939_tpcm_ack_t *)data;
    unsigned int pgn;
    int i;


    if ((J1939_PGN_TPCM != header->pgn) || (8 != len) || (J1939_TPCM_CONTROL_ACK != data[0]))
        return;

    pgn = tpcm_ack->pgn[0] | (tpcm_ack->pgn[1] << 8) | (tpcm_ack->pgn[2] << 16);
    for (i = 0; i < J1939_LARGE_MSG_TX_MAX; i++)
    {
        if (   (NULL == j1939_large_msg_tx_list[i].data)
            || (j1939_large_msg_tx_list[i].header.src_addr != header->dst_addr)
            || (j1939_large_msg_tx_list[i].header.dst_addr != header->src_addr)
            || (j1939_large_msg_tx_list[i].header.pgn != pgn))
            continue;

        LOG(LOGLEVEL_INFO, "recv tpcm_ack: [%02X->%02X] PGN:%06x, size:%u packets:%u, state:%u\n",
            header->src_addr, header->dst_addr, pgn, tpcm_ack->total_bytes[0] | tpcm_ack->total_bytes[1] << 8,
            tpcm_ack->total_packets, j1939_large_msg_tx_list[i].state);
        if ((J1939_LARGE_MSG_TX_STATE_WAIT_ACK != j1939_large_msg_tx_list[i].state))
            continue;

        j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_SUCCESS;
    }
}

static void handle_tpcm_abort(const j1939_msg_header_t *header, unsigned char *data, unsigned int len)
{
    j1939_tpcm_abort_t *tpcm_abort = (j1939_tpcm_abort_t *)data;
    unsigned int pgn;
    int i;


    if ((J1939_PGN_TPCM != header->pgn) || ((8 != len)) || (J1939_TPCM_CONTROL_ABORT != data[0]))
        return;

    pgn = tpcm_abort->pgn[0] | (tpcm_abort->pgn[1] << 8) | (tpcm_abort->pgn[2] << 16);
    if (J1939_TPCM_ABORT_ROLE_ORI == tpcm_abort->role)
    {
        for (i = 0; i < J1939_LARGE_MSG_CB_MAX; i++)
        {
            if (   (NULL == j1939_large_msg_cb_list[i].cb)
                || (j1939_large_msg_cb_list[i].header.dst_addr != header->dst_addr)
                || (j1939_large_msg_cb_list[i].header.src_addr != header->src_addr)
                || (j1939_large_msg_cb_list[i].header.pgn != pgn))
                continue;

            LOG(LOGLEVEL_INFO, "recv ORIGINATOR tpcm_abort: [%02X->%02X] PGN:%06x, reason:%u\n",
                header->src_addr, header->dst_addr, pgn, tpcm_abort->reason);
            j1939_large_msg_cb_list[i].start = 0;
        }
    }
    else
    {
        for (i = 0; i < J1939_LARGE_MSG_TX_MAX; i++)
        {
            if (   (NULL == j1939_large_msg_tx_list[i].data)
                || (j1939_large_msg_tx_list[i].header.src_addr != header->dst_addr)
                || (j1939_large_msg_tx_list[i].header.dst_addr != header->src_addr)
                || (j1939_large_msg_tx_list[i].header.pgn != pgn))
                continue;

            LOG(LOGLEVEL_INFO, "recv RESPONDER tpcm_abort: [%02X->%02X] PGN:%06x, reason:%u state:%u\n",
                header->src_addr, header->dst_addr, pgn, tpcm_abort->reason, j1939_large_msg_tx_list[i].state);
            if (!J1939_LARGE_MSG_TX_STARTED(j1939_large_msg_tx_list[i].state))
                continue;

            j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_ABORT;
        }
    }
}


#define J1939_SERVER_TASK_PERIOD  5  /* ms */
void j1939_service_task(void)
{
    static unsigned int last_ts = 0;
    unsigned int current_ts;
    can_msg_t *can_msg;
    j1939_msg_header_t header;
    j1939_pdu_t *pdu;
    int i, j;
    unsigned int pgn;
    unsigned char dst;
    unsigned char src;
    unsigned char state;
    unsigned char total_packets;
    unsigned char cur_packets;


    current_ts = HAL_GetTick();
    if (J1939_SERVER_TASK_PERIOD > ((unsigned int)(current_ts - last_ts)))
        return;
    last_ts = current_ts;

    /* RX */
    while (can_msg_head != can_msg_tail)
    {
        can_msg = &can_msg_ring_buf[can_msg_head];
        if ((0 == can_msg->ide) || (can_msg->rtr))
            goto CONTINUE;

        pdu = (j1939_pdu_t *)can_msg->extension_id;
        if (0xf0 <= pdu->pf)
        {
            header.pgn = (pdu->dp << 16) | (pdu->pf << 8) | pdu->ps;
            header.dst_addr = 0;
        }
        else
        {
            header.pgn = (pdu->dp << 16) | (pdu->pf << 8);
            header.dst_addr = pdu->ps;
        }
        header.priority = pdu->priority;
        header.ext_data_page = pdu->edp;
        header.src_addr = pdu->sa;

        LOG(LOGLEVEL_DEBUG, "recv can msg: [%02X->%02X] PGN:%06x, len:%u data:%02x %02x %02x %02x %02x %02x %02x %02x\n",
            header.src_addr, header.dst_addr, header.pgn, can_msg->dlc,
            can_msg->data[0], can_msg->data[1], can_msg->data[2], can_msg->data[3],
            can_msg->data[4], can_msg->data[5], can_msg->data[6], can_msg->data[7]);

        do_msg_cb(&header, can_msg->data, can_msg->dlc);
        handle_tpdt(&header, can_msg->data, can_msg->dlc);
        handle_tpcm_rts(&header, can_msg->data, can_msg->dlc);
        handle_tpcm_cts(&header, can_msg->data, can_msg->dlc);
        handle_tpcm_ack(&header, can_msg->data, can_msg->dlc);
        handle_tpcm_abort(&header, can_msg->data, can_msg->dlc);

    CONTINUE:
        can_msg_head++;
        if (CAN_MSG_COUNT_MAX <= can_msg_head)
            can_msg_head = 0;
    }

    /* TX */
    for (i = 0; i < J1939_LARGE_MSG_TX_MAX; i++)
    {
        if (NULL == j1939_large_msg_tx_list[i].data)
            continue;

        pgn = j1939_large_msg_tx_list[i].header.pgn;
        dst = j1939_large_msg_tx_list[i].header.dst_addr;
        src = j1939_large_msg_tx_list[i].header.src_addr;
        state = j1939_large_msg_tx_list[i].state;
        total_packets = j1939_large_msg_tx_list[i].total_packets;
        cur_packets = j1939_large_msg_tx_list[i].cur_packets;
        if (J1939_LARGE_MSG_TX_STATE_WAIT_RTS == state)
        {
            /* 相同目的和源地址只能有一个连接 */
            for (j = 0; j < J1939_LARGE_MSG_TX_MAX; j++)
            {
                if (   (dst == j1939_large_msg_tx_list[j].header.dst_addr)
                    && (src == j1939_large_msg_tx_list[j].header.src_addr)
                    && (J1939_LARGE_MSG_TX_STARTED(state)))
                    break;
            }
            if (J1939_LARGE_MSG_TX_MAX > j)
                continue;

            if (j1939_send_tpcm_rts(pgn, dst, src, j1939_large_msg_tx_list[i].total_bytes))
            {
                j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_ERROR;
                continue;
            }
            j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_WAIT_CTS;
            j1939_large_msg_tx_list[i].ts = HAL_GetTick();
        }
        else if (   (J1939_LARGE_MSG_TX_STATE_WAIT_CTS == state)
                 || (J1939_LARGE_MSG_TX_STATE_WAIT_ACK == state))
        {
            if (J1939_TP_TIME_T3 > ((unsigned int)(HAL_GetTick() - j1939_large_msg_tx_list[i].ts)))
                continue;

            j1939_send_tpcm_abort(pgn, dst, src, J1939_TPCM_ABORT_REASON_TIMEOUT, J1939_TPCM_ABORT_ROLE_ORI);
            j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_TIMEOUT;
        }
        else if (J1939_LARGE_MSG_TX_STATE_SENDING == state)
        {
            if (j1939_large_msg_tx_list[i].interval > ((unsigned int)(HAL_GetTick() - j1939_large_msg_tx_list[i].ts)))
                continue;

            if (cur_packets < total_packets - 1)
            {
                j1939_send_tpdt(dst, src, cur_packets + 1, &j1939_large_msg_tx_list[i].data[cur_packets * 7], 7);
                j1939_large_msg_tx_list[i].cur_packets++;
            }
            else
            {
                j1939_send_tpdt(dst, src, cur_packets + 1, &j1939_large_msg_tx_list[i].data[cur_packets * 7],
                                j1939_large_msg_tx_list[i].total_bytes - cur_packets * 7);
                j1939_large_msg_tx_list[i].state = J1939_LARGE_MSG_TX_STATE_WAIT_ACK;
            }
            j1939_large_msg_tx_list[i].ts = HAL_GetTick();
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef header;
    can_msg_t msg;

    if (HAL_OK != HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, msg.data))
        return;

    msg.chn = 1;
    if (CAN_ID_EXT == header.IDE)
    {
        msg.ide = 1;
        msg.extension_id[0] = (header.ExtId >> 24) & 0xff;
        msg.extension_id[1] = (header.ExtId >> 16) & 0xff;
        msg.extension_id[2] = (header.ExtId >> 8) & 0xff;
        msg.extension_id[3] = header.ExtId & 0xff;
    }
    else
    {
        msg.ide = 0;
        msg.base_id[0] = (header.StdId >> 8) & 0xff;
        msg.base_id[1] = header.StdId & 0xff;
    }
    if (CAN_RTR_REMOTE == header.RTR)
        msg.rtr = 1;
    else
        msg.rtr = 0;
    msg.dlc = header.DLC;
    j1939_put_can_msg(&msg);
}
