#ifndef _EMSG_CONFIG_H_
#define _EMSG_CONFIG_H_

#include "msg_define.h"
#include "emsg_user.h"


static uint8_t local_device_addr = DEVICE_ADDR_A;


emsg_conn_cfg_t emsg_conn_cfg_list[] =
{
    {&uart3_conn_id, DEVICE_ADDR_B, 0, NULL, uart3_sender}
};
#define EMSG_CONN_CFG_COUNT  (sizeof(emsg_conn_cfg_list) / sizeof(emsg_conn_cfg_list[0]))


#endif
