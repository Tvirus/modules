#include "flash.h"
#include "stm32l4xx_hal.h"
#include <string.h>




int flash_erase(unsigned int addr, unsigned int len)
{
    unsigned int page_start = 0;
    unsigned int page_count = 0;
    unsigned int bank1_page_start = 0;
    unsigned int bank1_page_count = 0;
    unsigned int bank2_page_start = 0;
    unsigned int bank2_page_count = 0;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PageError;
    unsigned int flash_page_count = FLASH_SIZE / FLASH_PAGE_SIZE;
    unsigned int bank_page_count = FLASH_BANK_SIZE / FLASH_PAGE_SIZE;


    if (0 == len)
        return 0;
    if ((!IS_FLASH_MAIN_MEM_ADDRESS(addr)) || (addr % FLASH_PAGE_SIZE) || (FLASH_SIZE < len))
        return -1;

    page_start = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    page_count = (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    if (flash_page_count < page_start + page_count)
        return -1;

    if (page_start < bank_page_count)
    {
        bank1_page_start = page_start;
        if ((page_start + page_count) <= bank_page_count)
        {
            bank1_page_count = page_count;
            bank2_page_count = 0;
        }
        else
        {
            bank1_page_count = bank_page_count - page_start;
            bank2_page_start = 0;
            bank2_page_count = page_count - bank1_page_count;
        }
    }
    else
    {
        bank1_page_count = 0;
        bank2_page_start = page_start - bank_page_count;
        bank2_page_count = page_count;
    }

    HAL_FLASH_Unlock();
    if (bank1_page_count)
    {
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
        EraseInitStruct.Banks     = FLASH_BANK_1;
        EraseInitStruct.Page      = bank1_page_start;
        EraseInitStruct.NbPages   = bank1_page_count;
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        if (HAL_OK != (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError)))
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    if (bank2_page_count)
    {
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
        EraseInitStruct.Banks     = FLASH_BANK_2;
        EraseInitStruct.Page      = bank2_page_start;
        EraseInitStruct.NbPages   = bank2_page_count;
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        if (HAL_OK != (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError)))
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();

    return 0;
}

int flash_write(unsigned int addr, const unsigned char *data, unsigned int len)
{
    unsigned int _len;
    uint64_t d;
    unsigned char *dp = (unsigned char *)&d;
    int i;

    if (0 == len)
        return 0;
    if (   (addr % 8) || (NULL == data) || (FLASH_SIZE < len)
        || (   ((!IS_FLASH_MAIN_MEM_ADDRESS(addr)) || (!IS_FLASH_MAIN_MEM_ADDRESS(addr + len -1)))
            && ((!IS_FLASH_OTP_ADDRESS(addr)) || (!IS_FLASH_OTP_ADDRESS(addr + len -1)))))
        return -1;

    _len = len & (~((unsigned int)0x7));
    HAL_FLASH_Unlock();
    for (i = 0; i < _len; i += 8)
    {
        dp[0] = data[0];
        dp[1] = data[1];
        dp[2] = data[2];
        dp[3] = data[3];
        dp[4] = data[4];
        dp[5] = data[5];
        dp[6] = data[6];
        dp[7] = data[7];
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, d))
        {
            HAL_FLASH_Lock();
            return -1;
        }
        addr += 8;
        data += 8;
    }
    if (len <= i)
    {
        HAL_FLASH_Lock();
        return 0;
    }

    d = ~((uint64_t)0);
    _len = len & 0x7;
    for (i = 0; i < _len; i++)
    {
        dp[i] = data[i];
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, d))
    {
        HAL_FLASH_Lock();
        return -1;
    }
    HAL_FLASH_Lock();

    return 0;
}

int flash_read(unsigned int addr, unsigned char *buf, unsigned int len)
{
    if (0 == len)
        return 0;
    if (   (NULL == buf) || (FLASH_SIZE < len)
        || (   ((!IS_FLASH_MAIN_MEM_ADDRESS(addr)) || (!IS_FLASH_MAIN_MEM_ADDRESS(addr + len -1)))
            && ((!IS_FLASH_OTP_ADDRESS(addr)) || (!IS_FLASH_OTP_ADDRESS(addr + len -1)))))
        return -1;

    memcpy(buf, (unsigned char *)addr, len);
    return 0;
}

int flash_write_otp(const unsigned char *data, unsigned int len)
{
    return flash_write(OTP_ADDR, data, len);
}

int flash_read_otp(unsigned char *buf, unsigned int len)
{
    if (0 == len)
        return 0;
    if ((NULL == buf) || (OTP_SIZE < len))
        return -1;

    memcpy(buf, (unsigned char *)OTP_ADDR, len);
    return 0;
}

int flash_program(unsigned int addr, const unsigned char *data, unsigned int len)
{
    if (flash_erase(addr, len))
        return -1;
    if (flash_write(addr, data, len))
        return -1;

    return 0;
}
