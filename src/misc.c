

#include <fcntl.h>
#include <sys/file.h>
int file_try_lock_ex(const char *file, int *fd)
{
    int _fd;

    if ((NULL == file) || (NULL == fd))
        return -1;
    *fd = -1;

    _fd = open(file, O_RDONLY);
    if (0 > _fd)
        return -1;

    if (0 != flock(_fd, LOCK_EX | LOCK_NB))
    {
        close(_fd);
        return -1;
    }

    *fd = _fd;
    return 0;
}
int file_unlock(int *fd)
{
    if (NULL == fd)
        return -1;
    if (0 > *fd)
        return 0;

    flock(*fd, LOCK_UN);
    close(*fd);
    *fd = -1;

    return 0;
}




#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#define MAX_BAR_LEN  200
//len:进度条'#'的长度
void print_progress_bar(const char *title, unsigned char percent, unsigned int len, const char *style)
{
    char bar[MAX_BAR_LEN + 1];
    struct winsize size;
    size_t title_len;
    unsigned int ws_col;
    unsigned int first;


    if ((NULL == title) || (0 == isatty(fileno(stdout))))
        return;

    if (100 < percent)
        percent = 100;

    if (10 > len)
    {
        title_len = strlen(title);
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        ws_col = size.ws_col;
        if (0 == ws_col)
            ws_col = 80;

        if (ws_col > title_len + 10 + 11 + 3)
            len = ws_col - title_len - 11 - 3;
        else
            len = 10;
    }
    if (MAX_BAR_LEN < len)
        len = MAX_BAR_LEN;

    first = len * percent / 100;
    memset(bar, '#', first);
    memset(bar + first, '.', len - first);
    bar[len] = '\0';
    if (style)
        printf("%s", style);
    printf("\r%s\e[0m: [%3d%%] [%s]", title, percent, bar);
    fflush(stdout);
}




int System(const char *fmt, ...)
{
    char buf[300];
    va_list args;

    if (NULL == fmt)
        return -1;

    va_start(args, fmt);
    if (sizeof(buf) <= vsnprintf(buf, sizeof(buf), fmt, args))
        return -1;

    return system(buf);
}


int get_by_cmd(char *buf, int len, const char *fmt, ...)
{
    va_list args;
    char cmd[300];
    FILE *fp;
    size_t ret;

    if ((NULL == buf) || (2 > len) || (NULL == fmt))
        return -1;

    va_start(args, fmt);
    if (sizeof(cmd) <= vsnprintf(cmd, sizeof(cmd), fmt, args))
        return -1;

    fp = popen(cmd, "r");
    if (!fp)
        return -1;

    ret = fread(buf, 1, len - 1, fp);
    pclose(fp);
    buf[ret] = 0;

    return (int)ret;
}

unsigned int get_line_by_cmd(char *buf, unsigned int len, const char *fmt, ...)
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
        printf("vsnprintf err: %s !", strerror(errno));
        buf[0] = 0;
        return 0;
    }
    if (sizeof(cmd) <= _len)
    {
        printf("cmd len(%d) exceeds the buf len(%zu) !", _len, sizeof(cmd));
        buf[0] = 0;
        return 0;
    }

    fp = popen(cmd, "r");
    if (!fp)
    {
        printf("popen failed, cmd: %s, err: %s !", cmd, strerror(errno));
        buf[0] = 0;
        return 0;
    }

    if (NULL == fgets(buf, len, fp))
    {
        printf("fgets err: %s !", strerror(errno));
        pclose(fp);
        buf[0] = 0;
        return 0;
    }

    pclose(fp);
    return (unsigned int)strlen(buf);
}
