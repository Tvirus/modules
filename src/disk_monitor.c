#include "disk_monitor.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/select.h>
#include <errno.h>




#define LOGLEVEL_ERROR  1
#define LOGLEVEL_INFO   2
#define LOGLEVEL_DEBUG  3
#define LOG(level, fmt, arg...) \
    do{ \
        if (diskm_debug < level) \
            break; \
        if (LOGLEVEL_ERROR == level) \
            printf("\e[1;31m--DiskM-- " fmt "\e[0m\n", ##arg); \
        else \
            printf("--DiskM-- " fmt "\n", ##arg); \
    }while(0)

#ifndef DISKM_DEV_NAME_MAX
#define DISKM_DEV_NAME_MAX  32
#endif
#ifndef DISKM_MOUNT_POINT_MAX
#define DISKM_MOUNT_POINT_MAX  64
#endif
#ifndef DISKM_CB_MAX
#define DISKM_CB_MAX  8
#endif
#ifndef DISKM_MOUNT_TIMEOUT
#define DISKM_MOUNT_TIMEOUT  4
#endif

typedef struct
{
    char dev_name[DISKM_DEV_NAME_MAX];
    char mount_point[DISKM_MOUNT_POINT_MAX];
    diskm_cb_t cb;  /* 同时用来标记是否已用 */
    unsigned char dev_added;
    unsigned char mounted;
    unsigned char mount_err;
    unsigned char initial_check;
    unsigned long check_ts;
} diskm_cb_info_t;


unsigned char diskm_debug = LOGLEVEL_INFO;
static pthread_mutex_t diskm_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char diskm_inited = 0;
static unsigned char should_run = 0;
static unsigned char is_running = 0;

static diskm_cb_info_t cb_info_list[DISKM_CB_MAX] = {0};



static int is_mounted(const char *dev_name, char *mount_point)
{
    FILE *fp = NULL;
    char buf[200];
    char *p;
    char *end;
    int mounted = 0;

    fp = fopen("/proc/mounts", "rt");
    if (NULL == fp)
    {
        LOG(LOGLEVEL_ERROR, "open /proc/mounts failed !");
        return 0;
    }

    for (;;)
    {
        if (NULL == fgets(buf, sizeof(buf), fp))
            break;

        p = strchr(buf, ' ');
        if (NULL == p)
            continue;
        *p++ = 0;
        if (NULL == strstr(buf, dev_name))
            continue;

        end = strchr(p, ' ');
        if (NULL == end)
            continue;
        *end = 0;

        strncpy(mount_point, p, DISKM_MOUNT_POINT_MAX);
        mounted = 1;
        break;
    }

    fclose(fp);
    return mounted;
}

static char* uevent_search(unsigned char *uevent, unsigned int uevent_len, const char *str)
{
    char *p;
    unsigned char *end = uevent + uevent_len - 1;

    if ((NULL == uevent) || (0 == uevent_len))
        return NULL;

    p = strstr(uevent, str);
    if (p)
        return p;

    while (uevent < end)
    {
        if (0 != *uevent++)
            continue;

        p = strstr(uevent, str);
        if (p)
            return p;
    }

    return NULL;
}

static void set_dev(const char *dev_name, unsigned char add)
{
    diskm_cb_info_t *cb_info;
    int i;

    for (i = 0; i < DISKM_CB_MAX; i++)
    {
        cb_info = &cb_info_list[i];

        if (NULL == cb_info->cb)
            continue;
        if (strcmp(cb_info->dev_name, dev_name))
            continue;

        cb_info->mount_err = 0;
        cb_info->initial_check = 0;
        if (add)
        {
            cb_info->dev_added = 1;
            cb_info->mounted = 0;
            cb_info->check_ts = 1;
            cb_info->cb(cb_info->dev_name, NULL, DISKM_EVENT_ADD);
        }
        else
        {
            cb_info->dev_added = 0;
            if (cb_info->mounted)
            {
                cb_info->mounted = 0;
                cb_info->cb(cb_info->dev_name, NULL, DISKM_EVENT_UNMOUNT);
            }
            cb_info->check_ts = 0;
            cb_info->cb(cb_info->dev_name, NULL, DISKM_EVENT_REMOVE);
        }
    }
}
static void* monitor(void *arg)
{
    int fd = -1;
    struct sockaddr_nl sa;
    fd_set fds;
    struct timeval timeout;
    int ret;
    ssize_t uevent_len;
    char uevent_buf[1000];
    char dev_name[5 + DISKM_DEV_NAME_MAX];
    struct timespec ts;
    char *value;
    int i;
    diskm_cb_info_t *cb_info;


    pthread_detach(pthread_self());
    prctl(PR_SET_NAME, (unsigned long)"disk_monitor", 0, 0, 0);

    fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (0 > fd)
    {
        LOG(LOGLEVEL_ERROR, "open netlink failed: %s !", strerror(errno));
        goto EXIT;
    }

    sa.nl_family = AF_NETLINK;
    sa.nl_groups = 1;
    sa.nl_pid = (getpid() << 16) | pthread_self();
    if (0 > bind(fd, (struct sockaddr *)&sa, sizeof(sa)))
    {
        LOG(LOGLEVEL_ERROR, "bind failed: %s !", strerror(errno));
        goto EXIT;
    }

    while (should_run)
    {
        for (i = 0; i < DISKM_CB_MAX; i++)
        {
            if (NULL == cb_info_list[i].cb)
                continue;
            cb_info = &cb_info_list[i];

            if (cb_info->initial_check)
            {
                sprintf(dev_name, "/dev/%s", cb_info->dev_name);
                if (0 == access(dev_name, F_OK))
                {
                    cb_info->initial_check = 0;
                    cb_info->dev_added = 1;
                    cb_info->mounted = 0;
                    cb_info->mount_err = 0;
                    cb_info->check_ts = 1;
                    cb_info->cb(cb_info->dev_name, NULL, DISKM_EVENT_ADD);
                }
                else
                {
                    cb_info->initial_check--;
                    cb_info->dev_added = 0;
                    cb_info->mounted = 0;
                    cb_info->check_ts = 0;
                }
            }
            if (cb_info->check_ts)
            {
                clock_gettime(CLOCK_MONOTONIC, &ts);
                if ((1 == cb_info->check_ts) || (DISKM_MOUNT_TIMEOUT > (((unsigned long)ts.tv_sec) - cb_info->check_ts)))
                {
                    if (is_mounted(cb_info->dev_name, cb_info->mount_point))
                    {
                        cb_info->dev_added = 1;
                        cb_info->mounted = 1;
                        cb_info->mount_err = 0;
                        cb_info->check_ts = 0;
                        cb_info->cb(cb_info->dev_name, cb_info->mount_point, DISKM_EVENT_MOUNT);
                    }
                    else if (1 == cb_info->check_ts)
                        cb_info->check_ts = (unsigned long)ts.tv_sec;
                }
                else
                {
                    cb_info->mounted = 0;
                    cb_info->mount_err = 1;
                    cb_info->check_ts = 0;
                    cb_info->cb(cb_info->dev_name, NULL, DISKM_EVENT_MOUNT_ERR);
                }
            }
        }

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 500 * 1000;
        ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        if (0 > ret)
        {
            LOG(LOGLEVEL_ERROR, "select failed !");
            sleep(1);
            continue;
        }
        else if (0 == ret)
        {
            continue;
        }
        uevent_len = recv(fd, uevent_buf, sizeof(uevent_buf), 0);
        if (0 >= uevent_len)
        {
            sleep(1);
            continue;
        }
        uevent_buf[uevent_len - 1] = 0;

        if (NULL == uevent_search(uevent_buf, uevent_len, "SUBSYSTEM=block"))
            continue;
        value = uevent_search(uevent_buf, uevent_len, "DEVNAME=");
        if (value)
            value += sizeof("DEVNAME=") - 1;
        else
            continue;
        if (uevent_search(uevent_buf, uevent_len, "ACTION=add"))
        {
            LOG(LOGLEVEL_INFO, "add dev: %s", value);
            set_dev(value, 1);
        }
        else if (uevent_search(uevent_buf, uevent_len, "ACTION=remove"))
        {
            LOG(LOGLEVEL_INFO, "remove dev: %s", value);
            set_dev(value, 0);
        }
    }

EXIT:
    if (0 <= fd)
        close(fd);
    is_running = 0;
    return NULL;
}

int diskm_init(void)
{
    pthread_t tid;

    pthread_mutex_lock(&diskm_mutex);
    if (diskm_inited)
    {
        pthread_mutex_unlock(&diskm_mutex);
        return 0;
    }

    should_run = 1;
    is_running = 1;
    pthread_create(&tid, NULL, monitor, NULL);

    diskm_inited = 1;
    pthread_mutex_unlock(&diskm_mutex);
    return 0;
}

int diskm_exit(void)
{
    int i;

    pthread_mutex_lock(&diskm_mutex);
    if (0 == diskm_inited)
    {
        pthread_mutex_unlock(&diskm_mutex);
        return 0;
    }
    diskm_inited = 0;

    should_run = 0;
    for (i = 0; i < 40; i++)
    {
        if (0 == is_running)
            break;
        usleep(100 * 1000);
    }
    if (40 <= i)
    {
        LOG(LOGLEVEL_ERROR, "exit timeout !");
        pthread_mutex_unlock(&diskm_mutex);
        return -1;
    }

    pthread_mutex_unlock(&diskm_mutex);
    return 0;
}

int diskm_register_cb(const char *dev_name, diskm_cb_t cb)
{
    int i;
    ssize_t dev_len;

    if ((NULL == dev_name) || (NULL == cb))
        return -1;

    dev_len = strnlen(dev_name, DISKM_DEV_NAME_MAX);
    if (DISKM_DEV_NAME_MAX <= dev_len)
    {
        LOG(LOGLEVEL_ERROR, "dev_len exceeds the max(%u) !", DISKM_DEV_NAME_MAX);
        return -1;
    }

    pthread_mutex_lock(&diskm_mutex);

    for (i = 0; i < DISKM_CB_MAX; i++)
    {
        if (cb_info_list[i].cb)
            continue;

        memcpy(cb_info_list[i].dev_name, dev_name, dev_len + 1);
        cb_info_list[i].dev_added = 0;
        cb_info_list[i].mounted = 0;
        cb_info_list[i].mount_err = 0;
        cb_info_list[i].initial_check = 4;
        cb_info_list[i].check_ts = 0;
        cb_info_list[i].cb = cb;
        break;
    }
    if (DISKM_CB_MAX <= i)
    {
        LOG(LOGLEVEL_ERROR, "cb count exceeds the max(%u) !", DISKM_CB_MAX);
        pthread_mutex_unlock(&diskm_mutex);
        return -1;
    }

    pthread_mutex_unlock(&diskm_mutex);
    return 0;
}
