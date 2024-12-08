#ifndef _J1939_SERVICE_H_
#define _J1939_SERVICE_H_


#define J1939_DEFUALT_PRIORITY 0x06
#define J1939_DEFUALT_CONTROL_PRIORITY 0x03

#define J1939_PGN_RQST    0x00EA00
#define J1939_PGN_ACKM    0x00E800
#define J1939_PGN_PROPA   0x00EF00
#define J1939_PGN_PROPA2  0x01EF00
#define J1939_PGN_RQST2   0x00C900
#define J1939_PGN_XFER    0x00CA00
#define J1939_PGN_TPCM    0x00EC00
#define J1939_PGN_TPDT    0x00EB00

#define J1939_ADDR_GLOBAL  0xFF

#define J1939_TP_TIME_TR  200  /* Response Time */
#define J1939_TP_TIME_TH  500  /* Hold Time */
#define J1939_TP_TIME_T1  750
#define J1939_TP_TIME_T2  1250
#define J1939_TP_TIME_T3  1250
#define J1939_TP_TIME_T4  1050

typedef struct
{
    unsigned char pgn[3];
}j1939_rqst_t;


#define J1939_ACKM_CONTROL_POSITIVE             0
#define J1939_ACKM_CONTROL_NEGATIVE             1
#define J1939_ACKM_CONTROL_DENIED               2
#define J1939_ACKM_CONTROL_BUSY                 3
#define J1939_ACKM_CONTROL_REQ2_1BYTE_POS     128
#define J1939_ACKM_CONTROL_REQ2_1BYTE_NEG     129
#define J1939_ACKM_CONTROL_REQ2_1BYTE_DENIED  130
#define J1939_ACKM_CONTROL_REQ2_1BYTE_BUSY    131
#define J1939_ACKM_CONTROL_REQ2_2BYTE_POS     144
#define J1939_ACKM_CONTROL_REQ2_2BYTE_NEG     145
#define J1939_ACKM_CONTROL_REQ2_2BYTE_DENIED  146
#define J1939_ACKM_CONTROL_REQ2_2BYTE_BUSY    147
#define J1939_ACKM_CONTROL_REQ2_3BYTE_POS     160
#define J1939_ACKM_CONTROL_REQ2_3BYTE_NEG     161
#define J1939_ACKM_CONTROL_REQ2_3BYTE_DENIED  162
#define J1939_ACKM_CONTROL_REQ2_3BYTE_BUSY    163
#define J1939_ACKM_CONTROL_REQ2_3BYTE_IGNORE  255
typedef struct
{
    unsigned char control;
    unsigned char group_fun;
    unsigned char rsv[2];  /* 0xFF */
    unsigned char ori_addr;
    unsigned char pgn[3];
}j1939_ackm_t;


#define J1939_TPCM_CONTROL_RTS    0x10
#define J1939_TPCM_CONTROL_CTS    0x11
#define J1939_TPCM_CONTROL_ACK    0x13
#define J1939_TPCM_CONTROL_BAM    0x20
#define J1939_TPCM_CONTROL_ABORT  0xFF
typedef struct
{
    unsigned char control;  /* J1939_TPCM_CONTROL_RTS  0x10 */
    unsigned char total_bytes[2];
    unsigned char total_packets;
    unsigned char max_packets;
    unsigned char pgn[3];
}j1939_tpcm_rts_t;

typedef struct
{
    unsigned char control;  /* J1939_TPCM_CONTROL_CTS  0x11 */
    unsigned char max_packets;
    unsigned char next_packet;
    unsigned char rsv[2];  /* 0xFF */
    unsigned char pgn[3];
}j1939_tpcm_cts_t;

typedef struct
{
    unsigned char control;  /* J1939_TPCM_CONTROL_ACK  0x13 */
    unsigned char total_bytes[2];
    unsigned char total_packets;
    unsigned char rsv;  /* 0xFF */
    unsigned char pgn[3];
}j1939_tpcm_ack_t;

#define J1939_TPCM_ABORT_REASON_CONN_EXISTS      1
#define J1939_TPCM_ABORT_REASON_SYSTEM           2
#define J1939_TPCM_ABORT_REASON_TIMEOUT          3
#define J1939_TPCM_ABORT_REASON_CTS              4
#define J1939_TPCM_ABORT_REASON_RETRANSMIT       5
#define J1939_TPCM_ABORT_REASON_ERROR_DATA       6
#define J1939_TPCM_ABORT_REASON_BAD_SEQ          7
#define J1939_TPCM_ABORT_REASON_DUPLICATE_SEQ    8
#define J1939_TPCM_ABORT_REASON_ERROR_SIZE       9
#define J1939_TPCM_ABORT_REASON_OTHER          250
#define J1939_TPCM_ABORT_ROLE_ORI   0
#define J1939_TPCM_ABORT_ROLE_RESP  1
#define J1939_TPCM_ABORT_ROLE_NULL  3
typedef struct
{
    unsigned char control;  /* J1939_TPCM_CONTROL_ABORT  0xFF */
    unsigned char reason;
    unsigned char role;
    unsigned char rsv[2];  /* 0xFF */
    unsigned char pgn[3];
}j1939_tpcm_abort_t;


typedef struct
{
    unsigned char seq_num;
    unsigned char data[7];
}j1939_tpdt_t;




typedef struct
{
    unsigned char chn;
    unsigned char ide;  /* id扩展 */
    union
    {
        unsigned char base_id[2];
        unsigned char extension_id[4];
    };
    unsigned char rtr;  /* 远程传输请求 */
    unsigned char dlc;  /* 数据长度 */
    unsigned char data[8];
}can_msg_t;


typedef struct
{
    unsigned int pgn;
    unsigned char priority;
    unsigned char ext_data_page;
    unsigned char dst_addr;
    unsigned char src_addr;
}j1939_msg_header_t;


typedef int (*j1939_msg_cb_t)(const j1939_msg_header_t *header, const unsigned char *data, unsigned int len);


extern void j1939_task(void);
extern int j1939_recv_can_msg(const can_msg_t *msg);
extern int j1939_send_msg(const j1939_msg_header_t *header, const unsigned char *data, unsigned char len);
extern int j1939_send_rqst(const j1939_msg_header_t *header);
extern int j1939_send_ackm(const j1939_msg_header_t *header, unsigned char ack, unsigned char group_fun, unsigned char ori_addr);
extern int j1939_register_msg_cb(const j1939_msg_header_t *header, j1939_msg_cb_t cb);
extern int j1939_register_large_msg_cb(const j1939_msg_header_t *header, unsigned char *buf, unsigned int size, j1939_msg_cb_t cb);
extern int j1939_create_large_msg_sending(const j1939_msg_header_t *header, unsigned char *data, unsigned int len, unsigned int interval);
extern int j1939_destroy_large_msg_sending(int handle);

#define J1939_LARGE_MSG_TX_STATE_WAIT_RTS    0
#define J1939_LARGE_MSG_TX_STATE_WAIT_CTS    1
#define J1939_LARGE_MSG_TX_STATE_SENDING     2
#define J1939_LARGE_MSG_TX_STATE_WAIT_ACK    3
#define J1939_LARGE_MSG_TX_STATE_SUCCESS     4
#define J1939_LARGE_MSG_TX_STATE_ERROR       5
#define J1939_LARGE_MSG_TX_STATE_ABORT       6
#define J1939_LARGE_MSG_TX_STATE_TIMEOUT     7
#define J1939_LARGE_MSG_TX_STARTED(state) (   (J1939_LARGE_MSG_TX_STATE_WAIT_CTS == (state)) \
                                           || (J1939_LARGE_MSG_TX_STATE_SENDING  == (state)) \
                                           || (J1939_LARGE_MSG_TX_STATE_WAIT_ACK == (state)))
#define J1939_LARGE_MSG_TX_FINISHED(state) (   (J1939_LARGE_MSG_TX_STATE_SUCCESS == (state)) \
                                            || (J1939_LARGE_MSG_TX_STATE_ERROR   == (state)) \
                                            || (J1939_LARGE_MSG_TX_STATE_ABORT   == (state)) \
                                            || (J1939_LARGE_MSG_TX_STATE_TIMEOUT == (state)))
extern int j1939_get_large_msg_sending_state(int handle);


#endif
