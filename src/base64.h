#ifndef _BASE64_H_
#define _BASE64_H_


/* xx_encode以'\0'结尾，返回实际写入字符串长度(不包括最后的'\0')，失败返回-1 */
extern int base16_encode(const unsigned char *in, int inlen, char *out, int outlen);
extern int base16_decode(const char *in, int inlen, unsigned char *out, int outlen);

extern int base32_encode(const unsigned char *in, int inlen, char *out, int outlen);
extern int base32_decode(const char *in, int inlen, unsigned char *out, int outlen);

extern int base64_encode(const unsigned char *in, int inlen, char *out, int outlen);
extern int base64_decode(const char *in, int inlen, unsigned char *out, int outlen);


#endif
