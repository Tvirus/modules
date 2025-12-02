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

    FORMAT_ID_MAX
} format_id_t;


typedef enum
{
    GGA_QUALITY_INVALID = 0,
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
    int64_t latitude;   /* 0.000000001 degree */
    int64_t longitude;  /* 0.000000001 degree */
    gga_quality_t quality;
    uint8_t satellites;
    uint32_t dhop;  /* Horizontal dilution of precision, 0.01m */
    int32_t altitude;  /* 0.001m */
    int32_t geoidal_separation;  /* 0.001m */
    uint16_t age_of_diff_data; 
    uint16_t diff_station_id;
} format_gga_t;

typedef struct
{
    talker_id_t talker_id;
    format_id_t format_id;
    union
    {
        format_gga_t gga;
    };
} nmea_msg_t;

extern char* nmea_parse(const char *str, nmea_msg_t *msg);


#endif
