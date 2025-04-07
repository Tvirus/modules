#include "space_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <linux/limits.h>
#include <errno.h>




#define LOGLEVEL_ERROR  1
#define LOGLEVEL_INFO   2
#define LOGLEVEL_DEBUG  3
#define LOG(level, fmt, arg...) \
    do { \
        if (spacem_debug < level) \
            break; \
        if (LOGLEVEL_ERROR == level) \
            printf("\e[1;31m--SpaceM-- " fmt "\e[0m\n", ##arg); \
        else \
            printf("--SpaceM-- " fmt "\n", ##arg); \
    } while(0)

#ifndef SPACEM_TASK_MAX
#define SPACEM_TASK_MAX  16
#endif

typedef struct
{
    char path[SPACEM_PATH_MAX];
    char ignore[SPACEM_IGNORE_STR_MAX];
    uint64_t limit_param;
    spacem_cb_t cb;
    unsigned char ignore_hidden;
    unsigned char obj_type;
    unsigned char limit_mode;
    unsigned char action;
} spacem_task_info_t;


unsigned char spacem_debug = LOGLEVEL_INFO;
static pthread_mutex_t spacem_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char spacem_inited = 0;
static unsigned char should_run = 0;
static unsigned char is_running = 0;

static spacem_task_info_t task_info_list[SPACEM_TASK_MAX] = {0};


static unsigned int get_line_by_cmd(char *buf, unsigned int len, const char *fmt, ...)
{
    va_list args;
    char cmd[300];
    FILE *fp;
    int _len;


    if ((NULL == buf) || (2 > len) || (NULL == fmt))
        return -1;

    va_start(args, fmt);
    _len = vsnprintf(cmd, sizeof(cmd), fmt, args);
    if (0 > _len)
    {
        LOG(LOGLEVEL_ERROR, "vsnprintf err: %s !", strerror(errno));
        buf[0] = 0;
        return 0;
    }
    if (sizeof(cmd) <= _len)
    {
        LOG(LOGLEVEL_ERROR, "cmd len(%d) exceeds the buf len(%zu) !", _len, sizeof(cmd));
        buf[0] = 0;
        return 0;
    }

    fp = popen(cmd, "r");
    if (!fp)
    {
        LOG(LOGLEVEL_ERROR, "popen failed, cmd: %s, err: %s !", cmd, strerror(errno));
        buf[0] = 0;
        return 0;
    }

    if (NULL == fgets(buf, len, fp))
    {
        LOG(LOGLEVEL_ERROR, "fgets err: %s !", strerror(errno));
        pclose(fp);
        buf[0] = 0;
        return 0;
    }

    pclose(fp);
    return (unsigned int)strlen(buf);
}

static unsigned int get_object_count(const char *path, spacem_obj_type_t type, unsigned char ignore_hidden, const char *ignore)
{
    struct stat stat_buf;
    char *_hidden;
    char *_type;
    char buf[16] = {};
    int count;

    if ((0 > stat(path, &stat_buf)) || (0 == S_ISDIR(stat_buf.st_mode)))
        return 0;

    if (type == SPACEM_OBJ_TYPE_FILE)
        _type = "'\"$|\\*$'";
    else if (type == SPACEM_OBJ_TYPE_DIR)
        _type = "/$";
    else
        return 0;

    if (ignore_hidden)
        _hidden = "";
    else
        _hidden = "A";

    if (ignore && ignore[0])
        get_line_by_cmd(buf, sizeof(buf), "ls %s -%sFQ1 | grep -E %s | grep -c -v %s", path, _hidden, _type, ignore);
    else
        get_line_by_cmd(buf, sizeof(buf), "ls %s -%sFQ1 | grep -E %s -c", path, _hidden, _type);

    count = atoi(buf);
    if (0 <= count)
        return (unsigned int)count;
    else
        return 0;
}

static uint64_t get_path_size(const char *path)
{
    struct stat stat_buf;
    char buf[32] = {};

    if ((0 > stat(path, &stat_buf)) || (0 == S_ISDIR(stat_buf.st_mode)))
        return 0;

    get_line_by_cmd(buf, sizeof(buf), "du %s -sb", path);
    return strtoull(buf, NULL, 10);
}

static int get_disk_size(const char *path, uint64_t *size)
{
    struct stat stat_buf;
    struct statfs statfs_buf;

    if ((0 > stat(path, &stat_buf)) || (0 == S_ISDIR(stat_buf.st_mode)))
        return -1;

    if (0 > statfs(path, &statfs_buf))
        return -1;
    *size = ((uint64_t)statfs_buf.f_bsize) * ((uint64_t)statfs_buf.f_blocks);
    return 0;
}

static int get_disk_free_size(const char *path, uint64_t *size)
{
    struct stat stat_buf;
    struct statfs statfs_buf;

    if ((0 > stat(path, &stat_buf)) || (0 == S_ISDIR(stat_buf.st_mode)))
        return -1;

    if (0 > statfs(path, &statfs_buf))
        return -1;
    *size = ((uint64_t)statfs_buf.f_bsize) * ((uint64_t)statfs_buf.f_bfree);
    return 0;
}

static int del_object(const spacem_task_info_t *task_info, char *name_buf, unsigned int name_buf_len)
{
    char *_hidden;
    char *_type;
    char *_sort;
    int len;
    char buf[SPACEM_PATH_MAX + NAME_MAX + 1];


    if (task_info->obj_type == SPACEM_OBJ_TYPE_FILE)
        _type = "'/$|=$|\\|$|@$' -v";
    else if (task_info->obj_type == SPACEM_OBJ_TYPE_DIR)
        _type = "/$";
    else
        return -1;

    if (task_info->ignore_hidden)
        _hidden = "";
    else
        _hidden = "A";

    if (task_info->action == SPACEM_ACTION_DEL_OLDEST)
        _sort = "tr";
    else if (task_info->action == SPACEM_ACTION_DEL_NAME_MIN)
        _sort = "";
    else if (task_info->action == SPACEM_ACTION_DEL_NAME_MAX)
        _sort = "r";
    else
        return -1;

    if (task_info->ignore && task_info->ignore[0])
        len = get_line_by_cmd(name_buf, name_buf_len, "ls %s -%sF1%s | grep -E %s | grep -v %s",
                              task_info->path, _hidden, _sort, _type, task_info->ignore);
    else
        len = get_line_by_cmd(name_buf, name_buf_len, "ls %s -%sF1%s | grep -E %s", task_info->path, _hidden, _sort, _type);
    if (1 >= len)
    {
        LOG(LOGLEVEL_ERROR, "get object failed, path:%s !", task_info->path);
        return -1;
    }
    if ('*' == name_buf[len - 2])
        name_buf[len - 2] = 0;
    else if ('\n' == name_buf[len - 1])
        name_buf[len - 1] = 0;

    if (sizeof(buf) <= snprintf(buf, sizeof(buf), "%s/%s", task_info->path, name_buf))
    {
        LOG(LOGLEVEL_ERROR, "build file path failed, path len(%zu) + object len(%zu) exceeds the max(%zu) !",
            strlen(task_info->path), strlen(name_buf), sizeof(buf));
        return -1;
    }
    if (remove(buf))
    {
        LOG(LOGLEVEL_ERROR, "rm failed: %s !", buf);
        return -1;
    }

    LOG(LOGLEVEL_INFO, "rm OK: %s", buf);
    return 0;
}

static void* manager(void *arg)
{
    int i;
    int time = 0;
    spacem_task_info_t *task_info;
    unsigned int count;
    uint64_t path_size;
    uint64_t size;
    char buf[NAME_MAX];


    pthread_detach(pthread_self());
    prctl(PR_SET_NAME, (unsigned long)"space_manager", 0, 0, 0);

    while (should_run)
    {
        if (10 > time++)
        {
            sleep(1);
            continue;
        }
        time = 0;

        for (i = 0; i < SPACEM_TASK_MAX; i++)
        {
            if (0 == task_info_list[i].path[0])
                continue;
            task_info = &task_info_list[i];

            if (task_info->limit_mode == SPACEM_LIMIT_MODE_COUNT)
            {
                count = get_object_count(task_info->path, task_info->obj_type, task_info->ignore_hidden, task_info->ignore);
                LOG(LOGLEVEL_DEBUG, "path:%s count:%u", task_info->path, count);
                if (task_info->limit_param >= count)
                    continue;
                LOG(LOGLEVEL_INFO, "path:%s count(%u) exceeds the limit(%llu)", task_info->path, count, task_info->limit_param);
            }
            else if (task_info->limit_mode == SPACEM_LIMIT_MODE_SIZE)
            {
                path_size = get_path_size(task_info->path);
                LOG(LOGLEVEL_DEBUG, "path:%s size:%llu", task_info->path, path_size);
                if (task_info->limit_param >= path_size)
                    continue;
                LOG(LOGLEVEL_INFO, "path:%s size(%llu) exceeds the limit(%llu)", task_info->path, path_size, task_info->limit_param);
            }
            else if (task_info->limit_mode == SPACEM_LIMIT_MODE_FREE)
            {
                if (get_disk_free_size(task_info->path, &size))
                    continue;
                LOG(LOGLEVEL_DEBUG, "path:%s freesize:%llu", task_info->path, size);
                if (task_info->limit_param <= size)
                    continue;
                LOG(LOGLEVEL_INFO, "path:%s freesize(%llu) is below the limit(%llu)", task_info->path, size, task_info->limit_param);
            }
            else if (task_info->limit_mode == SPACEM_LIMIT_MODE_DISKX)
            {
                path_size = get_path_size(task_info->path);
                if (get_disk_size(task_info->path, &size))
                    continue;
                LOG(LOGLEVEL_DEBUG, "path:%s size:%llu disksize:%llu", task_info->path, path_size, size);
                if (   (task_info->limit_param >= size)
                    || ((size - task_info->limit_param) >= path_size))
                    continue;
                LOG(LOGLEVEL_INFO, "path:%s size(%llu) exceeds disksize(%llu) - limit(%llu)",
                    task_info->path, path_size, size, task_info->limit_param);
            }
            else
            {
                continue;
            }

            if (task_info->action == SPACEM_ACTION_NOTIFY)
            {
                if (task_info->cb)
                    task_info->cb(task_info->path, SPACEM_EVENT_NOTIFY, NULL);
                continue;
            }
            if (del_object(task_info, buf, sizeof(buf)))
                continue;
            if (task_info->cb)
                task_info->cb(task_info->path, SPACEM_EVENT_DELETE, buf);
        }
    }

    is_running = 0;
    return NULL;
}

int spacem_init(void)
{
    pthread_t tid;

    pthread_mutex_lock(&spacem_mutex);
    if (spacem_inited)
    {
        pthread_mutex_unlock(&spacem_mutex);
        return 0;
    }

    should_run = 1;
    is_running = 1;
    pthread_create(&tid, NULL, manager, NULL);

    spacem_inited = 1;
    pthread_mutex_unlock(&spacem_mutex);
    return 0;
}

int spacem_exit(void)
{
    int i;

    pthread_mutex_lock(&spacem_mutex);
    if (0 == spacem_inited)
    {
        pthread_mutex_unlock(&spacem_mutex);
        return 0;
    }
    spacem_inited = 0;

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
        pthread_mutex_unlock(&spacem_mutex);
        return -1;
    }

    pthread_mutex_unlock(&spacem_mutex);
    return 0;
}

int spacem_register_task(const spacem_task_cfg_t *cfg)
{
    int i;
    size_t len;

    if (   (NULL == cfg)
        || (NULL == cfg->path)
        || (SPACEM_OBJ_TYPE_NUM <= cfg->obj_type)
        || (SPACEM_LIMIT_MODE_NUM <= cfg->limit_mode)
        || (SPACEM_ACTION_NUM <= cfg->action))
        return -1;

    len = strnlen(cfg->path, SPACEM_PATH_MAX);
    if ((0 == len) || (SPACEM_PATH_MAX <= len))
    {
        LOG(LOGLEVEL_ERROR, "path len(%zu) exceeds the max(%u) !", len, SPACEM_PATH_MAX);
        return -1;
    }
    if (cfg->ignore)
    {
        len = strnlen(cfg->ignore, SPACEM_IGNORE_STR_MAX);
        if (SPACEM_IGNORE_STR_MAX <= len)
        {
            LOG(LOGLEVEL_ERROR, "ignore string len(%zu) exceeds the max(%u) !", len, SPACEM_IGNORE_STR_MAX);
            return -1;
        }
    }
    if ((SPACEM_ACTION_NOTIFY == cfg->action) && (NULL == cfg->cb))
    {
        LOG(LOGLEVEL_ERROR, "cb is NULL !");
        return -1;
    }

    pthread_mutex_lock(&spacem_mutex);

    for (i = 0; i < SPACEM_TASK_MAX; i++)
    {
        if (task_info_list[i].path[0])
            continue;

        if (cfg->ignore)
            strcpy(task_info_list[i].ignore, cfg->ignore);
        task_info_list[i].ignore_hidden = cfg->ignore_hidden;
        task_info_list[i].obj_type = cfg->obj_type;
        task_info_list[i].limit_mode = cfg->limit_mode;
        task_info_list[i].limit_param = cfg->limit_param;
        task_info_list[i].action = cfg->action;
        task_info_list[i].cb = cfg->cb;
        strcpy(task_info_list[i].path, cfg->path);
        break;
    }
    if (SPACEM_TASK_MAX <= i)
    {
        LOG(LOGLEVEL_ERROR, "task count exceeds the max(%u) !", SPACEM_TASK_MAX);
        pthread_mutex_unlock(&spacem_mutex);
        return -1;
    }

    pthread_mutex_unlock(&spacem_mutex);
    return 0;
}
