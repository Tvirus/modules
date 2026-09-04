#include "n32_flash.h"
#include <string.h>
#include "n32h49x_flash.h"




int flash_erase(unsigned int addr, unsigned int len)
{
    if (0 == len)
        return 0;
    if (addr % FLASH_PAGE_SIZE)
        return -1;

    FLASH_Unlock();

    while (1)
    {
        if (FLASH_EOP != FLASH_EraseOnePage(addr))
        {
            FLASH_Lock();
            return -1;
        }
        if (FLASH_PAGE_SIZE < len)
        {
            addr += FLASH_PAGE_SIZE;
            len -= FLASH_PAGE_SIZE;
        }
        else
        {
            break;
        }
    }

    FLASH_Lock();
    return 0;
}

int flash_write(unsigned int addr, const void *data, unsigned int len)
{
    uint32_t word[2];

    if (0 == len)
        return 0;
    if ((addr % 8) || (NULL == data))
        return -1;

    FLASH_Unlock();

    while (8 <= len)
    {
        memcpy(&word, data, 8);
        if (FLASH_EOP != FLASH_ProgramdoubleWord(addr, word[0], word[1]))
        {
            FLASH_Lock();
            return -1;
        }
        addr += 8;
        data = ((uint8_t *)data) + 8;
        len -= 8;
    }
    if (0 == len)
    {
        FLASH_Lock();
        return 0;
    }

    memset(&word, 0xff, 8);
    memcpy(&word, data, len);
    if (FLASH_EOP != FLASH_ProgramdoubleWord(addr, word[0], word[1]))
    {
        FLASH_Lock();
        return -1;
    }

    FLASH_Lock();
    return 0;
}

int flash_program(unsigned int addr, const void *data, unsigned int len)
{
    if (flash_erase(addr, len))
        return -1;
    if (flash_write(addr, data, len))
        return -1;
    return 0;
}

void flash_switch_bank(void)
{
    if (OBT->USER2_USER & 0x80000)
    {
        FLASH_Unlock();
        FLASH_EraseOB();
        FLASH_ProgramOB_RRDC(FLASH_OB_RDP1_DISABLE, FLASH_OB_RDP2_DISABLE, FLASH_DUAL_BANK, CCMSRAM_RST_NERASE);
        FLASH_ProgramOB_U1U2(FLASH_OB_IWDG_SOFTWARE,
                             FLASH_OB_STOP_NORST,
                             FLASH_OB_STDBY_NORST,
                             FLASH_OB_IWDG_STOP_NOFRZ,
                             FLASH_OB_IWDG_STDBY_NOFRZ,
                             FLASH_OB_IWDG_SLEEP_NOFRZ,
                             FLASH_OB2_NBOOT0_SET,
                             FLASH_OB2_NBOOT1_SET,
                             FLASH_OB2_NSWBOOT0_SET,
                             FLASH_OB2_FLASHBOOT_CLR,
                             BOR_LEVEL_1_6V);
    }
    else
    {
        FLASH_Unlock();
        FLASH_EraseOB();
        FLASH_ProgramOB_RRDC(FLASH_OB_RDP1_DISABLE, FLASH_OB_RDP2_DISABLE, FLASH_DUAL_BANK, CCMSRAM_RST_NERASE);
        FLASH_ProgramOB_U1U2(FLASH_OB_IWDG_SOFTWARE,
                             FLASH_OB_STOP_NORST,
                             FLASH_OB_STDBY_NORST,
                             FLASH_OB_IWDG_STOP_NOFRZ,
                             FLASH_OB_IWDG_STDBY_NOFRZ,
                             FLASH_OB_IWDG_SLEEP_NOFRZ,
                             FLASH_OB2_NBOOT0_SET,
                             FLASH_OB2_NBOOT1_SET,
                             FLASH_OB2_NSWBOOT0_SET,
                             FLASH_OB2_FLASHBOOT_SET,
                             BOR_LEVEL_1_6V);
    }
    NVIC_SystemReset();
}
