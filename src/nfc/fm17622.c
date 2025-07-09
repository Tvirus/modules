#include "fm17622.h"
#include "log.h"
#include <string.h>
#include "stm32u5xx_hal.h"




/* 寄存器 */
#define CommandReg        0x01
#define CommIEnReg        0x02
#define DivIEnReg         0x03
#define CommIrqReg        0x04
#define DivIrqReg         0x05
#define ErrorReg          0x06
#define Status1Reg        0x07
#define Status2Reg        0x08
#define FIFODataReg       0x09
#define FIFOLevelReg      0x0A
#define WaterLevelReg     0x0B
#define ControlReg        0x0C
#define BitFramingReg     0x0D
#define CollReg           0x0E
#define EXReg             0x0F
#define ModeReg           0x11
#define TxModeReg         0x12
#define RxModeReg         0x13
#define TxControlReg      0x14
#define TxAutoReg         0x15
#define TxSelReg          0x16
#define RxSelReg          0x17
#define RxThresholdReg    0x18
#define DemodReg          0x19
#define TxReg             0x1C
#define RxReg             0x1D
#define TypeBReg          0x1E
#define SerialSpeedReg    0x1F
#define CRCResultMSBReg   0x21
#define CRCResultLSBReg   0x22
#define GsNOffReg         0x23
#define ModWidthReg       0x24
#define TxBitPhaseReg     0x25
#define RFCfgReg          0x26
#define GsNOnReg          0x27
#define CWGsPReg          0x28
#define ModGsPReg         0x29
#define TModeReg          0x2A
#define TPrescalerReg     0x2B
#define TReloadHiReg      0x2C
#define TReloadLoReg      0x2D
#define TCounterValHiReg  0x2E
#define TCounterValLoReg  0x2F
#define TestSel1Reg       0x31
#define TestSel2Reg       0x32
#define TestPinEnReg      0x33
#define TestPinValueReg   0x34
#define TestBusReg        0x35
#define TestCtrlReg       0x36
#define VersionReg        0x37
#define AnalogTestReg     0x38
#define TestSup1Reg       0x39
#define TestSup2Reg       0x3A
#define TestADCReg        0x3B


/* CommandReg  0x01 */
#define RcvOff           (0x01 << 5)
#define PowerDown        (0x01 << 4)
#define Command_MASK     0x0F
#define CMD_Idle         0x00
#define CMD_RandomID     0x02
#define CMD_CalcCRC      0x03
#define CMD_Transmit     0x04
#define CMD_NoCmdChange  0x07
#define CMD_Receive      0x08
#define CMD_Transceive   0x0C
#define CMD_M1Authent    0x0E
#define CMD_SoftReset    0x0F

/* CommlEnReg  0x02 */
#define IrqInv           (0x01 << 7)
#define TxlEn            (0x01 << 6)
#define RxlEn            (0x01 << 5)
#define IdleIEn          (0x01 << 4)
#define HiAlertIEn       (0x01 << 3)
#define LoAlertIEn       (0x01 << 2)
#define ErrIEn           (0x01 << 1)
#define TimerIEn         (0x01 << 0)

/* DivIEnReg  0x03 */
#define IRQPushPull      (0x01 << 7)
#define CRCIEn           (0x01 << 2)

/* ComIrqReg  0x04 */
/* 清标志位时Set1置0、要清的位置1 */
#define Set1             (0x01 << 7)
#define TxIRq            (0x01 << 6)
#define RxIRq            (0x01 << 5)
#define IdleIRq          (0x01 << 4)
#define HiAlertIRq       (0x01 << 3)
#define LoAlertIRq       (0x01 << 2)
#define ErrIRq           (0x01 << 1)
#define TimerIRq         (0x01 << 0)

/* DivIrqReg  0x05 */
#define Set2             (0x01 << 7)
#define CRCIRq           (0x01 << 2)

/* ErrorReg  0x06 */
#define WrErr            (0x01 << 7)
#define TempErr          (0x01 << 6)
#define BufferOvfl       (0x01 << 4)
#define CollErr          (0x01 << 3)
#define CRCErr           (0x01 << 2)
#define ParityErr        (0x01 << 1)
#define ProtocolErr      (0x01 << 0)

/* Status1Reg  0x07 */
#define CRCOk            (0x01 << 6)
#define CRCReady         (0x01 << 5)
#define IRq              (0x01 << 4)
#define TRunning         (0x01 << 3)
#define HiAlert          (0x01 << 1)
#define LoAlert          (0x01 << 0)

/* Status2Reg  0x08 */
#define TempSensClear         (0x01 << 7)
#define I2CForceHS            (0x01 << 6)
#define Crypto1On             (0x01 << 3)
#define ModemState_MASK       0x07
#define ModemState_Idle       0x00
#define ModemState_WaitStart  0x01
#define ModemState_TxWait     0x02
#define ModemState_Sending    0x03
#define ModemState_RxWait     0x04
#define ModemState_WaitData   0x05
#define ModemState_Recving    0x06

/* FIFOLevelReg  0x0A */
#define FlushBuffer      (0x01 << 7)
#define FIFOLevel_MASK   0x7F

/* WaterLevelReg  0x0B */
#define WaterLevel_MASK  0x3F

/* ControlReg  0x0C */
#define TStopNow         (0x01 << 7)
#define TStartNow        (0x01 << 6)
#define RxLastBits_MASK  0x07

/* BitFramingReg  0x0D */
#define StartSend        (0x01 << 7)
#define RxAlign_SHIFT    0x04
#define RxAlign_MASK     (0x07 << RxAlign_SHIFT)
#define TxLastBits_MASK  0x07

/* CollReg  0x0E */
#define ValuesAfterColl  (0x01 << 7)
#define CollPosNotValid  (0x01 << 5)
#define CollPos_MASK     0x1F

/* EXReg  0x0F */
#define EXmode_MASK      0xC0
#define EXmode_WrAddr    0x40
#define EXmode_RdAddr    0x80
#define EXmode_WrData    0xC0
#define EXmode_RdData    0x00
#define EXAddr_MASK      0x3F

/* ModeReg  0x11 */
#define MSBFirst         (0x01 << 7)
#define TxWaitRF         (0x01 << 5)
#define CRCPreset_MASK   0x03
#define CRCPreset_0000   0x00
#define CRCPreset_6363   0x01
#define CRCPreset_A671   0x02
#define CRCPreset_FFFF   0x03

/* TxModeReg  0x12 */
#define TxCRCEn          (0x01 << 7)
#define TxSpeed_MASK     0x70
#define TxSpeed_106K     0x00
#define TxSpeed_212K     0x10
#define TxSpeed_424K     0x20
#define InvMod           (0x01 << 3)
#define TxFraming_MASK   0x03
#define TxFraming_14443A 0x00
#define TxFraming_14443B 0x03

/* RxModeReg  0x13 */
#define RxCRCEn          (0x01 << 7)
#define RxSpeed_MASK     0x70
#define RxSpeed_106K     0x00
#define RxSpeed_212K     0x10
#define RxSpeed_424K     0x20
#define RxNoErr          (0x01 << 3)
#define RxMultiple       (0x01 << 2)
#define RxFraming_MASK   0x03
#define RxFraming_14443A 0x00
#define RxFraming_14443B 0x03

/* TxControlReg  0x14 */
#define InvTx2RFOn       (0x01 << 7)
#define InvTx1RFOn       (0x01 << 6)
#define InvTx2RFOff      (0x01 << 5)
#define InvTx1RFOff      (0x01 << 4)
#define Tx2CW            (0x01 << 3)
#define Tx2RFEn          (0x01 << 1)
#define Tx1RFEn          (0x01 << 0)

/* TxAutoReg  0x15 */
#define Force100ASK      (0x01 << 6)

/* TxSelReg  0x16 */
#define DriverSel_MASK   0x30
#define DriverSel_3State 0x00
#define DriverSel_Encode 0x10
#define DriverSel_High   0x30
#define TOutSel_MASK     0x0F
#define TOutSel_3State   0x00
#define TOutSel_Low      0x01
#define TOutSel_High     0x02
#define TOutSel_TestBus  0x03
#define TOutSel_Encode   0x04
#define TOutSel_SendData 0x05
#define TOutSel_RecvedData 0x07

/* RxSelReg  0x17 */
#define UartSel_MASK     0xC0
#define UartSel_Low      0x00
#define UartSel_IntSignal 0x80
#define RxWait_MASK      0x3F

/* RxThresholdReg  0x18 */
#define MinLevel_SHIFT   0x04
#define MinLevel_MASK    (0x0F << MinLevel_SHIFT)
#define CollLevel_SHIFT  0x00
#define CollLevel_MASK   0x07

/* DemodReg  0x19 */
#define AddIQ_MASK       0xC0
#define AddIQ_Strong     0x00
#define AddIQ_StrongKeep 0x40
#define AddIQ_IQ         0x80
#define AddIQ_FixIQ_KeepI 0x00
#define AddIQ_FixIQ_KeepQ 0x40
#define FixIQ            (0x01 << 5)
#define TypeBEOFMode     (0x01 << 4)
#define TauRcv_SHIFT     0x02
#define TauRcv_MASK      (0x03 << TauRcv_SHIFT)
#define TauSync_MASK     0x03

/* TxReg  0x1C */
#define TxWait_MASK      0x03

/* RxReg  0x1D */
#define ParityDisable    (0x01 << 4)

/* TypeBReg  0x1E */
#define RxSOFReq         (0x01 << 7)
#define RxEOFReq         (0x01 << 6)
#define EOFSOFWidth      (0x01 << 4)
#define NoTxSOF          (0x01 << 3)
#define NoTxEOF          (0x01 << 2)
#define TxEGT_MASK       0x03
#define TxEGT_0bit       0x00
#define TxEGT_2bit       0x01
#define TxEGT_4bit       0x02
#define TxEGT_6bit       0x03

/* SerialSpeedReg  0x1F */
#define BR_T0_SHIFT      0x05
#define BR_T0_MASK       (0x07 << BR_T0_SHIFT)
#define BR_T1_MASK       0x0F

/* GsNOffReg  0x23 */
#define CWGsNOff_SHIFT   0x04
#define CWGsNOff_MASK    (0x0F << CWGsNOff_SHIFT)
#define ModGsNOff_MASK   0x0F

/* RFCfgReg  0x26 */
#define RxGain_MASK      0x70
#define RxGain_18db      0x00
#define RxGain_23db      0x10
#define RxGain_18db_     0x20
#define RxGain_23db_     0x30
#define RxGain_33db      0x40
#define RxGain_38db      0x50
#define RxGain_43db      0x60
#define RxGain_48db      0x70

/* GsNOnReg  0x27 */
#define CWGsNOn_SHIFT    0x04
#define CWGsNOn_MASK     (0x0F << CWGsNOn_SHIFT)
#define ModGsNOn_MASK    0x0F

/* CWGsPReg  0x28 */
#define CWGsP_MASK       0x3F

/* ModGsPReg  0x29 */
#define ModGsP_MASK      0x3F

/* TModeReg  0x2A */
#define TAuto            (0x01 << 7)
#define TGated_MASK      0x60
#define TGated_Non       0x00
#define TAutoRestart     (0x01 << 4)
#define TPrescalerHi_MASK 0x0F


/* 扩展寄存器 */
#define LPCDTest        0x21
#define LPCDGmSel       0x24
#define LPCDSarADC1     0x25
#define LPCDDelta_H     0x26
#define LPCDDelta_L     0x27
#define LPCDICurr_H     0x28
#define LPCDICurr_L     0x29
#define LPCDQCurr_H     0x2A
#define LPCDQCurr_L     0x2B
#define LPCDILast_H     0x2C
#define LPCDILast_L     0x2D
#define LPCDQLast_H     0x2E
#define LPCDQLast_L     0x2F
#define LPCDAUX         0x30
#define LPCDMissWup     0x31
#define LPCDFlagInv     0x32
#define LPCDSleepTimer  0x33
#define LPCDThresh_H    0x34
#define LPCDThresh_L    0x35
#define LPCDReqaTimer   0x37
#define LPCDReqaAna     0x38
#define LPCDDetectMode  0x39
#define LPCDCtrlMode    0x3A
#define LPCDIRQ         0x3B
#define LPCDRFTimer     0x3C
#define LPCDTxCtrl1     0x3D
#define LPCDTxCtrl2     0x3E
#define LPCDTxCtrl3     0x3F


/* LPCDTest  0x21 */
#define LPCD_Detect_Once  (0x01 << 5)

/* LPCDGmSel  0x24 */
#define LPCD_Enb_Agc      (0x01 << 4)
#define LPCD_Judge_Mode   (0x01 << 2)
#define LPCD_Gm_Sel_MASK  0x03
#define LPCD_Gm_Sel_0     0x00
#define LPCD_Gm_Sel_1     0x01
#define LPCD_Gm_Sel_2     0x02
#define LPCD_Gm_Sel_3     0x03

/* LPCDSarADC1  0x25 */
#define SarADC_Soc_Cfg_RFT   0x38
#define SarADC_Soc_Cfg_MASK  0x03
#define SarADC_Soc_Cfg_4     0x00
#define SarADC_Soc_Cfg_8     0x01
#define SarADC_Soc_Cfg_16    0x02
#define SarADC_Soc_Cfg_24    0x03

/* LPCDAUX  0x30 */
#define Aux_Ctrl_MASK        0x1F
#define Aux_Ctrl_AVSS        0x0B
#define Aux_Ctrl_INTER_1V5   0x0D
#define Aux_Ctrl_INTER_1V5_  0x0E
#define Aux_Ctrl_AVDD        0x0F
#define Aux_Ctrl_HighZ       0x10

/* LPCDFlagInv  0x32 */
#define LPCD_Flag_Inv      (0x01 << 5)
#define LPCD_Flag_Inv_RFT  0x10

/* LPCDSleepTimer  0x33 */
#define SLEEP_MS_2_REG(ms)  ((0==ms)?0:(ms-1)/32)

/* LPCDThresh_H  0x34 */
#define LPCD_Thresh_H_RFT   0x24
#define LPCD_Thresh_H_MASK  0x03

/* LPCDReqaTimer  0x37 */
#define IRqMissWupEn            (0x01 << 5)
#define LPCD_Reqa_Timer_MASK    0x1F
#define LPCD_Reqa_Timer_80us    0x00
#define LPCD_Reqa_Timer_100us   0x01
#define LPCD_Reqa_Timer_120us   0x02
#define LPCD_Reqa_Timer_150us   0x03
#define LPCD_Reqa_Timer_200us   0x04
#define LPCD_Reqa_Timer_250us   0x05
#define LPCD_Reqa_Timer_300us   0x06
#define LPCD_Reqa_Timer_400us   0x07
#define LPCD_Reqa_Timer_500us   0x08
#define LPCD_Reqa_Timer_600us   0x09
#define LPCD_Reqa_Timer_800us   0x0A
#define LPCD_Reqa_Timer_1000us  0x0B
#define LPCD_Reqa_Timer_1200us  0x0C
#define LPCD_Reqa_Timer_1600us  0x0D
#define LPCD_Reqa_Timer_2000us  0x0E
#define LPCD_Reqa_Timer_2500us  0x0F
#define LPCD_Reqa_Timer_3000us  0x10
#define LPCD_Reqa_Timer_3500us  0x11
#define LPCD_Reqa_Timer_4000us  0x12
#define LPCD_Reqa_Timer_5000us  0x13
#define LPCD_Reqa_Timer_7000us  0x14

/* LPCDReqaAna  0x38 */
#define LPCD_RxGain_MASK    0x30
#define LPCD_RxGain_23db    0x00
#define LPCD_RxGain_33db    0x10
#define LPCD_RxGain_38db    0x20
#define LPCD_RxGain_43db    0x30
#define LPCD_MinLevel_MASK  0x0C
#define LPCD_MinLevel_3     0x00
#define LPCD_MinLevel_6     0x04
#define LPCD_MinLevel_9     0x08
#define LPCD_MinLevel_12    0x0C
#define LPCD_ModWidth_32    0x00
#define LPCD_ModWidth_38    0x02

/* LPCDDetectMode  0x39 */
#define LPCD_TxPwr_Scale_MASK  0x38
#define LPCD_TxPwr_Scale_1_8   0x00
#define LPCD_TxPwr_Scale_1_4   0x08
#define LPCD_TxPwr_Scale_1_2   0x10
#define LPCD_TxPwr_Scale_3_4   0x18
#define LPCD_TxPwr_Scale_1     0x20
#define LPCD_TxPwr_Scale_2     0x28
#define LPCD_TxPwr_Scale_3     0x30
#define LPCD_TxPwr_Scale_4     0x38
#define LPCD_Detect_Mode_MASK  0x07
#define LPCD_Detect_Mode_AMP   0x00
#define LPCD_Detect_Mode_CMD   0x01
#define LPCD_Detect_Mode_BOTH  0x02

/* LPCDCtrlMode  0x3A */
#define RF_Det_Eanble           (0x01 << 5)
#define RF_Det_Sen_MASK         0x18
#define RF_Det_Sen_135mv        0x00
#define RF_Det_Sen_180mv        0x08
#define RF_Det_Sen_240mv        0x10
#define RF_Det_Sen_300mv        0x18
#define LPCD_Ctrl_Mode_MASK     0x07
#define LPCD_Ctrl_Mode_DISABLE  0x00
#define LPCD_Ctrl_Mode_ENABLE   0x02

/* LPCDIRQ  0x3B */
#define IRq_Set      (0x01 << 5)
#define IRqMissWup   (0x01 << 4)
#define IRqRFDet     (0x01 << 3)
#define IRqRxChange  (0x01 << 2)
#define IRqAtqaRec   (0x01 << 1)

/* LPCDRFTimer  0x3C */
#define LPCD_IRqInv          (0x01 << 5)
#define LPCD_IRqPushPull     (0x01 << 4)
#define LPCD_RF_Timer_MASK   0x0F
#define LPCD_RF_Timer_5us    0x00
#define LPCD_RF_Timer_10us   0x01
#define LPCD_RF_Timer_15us   0x02
#define LPCD_RF_Timer_20us   0x03
#define LPCD_RF_Timer_25us   0x04
#define LPCD_RF_Timer_30us   0x05
#define LPCD_RF_Timer_35us   0x06
#define LPCD_RF_Timer_40us   0x07
#define LPCD_RF_Timer_50us   0x08
#define LPCD_RF_Timer_60us   0x09
#define LPCD_RF_Timer_70us   0x0A
#define LPCD_RF_Timer_80us   0x0B
#define LPCD_RF_Timer_100us  0x0C
#define LPCD_RF_Timer_120us  0x0D
#define LPCD_RF_Timer_150us  0x0E
#define LPCD_RF_Timer_200us  0x0F

/* LPCDTxCtrl1  0x3D */
#define LPCD_Tx2RF_EN     (0x01 << 5)
#define LPCD_Tx1RF_EN     (0x01 << 4)
#define LPCD_InvTx2RFOn   (0x01 << 3)
#define LPCD_InvTx1RFOn   (0x01 << 2)
#define LPCD_InvTx2RFOff  (0x01 << 1)
#define LPCD_InvTx1RFOff  (0x01 << 0)

/* LPCDTxCtrl3  0x3F */
#define LPCD_Flag_Tout     (0x01 << 5)
#define LPCD_Flag_D3       (0x01 << 4)
#define LPCD_CWGsNOn_MASK  0x0F




#define LOGLEVEL_ERROR 1
#define LOGLEVEL_INFO  2
#define LOGLEVEL_DEBUG 3
#define LOG(level, fmt, arg...)  do{if((level)<=pcd_log_level)log_printf("--PCD-- " fmt "\n", ##arg);}while(0)

#define FM17622_SPI
#define FM17622_I2C_ADDR  0x50
#define FM17622_NPD_PORT  GPIOB
#define FM17622_NPD_PIN   GPIO_PIN_5
#define NPD_RESET()  HAL_GPIO_WritePin(FM17622_NPD_PORT,FM17622_NPD_PIN,GPIO_PIN_RESET)
#define NPD_SET()    HAL_GPIO_WritePin(FM17622_NPD_PORT,FM17622_NPD_PIN,GPIO_PIN_SET)


unsigned char pcd_log_level = LOGLEVEL_INFO;


#ifdef FM17622_I2C
extern I2C_HandleTypeDef hi2c1;
static int fm17622_read_reg(unsigned char addr, unsigned char *value)
{
    if (HAL_I2C_Mem_Read(&hi2c1, FM17622_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, value, 1, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_write_reg(unsigned char addr, unsigned char value)
{
    if (HAL_I2C_Mem_Write(&hi2c1, FM17622_I2C_ADDR, addr, I2C_MEMADD_SIZE_8BIT, &value, 1, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_read_ex_reg(unsigned char addr, unsigned char *value)
{
    unsigned char v;

    v = EXmode_RdAddr | addr;
    if (HAL_I2C_Mem_Write(&hi2c1, FM17622_I2C_ADDR, EXReg, I2C_MEMADD_SIZE_8BIT, &v, 1, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_ex_reg failed, addr:%02x !", addr);
        return -1;
    }
    if (HAL_I2C_Mem_Read(&hi2c1, FM17622_I2C_ADDR, EXReg, I2C_MEMADD_SIZE_8BIT, value, 1, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_ex_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_write_ex_reg(unsigned char addr, unsigned char value)
{
    unsigned char buf[2];

    buf[0] = EXmode_WrAddr | addr;
    buf[1] = EXmode_WrData | value;
    if (HAL_I2C_Mem_Write(&hi2c1, FM17622_I2C_ADDR, EXReg, I2C_MEMADD_SIZE_8BIT, buf, 2, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_ex_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_read_fifo(unsigned char *buf, unsigned int len)
{
    if ((PCD_FIFO_SIZE < len) || (0 == len))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_fifo failed, len(%u) exceeds the max(%u) !", len, PCD_FIFO_SIZE);
        return -1;
    }

    if (HAL_I2C_Mem_Read(&hi2c1, FM17622_I2C_ADDR, FIFODataReg, I2C_MEMADD_SIZE_8BIT, buf, len, len + 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_fifo failed !");
        return -1;
    }
    return 0;
}
static int fm17622_write_fifo(unsigned char *data, unsigned int len)
{
    if ((PCD_FIFO_SIZE < len) || (0 == len))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_fifo failed, len(%u) exceeds the max(%u) !", len, PCD_FIFO_SIZE);
        return -1;
    }

    if (HAL_I2C_Mem_Write(&hi2c1, FM17622_I2C_ADDR, FIFODataReg, I2C_MEMADD_SIZE_8BIT, data, len, len + 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_fifo failed !");
        return -1;
    }
    return 0;
}
#else ifdef FM17622_SPI
extern SPI_HandleTypeDef hspi1;
int fm17622_read_reg(unsigned char addr, unsigned char *value)
{
    unsigned char tx[2];
    unsigned char rx[2];

    tx[0] = 0x80 | (addr << 1);
    tx[1] = 0;
    if (HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_reg failed, addr:%02x !", addr);
        return -1;
    }
    *value = rx[1];
    return 0;
}
static int fm17622_write_reg(unsigned char addr, unsigned char value)
{
    unsigned char tx[2];

    tx[0] = addr << 1;
    tx[1] = value;
    if (HAL_SPI_Transmit(&hspi1, tx, 2, 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_read_ex_reg(unsigned char addr, unsigned char *value)
{
    unsigned char tx[2];
    unsigned char rx[2];

    tx[0] = EXReg << 1;
    tx[1] = EXmode_RdAddr | addr;
    if (HAL_SPI_Transmit(&hspi1, tx, 2, 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_ex_reg failed, addr:%02x !", addr);
        return -1;
    }

    tx[0] = 0x80 | (EXReg << 1);
    tx[1] = 0;
    if (HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_read_ex_reg failed !");
        return -1;
    }
    *value = rx[1];
    return 0;
}
static int fm17622_write_ex_reg(unsigned char addr, unsigned char value)
{
    unsigned char tx[4];

    tx[0] = EXReg << 1;
    tx[1] = EXmode_WrAddr | addr;
    tx[2] = EXReg << 1;
    tx[3] = EXmode_WrData | value;
    if (HAL_SPI_Transmit(&hspi1, tx, 4, 2))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_ex_reg failed, addr:%02x !", addr);
        return -1;
    }
    return 0;
}
static int fm17622_read_fifo(unsigned char *buf, unsigned int len)
{
    unsigned char tx[1 + PCD_FIFO_SIZE] = {0};
    unsigned char rx[1 + PCD_FIFO_SIZE] = {0};

    memset(tx, 0x80 | (FIFODataReg << 1), len);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 1 + len, 4);
    memcpy(buf, rx + 1, len);
    return 0;
}
static int fm17622_write_fifo(unsigned char *data, unsigned int len)
{
    unsigned char tx[1 + PCD_FIFO_SIZE] = {0};

    tx[0] = FIFODataReg << 1;
    memcpy(tx + 1, data, len);
    if (HAL_SPI_Transmit(&hspi1, tx, 1 + len, 4))
    {
        LOG(LOGLEVEL_ERROR, "fm17622_write_fifo failed !");
        return -1;
    }
    return 0;
}
#endif

static int fm17622_set_bits(unsigned char addr, unsigned char mask, unsigned char value)
{
    unsigned char v;

    if (0xFF == mask)
        return fm17622_write_reg(addr, value);

    if (fm17622_read_reg(addr, &v))
        return -1;

    v &= ~mask;
    v |= value & mask;
    return fm17622_write_reg(addr, v);
}


int pcd_soft_reset(void)
{
    NPD_SET();
    HAL_Delay(2);
    if (fm17622_write_reg(CommandReg, RcvOff | CMD_SoftReset))
        return -1;
    HAL_Delay(2);
    return 0;
}
int pcd_hard_reset(void)
{
    NPD_SET();
    HAL_Delay(2);
    fm17622_write_ex_reg(LPCDCtrlMode, LPCD_Ctrl_Mode_DISABLE);
    NPD_RESET();
    HAL_Delay(1);
    NPD_SET();
    HAL_Delay(2);
    return 0;
}

int pcd_set_mode(int mode)
{
    if (PCD_MODE_POWERDOWN == mode)
    {
        NPD_SET();
        HAL_Delay(2);
        fm17622_write_ex_reg(LPCDCtrlMode, LPCD_Ctrl_Mode_DISABLE);
        NPD_RESET();
    }
    else if (PCD_MODE_LPCD == mode)
    {
        pcd_soft_reset();
        fm17622_write_ex_reg(LPCDGmSel, LPCD_Enb_Agc | LPCD_Judge_Mode);
        fm17622_write_ex_reg(LPCDSarADC1, SarADC_Soc_Cfg_RFT | SarADC_Soc_Cfg_16);
        fm17622_write_ex_reg(LPCDCtrlMode, LPCD_Ctrl_Mode_ENABLE);
        fm17622_write_ex_reg(LPCDSleepTimer, SLEEP_MS_2_REG(416));
        fm17622_write_ex_reg(LPCDRFTimer, LPCD_IRqInv | LPCD_IRqPushPull | LPCD_RF_Timer_5us);
        //fm17622_write_ex_reg(LPCDThresh_L, 0x10);
        fm17622_write_ex_reg(LPCDTxCtrl2, 4);
        fm17622_write_ex_reg(LPCDTxCtrl3, 4);
        fm17622_write_ex_reg(LPCDReqaTimer, LPCD_Reqa_Timer_5000us);
        fm17622_write_ex_reg(LPCDReqaAna, LPCD_RxGain_33db | LPCD_MinLevel_9 | LPCD_ModWidth_38);
        fm17622_write_ex_reg(LPCDDetectMode, LPCD_TxPwr_Scale_1_8 | LPCD_Detect_Mode_AMP);
        NPD_RESET();
    }
    else if (PCD_MODE_TYPEA == mode)
    {
        pcd_soft_reset();
        fm17622_set_bits(TxControlReg, Tx2RFEn | Tx1RFEn, Tx2RFEn | Tx1RFEn);
        fm17622_set_bits(TxAutoReg, Force100ASK, Force100ASK);
        fm17622_write_reg(TxModeReg, TxSpeed_106K | TxFraming_14443A);
        fm17622_write_reg(RxModeReg, RxSpeed_106K | RxFraming_14443A | RxNoErr);
        fm17622_write_reg(RxThresholdReg, (5 << MinLevel_SHIFT) | (5 << CollLevel_SHIFT));
        fm17622_set_bits(RFCfgReg, RxGain_MASK, RxGain_33db);
    }
    else
        return -1;

    return 0;
}

int pcd_enable_crc(void)
{
    if (   fm17622_set_bits(TxModeReg, TxCRCEn, TxCRCEn)
        || fm17622_set_bits(RxModeReg, RxCRCEn, RxCRCEn))
        return -1;
    return 0;
}
int pcd_disable_crc(void)
{
    if (   fm17622_set_bits(TxModeReg, TxCRCEn, 0)
        || fm17622_set_bits(RxModeReg, RxCRCEn, 0))
        return -1;
    return 0;
}

int pcd_set_timer(unsigned int ms)
{
    unsigned int reload;
    unsigned int prescaler;

    /* (ms*13560)/(2*0xFFF+1) <= 0xFFFF */
    if (39586 < ms)
        return -1;

    /* ms * 13560 < 0xFFFF */
    if (4 >= ms)
    {
        prescaler = 0;
        reload = ms * 13560;
    }
    else
    {
        prescaler = ((ms * 13560 - 1) / 0xFFFF + 1) / 2;
        reload = ms * 13560 / (prescaler * 2 + 1);
    }

    fm17622_set_bits(TModeReg, TPrescalerHi_MASK, prescaler >> 8);
    fm17622_write_reg(TPrescalerReg, prescaler & 0xFF);
    fm17622_write_reg(TReloadHiReg, reload >> 8);
    fm17622_write_reg(TReloadLoReg, reload & 0xFF);

    return 0;
}


/* typea模式下，coll返回冲突位置；typeb模式下返回是否有冲突(CRC错误) */
int pcd_send(unsigned char *send, unsigned int send_bits, unsigned char *recv, unsigned char recv_len, unsigned char recv_pos, unsigned char *coll)
{
    unsigned char irq;
    unsigned char fifo_level = 0;
    int rst = 0;
    int i;
    unsigned char err;
    unsigned char _coll;
    unsigned char rxmode;
    unsigned char ctrl;


    if ((NULL == send) || (0 == send_bits) || ((PCD_FIFO_SIZE * 8) < send_bits))
        return -1;

    if (coll)
        *coll = 0;
    if (recv_pos)
    {
        if (fm17622_read_reg(RxModeReg, &rxmode) || (RxSpeed_106K != (rxmode & RxSpeed_MASK)))
        {
            LOG(LOGLEVEL_ERROR, "recv_pos is %u but RxSpeed is not 106k !", recv_pos);
            return -1;
        }
    }

    /* 初始化 */
    fm17622_write_reg(CommandReg, RcvOff | CMD_Idle);
    fm17622_write_reg(FIFOLevelReg, FlushBuffer);
    //fm17622_write_reg(WaterLevelReg, 0x20 & WaterLevel_MASK);
    fm17622_set_bits(TModeReg, TAuto, TAuto);
    fm17622_write_reg(CommIrqReg, (unsigned char)~Set1);

    /* 发送 */
    fm17622_write_fifo(send, (send_bits + 7) / 8);
    if (recv && recv_len)
        fm17622_write_reg(CommandReg, CMD_Transceive);
    else
        fm17622_write_reg(CommandReg, CMD_Transmit);
    recv_pos = ((recv_pos & 0x07) << RxAlign_SHIFT) & RxAlign_MASK;
    send_bits = (send_bits & 0x07) & TxLastBits_MASK;
    fm17622_write_reg(BitFramingReg, StartSend | recv_pos | send_bits);

    /* 等待发送完成 */
    for (i = 0; i < 2000; i++)
    {
        fm17622_read_reg(CommIrqReg, &irq);
        if(irq & TxIRq)
            break;
    }
    if (2000 <= i)
    {
        LOG(LOGLEVEL_ERROR, "wait tx timeout !");
        rst = -1;
        goto EXIT;
    }

    if ((NULL == recv) || (0 == recv_len))
    {
        rst = 0;
        goto EXIT;
    }

    /* 等待接收完成 */
    for (i = 0; i < 4000; i++)
    {
        fm17622_read_reg(CommIrqReg, &irq);
        if(irq & RxIRq)
            break;
        if (irq & TimerIRq)
        {
            LOG(LOGLEVEL_DEBUG, "wait rx timeout irq !");
            rst = -1;
            goto EXIT;
        }
    }
    if (4000 <= i)
    {
        LOG(LOGLEVEL_ERROR, "wait rx timeout !");
        rst = -1;
        goto EXIT;
    }

    /* 检查错误 */
    fm17622_read_reg(ErrorReg, &err);
    fm17622_read_reg(RxModeReg, &rxmode);
    if (err & (WrErr | TempErr | BufferOvfl | ProtocolErr))
    {
        LOG(LOGLEVEL_ERROR, "rx error, reg:0x%02x !", err);
        rst = -1;
        goto EXIT;
    }
    if (RxFraming_14443A == (rxmode & RxFraming_MASK))
    {
        if (err & CRCErr)
        {
            LOG(LOGLEVEL_ERROR, "14443A rx CRC error !");
            rst = -1;
            goto EXIT;
        }
        if (   (RxSpeed_106K == (rxmode & RxSpeed_MASK))
            && (ParityErr == (err & (CollErr | ParityErr))))
        {
            LOG(LOGLEVEL_ERROR, "14443A 106k mode rx parity error(0x%02x) !", err);
            rst = -1;
            goto EXIT;
        }
    }
    else if (RxFraming_14443B == (rxmode & RxFraming_MASK))
    {
        if (err & CRCErr)
        {
            LOG(LOGLEVEL_INFO, "typeb rx CRC error !");
            if (NULL == coll)
            {
                rst = -1;
                goto EXIT;
            }
            *coll = 1;
        }
    }

    /* 读取数据 */
    fm17622_read_reg(FIFOLevelReg, &fifo_level);
    fm17622_read_reg(ControlReg, &ctrl);
    if (0 == fifo_level)
    {
        LOG(LOGLEVEL_ERROR, "no data received !");
        rst = -1;
        goto EXIT;
    }
    if (recv_len > fifo_level)
        recv_len = fifo_level;
    fm17622_read_fifo(recv, recv_len);
    rst = fifo_level * 8 - ((8 - (ctrl & RxLastBits_MASK)) & 0x07);

    /* 检查冲突 */
    if (RxFraming_14443A == (rxmode & RxFraming_MASK))
    {
        fm17622_read_reg(CollReg, &_coll);
        if (0 == (_coll & CollPosNotValid))
        {
            if (NULL == coll)
            {
                LOG(LOGLEVEL_ERROR, "typea rx collision but coll is NULL !");
                rst = -1;
                goto EXIT;
            }
            if (_coll & CollPos_MASK)
                *coll = _coll & CollPos_MASK;
            else
                *coll = 32;
        }
    }

EXIT:
    fm17622_set_bits(ControlReg, TStopNow, TStopNow);
    fm17622_write_reg(CommandReg, CMD_Idle);
    fm17622_set_bits(BitFramingReg, StartSend, 0);
    return rst;
}

int pcd_m1_auth(m1_auth_t *auth)
{
    unsigned char irq;
    unsigned char status2 = 0;
    int rst = 0;
    int i;


    if (NULL == auth)
        return -1;

    /* 初始化 */
    pcd_set_timer(1);
    fm17622_write_reg(CommandReg, RcvOff | CMD_Idle);
    fm17622_write_reg(FIFOLevelReg, FlushBuffer);
    fm17622_set_bits(TModeReg, TAuto, TAuto);
    fm17622_write_reg(CommIrqReg, (unsigned char)~Set1);
    fm17622_write_fifo((unsigned char *)auth, sizeof(m1_auth_t));
    fm17622_write_reg(CommandReg, CMD_M1Authent);

    /* 等待发送完成 */
    for (i = 0; i < 2000; i++)
    {
        fm17622_read_reg(CommIrqReg, &irq);
        if(irq & IdleIRq)
        {
            break;
        }
        else if(irq & TimerIRq)
        {
            LOG(LOGLEVEL_INFO, "M1 auth timeout irq !");
            rst = -1;
            goto EXIT;
        }
    }
    if (2000 <= i)
    {
        LOG(LOGLEVEL_ERROR, "M1 auth timeout !");
        rst = -1;
        goto EXIT;
    }

    fm17622_read_reg(Status2Reg, &status2);
    if (status2 & Crypto1On)
    {
        LOG(LOGLEVEL_DEBUG, "M1 auth OK");
        rst = 0;
    }
    else
    {
        LOG(LOGLEVEL_INFO, "M1 auth failed !");
        rst = -1;
        goto EXIT;
    }

EXIT:
    fm17622_set_bits(ControlReg, TStopNow, TStopNow);
    fm17622_write_reg(CommandReg, CMD_Idle);
    return rst;
}
int pcd_disable_m1_auth(void)
{
    return fm17622_set_bits(Status2Reg, Crypto1On, 0);
}
