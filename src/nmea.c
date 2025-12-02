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
    {
        decimal *= 10;
    }

    *out = (int32_t)integer * factor + (int32_t)decimal;
    if (is_negative)
        *out = -(*out);

    while (('0' <= *p) && ('9' >= *p))
    {
        p++;
    }

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
    {
        m2 *= 10;
    }

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
    const char *p;
    int32_t v;

    if (',' != *str)
    {
        v = -1;
        p = str_to_int(str, 3, &v);
        if ((NULL == p) || (0 > v) || (235959999 < v))
        {
            LOG(LOGLEVEL_ERROR, "gga time err !");
            return -1;
        }
        gga->utc_hour = v / 10000000;
        gga->utc_minute = v / 100000 - (v / 10000000) * 100;
        gga->utc_second = v / 1000 - (v / 100000) * 100;
        gga->utc_usecond = v - (v / 1000) * 1000;
        str = p;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga time is none");
        gga->time_valid = 0;
    }

    /* latitude */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga latitude not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_dm_to_d(str, 9, &gga->latitude);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga latitude err !");
            return -1;
        }
        str = p;
    }
    else
    {
        if (',' != *++str)
        {
            LOG(LOGLEVEL_ERROR, "gga latitude format err !");
            return -1;
        }
        LOG(LOGLEVEL_DEBUG, "gga latitude is none");
        gga->latitude_valid = 0;
    }

    /* longitude */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga longitude not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_dm_to_d(str, 9, &gga->longitude);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga longitude err !");
            return -1;
        }
        str = p;
    }
    else
    {
        if (',' != *++str)
        {
            LOG(LOGLEVEL_ERROR, "gga longitude format err !");
            return -1;
        }
        LOG(LOGLEVEL_DEBUG, "gga longitude is none");
        gga->longitude_valid = 0;
    }

    /* quality */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga quality not exist !");
        return -1;
    }
    if ('0' == *str)
        gga->quality = GGA_QUALITY_INVALID;
    else if ('1' == *str)
        gga->quality = GGA_QUALITY_VALID;
    else if ('2' == *str)
        gga->quality = GGA_QUALITY_DIFF;
    else if ('3' == *str)
        gga->quality = GGA_QUALITY_PPS;
    else if ('4' == *str)
        gga->quality = GGA_QUALITY_RTK_FIXED;
    else if ('5' == *str)
        gga->quality = GGA_QUALITY_RTK_FLOATING;
    else if ('6' == *str)
        gga->quality = GGA_QUALITY_ESTIMATED;
    else if ('7' == *str)
        gga->quality = GGA_QUALITY_MANUAL;
    else if ('8' == *str)
        gga->quality = GGA_QUALITY_SIMULATOR;
    else
    {
        LOG(LOGLEVEL_ERROR, "unsupported gga quality: '%c' !", *str);
        return -1;
    }
    str++;

    /* satellites */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga satellites not exist !");
        return -1;
    }
    v = -1;
    p = str_to_int(str, 0, &v);
    if ((NULL == p) || (0 > v))
    {
        LOG(LOGLEVEL_ERROR, "gga satellites err !");
        return -1;
    }
    gga->satellites = (typeof(gga->satellites))v;
    str = p;

    /* dhop */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga dhop not exist !");
        return -1;
    }
    if (',' != *str)
    {
        v = -1;
        p = str_to_int(str, 2, &v);
        if ((NULL == p) || (0 > v))
        {
            LOG(LOGLEVEL_ERROR, "gga dhop err !");
            return -1;
        }
        gga->dhop = (typeof(gga->dhop))v;
        str = p;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga dhop is none");
        gga->dhop_valid = 0;
    }


    /* altitude */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga altitude not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_to_int(str, 3, &v);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga altitude err !");
            return -1;
        }
        gga->altitude = (typeof(gga->altitude))v;
        str = p;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga altitude is none");
        gga->altitude_valid = 0;
    }
    if ((',' != str[0]) || 'M' != str[1])
    {
        LOG(LOGLEVEL_ERROR, "gga altitude format err !");
        return -1;
    }
    str += 2;

    /* geoidal_separation */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga geoidal_separation not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_to_int(str, 3, &v);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga geoidal_separation err !");
            return -1;
        }
        gga->geoidal_separation = (typeof(gga->geoidal_separation))v;
        str = p;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga geoidal_separation is none");
        gga->geoidal_separation_valid = 0;
    }
    if ((',' != str[0]) || 'M' != str[1])
    {
        LOG(LOGLEVEL_ERROR, "gga geoidal_separation format err !");
        return -1;
    }
    str += 2;

    /* age_of_diff_data */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga age_of_diff_data not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_to_int(str, 1, &v);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga age_of_diff_data err !");
            return -1;
        }
        gga->age_of_diff_data = (typeof(gga->age_of_diff_data))v;
        str = p;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "gga age_of_diff_data is none");
        gga->age_of_diff_data_valid = 0;
        gga->diff_station_id_valid = 0;
        return 0;
    }

    /* diff_station_id */
    if (',' != *str++)
    {
        LOG(LOGLEVEL_ERROR, "gga diff_station_id not exist !");
        return -1;
    }
    if (',' != *str)
    {
        p = str_to_int(str, 0, &v);
        if (NULL == p)
        {
            LOG(LOGLEVEL_ERROR, "gga diff_station_id err !");
            return -1;
        }
        gga->diff_station_id = (typeof(gga->diff_station_id))v;
        str = p;
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
        if (nmea_parse_gga(str + 4, &msg->gga))
            return NULL;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "unsupported format: '%c%c%c'", str[0], str[1], str[2]);
        msg->format_id = FORMAT_ID_UNKNOWN;
    }
    return (char *)end;
}
