#ifndef _DISK_MONITOR_H_
#define _DISK_MONITOR_H_


#define DISKM_EVENT_ADD        1
#define DISKM_EVENT_REMOVE     2
#define DISKM_EVENT_MOUNT      3
#define DISKM_EVENT_UNMOUNT    4
#define DISKM_EVENT_MOUNT_ERR  5
typedef void (*diskm_cb_t)(const char *dev_name, const char *mount_path, unsigned int event);

extern int diskm_init(void);
extern int diskm_exit(void);
extern int diskm_register_cb(const char *dev_name, diskm_cb_t cb);


#endif
