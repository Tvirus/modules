#ifndef _SPACE_MANAGER_H_
#define _SPACE_MANAGER_H_


#include <stdint.h>


#ifndef SPACEM_PATH_MAX
#define SPACEM_PATH_MAX  64
#endif
#ifndef SPACEM_IGNORE_STR_MAX
#define SPACEM_IGNORE_STR_MAX  16
#endif


#define SPACEM_EVENT_NOTIFY  0
#define SPACEM_EVENT_DELETE  1
typedef void (*spacem_cb_t)(const char *path, unsigned int event, const char *file);

typedef enum
{
    SPACEM_OBJ_TYPE_FILE = 0,
    SPACEM_OBJ_TYPE_DIR,
    SPACEM_OBJ_TYPE_NUM,
} spacem_obj_type_t;

typedef enum
{
    SPACEM_LIMIT_MODE_COUNT = 0,  /* 文件或目录总数，不包括忽略的项 */
    SPACEM_LIMIT_MODE_SIZE,       /* 目录总大小，包括忽略的项 */
    SPACEM_LIMIT_MODE_FREE,       /* 磁盘剩余空间 */
    SPACEM_LIMIT_MODE_DISKX,      /* 磁盘总空间 - x */
    SPACEM_LIMIT_MODE_NUM,
} spacem_limit_mode_t;

typedef enum
{
    SPACEM_ACTION_NOTIFY = 0,    /* 通知 */
    SPACEM_ACTION_DEL_OLDEST,    /* 删除文件修改时间最旧的，不包括忽略的项 */
    SPACEM_ACTION_DEL_NAME_MIN,  /* 删除文件名最小的，不包括忽略的项 */
    SPACEM_ACTION_DEL_NAME_MAX,  /* 删除文件名最大的，不包括忽略的项 */
    SPACEM_ACTION_NUM,
} spacem_action_t;

typedef struct
{
    const char *path;
    const char *ignore;
    unsigned char ignore_hidden;
    spacem_obj_type_t obj_type;
    spacem_limit_mode_t limit_mode;
    uint64_t limit_param;
    spacem_action_t action;
    spacem_cb_t cb;
} spacem_task_cfg_t;

extern int spacem_init(void);
extern int spacem_exit(void);
extern int spacem_register_task(const spacem_task_cfg_t *cfg);


#endif
