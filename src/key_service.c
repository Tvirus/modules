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
#include <string.h>
#include "log.h"
#include "stm32u5xx_hal.h"




#define LOGLEVEL_ERROR 1
#define LOGLEVEL_INFO 2
#define LOGLEVEL_DEBUG 3
#define LOG(level, fmt, arg...)  do{if((level) <= keys_log_level)log_printf("--KEYS-- " fmt "\n", ##arg);}while(0)
unsigned char keys_log_level = LOGLEVEL_INFO;


typedef struct
{
    unsigned char key_id;
    unsigned char key_type;
    unsigned char click_state;       /* 0:空闲, n:已n连击 */
    unsigned char long_press_state;  /* 0:空闲, 1:已长按 */
    signed int new_value;
    signed int old_value;            /* 按键上一个值 */
    unsigned int last_ts;            /* 最后一次值发生变化的时间，用于防抖，ms */
    unsigned int last_press_ts;      /* 针对普通按键，最后一次按下的时间，用于判断连击，ms */
    unsigned char cache_event;
    key_cfg_t key_cfg;
} key_state_t;

typedef struct
{
    unsigned char key_id;
    key_cb_t cb;
} key_cb_info_t;


#define KEY_COUNT_MAX     20
#define KEY_CB_COUNT_MAX  30

static key_state_t key_state_list[KEY_COUNT_MAX] = {0};
static key_cb_info_t key_cb_list[KEY_CB_COUNT_MAX] = {0};
static unsigned char key_changed = 0;


int key_register(unsigned char key_id, unsigned char key_type, const key_cfg_t *key_cfg)
{
    int i;

    if ((KEY_ID_INVALID == key_id) || (KEY_TYPE_INVALID == key_type) || (NULL == key_cfg))
        return -1;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (key_state_list[i].key_id == key_id)
        {
            LOG(LOGLEVEL_ERROR, "register key_id conflict !");
            return -1;
        }
        if (KEY_ID_INVALID != key_state_list[i].key_id)
            continue;

        memset(&key_state_list[i], 0, sizeof(key_state_list[i]));
        key_state_list[i].key_id = key_id;
        key_state_list[i].key_type = key_type;
        key_state_list[i].key_cfg = *key_cfg;
        if (KEY_TYPE_BTN == key_type)
        {
            if (   (0 == key_state_list[i].key_cfg.btn_cfg.spt_multiclick)
                && (0 == key_state_list[i].key_cfg.btn_cfg.spt_long_press)
                && (0 == key_state_list[i].key_cfg.btn_cfg.trigger_on_release))
            {
                LOG(LOGLEVEL_ERROR, "no event has been configured !");
                return -1;
            }
            if (   (1 < key_state_list[i].key_cfg.btn_cfg.spt_multiclick)
                && (key_state_list[i].key_cfg.btn_cfg.spt_long_press)
                && (key_state_list[i].key_cfg.btn_cfg.long_press_threshold < key_state_list[i].key_cfg.btn_cfg.click_interval))
            {
                key_state_list[i].key_cfg.btn_cfg.long_press_threshold = key_state_list[i].key_cfg.btn_cfg.click_interval;
                LOG(LOGLEVEL_ERROR, "click_interval is greater than long_press_threshold !");
            }
        }
        break;
    }
    if (KEY_COUNT_MAX <= i)
    {
        LOG(LOGLEVEL_ERROR, "register key failed !");
        return -1;
    }
    return 0;
}

int key_register_cb(unsigned char key_id, key_cb_t cb)
{
    int i;

    if ((KEY_ID_INVALID == key_id) || (NULL == cb))
        return -1;

    for (i = 0; i < KEY_CB_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID != key_cb_list[i].key_id)
            continue;

        key_cb_list[i].key_id = key_id;
        key_cb_list[i].cb = cb;
        break;
    }
    if (KEY_CB_COUNT_MAX <= i)
    {
        LOG(LOGLEVEL_ERROR, "register key cb failed !");
        return -1;
    }
    return 0;
}

/*
key_task可以和中断中的key_set_value一起用: key_task中读变量时已关中断
key_task可以和任务中的key_set_value一起用: 串行执行没有竞争关系
任务中的key_set_value可以和中断中的key_set_value一起用: key_set_value中读变量时已关中断
*/
int key_set_value(unsigned char key_id, signed int value)
{
    int i;
    unsigned int ts;

    if (KEY_ID_INVALID == key_id)
        return -1;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_state_list[i].key_id)
            return -1;
        if (key_id != key_state_list[i].key_id)
            continue;

        if (KEY_TYPE_ABS == key_state_list[i].key_type)
        {
            key_state_list[i].new_value = value;
            key_changed = 1;
            return 0;
        }
        else if (KEY_TYPE_REL == key_state_list[i].key_type)
        {
            __disable_irq();
            key_state_list[i].new_value += value;
            __enable_irq();
            key_changed = 1;
            return 0;
        }
        else if (KEY_TYPE_BTN == key_state_list[i].key_type)
        {
            if ((BUTTON_RELEASED != value) && (BUTTON_PRESSED != value))
                return -1;
            if (key_state_list[i].new_value == value)
                return 0;

            ts = HAL_GetTick();
            __disable_irq();
            key_state_list[i].new_value = value;
            key_state_list[i].last_ts = ts;
            if (BUTTON_PRESSED == value)
                key_state_list[i].last_press_ts = ts;
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

    LOG(LOGLEVEL_DEBUG, "id:%u event:%d", key_id, event);
    for (i = 0; i < KEY_CB_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_cb_list[i].key_id)
            break;
        if ((key_id != key_cb_list[i].key_id) || (NULL == key_cb_list[i].cb))
            continue;
        key_cb_list[i].cb(key_id, key_type, event);
    }
}
#define KEY_TASK_PERIOD  10  /* ms */
void key_task(void)
{
    static unsigned int task_ts = 0;
    unsigned int current_ts;
    signed int new_value;
    signed int old_value;
    unsigned int last_press_ts;
    unsigned int last_ts;
    int i;


    current_ts = HAL_GetTick();
    if ((0 == key_changed) && (KEY_TASK_PERIOD > ((unsigned int)(current_ts - task_ts))))
        return;
    key_changed = 0;
    task_ts = current_ts;

    for (i = 0; i < KEY_COUNT_MAX; i++)
    {
        if (KEY_ID_INVALID == key_state_list[i].key_id)
            break;

        if (KEY_TYPE_ABS == key_state_list[i].key_type)
        {
            new_value = key_state_list[i].new_value;
            if (key_state_list[i].old_value == new_value)
                continue;
            key_state_list[i].old_value = new_value;
            do_cb(key_state_list[i].key_id, KEY_TYPE_ABS, new_value);
            continue;
        }
        if (KEY_TYPE_REL == key_state_list[i].key_type)
        {
            __disable_irq();
            new_value = key_state_list[i].new_value;
            key_state_list[i].new_value = 0;
            __enable_irq();
            if (0 == new_value)
                continue;
            do_cb(key_state_list[i].key_id, KEY_TYPE_REL, new_value);
            continue;
        }

        /* KEY_TYPE_BTN */
        __disable_irq();
        new_value = key_state_list[i].new_value;
        old_value = key_state_list[i].old_value;
        last_press_ts = key_state_list[i].last_press_ts;
        last_ts = key_state_list[i].last_ts;
        __enable_irq();
        if ((key_state_list[i].key_cfg.btn_cfg.debounce_delay > ((unsigned int)(HAL_GetTick() - last_ts))))
            continue;
        key_state_list[i].old_value = new_value;

        /* 已产生n次连击且下一次连击已超时 */
        if (   (key_state_list[i].click_state)
            && (key_state_list[i].key_cfg.btn_cfg.click_interval < (HAL_GetTick() - last_press_ts)))
        {
            if (0 == key_state_list[i].cache_event)
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, key_state_list[i].click_state);
            key_state_list[i].click_state = 0;

            /* 补发之前忽略的释放事件 */
            if ( (BUTTON_RELEASED == old_value) && (BUTTON_RELEASED == new_value))
            {
                if (key_state_list[i].cache_event)
                {
                    do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_EVENT_1_CLICK);
                    key_state_list[i].cache_event = 0;
                }
                if (key_state_list[i].key_cfg.btn_cfg.trigger_on_release)
                    do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_RELEASED);
            }
        }
        /* 一直按下 */
        if ((BUTTON_PRESSED == old_value) && (BUTTON_PRESSED == new_value))
        {
            if ((0 == key_state_list[i].key_cfg.btn_cfg.spt_long_press) || (key_state_list[i].long_press_state))
                continue;

            if (key_state_list[i].key_cfg.btn_cfg.long_press_threshold < (HAL_GetTick() - last_press_ts))
            {
                do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_EVENT_LONG_PRESS);
                key_state_list[i].long_press_state = 1;
                key_state_list[i].cache_event = 0;
            }
            continue;
        }
        /* 释放 */
        if ((BUTTON_PRESSED == old_value) && (BUTTON_RELEASED == new_value))
        {
            /* 在连击结束前不触发释放事件 */
            if (0 == key_state_list[i].click_state)
            {
                if (key_state_list[i].cache_event)
                {
                    do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_EVENT_1_CLICK);
                    key_state_list[i].cache_event = 0;
                }
                if (key_state_list[i].key_cfg.btn_cfg.trigger_on_release)
                    do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, BUTTON_RELEASED);
            }
            key_state_list[i].long_press_state = 0;
            continue;
        }
        /* 按下 */
        if ((BUTTON_RELEASED == old_value) && (BUTTON_PRESSED == new_value))
        {
            if (key_state_list[i].key_cfg.btn_cfg.spt_multiclick > key_state_list[i].click_state)
            {
                key_state_list[i].click_state++;
                /* 缓存支持单击和长按时的第一次按下 */
                if (   (1 == key_state_list[i].key_cfg.btn_cfg.spt_multiclick)
                    && (1 == key_state_list[i].click_state)
                    && key_state_list[i].key_cfg.btn_cfg.spt_long_press)
                    key_state_list[i].cache_event = 1;
                else
                    key_state_list[i].cache_event = 0;
                if (key_state_list[i].key_cfg.btn_cfg.spt_multiclick <= key_state_list[i].click_state)
                {
                    if (0 == key_state_list[i].cache_event)
                        do_cb(key_state_list[i].key_id, KEY_TYPE_BTN, key_state_list[i].click_state);
                    key_state_list[i].click_state = 0;
                }
            }
            continue;
        }
    }
}
