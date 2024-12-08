#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_


typedef struct
{
    unsigned char *buf;
    unsigned int buf_size;
    unsigned int member_size;
    unsigned int member_count;
    unsigned int head;
    unsigned int cur;
    unsigned int tail;  /* 永远指向空节点 */
}ring_buffer_t;

#define RBUF_INITIALIZER(_buf, _buf_size, _member_size) {.buf=(_buf), .buf_size=(_buf_size), .member_size=(_member_size), .member_count=(_buf_size)/(_member_size), .head=0, .cur=0, .tail=0}
extern int rbuf_init(ring_buffer_t *ring_buf, unsigned char *buf, unsigned int buf_size, unsigned int member_size);
extern unsigned int rbuf_put(ring_buffer_t *ring_buf, const unsigned char *data, unsigned int count, unsigned char full_mode);
extern unsigned int rbuf_get(ring_buffer_t *ring_buf, unsigned char **data, unsigned int count);


#endif
