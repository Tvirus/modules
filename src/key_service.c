/*
      +------+    +------+
      | gpio |    | adc  | <--- 不同类型的按键有独立的轮询
      | 按键 |    | 按键 |      当状态刷新后调用状态机
      +------+    +------+      新老状态的存储由状态机负责
         ||          ||
         \/          \/
  +---------------------------+
  |   按键状态机(key_state)   |
  +---------------------------+


状态转移图:
                                   长按
                      |----------------->---------------|
                      |                                 v
  +------+  按下   +------+  连击   +------+  长按   +------+
  | 空闲 | ------> | 单击 | ------> | 双击 | ------> | 长按 |
  +------+         +------+         +------+         +------+
     ^               |抬起            |抬起            |抬起
     |               v                v                v
     +-------<-------+--------<-------+--------<-------+

1. 单击后抬起: 单击事件
2. 单击后不抬起长按: 单击事件 + 长按事件
3. 双击后抬起: 双击事件
4. 双击后不抬起长按: 双击事件 + 长按事件
5. 单击、连击或长按后抬起: 可选抬起事件
*/

#include "key_service.h"
#include "stm32l4xx_hal.h"
#include <string.h>




typedef struct
{
    unsigned char key_id;
    unsigned char key_type;
    unsigned char click_state;          /* 0:空闲, n:已n连击 */
    unsigned char long_press_state;     /* 0:空闲, 1:已长按 */
    signed int new_value;
    signed int old_value;               /* 按键上一个值 */
    unsigned int timestamp;             /* 最后一次值发生变化的时间，用于防抖，ms */
    unsigned int last_press_timestamp;  /* 针对普通按键，最后一次按下的时间，用于判断连击，ms */
    key_cfg_t key_cfg;
}key_state_t;

typedef struct
{
    unsigned char key_id;
    unsigned char key_type;
    key_cb_t cb;
}key_cb_info_t;


#define KEY_COUNT_MAX     10
#define KEY_CB_COUNT_MAX  20

static key_state_t key_state_list[KEY_COUNT_MAX] = {0};
static key_cb_info_t key_cb_list[KEY_CB_COUNT_MAX] = {0};
static unsigned char key_changed = 0;


int key_register(unsigned char key_id, unsigned char key_type, const key_cfg_t *key_cfg)
{
    int i;

    if ((KEY_ID_INVALID == key_id) || (KEY_TYPE_NONE == key_type) || (NULL == key_cfg))
        return -1;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID != key_state_list[i].key_id)
            continue;

        memset(&key_state_list[i], 0, sizeof(key_state_list[i]));
        key_state_list[i].key_id = key_id;
        key_state_list[i].key_type = key_type;
        key_state_list[i].key_cfg = *key_cfg;

        if (KEY_TYPE_BTN == key_state_list[i].key_type)
        {
            if (0 == key_state_list[i].key_cfg.btn_key_cfg.spt_multiclick)
                key_state_list[i].key_cfg.btn_key_cfg.spt_multiclick = 1;
        }
        break;
    }
    if (KEY_COUNT_MAX <= i)
        return -1;

    return 0;
}

int key_register_cb(unsigned char key_id, unsigned char key_type, key_cb_t cb)
{
    int i;

    if ((KEY_ID_INVALID == key_id) || (NULL == cb))
        return -1;

    for (i = 0; i < KEY_CB_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID != key_cb_list[i].key_id)
            continue;

        key_cb_list[i].key_id = key_id;
        key_cb_list[i].key_type = key_type;
        key_cb_list[i].cb = cb;
        break;
    }
    if (KEY_CB_COUNT_MAX <= i)
        return -1;

    return 0;
}

/*
key_task可以和中断中的key_set_value一起用: key_task中读变量时已关中断
key_task可以和任务中的key_set_value一起用: 串行执行没有竞争关系
任务中的key_set_value可以和中断中的key_set_value一起用: key_set_value中读变量时已关中断
*/
int key_set_value(unsigned char key_id, unsigned char key_type, signed int value)
{
    int i;
    unsigned int ts;


    if ((KEY_ID_INVALID == key_id) || (KEY_TYPE_NONE == key_type))
        return -1;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_state_list[i].key_id)
            return -1;

        if ((key_id != key_state_list[i].key_id) || (key_type != key_state_list[i].key_type))
            continue;

        if ((KEY_TYPE_REL == key_state_list[i].key_type) || (KEY_TYPE_ABS == key_state_list[i].key_type))
        {
            key_state_list[i].new_value = value;
            key_changed = 1;
            return 0;
        }
        if (KEY_TYPE_BTN == key_state_list[i].key_type)
        {
            if ((BUTTON_RELEASED != value) && (BUTTON_PRESSED != value))
                return -1;

            ts = HAL_GetTick();
            __disable_irq();
            /* 新值已经设置、处在防抖时间内或者上次的事件还没有处理则直接返回 */
            if (   (key_state_list[i].new_value == value)
                || (key_state_list[i].key_cfg.btn_key_cfg.debounce_delay > (ts - key_state_list[i].timestamp))
                || (key_state_list[i].new_value != key_state_list[i].old_value))
            {
                __enable_irq();
                return 0;
            }
            key_state_list[i].new_value = value;
            key_state_list[i].timestamp = ts;
            if (BUTTON_PRESSED == key_state_list[i].new_value)
                key_state_list[i].last_press_timestamp = ts;
            __enable_irq();

            key_changed = 1;
            return 0;
        }
    }
    if (KEY_COUNT_MAX <= i)
        return -1;

    return 0;
}

static void do_cb(unsigned char key_id, unsigned char key_type, signed int event)
{
    int i;

    for (i = 0; i < KEY_CB_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_cb_list[i].key_id)
            break;
        if ((key_id != key_cb_list[i].key_id) || (NULL == key_cb_list[i].cb))
            continue;
        if ((key_type != key_cb_list[i].key_type) && (KEY_TYPE_NONE != key_cb_list[i].key_type))
            continue;

        key_cb_list[i].cb(key_id, key_type, event);
    }
}
#define KEY_TASK_PERIOD  10  /* ms */
void key_task(void)
{
    static unsigned int last_ts = 0;
    unsigned int current_ts;
    signed int temp_new_value;
    signed int temp_old_value;
    unsigned int temp_press_ts;
    int i;


    current_ts = HAL_GetTick();
    if ((0 == key_changed) && (KEY_TASK_PERIOD > ((unsigned int)(current_ts - last_ts))))
        return;
    key_changed = 0;
    last_ts = current_ts;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_state_list[i].key_id)
            break;

        if ((KEY_TYPE_REL == key_state_list[i].key_type) || (KEY_TYPE_ABS == key_state_list[i].key_type))
        {
            temp_new_value = key_state_list[i].new_value;
            if (key_state_list[i].old_value == temp_new_value)
                continue;
            key_state_list[i].old_value = temp_new_value;
            do_cb(key_state_list[i].key_id, key_state_list[i].key_type, temp_new_value);

            continue;
        }

        /* KEY_TYPE_BTN */
        __disable_irq();
        temp_new_value = key_state_list[i].new_value;
        temp_old_value = key_state_list[i].old_value;
        temp_press_ts = key_state_list[i].last_press_timestamp;
        __enable_irq();
        key_state_list[i].old_value = temp_new_value;

        /* 已产生n次连击且下一次连击已超时 */
        if (   (key_state_list[i].click_state)
            && (key_state_list[i].key_cfg.btn_key_cfg.click_interval < (current_ts - temp_press_ts)))
        {
            do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, key_state_list[i].click_state);
            key_state_list[i].click_state = 0;

            /* 补发之前忽略的释放事件 */
            if (   (BUTTON_RELEASED == temp_old_value)
                && (BUTTON_RELEASED == temp_new_value)
                && (key_state_list[i].key_cfg.btn_key_cfg.trigger_on_release))
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_RELEASED);
        }
        /* 一直按下 */
        if ((BUTTON_PRESSED == temp_old_value) && (BUTTON_PRESSED == temp_new_value))
        {
            if ((0 == key_state_list[i].key_cfg.btn_key_cfg.spt_long_press) || (key_state_list[i].long_press_state))
                continue;

            if (key_state_list[i].key_cfg.btn_key_cfg.long_press_threshold < (current_ts - temp_press_ts))
            {
                /* 有在等待连击、还没发送的短按事件先处理，当配置的长按时间阈值短于连击时间间隔时可能发生这种情况 */
                if (key_state_list[i].click_state)
                {
                    do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, key_state_list[i].click_state);
                    key_state_list[i].click_state = 0;
                }
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_EVENT_LONG_PRESS);
                key_state_list[i].long_press_state = 1;
            }
            continue;
        }
        /* 释放 */
        if ((BUTTON_PRESSED == temp_old_value) && (BUTTON_RELEASED == temp_new_value))
        {
            /* 在连击结束前不触发释放事件 */
            if (key_state_list[i].key_cfg.btn_key_cfg.trigger_on_release && 0 == key_state_list[i].click_state)
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_RELEASED);
            key_state_list[i].long_press_state = 0;
            continue;
        }
        /* 按下 */
        if ((BUTTON_RELEASED == temp_old_value) && (BUTTON_PRESSED == temp_new_value))
        {
            if (key_state_list[i].key_cfg.btn_key_cfg.spt_multiclick > key_state_list[i].click_state)
                key_state_list[i].click_state++;
            if (key_state_list[i].key_cfg.btn_key_cfg.spt_multiclick <= key_state_list[i].click_state)
            {
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, key_state_list[i].click_state);
                key_state_list[i].click_state = 0;;
            }
            continue;
        }
    }
}
