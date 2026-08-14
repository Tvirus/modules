#include "bq27220.h"
#include "log.h"
#include "stm32u5xx_hal.h"




/* REG CMD */
#define REG_CMD_CONTROL                0x00
#define REG_CMD_ATRATE                 0x02  /* mA */
#define REG_CMD_ATRATETIMETOEMPTY      0x04  /* 分钟 */
#define REG_CMD_TEMPERATURE            0x06  /* 0.1°K */
#define REG_CMD_VOLTAGE                0x08  /* mV */
#define REG_CMD_BATTERYSTATUS          0x0A
#define REG_CMD_CURRENT                0x0C  /* mA */
#define REG_CMD_REMAININGCAPACITY      0x10  /* mAh */
#define REG_CMD_FULLCHARGECAPACITY     0x12  /* mAh */
#define REG_CMD_AVERAGECURRENT         0x14  /* mA */
#define REG_CMD_TIMETOEMPTY            0x16  /* 分钟 */
#define REG_CMD_TIMETOFULL             0x18  /* 分钟 */
#define REG_CMD_STANDBYCURRENT         0x1A  /* mA */
#define REG_CMD_STANDBYTIMETOEMPTY     0x1C  /* 分钟 */
#define REG_CMD_MAXLOADCURRENT         0x1E  /* mA */
#define REG_CMD_MAXLOADTIMETOEMPTY     0x20  /* min */
#define REG_CMD_RAWCOULOMBCOUNT        0x22  /* mAh */
#define REG_CMD_AVERAGEPOWER           0x24  /* mW */
#define REG_CMD_INTERNALTEMPERATURE    0x28  /* 0.1°K */
#define REG_CMD_CYCLECOUNT             0x2A  /* 数字 */
#define REG_CMD_RELATIVESTATEOFCHARGE  0x2C  /* % */
#define REG_CMD_STATEOFHEALTH          0x2E  /* %/数字 */
#define REG_CMD_CHARGEVOLTAGE          0x30  /* mV */
#define REG_CMD_CHARGECURRENT          0x32  /* mA */
#define REG_CMD_BTPDISCHARGESET        0x34  /* mAh */
#define REG_CMD_BTPCHARGESET           0x36  /* mAh */
#define REG_CMD_OPERATIONSTATUS        0x3A
#define REG_CMD_DESIGNCAPACITY         0x3C  /* mAh */
#define REG_CMD_MAP_ADDR               0x3E
#define REG_CMD_MACDATA                0x40
#define REG_CMD_MACDATASUM             0x60
#define REG_CMD_MACDATALEN             0x61
#define REG_CMD_ANALOGCOUNT            0x79
#define REG_CMD_RAWCURRENT             0x7A
#define REG_CMD_RAWVOLTAGE             0x7C
#define REG_CMD_RAWINTTEMP             0x7E

#define CTRL_DEVICE_NUMBER           0x0001  /* 报告器件类型(例如：0x0320) */
#define CTRL_FW_VERSION              0x0002  /* 报告固件版本块(器件、版本、内部版本等) */
#define CTRL_BOARD_OFFSET            0x0009  /* 调用电路板偏移校正 */
#define CTRL_CC_OFFSET               0x000A  /* 调用 CC 偏移校正 */
#define CTRL_CC_OFFSET_SAVE          0x000B  /* 保存偏移校准过程的结果 */
#define CTRL_OCV_CMD                 0x000C  /* 请求电量监测计进行 OCV 测量 */
#define CTRL_BAT_INSERT              0x000D  /* 当 Operation Config B [BIEnable] 位 = 0 时强制设置BatteryStatus()[BATTPRES] 位 */
#define CTRL_BAT_REMOVE              0x000E  /* 当 Operation Config B [BIEnable] 位 = 0 时强制清除BatteryStatus()[BATTPRES] 位 */
#define CTRL_SET_SNOOZE              0x0013  /* 强制将 CONTROL_STATUS()[SNOOZE] 位设置为 1 */
#define CTRL_CLEAR_SNOOZE            0x0014  /* 强制将 CONTROL_STATUS()[SNOOZE] 位设置为 0 */
#define CTRL_SET_PROFILE_1           0x0015  /* 选择 CEDV Profile 1 */
#define CTRL_SET_PROFILE_2           0x0016  /* 选择 CEDV Profile 2 */
#define CTRL_SET_PROFILE_3           0x0017  /* 选择 CEDV Profile 3 */
#define CTRL_SET_PROFILE_4           0x0018  /* 选择 CEDV Profile 4 */
#define CTRL_SET_PROFILE_5           0x0019  /* 选择 CEDV Profile 5 */
#define CTRL_SET_PROFILE_6           0x001A  /* 选择 CEDV Profile 6 */
#define CTRL_CAL_TOGGLE              0x002D  /* 切换 OperationStatus()[CALMD] */
#define CTRL_SEALED                  0x0030  /* 将电量监测计置于 SEALED 访问模式 */
#define CTRL_RESET                   0x0041  /* 复位器件 */
#define CTRL_EXIT_CAL                0x0080  /* 指示电量监测计退出 CALIBRATION 模式 */
#define CTRL_ENTER_CAL               0x0081  /* 指示电量监测计进入 CALIBRATION 模式 */
#define CTRL_ENTER_CFG_UPDATE        0x0090  /* 进入 CONFIG UPDATE 模式 */
#define CTRL_EXIT_CFG_UPDATE_REINIT  0x0091  /* 退出 CONFIG UPDATE 模式并重新初始化 */
#define CTRL_EXIT_CFG_UPDATE         0x0092  /* 退出 CONFIG UPDATE 模式，不重新初始化 */
#define CTRL_RETURN_TO_ROM           0x0F00  /* 器件置于 ROM 模式 */

#define UNSEAL_KEY1  0x0414
#define UNSEAL_KEY2  0x3672

/* REG_CMD_OPERATIONSTATUS  0x3A */
#define CFGUPDATE  0x0400
#define BTPINT     0x0080
#define SMTH       0x0040
#define INITCOMP   0x0020
#define VDQ        0x0010
#define EDV2       0x0008
#define SEC1       0x0004
#define SEC0       0x0002
#define CALMD      0x0001

/* REG RAM */
#define REG_RAM_FULL_CHARGE_CAPACITY  0x929D
#define REG_RAM_DESIGN_CAPACITY       0x929F
#define REG_RAM_EMF                   0x92A7
#define REG_RAM_C0                    0x92A9
#define REG_RAM_R0                    0x92AB
#define REG_RAM_T0                    0x92AD
#define REG_RAM_R1                    0x92AF
#define REG_RAM_TC                    0x92B1
#define REG_RAM_C1                    0x92B2




#define LOGLEVEL_ERROR  1
#define LOGLEVEL_INFO   2
#define LOGLEVEL_DEBUG  3
#define LOG_ERROR(fmt, arg...)  do{if((LOGLEVEL_ERROR) <= bq27220_log_level)log_printf("--BQ27220-- " fmt "\n", ##arg);}while(0)
#define LOG_INFO(fmt, arg...)   do{if((LOGLEVEL_INFO)  <= bq27220_log_level)log_printf("--BQ27220-- " fmt "\n", ##arg);}while(0)
#define LOG_DEBUG(fmt, arg...)  do{if((LOGLEVEL_DEBUG) <= bq27220_log_level)log_printf("--BQ27220-- " fmt "\n", ##arg);}while(0)
unsigned int bq27220_log_level = LOGLEVEL_INFO;


extern I2C_HandleTypeDef hi2c1;
#define BQ27220_I2C_HANDLE hi2c1
#define BQ27220_I2C_ADDR 0xAA
static int bq27220_read_reg(uint8_t addr, uint16_t *value)
{
    uint8_t v[2];

    if (HAL_I2C_Mem_Read(&BQ27220_I2C_HANDLE, BQ27220_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, v, 2, 2))
    {
        LOG_ERROR("read reg(%02x) failed !", addr);
        return -1;
    }
    *value =  (v[1] << 8) | v[0];
    return 0;
}
static int bq27220_read_byte(uint8_t addr, uint8_t *value)
{
    if (HAL_I2C_Mem_Read(&BQ27220_I2C_HANDLE, BQ27220_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, value, 1, 2))
    {
        LOG_ERROR("read byte(%02x) failed !", addr);
        return -1;
    }
    return 0;
}
static int bq27220_write_reg(uint8_t addr, uint16_t value)
{
    uint8_t v[2];

    v[0] = value & 0xff;
    v[1] = value >> 8;
    if (HAL_I2C_Mem_Write(&BQ27220_I2C_HANDLE, BQ27220_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, v, 2, 2))
    {
        LOG_ERROR("write reg(%02x) failed !", addr);
        return -1;
    }
    return 0;
}
static int bq27220_write_byte(uint8_t addr, uint8_t value)
{
    if (HAL_I2C_Mem_Write(&BQ27220_I2C_HANDLE, BQ27220_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, &value, 1, 2))
    {
        LOG_ERROR("write byte(%02x) failed !", addr);
        return -1;
    }
    return 0;
}


static int bq27220_unseal(void)
{
    uint16_t operation_status = SEC1 | SEC0;
    int i;

    bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
    if ((SEC1 | SEC0) != (operation_status & (SEC1 | SEC0)))
    {
        LOG_DEBUG("UNSEAL OK");
        return 0;
    }
    for (i = 0; i < 4; i++)
    {
        if (i)
        {
            bq27220_write_reg(REG_CMD_CONTROL, CTRL_RESET);
            HAL_Delay(1000);
        }
        bq27220_write_reg(REG_CMD_CONTROL, UNSEAL_KEY1);
        HAL_Delay(10);
        bq27220_write_reg(REG_CMD_CONTROL, UNSEAL_KEY2);
        HAL_Delay(10);

        bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
        if ((SEC1 | SEC0) != (operation_status & (SEC1 | SEC0)))
        {
            LOG_DEBUG("UNSEAL OK(%d)", i);
            return 0;
        }
    }
    LOG_ERROR("UNSEAL failed !");
    return -1;
}

static int bq27220_seal(void)
{
    uint16_t operation_status = 0;
    int i;

    bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
    if ((SEC1 | SEC0) == (operation_status & (SEC1 | SEC0)))
    {
        LOG_DEBUG("SEAL OK");
        return 0;
    }
    for (i = 0; i < 4; i++)
    {
        bq27220_write_reg(REG_CMD_CONTROL, CTRL_SEALED);
        HAL_Delay(10);

        bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
        if ((SEC1 | SEC0) == (operation_status & (SEC1 | SEC0)))
        {
            LOG_DEBUG("SEAL OK(%d)", i);
            return 0;
        }
    }
    LOG_ERROR("SEAL failed !");
    return -1;
}

static int bq27220_full_access(void)
{
    uint16_t operation_status = 0;
    int i;

    bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
    if (SEC0 == (operation_status & (SEC1 | SEC0)))
    {
        LOG_DEBUG("FULL ACCESS OK");
        return 0;
    }
    for (i = 0; i < 4; i++)
    {
        bq27220_write_reg(REG_CMD_CONTROL, 0xffff);
        HAL_Delay(10);
        bq27220_write_reg(REG_CMD_CONTROL, 0xffff);
        HAL_Delay(10);

        bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
        if (SEC0 == (operation_status & (SEC1 | SEC0)))
        {
            LOG_DEBUG("FULL ACCESS OK(%d)", i);
            return 0;
        }
    }
    LOG_ERROR("FULL ACCESS failed !");
    return -1;
}

static int bq27220_enter_config_update(void)
{
    uint16_t operation_status = 0;
    int i;

    bq27220_write_reg(REG_CMD_CONTROL, CTRL_ENTER_CFG_UPDATE);
    for (i = 0; i < 100; i++)
    {
        HAL_Delay(10);
        bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
        if (operation_status & CFGUPDATE)
        {
            LOG_DEBUG("enter CONFIG UPDATE OK(%d)", i);
            return 0;
        }
    }
    LOG_ERROR("enter CONFIG UPDATE failed !");
    return -1;
}

static int bq27220_exit_config_update(void)
{
    uint16_t operation_status = CFGUPDATE;
    int i;

    bq27220_write_reg(REG_CMD_CONTROL, CTRL_EXIT_CFG_UPDATE_REINIT);
    for (i = 0; i < 100; i++)
    {
        HAL_Delay(10);
        bq27220_read_reg(REG_CMD_OPERATIONSTATUS, &operation_status);
        if (!(operation_status & CFGUPDATE))
        {
            LOG_DEBUG("exit CONFIG UPDATE OK(%d)", i);
            return 0;
        }
    }
    LOG_ERROR("exit CONFIG UPDATE failed !");
    return -1;
}

int bq27220_modify_ram(const bq27220_reg_t *list, unsigned int count)
{
    uint8_t old_sum = 0;
    uint8_t data_len = 0;
    uint8_t old_msb = 0;
    uint8_t old_lsb = 0;
    uint8_t new_msb = 0;
    uint8_t new_lsb = 0;
    unsigned int i;

    if (bq27220_unseal())
        return -1;
    if (bq27220_full_access())
        return -1;
    if (bq27220_enter_config_update())
        return -1;

    for (i = 0; i < count; i++)
    {
        /* 读校验 */
        bq27220_write_reg(REG_CMD_MAP_ADDR, list[i].addr);
        HAL_Delay(10);
        bq27220_read_byte(REG_CMD_MACDATASUM, &old_sum);
        bq27220_read_byte(REG_CMD_MACDATALEN, &data_len);

        if (2 == list[i].size)
        {
            /* 读旧值 */
            bq27220_write_reg(REG_CMD_MAP_ADDR, list[i].addr);
            HAL_Delay(10);
            bq27220_read_byte(REG_CMD_MACDATA,     &old_msb);
            bq27220_read_byte(REG_CMD_MACDATA + 1, &old_lsb);

            /* 写新值 */
            bq27220_write_reg(REG_CMD_MAP_ADDR, list[i].addr);
            HAL_Delay(10);
            new_msb = (list[i].value >> 8) & 0xff;
            new_lsb = list[i].value & 0xff;
            bq27220_write_reg(REG_CMD_MACDATA, (new_lsb << 8) | new_msb);  /* 此处先发msb */
            HAL_Delay(10);

            /* 写校验 */
            bq27220_write_byte(REG_CMD_MACDATASUM, old_sum + old_msb + old_lsb - new_msb - new_lsb);
            HAL_Delay(10);
            bq27220_write_byte(REG_CMD_MACDATALEN, data_len);
            HAL_Delay(10);
        }
        else
        {
            /* 读旧值 */
            bq27220_write_reg(REG_CMD_MAP_ADDR, list[i].addr);
            HAL_Delay(10);
            bq27220_read_byte(REG_CMD_MACDATA, &old_msb);

            /* 写新值 */
            bq27220_write_reg(REG_CMD_MAP_ADDR, list[i].addr);
            HAL_Delay(10);
            bq27220_write_byte(REG_CMD_MACDATA, list[i].value & 0xff);
            HAL_Delay(10);

            /* 写校验 */
            bq27220_write_byte(REG_CMD_MACDATASUM, old_sum + old_msb - (list[i].value & 0xff));
            HAL_Delay(10);
            bq27220_write_byte(REG_CMD_MACDATALEN, data_len);
            HAL_Delay(10);
        }
    }

    bq27220_exit_config_update();
    bq27220_seal();

    return 0;
}

/**
bq27220_reg_t ram_list[] =
{
    {REG_RAM_FULL_CHARGE_CAPACITY, 2, 3000},
    {REG_RAM_DESIGN_CAPACITY,      2, 3000},
    {REG_RAM_EMF,                  2, 3743},
    {REG_RAM_C0,                   2, 149},
    {REG_RAM_R0,                   2, 867},
    {REG_RAM_T0,                   2, 4030},
    {REG_RAM_R1,                   2, 316},
    {REG_RAM_TC,                   1, 9},
    {REG_RAM_C1,                   1, 0}
};
#define RAM_LIST_COUNT  (sizeof(ram_list) / sizeof(ram_list[0]))

    bq27220_modify_ram(ram_list, RAM_LIST_COUNT);
    HAL_Delay(1000);
*/
