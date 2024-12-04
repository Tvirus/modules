#include "riff.h"
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <ctype.h>




#define CHUNK_ID_RIFF               "RIFF"
#define CHUNK_ID_LIST               "LIST"
#define CHUNK_ID_AVI                "AVI "
#define CHUNK_ID_CHARACTER_SET      "CSET"
#define CHUNK_ID_CTOC               "CTOC"  /* Compound File Table of Contents */
#define CHUNK_ID_CGRP               "CGRP"  /* Compound File Element Group */
#define CHUNK_ID_DISPLAY            "DISP"
#define CHUNK_ID_JUNK               "JUNK"
#define CHUNK_ID_PAD                "PAD "
#define CHUNK_ID_EXTERNAL_PALETTE   "XPLT"

#define FORM_TYPE_BUNDLE            "BND "
#define FORM_TYPE_DIB               "RDIB"  /* RIFF Device Independent Bitmap */
#define FORM_TYPE_MIDI              "RMID"  /* RIFF Musical Instrument Digital Interface */
#define FORM_TYPE_MULTIMEDIA_MOVIE  "RMMP"  /* RIFF Multimedia Movie */
#define FORM_TYPE_PALETTE           "PAL "  /* Palette */
#define FORM_TYPE_RICH_TEXT         "RTF "
#define FORM_TYPE_WAVE              "WAVE"  /* Waveform Audio */
//#define FORM_TYPE_AVI               "AVI "
#define FORM_TYPE_CPPO              "CPPO"
#define FORM_TYPE_ANIMATED_CURSOR   "ACON"  /* Windows NT Animated Cursor */

#define LIST_TYPE_INFO             "INFO"


/* ---------------------------------------------------------------------------------------- */
/*
    INFO LIST CHUNKS (from the Multimedia Programmer's Reference plus new ones)
*/
#define INFO_LIST_CHUNK_IARL  "IARL"  /* Archival location */
#define INFO_LIST_CHUNK_IART  "IART"  /* Artist */
#define INFO_LIST_CHUNK_ICMS  "ICMS"  /* Commissioned */
#define INFO_LIST_CHUNK_ICMT  "ICMT"  /* Comments */
#define INFO_LIST_CHUNK_ICOP  "ICOP"  /* Copyright */
#define INFO_LIST_CHUNK_ICRD  "ICRD"  /* Creation date of subject */
#define INFO_LIST_CHUNK_ICRP  "ICRP"  /* Cropped */
#define INFO_LIST_CHUNK_IDIM  "IDIM"  /* Dimensions */
#define INFO_LIST_CHUNK_IDPI  "IDPI"  /* Dots per inch */
#define INFO_LIST_CHUNK_IENG  "IENG"  /* Engineer */
#define INFO_LIST_CHUNK_IGNR  "IGNR"  /* Genre */
#define INFO_LIST_CHUNK_IKEY  "IKEY"  /* Keywords */
#define INFO_LIST_CHUNK_ILGT  "ILGT"  /* Lightness settings */
#define INFO_LIST_CHUNK_IMED  "IMED"  /* Medium */
#define INFO_LIST_CHUNK_INAM  "INAM"  /* Name of subject */
#define INFO_LIST_CHUNK_IPLT  "IPLT"  /* Palette Settings. No. of colors requested. */
#define INFO_LIST_CHUNK_IPRD  "IPRD"  /* Product */
#define INFO_LIST_CHUNK_ISBJ  "ISBJ"  /* Subject description */
#define INFO_LIST_CHUNK_ISFT  "ISFT"  /* Software. Name of package used to create file. */
#define INFO_LIST_CHUNK_ISHP  "ISHP"  /* Sharpness. */
#define INFO_LIST_CHUNK_ISRC  "ISRC"  /* Source. */
#define INFO_LIST_CHUNK_ISRF  "ISRF"  /* Source Form. ie slide, paper */
#define INFO_LIST_CHUNK_ITCH  "ITCH"  /* Technician who digitized the subject. */
#define INFO_LIST_CHUNK_ISMP  "ISMP"  /* SMPTE time code */
/*
    ISMP: SMPTE time code of digitization start point expressed as a NULL terminated
    text string "HH:MM:SS:FF". If performing MCI capture in AVICAP, this
    chunk will be automatically set based on the MCI start time.
*/
#define INFO_LIST_CHUNK_IDIT  "IDIT"  /* Digitization Time */
/*
    IDIT: "Digitization Time" Specifies the time and date that the digitization commenced.
    The digitization time is contained in an ASCII string which
    contains exactly 26 characters and is in the format
    "Wed Jan 02 02:03:55 1990\n\0".
    The ctime(), asctime(), functions can be used to create strings
    in this format. This chunk is automatically added to the capture
    file based on the current system time at the moment capture is
    initiated.
*/
#define INFO_LIST_CHUNK_ITRK  "ITRK"  /* ASCIIZ representation of the 1-based track number of the content. */
#define INFO_LIST_CHUNK_ITOC  "ITOC"  /* A dump of the table of contents from the CD the content originated from. */

/* ---------------------------------------------------------------------------------------- */
/*
    RDIB
*/
#define RDIB_CHUNK_ID_BITMAP_HEADER  "bmhd"

/* ---------------------------------------------------------------------------------------- */
/*
    WAVE
*/
#define WAVE_CHUNK_ID_FMT          "fmt "
#define WAVE_CHUNK_ID_DATA         "data"
#define WAVE_CHUNK_ID_CUE_POINTS   "cue "
#define WAVE_CHUNK_ID_FACK         "fact"
#define WAVE_CHUNK_ID_INSTRUMENT   "inst"
#define WAVE_CHUNK_ID_PLAYLIST     "plst"
#define WAVE_CHUNK_ID_SAMPLE       "smpl"

#define WAVE_LIST_ASSOCIATED_DATA  "adtl"
#define WAVE_LIST_WAVL             "wavl"

/* ---------------------------------------------------------------------------------------- */


#define DEBUG(fmt, arg...)  printf("--RIFF-- %s: " fmt "\n", __func__, ##arg)
#define ERROR(fmt, arg...)  printf("\e[1;31m--RIFF-- %s: " fmt "\e[0m\n", __func__, ##arg)

#pragma pack (1)

typedef struct
{
    char id[4];       /* "RIFF" */
    uint32_t size;    /* 不包括id和size */
    char type[4];     /* form type 或者 list type */
    uint8_t data[0];  /* 长度2字节对齐，不足补0，size不包括补的0 */
} chunk_t;

typedef struct
{
    char id[4];
    uint32_t size;
    uint8_t  data[0];
} sub_chunk_t;

#pragma pack ()

/* ---------------------------------------------------------------------------------------- */
/*
    WAVE
*/
#pragma pack (1)

typedef struct
{
    uint16_t format_tag;         /* Format category */
    uint16_t channels;           /* Number of channels */
    uint32_t samples_per_sec;    /* Sampling rate */
    uint32_t avg_bytes_per_sec;  /* For buffer estimation */
    uint16_t block_align;        /* Data block size */
} wave_fmt_common_t;

typedef struct
{
    uint16_t bits_per_sample;    /* Sample size */
} wave_fmt_pcm_t;

#pragma pack ()


static int parse_wave_fmt(const uint8_t *data, uint32_t size, riff_wave_info_t *info)
{
    wave_fmt_common_t *fmt_common = (wave_fmt_common_t *)data;
    uint16_t format_tag;
    wave_fmt_pcm_t *fmt_pcm;


    if (sizeof(wave_fmt_common_t) > size)
        return -1;

    format_tag = le16toh(fmt_common->format_tag);
    info->format            = format_tag;
    info->channels          = le16toh(fmt_common->channels);
    info->samples_per_sec   = le32toh(fmt_common->samples_per_sec);
    info->avg_bytes_per_sec = le32toh(fmt_common->avg_bytes_per_sec);
    info->block_align       = le16toh(fmt_common->block_align);
    if (format_tag == WAVE_FORMAT_PCM)
    {
        if (sizeof(wave_fmt_pcm_t) > (size - sizeof(wave_fmt_common_t)))
            return -1;
        fmt_pcm = (wave_fmt_pcm_t *)(data + sizeof(wave_fmt_common_t));
        info->pcm_info.bits_per_sample = le16toh(fmt_pcm->bits_per_sample);
    }
    else
    {
        DEBUG("Unknown WAVE format: 0x%04x", format_tag);
        return -1;
    }

    return 0;
}
static int get_wave_info(const uint8_t *data, uint32_t size, riff_wave_info_t *info)
{
    sub_chunk_t *chunk;
    uint32_t chunk_size;


    for (;;)
    {
        if (sizeof(sub_chunk_t) > size)
            break;

        chunk = (sub_chunk_t *)data;
        chunk_size = le32toh(chunk->size);
        if (chunk_size > (size - sizeof(sub_chunk_t)))
            break;

        if (!memcmp(chunk->id, WAVE_CHUNK_ID_FMT, 4))
        {
            if (parse_wave_fmt(chunk->data, chunk_size, info))
                return -1;
        }
        else if (!memcmp(chunk->id, WAVE_CHUNK_ID_DATA, 4))
        {
            info->data = chunk->data;
            info->data_size = chunk_size;
        }
        else
        {
            DEBUG("Unknown WAVE chunk: %02x %02x %02x %02x \"%c%c%c%c\"",
                   chunk->id[0], chunk->id[1], chunk->id[2], chunk->id[3],
                   isprint(chunk->id[0])?chunk->id[0]:' ',
                   isprint(chunk->id[1])?chunk->id[1]:' ',
                   isprint(chunk->id[2])?chunk->id[2]:' ',
                   isprint(chunk->id[3])?chunk->id[3]:' ');
        }

        chunk_size = ((chunk_size + 1) >> 1) << 1;
        if (0 == chunk_size)
            break;
        data += sizeof(sub_chunk_t) + chunk_size;
        size -= sizeof(sub_chunk_t) + chunk_size;
    }

    if (WAVE_FORMAT_UNKNOWN == info->format || NULL == info->data)
        return -1;
    return 0;
}

/* ---------------------------------------------------------------------------------------- */


int riff_get_file_info(const uint8_t *data, size_t size, riff_info_t *info)
{
    chunk_t *chunk = (chunk_t *)data;
    uint32_t chunk_size;


    if (NULL == chunk || sizeof(chunk_t) > size || NULL == info)
        return -1;
    if (memcmp(chunk->id, CHUNK_ID_RIFF, 4))
    {
        ERROR("Invalid RIFF format !");
        return -1;
    }
    chunk_size = le32toh(chunk->size);
    if ((4 >= chunk_size) || (chunk_size > (size - 8)))
    {
        ERROR("chunksize(%u) + 8 > filesize(%zu) !", chunk_size, size);
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->size = chunk_size;

    if (!memcmp(chunk->type, FORM_TYPE_WAVE, 4))
    {
        info->type = RIFF_TYPE_WAVE;
        return get_wave_info(chunk->data, chunk_size - 4, &info->wave_info);
    }
    else
    {
        DEBUG("Unknown RIFF type: %02x %02x %02x %02x \"%c%c%c%c\"",
               chunk->type[0], chunk->type[1], chunk->type[2], chunk->type[3],
               isprint(chunk->type[0])?chunk->type[0]:' ',
               isprint(chunk->type[1])?chunk->type[1]:' ',
               isprint(chunk->type[2])?chunk->type[2]:' ',
               isprint(chunk->type[3])?chunk->type[3]:' ');
        info->type = RIFF_TYPE_UNKNOWN;
        return 0;
    }

    return 0;
}
