/**
 *
 * EMBEDDED_MESSAGE
 *
 * header: (big endian)
 * ==================================================
 * | SOF | SRC | DST | MSG_ID | LEN | PAYLOAD | CRC |
 * +-----+-----+-----+--------+-----+---------+-----+
 * |  2  |  1  |  1  |    2   |  2  |    n    |  4  |
 * ==================================================
 * len: raw payload length
 * crc: raw header + payload crc
 *
 * ESCAPE
 * raw               encoded
 * aa 55        <->  aa a5 55
 * aa a5        <->  aa a5 a5
 * aa a5 a5     <->  aa a5 a5 a5 a5
 * aa a5 a5 a5  <->  aa a5 a5 a5 a5 a5 a5
 *              ...
 *
 */

#include "emsg.h"
#include "crc.h"
#include "log.h"
#include "emsg_config.h"




#define LOGLEVEL_ERROR  1
#define LOGLEVEL_INFO   2
#define LOGLEVEL_DEBUG  3
#define LOG(level, fmt, arg...)  do{if((level) <= emsg_log_level)log_printf("--EMSG-- " fmt, ##arg);}while(0)

#define EMSG_SOF_1        0xaa
#define EMSG_SOF_2        0x55
#define EMSG_ESCAPE_CHAR  0xa5

#ifndef EMSG_CB_MAX
#define EMSG_CB_MAX  64
#endif


typedef struct
{
    uint32_t decoded_len;
    uint8_t escape_state;
    uint8_t pad;
    uint16_t errors;
}decoder_state_t;
typedef struct
{
    uint8_t dst_addr;
    uint8_t pad[2];
    uint8_t route_count;
    uint8_t *routing_table;
    uint8_t decode_buf[EMSG_MSG_LEN_MAX];
    emsg_sender_t sender;
    decoder_state_t decoder_state;
}emsg_conn_t;

typedef struct
{
    emsg_cb_t cb;  /* 同时用来标记是否已用 */
    uint16_t msg_id;
    uint8_t local_only;
    uint8_t pad;
}emsg_cb_info_t;

uint8_t emsg_log_level = LOGLEVEL_ERROR;

static emsg_conn_t emsg_conn_list[EMSG_CONN_CFG_COUNT] = {0};
static emsg_cb_info_t emsg_cb_list[EMSG_CB_MAX] = {0};


int emsg_init(void)
{
    int i;

    for (i = 0; i < EMSG_CONN_CFG_COUNT; i++)
    {
        if (EMSG_DEVICE_ADDR_LOCAL == emsg_conn_cfg_list[i].dst_addr)
        {
            LOG(LOGLEVEL_ERROR, "conn(%u) dst_addr cannot be LOCAL_ADDR(%u or %u) !",
                i, EMSG_DEVICE_ADDR_LOCAL, local_device_addr);
            return -1;
        }
        emsg_conn_list[i].dst_addr = emsg_conn_cfg_list[i].dst_addr;

        if ((0 == emsg_conn_cfg_list[i].route_count) || (NULL == emsg_conn_cfg_list[i].routing_table))
        {
            emsg_conn_list[i].route_count = 0;
            emsg_conn_list[i].routing_table = NULL;
        }
        else
        {
            emsg_conn_list[i].route_count = emsg_conn_cfg_list[i].route_count;
            emsg_conn_list[i].routing_table = emsg_conn_cfg_list[i].routing_table;
        }
        emsg_conn_list[i].sender = emsg_conn_cfg_list[i].sender;
        *emsg_conn_cfg_list[i].conn_id = i;
    }

    return 0;
}

int emsg_register_cb(uint16_t msg_id, emsg_cb_t cb, uint8_t local_only)
{
    int i;

    if (NULL == cb)
        return -1;

    for (i = 0; i < EMSG_CB_MAX; i++)
    {
        if (emsg_cb_list[i].cb)
            continue;
        emsg_cb_list[i].msg_id = msg_id;
        emsg_cb_list[i].local_only = local_only;
        emsg_cb_list[i].cb = cb;
        return 0;
    }

    LOG(LOGLEVEL_ERROR, "register cb failed, count exceeds the max(%u) !\n", EMSG_CB_MAX);
    return -1;
}

static void do_msg_cb(uint8_t src_addr, uint16_t msg_id, const void *data, uint16_t len)
{
    int i;

    if (0 == len)
        data = NULL;

    for (i = 0; i < EMSG_CB_MAX; i++)
    {
        if (NULL == emsg_cb_list[i].cb)
            return;
        if (msg_id != emsg_cb_list[i].msg_id)
            continue;
        if (emsg_cb_list[i].local_only && (EMSG_DEVICE_ADDR_LOCAL != src_addr))
            continue;
        emsg_cb_list[i].cb(src_addr, msg_id, data, len);
    }
}

int emsg_recv(uint8_t conn_id, const void *data, size_t len)
{
    uint32_t decoded_len;
    uint8_t escape_state;  /* 1:aa 2:aaa5 3:aa[a5a5] 4:aa[a5a5]a5 */
    uint16_t errors;
    emsg_header_t *header;
    uint16_t payload_len;
    uint32_t msg_len = 0;
    uint16_t msg_id;
    uint8_t _data;
    uint8_t *pcrc;
    uint32_t crc;
    uint32_t i;


    if (EMSG_CONN_CFG_COUNT <= conn_id)
        return -1;
    if ((NULL == data) && len)
        return -1;
    if (0 == len)
        return 0;

    if (LOGLEVEL_DEBUG <= emsg_log_level)
    {
        LOG(LOGLEVEL_DEBUG, "conn(%d) recv msg(%zu): ", conn_id, len);
        for (i = 0; i < len; i++)
            log_printf("%02x ", ((uint8_t *)data)[i]);
        log_printf("\n");
    }

    decoded_len  = emsg_conn_list[conn_id].decoder_state.decoded_len;
    escape_state = emsg_conn_list[conn_id].decoder_state.escape_state;
    errors       = emsg_conn_list[conn_id].decoder_state.errors;
    header       = (emsg_header_t *)emsg_conn_list[conn_id].decode_buf;

    while (len--)
    {
        _data = *((uint8_t *)data);
        data = ((uint8_t *)data) + 1;

        if (2 > decoded_len)
        {
            if (0 == decoded_len)
            {
                if (EMSG_SOF_1 == _data)
                {
                    /* header->sof[0] = EMSG_SOF_1; */
                    decoded_len = 1;
                }
            }
            else if (1 == decoded_len)
            {
                if (EMSG_SOF_2 == _data)
                {
                    /* header->sof[1] = EMSG_SOF_2; */
                    decoded_len = 2;
                    escape_state = 0;
                    msg_len = 0;
                }
                else
                {
                    decoded_len = 0;
                }
            }
            continue;
        }

        if (0 == escape_state)
        {
            if (EMSG_SOF_1 == _data)
                escape_state = 1;
        }
        else if (1 == escape_state)
        {
            if (EMSG_ESCAPE_CHAR == _data)
            {
                escape_state = 2;
            }
            else if (EMSG_SOF_1 == _data)
            {
                ;
            }
            else if (EMSG_SOF_2 == _data)
            {
                decoded_len = 2;
                escape_state = 0;
                msg_len = 0;
                emsg_conn_list[conn_id].decoder_state.errors++;
                continue;
            }
            else
            {
                escape_state = 0;
            }
        }
        else if (2 == escape_state)
        {
            if (EMSG_ESCAPE_CHAR == _data)
            {
                escape_state = 3;
                decoded_len--;
            }
            else if (EMSG_SOF_2 == _data)
            {
                escape_state = 0;
                decoded_len--;
            }
            else if (EMSG_SOF_1 == _data)
            {
                decoded_len = 1;
                emsg_conn_list[conn_id].decoder_state.errors++;
                continue;
            }
            else
            {
                decoded_len = 0;
                emsg_conn_list[conn_id].decoder_state.errors++;
                continue;
            }
        }
        else if (3 == escape_state)
        {
            if (EMSG_ESCAPE_CHAR == _data)
            {
                escape_state = 4;
            }
            else if (EMSG_SOF_1 == _data)
            {
                escape_state = 1;
            }
            else
            {
                escape_state = 0;
            }
        }
        else if (4 == escape_state)
        {
            if (EMSG_ESCAPE_CHAR == _data)
            {
                escape_state = 3;
                decoded_len--;
            }
            else if (EMSG_SOF_1 == _data)
            {
                decoded_len = 1;
                emsg_conn_list[conn_id].decoder_state.errors++;
                continue;
            }
            else
            {
                decoded_len = 0;
                emsg_conn_list[conn_id].decoder_state.errors++;
                continue;
            }
        }

        ((uint8_t *)header)[decoded_len++] = _data;
        if (0 == msg_len)
        {
            if (   ((sizeof(emsg_header_t) <= decoded_len) && (2 != escape_state) && (4 != escape_state))
                || (sizeof(emsg_header_t) < decoded_len))
            {
                payload_len = (header->len[0] << 8) | header->len[1];
                if (EMSG_PAYLOAD_LEN_MAX < payload_len)
                {
                    LOG(LOGLEVEL_ERROR, "payload len(%u) exceeds the max(%u), ignore this msg !", payload_len, EMSG_PAYLOAD_LEN_MAX);
                    decoded_len = (((uint8_t *)header)[decoded_len - 1] == EMSG_SOF_1)? 1: 0;
                    emsg_conn_list[conn_id].decoder_state.errors++;
                    continue;
                }
                msg_len = sizeof(emsg_header_t) + payload_len + 4;
            }
        }
        if ((0 == msg_len) || (msg_len > decoded_len) || (2 == escape_state) || (4 == escape_state))
            continue;

        header->sof[0] = EMSG_SOF_1;
        header->sof[1] = EMSG_SOF_2;
        pcrc = ((uint8_t *)header) + msg_len - 4;

        if (LOGLEVEL_DEBUG <= emsg_log_level)
        {
            LOG(LOGLEVEL_DEBUG, "    header: ");
            for (i = 0; i < sizeof(emsg_header_t); i++)
                log_printf("%02x ", ((uint8_t *)header)[i]);
            log_printf("\n");

            LOG(LOGLEVEL_DEBUG, "    data  : ");
            for (i = 0; i < payload_len; i++)
                log_printf("%02x ", header->payload[i]);
            log_printf("\n");

            LOG(LOGLEVEL_DEBUG, "    crc   : %02x %02x %02x %02x\n", pcrc[0], pcrc[1], pcrc[2], pcrc[3]);
        }

        crc = crc32_cksum(NULL, header, msg_len - 4);
        if (   (pcrc[0] != (crc >> 24))
            || (pcrc[1] != ((crc >> 16) & 0xff))
            || (pcrc[2] != ((crc >> 8) & 0xff))
            || (pcrc[3] != (crc & 0xff)))
        {
            LOG(LOGLEVEL_ERROR, "conn(%u) recv msg crc error !\n", conn_id);
            decoded_len = (((uint8_t *)header)[decoded_len - 1] == EMSG_SOF_1)? 1: 0;
            emsg_conn_list[conn_id].decoder_state.errors++;
            continue;
        }
        if (EMSG_DEVICE_ADDR_LOCAL == header->src)
        {
            LOG(LOGLEVEL_ERROR, "conn(%u) recv msg failed, src_addr cannot be LOCAL_ADDR(%u) !\n", conn_id, EMSG_DEVICE_ADDR_LOCAL);
            decoded_len = (((uint8_t *)header)[decoded_len - 1] == EMSG_SOF_1)? 1: 0;
            continue;
        }

        msg_id = (header->msg_id[0] << 8) | header->msg_id[1];
        LOG(LOGLEVEL_INFO, "recv msg, src:%u dst:%u id:%u len:%u\n", header->src, local_device_addr, msg_id, payload_len);
        decoded_len = 0;
        if (header->dst == local_device_addr)
        {
            do_msg_cb(header->src, msg_id, header->payload, payload_len);
        }
        else
        {
            LOG(LOGLEVEL_INFO, "do route:\n");
            emsg_send(header->dst, msg_id, header->payload, payload_len);
        }
    }

    emsg_conn_list[conn_id].decoder_state.decoded_len = decoded_len;
    emsg_conn_list[conn_id].decoder_state.escape_state = escape_state;
    if (errors != emsg_conn_list[conn_id].decoder_state.errors)
        LOG(LOGLEVEL_ERROR, "conn(%u) decode errors: %u !\n", conn_id, emsg_conn_list[conn_id].decoder_state.errors);
    return 0;
}

/* state 1:aa 2:aa[a5] */
static uint32_t escape_encode(uint8_t *state, const uint8_t *src, uint32_t src_len, uint8_t *dst)
{
    uint8_t _state = *state;
    uint8_t data;
    uint8_t *_dst = dst;

    while (src_len--)
    {
        data = *src++;

        if (EMSG_SOF_1 == data)
        {
            _state = 1;
        }
        else if (0 == _state)
        {
            ;
        }
        else if (1 == _state)
        {
            if (EMSG_SOF_2 == data)
            {
                *_dst++ = EMSG_ESCAPE_CHAR;
                _state = 0;
            }
            else if (EMSG_ESCAPE_CHAR == data)
            {
                *_dst++ = EMSG_ESCAPE_CHAR;
                _state = 2;
            }
            else
            {
                _state = 0;
            }
        }
        else if (2 == _state)
        {
            if (EMSG_ESCAPE_CHAR == data)
                *_dst++ = EMSG_ESCAPE_CHAR;
            else
                _state = 0;
        }
        *_dst++ = data;
    }

    *state = _state;
    return (uint32_t)(_dst - dst);
}

int emsg_send(uint8_t dst_addr, uint16_t msg_id, const void *data, size_t len)
{
    uint8_t buf[EMSG_ENCODE_LEN_MAX];
    emsg_header_t header;
    uint8_t escape_state = 0;
    uint32_t _len;
    uint32_t crc;
    uint8_t crc_buf[4];
    uint32_t i, j;


    if ((NULL == data) && len)
        return -1;
    if (EMSG_PAYLOAD_LEN_MAX < len)
    {
        LOG(LOGLEVEL_ERROR, "send failed, data len(%zu) exceeds the max(%u), dst_addr:%u msg_id:%u !\n",
            len, EMSG_PAYLOAD_LEN_MAX, dst_addr, msg_id);
        return -1;
    }

    if ((EMSG_DEVICE_ADDR_LOCAL == dst_addr) || (local_device_addr == dst_addr))
    {
        LOG(LOGLEVEL_INFO, "send local msg, src:%u dst:%u id:%u len:%zu\n", local_device_addr, dst_addr, msg_id, len);
        do_msg_cb(EMSG_DEVICE_ADDR_LOCAL, msg_id, data, (uint16_t)len);
        return 0;
    }
    LOG(LOGLEVEL_INFO, "send msg, src:%u dst:%u id:%u len:%zu\n", local_device_addr, dst_addr, msg_id, len);

    header.sof[0] = EMSG_SOF_1;
    header.sof[1] = EMSG_SOF_2;
    header.src = local_device_addr;
    header.dst = dst_addr;
    header.msg_id[0] = msg_id >> 8;
    header.msg_id[1] = msg_id & 0xff;
    header.len[0] = (len >> 8) & 0xff;
    header.len[1] = len & 0xff;
    crc = crc32_cksum(NULL, &header, sizeof(emsg_header_t));
    crc = crc32_cksum(&crc, data, len);
    crc_buf[0] = (crc >> 24) & 0xff;
    crc_buf[1] = (crc >> 16) & 0xff;
    crc_buf[2] = (crc >> 8) & 0xff;
    crc_buf[3] = crc & 0xff;

    buf[0] = EMSG_SOF_1;
    buf[1] = EMSG_SOF_2;
    _len = 2;
    _len += escape_encode(&escape_state, ((uint8_t *)&header) + 2, (uint32_t)(sizeof(emsg_header_t) - 2), buf + 2);
    _len += escape_encode(&escape_state, data, (uint32_t)len, buf + _len);
    _len += escape_encode(&escape_state, crc_buf, (uint32_t)sizeof(crc_buf), buf + _len);

    if (LOGLEVEL_DEBUG <= emsg_log_level)
    {
        LOG(LOGLEVEL_DEBUG, "    header: ");
        for (i = 0; i < sizeof(emsg_header_t); i++)
            log_printf("%02x ", ((uint8_t *)&header)[i]);
        log_printf("\n");

        LOG(LOGLEVEL_DEBUG, "    data  : ");
        for (i = 0; i < len; i++)
            log_printf("%02x ", ((uint8_t *)data)[i]);
        log_printf("\n");

        LOG(LOGLEVEL_DEBUG, "    crc   : %02x %02x %02x %02x\n", crc_buf[0], crc_buf[1], crc_buf[2], crc_buf[3]);

        LOG(LOGLEVEL_DEBUG, "    encode: ");
        for (i = 0; i < _len; i++)
            log_printf("%02x ", buf[i]);
        log_printf("\n");
    }

    for (i = 0; i < EMSG_CONN_CFG_COUNT; i++)
    {
        if (dst_addr != emsg_conn_list[i].dst_addr)
            continue;
        if (emsg_conn_list[i].sender)
            emsg_conn_list[i].sender(buf, _len);
        return 0;
    }
    for (i = 0; i < EMSG_CONN_CFG_COUNT; i++)
    {
        if (0 == emsg_conn_list[i].route_count)
            continue;
        for (j = 0; j < emsg_conn_list[i].route_count; j++)
        {
            if (dst_addr != emsg_conn_list[i].routing_table[j])
                continue;
            if (emsg_conn_list[i].sender)
                emsg_conn_list[i].sender(buf, _len);
            return 0;
        }
    }

    LOG(LOGLEVEL_ERROR, "send failed, dst addr(%u) is not configured !\n", dst_addr);
    return -1;
}
