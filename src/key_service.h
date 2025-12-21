#ifndef _KEY_SERVICE_H_
#define _KEY_SERVICE_H_


#define KEY_ID_INVALID  0

#define KEY_TYPE_INVALID  0
#define KEY_TYPE_BTN      1  /* 普通按键 */
#define KEY_TYPE_REL      2  /* 滚轮等相对值 */
#define KEY_TYPE_ABS      3  /* 旋钮、xy坐标等绝对值 */

#define BUTTON_EVENT_LONG_PRESS  (-1)
#define BUTTON_EVENT_RELEASE      0
#define BUTTON_EVENT_1_CLICK      1
#define BUTTON_EVENT_2_CLICK      2
#define BUTTON_EVENT_3_CLICK      3


typedef struct
{
    unsigned char debounce_delay;       /* 防抖时间, ms */
    unsigned char spt_multiclick;       /* 最高支持n连击 */
    unsigned char spt_long_press;       /* 是否支持长按 */
    unsigned char trigger_on_release;   /* 抬起时是否触发，默认只有按下触发 */
    unsigned int click_interval;        /* 判断连击的时间间隔, ms */
    unsigned int long_press_threshold;  /* 判断长按的时间，要大于连击时间间隔, ms */
} btn_key_cfg_t;

typedef struct
{
} rel_key_cfg_t;

typedef struct
{
} abs_key_cfg_t;

typedef union
{
    btn_key_cfg_t btn_cfg;
    rel_key_cfg_t rel_cfg;
    abs_key_cfg_t abs_cfg;
} key_cfg_t;


typedef void (*key_cb_t)(unsigned char key_id, unsigned char key_type, signed int event);

extern void key_task(void);
extern int key_register(unsigned char key_id, unsigned char key_type, const key_cfg_t *key_cfg);
extern int key_register_cb(unsigned char key_id, key_cb_t cb);
#define BUTTON_RELEASED 0
#define BUTTON_PRESSED  1
extern int key_set_value(unsigned char key_id, signed int value);


#endif
