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
        LOG(LOGLEVEL_DEBUG, "sentence does not start with '$' !");
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
            LOG(LOGLEVEL_ERROR, "unsupported gga quality: 0x%02x !", c);
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

static int nmea_parse_rmc(const char *str, format_rmc_t *rmc)
{
    char *p[13];
    int32_t val;
    char c;

    str_sep(str, p, sizeof(p) / sizeof(p[0]));

    /* time */
    if (NULL == p[0])
    {
        LOG(LOGLEVEL_ERROR, "rmc time not exist !");
        return -1;
    }
    if (',' != *(p[0] + 1))
    {
        if ((NULL == str_to_int(p[0] + 1, 3, &val)) || (0 > val) || (235959999 < val))
        {
            LOG(LOGLEVEL_ERROR, "rmc time format err !");
            return -1;
        }
        rmc->utc_hour = val / 10000000;
        rmc->utc_minute = val / 100000 - (val / 10000000) * 100;
        rmc->utc_second = val / 1000 - (val / 100000) * 100;
        rmc->utc_usecond = val - (val / 1000) * 1000;
        rmc->time_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc time is none");
        rmc->time_valid = 0;
    }

    /* status */
    if (NULL == p[1])
    {
        LOG(LOGLEVEL_ERROR, "rmc status not exist !");
        return -1;
    }
    c = *(p[1] + 1);
    if (',' != c)
    {
        if ('V' == c)
            rmc->status = RMC_STATUS_INVALID;
        else if ('A' == c)
            rmc->status = RMC_STATUS_AUTONOMOUS;
        else if ('D' == c)
            rmc->status = RMC_STATUS_DIFFERENTIAL;
        else
        {
            LOG(LOGLEVEL_ERROR, "unsupported rmc status: 0x%02x !", c);
            return -1;
        }
    }
    else
    {
        LOG(LOGLEVEL_ERROR, "rmc status is none !");
        return -1;
    }

    /* latitude */
    if (NULL == p[2])
    {
        LOG(LOGLEVEL_ERROR, "rmc latitude not exist !");
        return -1;
    }
    if (',' != *(p[2] + 1))
    {
        if (NULL == str_dm_to_d(p[2] + 1, 9, &rmc->latitude))
        {
            LOG(LOGLEVEL_ERROR, "rmc latitude format err !");
            return -1;
        }
        rmc->latitude_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc latitude is none");
        rmc->latitude_valid = 0;
    }

    /* longitude */
    if (NULL == p[4])
    {
        LOG(LOGLEVEL_ERROR, "rmc longitude not exist !");
        return -1;
    }
    if (',' != *(p[4] + 1))
    {
        if (NULL == str_dm_to_d(p[4] + 1, 9, &rmc->longitude))
        {
            LOG(LOGLEVEL_ERROR, "rmc longitude format err !");
            return -1;
        }
        rmc->longitude_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc longitude is none");
        rmc->longitude_valid = 0;
    }

    /* speed */
    if (NULL == p[6])
    {
        LOG(LOGLEVEL_ERROR, "rmc speed not exist !");
        return -1;
    }
    if (',' != *(p[6] + 1))
    {
        if ((NULL == str_to_int(p[6] + 1, 3, &val)) || (0 > val))
        {
            LOG(LOGLEVEL_ERROR, "rmc speed format err !");
            return -1;
        }
        rmc->speed = val;
        rmc->speed_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc speed is none");
        rmc->speed_valid = 0;
    }

    /* course */
    if (NULL == p[7])
    {
        LOG(LOGLEVEL_ERROR, "rmc course not exist !");
        return -1;
    }
    if (',' != *(p[7] + 1))
    {
        if ((NULL == str_to_int(p[7] + 1, 2, &val)) || (0 > val) || (36000 <= val))
        {
            LOG(LOGLEVEL_ERROR, "rmc course format err !");
            return -1;
        }
        rmc->course = val;
        rmc->course_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc course is none");
        rmc->course_valid = 0;
    }

    /* date */
    if (NULL == p[8])
    {
        LOG(LOGLEVEL_ERROR, "rmc date not exist !");
        return -1;
    }
    if (',' != *(p[8] + 1))
    {
        if ((NULL == str_to_int(p[8] + 1, 0, &val)) || (0 > val))
        {
            LOG(LOGLEVEL_ERROR, "rmc date format err !");
            return -1;
        }
        rmc->day   = val / 10000;
        rmc->month = val / 100 - (val / 10000) * 100;
        rmc->year   = val - (val / 100) * 100;
        if ((0 == rmc->day) || (31 < rmc->day) || (0 == rmc->month) || (12 < rmc->month) || (25 > rmc->year))
        {
            LOG(LOGLEVEL_ERROR, "rmc date format err: %d %u-%u-%u !", val, rmc->day, rmc->month, rmc->year);
            return -1;
        }
        rmc->date_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc date is none");
        rmc->date_valid = 0;
    }

    /* declination */
    if ((NULL == p[9]) || (NULL == p[10]))
    {
        LOG(LOGLEVEL_ERROR, "rmc declination not exist !");
        return -1;
    }
    if (',' != *(p[9] + 1))
    {
        if ((NULL == str_to_int(p[9] + 1, 1, &val)) || (0 > val))
        {
            LOG(LOGLEVEL_ERROR, "rmc declination format err !");
            return -1;
        }
        rmc->declination = val;
        c = *(p[10] + 1);
        if ('W' == c)
        {
            rmc->declination = -rmc->declination;
        }
        else if ('E' == c)
        {
            ;
        }
        else
        {
            LOG(LOGLEVEL_ERROR, "rmc declination direction format err: 0x%02x !", c);
            return -1;
        }
        rmc->declination_valid = 1;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "rmc declination is none");
        rmc->declination_valid = 0;
    }

    /* mode */
    if (NULL == p[11])
    {
        LOG(LOGLEVEL_ERROR, "rmc mode not exist !");
        return -1;
    }
    c = *(p[11] + 1);
    if (',' != c)
    {
        if ('A' == c)
            rmc->mode = RMC_MODE_AUTONOMOUS;
        else if ('D' == c)
            rmc->mode = RMC_MODE_DIFFERENTIAL;
        else if ('E' == c)
            rmc->mode = RMC_MODE_ESTIMATED;
        else if ('F' == c)
            rmc->mode = RMC_MODE_FLOAT_RTK;
        else if ('M' == c)
            rmc->mode = RMC_MODE_MANUAL;
        else if ('N' == c)
            rmc->mode = RMC_MODE_NO_FIX;
        else if ('P' == c)
            rmc->mode = RMC_MODE_PRECISE;
        else if ('R' == c)
            rmc->mode = RMC_MODE_RTK;
        else if ('S' == c)
            rmc->mode = RMC_MODE_SIMULATOR;
        else
        {
            LOG(LOGLEVEL_ERROR, "rmc mode is unknown: 0x%02x !", c);
            return -1;
        }
    }
    else
    {
        LOG(LOGLEVEL_ERROR, "rmc mode is none !");
        return -1;
    }

    /* navigational status */
    if (NULL == p[12])
    {
        LOG(LOGLEVEL_DEBUG, "rmc navigational not exist");
        rmc->navigational = RMC_NAVIGATIONAL_NULL;
        return 0;
    }
    c = *(p[12] + 1);
    if ('*' != c)
    {
        if ('C' == c)
            rmc->navigational = RMC_NAVIGATIONAL_CAUTION;
        else if ('S' == c)
            rmc->navigational = RMC_NAVIGATIONAL_SAFE;
        else if ('U' == c)
            rmc->navigational = RMC_NAVIGATIONAL_UNSAFE;
        else if ('V' == c)
            rmc->navigational = RMC_NAVIGATIONAL_INVALID;
        else
        {
            LOG(LOGLEVEL_ERROR, "rmc navigational status is unknown: 0x%02x !", c);
            return -1;
        }
    }
    else
    {
        LOG(LOGLEVEL_ERROR, "rmc navigational is none !");
        return -1;
    }

    return 0;
}

/* 返回当前语句的末尾 */
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
    else if (!memcmp(str, "RMC,", 4))
    {
        msg->format_id = FORMAT_ID_RMC;
        if (nmea_parse_rmc(str, &msg->rmc))
            return NULL;
    }
    else
    {
        LOG(LOGLEVEL_DEBUG, "unsupported format: '%c%c%c'", str[0], str[1], str[2]);
        msg->format_id = FORMAT_ID_UNKNOWN;
    }
    return (char *)end;
}
