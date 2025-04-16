#ifndef _DISK_MONITOR_H_
#define _DISK_MONITOR_H_


#include <stdint.h>


#ifndef DISKM_DEV_NAME_MAX
#define DISKM_DEV_NAME_MAX  32
#endif
#ifndef DISKM_MOUNT_POINT_MAX
#define DISKM_MOUNT_POINT_MAX  64
#endif
#ifndef DISKM_FSTYPE_MAX
#define DISKM_FSTYPE_MAX  16
#endif


#define DISKM_EVENT_ADD        1
#define DISKM_EVENT_REMOVE     2
#define DISKM_EVENT_MOUNT      3
#define DISKM_EVENT_UNMOUNT    4
#define DISKM_EVENT_MOUNT_ERR  5
typedef struct
{
    char mount_point[DISKM_MOUNT_POINT_MAX];
    char type[DISKM_FSTYPE_MAX];
    uint64_t size;
} fs_info_t;
typedef void (*diskm_cb_t)(const char *dev_name, unsigned int event, const fs_info_t *fs_info);

/* 不能在回调中调用 */
extern int diskm_init(void);
extern int diskm_exit(void);
extern int diskm_register_cb(const char *dev_name, diskm_cb_t cb);

extern int diskm_force_unmount(const char *dev_name);
extern int diskm_recheck(const char *dev_name);


#endif
