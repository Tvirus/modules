#ifndef _NMEA_H_
#define _NMEA_H_


#include <stdint.h>

typedef enum
{
    TALKER_ID_UNKNOWN = 0,
    TALKER_ID_GNSS,
    TALKER_ID_GPS,
    TALKER_ID_BDS,
    TALKER_ID_GALILEO,
    TALKER_ID_GLONASS,

    TALKER_ID_MAX
} talker_id_t;

typedef enum
{
    FORMAT_ID_UNKNOWN = 0,
    FORMAT_ID_GGA,
    FORMAT_ID_RMC,

    FORMAT_ID_MAX
} format_id_t;


typedef enum
{
    GGA_QUALITY_NULL = 0,
    GGA_QUALITY_INVALID,
    GGA_QUALITY_VALID,
    GGA_QUALITY_DIFF,
    GGA_QUALITY_PPS,
    GGA_QUALITY_RTK_FIXED,
    GGA_QUALITY_RTK_FLOATING,
    GGA_QUALITY_ESTIMATED,
    GGA_QUALITY_MANUAL,
    GGA_QUALITY_SIMULATOR,

    GGA_QUALITY_MAX
} gga_quality_t;


typedef struct
{
    uint8_t time_valid;
    uint8_t latitude_valid;
    uint8_t longitude_valid;
    uint8_t dhop_valid;
    uint8_t altitude_valid;
    uint8_t geoidal_separation_valid;
    uint8_t age_of_diff_data_valid;
    uint8_t diff_station_id_valid;

    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;
    uint16_t utc_usecond;
    int64_t latitude;  /* 0.000000001 degree */
    int64_t longitude;  /* 0.000000001 degree */
    gga_quality_t quality;
    uint8_t satellites;
    uint32_t dhop;  /* Horizontal dilution of precision, 0.01m */
    int32_t altitude;  /* 0.001m */
    int32_t geoidal_separation;  /* 0.001m */
    uint16_t age_of_diff_data; 
    uint16_t diff_station_id;
} format_gga_t;


typedef enum
{
    RMC_STATUS_NULL = 0,
    RMC_STATUS_INVALID,       /* V */
    RMC_STATUS_AUTONOMOUS,    /* A */
    RMC_STATUS_DIFFERENTIAL,  /* D */

    RMC_STATUS_MAX
} rmc_status_t;

typedef enum
{
    RMC_MODE_NULL = 0,
    RMC_MODE_AUTONOMOUS,    /* A */
    RMC_MODE_DIFFERENTIAL,  /* D */
    RMC_MODE_ESTIMATED,     /* E */
    RMC_MODE_FLOAT_RTK,     /* F */
    RMC_MODE_MANUAL,        /* M */
    RMC_MODE_NO_FIX,        /* N */
    RMC_MODE_PRECISE,       /* P */
    RMC_MODE_RTK,           /* R */
    RMC_MODE_SIMULATOR,     /* S */

    RMC_MODE_MAX
} rmc_mode_t;

typedef enum
{
    RMC_NAVIGATIONAL_NULL = 0,
    RMC_NAVIGATIONAL_CAUTION,  /* C */
    RMC_NAVIGATIONAL_SAFE,     /* S */
    RMC_NAVIGATIONAL_UNSAFE,   /* U */
    RMC_NAVIGATIONAL_INVALID,  /* V */

    RMC_NAVIGATIONAL_MAX
} rmc_navigational_t;

typedef struct
{
    uint8_t time_valid;
    uint8_t latitude_valid;
    uint8_t longitude_valid;
    uint8_t speed_valid;
    uint8_t course_valid;
    uint8_t date_valid;
    uint8_t declination_valid;

    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;
    uint16_t utc_usecond;
    rmc_status_t status;
    int64_t latitude;  /* 0.000000001 degree */
    int64_t longitude;  /* 0.000000001 degree */
    uint32_t speed;  /* 0.001 knot */
    uint32_t course;  /* 0.01 degree */
    uint8_t day;
    uint8_t month;
    uint8_t year;
    int32_t declination;  /* 0.1 degree */
    rmc_mode_t mode;
    rmc_navigational_t navigational;
} format_rmc_t;


typedef struct
{
    talker_id_t talker_id;
    format_id_t format_id;
    union
    {
        format_gga_t gga;
        format_rmc_t rmc;
    };
} nmea_msg_t;

extern char* nmea_parse(const char *str, nmea_msg_t *msg);


#endif
