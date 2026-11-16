#include "norlog.h"
#include <stddef.h>
#include "log.h"




#define NLOG_LOGLEVEL_ERROR  1
#define NLOG_LOGLEVEL_INFO   2
#define NLOG_LOGLEVEL_DEBUG  3
#ifndef NLOG_LOGLEVEL
#define NLOG_LOGLEVEL  NLOG_LOGLEVEL_INFO
#endif
#define NLOG_ERROR(fmt, arg...)  do{if((NLOG_LOGLEVEL_ERROR) <= (NLOG_LOGLEVEL))log_printf("--NLOG-- " fmt "\n", ##arg);}while(0)
#define NLOG_INFO(fmt, arg...)   do{if((NLOG_LOGLEVEL_INFO)  <= (NLOG_LOGLEVEL))log_printf("--NLOG-- " fmt "\n", ##arg);}while(0)
#define NLOG_DEBUG(fmt, arg...)  do{if((NLOG_LOGLEVEL_DEBUG) <= (NLOG_LOGLEVEL))log_printf("--NLOG-- " fmt "\n", ##arg);}while(0)


#define NLOG_VALID  0x4C4C4C6A

typedef struct
{
    uint8_t log_id;  /* 不能重复 */
    uint8_t dev_id;
    uint32_t sector_size;
    uint32_t partition_addr;
    uint32_t partition_size;  /* partition_size = record_size + data_size */
    uint32_t record_size;
} nlog_cfg_t;

typedef struct
{
    uint8_t log_id;
    uint8_t dev_id;
    uint8_t inited;
    uint32_t sector_size;
    uint32_t record_addr;
    uint32_t record_size;
    uint32_t data_addr;
    uint32_t data_size;

    uint32_t cur_record;
    uint32_t cur_head;
    uint32_t cur_len;
    uint8_t need_reset_record;
} nlog_partition_t;

typedef struct
{
    uint32_t head;
    uint32_t len;
    uint32_t magic;
} nlog_record_t;
#define NLOG_RECORD_SIZE  12
typedef char nlog_record_size_check[(sizeof(nlog_record_t) == NLOG_RECORD_SIZE) ? 1: -1];


static const nlog_cfg_t nlog_cfg_list[] =
{
    /* log_id  dev_id  sector_size  partition_addr   partition_size  record_size */
    {       0,      0,    4 * 1024,              0, 8 * 1024 * 1024,   64 * 1024}
};
#define NLOG_PARTITION_COUNT (sizeof(nlog_cfg_list) / sizeof(nlog_cfg_list[0]))

static nlog_partition_t nlog_partition_list[NLOG_PARTITION_COUNT] = {0};


static int nlog_read_flash(unsigned int dev_id, uint32_t addr, void *buf, uint32_t len)
{
    return -1;
}

static int nlog_erase_sector(unsigned int dev_id, uint32_t addr)
{
    return -1;
}

static int nlog_write_flash(unsigned int dev_id, uint32_t addr, const void *buf, uint32_t len)
{
    return -1;
}


#ifndef NLOG_RECORD_BUF_SIZE
#define NLOG_RECORD_BUF_SIZE  300
#endif
#if (NLOG_RECORD_SIZE > NLOG_RECORD_BUF_SIZE) || (NLOG_RECORD_BUF_SIZE % NLOG_RECORD_SIZE)
#error
#endif
/* 1:需要修复 0:成功 -1:没有记录或记录损坏 -2:读取失败 */
static int nlog_load_record(nlog_partition_t *partition)
{
    uint8_t dev_id;
    uint32_t sector_size;
    uint32_t record_addr;
    uint32_t read_len;
    nlog_record_t record_buf[NLOG_RECORD_BUF_SIZE / sizeof(nlog_record_t)];
    unsigned int i;
    unsigned int count;
    unsigned int valid = 0;


    dev_id = partition->dev_id;
    sector_size = partition->sector_size;
    record_addr = partition->record_addr;

    while (1)
    {
        read_len = sector_size - (record_addr % sector_size);
        if (NLOG_RECORD_BUF_SIZE < read_len)
            read_len = NLOG_RECORD_BUF_SIZE;
        read_len = (read_len / sizeof(nlog_record_t)) * sizeof(nlog_record_t);
        if (nlog_read_flash(dev_id, record_addr, record_buf, read_len))
        {
            NLOG_ERROR("Log(%u) load record failed, read 0x%x failed", partition->log_id, record_addr);
            return -2;
        }

        count = read_len / sizeof(nlog_record_t);
        for (i = 0; i < count; i++)
        {
            if (   (0xffffffff == record_buf[i].head)
                && (0xffffffff == record_buf[i].len)
                && (0xffffffff == record_buf[i].magic))
            {
                if (valid)
                {
                    NLOG_INFO("Log(%u) load record OK, record:0x%x head:0x%x len:%u",
                              partition->log_id, partition->cur_record, partition->cur_head, partition->cur_len);
                    return 0;
                }
                break;
            }
            else if (NLOG_VALID != record_buf[i].magic)
            {
                if (0 == valid)
                    continue;
                if (i || (record_addr % sector_size))
                {
                    NLOG_ERROR("Log(%u) record(0x%x) is not FF after valid record", partition->log_id, record_addr + (sizeof(nlog_record_t) * i));
                    return 1;
                }
                else
                {
                    NLOG_INFO("Log(%u) load record OK, record:0x%x head:0x%x len:%u",
                              partition->log_id, partition->cur_record, partition->cur_head, partition->cur_len);
                    return 0;
                }
            }
            else
            {
                if (valid)
                {
                    NLOG_ERROR("Log(%u) record(0x%x) is valid after valid record", partition->log_id, record_addr + (sizeof(nlog_record_t) * i));
                    return 1;
                }
                if (   (record_buf[i].head % sector_size)
                    || (partition->data_addr > record_buf[i].head)
                    || ((partition->data_addr + partition->data_size) <= record_buf[i].head)
                    || (partition->data_size < record_buf[i].len))
                {
                    NLOG_ERROR("Log(%u) record invalid, addr:0x%x head:0x%x len:0x%x",
                               partition->log_id, record_addr + (sizeof(nlog_record_t) * i), record_buf[i].head, record_buf[i].len);
                    return -1;
                }
                partition->cur_record = record_addr + (sizeof(nlog_record_t) * i);
                partition->cur_head = record_buf[i].head;
                partition->cur_len = record_buf[i].len;
                valid = 1;
            }
        }
        if ((count > i) || (sizeof(nlog_record_t) > (sector_size - (record_addr % sector_size) - read_len)))
        {
            if (valid)
            {
                NLOG_INFO("Log(%u) load record OK, record:0x%x head:0x%x len:%u",
                          partition->log_id, partition->cur_record, partition->cur_head, partition->cur_len);
                return 0;
            }
            record_addr = ((record_addr / sector_size) * sector_size) + sector_size;
            if ((partition->record_addr + partition->record_size) <= record_addr)
            {
                NLOG_ERROR("Log(%u) load record failed, no valid record", partition->log_id);
                return -1;
            }
        }
        else
        {
            record_addr += read_len;
        }
    }
}

/* 保留日志头和长度信息 */
static int nlog_reset_record(nlog_partition_t *partition)
{
    uint32_t origin_sector;
    nlog_record_t record;

    record.head = partition->cur_head;
    record.len = partition->cur_len;
    record.magic = NLOG_VALID;

    origin_sector = (partition->cur_record - (partition->cur_record % partition->sector_size));
    if (partition->record_addr == origin_sector)
    {
        origin_sector = partition->record_addr + partition->sector_size;

        if (nlog_erase_sector(partition->dev_id, origin_sector))
        {
            NLOG_ERROR("Log(%u) reset record failed, erase backup sector failed", partition->log_id);
            return -1;
        }
        if (nlog_write_flash(partition->dev_id, origin_sector, &record, sizeof(record)))
        {
            NLOG_ERROR("Log(%u) reset record failed, write backup record failed", partition->log_id);
            return -1;
        }
        partition->cur_record = origin_sector;
    }

    if (nlog_erase_sector(partition->dev_id, partition->record_addr))
    {
        NLOG_ERROR("Log(%u) reset record failed, erase sector failed", partition->log_id);
        return -1;
    }
    if (nlog_write_flash(partition->dev_id, partition->record_addr, &record, sizeof(record)))
    {
        NLOG_ERROR("Log(%u) reset record failed, write record failed", partition->log_id);
        return -1;
    }
    nlog_erase_sector(partition->dev_id, origin_sector);
    partition->cur_record = partition->record_addr;

    NLOG_INFO("Log(%u) reset record OK, record:0x%x head:0x%x len:%u",
              partition->log_id, partition->cur_record, partition->cur_head, partition->cur_len);
    return 0;
}

static int nlog_save_record(nlog_partition_t *partition)
{
    uint32_t sector_size;
    uint32_t cur_addr;
    uint32_t next_addr;
    nlog_record_t record;

    sector_size = partition->sector_size;
    cur_addr = partition->cur_record;
    next_addr = cur_addr + sizeof(nlog_record_t);
    if (((sector_size / sizeof(nlog_record_t)) * sizeof(nlog_record_t)) <= (next_addr % sector_size))
        next_addr = ((next_addr / sector_size) * sector_size) + sector_size;
    if ((partition->record_addr + partition->record_size) <= next_addr)
        next_addr = partition->record_addr;

    if (0 == (next_addr % sector_size))
    {
        if (nlog_erase_sector(partition->dev_id, next_addr))
        {
            NLOG_ERROR("Log(%u) save record failed, erase sector(0x%x) failed", partition->log_id, next_addr);
            return -1;
        }
    }

    record.head = partition->cur_head;
    record.len = partition->cur_len;
    record.magic = NLOG_VALID;
    if (nlog_write_flash(partition->dev_id, next_addr, &record, sizeof(record)))
    {
        record.magic = 0;
        nlog_write_flash(partition->dev_id, next_addr + offsetof(nlog_record_t, magic), &record.magic, sizeof(record.magic));
        NLOG_ERROR("Log(%u) save record failed, write new record(0x%x) failed", partition->log_id, next_addr);
        return -1;
    }
    partition->cur_record = next_addr;

    record.magic = 0;
    if (nlog_write_flash(partition->dev_id, cur_addr + offsetof(nlog_record_t, magic), &record.magic, sizeof(record.magic)))
    {
        NLOG_ERROR("Log(%u) save record failed, clear old record(0x%x) failed", partition->log_id, cur_addr);
        return -1;
    }
    return 0;
}

int nlog_init(void)
{
    nlog_partition_t *partition;
    uint32_t sector_size;
    uint32_t partition_addr;
    uint32_t partition_size;
    uint32_t record_size;
    unsigned int i, j;
    int rst;
    int ret = 0;

    for (i = 0; i < NLOG_PARTITION_COUNT; i++)
    {
        sector_size    = nlog_cfg_list[i].sector_size;
        partition_addr = nlog_cfg_list[i].partition_addr;
        partition_size = nlog_cfg_list[i].partition_size;
        record_size    = nlog_cfg_list[i].record_size;
        if (   ((4 * 1024) > sector_size)            /* 扇区最小4K */
            || (sector_size & (sector_size - 1))     /* 扇区大小是2的n次方 */
            || (partition_addr % sector_size)        /* 分区地址按扇区对齐 */
            || ((sector_size * 4) > partition_size)  /* 分区最小4个扇区 */
            || (partition_size % sector_size)        /* 分区大小按扇区对齐 */
            || ((sector_size * 2) > record_size)     /* 记录区最小2个扇区 */
            || (record_size % sector_size)           /* 记录区大小按扇区对齐 */
            || (partition_size <= record_size)       /* 记录区小于分区 */
            || ((sector_size * 2) > (partition_size - record_size)))  /* 数据区最小2个扇区 */
        {
            nlog_partition_list[i].log_id = nlog_cfg_list[i].log_id;
            nlog_partition_list[i].inited = 0;
            NLOG_ERROR("Log(%u) config invalid", nlog_cfg_list[i].log_id);
            ret = -1;
            continue;
        }

        partition = &nlog_partition_list[i];
        partition->log_id      = nlog_cfg_list[i].log_id;
        partition->dev_id      = nlog_cfg_list[i].dev_id;
        partition->sector_size = sector_size;
        partition->record_addr = partition_addr;
        partition->record_size = record_size;
        partition->data_addr   = partition_addr + record_size;
        partition->data_size   = partition_size - record_size;

        for (j = 0; j < 3; j++)
        {
            rst = nlog_load_record(partition);
            if (0 == rst)
            {
                break;
            }
            else if (1 == rst)
            {
                if (nlog_reset_record(partition))
                    partition->need_reset_record = 1;
                break;
            }
            else if (-2 == rst)
            {
                continue;
            }
            else if (-1 == rst)
            {
                nlog_erase_sector(partition->dev_id, partition->data_addr);
                partition->cur_record = partition->record_addr;
                partition->cur_head = partition->data_addr;
                partition->cur_len = 0;
                if (nlog_reset_record(partition))
                    partition->need_reset_record = 1;
                break;
            }
        }
        if (3 <= j)
        {
            nlog_erase_sector(partition->dev_id, partition->data_addr);
            partition->cur_record = partition->record_addr;
            partition->cur_head = partition->data_addr;
            partition->cur_len = 0;
            if (nlog_reset_record(partition))
                partition->need_reset_record = 1;
        }
        partition->inited = 1;
    }

    return ret;
}

int nlog_write(unsigned int log_id, const unsigned char *data, unsigned int len)
{
    nlog_partition_t *partition;
    uint32_t sector_size;
    uint32_t data_size;
    uint32_t tail_addr;
    uint32_t sector_left;
    uint32_t write_len;
    uint32_t old_head;
    uint32_t old_len;
    int i;

    if ((NULL == data) && len)
        return -1;
    if (0 == len)
        return 0;

    for (i = 0; i < NLOG_PARTITION_COUNT; i++)
    {
        if (nlog_partition_list[i].log_id == log_id)
            break;
    }
    if ((NLOG_PARTITION_COUNT <= i) || (0 == nlog_partition_list[i].inited))
        return -1;
    partition = &nlog_partition_list[i];
    if (partition->need_reset_record)
    {
        if (nlog_reset_record(partition))
            return -1;
        else
            partition->need_reset_record = 0;
    }
    sector_size = partition->sector_size;
    data_size = partition->data_size;

    while (len)
    {
        old_head = partition->cur_head;
        old_len = partition->cur_len;
        if (data_size <= partition->cur_len)
        {
            partition->cur_len = data_size - sector_size;
            partition->cur_head += sector_size;
            if ((partition->data_addr + data_size) <= partition->cur_head)
                partition->cur_head = partition->data_addr;
        }

        tail_addr = partition->cur_head + partition->cur_len;
        if ((partition->data_addr + data_size) <= tail_addr)
            tail_addr -= data_size;
        sector_left = sector_size - (tail_addr % sector_size);
        write_len = (sector_left >= len)? len: sector_left;

        /* 如果在擦除前保存记录，当擦除失败时tail会落在非扇区起始位置，下次写入时不会再次擦除 */
        if (0 == (tail_addr % sector_size))
        {
            if (nlog_erase_sector(partition->dev_id, tail_addr))
            {
                partition->cur_head = old_head;
                partition->cur_len = old_len;
                return -1;
            }
        }
        partition->cur_len += write_len;
        if (nlog_save_record(partition))
        {
            if (nlog_reset_record(partition))
            {
                partition->cur_head = old_head;
                partition->cur_len = old_len;
                partition->need_reset_record = 1;
                return -1;
            }
        }

        if (nlog_write_flash(partition->dev_id, tail_addr, data, write_len))
            return -1;
        data += write_len;
        len -= write_len;
    }

    return 0;
}

uint32_t nlog_get_len(unsigned int log_id)
{
    int i;

    for (i = 0; i < NLOG_PARTITION_COUNT; i++)
    {
        if (nlog_partition_list[i].log_id == log_id)
            break;
    }
    if ((NLOG_PARTITION_COUNT <= i) || (0 == nlog_partition_list[i].inited))
        return 0;
    return nlog_partition_list[i].cur_len;
}

/* 读取日志时不要写入 */
int nlog_read(unsigned int log_id, uint32_t offset, unsigned char *buf, unsigned int len)
{
    nlog_partition_t *partition;
    uint32_t head;
    uint32_t read_len_1;
    uint32_t read_len_2;
    int i;

    if ((NULL == buf) && len)
        return -1;
    if (0 == len)
        return 0;

    for (i = 0; i < NLOG_PARTITION_COUNT; i++)
    {
        if (nlog_partition_list[i].log_id == log_id)
            break;
    }
    if ((NLOG_PARTITION_COUNT <= i) || (0 == nlog_partition_list[i].inited))
        return -1;
    partition = &nlog_partition_list[i];
    if (partition->cur_len <= offset)
        return 0;

    head = partition->cur_head + offset;
    if ((partition->data_addr + partition->data_size) <= head)
        head -= partition->data_size;
    read_len_1 = partition->data_addr + partition->data_size - head;
    if (len < read_len_1)
        read_len_1 = len;
    if ((partition->cur_len - offset) < read_len_1)
        read_len_1 = partition->cur_len - offset;
    if (nlog_read_flash(partition->dev_id, head, buf, read_len_1))
    {
        NLOG_ERROR("Log(%u) read failed, addr:0x%x len:%u", partition->log_id, head, read_len_1);
        return -1;
    }
    if ((len <= read_len_1) || ((partition->cur_len - offset) <= read_len_1))
        return read_len_1;

    len -= read_len_1;
    read_len_2 = partition->cur_len - offset - read_len_1;
    if (len < read_len_2)
        read_len_2 = len;
    if (nlog_read_flash(partition->dev_id, partition->data_addr, buf + read_len_1, read_len_2))
    {
        NLOG_ERROR("Log(%u) read failed, addr:0x%x len:%u", partition->log_id, partition->data_addr, read_len_2);
        return -1;
    }

    return read_len_1 + read_len_2;
}
