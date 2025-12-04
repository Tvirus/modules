#include "nmea.h"
#include <string.h>
#include <stdlib.h>
#include "log.h"




#define LOGLEVEL_ERROR 1
#define LOGLEVEL_INFO 2
#define LOGLEVEL_DEBUG 3
#define LOG(level, fmt, arg...)  do{if((level) <= nmea_log_level)log_printf("--NMEA-- " fmt "\n", ##arg);}while(0)
unsigned char nmea_log_level = LOGLEVEL_INFO;


static char* check_sentence(const char *str)
{
    unsigned char checksum = 0;
    char h;
    char l;

    if (NULL == str)
        return NULL;
    if ('$' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "sentence does not start with '$' !");
        return NULL;
    }
    if ('*' == *str)
    {
        LOG(LOGLEVEL_ERROR, "sentence is empty !");
        return NULL;
    }
    checksum = *str++;
    while ('*' != *str)
    {
        if ('\0' == *str)
        {
            LOG(LOGLEVEL_ERROR, "sentence does not end with '*' !");
            return NULL;
        }
        checksum ^= *str++;
    }
    str++;

    if (0xa > (checksum >> 4))
        h = (checksum >> 4) + '0';
    else
        h = (checksum >> 4) - 0xa + 'A';
    if (0xa > (checksum & 0xf))
        l = (checksum & 0xf) + '0';
    else
        l = (checksum & 0xf) - 0xa + 'A';
    if ((h != str[0]) || (l != str[1]))
    {
        LOG(LOGLEVEL_ERROR, "sentence checksum err !");
        return NULL;
    }
    str += 2;

    if (('\r' == str[0]) && ('\n' == str[1]))
    {
        return (char *)str + 2;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "sentence does not end with '<CR><LF>'");
        return (char *)str;
    }
}

static void str_sep(const char *str, char **p, unsigned int max)
{
    while (('*' != *str) && ('\r' != *str) && ('\n' != *str) && ('\0' != *str) && max)
    {
        if (',' == *str)
        {
            *p++ = (char *)str;
            max--;
        }
        str++;
    }
    while (max--)
        *p++ = NULL;
}

static char* str_to_int(const char *str, unsigned char exponent, int32_t *out)
{
    unsigned char is_negative;
    uint32_t factor;
    uint32_t integer;
    uint32_t decimal;
    char *p = NULL;
    int i;

    if ('+' == *str)
    {
        is_negative = 0;
        str++;
    }
    else if ('-' == *str)
    {
        is_negative = 1;
        str++;
    }
    else
        is_negative = 0;

    if (('0' > *str) || ('9' < *str) || (4 < exponent))
        return NULL;
    factor = 1;
    for (i = 0; i < exponent; i++)
        factor *= 10;

    integer = (uint32_t)strtoul(str, &p, 10);
    if ((NULL == p) || ('.' != *p))
    {
        *out = (int32_t)integer * factor;
        if (is_negative)
            *out = -(*out);
        return p;
    }
    p++;

    if (('0' > *p) || ('9' < *p))
        return NULL;
    decimal = 0;
    for (i = 0; i < exponent; i++)
    {
        if (('0' > *p) || ('9' < *p))
            break;
        decimal = decimal * 10 + *p++ - '0';
    }
    for (; i < exponent; i++)
        decimal *= 10;

    *out = (int32_t)integer * factor + (int32_t)decimal;
    if (is_negative)
        *out = -(*out);

    while (('0' <= *p) && ('9' >= *p))
        p++;
    return p;
}

static char* str_dm_to_d(const char *dm, unsigned char exponent, int64_t *degree)
{
    uint32_t factor;
    uint32_t integer;
    uint32_t d;
    uint32_t m1;
    uint32_t m2;
    char *p = NULL;
    int i;

    if (('0' > *dm) || ('9' < *dm) || (9 < exponent))
        return NULL;
    factor = 1;
    for (i = 0; i < exponent; i++)
        factor *= 10;

    integer = (uint32_t)strtoul(dm, &p, 10);

    if ((NULL == p) || ('.' != *p))
        return NULL;
    m2 = 0;
    p++;
    for (i = 0; i < exponent; i++)
    {
        if (('0' > *p) || ('9' < *p))
            break;
        m2 = m2 * 10 + *p++ - '0';
    }
    for (; i < exponent; i++)
        m2 *= 10;

    d = integer / 100;
    m1 = integer % 100;
    *degree = ((uint64_t)d) * factor + ((uint64_t)m1) * factor / 60 + ((uint64_t)m2) / 60;

    if (',' != *p++)
        return NULL;
    if (('N' == *p) || ('E' == *p))
        ;
    else if (('S' == *p) || ('W' == *p))
        *degree = -(*degree);
    else
        return NULL;

    return p + 1;
}

static int nmea_parse_gga(const char *str, format_gga_t *gga)
{
    char *p[14];
    int32_t val;
    char c;

    str_sep(str, p, sizeof(p) / sizeof(p[0]));

    /* time */
    if (NULL == p[0])
    {
        LOG(LOGLEVEL_ERROR, "gga time not exist !");
        return -1;
    }
    if (',' != *(p[0] + 1))
    {
        if ((NULL == str_to_int(p[0] + 1, 3, &val)) || (0 > val) || (235959999 < val))
        {
            LOG(LOGLEVEL_ERROR, "gga time format err !");
            return -1;
        }
        gga->utc_hour = val / 10000000;
        gga->utc_minute = val / 100000 - (val / 10000000) * 100;
        gga->utc_second = val / 1000 - (val / 100000) * 100;
        gga->utc_usecond = val - (val / 1000) * 1000;
        gga->time_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga time is none");
        gga->time_valid = 0;
    }

    /* latitude */
    if (NULL == p[1])
    {
        LOG(LOGLEVEL_ERROR, "gga latitude not exist !");
        return -1;
    }
    if (',' != *(p[1] + 1))
    {
        if (NULL == str_dm_to_d(p[1] + 1, 9, &gga->latitude))
        {
            LOG(LOGLEVEL_ERROR, "gga latitude format err !");
            return -1;
        }
        gga->latitude_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga latitude is none");
        gga->latitude_valid = 0;
    }

    /* longitude */
    if (NULL == p[3])
    {
        LOG(LOGLEVEL_ERROR, "gga longitude not exist !");
        return -1;
    }
    if (',' != *(p[3] + 1))
    {
        if (NULL == str_dm_to_d(p[3] + 1, 9, &gga->longitude))
        {
            LOG(LOGLEVEL_ERROR, "gga longitude format err !");
            return -1;
        }
        gga->longitude_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga longitude is none");
        gga->longitude_valid = 0;
    }

    /* quality */
    if (NULL == p[5])
    {
        LOG(LOGLEVEL_ERROR, "gga quality not exist !");
        return -1;
    }
    c = *(p[5] + 1);
    if (',' != c)
    {
        if ('0' == c)
            gga->quality = GGA_QUALITY_INVALID;
        else if ('1' == c)
            gga->quality = GGA_QUALITY_VALID;
        else if ('2' == c)
            gga->quality = GGA_QUALITY_DIFF;
        else if ('3' == c)
            gga->quality = GGA_QUALITY_PPS;
        else if ('4' == c)
            gga->quality = GGA_QUALITY_RTK_FIXED;
        else if ('5' == c)
            gga->quality = GGA_QUALITY_RTK_FLOATING;
        else if ('6' == c)
            gga->quality = GGA_QUALITY_ESTIMATED;
        else if ('7' == c)
            gga->quality = GGA_QUALITY_MANUAL;
        else if ('8' == c)
            gga->quality = GGA_QUALITY_SIMULATOR;
        else
        {
            LOG(LOGLEVEL_ERROR, "unsupported gga quality: '%c' !", c);
            return -1;
        }
    }
    else
    {
        LOG(LOGLEVEL_ERROR, "gga quality is none !");
        return -1;
    }

    /* satellites */
    if (NULL == p[6])
    {
        LOG(LOGLEVEL_ERROR, "gga satellites not exist !");
        return -1;
    }
    if (',' != *(p[6] + 1))
    {
        if ((NULL == str_to_int(p[6] + 1, 0, &val)) || (0 > val))
        {
            LOG(LOGLEVEL_ERROR, "gga satellites format err !");
            return -1;
        }
        gga->satellites = (typeof(gga->satellites))val;
    }
    else
    {
        LOG(LOGLEVEL_ERROR, "gga satellites is none !");
        return -1;
    }

    /* dhop */
    if (NULL == p[7])
    {
        LOG(LOGLEVEL_ERROR, "gga dhop not exist !");
        return -1;
    }
    if (',' != *(p[7] + 1))
    {
        if ((NULL == str_to_int(p[7] + 1, 2, &val)) || (0 > val))
        {
            LOG(LOGLEVEL_ERROR, "gga dhop format err !");
            return -1;
        }
        gga->dhop = (typeof(gga->dhop))val;
        gga->dhop_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga dhop is none");
        gga->dhop_valid = 0;
    }

    /* altitude */
    if (NULL == p[8])
    {
        LOG(LOGLEVEL_ERROR, "gga altitude not exist !");
        return -1;
    }
    if (',' != *(p[8] + 1))
    {
        if (NULL == str_to_int(p[8] + 1, 3, &val))
        {
            LOG(LOGLEVEL_ERROR, "gga altitude format err !");
            return -1;
        }
        gga->altitude = (typeof(gga->altitude))val;
        gga->altitude_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga altitude is none");
        gga->altitude_valid = 0;
    }
    if ((NULL == p[9]) || 'M' != *(p[9] + 1))
    {
        LOG(LOGLEVEL_ERROR, "gga altitude unit err !");
        return -1;
    }

    /* geoidal_separation */
    if (NULL == p[10])
    {
        LOG(LOGLEVEL_ERROR, "gga geoidal_separation not exist !");
        return -1;
    }
    if (',' != *(p[10] + 1))
    {
        if (NULL == str_to_int(p[10] + 1, 3, &val))
        {
            LOG(LOGLEVEL_ERROR, "gga geoidal_separation format err !");
            return -1;
        }
        gga->geoidal_separation = (typeof(gga->geoidal_separation))val;
        gga->geoidal_separation_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga geoidal_separation is none");
        gga->geoidal_separation_valid = 0;
    }
    if ((NULL == p[11]) || 'M' != *(p[11] + 1))
    {
        LOG(LOGLEVEL_ERROR, "gga geoidal_separation unit err !");
        return -1;
    }

    /* age_of_diff_data */
    if (NULL == p[12])
    {
        LOG(LOGLEVEL_ERROR, "gga age_of_diff_data not exist !");
        return -1;
    }
    if (',' != *(p[12] + 1))
    {
        if (NULL == str_to_int(p[12] + 1, 1, &val))
        {
            LOG(LOGLEVEL_ERROR, "gga age_of_diff_data format err !");
            return -1;
        }
        gga->age_of_diff_data = (typeof(gga->age_of_diff_data))val;
        gga->age_of_diff_data_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga age_of_diff_data is none");
        gga->age_of_diff_data_valid = 0;
        gga->diff_station_id_valid = 0;
        return 0;
    }

    /* diff_station_id */
    if (NULL == p[13])
    {
        LOG(LOGLEVEL_ERROR, "gga diff_station_id not exist !");
        return -1;
    }
    if (',' != *(p[13] + 1))
    {
        if (NULL == str_to_int(p[13] + 1, 0, &val))
        {
            LOG(LOGLEVEL_ERROR, "gga diff_station_id format err !");
            return -1;
        }
        gga->diff_station_id = (typeof(gga->diff_station_id))val;
        gga->diff_station_id_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga diff_station_id is none");
        gga->diff_station_id_valid = 0;
    }

    return 0;
}

char* nmea_parse(const char *str, nmea_msg_t *msg)
{
    const char *end;

    end = check_sentence(str);
    if (NULL == end)
        return NULL;

    str++;
    if (('G' == str[0]) && ('N' == str[1]))
        msg->talker_id = TALKER_ID_GNSS;
    else if (('G' == str[0]) && ('P' == str[1]))
        msg->talker_id = TALKER_ID_GPS;
    else if (('B' == str[0]) && ('D' == str[1]))
        msg->talker_id = TALKER_ID_BDS;
    else if (('G' == str[0]) && ('A' == str[1]))
        msg->talker_id = TALKER_ID_GALILEO;
    else if (('G' == str[0]) && ('L' == str[1]))
        msg->talker_id = TALKER_ID_GLONASS;
    else if (('A' <= str[0]) && ('Z' >= str[0]) && ('A' <= str[1]) && ('Z' >= str[1]))
        msg->talker_id = TALKER_ID_UNKNOWN;
    else
    {
        LOG(LOGLEVEL_ERROR, "talker id err !");
        return NULL;
    }

    str += 2;
    if (!memcmp(str, "GGA,", 4))
    {
        msg->format_id = FORMAT_ID_GGA;
        if (nmea_parse_gga(str, &msg->gga))
            return NULL;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "unsupported format: '%c%c%c'", str[0], str[1], str[2]);
        msg->format_id = FORMAT_ID_UNKNOWN;
    }
    return (char *)end;
}
