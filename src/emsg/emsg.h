#ifndef _EMSG_H_
#define _EMSG_H_

#include <stdint.h>
#include <stddef.h>


typedef struct
{
    uint8_t sof[2];
    uint8_t src;
    uint8_t dst;
    uint8_t msg_id[2];
    uint8_t len[2];
    uint8_t payload[0];
}emsg_header_t;

#define EMSG_DEVICE_ADDR_LOCAL  0

#define EMSG_PAYLOAD_LEN_MAX  300
#define EMSG_MSG_LEN_MAX      (sizeof(emsg_header_t) + EMSG_PAYLOAD_LEN_MAX + 4)
#define EMSG_ENCODE_LEN_MAX   ((((EMSG_MSG_LEN_MAX * 2) + 7) / 8) * 8)

typedef uint32_t (*emsg_sender_t)(const uint8_t *data, uint32_t len);

typedef struct
{
    uint8_t *conn_id;
    uint8_t dst_addr;
    uint8_t route_count;
    uint8_t *routing_table;
    emsg_sender_t sender;
}emsg_conn_cfg_t;

typedef void (*emsg_cb_t)(uint8_t src_addr, uint16_t msg_id, const void *data, uint16_t len);

extern int emsg_init(void);
extern int emsg_register_cb(uint16_t msg_id, emsg_cb_t cb, unsigned char local_only);
extern int emsg_recv(uint8_t conn_id, const void *data, size_t len);
extern int emsg_send(uint8_t dst_addr, uint16_t msg_id, const void *data, size_t len);


#endif
