/*----------------------------------------------------------------------------*
 *  Copyright (c) 2012        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Primitive trigger control for VME CPUs using the TJNAF Trigger
 *     Supervisor (TI) card
 *
 *----------------------------------------------------------------------------*/
#ifndef TILIB_H
#define TILIB_H

#ifndef VXWORKS
#include <pthread.h>

pthread_mutex_t tiISR_mutex=PTHREAD_MUTEX_INITIALIZER;
#else
/* #include <intLib.h> */
extern int intLock();
extern int intUnlock();
#endif

#ifdef VXWORKS
int intLockKeya;
#define INTLOCK {				\
    intLockKeya = intLock();			\
}

#define INTUNLOCK {				\
    intUnlock(intLockKeya);			\
}
#else
#define INTLOCK {				\
    vmeBusLock();				\
}
#define INTUNLOCK {				\
    vmeBusUnlock();				\
}
#endif

struct TIPCIE_RegStruct
{
  /** 0x00000 */ volatile unsigned int boardID;
  /** 0x00004 */ volatile unsigned int fiber;
  /** 0x00008 */ volatile unsigned int intsetup;
  /** 0x0000C */ volatile unsigned int trigDelay;
  /** 0x00010 */ volatile unsigned int adr32;
  /** 0x00014 */ volatile unsigned int blocklevel;
  /** 0x00018 */ volatile unsigned int dataFormat;
  /** 0x0001C */ volatile unsigned int vmeControl;
  /** 0x00020 */ volatile unsigned int trigsrc;
  /** 0x00024 */ volatile unsigned int sync;
  /** 0x00028 */ volatile unsigned int busy;
  /** 0x0002C */ volatile unsigned int clock;
  /** 0x00030 */ volatile unsigned int trig1Prescale;
  /** 0x00034 */ volatile unsigned int blockBuffer;
  /** 0x00038 */ volatile unsigned int triggerRule;
  /** 0x0003C */ volatile unsigned int triggerWindow;
  /** 0x00040 */          unsigned int blank0;
  /** 0x00044 */ volatile unsigned int tsInput;
  /** 0x00048 */          unsigned int blank1;
  /** 0x0004C */ volatile unsigned int output;
  /** 0x00050 */ volatile unsigned int fiberSyncDelay;
  /** 0x00054 */          unsigned int blank2[(0x64-0x54)/4];
  /** 0x00064 */          unsigned int inputPrescale;
  /** 0x00068 */          unsigned int blank3[(0x74-0x68)/4];
  /** 0x00074 */ volatile unsigned int pulserEvType;
  /** 0x00078 */ volatile unsigned int syncCommand;
  /** 0x0007C */ volatile unsigned int syncDelay;
  /** 0x00080 */ volatile unsigned int syncWidth;
  /** 0x00084 */ volatile unsigned int triggerCommand;
  /** 0x00088 */ volatile unsigned int randomPulser;
  /** 0x0008C */ volatile unsigned int fixedPulser1;
  /** 0x00090 */ volatile unsigned int fixedPulser2;
  /** 0x00094 */ volatile unsigned int nblocks;
  /** 0x00098 */ volatile unsigned int syncHistory;
  /** 0x0009C */ volatile unsigned int runningMode;
  /** 0x000A0 */ volatile unsigned int fiberLatencyMeasurement;
  /** 0x000A4 */ volatile unsigned int fiberAlignment;
  /** 0x000A8 */ volatile unsigned int livetime;
  /** 0x000AC */ volatile unsigned int busytime;
  /** 0x000B0 */ volatile unsigned int GTPStatusA;
  /** 0x000B4 */ volatile unsigned int GTPStatusB;
  /** 0x000B8 */ volatile unsigned int GTPtriggerBufferLength;
  /** 0x000BC */ volatile unsigned int inputCounter;
  /** 0x000C0 */ volatile unsigned int blockStatus[4];
  /** 0x000D0 */ volatile unsigned int adr24;
  /** 0x000D4 */ volatile unsigned int syncEventCtrl;
  /** 0x000D8 */ volatile unsigned int eventNumber_hi;
  /** 0x000DC */ volatile unsigned int eventNumber_lo;
  /** 0x000E0 */          unsigned int blank4[(0xEC-0xE0)/4];
  /** 0x000EC */ volatile unsigned int rocEnable;
  /** 0x000F0 */          unsigned int blank5[(0xFC-0xF0)/4];
  /** 0x000FC */ volatile unsigned int blocklimit;
  /** 0x00100 */ volatile unsigned int reset;
  /** 0x00104 */ volatile unsigned int fpDelay[2];
  /** 0x0010C */          unsigned int blank6[(0x110-0x10C)/4];
  /** 0x00110 */          unsigned int busy_scaler1[7];
  /** 0x0012C */          unsigned int blank7[(0x138-0x12C)/4];
  /** 0x00138 */ volatile unsigned int triggerRuleMin;
  /** 0x0013C */          unsigned int blank8;
  /** 0x00140 */ volatile unsigned int trigTable[(0x180-0x140)/4];
  /** 0x00180 */ volatile unsigned int ts_scaler[6];
  /** 0x00198 */          unsigned int blank9;
  /** 0x0019C */ volatile unsigned int busy_scaler2[9];
  /** 0x001C0 */          unsigned int blank10[(0x1D0-0x1C0)/4];
  /** 0x001D0 */ volatile unsigned int hfbr_tiID[8];
  /** 0x001F0 */ volatile unsigned int master_tiID;
  /** 0x001F4 */          unsigned int blank11[(0x2000-0x1F4)/4];
  /** 0x02000 */ volatile unsigned int SWB_status[(0x2200-0x2000)/4];
  /** 0x02200 */          unsigned int blank12[(0x2800-0x2200)/4];
  /** 0x02800 */ volatile unsigned int SWA_status[(0x3000-0x2800)/4];
  /** 0x03000 */          unsigned int blank13[(0xFFFC-0x3000)/4];
  /** 0x0FFFC */ volatile unsigned int eJTAGLoad;
  /** 0x10000 */ volatile unsigned int JTAGPROMBase[(0x20000-0x10000)/4];
  /** 0x20000 */ volatile unsigned int JTAGFPGABase[(0x30000-0x20000)/4];
  /** 0x30000 */ volatile unsigned int SWA[(0x40000-0x30000)/4];
  /** 0x40000 */ volatile unsigned int SWB[(0x50000-0x40000)/4];
};

/* Define TI Modes of operation:     Ext trigger - Interrupt mode   0
                                     TS  trigger - Interrupt mode   1
                                     Ext trigger - polling  mode    2 
                                     TS  trigger - polling  mode    3  */
#define TI_READOUT_EXT_INT    0
#define TI_READOUT_TS_INT     1
#define TI_READOUT_EXT_POLL   2
#define TI_READOUT_TS_POLL    3

/* Supported firmware version */
#define TI_SUPPORTED_FIRMWARE 0x023
#define TI_SUPPORTED_TYPE     3

/* Firmware Masks */
#define TI_FIRMWARE_ID_MASK              0xFFFF0000
#define TI_FIRMWARE_TYPE_MASK            0x0000F000
#define TI_FIRMWARE_TYPE_REV2            0
#define TI_FIRMWARE_TYPE_PROD            1
#define TI_FIRMWARE_TYPE_MODTI           2
#define TI_FIRMWARE_TYPE_PROD2           3
#define TI_FIRMWARE_MAJOR_VERSION_MASK   0x00000FF0
#define TI_FIRWMARE_MINOR_VERSION_MASK   0x0000000F

/* 0x0 boardID bits and masks */
#define TI_BOARDID_TYPE_TIDS         0x71D5
#define TI_BOARDID_TYPE_TI           0x7100
#define TI_BOARDID_TYPE_TS           0x7500
#define TI_BOARDID_TYPE_TD           0x7D00
#define TI_BOARDID_TYPE_MASK     0xFF000000
#define TI_BOARDID_PROD_MASK     0x00FF0000
#define TI_BOARDID_GEOADR_MASK   0x00001F00
#define TI_BOARDID_CRATEID_MASK  0x000000FF

/* 0x4 fiber bits and masks */
#define TI_FIBER_1                        (1<<0)
#define TI_FIBER_2                        (1<<1)
#define TI_FIBER_3                        (1<<2)
#define TI_FIBER_4                        (1<<3)
#define TI_FIBER_5                        (1<<4)
#define TI_FIBER_6                        (1<<5)
#define TI_FIBER_7                        (1<<6)
#define TI_FIBER_8                        (1<<7)
#define TI_FIBER_ENABLE_P0                (1<<8)
#define TI_FIBER_ENABLED(x)           (1<<(x+1))
#define TI_FIBER_MASK                 0x000000FF
#define TI_FIBER_CONNECTED_1             (1<<16)
#define TI_FIBER_CONNECTED_2             (1<<17)
#define TI_FIBER_CONNECTED_3             (1<<18)
#define TI_FIBER_CONNECTED_4             (1<<19)
#define TI_FIBER_CONNECTED_5             (1<<20)
#define TI_FIBER_CONNECTED_6             (1<<21)
#define TI_FIBER_CONNECTED_7             (1<<22)
#define TI_FIBER_CONNECTED_8             (1<<23)
#define TI_FIBER_CONNECTED_TI(x)     (1<<(x+15))
#define TI_FIBER_CONNECTED_MASK       0x00FF0000
#define TI_FIBER_TRIGSRC_ENABLED_1       (1<<24)
#define TI_FIBER_TRIGSRC_ENABLED_2       (1<<25)
#define TI_FIBER_TRIGSRC_ENABLED_3       (1<<26)
#define TI_FIBER_TRIGSRC_ENABLED_4       (1<<27)
#define TI_FIBER_TRIGSRC_ENABLED_5       (1<<28)
#define TI_FIBER_TRIGSRC_ENABLED_6       (1<<29)
#define TI_FIBER_TRIGSRC_ENABLED_7       (1<<30)
#define TI_FIBER_TRIGSRC_ENABLED_8       (1<<31)
#define TI_FIBER_TRIGSRC_ENABLED_TI(x) (1<<(x+23))
#define TI_FIBER_TRIGSRC_ENABLED_MASK 0xFF000000

/* 0x8 intsetup bits and masks */
#define TI_INTSETUP_VECTOR_MASK   0x000000FF
#define TI_INTSETUP_LEVEL_MASK    0x00000F00
#define TI_INTSETUP_ENABLE        (1<<16)

/* 0xC trigDelay bits and masks */
#define TI_TRIGDELAY_TRIG1_DELAY_MASK 0x000000FF
#define TI_TRIGDELAY_TRIG1_WIDTH_MASK 0x0000FF00
#define TI_TRIGDELAY_TRIG2_DELAY_MASK 0x00FF0000
#define TI_TRIGDELAY_TRIG2_WIDTH_MASK 0xFF000000
#define TI_TRIGDELAY_TRIG1_64NS_STEP  (1<<7)
#define TI_TRIGDELAY_TRIG2_64NS_STEP  (1<<23)

/* 0x10 adr32 bits and masks */
#define TI_ADR32_MBLK_ADDR_MAX_MASK  0x000003FE
#define TI_ADR32_MBLK_ADDR_MIN_MASK  0x003FC000
#define TI_ADR32_BASE_MASK       0xFF800000

/* 0x14 blocklevel bits and masks */
#define TI_BLOCKLEVEL_MASK           0x000000FF
#define TI_BLOCKLEVEL_CURRENT_MASK   0x00FF0000
#define TI_BLOCKLEVEL_RECEIVED_MASK  0xFF000000


/* 0x18 dataFormat bits and masks */
#define TI_DATAFORMAT_TWOBLOCK_PLACEHOLDER (1<<0)
#define TI_DATAFORMAT_TIMING_WORD          (1<<1)
#define TI_DATAFORMAT_HIGHERBITS_WORD      (1<<2)

/* 0x1C vmeControl bits and masks */
#define TI_VMECONTROL_BERR           (1<<0)
#define TI_VMECONTROL_TOKEN_TESTMODE (1<<1)
#define TI_VMECONTROL_MBLK           (1<<2)
#define TI_VMECONTROL_A32M           (1<<3)
#define TI_VMECONTROL_A32            (1<<4)
#define TI_VMECONTROL_ERROR_INT      (1<<7)
#define TI_VMECONTROL_I2CDEV_HACK    (1<<8)
#define TI_VMECONTROL_TOKENOUT_HI    (1<<9)
#define TI_VMECONTROL_FIRST_BOARD    (1<<10)
#define TI_VMECONTROL_LAST_BOARD     (1<<11)
#define TI_VMECONTROL_BUFFER_DISABLE (1<<15)
#define TI_VMECONTROL_BLOCKLEVEL_UPDATE (1<<21)

/* 0x20 trigsrc bits and masks */
#define TI_TRIGSRC_SOURCEMASK       0x0000F3FF
#define TI_TRIGSRC_P0               (1<<0)
#define TI_TRIGSRC_HFBR1            (1<<1)
#define TI_TRIGSRC_LOOPBACK         (1<<2)
#define TI_TRIGSRC_FPTRG            (1<<3)
#define TI_TRIGSRC_VME              (1<<4)
#define TI_TRIGSRC_TSINPUTS         (1<<5)
#define TI_TRIGSRC_TSREV2           (1<<6)
#define TI_TRIGSRC_PULSER           (1<<7)
#define TI_TRIGSRC_HFBR5            (1<<10)
#define TI_TRIGSRC_TRIG21           (1<<11)
#define TI_TRIGSRC_PART_1           (1<<12)
#define TI_TRIGSRC_PART_2           (1<<13)
#define TI_TRIGSRC_PART_3           (1<<14)
#define TI_TRIGSRC_PART_4           (1<<15)
#define TI_TRIGSRC_MONITOR_MASK     0xFFFF0000

/* 0x24 sync bits and masks */
#define TI_SYNC_SOURCEMASK              0x000000FF
#define TI_SYNC_P0                      (1<<0)
#define TI_SYNC_HFBR1                   (1<<1)
#define TI_SYNC_HFBR5                   (1<<2)
#define TI_SYNC_FP                      (1<<3)
#define TI_SYNC_LOOPBACK                (1<<4)
#define TI_SYNC_USER_SYNCRESET_ENABLED  (1<<7)
#define TI_SYNC_HFBR1_CODE_MASK         0x00000F00
#define TI_SYNC_HFBR5_CODE_MASK         0x0000F000
#define TI_SYNC_LOOPBACK_CODE_MASK      0x000F0000
#define TI_SYNC_HISTORY_FIFO_MASK       0x00700000
#define TI_SYNC_HISTORY_FIFO_EMPTY      (1<<20)
#define TI_SYNC_HISTORY_FIFO_HALF_FULL  (1<<21)
#define TI_SYNC_HISTORY_FIFO_FULL       (1<<22)
#define TI_SYNC_MONITOR_MASK            0xFF000000

/* 0x28 busy bits and masks */
#define TI_BUSY_SOURCEMASK      0x0000FFFF
#define TI_BUSY_SWA              (1<<0)
#define TI_BUSY_SWB              (1<<1)
#define TI_BUSY_P2               (1<<2)
#define TI_BUSY_FP_FTDC          (1<<3)
#define TI_BUSY_FP_FADC          (1<<4)
#define TI_BUSY_FP               (1<<5)
#define TI_BUSY_TRIGGER_LOCK     (1<<6)
#define TI_BUSY_LOOPBACK         (1<<7)
#define TI_BUSY_HFBR1            (1<<8)
#define TI_BUSY_HFBR2            (1<<9)
#define TI_BUSY_HFBR3            (1<<10)
#define TI_BUSY_HFBR4            (1<<11)
#define TI_BUSY_HFBR5            (1<<12)
#define TI_BUSY_HFBR6            (1<<13)
#define TI_BUSY_HFBR7            (1<<14)
#define TI_BUSY_HFBR8            (1<<15)
#define TI_BUSY_MONITOR_MASK     0xFFFF0000
#define TI_BUSY_MONITOR_SWA      (1<<16)
#define TI_BUSY_MONITOR_SWB      (1<<17)
#define TI_BUSY_MONITOR_P2       (1<<18)
#define TI_BUSY_MONITOR_FP_FTDC  (1<<19)
#define TI_BUSY_MONITOR_FP_FADC  (1<<20)
#define TI_BUSY_MONITOR_FP       (1<<21)
#define TI_BUSY_MONITOR_TRIG_LOST (1<<22)
#define TI_BUSY_MONITOR_LOOPBACK (1<<23)
#define TI_BUSY_MONITOR_FIBER_BUSY(x) (1<<(x+23))
#define TI_BUSY_MONITOR_HFBR1    (1<<24)
#define TI_BUSY_MONITOR_HFBR2    (1<<25)
#define TI_BUSY_MONITOR_HFBR3    (1<<26)
#define TI_BUSY_MONITOR_HFBR4    (1<<27)
#define TI_BUSY_MONITOR_HFBR5    (1<<28)
#define TI_BUSY_MONITOR_HFBR6    (1<<29)
#define TI_BUSY_MONITOR_HFBR7    (1<<30)
#define TI_BUSY_MONITOR_HFBR8    (1<<31)

/* 0x2C clock bits and mask  */
#define TI_CLOCK_INTERNAL    (0)
#define TI_CLOCK_HFBR5       (1)
#define TI_CLOCK_HFBR1       (2)
#define TI_CLOCK_FP          (3)
#define TI_CLOCK_MASK        0x0000000F

/* 0x30 trig1Prescale bits and masks */
#define TI_TRIG1PRESCALE_MASK          0x0000FFFF

/* 0x34 blockBuffer bits and masks */
#define TI_BLOCKBUFFER_BUFFERLEVEL_MASK      0x000000FF
#define TI_BLOCKBUFFER_BLOCKS_READY_MASK     0x0000FF00
#define TI_BLOCKBUFFER_TRIGGERS_IN_BLOCK     0x00FF0000
#define TI_BLOCKBUFFER_RO_NEVENTS_MASK       0x07000000
#define TI_BLOCKBUFFER_BLOCKS_NEEDACK_MASK   0x7F000000
#define TI_BLOCKBUFFER_BREADY_INT_MASK       0x0F000000
#define TI_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT    (1<<28)
#define TI_BLOCKBUFFER_SYNCRESET_REQUESTED   (1<<30)
#define TI_BLOCKBUFFER_SYNCEVENT             (1<<31)

/* 0x38 triggerRule bits and masks */
#define TI_TRIGGERRULE_RULE1_MASK 0x000000FF
#define TI_TRIGGERRULE_RULE2_MASK 0x0000FF00
#define TI_TRIGGERRULE_RULE3_MASK 0x00FF0000
#define TI_TRIGGERRULE_RULE4_MASK 0xFF000000

/* 0x3C triggerWindow bits and masks */
#define TI_TRIGGERWINDOW_COINC_MASK   0x000000FF
#define TI_TRIGGERWINDOW_INHIBIT_MASK 0x0000FF00
#define TI_TRIGGERWINDOW_TRIG21_MASK  0x01FF0000

/* 0x48 tsInput bits and masks */
#define TI_TSINPUT_MASK      0x0000003F
#define TI_TSINPUT_1         (1<<0)
#define TI_TSINPUT_2         (1<<1)
#define TI_TSINPUT_3         (1<<2)
#define TI_TSINPUT_4         (1<<3)
#define TI_TSINPUT_5         (1<<4)
#define TI_TSINPUT_6         (1<<5)
#define TI_TSINPUT_ALL       (0x3F)


/* 0x4C output bits and masks */
#define TI_OUTPUT_MASK                 0x0000FFFF
#define TI_OUTPUT_BLOCKS_READY_MASK    0x00FF0000
#define TI_OUTPUT_EVENTS_IN_BLOCK_MASK 0xFF000000

/* 0x50 fiberSyncDelay bits and masks */
#define TI_FIBERSYNCDELAY_HFBR1_SYNCPHASE_MASK    0x000000FF
#define TI_FIBERSYNCDELAY_HFBR1_SYNCDELAY_MASK    0x0000FF00
#define TI_FIBERSYNCDELAY_LOOPBACK_SYNCDELAY_MASK 0x00FF0000
#define TI_FIBERSYNCDELAY_HFBR5_SYNCDELAY_MASK    0xFF000000

/* 0x74 inputPrescale bits and masks */
#define TI_INPUTPRESCALE_FP1_MASK   0x0000000F
#define TI_INPUTPRESCALE_FP2_MASK   0x000000F0
#define TI_INPUTPRESCALE_FP3_MASK   0x00000F00
#define TI_INPUTPRESCALE_FP4_MASK   0x0000F000
#define TI_INPUTPRESCALE_FP5_MASK   0x000F0000
#define TI_INPUTPRESCALE_FP6_MASK   0x00F00000
#define TI_INPUTPRESCALE_FP_MASK(x) (0xF<<4*((x-1)))

/* 0x78 syncCommand bits and masks */
#define TI_SYNCCOMMAND_VME_CLOCKRESET      0x11
#define TI_SYNCCOMMAND_CLK250_RESYNC       0x22
#define TI_SYNCCOMMAND_AD9510_RESYNC       0x33
#define TI_SYNCCOMMAND_GTP_STATUSB_RESET   0x44
#define TI_SYNCCOMMAND_TRIGGERLINK_ENABLE  0x55
#define TI_SYNCCOMMAND_TRIGGERLINK_DISABLE 0x77
#define TI_SYNCCOMMAND_SYNCRESET_HIGH      0x99
#define TI_SYNCCOMMAND_TRIGGER_READY_RESET 0xAA
#define TI_SYNCCOMMAND_RESET_EVNUM         0xBB
#define TI_SYNCCOMMAND_SYNCRESET_LOW       0xCC
#define TI_SYNCCOMMAND_SYNCRESET           0xDD
#define TI_SYNCCOMMAND_SYNCRESET_4US       0xEE
#define TI_SYNCCOMMAND_SYNCCODE_MASK       0x000000FF

/* 0x7C syncDelay bits and masks */
#define TI_SYNCDELAY_MASK              0x0000007F

/* 0x80 syncWidth bits and masks */
#define TI_SYNCWIDTH_MASK              0x7F
#define TI_SYNCWIDTH_LONGWIDTH_ENABLE  (1<<7)

/* 0x84 triggerCommand bits and masks */
#define TI_TRIGGERCOMMAND_VALUE_MASK     0x000000FF
#define TI_TRIGGERCOMMAND_CODE_MASK      0x00000F00
#define TI_TRIGGERCOMMAND_TRIG1          0x00000100
#define TI_TRIGGERCOMMAND_TRIG2          0x00000200
#define TI_TRIGGERCOMMAND_SYNC_EVENT     0x00000300
#define TI_TRIGGERCOMMAND_SET_BLOCKLEVEL 0x00000800

/* 0x88 randomPulser bits and masks */
#define TI_RANDOMPULSER_TRIG1_RATE_MASK 0x0000000F
#define TI_RANDOMPULSER_TRIG1_ENABLE    (1<<7)
#define TI_RANDOMPULSER_TRIG2_RATE_MASK 0x00000F00
#define TI_RANDOMPULSER_TRIG2_ENABLE    (1<<15)

/* 0x8C fixedPulser1 bits and masks */
#define TI_FIXEDPULSER1_NTRIGGERS_MASK 0x0000FFFF
#define TI_FIXEDPULSER1_PERIOD_MASK    0x7FFF0000
#define TI_FIXEDPULSER1_PERIOD_RANGE   (1<<31)

/* 0x90 fixedPulser2 bits and masks */
#define TI_FIXEDPULSER2_NTRIGGERS_MASK 0x0000FFFF
#define TI_FIXEDPULSER2_PERIOD_MASK    0x7FFF0000
#define TI_FIXEDPULSER2_PERIOD_RANGE   (1<<31)

/* 0x94 nblocks bits and masks */
#define TI_NBLOCKS_COUNT_MASK           0x00FFFFFF
#define TI_NBLOCKS_EVENTS_IN_BLOCK_MASK 0xFF000000

/* 0x98 syncHistory bits and masks */
#define TI_SYNCHISTORY_HFBR1_CODE_MASK     0x0000000F
#define TI_SYNCHISTORY_HFBR1_CODE_VALID    (1<<4)
#define TI_SYNCHISTORY_HFBR5_CODE_MASK     0x000001E0
#define TI_SYNCHISTORY_HFBR5_CODE_VALID    (1<<9)
#define TI_SYNCHISTORY_LOOPBACK_CODE_MASK  0x00003C00
#define TI_SYNCHISTORY_LOOPBACK_CODE_VALID (1<<14)
#define TI_SYNCHISTORY_TIMESTAMP_OVERFLOW  (1<<15)
#define TI_SYNCHISTORY_TIMESTAMP_MASK      0xFFFF0000

/* 0x9C runningMode settings */
#define TI_RUNNINGMODE_ENABLE          0x71
#define TI_RUNNINGMODE_DISABLE         0x0

/* 0xA0 fiberLatencyMeasurement bits and masks */
#define TI_FIBERLATENCYMEASUREMENT_CARRYCHAIN_MASK 0x0000FFFF
#define TI_FIBERLATENCYMEASUREMENT_IODELAY_MASK    0x007F0000
#define TI_FIBERLATENCYMEASUREMENT_DATA_MASK       0xFF800000

/* 0xA4 fiberAlignment bits and masks */
#define TI_FIBERALIGNMENT_HFBR1_IODELAY_MASK   0x000000FF
#define TI_FIBERALIGNMENT_HFBR1_SYNCDELAY_MASK 0x0000FF00
#define TI_FIBERALIGNMENT_HFBR5_IODELAY_MASK   0x00FF0000
#define TI_FIBERALIGNMENT_HFBR5_SYNCDELAY_MASK 0xFF000000

/* 0xC0 blockStatus bits and masks */
#define TI_BLOCKSTATUS_NBLOCKS_READY0    0x000000FF
#define TI_BLOCKSTATUS_NBLOCKS_NEEDACK0  0x0000FF00
#define TI_BLOCKSTATUS_NBLOCKS_READY1    0x00FF0000
#define TI_BLOCKSTATUS_NBLOCKS_NEEDACK1  0xFF000000

/* 0xD0 adr24 bits and masks */
#define TI_ADR24_ADDRESS_MASK         0x0000001F
#define TI_ADR24_HARDWARE_SET_MASK    0x000003E0
#define TI_ADR24_GEOADDR_MASK         0x00007C00
#define TI_ADR24_TM_NBLOCKS_READY1    0x00FF0000
#define TI_ADR24_TM_NBLOCKS_NEEDACK1  0xFF000000

/* 0xD4 syncEventCtrl bits and masks */
#define TI_SYNCEVENTCTRL_NBLOCKS_MASK 0x00FFFFFF

/* 0xD8 eventNumber_hi bits and masks */
#define TI_PROMPT_TRIG_WIDTH_MASK     0x0000007F
#define TI_EVENTNUMBER_HI_MASK        0xFFFF0000


/* 0xEC rocEnable bits and masks */
#define TI_ROCENABLE_MASK             0x000000FF
#define TI_ROCENABLE_ROC(x)           (1<<(x))

/* 0x100 reset bits and masks */
#define TI_RESET_I2C                  (1<<1)
#define TI_RESET_JTAG                 (1<<2)
#define TI_RESET_SFM                  (1<<3)
#define TI_RESET_SOFT                 (1<<4)
#define TI_RESET_SYNC_HISTORY         (1<<6)
#define TI_RESET_BUSYACK              (1<<7)
#define TI_RESET_CLK250               (1<<8)
#define TI_RESET_CLK200               (1<<8)
#define TI_RESET_CLK125               (1<<9)
#define TI_RESET_MGT                  (1<<10)
#define TI_RESET_AUTOALIGN_HFBR1_SYNC (1<<11)
#define TI_RESET_AUTOALIGN_HFBR5_SYNC (1<<12)
#define TI_RESET_RAM_WRITE            (1<<12)
#define TI_RESET_FIBER_AUTO_ALIGN     (1<<13)
#define TI_RESET_IODELAY              (1<<14)
#define TI_RESET_MEASURE_LATENCY      (1<<15)
#define TI_RESET_TAKE_TOKEN           (1<<16)
#define TI_RESET_BLOCK_READOUT        (1<<17)
#define TI_RESET_FORCE_SYNCEVENT      (1<<20)
#define TI_RESET_SYNCRESET_REQUEST    (1<<23)
#define TI_RESET_SCALERS_LATCH        (1<<24)
#define TI_RESET_SCALERS_RESET        (1<<25)
#define TI_RESET_FILL_TO_END_BLOCK    (1<<31)

/* 0x104 fpDelay Masks */
#define TI_FPDELAY_MASK(x) (0x1FF<<(10*(x%3)))

/* 0x138 triggerRuleMin bits and masks */
#define TI_TRIGGERRULEMIN_MIN2_MASK  0x00007F00
#define TI_TRIGGERRULEMIN_MIN2_EN    (1<<15)
#define TI_TRIGGERRULEMIN_MIN3_MASK  0x007F0000
#define TI_TRIGGERRULEMIN_MIN3_EN    (1<<23)
#define TI_TRIGGERRULEMIN_MIN4_MASK  0x7F000000
#define TI_TRIGGERRULEMIN_MIN4_EN    (1<<31)

/* 0x1D0-0x1F0 TI ID bits and masks */
#define TI_ID_TRIGSRC_ENABLE_MASK     0x000000FF
#define TI_ID_CRATEID_MASK            0x0000FF00
#define TI_ID_BLOCKLEVEL_MASK         0x00FF0000

/* Trigger Sources, used by tiSetTriggerSource  */
#define TI_TRIGGER_P0        0
#define TI_TRIGGER_HFBR1     1
#define TI_TRIGGER_FPTRG     2
#define TI_TRIGGER_TSINPUTS  3
#define TI_TRIGGER_TSREV2    4
#define TI_TRIGGER_RANDOM    5
#define TI_TRIGGER_PULSER    5
#define TI_TRIGGER_PART_1    6
#define TI_TRIGGER_PART_2    7
#define TI_TRIGGER_PART_3    8
#define TI_TRIGGER_PART_4    9
#define TI_TRIGGER_HFBR5    10
#define TI_TRIGGER_TRIG21   11

/* Define default Interrupt vector and level */
#define TI_INT_VEC      0xec
/* #define TI_INT_VEC      0xc8 */
#define TI_INT_LEVEL    5

/* i2c data masks - 16bit data default */
#define TI_I2C_DATA_MASK             0x0000ffff
#define TI_I2C_8BIT_DATA_MASK        0x000000ff

/* Data buffer bits and masks */
#define TI_DATA_TYPE_DEFINE_MASK           0x80000000
#define TI_WORD_TYPE_MASK                  0x78000000
#define TI_FILLER_WORD_TYPE                0x78000000
#define TI_BLOCK_HEADER_WORD_TYPE          0x00000000
#define TI_BLOCK_TRAILER_WORD_TYPE         0x08000000
#define TI_EMPTY_FIFO                      0xF0BAD0F0
#define TI_BLOCK_HEADER_CRATEID_MASK       0xFF000000
#define TI_BLOCK_HEADER_SLOTS_MASK         0x001F0000
#define TI_BLOCK_TRAILER_CRATEID_MASK      0x00FF0000
#define TI_BLOCK_TRAILER_SLOTS_MASK        0x1F000000
#define TI_DATA_BLKNUM_MASK                0x0000FF00
#define TI_DATA_BLKLEVEL_MASK              0x000000FF

/* tiInit initialization flag bits */
#define TI_INIT_NO_INIT                 (1<<0)
#define TI_INIT_SLAVE_FIBER_5           (1<<1)
#define TI_INIT_SKIP_FIRMWARE_CHECK     (1<<2)

/* Some pre-initialization routine prototypes */
int  tiSetFiberLatencyOffset_preInit(int flo);
int  tiSetCrateID_prIinit(int cid);

/* Function prototypes */
int  tiInit(unsigned int mode, int force);

#ifdef NOTDONEYET
int  tiCheckAddresses();
void tiStatus(int pflag);
int  tiSetSlavePort(int port);
int  tiGetSlavePort();
void tiSlaveStatus(int pflag);
int  tiGetFirmwareVersion();
int  tiReload();
unsigned int tiGetSerialNumber(char **rSN);
int  tiClockResync();
int  tiReset();
int  tiSetCrateID(unsigned int crateID);
int  tiGetCrateID(int port);
int  tiGetPortTrigSrcEnabled(int port);
int  tiGetSlaveBlocklevel(int port);
int  tiSetBlockLevel(int blockLevel);
int  tiBroadcastNextBlockLevel(int blockLevel);
int  tiGetNextBlockLevel();
int  tiGetCurrentBlockLevel();
int  tiSetInstantBlockLevelChange(int enable);
int  tiGetInstantBlockLevelChange();
int  tiSetTriggerSource(int trig);
int  tiSetTriggerSourceMask(int trigmask);
int  tiEnableTriggerSource();
int  tiDisableTriggerSource(int fflag);
int  tiSetSyncSource(unsigned int sync);
int  tiSetEventFormat(int format);
int  tiSoftTrig(int trigger, unsigned int nevents, unsigned int period_inc, int range);
int  tiSetRandomTrigger(int trigger, int setting);
int  tiDisableRandomTrigger();
int  tiReadBlock(volatile unsigned int *data, int nwrds, int rflag);
int  tiReadTriggerBlock(volatile unsigned int *data);
int  tiCheckTriggerBlock(volatile unsigned int *data);
int  tiEnableFiber(unsigned int fiber);
int  tiDisableFiber(unsigned int fiber);
int  tiSetBusySource(unsigned int sourcemask, int rFlag);
int  tiSetTriggerLock(int enable);
int  tiGetTriggerLock();
void tiEnableBusError();
void tiDisableBusError();
int  tiPayloadPort2VMESlot(int payloadport);
unsigned int  tiPayloadPortMask2VMESlotMask(unsigned int ppmask);
int  tiVMESlot2PayloadPort(int vmeslot);
unsigned int  tiVMESlotMask2PayloadPortMask(unsigned int vmemask);
int  tiSetPrescale(int prescale);
int  tiGetPrescale();
int  tiSetInputPrescale(int input, int prescale);
int  tiGetInputPrescale(int input);
int  tiSetTriggerPulse(int trigger, int delay, int width, int delay_step);
int  tiSetPromptTriggerWidth(int width);
int  tiGetPromptTriggerWidth();
void tiSetSyncDelayWidth(unsigned int delay, unsigned int width, int widthstep);
void tiTrigLinkReset();
int  tiSetSyncResetType(int type);
void tiSyncReset(int bflag);
void tiSyncResetResync();
void tiClockReset();
int  tiSetAdr32(unsigned int a32base);
int  tiDisableA32();
int  tiResetEventCounter();
unsigned long long int tiGetEventCounter();
int  tiSetBlockLimit(unsigned int limit);
unsigned int  tiGetBlockLimit();
unsigned int  tiBReady();
int  tiGetSyncEventFlag();
int  tiGetSyncEventReceived();
int  tiGetReadoutEvents();
int  tiEnableVXSSignals();
int  tiDisableVXSSignals();
int  tiSetBlockBufferLevel(unsigned int level);
int  tiEnableTSInput(unsigned int inpMask);
int  tiDisableTSInput(unsigned int inpMask);
int  tiSetOutputPort(unsigned int set1, unsigned int set2, unsigned int set3, unsigned int set4);
int  tiSetClockSource(unsigned int source);
int  tiGetClockSource();
void  tiSetFiberDelay(unsigned int delay, unsigned int offset);
int  tiAddSlave(unsigned int fiber);
int  tiSetTriggerHoldoff(int rule, unsigned int value, int timestep);
int  tiGetTriggerHoldoff(int rule);
int  tiSetTriggerHoldoffMin(int rule, unsigned int value);
int  tiGetTriggerHoldoffMin(int rule, int pflag);

int  tiDisableDataReadout();
int  tiEnableDataReadout();
void tiResetBlockReadout();

int  tiTriggerTableConfig(unsigned int *itable);
int  tiGetTriggerTable(unsigned int *otable);
int  tiTriggerTablePredefinedConfig(int mode);
int  tiDefineEventType(int trigMask, int hwTrig, int evType);
int  tiDefinePulserEventType(int fixed_type, int random_type);
int  tiLoadTriggerTable(int mode);
void tiPrintTriggerTable(int showbits);
int  tiSetTriggerWindow(int window_width);
int  tiGetTriggerWindow();
int  tiSetTriggerInhibitWindow(int window_width);
int  tiGetTriggerInhibitWindow();
int  tiSetTrig21Delay(int delay);
int  tiGetTrig21Delay();
int  tiLatchTimers();
unsigned int tiGetLiveTime();
unsigned int tiGetBusyTime();
int  tiLive(int sflag);
unsigned int tiGetTSscaler(int input, int latch);
unsigned int tiBlockStatus(int fiber, int pflag);

int  tiGetFiberLatencyMeasurement();
int  tiSetUserSyncResetReceive(int enable);
int  tiGetLastSyncCodes(int pflag);
int  tiGetSyncHistoryBufferStatus(int pflag);
void tiResetSyncHistory();
void tiUserSyncReset(int enable, int pflag);
void tiPrintSyncHistory();
int  tiSetSyncEventInterval(int blk_interval);
int  tiGetSyncEventInterval();
int  tiForceSyncEvent();
int  tiSyncResetRequest();
int  tiGetSyncResetRequest();
void tiTriggerReadyReset();
int  tiFillToEndBlock();
int  tiResetMGT();
int  tiSetTSInputDelay(int chan, int delay);
int  tiGetTSInputDelay(int chan);
int  tiPrintTSInputDelay();
unsigned int tiGetGTPBufferLength(int pflag);
unsigned int tiGetSWAStatus(int reg);
unsigned int tiGetSWBStatus(int reg);
int  tiGetGeoAddress();

/* Library Interrupt/Polling routine prototypes */
int  tiIntConnect(unsigned int vector, VOIDFUNCPTR routine, unsigned int arg);
int  tiIntDisconnect();
int  tiAckConnect(VOIDFUNCPTR routine, unsigned int arg);
void tiIntAck();
int  tiIntEnable(int iflag);
void tiIntDisable();
unsigned int  tiGetIntCount();
unsigned int  tiGetAckCount();

int  tiGetSWBBusy(int pflag);
unsigned int tiGetBusyCounter(int busysrc);
int  tiPrintBusyCounters();

/* Some token testing routines */
int  tiSetTokenTestMode(int mode);
int  tiSetTokenOutTest(int level);

int  tiRocEnable(int roc);
int  tiRocEnableMask(int rocmask);
int  tiGetRocEnableMask();
#endif /* NOTDONEYET */

unsigned int tipRead(volatile unsigned int *reg);
int  tipWrite(volatile unsigned int *reg, unsigned int value);
int  tipReadBlock(int bar, unsigned int *reg, unsigned int *value, int nreg);
int  tipWriteBlock(int bar, unsigned int *reg, unsigned int *value, int nreg);
#endif /* TILIB_H */
