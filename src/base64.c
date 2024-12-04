#include "base64.h"
#include <string.h>




static const char b16_enc[16] =
{
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F'
};
int base16_encode(const unsigned char *in, int inlen, char *out, int outlen)
{
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 >= inlen)
        return 0;
    if (inlen >= ((outlen + 1) / 2))
        return -1;

    for (i = 0; i < inlen; i++)
    {
        out[0] = b16_enc[in[i] >> 4];
        out[1] = b16_enc[in[i] & 0xf];
        out += 2;
    }

    *out = 0;
    return inlen * 2;
}
static inline char b16_dec(char c)
{
    if (('9' >= c) && ('0' <= c))
        return c - '0';
    else if (('F' >= c) && ('A' <= c))
        return c - 'A' + 10;
    else if (('f' >= c) && ('a' <= c))
        return c - 'a' + 10;
    else
        return -1;
}
int base16_decode(const char *in, int inlen, unsigned char *out, int outlen)
{
    char h, l;
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 > inlen)
        inlen = (int)strlen(in);
    if (0 >= inlen)
        return 0;
    if ((inlen % 2) || (inlen / 2 > outlen))
        return -1;

    inlen /= 2;
    for (i = 0; i < inlen; i++)
    {
        h = b16_dec(in[0]);
        l = b16_dec(in[1]);
        if ((0 > h) || (0 > l))
            return -1;

        *out = (h << 4) | l;

        in += 2;
        out++;
    }

    return inlen;
}


static const char b32_enc[32] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '2', '3', '4', '5', '6', '7'
};
int base32_encode(const unsigned char *in, int inlen, char *out, int outlen)
{
    int group, left;
    unsigned char tmp[7] = {0};
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 >= inlen)
        return 0;
    if (inlen > (((outlen + 7) / 8 - 1) * 5))
        return -1;

    group = inlen / 5;
    left = inlen % 5;
    outlen = (inlen + 4) / 5 * 8;
    for (i = 0; i < group; i++)
    {
        out[0] = b32_enc[in[0] >> 3];
        out[1] = b32_enc[((in[0] & 0x7) << 2) | (in[1] >> 6)];
        out[2] = b32_enc[(in[1] & 0x3e) >> 1];
        out[3] = b32_enc[((in[1] & 0x1) << 4) | (in[2] >> 4)];
        out[4] = b32_enc[((in[2] & 0xf) << 1) | (in[3] >> 7)];
        out[5] = b32_enc[(in[3] & 0x7c) >> 2];
        out[6] = b32_enc[((in[3] & 0x3) << 3) | (in[4] >> 5)];
        out[7] = b32_enc[in[4] & 0x1f];

        in += 5;
        out += 8;
    }

    if (!left)
    {
        *out = 0;
        return outlen;
    }

    switch (left)
    {
        case 4:
            tmp[6] = (in[3] & 0x3) << 3;
            tmp[5] = (in[3] & 0x7c) >> 2;
            tmp[4] = in[3] >> 7;
        case 3:
            tmp[4] |= (in[2] & 0xf) << 1;
            tmp[3] = in[2] >> 4;
        case 2:
            tmp[3] |= (in[1] & 0x1) << 4;
            tmp[2] = (in[1] & 0x3e) >> 1;
            tmp[1] = in[1] >> 6;
        case 1:
            tmp[1] |= (in[0] & 0x7) << 2;
            tmp[0] = in[0] >> 3;
    }
    for (i = 0; i < (left * 8 + 4) / 5; i++)
        out[i] = b32_enc[tmp[i]];
    for (; i < 8; i++)
        out[i] = '=';

    out[8] = 0;
    return outlen;
}
static inline char b32_dec(char c)
{
    if (('A' <= c) && ('Z' >= c))
        return c - 'A';
    else if (('a' <= c) && ('z' >= c))
        return c - 'a';
    else if (('2' <= c) && ('7' >= c))
        return c - '2' + 26;
    else
        return -1;
}
int base32_decode(const char *in, int inlen, unsigned char *out, int outlen)
{
    int group, left;
    int _outlen;
    char tmp[8];
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 > inlen)
        inlen = (int)strlen(in);
    if (0 >= inlen)
        return 0;

    for (; inlen > 0; inlen--)
    {
        if ('=' != in[inlen - 1])
            break;
    }
    if (0 >= inlen)
        return -1;

    group = inlen / 8;
    left = inlen % 8;
    if ((1 == left) || (3 == left) || (6 == left))
        return -1;

    _outlen = group * 5 + ((left + 1) / 2);
    if (_outlen > outlen)
        return -1;

    for (i = 0; i < group; i++)
    {
        tmp[0] = b32_dec(in[0]);
        tmp[1] = b32_dec(in[1]);
        tmp[2] = b32_dec(in[2]);
        tmp[3] = b32_dec(in[3]);
        tmp[4] = b32_dec(in[4]);
        tmp[5] = b32_dec(in[5]);
        tmp[6] = b32_dec(in[6]);
        tmp[7] = b32_dec(in[7]);
        if (0 > tmp[0] || 0 > tmp[1] || 0 > tmp[2] || 0 > tmp[3] ||
            0 > tmp[4] || 0 > tmp[5] || 0 > tmp[6] || 0 > tmp[7])
            return -1;

        out[0] = (tmp[0] << 3) | (tmp[1] >> 2);
        out[1] = (tmp[1] << 6) | (tmp[2] << 1) | (tmp[3] >> 4);
        out[2] = (tmp[3] << 4) | (tmp[4] >> 1);
        out[3] = (tmp[4] << 7) | (tmp[5] << 2) | (tmp[6] >> 3);
        out[4] = (tmp[6] << 5) | tmp[7];

        in += 8;
        out += 5;
    }

    if (!left)
        return _outlen;

    for (i = 0; i < left; i++)
    {
        tmp[i] = b32_dec(in[i]);
        if (0 > tmp[i])
            return -1;
    }
    switch (left)
    {
        case 7:
            out[3] = (tmp[4] << 7) | (tmp[5] << 2) | (tmp[6] >> 3);
        case 5:
            out[2] = (tmp[3] << 4) | (tmp[4] >> 1);
        case 4:
            out[1] = (tmp[1] << 6) | (tmp[2] << 1) | (tmp[3] >> 4);
        case 2:
            out[0] = (tmp[0] << 3) | (tmp[1] >> 2);
    }

    return _outlen;
}


static const char b64_enc[64] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};
int base64_encode(const unsigned char *in, int inlen, char *out, int outlen)
{
    int group, left;
    unsigned char tmp[3] = {0};
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 >= inlen)
        return 0;
    if (inlen > (((outlen + 3) / 4 - 1) * 3))
        return -1;

    group = inlen / 3;
    left = inlen % 3;
    outlen = (inlen + 2) / 3 * 4;
    for (i = 0; i < group; i++)
    {
        out[0] = b64_enc[in[0] >> 2];
        out[1] = b64_enc[((in[0] & 0x3) << 4) | (in[1] >> 4)];
        out[2] = b64_enc[((in[1] & 0xf) << 2) | (in[2] >> 6)];
        out[3] = b64_enc[in[2] & 0x3f];

        in += 3;
        out += 4;
    }

    if (!left)
    {
        *out = 0;
        return outlen;
    }

    switch (left)
    {
        case 2:
            tmp[2] = (in[1] & 0xf) << 2;
            tmp[1] = in[1] >> 4;
        case 1:
            tmp[1] |= (in[0] & 0x3) << 4;
            tmp[0] = in[0] >> 2;
    }
    for (i = 0; i < (left + 1); i++)
        out[i] = b64_enc[tmp[i]];
    for (; i < 4; i++)
        out[i] = '=';

    out[4] = 0;
    return outlen;
}
static inline char b64_dec(char c)
{
    if (('A' <= c) && ('Z' >= c))
        return c - 'A';
    else if (('a' <= c) && ('z' >= c))
        return c - 'a' + 26;
    else if (('0' <= c) && ('9' >= c))
        return c - '0' + 52;
    else if ('+' == c)
        return 62;
    else if ('/' == c)
        return 63;
    else
        return -1;
}
int base64_decode(const char *in, int inlen, unsigned char *out, int outlen)
{
    int group, left;
    int _outlen;
    char tmp[4];
    int i;

    if ((NULL == in) || (NULL == out))
        return -1;
    if (0 > inlen)
        inlen = (int)strlen(in);
    if (0 >= inlen)
        return 0;

    for (; inlen > 0; inlen--)
    {
        if ('=' != in[inlen - 1])
            break;
    }
    if (0 >= inlen)
        return -1;

    group = inlen / 4;
    left = inlen % 4;
    if (1 == left)
        return -1;

    _outlen = group * 3 + ((left + 1) / 2);
    if (_outlen > outlen)
        return -1;

    for (i = 0; i < group; i++)
    {
        tmp[0] = b64_dec(in[0]);
        tmp[1] = b64_dec(in[1]);
        tmp[2] = b64_dec(in[2]);
        tmp[3] = b64_dec(in[3]);
        if (0 > tmp[0] || 0 > tmp[1] || 0 > tmp[2] || 0 > tmp[3])
            return -1;

        out[0] = (tmp[0] << 2) | (tmp[1] >> 4);
        out[1] = (tmp[1] << 4) | (tmp[2] >> 2);
        out[2] = (tmp[2] << 6) | tmp[3];

        in += 4;
        out += 3;
    }

    if (!left)
        return _outlen;

    for (i = 0; i < left; i++)
    {
        tmp[i] = b64_dec(in[i]);
        if (0 > tmp[i])
            return -1;
    }
    switch (left)
    {
        case 3:
            out[1] = (tmp[1] << 4) | (tmp[2] >> 2);
        case 2:
            out[0] = (tmp[0] << 2) | (tmp[1] >> 4);
    }

    return _outlen;
}
