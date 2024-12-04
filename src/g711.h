#ifndef _G711_H_
#define _G711_H_


extern void g711a_encode(const short *pcm, int cnt, unsigned char *out);
extern void g711u_encode(const short *pcm, int cnt, unsigned char *out);
extern void g711a_decode(const unsigned char *a, int cnt, short *out);
extern void g711u_decode(const unsigned char *u, int cnt, short *out);


#endif
