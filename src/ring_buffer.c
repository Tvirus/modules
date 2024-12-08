#include "ring_buffer.h"
#include <string.h>


int rbuf_init(ring_buffer_t *ring_buf, unsigned char *buf, unsigned int buf_size, unsigned int member_size)
{
    if ((NULL == ring_buf) || (NULL == buf) || (0 >= buf_size) || (0 >= member_size))
        return -1;

    ring_buf->buf = buf;
    ring_buf->buf_size = buf_size;
    ring_buf->member_size = member_size;
    ring_buf->member_count = buf_size / member_size;
    ring_buf->head = 0;
    ring_buf->cur = 0;
    ring_buf->tail = 0;
    if (2 > ring_buf->member_count)
        return -1;

    return 0;
}

unsigned int rbuf_put(ring_buffer_t *ring_buf, const unsigned char *data, unsigned int count, unsigned char full_mode)
{
    unsigned char *buf;
    unsigned int member_size;
    unsigned int member_count;
    unsigned int head;
    unsigned int tail;
    unsigned int max;
    unsigned int n1;


    if ((NULL == ring_buf) || (NULL == data) || (0 >= count))
        return 0;

    buf = ring_buf->buf;
    member_size = ring_buf->member_size;
    member_count = ring_buf->member_count;
    head = ring_buf->head;
    tail = ring_buf->tail;
    if (   (NULL == buf)
        || (2 > member_count)
        || (member_count <= head)
        || (member_count <= tail))
        return 0;
    if ((tail + 1 == head) || ((0 == head) && (tail == member_count - 1)))
        return 0;

    if (tail < head)
    {
        max = head - tail - 1;
        if (max < count)
        {
            if (full_mode)
                return 0;
            else
                count = max;
        }
        memcpy(buf + (tail * member_size), data, count * member_size);
        ring_buf->tail = tail + count;
        return count;
    }

    max = member_count - tail + head - 1;
    if (max < count)
    {
        if (full_mode)
            return 0;
        else
            count = max;
    }
    n1 = member_count - tail;
    if (count < n1)
    {
        memcpy(buf + (tail * member_size), data, count * member_size);
        ring_buf->tail = tail + count;
        return count;
    }

    memcpy(buf + (tail * member_size), data, n1 * member_size);
    memcpy(buf, data + n1 * member_size, (count - n1) * member_size);
    ring_buf->tail = tail + count - member_count;
    return count;
}

unsigned int rbuf_get(ring_buffer_t *ring_buf, unsigned char **data, unsigned int count)
{
    unsigned char *buf;
    unsigned int member_count;
    unsigned int head;
    unsigned int cur;
    unsigned int tail;
    unsigned int max;


    if ((NULL == ring_buf) || (NULL == data) || (0 >= count))
        return 0;

    buf = ring_buf->buf;
    member_count = ring_buf->member_count;
    head = ring_buf->head;
    cur = ring_buf->cur;
    tail = ring_buf->tail;
    if (   (NULL == buf)
        || (2 > member_count)
        || (member_count <= head)
        || (member_count <= tail))
        return 0;
    if ((head <= tail) && ((cur < head) || (tail < cur)))
        return 0;
    if ((tail < head) && (tail < cur) && (cur < head))
        return 0;

    ring_buf->head = cur;
    head = cur;

    if (head == tail)
        return 0;
    if (head < tail)
        max = tail - head;
    else
        max = member_count - head;
    if (max < count)
        count = max;
    cur += count;
    if (member_count <= cur)
        ring_buf->cur = 0;
    else
        ring_buf->cur = cur;
    *data = buf + (head * ring_buf->member_size);
    return count;
}
