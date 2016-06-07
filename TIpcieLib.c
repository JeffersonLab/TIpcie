/*----------------------------------------------------------------------------*/
/**
 * @mainpage
 * <pre>
 *  Copyright (c) 2015        Southeastern Universities Research Association, *
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
 *     Primitive trigger control for Intel CPUs running Linux using the TJNAF 
 *     Trigger Interface (TI) PCIexpress card
 *
 * </pre>
 *----------------------------------------------------------------------------*/

#define _GNU_SOURCE

#define DEVEL
/* #define ALLOCMEM */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
/* #define WAITFORDATA */
#include "TIpcieLib.h"

#define TIPCIE_IOC_MAGIC  'k'
#define TIPCIE_IOC_RW         _IO(TIPCIE_IOC_MAGIC, 1)
#define TIPCIE_IOC_MEM        _IO(TIPCIE_IOC_MAGIC, 2)

#define TIPCIE_WRITE 0
#define TIPCIE_READ  1
#define TIPCIE_STAT 2

#define TIPCIE_MEM_ALLOC 0
#define TIPCIE_MEM_FREE  1

/* #define OLDWAY 1 */
/* #define OLDWAY2 1 */

typedef struct pci_ioctl_struct
{
  int command_type;
  int mem_region;
  unsigned int nreg;
  unsigned int *reg;
  unsigned int *value;
} PCI_IOCTL_INFO;

typedef struct DMA_BUF_INFO_STRUCT
{
  uint64_t  dma_osspec_hdl;
  uint64_t  command_type;
  uint64_t  phys_addr;
  uint64_t  virt_addr;
  uint64_t   size;
} DMA_BUF_INFO;

typedef struct DMA_MAP_STRUCT
{
  uint64_t          dmaHdl;
  volatile unsigned long        map_addr;
  unsigned int             size;
} DMA_MAP_INFO;

/* Mutex to guard TI read/writes */
pthread_mutex_t   tipcieMutex = PTHREAD_MUTEX_INITIALIZER;
#define TIPLOCK     if(pthread_mutex_lock(&tipcieMutex)<0) perror("pthread_mutex_lock");
#define TIPUNLOCK   if(pthread_mutex_unlock(&tipcieMutex)<0) perror("pthread_mutex_unlock");

/* Global Variables */
volatile struct TIPCIE_RegStruct  *TIPp=NULL;    /* pointer to TI memory map */
volatile unsigned int *TIPpd=NULL;             /* pointer to TI DMA memory */
volatile unsigned int *TIPpj=NULL;             /* pointer to TI JTAG memory */
static unsigned long tipDmaAddrBase=0;
static void          *tipMappedBase;
static void          *tipJTAGMappedBase;

static int tipUseDma=0; 
int tipMaster=1;                               /* Whether or not this TIP is the Master */
int tipCrateID=0x59;                           /* Crate ID */
int tipBlockLevel=0;                           /* Current Block level for TIP */
int tipNextBlockLevel=0;                       /* Next Block level for TIP */
unsigned int        tipIntCount    = 0;
unsigned int        tipAckCount    = 0;
unsigned int        tipDaqCount    = 0;       /* Block count from previous update (in daqStatus) */
unsigned int        tipReadoutMode = 0;
unsigned int        tipTriggerSource = 0;     /* Set with tipSetTriggerSource(...) */
unsigned int        tipSlaveMask   = 0;       /* TIP Slaves (mask) to be used with TIP Master */
int                 tipDoAck       = 0;
int                 tipNeedAck     = 0;
static BOOL         tipIntRunning  = FALSE;   /* running flag */
static VOIDFUNCPTR  tipIntRoutine  = NULL;    /* user intererrupt service routine */
static int          tipIntArg      = 0;       /* arg to user routine */
static unsigned int tipIntLevel    = TIP_INT_LEVEL;       /* VME Interrupt level */
static unsigned int tipIntVec      = TIP_INT_VEC;  /* default interrupt vector */
static VOIDFUNCPTR  tipAckRoutine  = NULL;    /* user trigger acknowledge routine */
static int          tipAckArg      = 0;       /* arg to user trigger ack routine */
static int          tipReadoutEnabled = 1;    /* Readout enabled, by default */
static int          tipFiberLatencyMeasurement = 0; /* Measured fiber latency */
static int          tipSyncEventFlag = 0;     /* Sync Event/Block Flag */
static int          tipSyncEventReceived = 0; /* Indicates reception of sync event */
static int          tipNReadoutEvents = 0;    /* Number of events to readout from crate modules */
static int          tipDoSyncResetRequest =0; /* Option to request a sync reset during readout ack */
static int          tipSyncResetType=TIP_SYNCCOMMAND_SYNCRESET_4US;  /* Set default SyncReset Type to Fixed 4 us */
static int          tipVersion     = 0x0;     /* Firmware version */
int                 tipFiberLatencyOffset = 0xbf; /* Default offset for fiber latency */
static int          tipDoIntPolling= 1;       /* Decision to use library polling thread routine */
static int tipFD;
static DMA_MAP_INFO tipMapInfo;

static unsigned int tipTrigPatternData[16]=   /* Default Trigger Table to be loaded */
  { /* TS#1,2,3,4,5,6 generates Trigger1 (physics trigger),
       No Trigger2 (playback trigger),
       No SyncEvent;
    */
    0x43424100, 0x47464544, 0x4b4a4948, 0x4f4e4d4c,
    0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c,
    0x63626160, 0x67666564, 0x6b6a6968, 0x6f6e6d6c,
    0x73727170, 0x77767574, 0x7b7a7978, 0x7f7e7d7c,
  };

static int  tipRW(PCI_IOCTL_INFO info);
#ifdef ALLOCMEM
static DMA_MAP_INFO tipAllocDmaMemory(int size, unsigned long *phys_addr);
static int  tipFreeDmaMemory(DMA_MAP_INFO mapInfo);
#endif /* ALLOCMEM */
static int  tipGetPciBar(unsigned int *regs);

static int tipMemCount=0;

#ifdef WAITFORDATA
static int tipWaitForData();
#endif

/* Interrupt/Polling routine prototypes (static) */
#ifdef NOTDONEYET
static void tipInt(void);
#endif
static void tipPoll(void);
static void tipStartPollingThread(void);
/* polling thread pthread and pthread_attr */
pthread_attr_t tippollthread_attr;
pthread_t      tippollthread=0;

static void FiberMeas();

/**
 * @defgroup PreInit Pre-Initialization
 * @defgroup SlavePreInit Slave Pre-Initialization
 *   @ingroup PreInit
 * @defgroup Config Initialization/Configuration
 * @defgroup MasterConfig Master Configuration
 *   @ingroup Config
 * @defgroup SlaveConfig Slave Configuration
 *   @ingroup Config
 * @defgroup Status Status
 * @defgroup MasterStatus Master Status
 *   @ingroup Status
 * @defgroup Readout Data Readout
 * @defgroup MasterReadout Master Data Readout
 *   @ingroup Readout
 * @defgroup IntPoll Interrupt/Polling
 * @defgroup Deprec Deprecated - To be removed
 */

unsigned long long int rdtsc(void)
{
  /*    unsigned long long int x; */
  unsigned a, d;
   
  __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

  return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}


/**
 * @ingroup PreInit
 * @brief Set the Fiber Latency Offset to be used during initialization
 *
 * @param flo fiber latency offset
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetFiberLatencyOffset_preInit(int flo)
{
  if((flo<0) || (flo>0x1ff))
    {
      printf("%s: ERROR: Invalid Fiber Latency Offset (%d)\n",
	     __FUNCTION__,flo);
      return ERROR;
    }

  tipFiberLatencyOffset = flo;

  return OK;
}

/**
 * @ingroup PreInit
 * @brief Set the CrateID to be used during initialization
 *
 * @param cid Crate ID
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetCrateID_preInit(int cid)
{
  if((cid<0) || (cid>0xff))
    {
      printf("%s: ERROR: Invalid Crate ID (%d)\n",
	     __FUNCTION__,cid);
      return ERROR;
    }

  tipCrateID = cid;

  return OK;
}


/**
 *  @ingroup Config
 *  @brief Initialize the TIp register space into local memory,
 *  and setup registers given user input
 *
 *  @param  mode  Readout/Triggering Mode
 *     - 0 External Trigger - Interrupt Mode
 *     - 1 TI/TImaster Trigger - Interrupt Mode
 *     - 2 External Trigger - Polling Mode
 *     - 3 TI/TImaster Trigger - Polling Mode
 *
 *  @param iFlag Initialization bit mask
 *     - 0   Do not initialize the board, just setup the pointers to the registers
 *     - 1   Ignore firmware check
 *
 *  @return OK if successful, otherwise ERROR.
 *
 */

float demon=0;

int
tipInit(unsigned int mode, int iFlag)
{
  unsigned int rval, prodID;
  unsigned int firmwareInfo;
  int noBoardInit=0, noFirmwareCheck=0;

  if(iFlag&TIP_INIT_NO_INIT)
    {
      noBoardInit = 1;
    }
  if(iFlag&TIP_INIT_SKIP_FIRMWARE_CHECK)
    {
      noFirmwareCheck=1;
    }
  if(iFlag&TIP_INIT_USE_DMA)
    {
      tipUseDma=1;
    }

  /* Pointer should already be set up tipOpen */
  if(TIPp==NULL)
    {
      printf("%s: Pointer not initialized.  Calling tipOpen()\n",
	     __FUNCTION__);
      if(tipOpen()==ERROR)
	return -1;
    }
  
  /* Check if TI board is readable */
  /* Read the boardID reg */
  rval = tipRead(&TIPp->boardID);

  if (rval == ERROR) 
    {
      printf("%s: ERROR: TIpcie card not addressable\n",__FUNCTION__);
      TIPp=NULL;
      return(-1);
    }
  else
    {
      /* Check that it is a TI */
      if(((rval&TIP_BOARDID_TYPE_MASK)>>16) != TIP_BOARDID_TYPE_TI) 
	{
	  printf("%s: ERROR: Invalid Board ID: 0x%x (rval = 0x%08x)\n",
		 __FUNCTION__,
		 (rval&TIP_BOARDID_TYPE_MASK)>>16,rval);
	  TIPp=NULL;
	  return(ERROR);
	}

      /* Get the "production" type bits.  2=modTI, 1=production, 0=prototype */
      prodID = (rval&TIP_BOARDID_PROD_MASK)>>16;

    }

  if(!noBoardInit)
    {
      if(tipMaster==0) /* Reload only on the TI Slaves */
	{
	  /* tiReload(); */
	  /* taskDelay(60); */
	}
      tipDisableTriggerSource(0);  
    }

  /* Get the Firmware Information and print out some details */
  firmwareInfo = tipGetFirmwareVersion();
  if(firmwareInfo>0)
    {
      int supportedVersion = TIP_SUPPORTED_FIRMWARE;
      int supportedType    = TIP_SUPPORTED_TYPE;
      int tipFirmwareType   = (firmwareInfo & TIP_FIRMWARE_TYPE_MASK)>>12;

      tipVersion = firmwareInfo&0xFFF;
      printf("  ID: 0x%x \tFirmware (type - revision): 0x%X - 0x%03X\n",
	     (firmwareInfo&TIP_FIRMWARE_ID_MASK)>>16, tipFirmwareType, tipVersion);

      if(tipFirmwareType != supportedType)
	{
	  if(noFirmwareCheck)
	    {
	      printf("%s: WARN: Firmware type (%d) not supported by this driver.\n  Supported type = %d  (IGNORED)\n",
		     __FUNCTION__,tipFirmwareType,supportedType);
	    }
	  else
	    {
	      printf("%s: ERROR: Firmware Type (%d) not supported by this driver.\n  Supported type = %d\n",
		     __FUNCTION__,tipFirmwareType,supportedType);
	      TIPp=NULL;
	      return ERROR;
	    }
	}

      if(tipVersion != supportedVersion)
	{
	  if(noFirmwareCheck)
	    {
	      printf("%s: WARN: Firmware version (0x%x) not supported by this driver.\n  Supported version = 0x%x  (IGNORED)\n",
		     __FUNCTION__,tipVersion,supportedVersion);
	    }
	  else
	    {
	      printf("%s: ERROR: Firmware version (0x%x) not supported by this driver.\n  Supported version = 0x%x\n",
		     __FUNCTION__,tipVersion,supportedVersion);
	      TIPp=NULL;
	      return ERROR;
	    }
	}
    }
  else
    {
      printf("%s:  ERROR: Invalid firmware 0x%08x\n",
	     __FUNCTION__,firmwareInfo);
      return ERROR;
    }

  /* Check if we should exit here, or initialize some board defaults */
  if(noBoardInit)
    {
      return OK;
    }

  tipReset();

  // FIXME Put this somewhere else.
  if(tipUseDma)
    {
      printf("%s: Configuring DMA\n",__FUNCTION__);
      tipDmaConfig(1, 0, 2); /* set up the DMA, 1MB, 32-bit, 256 byte packet */
      tipDmaSetAddr(tipDmaAddrBase,0);
    }
  else
    {
      printf("%s: Configuring FIFO\n",__FUNCTION__);
      tipEnableFifo();
    }

  /* Set some defaults, dependent on Master/Slave status */
  switch(mode)
    {
    case TIP_READOUT_EXT_INT:
    case TIP_READOUT_EXT_POLL:
      printf("... Configure as TI Master...\n");
      /* Master (Supervisor) Configuration: takes in external triggers */
      tipMaster = 1;

      /* Clear the Slave Mask */
      tipSlaveMask = 0;

      /* Self as busy source */
      tipSetBusySource(TIP_BUSY_LOOPBACK,1);

      /* Onboard Clock Source */
      tipSetClockSource(TIP_CLOCK_INTERNAL);
      /* Loopback Sync Source */
      tipSetSyncSource(TIP_SYNC_LOOPBACK);
      break;

    case TIP_READOUT_TS_INT:
    case TIP_READOUT_TS_POLL:
      printf("... Configure as TI Slave...\n");
      /* Slave Configuration: takes in triggers from the Master (supervisor) */
      tipMaster = 0;

      /* BUSY  */
      tipSetBusySource(0,1);

      /* Enable HFBR#1 */
      tipEnableFiber(1);
      /* HFBR#1 Clock Source */
      tipSetClockSource(1);
      /* HFBR#1 Sync Source */
      tipSetSyncSource(TIP_SYNC_HFBR1);
      /* HFBR#1 Trigger Source */
      tipSetTriggerSource(TIP_TRIGGER_HFBR1);

      break;

    default:
      printf("%s: ERROR: Invalid TI Mode %d\n",
	     __FUNCTION__,mode);
      return ERROR;
    }
  tipReadoutMode = mode;

#ifdef SKIPTHIS
  /* Setup some Other Library Defaults */
  if(tipMaster!=1)
    {
      FiberMeas();

      tipWrite(&TIPp->syncWidth, 0x24);
      // TI IODELAY reset
      tipWrite(&TIPp->reset,TIP_RESET_IODELAY);
      usleep(10000);

      // TI Sync auto alignment
      tipWrite(&TIPp->reset,TIP_RESET_AUTOALIGN_HFBR1_SYNC);
      usleep(10000);

      // TI auto fiber delay measurement
      tipWrite(&TIPp->reset,TIP_RESET_MEASURE_LATENCY);
      usleep(10000);

      // TI auto alignement fiber delay
      tipWrite(&TIPp->reset,TIP_RESET_FIBER_AUTO_ALIGN);
      usleep(10000);
    }
  else
    {
      // TI IODELAY reset
      tipWrite(&TIPp->reset,TIP_RESET_IODELAY);
      usleep(10000);

      // TI Sync auto alignment
      tipWrite(&TIPp->reset,TIP_RESET_AUTOALIGN_HFBR1_SYNC);
      usleep(10000);

      // Perform a trigger link reset
      tipTrigLinkReset();
      usleep(10000);
    }

  /* Setup a default Sync Delay and Pulse width */
  if(tipMaster==1)
    tipSetSyncDelayWidth(0x54, 0x2f, 0);

  /* Set default sync delay (fiber compensation) */
  if(tipMaster==1)
    tipWrite(&TIPp->fiberSyncDelay,
	       (tipFiberLatencyOffset<<16)&TIP_FIBERSYNCDELAY_LOOPBACK_SYNCDELAY_MASK);

#endif /* SKIPTHIS */

  /* Set Default Block Level to 1, and default crateID */
  if(tipMaster==1)
    tipSetBlockLevel(1);

  tipSetCrateID(tipCrateID);

  /* Set Event format for CODA 3.0 */
  tipSetEventFormat(3);

  /* Set Default Trig1 and Trig2 delay=16ns (0+1)*16ns, width=64ns (15+1)*4ns */
  tipSetTriggerPulse(1,0,15,0);
  tipSetTriggerPulse(2,0,15,0);

  /* Set the default prescale factor to 0 for rate/(0+1) */
  tipSetPrescale(0);

  /* MGT reset */
  if(tipMaster==1)
    {
      tipResetMGT();
    }

  /* Set this to 1 (ROC Lock mode), by default. */
  tipSetBlockBufferLevel(1);

  /* Disable all TS Inputs */
  tipDisableTSInput(TIP_TSINPUT_ALL);

  return OK;
}

int
tipCheckAddresses()
{
  unsigned long offset=0, expected=0, base=0;
  
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  printf("%s:\n\t ---------- Checking TI address space ---------- \n",__FUNCTION__);

  base = (unsigned long) &TIPp->boardID;

  offset = ((unsigned long) &TIPp->trigsrc) - base;
  expected = 0x20;
  if(offset != expected)
    printf("%s: ERROR TIPp->triggerSource not at offset = 0x%lx (@ 0x%lx)\n",
	   __FUNCTION__,expected,offset);

  offset = ((unsigned long) &TIPp->syncWidth) - base;
  expected = 0x80;
  if(offset != expected)
    printf("%s: ERROR TIPp->syncWidth not at offset = 0x%lx (@ 0x%lx)\n",
	   __FUNCTION__,expected,offset);
    
  offset = ((unsigned long) &TIPp->adr24) - base;
  expected = 0xD0;
  if(offset != expected)
    printf("%s: ERROR TIPp->adr24 not at offset = 0x%lx (@ 0x%lx)\n",
	   __FUNCTION__,expected,offset);
    
  offset = ((unsigned long) &TIPp->reset) - base;
  expected = 0x100;
  if(offset != expected)
    printf("%s: ERROR TIPp->reset not at offset = 0x%lx (@ 0x%lx)\n",
	   __FUNCTION__,expected,offset);
    
  return OK;
}

/**
 * @ingroup Status
 * @brief Print some status information of the TI to standard out
 * 
 * @param pflag if pflag>0, print out raw registers
 *
 */

void
tipStatus(int pflag)
{
  struct TIPCIE_RegStruct ro;
  int iinp, iblock, ifiber;
  unsigned int blockStatus[5], nblocksReady, nblocksNeedAck;
  unsigned int fibermask;
  unsigned long TIBase;
  unsigned long long int l1a_count=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  /* latch live and busytime scalers */
  tipLatchTimers();
  l1a_count    = tipGetEventCounter();
  tipGetCurrentBlockLevel();

  TIPLOCK;
  ro.boardID      = tipRead(&TIPp->boardID);
  ro.fiber        = tipRead(&TIPp->fiber);
  ro.intsetup     = tipRead(&TIPp->intsetup);
  ro.trigDelay    = tipRead(&TIPp->trigDelay);
  ro.__adr32      = tipRead(&TIPp->__adr32);
  ro.blocklevel   = tipRead(&TIPp->blocklevel);
  ro.vmeControl   = tipRead(&TIPp->vmeControl);
  ro.trigsrc      = tipRead(&TIPp->trigsrc);
  ro.sync         = tipRead(&TIPp->sync);
  ro.busy         = tipRead(&TIPp->busy);
  ro.clock        = tipRead(&TIPp->clock);
  ro.trig1Prescale = tipRead(&TIPp->trig1Prescale);
  ro.blockBuffer  = tipRead(&TIPp->blockBuffer);

  ro.tsInput      = tipRead(&TIPp->tsInput);

  ro.output       = tipRead(&TIPp->output);
  ro.blocklimit   = tipRead(&TIPp->blocklimit);
  ro.fiberSyncDelay = tipRead(&TIPp->fiberSyncDelay);

  ro.GTPStatusA   = tipRead(&TIPp->GTPStatusA);
  ro.GTPStatusB   = tipRead(&TIPp->GTPStatusB);

  /* Latch scalers first */
  tipWrite(&TIPp->reset,TIP_RESET_SCALERS_LATCH);
  ro.livetime     = tipRead(&TIPp->livetime);
  ro.busytime     = tipRead(&TIPp->busytime);

  ro.inputCounter = tipRead(&TIPp->inputCounter);

  for(iblock=0;iblock<4;iblock++)
    blockStatus[iblock] = tipRead(&TIPp->blockStatus[iblock]);

  blockStatus[4] = tipRead(&TIPp->adr24);

  ro.nblocks      = tipRead(&TIPp->nblocks);

  ro.GTPtriggerBufferLength = tipRead(&TIPp->GTPtriggerBufferLength);

  ro.rocEnable    = tipRead(&TIPp->rocEnable);
  TIPUNLOCK;

  TIBase = (unsigned long)TIPp;

  printf("\n");
  printf("STATUS for TIpcie\n");
  printf("--------------------------------------------------------------------------------\n");
  /* printf(" A32 Data buffer "); */
  /* if((ro.vmeControl&TIP_VMECONTROL_A32) == TIP_VMECONTROL_A32) */
  /*   { */
  /*     printf("ENABLED at "); */
  /*     printf("VME (Local) base address 0x%08lx (0x%lx)\n", */
  /* 	     (unsigned long)TIPpd - tiA32Offset, (unsigned long)TIPpd); */
  /*   } */
  /* else */
  /*   printf("DISABLED\n"); */

  if(tipMaster)
    printf(" Configured as a TI Master\n");
  else
    printf(" Configured as a TI Slave\n");

  printf(" Readout Count: %d\n",tipIntCount);
  printf("     Ack Count: %d\n",tipAckCount);
  printf("     L1A Count: %llu\n",l1a_count);
  printf("   Block Limit: %d   %s\n",ro.blocklimit,
	 (ro.blockBuffer & TIP_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT)?"* Finished *":"- In Progress -");
  printf("   Block Count: %d\n",ro.nblocks & TIP_NBLOCKS_COUNT_MASK);

  if(pflag>0)
    {
      printf(" Registers (offset):\n");
      printf("  boardID        (0x%04lx) = 0x%08x\t", (unsigned long)&TIPp->boardID - TIBase, ro.boardID);
      printf("  fiber          (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->fiber) - TIBase, ro.fiber);
      printf("  intsetup       (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->intsetup) - TIBase, ro.intsetup);
      printf("  trigDelay      (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->trigDelay) - TIBase, ro.trigDelay);
      printf("  __adr32        (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->__adr32) - TIBase, ro.__adr32);
      printf("  blocklevel     (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->blocklevel) - TIBase, ro.blocklevel);
      printf("  vmeControl     (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->vmeControl) - TIBase, ro.vmeControl);
      printf("  trigger        (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->trigsrc) - TIBase, ro.trigsrc);
      printf("  sync           (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->sync) - TIBase, ro.sync);
      printf("  busy           (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->busy) - TIBase, ro.busy);
      printf("  clock          (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->clock) - TIBase, ro.clock);
      printf("  blockBuffer    (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->blockBuffer) - TIBase, ro.blockBuffer);
      
      printf("  output         (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->output) - TIBase, ro.output);
      printf("  fiberSyncDelay (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->fiberSyncDelay) - TIBase, ro.fiberSyncDelay);

      printf("  GTPStatusA     (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->GTPStatusA) - TIBase, ro.GTPStatusA);
      printf("  GTPStatusB     (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->GTPStatusB) - TIBase, ro.GTPStatusB);
      
      printf("  livetime       (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->livetime) - TIBase, ro.livetime);
      printf("  busytime       (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->busytime) - TIBase, ro.busytime);
      printf("  GTPTrgBufLen   (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->GTPtriggerBufferLength) - TIBase, ro.GTPtriggerBufferLength);
      printf("  rocEnable      (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->rocEnable) - TIBase, ro.rocEnable);
    }
  printf("\n");

  if((!tipMaster) && (tipBlockLevel==0))
    {
      printf(" Block Level not yet received\n");
    }      
  else
    {
      printf(" Block Level = %d ", tipBlockLevel);
      if(tipBlockLevel != tipNextBlockLevel)
	printf("(To be set = %d)\n", tipNextBlockLevel);
      else
	printf("\n");
    }

  fibermask = ro.fiber;
  if(tipMaster)
    {
      if(fibermask)
	{
	  printf(" HFBR enabled (0x%x)= \n",fibermask&0xf);
	  for(ifiber=0; ifiber<8; ifiber++)
	    {
	      if( fibermask & (1<<ifiber) ) 
		printf("   %d: -%s-   -%s-\n",ifiber+1,
		       (ro.fiber & TIP_FIBER_CONNECTED_TI(ifiber+1))?"    CONNECTED":"NOT CONNECTED",
		       (ro.fiber & TIP_FIBER_TRIGSRC_ENABLED_TI(ifiber+1))?"TRIGSRC ENABLED":"TRIGSRC DISABLED");
	    }
	  printf("\n");
	}
      else
	printf(" All HFBR Disabled\n");
    }

  if(tipMaster)
    {
      if(tipSlaveMask)
	{
	  printf(" TI Slaves Configured on HFBR (0x%x) = ",tipSlaveMask);
	  fibermask = tipSlaveMask;
	  for(ifiber=0; ifiber<8; ifiber++)
	    {
	      if( fibermask & (1<<ifiber)) 
		printf(" %d",ifiber+1);
	    }
	  printf("\n");	
	}
      else
	printf(" No TI Slaves Configured on HFBR\n");
      
    }

  printf(" Clock Source (%d) = \n",ro.clock & TIP_CLOCK_MASK);
  switch(ro.clock & TIP_CLOCK_MASK)
    {
    case TIP_CLOCK_INTERNAL:
      printf("   Internal\n");
      break;

    case TIP_CLOCK_HFBR5:
      printf("   HFBR #5 Input\n");
      break;

    case TIP_CLOCK_HFBR1:
      printf("   HFBR #1 Input\n");
      break;

    case TIP_CLOCK_FP:
      printf("   Front Panel\n");
      break;

    default:
      printf("   UNDEFINED!\n");
    }

  if(tipTriggerSource&TIP_TRIGSRC_SOURCEMASK)
    {
      if(ro.trigsrc)
	printf(" Trigger input source (%s) =\n",
	       (ro.blockBuffer & TIP_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT)?"DISABLED on Block Limit":
	       "ENABLED");
      else
	printf(" Trigger input source (DISABLED) =\n");
      if(tipTriggerSource & TIP_TRIGSRC_P0)
	printf("   P0 Input\n");
      if(tipTriggerSource & TIP_TRIGSRC_HFBR1)
	printf("   HFBR #1 Input\n");
      if(tipTriggerSource & TIP_TRIGSRC_HFBR5)
	printf("   HFBR #5 Input\n");
      if(tipTriggerSource & TIP_TRIGSRC_LOOPBACK)
	printf("   Loopback\n");
      if(tipTriggerSource & TIP_TRIGSRC_FPTRG)
	printf("   Front Panel TRG\n");
      if(tipTriggerSource & TIP_TRIGSRC_VME)
	printf("   VME Command\n");
      if(tipTriggerSource & TIP_TRIGSRC_TSINPUTS)
	printf("   Front Panel TS Inputs\n");
      if(tipTriggerSource & TIP_TRIGSRC_TSREV2)
	printf("   Trigger Supervisor (rev2)\n");
      if(tipTriggerSource & TIP_TRIGSRC_PULSER)
	printf("   Internal Pulser\n");
      if(tipTriggerSource & TIP_TRIGSRC_PART_1)
	printf("   TS Partition 1 (HFBR #1)\n");
      if(tipTriggerSource & TIP_TRIGSRC_PART_2)
	printf("   TS Partition 2 (HFBR #1)\n");
      if(tipTriggerSource & TIP_TRIGSRC_PART_3)
	printf("   TS Partition 3 (HFBR #1)\n");
      if(tipTriggerSource & TIP_TRIGSRC_PART_4)
	printf("   TS Partition 4 (HFBR #1)\n");
    }
  else 
    {
      printf(" No Trigger input sources\n");
    }

  if(ro.tsInput & TIP_TSINPUT_MASK)
    {
      printf(" Front Panel TS Inputs Enabled: ");
      for(iinp=0; iinp<6; iinp++)
	{
	  if( (ro.tsInput & TIP_TSINPUT_MASK) & (1<<iinp)) 
	    printf(" %d",iinp+1);
	}
      printf("\n");	
    }
  else
    {
      printf(" All Front Panel TS Inputs Disabled\n");
    }

  if(ro.sync&TIP_SYNC_SOURCEMASK)
    {
      printf(" Sync source = \n");
      if(ro.sync & TIP_SYNC_P0)
	printf("   P0 Input\n");
      if(ro.sync & TIP_SYNC_HFBR1)
	printf("   HFBR #1 Input\n");
      if(ro.sync & TIP_SYNC_HFBR5)
	printf("   HFBR #5 Input\n");
      if(ro.sync & TIP_SYNC_FP)
	printf("   Front Panel Input\n");
      if(ro.sync & TIP_SYNC_LOOPBACK)
	printf("   Loopback\n");
      if(ro.sync & TIP_SYNC_USER_SYNCRESET_ENABLED)
	printf("   User SYNCRESET Receieve Enabled\n");
    }
  else
    {
      printf(" No SYNC input source configured\n");
    }

  if(ro.busy&TIP_BUSY_SOURCEMASK)
    {
      printf(" BUSY input source = \n");
      if(ro.busy & TIP_BUSY_SWA)
	printf("   Switch Slot A    %s\n",(ro.busy&TIP_BUSY_MONITOR_SWA)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_SWB)
	printf("   Switch Slot B    %s\n",(ro.busy&TIP_BUSY_MONITOR_SWB)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_P2)
	printf("   P2 Input         %s\n",(ro.busy&TIP_BUSY_MONITOR_P2)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_TRIGGER_LOCK)
	printf("   Trigger Lock     \n");
      if(ro.busy & TIP_BUSY_FP_FTDC)
	printf("   Front Panel TDC  %s\n",(ro.busy&TIP_BUSY_MONITOR_FP_FTDC)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_FP_FADC)
	printf("   Front Panel ADC  %s\n",(ro.busy&TIP_BUSY_MONITOR_FP_FADC)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_FP)
	printf("   Front Panel      %s\n",(ro.busy&TIP_BUSY_MONITOR_FP)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_LOOPBACK)
	printf("   Loopback         %s\n",(ro.busy&TIP_BUSY_MONITOR_LOOPBACK)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR1)
	printf("   HFBR #1          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR1)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR2)
	printf("   HFBR #2          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR2)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR3)
	printf("   HFBR #3          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR3)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR4)
	printf("   HFBR #4          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR4)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR5)
	printf("   HFBR #5          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR5)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR6)
	printf("   HFBR #6          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR6)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR7)
	printf("   HFBR #7          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR7)?"** BUSY **":"");
      if(ro.busy & TIP_BUSY_HFBR8)
	printf("   HFBR #8          %s\n",(ro.busy&TIP_BUSY_MONITOR_HFBR8)?"** BUSY **":"");
    }
  else
    {
      printf(" No BUSY input source configured\n");
    }

  if(ro.intsetup&TIP_INTSETUP_ENABLE)
    printf(" Interrupts ENABLED\n");
  else
    printf(" Interrupts DISABLED\n");
  printf("   Level = %d   Vector = 0x%02x\n",
	 (ro.intsetup&TIP_INTSETUP_LEVEL_MASK)>>8, (ro.intsetup&TIP_INTSETUP_VECTOR_MASK));
  
  printf(" Blocks ready for readout: %d\n",(ro.blockBuffer&TIP_BLOCKBUFFER_BLOCKS_READY_MASK)>>8);
  if(tipMaster)
    {
      printf(" Slave Block Status:   %s\n",
	     (ro.busy&TIP_BUSY_MONITOR_TRIG_LOST)?"** Waiting for Trigger Ack **":"");
      /* TI slave block status */
      fibermask = tipSlaveMask;
      for(ifiber=0; ifiber<8; ifiber++)
	{
	  if( fibermask & (1<<ifiber) )
	    {
	      if( (ifiber % 2) == 0)
		{
		  nblocksReady   = blockStatus[ifiber/2] & TIP_BLOCKSTATUS_NBLOCKS_READY0;
		  nblocksNeedAck = (blockStatus[ifiber/2] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;
		}
	      else
		{
		  nblocksReady   = (blockStatus[(ifiber-1)/2] & TIP_BLOCKSTATUS_NBLOCKS_READY1)>>16;
		  nblocksNeedAck = (blockStatus[(ifiber-1)/2] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
		}
	      printf("  Fiber %d  :  Blocks ready / need acknowledge: %d / %d\n",
		     ifiber+1,nblocksReady, nblocksNeedAck);
	    }
	}

      /* TI master block status */
      nblocksReady   = (blockStatus[4] & TIP_BLOCKSTATUS_NBLOCKS_READY1)>>16;
      nblocksNeedAck = (blockStatus[4] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
      printf("  Loopback :  Blocks ready / need acknowledge: %d / %d\n",
	     nblocksReady, nblocksNeedAck);

    }
  printf(" Input counter %d\n",ro.inputCounter);

  printf("--------------------------------------------------------------------------------\n");
  printf("\n\n");

}

/**
 * @ingroup SlaveConfig
 * @brief This routine provides the ability to switch the port that the TI Slave
 *     receives its Clock, SyncReset, and Trigger.
 *     If the TI has already been configured to use this port, nothing is done.
 *
 *   @param port
 *      -  1  - Port 1
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetSlavePort(int port)
{
 if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Slave.\n",__FUNCTION__);
      return ERROR;
    }

  if(port!=1)
    {
      printf("%s: ERROR: Invalid port specified (%d).  Must be 1 for TI Slave.\n",
	     __FUNCTION__,port);
      return ERROR;
    }

  /* Enable HFBR#1 */
  tipEnableFiber(1);
  /* HFBR#1 Clock Source */
  tipSetClockSource(1);
      /* HFBR#1 Sync Source */
  tipSetSyncSource(TIP_SYNC_HFBR1);
  /* HFBR#1 Trigger Source */
  tipSetTriggerSource(TIP_TRIGGER_HFBR1);

  /* Measure and apply fiber compensation */
  FiberMeas();
  
  /* TI IODELAY reset */
  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_IODELAY);
  usleep(10000);
  
  /* TI Sync auto alignment */
  tipWrite(&TIPp->reset,TIP_RESET_AUTOALIGN_HFBR1_SYNC);
  usleep(10000);
  
  /* TI auto fiber delay measurement */
  tipWrite(&TIPp->reset,TIP_RESET_MEASURE_LATENCY);
  usleep(10000);
  
  /* TI auto alignement fiber delay */
  tipWrite(&TIPp->reset,TIP_RESET_FIBER_AUTO_ALIGN);
  usleep(10000);
  TIPUNLOCK;

  printf("%s: INFO: TI Slave configured to use port %d.\n",
	 __FUNCTION__,port);
  return OK;
}

/**
 * @ingroup Status
 * @brief Print a summary of all fiber port connections to potential TI Slaves
 *
 * @param  pflag
 *   -  0  - Default output
 *   -  1  - Print Raw Registers
 *
 */

void
tipSlaveStatus(int pflag)
{
  int iport=0, ibs=0, ifiber=0;
  unsigned int TIBase;
  unsigned int hfbr_tiID[8] = {1,2,3,4,5,6,7};
  unsigned int master_tiID;
  unsigned int blockStatus[5];
  unsigned int fiber=0, busy=0, trigsrc=0;
  int nblocksReady=0, nblocksNeedAck=0, slaveCount=0;
  int blocklevel=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  TIPLOCK;
  for(iport=0; iport<1; iport++)
    {
      hfbr_tiID[iport] = tipRead(&TIPp->hfbr_tiID[iport]);
    }
  master_tiID = tipRead(&TIPp->master_tiID);
  fiber       = tipRead(&TIPp->fiber);
  busy        = tipRead(&TIPp->busy);
  trigsrc     = tipRead(&TIPp->trigsrc);
  for(ibs=0; ibs<4; ibs++)
    {
      blockStatus[ibs] = tipRead(&TIPp->blockStatus[ibs]);
    }
  blockStatus[4] = tipRead(&TIPp->adr24);

  blocklevel = (tipRead(&TIPp->blocklevel) & TIP_BLOCKLEVEL_CURRENT_MASK)>>16;

  TIPUNLOCK;

  TIBase = (unsigned long)TIPp;

  if(pflag>0)
    {
      printf(" Registers (offset):\n");
      /* printf("  TIBase     (0x%08x)\n",(unsigned int)(TIBase-tiA24Offset)); */
      printf("  busy           (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->busy) - TIBase, busy);
      printf("  fiber          (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->fiber) - TIBase, fiber);
      printf("  hfbr_tiID[0]   (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->hfbr_tiID[0]) - TIBase, hfbr_tiID[0]);
      printf("  hfbr_tiID[1]   (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->hfbr_tiID[1]) - TIBase, hfbr_tiID[1]);
      printf("  hfbr_tiID[2]   (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->hfbr_tiID[2]) - TIBase, hfbr_tiID[2]);
      printf("  hfbr_tiID[3]   (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->hfbr_tiID[3]) - TIBase, hfbr_tiID[3]);
      printf("  hfbr_tiID[4]   (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->hfbr_tiID[4]) - TIBase, hfbr_tiID[4]);
      printf("  hfbr_tiID[5]   (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->hfbr_tiID[5]) - TIBase, hfbr_tiID[5]);
      printf("  hfbr_tiID[6]   (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->hfbr_tiID[6]) - TIBase, hfbr_tiID[6]);
      printf("  hfbr_tiID[7]   (0x%04lx) = 0x%08x\n", (unsigned long)(&TIPp->hfbr_tiID[7]) - TIBase, hfbr_tiID[7]);
      printf("  master_tiID    (0x%04lx) = 0x%08x\t", (unsigned long)(&TIPp->master_tiID) - TIBase, master_tiID);

      printf("\n");
    }

  printf("TI-Master Port STATUS Summary\n");
  printf("                                                     Block Status\n");
  printf("Port  ROCID   Connected   TrigSrcEn   Busy Status   Ready / NeedAck  Blocklevel\n");
  printf("--------------------------------------------------------------------------------\n");
  /* Master first */
  /* Slot and Port number */
  printf("L     ");
  
  /* Port Name */
  printf("%5d      ",
	 (master_tiID&TIP_ID_CRATEID_MASK)>>8);
  
  /* Connection Status */
  printf("%s      %s       ",
	 "YES",
	 (trigsrc & TIP_TRIGSRC_LOOPBACK)?"ENABLED ":"DISABLED");
  
  /* Busy Status */
  printf("%s       ",
	 (busy & TIP_BUSY_MONITOR_LOOPBACK)?"BUSY":"    ");
  
  /* Block Status */
  nblocksReady   = (blockStatus[4] & TIP_BLOCKSTATUS_NBLOCKS_READY1)>>16;
  nblocksNeedAck = (blockStatus[4] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
  printf("  %3d / %3d",nblocksReady, nblocksNeedAck);
  printf("        %3d",blocklevel);
  printf("\n");

  /* Slaves last */
  for(iport=1; iport<2; iport++)
    {
      /* Only continue of this port has been configured as a slave */
      if((tipSlaveMask & (1<<(iport-1)))==0) continue;
      
      /* Slot and Port number */
      printf("%d     ", iport);

      /* Port Name */
      printf("%5d      ",
	     (hfbr_tiID[iport-1]&TIP_ID_CRATEID_MASK)>>8);
	  
      /* Connection Status */
      printf("%s      %s       ",
	     (fiber & TIP_FIBER_CONNECTED_TI(iport))?"YES":"NO ",
	     (fiber & TIP_FIBER_TRIGSRC_ENABLED_TI(iport))?"ENABLED ":"DISABLED");

      /* Busy Status */
      printf("%s       ",
	     (busy & TIP_BUSY_MONITOR_FIBER_BUSY(iport))?"BUSY":"    ");

      /* Block Status */
      ifiber=iport-1;
      if( (ifiber % 2) == 0)
	{
	  nblocksReady   = blockStatus[ifiber/2] & TIP_BLOCKSTATUS_NBLOCKS_READY0;
	  nblocksNeedAck = (blockStatus[ifiber/2] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;
	}
      else
	{
	  nblocksReady   = (blockStatus[(ifiber-1)/2] & TIP_BLOCKSTATUS_NBLOCKS_READY1)>>16;
	  nblocksNeedAck = (blockStatus[(ifiber-1)/2] & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
	}
      printf("  %3d / %3d",nblocksReady, nblocksNeedAck);

      printf("        %3d",(hfbr_tiID[iport-1]&TIP_ID_BLOCKLEVEL_MASK)>>16);

      printf("\n");
      slaveCount++;
    }
  printf("\n");
  printf("Total Slaves Added = %d\n",slaveCount);

}

/**
 * @ingroup Status
 * @brief Get the Firmware Version
 *
 * @return Firmware Version if successful, ERROR otherwise
 *
 */
int
tipGetFirmwareVersion()
{
  unsigned int rval=0;
  int delay=10000;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  /* reset the VME_to_JTAG engine logic */
  tipWrite(&TIPp->reset,TIP_RESET_JTAG);
  usleep(delay);

  /* Reset FPGA JTAG to "reset_idle" state */
  tipJTAGWrite(0x3c,0);
  usleep(delay);

  /* enable the user_code readback */
  tipJTAGWrite(0x26c,0x3c8);
  usleep(delay);

  /* shift in 32-bit to FPGA JTAG */
  tipJTAGWrite(0x7dc,0);
  usleep(delay);
  
  /* Readback the firmware version */
  rval = tipJTAGRead(0x00);
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the Module Serial Number
 *
 * @param rSN  Pointer to string to pass Serial Number
 *
 * @return SerialNumber if successful, ERROR otherwise
 *
 */
unsigned int
tipGetSerialNumber(char **rSN)
{
  unsigned int rval=0;
  char retSN[10];
  int delay=10000;

  memset(retSN,0,sizeof(retSN));
  
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_JTAG);           /* reset */
  usleep(delay);
  tipJTAGWrite(0x83c,0);     /* Reset_idle */
  usleep(delay);
  tipJTAGWrite(0xbec,0xFD); /* load the UserCode Enable */
  usleep(delay);
  tipJTAGWrite(0xfdc,0);   /* shift in 32-bit of data */
  usleep(delay);
  rval = tipJTAGRead(0x800);
  TIPUNLOCK;

  if(rSN!=NULL)
    {
      sprintf(retSN,"TI-%d",rval&0x7ff);
      strcpy((char *)rSN,retSN);
    }


  printf("%s: TI Serial Number is %s (0x%08x)\n", 
	 __FUNCTION__,retSN,rval);

  return rval;
  

}

int
tipPrintTempVolt()
{
  unsigned int rval=0;
  unsigned int Temperature;
  int delay=10000;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_JTAG);           /* reset */
  usleep(delay);

  tipJTAGWrite(0x3c,0x0); // Reset_idle 
  usleep(delay);

  tipJTAGWrite(0x26c,0x3f7); // load the UserCode Enable
  usleep(delay);
  
  tipJTAGWrite(0x7dc,0x04000000);     // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x1f1c);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04000000);     // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon temperature readout is %x \n", 
	 __FUNCTION__,rval);

  Temperature = 504*((rval >>6) & 0x3ff)/1024-273; 
  printf("\tThe temperature is : %d \n", Temperature);

  // maximum temperature readout
  tipJTAGWrite(0x7dc,0x04200000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04200000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf ("%s: FPGA silicon max. temperature readout is %x\n", 
	  __FUNCTION__,rval);
  Temperature = 504*((rval >>6) & 0x3ff)/1024-273; 
  printf("\tThe max. temperature is : %d \n", Temperature);

  // minimum temperature readout
  tipJTAGWrite(0x7dc,0x04240000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04240000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon min. temperature readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 504*((rval >>6) & 0x3ff)/1024-273; 
  printf ("\tThe min. temperature is : %d \n", Temperature);

  TIPUNLOCK;
  return OK;

  // VccInt readout
  tipJTAGWrite(0x7dc,0x04010000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04010000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon VccInt readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf ("\tThe VccInt is : %d mV \n", Temperature);

  // maximum VccInt readout
  tipJTAGWrite(0x7dc,0x04210000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04210000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);
  printf("%s: FPGA silicon Max. VccInt readout is %x\n", 
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf("\tThe Max. VccInt is : %d mV \n", Temperature);

  // minimum VccInt readout
  tipJTAGWrite(0x7dc,0x04250000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04250000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);
  printf("%s: FPGA silicon Min. VccInt readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf("\tThe Min. VccInt is : %d mV \n", Temperature);

  // VccAux readout
  tipJTAGWrite(0x7dc,0x04020000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04020000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon VccAux readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf("\tThe VccAux is : %d mV \n", Temperature);

  // maximum VccAux readout
  tipJTAGWrite(0x7dc,0x04220000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04220000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon Max. VccAux readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf("\tThe Max. VccAux is : %d mV \n", Temperature);

  // minimum VccAux readout
  tipJTAGWrite(0x7dc,0x04260000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  //second read is required to get the correct DRP value
  tipJTAGWrite(0x7dc,0x04260000);    // shift in 32-bit of data
  usleep(delay);
  rval = tipJTAGRead(0x7dc);

  printf("%s: FPGA silicon Min. VccAux readout is %x\n",
	 __FUNCTION__,rval);
  Temperature = 3000*((rval >>6) & 0x3ff)/1024; 
  printf("\tThe Min. VccAux is : %d mV \n", Temperature);

  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Resync the 250 MHz Clock
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipClockResync()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  
  TIPLOCK;
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_AD9510_RESYNC); 
  TIPUNLOCK;

  printf ("%s: \n\t AD9510 ReSync ! \n",__FUNCTION__);
  return OK;
  
}

/**
 * @ingroup Config
 * @brief Perform a soft reset of the TI
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipReset()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_SOFT);
  TIPUNLOCK;
  return OK;
}

/**
 * @ingroup Config
 * @brief Set the crate ID 
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetCrateID(unsigned int crateID)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(crateID>0xff)
    {
      printf("%s: ERROR: Invalid crate id (0x%x)\n",__FUNCTION__,crateID);
      return ERROR;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->boardID,
	   (tipRead(&TIPp->boardID) & ~TIP_BOARDID_CRATEID_MASK)  | crateID);
  TIPUNLOCK;

  return OK;
  
}

/**
 * @ingroup Status
 * @brief Get the crate ID of the selected port
 *
 * @param  port
 *       - 0 - Self
 *       - 1 - Fiber port 1 (If Master)
 *
 * @return port Crate ID if successful, ERROR otherwise
 *
 */
int
tipGetCrateID(int port)
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((port<0) || (port>1))
    {
      printf("%s: ERROR: Invalid port (%d)\n",
	     __FUNCTION__,port);
    }

  TIPLOCK;
  if(port==0)
    {
      rval = (tipRead(&TIPp->master_tiID) & TIP_ID_CRATEID_MASK)>>8;
    }
  else
    {
      rval = (tipRead(&TIPp->hfbr_tiID[port-1]) & TIP_ID_CRATEID_MASK)>>8;
    }
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the trigger sources enabled bits of the selected port
 *
 * @param  port
 *       - 0 - Self
 *       - 1-8 - Fiber port 1-8  (If Master)
 *
 * @return bitmask of rigger sources enabled if successful, otherwise ERROR
 *         bitmask
 *         - 0 - P0 
 *         - 1 - Fiber 1
 *         - 2 - Loopback
 *         - 3 - TRG (FP)
 *         - 4  - VME
 *         - 5 - TS Inputs (FP)
 *         - 6 - TS (rev 2)
 *         - 7 - Internal Pulser
 *
 */
int
tipGetPortTrigSrcEnabled(int port)
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((port<0) || (port>8))
    {
      printf("%s: ERROR: Invalid port (%d)\n",
	     __FUNCTION__,port);
    }

  TIPLOCK;
  if(port==0)
    {
      rval = (tipRead(&TIPp->master_tiID) & TIP_ID_TRIGSRC_ENABLE_MASK);
    }
  else
    {
      rval = (tipRead(&TIPp->hfbr_tiID[port-1]) & TIP_ID_TRIGSRC_ENABLE_MASK);
    }
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the blocklevel of the TI-Slave on the selected port
 * @param port
 *       - 1 - Fiber port 1
 *
 * @return port blocklevel if successful, ERROR otherwise
 *
 */
int
tipGetSlaveBlocklevel(int port)
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(port!=1)
    {
      printf("%s: ERROR: Invalid port (%d)\n",
	     __FUNCTION__,port);
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->hfbr_tiID[port-1]) & TIP_ID_BLOCKLEVEL_MASK)>>16;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup MasterConfig
 * @brief Set the number of events per block
 * @param blockLevel Number of events per block
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetBlockLevel(int blockLevel)
{
  return tipBroadcastNextBlockLevel(blockLevel);
}

/**
 * @ingroup MasterConfig
 * @brief Broadcast the next block level (to be changed at the end of
 * the next sync event, or during a call to tiSyncReset(1).
 *
 * @see tiSyncReset(1)
 * @param blockLevel block level to broadcats
 *
 * @return OK if successful, ERROR otherwise
 *
 */

int
tipBroadcastNextBlockLevel(int blockLevel)
{
  unsigned int trigger=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (blockLevel>TIP_BLOCKLEVEL_MASK) || (blockLevel==0) )
    {
      printf("%s: ERROR: Invalid Block Level (%d)\n",__FUNCTION__,blockLevel);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  trigger = tipRead(&TIPp->trigsrc);

  if(!(trigger & TIP_TRIGSRC_VME)) /* Turn on the VME trigger, if not enabled */
    tipWrite(&TIPp->trigsrc, TIP_TRIGSRC_VME | trigger);

  tipWrite(&TIPp->triggerCommand, TIP_TRIGGERCOMMAND_SET_BLOCKLEVEL | blockLevel);

  if(!(trigger & TIP_TRIGSRC_VME)) /* Turn off the VME trigger, if it was initially disabled */
    tipWrite(&TIPp->trigsrc, trigger);

  TIPUNLOCK;

  tipGetNextBlockLevel();

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the block level that will be updated on the end of the block readout.
 *
 * @return Next Block Level if successful, ERROR otherwise
 *
 */

int
tipGetNextBlockLevel()
{
  unsigned int reg_bl=0;
  int bl=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  reg_bl = tipRead(&TIPp->blocklevel);
  bl = (reg_bl & TIP_BLOCKLEVEL_RECEIVED_MASK)>>24;
  tipNextBlockLevel = bl;

  tipBlockLevel = (reg_bl & TIP_BLOCKLEVEL_CURRENT_MASK)>>16;
  TIPUNLOCK;

  return bl;
}

/**
 * @ingroup Status
 * @brief Get the current block level
 *
 * @return Next Block Level if successful, ERROR otherwise
 *
 */
int
tipGetCurrentBlockLevel()
{
  unsigned int reg_bl=0;
  int bl=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  reg_bl = tipRead(&TIPp->blocklevel);
  bl = (reg_bl & TIP_BLOCKLEVEL_CURRENT_MASK)>>16;
  tipBlockLevel = bl;
  tipNextBlockLevel = (reg_bl & TIP_BLOCKLEVEL_RECEIVED_MASK)>>24;
  TIPUNLOCK;

  return bl;
}

/**
 * @ingroup Config
 * @brief Set TS to instantly change blocklevel when broadcast is received.
 *
 * @param enable Option to enable or disable this feature
 *       - 0: Disable
 *        !0: Enable
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetInstantBlockLevelChange(int enable)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  if(enable)
    tipWrite(&TIPp->vmeControl, 
	       tipRead(&TIPp->vmeControl) | TIP_VMECONTROL_BLOCKLEVEL_UPDATE);
  else
    tipWrite(&TIPp->vmeControl, 
	       tipRead(&TIPp->vmeControl) & ~TIP_VMECONTROL_BLOCKLEVEL_UPDATE);
  TIPUNLOCK;
  
  return OK;
}

/**
 * @ingroup Status
 * @brief Get Status of instant blocklevel change when broadcast is received.
 *
 * @return 1 if enabled, 0 if disabled , ERROR otherwise
 *
 */
int
tipGetInstantBlockLevelChange()
{
  int rval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->vmeControl) & TIP_VMECONTROL_BLOCKLEVEL_UPDATE)>>21;
  TIPUNLOCK;
  
  return rval;
}

/**
 * @ingroup Config
 * @brief Set the trigger source
 *     This routine will set a library variable to be set in the TI registers
 *     at a call to tiIntEnable.  
 *
 *  @param trig - integer indicating the trigger source
 *         - 0: P0
 *         - 1: HFBR#1
 *         - 2: Front Panel (TRG)
 *         - 3: Front Panel TS Inputs
 *         - 4: TS (rev2) 
 *         - 5: Random
 *         - 6-9: TS Partition 1-4
 *         - 10: HFBR#5
 *         - 11: Pulser Trig 2 then Trig1 after specified delay 
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetTriggerSource(int trig)
{
  unsigned int trigenable=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (trig>10) || (trig<0) )
    {
      printf("%s: ERROR: Invalid Trigger Source (%d).  Must be between 0 and 10.\n",
	     __FUNCTION__,trig);
      return ERROR;
    }


  if(!tipMaster)
    { 
      /* Setup for TI Slave */
      trigenable = TIP_TRIGSRC_VME;

      if((trig>=6) && (trig<=9)) /* TS partition specified */
	{

	  trigenable |= TIP_TRIGSRC_HFBR1;
	  switch(trig)
	    {
	    case TIP_TRIGGER_PART_1:
	      trigenable |= TIP_TRIGSRC_PART_1;
	      break;
	  
	    case TIP_TRIGGER_PART_2:
	      trigenable |= TIP_TRIGSRC_PART_2;
	      break;
	  
	    case TIP_TRIGGER_PART_3:
	      trigenable |= TIP_TRIGSRC_PART_3;
	      break;

	    case TIP_TRIGGER_PART_4:
	      trigenable |= TIP_TRIGSRC_PART_4;
	      break;
	    }
	}
      else
	{
	  trigenable |= TIP_TRIGSRC_HFBR1;

	  if(trig != TIP_TRIGGER_HFBR1)
	    {
	      printf("%s: WARN:  Only valid trigger source for TI Slave is HFBR1 (%d)",
		     __FUNCTION__, TIP_TRIGGER_HFBR1);
	      printf("  Ignoring specified trig (%d)\n",trig);
	    }
	}

    }
  else
    {
      /* Setup for TI Master */

      /* Set VME and Loopback by default */
      trigenable  = TIP_TRIGSRC_VME;
      trigenable |= TIP_TRIGSRC_LOOPBACK;

      switch(trig)
	{
	case TIP_TRIGGER_P0:
	  trigenable |= TIP_TRIGSRC_P0;
	  break;

	case TIP_TRIGGER_HFBR1:
	  trigenable |= TIP_TRIGSRC_HFBR1;
	  break;

	case TIP_TRIGGER_FPTRG:
	  trigenable |= TIP_TRIGSRC_FPTRG;
	  break;

	case TIP_TRIGGER_TSINPUTS:
	  trigenable |= TIP_TRIGSRC_TSINPUTS;
	  break;

	case TIP_TRIGGER_TSREV2:
	  trigenable |= TIP_TRIGSRC_TSREV2;
	  break;

	case TIP_TRIGGER_PULSER:
	  trigenable |= TIP_TRIGSRC_PULSER;
	  break;

	case TIP_TRIGGER_TRIG21:
	  trigenable |= TIP_TRIGSRC_PULSER;
	  trigenable |= TIP_TRIGSRC_TRIG21;
	  break;

	default:
	  printf("%s: ERROR: Invalid Trigger Source (%d) for TI Master\n",
		 __FUNCTION__,trig);
	  return ERROR;
	}
    }

  tipTriggerSource = trigenable;
  printf("%s: INFO: tipTriggerSource = 0x%x\n",__FUNCTION__,tipTriggerSource);

  return OK;
}


/**
 * @ingroup Config
 * @brief Set trigger sources with specified trigmask
 *    This routine is for special use when tiSetTriggerSource(...) does
 *    not set all of the trigger sources that is required by the user.
 *
 * @param trigmask bits:  
 *        -         0:  P0
 *        -         1:  HFBR #1 
 *        -         2:  TI Master Loopback
 *        -         3:  Front Panel (TRG) Input
 *        -         4:  VME Trigger
 *        -         5:  Front Panel TS Inputs
 *        -         6:  TS (rev 2) Input
 *        -         7:  Random Trigger
 *        -         8:  FP/Ext/GTP 
 *        -         9:  P2 Busy 
 *        -        10:  HFBR #5
 *        -        11:  Pulser Trig2 with delayed Trig1 (only compatible with 2 and 7)
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetTriggerSourceMask(int trigmask)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  /* Check input mask */
  if(trigmask>TIP_TRIGSRC_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid trigger source mask (0x%x).\n",
	     __FUNCTION__,trigmask);
      return ERROR;
    }

  tipTriggerSource = trigmask;

  return OK;
}

/**
 * @ingroup Config
 * @brief Enable trigger sources
 * Enable trigger sources set by 
 *                          tiSetTriggerSource(...) or
 *                          tiSetTriggerSourceMask(...)
 * @sa tiSetTriggerSource
 * @sa tiSetTriggerSourceMask
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipEnableTriggerSource()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tipTriggerSource==0)
    {
      printf("%s: WARN: No Trigger Sources Enabled\n",__FUNCTION__);
    }

  TIPLOCK;
  tipWrite(&TIPp->trigsrc, tipTriggerSource);
  TIPUNLOCK;

  return OK;

}

/**
 * @ingroup Config
 * @brief Disable trigger sources
 *    
 * @param fflag 
 *   -  0: Disable Triggers
 *   - >0: Disable Triggers and generate enough triggers to fill the current block
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipDisableTriggerSource(int fflag)
{
  int regset=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;

  if(tipMaster)
    regset = TIP_TRIGSRC_LOOPBACK;

  tipWrite(&TIPp->trigsrc,regset);

  TIPUNLOCK;
  if(fflag && tipMaster)
    {
      tipFillToEndBlock();      
    }

  return OK;

}

/**
 * @ingroup Config
 * @brief Set the Sync source mask
 *
 * @param sync - MASK indicating the sync source
 *       bit: description
 *       -  0: P0
 *       -  1: HFBR1
 *       -  3: FP
 *       -  4: LOOPBACK
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetSyncSource(unsigned int sync)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(sync>TIP_SYNC_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid Sync Source Mask (%d).\n",
	     __FUNCTION__,sync);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->sync,sync);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the event format
 *
 * @param format - integer number indicating the event format
 *          - 0: 32 bit event number only
 *          - 1: 32 bit event number + 32 bit timestamp
 *          - 2: 32 bit event number + higher 16 bits of timestamp + higher 16 bits of eventnumber
 *          - 3: 32 bit event number + 32 bit timestamp
 *              + higher 16 bits of timestamp + higher 16 bits of eventnumber
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSetEventFormat(int format)
{
  unsigned int formatset=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (format>3) || (format<0) )
    {
      printf("%s: ERROR: Invalid Event Format (%d).  Must be between 0 and 3.\n",
	     __FUNCTION__,format);
      return ERROR;
    }

  TIPLOCK;

  switch(format)
    {
    case 0:
      break;

    case 1:
      formatset |= TIP_DATAFORMAT_TIMING_WORD;
      break;

    case 2:
      formatset |= TIP_DATAFORMAT_HIGHERBITS_WORD;
      break;

    case 3:
      formatset |= (TIP_DATAFORMAT_TIMING_WORD | TIP_DATAFORMAT_HIGHERBITS_WORD);
      break;

    }
 
  tipWrite(&TIPp->dataFormat,formatset);

  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Set and enable the "software" trigger
 *
 *  @param trigger  trigger type 1 or 2 (playback trigger)
 *  @param nevents  integer number of events to trigger
 *  @param period_inc  period multiplier, depends on range (0-0x7FFF)
 *  @param range  
 *     - 0: small period range (min: 120ns, increments of 120ns)
 *     - 1: large period range (min: 120ns, increments of 245.7us)
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tipSoftTrig(int trigger, unsigned int nevents, unsigned int period_inc, int range)
{
  unsigned int periodMax=(TIP_FIXEDPULSER1_PERIOD_MASK>>16);
  unsigned int reg=0;
  unsigned int time=0;

  if(TIPp==NULL)
    {
      printf("\ntiSoftTrig: ERROR: TI not initialized\n");
      return ERROR;
    }

  if(trigger!=1 && trigger!=2)
    {
      printf("\ntiSoftTrig: ERROR: Invalid trigger type %d\n",trigger);
      return ERROR;
    }

  if(nevents>TIP_FIXEDPULSER1_NTRIGGERS_MASK)
    {
      printf("\ntiSoftTrig: ERROR: nevents (%d) must be less than %d\n",nevents,
	     TIP_FIXEDPULSER1_NTRIGGERS_MASK);
      return ERROR;
    }
  if(period_inc>periodMax)
    {
      printf("\ntiSoftTrig: ERROR: period_inc (%d) must be less than %d ns\n",
	     period_inc,periodMax);
      return ERROR;
    }
  if( (range!=0) && (range!=1) )
    {
      printf("\ntiSoftTrig: ERROR: range (%d) must be 0 or 1\n",range);
      return ERROR;
    }

  if(range==0)
    time = 32+8*period_inc;
  if(range==1)
    time = 32+8*period_inc*2048;

  printf("\ntiSoftTrig: INFO: Setting software trigger for %d nevents with period of %.1f\n",
	 nevents,((float)time)/(1000.0));

  reg = (range<<31)| (period_inc<<16) | (nevents);
  TIPLOCK;
  if(trigger==1)
    {
      tipWrite(&TIPp->fixedPulser1, reg);
    }
  else if(trigger==2)
    {
      tipWrite(&TIPp->fixedPulser2, reg);
    }
  TIPUNLOCK;

  return OK;

}


/**
 * @ingroup MasterConfig
 * @brief Set the parameters of the random internal trigger
 *
 * @param trigger  - Trigger Selection
 *       -              1: trig1
 *       -              2: trig2
 * @param setting  - frequency prescale from 500MHz
 *
 * @sa tiDisableRandomTrigger
 * @return OK if successful, ERROR otherwise.
 *
 */
int
tipSetRandomTrigger(int trigger, int setting)
{
  double rate;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(trigger!=1 && trigger!=2)
    {
      printf("\ntiSetRandomTrigger: ERROR: Invalid trigger type %d\n",trigger);
      return ERROR;
    }

  if(setting>TIP_RANDOMPULSER_TRIG1_RATE_MASK)
    {
      printf("%s: ERROR: setting (0x%x) must be less than 0x%x\n",
	     __FUNCTION__,setting,TIP_RANDOMPULSER_TRIG1_RATE_MASK);
      return ERROR;
    }

  if(setting>0)
    rate = ((double)500000) / ((double) (2<<(setting-1)));
  else
    rate = ((double)500000);

  printf("%s: Enabling random trigger (trig%d) at rate (kHz) = %.2f\n",
	 __FUNCTION__,trigger,rate);

  TIPLOCK;
  if(trigger==1)
    tipWrite(&TIPp->randomPulser, 
	       setting | (setting<<4) | TIP_RANDOMPULSER_TRIG1_ENABLE);
  else if (trigger==2)
    tipWrite(&TIPp->randomPulser, 
	       (setting | (setting<<4))<<8 | TIP_RANDOMPULSER_TRIG2_ENABLE );

  TIPUNLOCK;

  return OK;
}


/**
 * @ingroup MasterConfig
 * @brief Disable random trigger generation
 * @sa tiSetRandomTrigger
 * @return OK if successful, ERROR otherwise.
 */
int
tipDisableRandomTrigger()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->randomPulser,0);
  TIPUNLOCK;
  return OK;
}

int tipTriedAgain=0;
/**
 * @ingroup Readout
 * @brief Read a block of events from the TI
 *
 * @param   data  - local memory address to place data
 * @param   nwrds - Max number of words to transfer
 * @param   rflag - Readout Flag
 *       -       0 - programmed I/O from the specified board
 *       -       1 - DMA transfer
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tipReadBlock(volatile unsigned int *data, int nwrds, int rflag)
{
  int ii;
  int dCnt, wCnt=0;
  volatile unsigned int val;
  static int bump = 0;
  int blocksize = 0x1000;
  unsigned long newaddr=0;

  if(TIPp==NULL)
    {
      printf("\n%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(data==NULL) 
    {
      printf("\n%s: ERROR: Invalid Destination address\n",__FUNCTION__);
      return(ERROR);
    }

  /* For now, ignore routine argument and go with how the library was configured
     prior to calling here */
  rflag = tipUseDma;

  tipTriedAgain=0;

  TIPLOCK;
  if(rflag >= 1)
    { /* Block transfer */

      dCnt = 0;
      ii=0;

      if(TIPpd==NULL)
	TIPpd = (volatile unsigned int*)tipMapInfo.map_addr;

      val = (volatile unsigned int)*TIPpd;
#ifdef DEBUGDMA
      printf("%s: TIPpd = 0x%lx \n",__FUNCTION__,(long unsigned int)TIPpd);
      printf("%s: 0x10 = 0x%08x\n",
	     __FUNCTION__,tipRead(&TIPp->__adr32));
      
      printf("%s: val = 0x%08x\n",
	     __FUNCTION__,val);
#endif

      int counter=0, ncount=1000;
      while(val==0)
	{
	  val = (volatile unsigned int)*TIPpd;
	  usleep(10);
	  counter++;
	  if(counter==ncount)
	    break;
	}

      if(counter==ncount)
	{
	  printf("%s: counter = %d.. give up\n",__FUNCTION__,counter);
	  /* TIPpd = (volatile unsigned int*)tipMapInfo.map_addr; */

	  printf("Dump the memory\n");
	  int imem=0, wcount=0;
	  for(imem=0; imem<(0x1000)/4; imem++)
	    {
	      val = *TIPpd++;
	      if(val!=0)
		{
		  printf("  0x%08x ",val);
		  wcount++;
		}
	      if(((imem-7)%8)==0) {
		if(wcount) 
		  printf("[%04x]\n",(imem-7)<<2);

		wcount=0;
	      }
	    }
	  printf("\n");
	  tipMemCount--;
	  TIPUNLOCK;
	  return -1;
	}

      /* First word should be the 64bit word count from PCIE */
      wCnt = ((val>>16)&0xFFF)*2;

      

      if(wCnt<nwrds)
      	nwrds=(wCnt);
      else
      	printf("%s: WARN: Data words (%d) > requested max (%d)   0x%08x\n",
      	       __FUNCTION__,wCnt,nwrds,val);

#ifdef SKIPTHIS
      if(val==0)
	{
	  /* Chase until we find a non-zero value */
	  unsigned long chase = tipMapInfo.map_addr + 0x100000;
	  int ichase=0;
	  TIPpd = (volatile unsigned int*)tipMapInfo.map_addr;
	  while((unsigned long)TIPpd < chase)
	    {
	      if(ichase==0)
		{
		  printf("starting the chase\n");
		}
	      ichase++;

	      val = (volatile unsigned int)*TIPpd;
	      if(val != 0)
		{
		  printf("%s: 0x%lx: Found 0x%08x\n",
			 __FUNCTION__,
			 (unsigned long)((unsigned long)TIPpd - tipMapInfo.map_addr),
			 *TIPpd);
		}
	      *TIPpd++;
	    }
	  printf("ichase = %d\n",ichase);
	  printf("%s: TIPpd = 0x%lx \n",__FUNCTION__,(unsigned long)TIPpd);

	  /* if(((unsigned long)TIPpd - tipMapInfo.map_addr)>=0x100000) */
	  TIPpd = (volatile unsigned int*)tipMapInfo.map_addr;

	  bump=0;
	  TIPUNLOCK;
	  return dCnt;
	}
#endif
      
      *TIPpd++;      
      while(ii<nwrds) 
	{
	  val = (volatile unsigned int)*TIPpd++;

	  if(val == (TIP_DATA_TYPE_DEFINE_MASK | TIP_BLOCK_TRAILER_WORD_TYPE
	  	     | (ii)) )
	    {
	      data[ii] = val;
/* #define READOUTFILLER */
#ifdef READOUTFILLER
	      if(((ii)%2)!=0)
	      	{
	      	  /* Read out an extra word (filler) in the fifo */
	      	  val = *TIPpd++;
	      	  if(((val & TIP_DATA_TYPE_DEFINE_MASK) != TIP_DATA_TYPE_DEFINE_MASK) ||
	      	     ((val & TIP_WORD_TYPE_MASK) != TIP_FILLER_WORD_TYPE))
	      	    {
	      	      printf("\n%s: ERROR: Unexpected word after block trailer (0x%08x)\n",
	      		     __FUNCTION__,val);
	      	    }
	      	}
#endif
	      break;
	    }
	  data[ii] = val;
	  ii++;
	}
      /* ii++; */
      dCnt += ii;

	  /* printf("Dump the memory\n"); */
	  /* int imem=0, wcount=0; */
	  /* for(imem=0; imem<(0x1000)/4; imem++) */
	  /*   { */
	  /*     val = *TIPpd++; */
	  /*     if(val!=0) */
	  /* 	{ */
	  /* 	  printf("  0x%08x ",val); */
	  /* 	  wcount++; */
	  /* 	} */
	  /*     if(((imem-7)%8)==0) { */
	  /* 	if(wcount)  */
	  /* 	  printf("[%04x]\n",(imem-7)<<2); */

	  /* 	wcount=0; */
	  /*     } */
	  /*   } */
	  /* printf("\n"); */

      newaddr = (tipMapInfo.map_addr + (blocksize*(++bump)));
      if((newaddr - tipMapInfo.map_addr)>=0x100000)
	{
	  newaddr = tipMapInfo.map_addr;
	  bump=0;
	}
					   
      /* TIPpd = (unsigned int *)newaddr; */
      TIPpd = (volatile unsigned int*)newaddr;

      if(dCnt==0)
	{
	  TIPpd = (volatile unsigned int*)tipMapInfo.map_addr;
	  bump--;
	}
      else
	tipMemCount--;

#ifdef DEBUGDMA
      printf("%s: TIPpd = 0x%lx  bump = 0x%x (%d)  counter = %d\n",
	     __FUNCTION__,(long unsigned int)TIPpd,
	     bump,bump,counter);
#endif

      TIPUNLOCK;
      return(dCnt);
    }
  else
    { /* Programmed IO */
      int blocklevel=0;
      int iev=0, idata=0;
      int ev_nwords=0;
      dCnt = 0;
      ii=0;
      
      /* Read Block header - should be first word */
      val = tipRead(&TIPp->fifo);
      data[dCnt++] = val;

      if((val & TIP_DATA_TYPE_DEFINE_MASK) && 
	 ((val & TIP_WORD_TYPE_MASK) == TIP_BLOCK_HEADER_WORD_TYPE))
	{
	  /* data[dCnt++] = val; */
	}
      else
	{
	  printf("%s: ERROR: Invalid Block Header Word 0x%08x\n",
		 __FUNCTION__,val);
	  TIPUNLOCK;
	  return(dCnt);
	}

      /* Read trigger bank header - 2nd word */
      val = tipRead(&TIPp->fifo);

      if( ((val & 0xFF100000)>>16 == 0xFF10) && 
	  ((val & 0xFF00)>>8 == 0x20) )
	{
	  data[dCnt++] = val;
	}
      else
	{
	  printf("%s: ERROR: Invalid Trigger Bank Header Word 0x%08x\n",
		 __FUNCTION__,val);
	  TIPUNLOCK;
	  return(ERROR);
	}

      blocklevel = val & TIP_DATA_BLKLEVEL_MASK;

      /* Loop through each event in the block */
      for(iev=0; iev<blocklevel; iev++)
	{
	  /* Event header */
	TRYAGAIN:
	  val = tipRead(&TIPp->fifo);

	  if((val & 0xFF0000)>>16 == 0x01)
	    {
	      data[dCnt++] = val;
	      ev_nwords = val & 0xffff;
	      for(idata=0; idata<ev_nwords; idata++)
		{
		  val = tipRead(&TIPp->fifo);
		  data[dCnt++] = val;
		}
	    }
	  else
	    {
	      tipTriedAgain++;
	      usleep(1);
	      if(tipTriedAgain>20)
		{
		  printf("%s: ERROR: Invalid Event Header Word 0x%08x\n",
			 __FUNCTION__,val);
		  TIPUNLOCK;
		  return -1;
		}
	      goto TRYAGAIN;
	    }
	}

      /* Read Block header */
      val = tipRead(&TIPp->fifo);

      if((val & TIP_DATA_TYPE_DEFINE_MASK) &&
	 (val & TIP_WORD_TYPE_MASK)==TIP_BLOCK_TRAILER_WORD_TYPE)
	{
	  data[dCnt++] = val;
	}
      else
	{
	  printf("%s: ERROR: Invalid Block Trailer Word 0x%08x\n",
		 __FUNCTION__,val);
	  TIPUNLOCK;
	  return ERROR;
	}

      /* Readout an extra filler word, if we need an even word count */
      if((dCnt%2)!=0)
	{
	  val = tipRead(&TIPp->fifo);
	  if((val & TIP_DATA_TYPE_DEFINE_MASK) &&
	     (val & TIP_WORD_TYPE_MASK)==TIP_FILLER_WORD_TYPE)
	    {
	      data[dCnt++] = val;
	    }
	  else
	    {
	      printf("%s: ERROR: Invalid Filler Word 0x%08x\n",
		     __FUNCTION__,val);
	      TIPUNLOCK;
	      return ERROR;
	    }
	}

      TIPUNLOCK;
      return(dCnt);
    }

  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Read a block from the TI and form it into a CODA Trigger Bank
 *
 * @param   data  - local memory address to place data
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tipReadTriggerBlock(volatile unsigned int *data)
{
  int rval=0, nwrds=0, rflag=0;
  int iword=0;
  unsigned int word=0;
  int iblkhead=-1, iblktrl=-1;


  if(data==NULL) 
    {
      printf("\n%s: ERROR: Invalid Destination address\n",
	     __FUNCTION__);
      return(ERROR);
    }

  /* Determine the maximum number of words to expect, from the block level */
  nwrds = (4*tipBlockLevel) + 8;

  /* Optimize the transfer type based on the blocklevel */
  if(tipBlockLevel>2)
    { /* Use DMA */
      rflag = 1;
    }
  else
    { /* Use programmed I/O (Single cycle reads) */
      rflag = 0;
    }

  /* Obtain the trigger bank by just making a call the tiReadBlock */
  rval = tipReadBlock(data, nwrds, rflag);
  if(rval < 0)
    {
      /* Error occurred */
      printf("%s: ERROR: tiReadBlock returned ERROR\n",
	     __FUNCTION__);
      return ERROR;
    }
  else if (rval == 0)
    {
      /* No data returned */
      printf("%s: WARN: No data available\n",
	     __FUNCTION__);
      return 0; 
    }

  /* Work down to find index of block header */
  while(iword<rval)
    { 

      word = data[iword];

      if(word & TIP_DATA_TYPE_DEFINE_MASK)
	{
	  if(((word & TIP_WORD_TYPE_MASK)) == TIP_BLOCK_HEADER_WORD_TYPE)
	    {
	      iblkhead = iword;
	      break;
	    }
	}     
      iword++;
    }

  /* Check if the index is valid */
  if(iblkhead == -1)
    {
      printf("%s: ERROR: Failed to find TI Block Header\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(iblkhead != 0)
    {
      printf("%s: WARN: Invalid index (%d) for the TI Block header.\n",
	     __FUNCTION__,
	     iblkhead);
    }

  /* Work up to find index of block trailer */
  iword=rval-1;
  while(iword>=0)
    { 

      word = data[iword];
      if(word & TIP_DATA_TYPE_DEFINE_MASK)
	{
	  if(((word & TIP_WORD_TYPE_MASK)) == TIP_BLOCK_TRAILER_WORD_TYPE)
	    {
#ifdef CDEBUG
	      printf("%s: block trailer? 0x%08x\n",
		     __FUNCTION__,word);
#endif
	      iblktrl = iword;
	      break;
	    }
	}     
      iword--;
    }

  /* Check if the index is valid */
  if(iblktrl == -1)
    {
      printf("%s: ERROR: Failed to find TI Block Trailer\n",
	     __FUNCTION__);
      return ERROR;
    }

  /* Get the block trailer, and check the number of words contained in it */
  word = data[iblktrl];
  if((iblktrl - iblkhead + 1) != (word & 0x3fffff))
    {
      printf("%s: Number of words inconsistent (index count = %d, block trailer count = %d\n",
	     __FUNCTION__,
	     (iblktrl - iblkhead + 1), word & 0x3fffff);
      return ERROR;
    }

  /* Modify the total words returned */
  rval = iblktrl - iblkhead;

  /* Write in the Trigger Bank Length */
  data[iblkhead] = rval-1;

  return rval;

}


int
tipCheckTriggerBlock(volatile unsigned int *data)
{
  unsigned int blen=0, blevel=0, evlen=0;
  int iword=0, iev=0, ievword=0;
  int rval=OK;

  printf("--------------------------------------------------------------------------------\n");
  /* First word should be the trigger bank length */
  blen = data[iword];
  printf("%4d: %08X - TRIGGER BANK LENGTH - len = %d\n",iword, data[iword], blen);
  iword++;

  /* Trigger Bank Header */
  if( ((data[iword] & 0xFF100000)>>16 != 0xFF10) ||
      ((data[iword] & 0x0000FF00)>>8 != 0x20) )
    {
      rval = ERROR;
      printf("%4d: %08X - **** INVALID TRIGGER BANK HEADER ****\n",
	     iword, 
	     data[iword]);
      iword++;
      while(iword<blen+1)
	{
	  if(iword>blen)
	    {
	      rval = ERROR;
	      printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
	    }
	  printf("%4d: %08X - **** REST OF DATA ****\n",
		 iword,
		 data[iword]);
	  iword++;
	}
    }
  else
    {
      if(iword>blen)
	{
	  rval = ERROR;
	  printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
	}
      blevel = data[iword] & 0xFF;
      printf("%4d: %08X - TRIGGER BANK HEADER - type = %d  blocklevel = %d\n",
	     iword, 
	     data[iword],
	     (data[iword] & 0x000F0000)>>16, 
	     blevel);
      iword++;

      for(iev=0; iev<blevel; iev++)
	{
	  if(iword>blen)
	    {
	      rval = ERROR;
	      printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
	    }
	  
	  if((data[iword] & 0x00FF0000)>>16!=0x01)
	    {
	      rval = ERROR;
	      printf("%4d: %08x - **** INVALID EVENT HEADER ****\n",
		     iword, data[iword]);
	      iword++;
	      while(iword<blen+1)
		{
		  printf("%4d: %08X - **** REST OF DATA ****\n",
			 iword,
			 data[iword]);
		  iword++;
		}
	      break;
	    }
	  else
	    {
	      if(iword>blen)
		{
		  rval = ERROR;
		  printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
		}
	      
	      evlen = data[iword] & 0x0000FFFF;
	      printf("%4d: %08x - EVENT HEADER - trigtype = %d  len = %d\n",
		     iword,
		     data[iword],
		     (data[iword] & 0xFF000000)>>24,
		     evlen);
	      iword++;

	      if(iword>blen)
		{
		  rval = ERROR;
		  printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
		}

	      printf("%4d: %08x - EVENT NUMBER - evnum = %d\n",
		     iword,
		     data[iword],
		     data[iword]);
	      iword++;
	      for(ievword=1; ievword<evlen; ievword++)
		{
		  if(iword>blen)
		    {
		      rval = ERROR;
		      printf("----: **** ERROR: Data continues past Trigger Bank Length (%d) ****\n",blen);
		    }
		  printf("%4d: %08X - EVENT DATA\n",
			 iword,
			 data[iword]);
		  iword++;
		}
	    }
	}
    }

  printf("--------------------------------------------------------------------------------\n");
  return rval;
}

/**
 * @ingroup Config
 * @brief Enable Fiber transceiver
 *
 *  Note:  All Fiber are enabled by default 
 *         (no harm, except for 1-2W power usage)
 *
 * @sa tiDisableFiber
 * @param   fiber: integer indicative of the transceiver to enable
 *
 *
 * @return OK if successful, ERROR otherwise.
 *
 */
int
tipEnableFiber(unsigned int fiber)
{
  unsigned int sval;
  unsigned int fiberbit;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fiber<1) | (fiber>8))
    {
      printf("%s: ERROR: Invalid value for fiber (%d)\n",
	     __FUNCTION__,fiber);
      return ERROR;
    }

  fiberbit = (1<<(fiber-1));

  TIPLOCK;
  sval = tipRead(&TIPp->fiber);
  tipWrite(&TIPp->fiber,
	     sval | fiberbit );
  TIPUNLOCK;

  return OK;
  
}

/**
 * @ingroup Config
 * @brief Disable Fiber transceiver
 *
 * @sa tiEnableFiber
 *
 * @param   fiber: integer indicative of the transceiver to disable
 *
 *
 * @return OK if successful, ERROR otherwise.
 *
 */
int
tipDisableFiber(unsigned int fiber)
{
  unsigned int rval;
  unsigned int fiberbit;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fiber<1) | (fiber>8))
    {
      printf("%s: ERROR: Invalid value for fiber (%d)\n",
	     __FUNCTION__,fiber);
      return ERROR;
    }

  fiberbit = (1<<(fiber-1));

  TIPLOCK;
  rval = tipRead(&TIPp->fiber);
  tipWrite(&TIPp->fiber,
	   rval & ~fiberbit );
  TIPUNLOCK;

  return rval;
  
}

/**
 * @ingroup Config
 * @brief Set the busy source with a given sourcemask sourcemask bits: 
 *
 * @param sourcemask 
 *  - 0: SWA
 *  - 1: SWB
 *  - 2: P2
 *  - 3: FP-FTDC
 *  - 4: FP-FADC
 *  - 5: FP
 *  - 6: Unused
 *  - 7: Loopack
 *  - 8-15: Fiber 1-8
 *
 * @param rFlag - decision to reset the global source flags
 *       -      0: Keep prior busy source settings and set new "sourcemask"
 *       -      1: Reset, using only that specified with "sourcemask"
 * @return OK if successful, ERROR otherwise.
 */
int
tipSetBusySource(unsigned int sourcemask, int rFlag)
{
  unsigned int busybits=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if(sourcemask>TIP_BUSY_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid value for sourcemask (0x%x)\n",
	     __FUNCTION__, sourcemask);
      return ERROR;
    }

  TIPLOCK;
  if(rFlag)
    {
      /* Read in the previous value , resetting previous BUSYs*/
      busybits = tipRead(&TIPp->busy) & ~(TIP_BUSY_SOURCEMASK);
    }
  else
    {
      /* Read in the previous value , keeping previous BUSYs*/
      busybits = tipRead(&TIPp->busy);
    }

  busybits |= sourcemask;

  tipWrite(&TIPp->busy, busybits);
  TIPUNLOCK;

  return OK;

}


/**
 *  @ingroup MasterConfig
 *  @brief Set the the trigger lock mode.
 *
 *  @param enable Enable flag
 *      0: Disable
 *     !0: Enable
 *
 * @return OK if successful, ERROR otherwise.
 */
int
tipSetTriggerLock(int enable)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  if(enable)
    tipWrite(&TIPp->busy,
	       tipRead(&TIPp->busy) | TIP_BUSY_TRIGGER_LOCK);
  else
    tipWrite(&TIPp->busy,
	       tipRead(&TIPp->busy) & ~TIP_BUSY_TRIGGER_LOCK);
  TIPUNLOCK;

  return OK;
}

/**
 *  @ingroup MasterStatus
 *  @brief Get the current setting of the trigger lock mode.
 *
 * @return 1 if enabled, 0 if disabled, ERROR otherwise.
*/
int
tipGetTriggerLock()
{
  int rval=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->busy) & TIP_BUSY_TRIGGER_LOCK)>>6;
  TIPUNLOCK;

  return rval;
}


/**
 *  @ingroup MasterConfig
 *  @brief Set the prescale factor for the external trigger
 *
 *  @param   prescale Factor for prescale.  
 *               Max {prescale} available is 65535
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
tipSetPrescale(int prescale)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(prescale<0 || prescale>0xffff)
    {
      printf("%s: ERROR: Invalid prescale (%d).  Must be between 0 and 65535.",
	     __FUNCTION__,prescale);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->trig1Prescale, prescale);
  TIPUNLOCK;

  return OK;
}


/**
 *  @ingroup Status
 *  @brief Get the current prescale factor
 *  @return Current prescale factor, otherwise ERROR.
 */
int
tipGetPrescale()
{
  int rval;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->trig1Prescale);
  TIPUNLOCK;

  return rval;
}

/**
 *  @ingroup MasterConfig
 *  @brief Set the prescale factor for the selected input
 *
 *  @param   input Selected trigger input (1-6)
 *  @param   prescale Factor for prescale.  
 *               Max {prescale} available is 65535
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
tipSetInputPrescale(int input, int prescale)
{
  unsigned int oldval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((prescale<0) || (prescale>0xf))
    {
      printf("%s: ERROR: Invalid prescale (%d).  Must be between 0 and 15.",
	     __FUNCTION__,prescale);
      return ERROR;
    }

  if((input<1) || (input>6))
    {
    {
      printf("%s: ERROR: Invalid input (%d).",
	     __FUNCTION__,input);
      return ERROR;
    }
    }

  TIPLOCK;
  oldval = tipRead(&TIPp->inputPrescale) & ~(TIP_INPUTPRESCALE_FP_MASK(input));
  tipWrite(&TIPp->inputPrescale, oldval | (prescale<<(4*(input-1) )) );
  TIPUNLOCK;

  return OK;
}


/**
 *  @ingroup Status
 *  @brief Get the current prescale factor for the selected input
 *  @param   input Selected trigger input (1-6)
 *  @return Current prescale factor, otherwise ERROR.
 */
int
tipGetInputPrescale(int input)
{
  int rval;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->inputPrescale) & TIP_INPUTPRESCALE_FP_MASK(input);
  rval = rval>>(4*(input-1));
  TIPUNLOCK;

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set the characteristics of a specified trigger
 *
 *  @param trigger
 *           - 1: set for trigger 1
 *           - 2: set for trigger 2 (playback trigger)
 *  @param delay    delay in units of delay_step
 *  @param width    pulse width in units of 4ns
 *  @param delay_step step size of the delay
 *         - 0: 16ns
 *          !0: 64ns (with an offset of ~4.1 us)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetTriggerPulse(int trigger, int delay, int width, int delay_step)
{
  unsigned int rval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(trigger<1 || trigger>2)
    {
      printf("%s: ERROR: Invalid trigger (%d).  Must be 1 or 2.\n",
	     __FUNCTION__,trigger);
      return ERROR;
    }
  if(delay<0 || delay>0x7F)
    {
      printf("%s: ERROR: Invalid delay (%d).  Must be less than %d\n",
	     __FUNCTION__,delay,TIP_TRIGDELAY_TRIG1_DELAY_MASK);
      return ERROR;
    }
  if(width<0 || width>TIP_TRIGDELAY_TRIG1_WIDTH_MASK)
    {
      printf("%s: ERROR: Invalid width (%d).  Must be less than %d\n",
	     __FUNCTION__,width,TIP_TRIGDELAY_TRIG1_WIDTH_MASK);
    }


  TIPLOCK;
  if(trigger==1)
    {
      rval = tipRead(&TIPp->trigDelay) & 
	~(TIP_TRIGDELAY_TRIG1_DELAY_MASK | TIP_TRIGDELAY_TRIG1_WIDTH_MASK) ;
      rval |= ( (delay) | (width<<8) );
      if(delay_step)
	rval |= TIP_TRIGDELAY_TRIG1_64NS_STEP;

      tipWrite(&TIPp->trigDelay, rval);
    }
  if(trigger==2)
    {
      rval = tipRead(&TIPp->trigDelay) & 
	~(TIP_TRIGDELAY_TRIG2_DELAY_MASK | TIP_TRIGDELAY_TRIG2_WIDTH_MASK) ;
      rval |= ( (delay<<16) | (width<<24) );
      if(delay_step)
	rval |= TIP_TRIGDELAY_TRIG2_64NS_STEP;

      tipWrite(&TIPp->trigDelay, rval);
    }
  TIPUNLOCK;
  
  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the width of the prompt trigger from OT#2
 *
 *  @param width Output width will be set to (width + 2) * 4ns
 *
 *    This routine is only functional for Firmware type=2 (modTI)
 *
 *  @return OK if successful, otherwise ERROR
 */
int
tipSetPromptTriggerWidth(int width)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((width<0) || (width>TIP_PROMPT_TRIG_WIDTH_MASK))
    {
      printf("%s: ERROR: Invalid prompt trigger width (%d)\n",
	     __FUNCTION__,width);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->eventNumber_hi, width);
  TIPUNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Get the width of the prompt trigger from OT#2
 *
 *    This routine is only functional for Firmware type=2 (modTI)
 *
 *  @return Output width set to (return value + 2) * 4ns, if successful. Otherwise ERROR
 */
int
tipGetPromptTriggerWidth()
{
  unsigned int rval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->eventNumber_hi) & TIP_PROMPT_TRIG_WIDTH_MASK;
  TIPUNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the delay time and width of the Sync signal
 *
 * @param delay  the delay (latency) set in units of 4ns.
 * @param width  the width set in units of 4ns.
 * @param twidth  if this is non-zero, set width in units of 32ns.
 *
 */
void
tipSetSyncDelayWidth(unsigned int delay, unsigned int width, int widthstep)
{
  int twidth=0, tdelay=0;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  if(delay>TIP_SYNCDELAY_MASK)
    {
      printf("%s: ERROR: Invalid delay (%d)\n",__FUNCTION__,delay);
      return;
    }

  if(width>TIP_SYNCWIDTH_MASK)
    {
      printf("%s: WARN: Invalid width (%d).\n",__FUNCTION__,width);
      return;
    }

  if(widthstep)
    width |= TIP_SYNCWIDTH_LONGWIDTH_ENABLE;

  tdelay = delay*4;
  if(widthstep)
    twidth = (width&TIP_SYNCWIDTH_MASK)*32;
  else
    twidth = width*4;

  printf("%s: Setting Sync delay = %d (ns)   width = %d (ns)\n",
	 __FUNCTION__,tdelay,twidth);

  TIPLOCK;
  tipWrite(&TIPp->syncDelay,delay);
  tipWrite(&TIPp->syncWidth,width);
  TIPUNLOCK;

}

/**
 * @ingroup MasterConfig
 * @brief Reset the trigger link.
 */
void 
tipTrigLinkReset()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_TRIGGERLINK_DISABLE); 
  usleep(10000);

  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_TRIGGERLINK_DISABLE); 
  usleep(10000);

  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_TRIGGERLINK_ENABLE);
  usleep(10000);

  TIPUNLOCK;

  printf ("%s: Trigger Data Link was reset.\n",__FUNCTION__);
}

/**
 * @ingroup MasterConfig
 * @brief Set type of SyncReset to send to TI Slaves
 *
 * @param type Sync Reset Type
 *    - 0: User programmed width in each TI
 *    - !0: Fixed 4 microsecond width in each TI
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetSyncResetType(int type)
{

  if(type)
    tipSyncResetType=TIP_SYNCCOMMAND_SYNCRESET_4US;
  else
    tipSyncResetType=TIP_SYNCCOMMAND_SYNCRESET;

  return OK;
}


/**
 * @ingroup MasterConfig
 * @brief Generate a Sync Reset signal.  This signal is sent to the loopback and
 *    all configured TI Slaves.
 *
 *  @param blflag Option to change block level, after SyncReset issued
 *       -   0: Do not change block level
 *       -  >0: Broadcast block level to all connected slaves (including self)
 *            BlockLevel broadcasted will be set to library value
 *            (Set with tiSetBlockLevel)
 *
 */
void
tipSyncReset(int blflag)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->syncCommand,tipSyncResetType); 
  usleep(10000);
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_RESET_EVNUM); 
  usleep(10000);
  TIPUNLOCK;
  
  if(blflag) /* Set the block level from "Next" to Current */
    {
      printf("%s: INFO: Setting Block Level to %d\n",
	     __FUNCTION__,tipNextBlockLevel);
      tipBroadcastNextBlockLevel(tipNextBlockLevel);
    }

}

/**
 * @ingroup MasterConfig
 * @brief Generate a Sync Reset Resync signal.  This signal is sent to the loopback and
 *    all configured TI Slaves.  This type of Sync Reset will NOT reset 
 *    event numbers
 *
 */
void
tipSyncResetResync()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  TIPLOCK;
  tipWrite(&TIPp->syncCommand,tipSyncResetType); 
  TIPUNLOCK;

}

/**
 * @ingroup MasterConfig
 * @brief Generate a Clock Reset signal.  This signal is sent to the loopback and
 *    all configured TI Slaves.
 *
 */
void
tipClockReset()
{
  unsigned int old_syncsrc=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  if(tipMaster!=1)
    {
      printf("%s: ERROR: TI is not the Master.  No Clock Reset.\n", __FUNCTION__);
      return;
    }
  
  TIPLOCK;
  /* Send a clock reset */
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_CLK250_RESYNC); 
  usleep(20000);

  /* Store the old sync source */
  old_syncsrc = tipRead(&TIPp->sync) & TIP_SYNC_SOURCEMASK;
  /* Disable sync source */
  tipWrite(&TIPp->sync, 0);
  usleep(20000);

  /* Send another clock reset */
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_CLK250_RESYNC); 
  usleep(20000);

  /* Re-enable the sync source */
  tipWrite(&TIPp->sync, old_syncsrc);
  TIPUNLOCK;

}


/**
 * @ingroup Config
 * @brief Reset the L1A counter, as incremented by the TI.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipResetEventCounter()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_SCALERS_RESET);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Returns the event counter (48 bit)
 *
 * @return Number of accepted events if successful, otherwise ERROR
 */
unsigned long long int
tipGetEventCounter()
{
  unsigned long long int rval=0;
  unsigned int lo=0, hi=0;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  lo = tipRead(&TIPp->eventNumber_lo);
  hi = (tipRead(&TIPp->eventNumber_hi) & TIP_EVENTNUMBER_HI_MASK)>>16;

  rval = lo | ((unsigned long long)hi<<32);
  TIPUNLOCK;
  
  return rval;
}

/**
 * @ingroup MasterConfig
 * @brief Set the block number at which triggers will be disabled automatically
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetBlockLimit(unsigned int limit)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->blocklimit,limit);
  TIPUNLOCK;

  return OK;
}


/**
 * @ingroup Status
 * @brief Returns the value that is currently programmed as the block limit
 *
 * @return Current Block Limit if successful, otherwise ERROR
 */
unsigned int
tipGetBlockLimit()
{
  unsigned int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->blocklimit);
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the current status of the block limit
 *    
 * @return 1 if block limit has been reached, 0 if not, otherwise ERROR;
 *    
 */
int
tipGetBlockLimitStatus()
{
  unsigned int reg=0, rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  reg = tipRead(&TIPp->blockBuffer) & TIP_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT;
  if(reg)
    rval = 1;
  else
    rval = 0;
  TIPUNLOCK;

  return rval;
}


/**
 * @ingroup Readout
 * @brief Returns the number of Blocks available for readout
 *
 * @return Number of blocks available for readout if successful, otherwise ERROR
 *
 */
unsigned int
tipBReady()
{
  unsigned int blockBuffer=0, blockReady=0, rval=0;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return 0;
    }

  TIPLOCK;
  blockBuffer = tipRead(&TIPp->blockBuffer);
  rval        = (blockBuffer&TIP_BLOCKBUFFER_BLOCKS_READY_MASK)>>8;
  blockReady  = ((blockBuffer&TIP_BLOCKBUFFER_TRIGGERS_NEEDED_IN_BLOCK)>>16)?0:1;
  tipSyncEventReceived = (blockBuffer&TIP_BLOCKBUFFER_SYNCEVENT)>>31;
  tipNReadoutEvents = (blockBuffer&TIP_BLOCKBUFFER_RO_NEVENTS_MASK)>>24;

  if( (rval==1) && (tipSyncEventReceived) & (blockReady))
    tipSyncEventFlag = 1;
  else
    tipSyncEventFlag = 0;

  TIPUNLOCK;

  if(blockBuffer==ERROR)
    return ERROR;

  return rval;
}

/**
 * @ingroup Readout
 * @brief Return the value of the Synchronization flag, obtained from tiBReady.
 *   i.e. Return the value of the SyncFlag for the current readout block.
 *
 * @sa tiBReady
 * @return
 *   -  1: if current readout block contains a Sync Event.
 *   -  0: Otherwise
 *
 */
int
tipGetSyncEventFlag()
{
  int rval=0;
  
  TIPLOCK;
  rval = tipSyncEventFlag;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Readout
 * @brief Return the value of whether or not the sync event has been received
 *
 * @return
 *     - 1: if sync event received
 *     - 0: Otherwise
 *
 */
int
tipGetSyncEventReceived()
{
  int rval=0;
  
  TIPLOCK;
  rval = tipSyncEventReceived;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Readout
 * @brief Return the value of the number of readout events accepted
 *
 * @return Number of readout events accepted
 */
int
tipGetReadoutEvents()
{
  int rval=0;
  
  TIPLOCK;
  rval = tipNReadoutEvents;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup MasterConfig
 * @brief Set the block buffer level for the number of blocks in the system
 *     that need to be read out.
 *
 *     If this buffer level is full, the TI will go BUSY.
 *     The BUSY is released as soon as the number of buffers in the system
 *     drops below this level.
 *
 *  @param     level
 *       -        0:  No Buffer Limit  -  Pipeline mode
 *       -        1:  One Block Limit - "ROC LOCK" mode
 *       -  2-65535:  "Buffered" mode.
 *
 * @return OK if successful, otherwise ERROR
 *
 */
int
tipSetBlockBufferLevel(unsigned int level)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(level>TIP_BLOCKBUFFER_BUFFERLEVEL_MASK)
    {
      printf("%s: ERROR: Invalid value for level (%d)\n",
	     __FUNCTION__,level);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->blockBuffer, level);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Enable/Disable trigger inputs labelled TS#1-6 on the Front Panel
 *
 *     These inputs MUST be disabled if not connected.
 *
 *   @param   inpMask
 *       - 0:  TS#1
 *       - 1:  TS#2
 *       - 2:  TS#3
 *       - 3:  TS#4
 *       - 4:  TS#5
 *       - 5:  TS#6
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipEnableTSInput(unsigned int inpMask)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(inpMask>0x3f)
    {
      printf("%s: ERROR: Invalid inpMask (0x%x)\n",__FUNCTION__,inpMask);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->tsInput, inpMask);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Disable trigger inputs labelled TS#1-6 on the Front Panel
 *
 *     These inputs MUST be disabled if not connected.
 *
 *   @param   inpMask
 *       - 0:  TS#1
 *       - 1:  TS#2
 *       - 2:  TS#3
 *       - 3:  TS#4
 *       - 4:  TS#5
 *       - 5:  TS#6
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipDisableTSInput(unsigned int inpMask)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(inpMask>0x3f)
    {
      printf("%s: ERROR: Invalid inpMask (0x%x)\n",__FUNCTION__,inpMask);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->tsInput, tipRead(&TIPp->tsInput) & ~inpMask);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set (or unset) high level for the output ports on the front panel
 *     labelled as O#1-4
 *
 * @param         set1  O#1
 * @param         set2  O#2
 * @param         set3  O#3
 * @param         set4  O#4
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetOutputPort(unsigned int set1, unsigned int set2, unsigned int set3, unsigned int set4)
{
  unsigned int bits=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if(set1)
    bits |= (1<<0);
  if(set2)
    bits |= (1<<1);
  if(set3)
    bits |= (1<<2);
  if(set4)
    bits |= (1<<3);

  TIPLOCK;
  tipWrite(&TIPp->output, bits);
  TIPUNLOCK;

  return OK;
}


/**
 * @ingroup Config
 * @brief Set the clock to the specified source.
 *
 * @param   source
 *         -   0:  Onboard clock
 *         -   1:  External clock (HFBR1 input)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetClockSource(unsigned int source)
{
  int rval=OK;
  unsigned int clkset=0;
  unsigned int clkread=0;
  char sClock[20] = "";

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(source)
    {
    case 0: /* ONBOARD */
      clkset = TIP_CLOCK_INTERNAL;
      sprintf(sClock,"ONBOARD (%d)",source);
      break;
    case 1: /* EXTERNAL (HFBR1) */
      clkset = TIP_CLOCK_HFBR1;
      sprintf(sClock,"EXTERNAL-HFBR1 (%d)",source);
      break;
    default:
      printf("%s: ERROR: Invalid Clock Souce (%d)\n",__FUNCTION__,source);
      return ERROR;      
    }

  printf("%s: Setting clock source to %s\n",__FUNCTION__,sClock);


  TIPLOCK;
  tipWrite(&TIPp->clock, clkset);
  usleep(10);

  /* Reset DCM (Digital Clock Manager) - 250/200MHz */
  tipWrite(&TIPp->reset,TIP_RESET_CLK250);
  usleep(10);
  /* Reset DCM (Digital Clock Manager) - 125MHz */
  tipWrite(&TIPp->reset,TIP_RESET_CLK125);
  usleep(10);

  if(source==1) /* Turn on running mode for External Clock verification */
    {
      tipWrite(&TIPp->runningMode,TIP_RUNNINGMODE_ENABLE);
      usleep(10000);
      clkread = tipRead(&TIPp->clock) & TIP_CLOCK_MASK;
      if(clkread != clkset)
	{
	  printf("%s: ERROR Setting Clock Source (clkset = 0x%x, clkread = 0x%x)\n",
		 __FUNCTION__,clkset, clkread);
	  rval = ERROR;
	}
      tipWrite(&TIPp->runningMode,TIP_RUNNINGMODE_DISABLE);
    }
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the current clock source
 * @return Current Clock Source
 */
int
tipGetClockSource()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->clock) & 0x3;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set the fiber delay required to align the sync and triggers for all crates.
 * @return Current fiber delay setting
 */
void
tipSetFiberDelay(unsigned int delay, unsigned int offset)
{
  unsigned int fiberLatency=0, syncDelay=0, syncDelay_write=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  fiberLatency=0;
  TIPLOCK;

  if(delay>offset)
    {
      printf("%s: WARN: delay (%d) greater than offset (%d).  Setting difference to zero\n",
	     __FUNCTION__,delay,offset);
      syncDelay = 0;
    }
  else
    {
      syncDelay = (offset-(delay));
    }

  syncDelay_write = (syncDelay&0xff<<8) | (syncDelay&0xff<<16) | (syncDelay&0xff<<24);  /* set the sync delay according to the fiber latency */

  tipWrite(&TIPp->fiberSyncDelay,syncDelay_write);

#ifdef STOPTHIS
  if(tipMaster != 1)  /* Slave only */
    {
      taskDelay(10);
      tipWrite(&TIPp->reset,0x4000);  /* reset the IODELAY */
      taskDelay(10);
      tipWrite(&TIPp->reset,0x800);   /* auto adjust the sync phase for HFBR#1 */
      taskDelay(10);
    }
#endif

  TIPUNLOCK;

  printf("%s: Wrote 0x%08x to fiberSyncDelay\n",__FUNCTION__,syncDelay_write);

}

/**
 * @ingroup MasterConfig
 * @brief Add and configurate a TI Slave for the TI Master.
 *
 *      This routine should be used by the TI Master to configure
 *      HFBR porti and BUSY sources.
 *
 * @param    fiber  The fiber port of the TI Master that is connected to the slave
 *
 * @sa tiAddSlaveMask
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipAddSlave(unsigned int fiber)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  if(fiber!=1)
    {
      printf("%s: ERROR: Invalid value for fiber (%d)\n",
	     __FUNCTION__,fiber);
      return ERROR;
    }

  /* Add this slave to the global slave mask */
  tipSlaveMask |= (1<<(fiber-1));
  
  /* Add this fiber as a busy source (use first fiber macro as the base) */
  if(tipSetBusySource(TIP_BUSY_HFBR1<<(fiber-1),0)!=OK)
    return ERROR;

  /* Enable the fiber */
  if(tipEnableFiber(fiber)!=OK)
    return ERROR;

  return OK;

}

/**
 * @ingroup MasterConfig
 * @brief Set the value for a specified trigger rule.
 *
 * @param   rule  the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param   value  the specified time period (in steps of timestep)
 * @param timestep Timestep that is dependent on the trigger rule selected
 *<pre>
 *                         rule
 *    timestep    1      2       3       4
 *    -------   ----- ------- ------- -------
 *       0       16ns    16ns    16ns    16ns 
 *       1      160ns   320ns   640ns  1280ns 
 *       2     5120ns 10240ns 20480ns 40960ns
 *</pre>
 *
 * @return OK if successful, otherwise ERROR.
 *
 */
int
tipSetTriggerHoldoff(int rule, unsigned int value, int timestep)
{
  unsigned int wval=0, rval=0;
  unsigned int maxvalue=0x7f;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (rule<1) || (rule>5) )
    {
      printf("%s: ERROR: Invalid value for rule (%d).  Must be 1-4\n",
	     __FUNCTION__,rule);
      return ERROR;
    }
  if(value>maxvalue)
    {
      printf("%s: ERROR: Invalid value (%d). Must be less than %d.\n",
	     __FUNCTION__,value,maxvalue);
      return ERROR;
    }

  if(timestep)
    value |= (1<<7);

  /* Read the previous values */
  TIPLOCK;
  rval = tipRead(&TIPp->triggerRule);
  
  switch(rule)
    {
    case 1:
      wval = value | (rval & ~TIP_TRIGGERRULE_RULE1_MASK);
      break;
    case 2:
      wval = (value<<8) | (rval & ~TIP_TRIGGERRULE_RULE2_MASK);
      break;
    case 3:
      wval = (value<<16) | (rval & ~TIP_TRIGGERRULE_RULE3_MASK);
      break;
    case 4:
      wval = (value<<24) | (rval & ~TIP_TRIGGERRULE_RULE4_MASK);
      break;
    }

  tipWrite(&TIPp->triggerRule,wval);

  if(timestep==2)
    tipWrite(&TIPp->vmeControl, 
	     tipRead(&TIPp->vmeControl) | TIP_VMECONTROL_SLOWER_TRIGGER_RULES);
  else
    tipWrite(&TIPp->vmeControl, 
	     tipRead(&TIPp->vmeControl) & ~TIP_VMECONTROL_SLOWER_TRIGGER_RULES);

  TIPUNLOCK;

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the value for a specified trigger rule.
 *
 * @param   rule   the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @return If successful, returns the value (in steps of 16ns) 
 *            for the specified rule. ERROR, otherwise.
 *
 */
int
tipGetTriggerHoldoff(int rule)
{
  unsigned int rval=0;
  
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if(rule<1 || rule>5)
    {
      printf("%s: ERROR: Invalid value for rule (%d).  Must be 1-4.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->triggerRule);
  TIPUNLOCK;
  
  switch(rule)
    {
    case 1:
      rval = (rval & TIP_TRIGGERRULE_RULE1_MASK);
      break;

    case 2:
      rval = (rval & TIP_TRIGGERRULE_RULE2_MASK)>>8;
      break;

    case 3:
      rval = (rval & TIP_TRIGGERRULE_RULE3_MASK)>>16;
      break;

    case 4:
      rval = (rval & TIP_TRIGGERRULE_RULE4_MASK)>>24;
      break;
    }

  return rval;

}

/**
 * @ingroup MasterConfig
 * @brief Set the value for the minimum time of specified trigger rule.
 *
 * @param   rule  the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param   value  the specified time period (in steps of timestep)
 *<pre>
 *       	 	      rule
 *    		         2      3      4
 *    		       ----- ------ ------
 *    		        16ns  160ns  160ns 
 *    	(timestep=2)    16ns 5120ns 5120ns
 *</pre>
 *
 * @return OK if successful, otherwise ERROR.
 *
 */
int
tipSetTriggerHoldoffMin(int rule, unsigned int value)
{
  unsigned int mask=0, enable=0, shift=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if(rule<2 || rule>5)
    {
      printf("%s: ERROR: Invalid rule (%d).  Must be 2-4.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  if(value > 0x7f)
    {
      printf("%s: ERROR: Invalid value (%d). Must be less than %d.\n",
	     __FUNCTION__,value,0x7f);
      return ERROR;
    }

  switch(rule)
    {
    case 2:
      mask = ~(TIP_TRIGGERRULEMIN_MIN2_MASK | TIP_TRIGGERRULEMIN_MIN2_EN);
      enable = TIP_TRIGGERRULEMIN_MIN2_EN;
      shift = 8;
      break;
    case 3:
      mask = ~(TIP_TRIGGERRULEMIN_MIN3_MASK | TIP_TRIGGERRULEMIN_MIN3_EN);
      enable = TIP_TRIGGERRULEMIN_MIN3_EN;
      shift = 16;
      break;
    case 4:
      mask = ~(TIP_TRIGGERRULEMIN_MIN4_MASK | TIP_TRIGGERRULEMIN_MIN4_EN);
      enable = TIP_TRIGGERRULEMIN_MIN4_EN;
      shift = 24;
      break;
    }

  TIPLOCK;
  tipWrite(&TIPp->triggerRuleMin, 
	     (tipRead(&TIPp->triggerRuleMin) & mask) |
	     enable |
	     (value << shift) );
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get the value for a specified trigger rule minimum busy.
 *
 * @param   rule   the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param  pflag  if not 0, print the setting to standard out.
 *
 * @return If successful, returns the value 
 *          (in steps of 16ns for rule 2, 480ns otherwise) 
 *            for the specified rule. ERROR, otherwise.
 *
 */
int
tipGetTriggerHoldoffMin(int rule, int pflag)
{
  int rval=0;
  unsigned int mask=0, enable=0, shift=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if(rule<2 || rule>5)
    {
      printf("%s: ERROR: Invalid rule (%d).  Must be 2-4.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  switch(rule)
    {
    case 2:
      mask = TIP_TRIGGERRULEMIN_MIN2_MASK;
      enable = TIP_TRIGGERRULEMIN_MIN2_EN;
      shift = 8;
      break;
    case 3:
      mask = TIP_TRIGGERRULEMIN_MIN3_MASK;
      enable = TIP_TRIGGERRULEMIN_MIN3_EN;
      shift = 16;
      break;
    case 4:
      mask = TIP_TRIGGERRULEMIN_MIN4_MASK;
      enable = TIP_TRIGGERRULEMIN_MIN4_EN;
      shift = 24;
      break;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->triggerRuleMin) & mask)>>shift;
  TIPUNLOCK;

  if(pflag)
    {
      printf("%s: Trigger rule %d  minimum busy = %d - %s\n",
	     __FUNCTION__,rule,
	     rval & 0x7f,
	     (rval & (1<<7))?"ENABLED":"DISABLED");
    }

  return rval & ~(1<<8);
}

#ifdef NOTIMPLEMENTED
/**
 *  @ingroup Config
 *  @brief Disable the necessity to readout the TI for every block.
 *
 *      For instances when the TI data is not required for analysis
 *      When a block is "ready", a call to tiResetBlockReadout must be made.
 *
 * @sa tiEnableDataReadout tiResetBlockReadout
 * @return OK if successful, otherwise ERROR
 */
int
tipDisableDataReadout()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  tipReadoutEnabled = 0;
  TIPLOCK;
  tipWrite(&TIPp->vmeControl,
	     tipRead(&TIPp->vmeControl) | TIP_VMECONTROL_BUFFER_DISABLE);
  TIPUNLOCK;
  
  printf("%s: Readout disabled.\n",__FUNCTION__);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Enable readout the TI for every block.
 *
 * @sa tiDisableDataReadout
 * @return OK if successful, otherwise ERROR
 */
int
tipEnableDataReadout()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  tipReadoutEnabled = 1;
  TIPLOCK;
  tipWrite(&TIPp->vmeControl,
	   tipRead(&TIPp->vmeControl) & ~TIP_VMECONTROL_BUFFER_DISABLE);
  TIPUNLOCK;

  printf("%s: Readout enabled.\n",__FUNCTION__);

  return OK;
}
#endif /* NOTIMPLEMENTED */

/**
 *  @ingroup Readout
 *  @brief Decrement the hardware counter for blocks available, effectively
 *      simulating a readout from the data fifo.
 *
 * @sa tiDisableDataReadout
 */
void
tipResetBlockReadout()
{
 
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }
 
  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_BLOCK_READOUT);
  TIPUNLOCK;

}


/**
 * @ingroup MasterConfig
 * @brief Configure trigger table to be loaded with a user provided array.
 *
 * @param itable Input Table (Array of 16 4byte words)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipTriggerTableConfig(unsigned int *itable)
{
  int ielement=0;

  if(itable==NULL)
    {
      printf("%s: ERROR: Invalid input table address\n",
	     __FUNCTION__);
      return ERROR;
    }

  for(ielement=0; ielement<16; ielement++)
    tipTrigPatternData[ielement] = itable[ielement];
  
  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Get the current trigger table stored in local memory (not necessarily on TI).
 *
 * @param otable Output Table (Array of 16 4byte words, user must allocate memory)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipGetTriggerTable(unsigned int *otable)
{
  int ielement=0;

  if(otable==NULL)
    {
      printf("%s: ERROR: Invalid output table address\n",
	     __FUNCTION__);
      return ERROR;
    }

  for(ielement=0; ielement<16; ielement++)
    otable[ielement] = tipTrigPatternData[ielement];
  
  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Configure trigger tabled to be loaded with a predefined
 * trigger table (mapping TS inputs to trigger types).
 *
 * @param mode
 *  - 0:
 *    - TS#1,2,3,4,5 generates Trigger1 (physics trigger),
 *    - TS#6 generates Trigger2 (playback trigger),
 *    - No SyncEvent;
 *  - 1:
 *    - TS#1,2,3 generates Trigger1 (physics trigger), 
 *    - TS#4,5,6 generates Trigger2 (playback trigger).  
 *    - If both Trigger1 and Trigger2, they are SyncEvent;
 *  - 2:
 *    - TS#1,2,3,4,5 generates Trigger1 (physics trigger),
 *    - TS#6 generates Trigger2 (playback trigger),
 *    - If both Trigger1 and Trigger2, generates SyncEvent;
 *  - 3:
 *    - TS#1,2,3,4,5,6 generates Trigger1 (physics trigger),
 *    - No Trigger2 (playback trigger),
 *    - No SyncEvent; 
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipTriggerTablePredefinedConfig(int mode)
{
  int ielement=0;
  unsigned int trigPattern[4][16] = 
    {
      { /* mode 0:
	   TS#1,2,3,4,5 generates Trigger1 (physics trigger),
	   TS#6 generates Trigger2 (playback trigger),
	   No SyncEvent;
	*/
	0x43424100, 0x47464544, 0x4b4a4948, 0x4f4e4d4c,
	0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c,
	0x636261a0, 0x67666564, 0x6b6a6968, 0x6f6e6d6c,
	0x73727170, 0x77767574, 0x7b7a7978, 0x7f7e7d7c, 
      },
      { /* mode 1:
	   TS#1,2,3 generates Trigger1 (physics trigger), 
	   TS#4,5,6 generates Trigger2 (playback trigger).  
	   If both Trigger1 and Trigger2, they are SyncEvent;
	*/
	0x43424100, 0x47464544, 0xcbcac988, 0xcfcecdcc,
	0xd3d2d190, 0xd7d6d5d4, 0xdbdad998, 0xdfdedddc,
	0xe3e2e1a0, 0xe7e6e5e4, 0xebeae9a8, 0xefeeedec,
	0xf3f2f1b0, 0xf7f6f5f4, 0xfbfaf9b8, 0xfffefdfc, 
      },
      { /* mode 2:
	   TS#1,2,3,4,5 generates Trigger1 (physics trigger),
	   TS#6 generates Trigger2 (playback trigger),
	   If both Trigger1 and Trigger2, generates SyncEvent;
	*/
	0x43424100, 0x47464544, 0x4b4a4948, 0x4f4e4d4c,
	0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c,
	0xe3e2e1a0, 0xe7e6e5e4, 0xebeae9e8, 0xefeeedec,
	0xf3f2f1f0, 0xf7f6f5f4, 0xfbfaf9f8, 0xfffefdfc 
      },
      { /* mode 3:
           TS#1,2,3,4,5,6 generates Trigger1 (physics trigger),
           No Trigger2 (playback trigger),
           No SyncEvent;
        */
        0x43424100, 0x47464544, 0x4b4a4948, 0x4f4e4d4c,
        0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c,
        0x63626160, 0x67666564, 0x6b6a6968, 0x6f6e6d6c,
        0x73727170, 0x77767574, 0x7b7a7978, 0x7f7e7d7c,
      }
    };

  if(mode>3)
    {
      printf("%s: WARN: Invalid mode %d.  Using Trigger Table mode = 0\n",
	     __FUNCTION__,mode);
      mode=0;
    }

  /* Copy predefined choice into static array to be loaded */

  for(ielement=0; ielement<16; ielement++)
    {
      tipTrigPatternData[ielement] = trigPattern[mode][ielement];
    }
  
  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Define a specific trigger pattern as a hardware trigger (trig1/trig2/syncevent)
 * and Event Type
 *
 * @param trigMask Trigger Pattern (must be less than 0x3F)
 *    - TS inputs defining the pattern.  Starting bit: TS#1 = bit0
 * @param hwTrig Hardware trigger type (must be less than 3)
 *      0:  no trigger
 *      1:  Trig1 (event trigger)
 *      2:  Trig2 (playback trigger)
 *      3:  SyncEvent
 * @param evType Event Type (must be less than 255)
 *
 * @return OK if successful, otherwise ERROR
 */

int
tipDefineEventType(int trigMask, int hwTrig, int evType)
{
  int element=0, byte=0;
  int data=0;
  unsigned int old_pattern=0;

  if(trigMask>0x3f)
    {
      printf("%s: ERROR: Invalid trigMask (0x%x)\n",
	     __FUNCTION__, trigMask);
      return ERROR;
    }

  if(hwTrig>3)
    {
      printf("%s: ERROR: Invalid hwTrig (%d)\n",
	     __FUNCTION__, hwTrig);
      return ERROR;
    }

  if(evType>0x3F)
    {
      printf("%s: ERROR: Invalid evType (%d)\n",
	     __FUNCTION__, evType);
      return ERROR;
    }

  element = trigMask/4;
  byte    = trigMask%4;

  data    = (hwTrig<<6) | evType;

  old_pattern = (tipTrigPatternData[element] & ~(0xFF<<(byte*8)));
  tipTrigPatternData[element] = old_pattern | (data<<(byte*8));

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Define the event type for the TI Master's fixed and random internal trigger.
 *
 * @param fixed_type Fixed Pulser Event Type
 * @param random_type Pseudo Random Pulser Event Type
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipDefinePulserEventType(int fixed_type, int random_type)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fixed_type<0)||(fixed_type>0xFF))
    {
      printf("%s: ERROR: Invalid fixed_type (0x%x)\n",__FUNCTION__,fixed_type);
      return ERROR;
    }

  if((random_type<0)||(random_type>0xFF))
    {
      printf("%s: ERROR: Invalid random_type (0x%x)\n",__FUNCTION__,random_type);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->pulserEvType,
	     (fixed_type)<<16 | (random_type)<<24);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Load a predefined trigger table (mapping TS inputs to trigger types).
 *
 * @param mode
 *  - 0:
 *    - TS#1,2,3,4,5 generates Trigger1 (physics trigger),
 *    - TS#6 generates Trigger2 (playback trigger),
 *    - No SyncEvent;
 *  - 1:
 *    - TS#1,2,3 generates Trigger1 (physics trigger), 
 *    - TS#4,5,6 generates Trigger2 (playback trigger).  
 *    - If both Trigger1 and Trigger2, they are SyncEvent;
 *  - 2:
 *    - TS#1,2,3,4,5 generates Trigger1 (physics trigger),
 *    - TS#6 generates Trigger2 (playback trigger),
 *    - If both Trigger1 and Trigger2, generates SyncEvent;
 *  - 3:
 *    - TS#1,2,3,4,5,6 generates Trigger1 (physics trigger),
 *    - No Trigger2 (playback trigger),
 *    - No SyncEvent; 
 *  - 4:
 *    User configured table @sa tiDefineEventType, tiTriggerTablePredefinedConfig
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipLoadTriggerTable(int mode)
{
  int ipat;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(mode>4)
    {
      printf("%s: WARN: Invalid mode %d.  Using Trigger Table mode = 0\n",
	     __FUNCTION__,mode);
      mode=0;
    }

  if(mode!=4)
    tipTriggerTablePredefinedConfig(mode);
  
  TIPLOCK;
  for(ipat=0; ipat<16; ipat++)
    tipWrite(&TIPp->trigTable[ipat], tipTrigPatternData[ipat]);

  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterStatus
 * @brief Print trigger table to standard out.
 *
 * @param showbits Show trigger bit pattern, instead of hex
 *
 */
void
tipPrintTriggerTable(int showbits)
{
  int ielement, ibyte;
  int hwTrig=0, evType=0;

  for(ielement = 0; ielement<16; ielement++)
    {
      if(showbits)
	{
	  printf("--TS INPUT-\n");
	  printf("1 2 3 4 5 6  HW evType\n");
	}
      else
	{
	  printf("TS Pattern  HW evType\n");
	}

      for(ibyte=0; ibyte<4; ibyte++)
	{
	  hwTrig = ((tipTrigPatternData[ielement]>>(ibyte*8)) & 0xC0)>>6;
	  evType = (tipTrigPatternData[ielement]>>(ibyte*8)) & 0x3F;
	  
	  if(showbits)
	    {
	      printf("%d %d %d %d %d %d   %d   %2d\n", 
		     ((ielement*4+ibyte) & (1<<0))?1:0,
		     ((ielement*4+ibyte) & (1<<1))?1:0,
		     ((ielement*4+ibyte) & (1<<2))?1:0,
		     ((ielement*4+ibyte) & (1<<3))?1:0,
		     ((ielement*4+ibyte) & (1<<4))?1:0,
		     ((ielement*4+ibyte) & (1<<5))?1:0,
		     hwTrig, evType);
	    }
	  else
	    {
	      printf("  0x%02x       %d   %2d\n", ielement*4+ibyte,hwTrig, evType);
	    }
	}
      printf("\n");

    }


}


/**
 *  @ingroup MasterConfig
 *  @brief Set the window of the input trigger coincidence window
 *  @param window_width Width of the input coincidence window (units of 4ns)
 *  @return OK if successful, otherwise ERROR
 */
int
tipSetTriggerWindow(int window_width)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((window_width<1) || (window_width>TIP_TRIGGERWINDOW_COINC_MASK))
    {
      printf("%s: ERROR: Invalid Trigger Coincidence Window (%d)\n",
	     __FUNCTION__,window_width);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->triggerWindow,
	     (tipRead(&TIPp->triggerWindow) & ~TIP_TRIGGERWINDOW_COINC_MASK) 
	     | window_width);
  TIPUNLOCK;

  return OK;
}

/**
 *  @ingroup MasterStatus
 *  @brief Get the window of the input trigger coincidence window
 *  @return Width of the input coincidence window (units of 4ns), otherwise ERROR
 */
int
tipGetTriggerWindow()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->triggerWindow) & ~TIP_TRIGGERWINDOW_COINC_MASK;
  TIPUNLOCK;

  return rval;
}

/**
 *  @ingroup MasterConfig
 *  @brief Set the width of the input trigger inhibit window
 *  @param window_width Width of the input inhibit window (units of 4ns)
 *  @return OK if successful, otherwise ERROR
 */
int
tipSetTriggerInhibitWindow(int window_width)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((window_width<1) || (window_width>(TIP_TRIGGERWINDOW_INHIBIT_MASK>>8)))
    {
      printf("%s: ERROR: Invalid Trigger Inhibit Window (%d)\n",
	     __FUNCTION__,window_width);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->triggerWindow,
	     (tipRead(&TIPp->triggerWindow) & ~TIP_TRIGGERWINDOW_INHIBIT_MASK) 
	     | (window_width<<8));
  TIPUNLOCK;

  return OK;
}

/**
 *  @ingroup MasterStatus
 *  @brief Get the width of the input trigger inhibit window
 *  @return Width of the input inhibit window (units of 4ns), otherwise ERROR
 */
int
tipGetTriggerInhibitWindow()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->triggerWindow) & TIP_TRIGGERWINDOW_INHIBIT_MASK)>>8;
  TIPUNLOCK;

  return rval;
}

/**
 *  @ingroup MasterConfig
 *  @brief Set the delay of Trig1 relative to Trig2 when trigger source is 11.
 *
 *  @param delay Trig1 delay after Trig2
 *    - Latency in steps of 4 nanoseconds with an offset of ~2.6 microseconds
 *
 *  @return OK if successful, otherwise ERROR
 */

int
tipSetTrig21Delay(int delay)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(delay>0x1FF)
    {
      printf("%s: ERROR: Invalid delay (%d)\n",
	     __FUNCTION__,delay);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->triggerWindow, 
	     (tipRead(&TIPp->triggerWindow) & ~TIP_TRIGGERWINDOW_TRIG21_MASK) |
	     (delay<<16));
  TIPUNLOCK;
  return OK;
}

/**
 *  @ingroup MasterStatus
 *  @brief Get the delay of Trig1 relative to Trig2 when trigger source is 11.
 *
 *  @return Latency in steps of 4 nanoseconds with an offset of ~2.6 microseconds, otherwise ERROR
 */

int
tipGetTrig21Delay()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->triggerWindow) & TIP_TRIGGERWINDOW_TRIG21_MASK)>>16;
  TIPUNLOCK;

  return rval;
}


/**
 *  @ingroup MasterConfig
 *  @brief Latch the Busy and Live Timers.
 *
 *     This routine should be called prior to a call to tiGetLiveTime and tiGetBusyTime
 *
 *  @sa tiGetLiveTime
 *  @sa tiGetBusyTime
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipLatchTimers()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_SCALERS_LATCH);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Return the current "live" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tipGetLiveTime()
{
  unsigned int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->livetime);
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Return the current "busy" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tipGetBusyTime()
{
  unsigned int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->busytime);
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Calculate the live time (percentage) from the live and busy time scalers
 *
 * @param sflag if > 0, then returns the integrated live time
 *
 * @return live time as a 3 digit integer % (e.g. 987 = 98.7%)
 *
 */
int
tipLive(int sflag)
{
  int rval=0;
  float fval=0;
  unsigned int newBusy=0, newLive=0, newTotal=0;
  unsigned int live=0, total=0;
  static unsigned int oldLive=0, oldTotal=0;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_SCALERS_LATCH);
  newLive = tipRead(&TIPp->livetime);
  newBusy = tipRead(&TIPp->busytime);

  newTotal = newLive+newBusy;

  if((sflag==0) && (oldTotal<newTotal))
    { /* Differential */
      live  = newLive - oldLive;
      total = newTotal - oldTotal;
    }
  else
    { /* Integrated */
      live = newLive;
      total = newTotal;
    }

  oldLive = newLive;
  oldTotal = newTotal;

  if(total>0)
    fval = 1000*(((float) live)/((float) total));

  rval = (int) fval;

  TIPUNLOCK;

  return rval;
}


/**
 * @ingroup Status
 * @brief Get the current counter for the specified TS Input
 *
 * @param input
 *   - 1-6 : TS Input (1-6)
 * @param latch:
 *   -  0: Do not latch before readout
 *   -  1: Latch before readout
 *   -  2: Latch and reset before readout
 *      
 *
 * @return Specified counter value
 *
 */
unsigned int
tipGetTSscaler(int input, int latch)
{
  unsigned int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((input<1)||(input>6))
    {
      printf("%s: ERROR: Invalid input (%d).\n",
	     __FUNCTION__,input);
      return ERROR;
    }

  if((latch<0) || (latch>2))
    {
      printf("%s: ERROR: Invalid latch (%d).\n",
	     __FUNCTION__,latch);
      return ERROR;
    }

  TIPLOCK;
  switch(latch)
    {
    case 1: 
      tipWrite(&TIPp->reset,TIP_RESET_SCALERS_LATCH);
      break;

    case 2:
      tipWrite(&TIPp->reset,TIP_RESET_SCALERS_LATCH | TIP_RESET_SCALERS_RESET);
      break;
    }

  rval = tipRead(&TIPp->ts_scaler[input-1]);
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Show block Status of specified fiber
 * @param fiber  Fiber port to show
 * @param pflag  Whether or not to print to standard out
 * @return 0
 */
unsigned int
tipBlockStatus(int fiber, int pflag)
{
  unsigned int rval=0;
  char name[50];
  unsigned int nblocksReady, nblocksNeedAck;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(fiber>1)
    {
      printf("%s: ERROR: Invalid value (%d) for fiber\n",__FUNCTION__,fiber);
      return ERROR;

    }

  switch(fiber)
    {
    case 0:
      rval = (tipRead(&TIPp->adr24) & 0xFFFF0000)>>16;
      break;

    case 1:
    case 3:
    case 5:
    case 7:
      rval = (tipRead(&TIPp->blockStatus[(fiber-1)/2]) & 0xFFFF);
      break;

    case 2:
    case 4:
    case 6:
    case 8: 
      rval = ( tipRead(&TIPp->blockStatus[(fiber/2)-1]) & 0xFFFF0000 )>>16;
      break;
    }

  if(pflag)
    {
      nblocksReady   = rval & TIP_BLOCKSTATUS_NBLOCKS_READY0;
      nblocksNeedAck = (rval & TIP_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;

      if(fiber==0)
	sprintf(name,"Loopback");
      else
	sprintf(name,"Fiber %d",fiber);

      printf("%s: %s : Blocks ready / need acknowledge: %d / %d\n",
	     __FUNCTION__, name,
	     nblocksReady, nblocksNeedAck);

    }

  return rval;
}

static void 
FiberMeas()
{
  int clksrc=0;
  unsigned int defaultDelay=0x1f1f1f00, fiberLatency=0, syncDelay=0, syncDelay_write=0;


  clksrc = tipGetClockSource();
  /* Check to be sure the TI has external HFBR1 clock enabled */
  if(clksrc != TIP_CLOCK_HFBR1)
    {
      printf("%s: ERROR: Unable to measure fiber latency without HFBR1 as Clock Source\n",
	     __FUNCTION__);
      printf("\t Using default Fiber Sync Delay = %d (0x%x)",
	     defaultDelay, defaultDelay);

      TIPLOCK;
      tipWrite(&TIPp->fiberSyncDelay,defaultDelay);
      TIPUNLOCK;

      return;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset,TIP_RESET_IODELAY); // reset the IODELAY
  usleep(100);
  tipWrite(&TIPp->reset,TIP_RESET_FIBER_AUTO_ALIGN);  // auto adjust the return signal phase
  usleep(100);
  tipWrite(&TIPp->reset,TIP_RESET_MEASURE_LATENCY);  // measure the fiber latency
  usleep(1000);

  fiberLatency = tipRead(&TIPp->fiberLatencyMeasurement);  //fiber 1 latency measurement result

  printf("Software offset = 0x%08x (%d)\n",tipFiberLatencyOffset, tipFiberLatencyOffset);
  printf("Fiber Latency is 0x%08x\n",fiberLatency);
  printf("  Latency data = 0x%08x (%d ns)\n",(fiberLatency>>23), (fiberLatency>>23) * 4);


  tipWrite(&TIPp->reset,TIP_RESET_AUTOALIGN_HFBR1_SYNC);   // auto adjust the sync phase for HFBR#1

  usleep(100);

  fiberLatency = tipRead(&TIPp->fiberLatencyMeasurement);  //fiber 1 latency measurement result

  tipFiberLatencyMeasurement = ((fiberLatency & TIP_FIBERLATENCYMEASUREMENT_DATA_MASK)>>23)>>1;
  syncDelay = (tipFiberLatencyOffset-(((fiberLatency>>23)&0x1ff)>>1));
  syncDelay_write = (syncDelay&0xFF)<<8 | (syncDelay&0xFF)<<16 | (syncDelay&0xFF)<<24;
  usleep(100);

  tipWrite(&TIPp->fiberSyncDelay,syncDelay_write);
  usleep(10);
  syncDelay = tipRead(&TIPp->fiberSyncDelay);
  TIPUNLOCK;

  printf (" \n The fiber latency of 0xA0 is: 0x%08x\n", fiberLatency);
  printf (" \n The sync latency of 0x50 is: 0x%08x\n",syncDelay);
}

/**
 * @ingroup Status
 * @brief Return measured fiber length
 * @return Value of measured fiber length
 */
int
tipGetFiberLatencyMeasurement()
{
  return tipFiberLatencyMeasurement;
}

/**
 * @ingroup MasterConfig
 * @brief Enable/Disable operation of User SyncReset
 * @sa tiUserSyncReset
 * @param enable
 *   - >0: Enable
 *   - 0: Disable
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSetUserSyncResetReceive(int enable)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  if(enable)
    tipWrite(&TIPp->sync, (tipRead(&TIPp->sync) & TIP_SYNC_SOURCEMASK) | 
	       TIP_SYNC_USER_SYNCRESET_ENABLED);
  else
    tipWrite(&TIPp->sync, (tipRead(&TIPp->sync) & TIP_SYNC_SOURCEMASK) &
	       ~TIP_SYNC_USER_SYNCRESET_ENABLED);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Return last SyncCommand received
 * @param 
 *   - >0: print to standard out
 * @return Last SyncCommand received
 */
int
tipGetLastSyncCodes(int pflag)
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }


  TIPLOCK;
  if(tipMaster)
    rval = ((tipRead(&TIPp->sync) & TIP_SYNC_LOOPBACK_CODE_MASK)>>16) & 0xF;
  else
    rval = ((tipRead(&TIPp->sync) & TIP_SYNC_HFBR1_CODE_MASK)>>8) & 0xF;
  TIPUNLOCK;

  if(pflag)
    {
      printf(" Last Sync Code received:  0x%x\n",rval);
    }

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the status of the SyncCommand History Buffer
 *
 * @param pflag  
 *   - >0: Print to standard out
 *
 * @return
 *   - 0: Empty
 *   - 1: Half Full
 *   - 2: Full
 */
int
tipGetSyncHistoryBufferStatus(int pflag)
{
  int hist_status=0, rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  hist_status = tipRead(&TIPp->sync) 
    & (TIP_SYNC_HISTORY_FIFO_MASK);
  TIPUNLOCK;

  switch(hist_status)
    {
    case TIP_SYNC_HISTORY_FIFO_EMPTY:
      rval=0;
      if(pflag) printf("%s: Sync history buffer EMPTY\n",__FUNCTION__);
      break;

    case TIP_SYNC_HISTORY_FIFO_HALF_FULL:
      rval=1;
      if(pflag) printf("%s: Sync history buffer HALF FULL\n",__FUNCTION__);
      break;

    case TIP_SYNC_HISTORY_FIFO_FULL:
    default:
      rval=2;
      if(pflag) printf("%s: Sync history buffer FULL\n",__FUNCTION__);
      break;
    }

  return rval;

}

/**
 * @ingroup Config
 * @brief Reset the SyncCommand history buffer
 */
void
tipResetSyncHistory()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_SYNC_HISTORY);
  TIPUNLOCK;

}

/**
 * @ingroup Config
 * @brief Control level of the SyncReset signal
 * @sa tiSetUserSyncResetReceive
 * @param enable
 *   - >0: High
 *   -  0: Low
 * @param pflag
 *   - >0: Print status to standard out
 *   -  0: Supress status message
 */
void
tipUserSyncReset(int enable, int pflag)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  TIPLOCK;
  if(enable)
    tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_SYNCRESET_HIGH); 
  else
    tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_SYNCRESET_LOW); 

  usleep(20000);
  TIPUNLOCK;

  if(pflag)
    {
      printf("%s: User Sync Reset ",__FUNCTION__);
      if(enable)
	printf("HIGH\n");
      else
	printf("LOW\n");
    }

}

/**
 * @ingroup Status
 * @brief Print to standard out the history buffer of Sync Commands received.
 */
void
tipPrintSyncHistory()
{
  unsigned int syncHistory=0;
  int count=0, code=1, valid=0, timestamp=0, overflow=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }
  
  while(code!=0)
    {
      TIPLOCK;
      syncHistory = tipRead(&TIPp->syncHistory);
      TIPUNLOCK;

      printf("     TimeStamp: Code (valid)\n");

      if(tipMaster)
	{
	  code  = (syncHistory & TIP_SYNCHISTORY_LOOPBACK_CODE_MASK)>>10;
	  valid = (syncHistory & TIP_SYNCHISTORY_LOOPBACK_CODE_VALID)>>14;
	}
      else
	{
	  code  = syncHistory & TIP_SYNCHISTORY_HFBR1_CODE_MASK;
	  valid = (syncHistory & TIP_SYNCHISTORY_HFBR1_CODE_VALID)>>4;
	}
      
      overflow  = (syncHistory & TIP_SYNCHISTORY_TIMESTAMP_OVERFLOW)>>15;
      timestamp = (syncHistory & TIP_SYNCHISTORY_TIMESTAMP_MASK)>>16;

/*       if(valid) */
	{
	  printf("%4d: 0x%08x   %d %5d :  0x%x (%d)\n",
		 count, syncHistory,
		 overflow, timestamp, code, valid);
	}
      count++;
      if(count>1024)
	{
	  printf("%s: More than expected in the Sync History Buffer... exitting\n",
		 __FUNCTION__);
	  break;
	}
    }
}


/**
 * @ingroup MasterConfig
 * @brief Set the value of the syncronization event interval
 *
 * 
 * @param  blk_interval 
 *      Sync Event will occur in the last event of the set blk_interval (number of blocks)
 * 
 * @return OK if successful, otherwise ERROR
 */
int
tipSetSyncEventInterval(int blk_interval)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  if(blk_interval>TIP_SYNCEVENTCTRL_NBLOCKS_MASK)
    {
      printf("%s: WARN: Value for blk_interval (%d) too large.  Setting to %d\n",
	     __FUNCTION__,blk_interval,TIP_SYNCEVENTCTRL_NBLOCKS_MASK);
      blk_interval = TIP_SYNCEVENTCTRL_NBLOCKS_MASK;
    }

  TIPLOCK;
  tipWrite(&TIPp->syncEventCtrl, blk_interval);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterStatus
 * @brief Get the SyncEvent Block interval
 * @return Block interval of the SyncEvent
 */
int
tipGetSyncEventInterval()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->syncEventCtrl) & TIP_SYNCEVENTCTRL_NBLOCKS_MASK;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup MasterReadout
 * @brief Force a sync event (type = 0).
 * @return OK if successful, otherwise ERROR
 */
int
tipForceSyncEvent()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_FORCE_SYNCEVENT);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Sync Reset Request is sent to TI-Master or TS.  
 *
 *    This option is available for multicrate systems when the
 *    synchronization is suspect.  It should be exercised only during
 *    "sync events" where the requested sync reset will immediately
 *    follow all ROCs concluding their readout.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipSyncResetRequest()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipDoSyncResetRequest=1;
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterReadout
 * @brief Determine if a TI has requested a Sync Reset
 *
 * @return 1 if requested received, 0 if not, otherwise ERROR
 */
int
tipGetSyncResetRequest()
{
  int request=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }


  TIPLOCK;
  request = (tipRead(&TIPp->blockBuffer) & TIP_BLOCKBUFFER_SYNCRESET_REQUESTED)>>30;
  TIPUNLOCK;

  return request;
}

/**
 * @ingroup MasterConfig
 * @brief Reset the registers that record the triggers enabled status of TI Slaves.
 *
 */
void
tipTriggerReadyReset()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return;
    }
  
  TIPLOCK;
  tipWrite(&TIPp->syncCommand,TIP_SYNCCOMMAND_TRIGGER_READY_RESET); 
  TIPUNLOCK;


}

/**
 * @ingroup MasterReadout
 * @brief Generate non-physics triggers until the current block is filled.
 *    This feature is useful for "end of run" situations.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipFillToEndBlock()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_FILL_TO_END_BLOCK);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup MasterConfig
 * @brief Reset the MGT
 * @return OK if successful, otherwise ERROR
 */
int
tipResetMGT()
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->reset, TIP_RESET_MGT);
  TIPUNLOCK;
  usleep(10000);

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the input delay for the specified front panel TSinput (1-6)
 * @param chan Front Panel TSInput Channel (1-6)
 * @param delay Delay in units of 4ns (0=8ns)
 * @return OK if successful, otherwise ERROR
 */
int
tipSetTSInputDelay(int chan, int delay)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((chan<1) || (chan>6))
    {
      printf("%s: ERROR: Invalid chan (%d)\n",__FUNCTION__,
	     chan);
      return ERROR;
    }

  if((delay<0) || (delay>0x1ff))
    {
      printf("%s: ERROR: Invalid delay (%d)\n",__FUNCTION__,
	     delay);
      return ERROR;
    }

  TIPLOCK;
  chan--;
  tipWrite(&TIPp->fpDelay[chan/3],
	     (tipRead(&TIPp->fpDelay[chan/3]) & ~TIP_FPDELAY_MASK(chan))
	     | delay<<(10*(chan%3)));
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get the input delay for the specified front panel TSinput (1-6)
 * @param chan Front Panel TSInput Channel (1-6)
 * @return Channel delay (units of 4ns) if successful, otherwise ERROR
 */
int
tipGetTSInputDelay(int chan)
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((chan<1) || (chan>6))
    {
      printf("%s: ERROR: Invalid chan (%d)\n",__FUNCTION__,
	     chan);
      return ERROR;
    }

  TIPLOCK;
  chan--;
  rval = (tipRead(&TIPp->fpDelay[chan/3]) & TIP_FPDELAY_MASK(chan))>>(10*(chan%3));
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Print Front Panel TSinput Delays to Standard Out
 * @return OK if successful, otherwise ERROR
 */
int
tipPrintTSInputDelay()
{
  unsigned int reg[2];
  int ireg=0, ichan=0, delay=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  for(ireg=0; ireg<2; ireg++)
      reg[ireg] = tipRead(&TIPp->fpDelay[ireg]);
  TIPUNLOCK;

  printf("%s: Front panel delays:", __FUNCTION__);
  for(ichan=0;ichan<6;ichan++) 
    {
      delay = reg[ichan/3] & TIP_FPDELAY_MASK(ichan)>>(10*(ichan%3));
      if((ichan%4)==0) 
	{
	  printf("\n");
	}
      printf("Chan %2d: %5d   ",ichan+1,delay);
    }
  printf("\n");

  return OK;
}

/**
 * @ingroup Status
 * @brief Return value of buffer length from GTP
 * @return value of buffer length from GTP
 */
unsigned int
tipGetGTPBufferLength(int pflag)
{
  unsigned int rval=0;

  TIPLOCK;
  rval = tipRead(&TIPp->GTPtriggerBufferLength);
  TIPUNLOCK;

  if(pflag)
    printf("%s: 0x%08x\n",__FUNCTION__,rval);

  return rval;
}

/**
 * @ingroup MasterStatus
 * @brief Returns the mask of fiber channels that report a "connected"
 *     status from a TI.
 *
 * @return Fiber Connected Mask
 */
int
tipGetConnectedFiberMask()
{
  int rval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->fiber) & TIP_FIBER_CONNECTED_MASK)>>16;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup MasterStatus
 * @brief Returns the mask of fiber channels that report a "connected"
 *     status from a TI has it's trigger source enabled.
 *
 * @return Trigger Source Enabled Mask
 */
int
tipGetTrigSrcEnabledFiberMask()
{
  int rval=0;
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!tipMaster)
    {
      printf("%s: ERROR: TI is not the TI Master.\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = (tipRead(&TIPp->fiber) & TIP_FIBER_TRIGSRC_ENABLED_MASK)>>24;
  TIPUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Enable the readout fifo in BAR0 for block data readout, instead of DMA.
 * @return OK if successful, otherwise ERROR
 */
int
tipEnableFifo()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  /* Disable DMA readout */
  tipWrite(&TIPp->vmeControl, 
	   tipRead(&TIPp->vmeControl) &~TIP_VMECONTROL_DMASETTING_MASK);

  /* Enable FIFO */
  tipWrite(&TIPp->rocEnable, TIP_ROCENABLE_FIFO_ENABLE);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Configure the Direct Memory Access (DMA) for the TI
 *
 * @param packet_size TLP Maximum Packet Size
 *   1 - 128 B
 *   2 - 256 B
 *   4 - 512 B
 *
 * @param adr_mode TLP Address Mode
 *   0 - 32bit/3 header mode
 *   1 - 64bit/4 header mode
 *
 * @param dma_size DMA memory size to allocate
 *   1 - 1 MB
 *   2 - 2 MB
 *   3 - 4 MB
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipDmaConfig(int packet_size, int adr_mode, int dma_size)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (packet_size!=1) && (packet_size!=2) && (packet_size!=4) )
    {
      printf("%s: ERROR: Invalid packet_size (%d)\n",
	     __FUNCTION__,packet_size);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->dmaSetting,
	   (tipRead(&TIPp->dmaSetting) & TIP_DMASETTING_PHYS_ADDR_HI_MASK) |
	   (packet_size<<24) | 
	   (dma_size<<28) |
	   (adr_mode<<31) );

  tipWrite(&TIPp->vmeControl, 
	   tipRead(&TIPp->vmeControl) | TIP_VMECONTROL_BIT22);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the physical memory address for DMA
 *
 * @param phys_addr_lo Low 32 bits of memory address
 *
 * @param phys_addr_hi High 16 bits of memory address
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipDmaSetAddr(unsigned int phys_addr_lo, unsigned int phys_addr_hi)
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  if(phys_addr_hi>0)
    {
      tipWrite(&TIPp->dmaSetting,
	       (tipRead(&TIPp->dmaSetting) & ~TIP_DMASETTING_PHYS_ADDR_HI_MASK) |
	       (phys_addr_hi & TIP_DMASETTING_PHYS_ADDR_HI_MASK));
    }

  tipWrite(&TIPp->dmaAddr, phys_addr_lo);
  TIPUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Show the PCIE status
 *
 * @param pflag Print Flag
 *   !0 - Print out raw registers
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipPCIEStatus(int pflag)
{
  unsigned int dmaSetting, dmaAddr, 
    pcieConfigLink, pcieConfigStatus, pcieConfig, pcieDevConfig;
  unsigned long TIBase=0;

  if(TIPp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  dmaSetting       = tipRead(&TIPp->dmaSetting);
  dmaAddr          = tipRead(&TIPp->dmaAddr);
  pcieConfigLink   = tipRead(&TIPp->pcieConfigLink);
  pcieConfigStatus = tipRead(&TIPp->pcieConfigStatus);
  pcieConfig       = tipRead(&TIPp->pcieConfig);
  pcieDevConfig    = tipRead(&TIPp->pcieDevConfig);
  TIPUNLOCK;

  TIBase = (unsigned long)TIPp;

  printf("\n");
  printf("PCIE STATUS for TIpcie\n");
  printf("--------------------------------------------------------------------------------\n");
  printf("\n");

  if(pflag)
    {
      printf(" Registers (offset):\n");

      printf(" dmaSetting       (0x%04lx) = 0x%08x ", 
	     (unsigned long)&TIPp->dmaSetting - TIBase, dmaSetting);
      printf(" dmaAddr          (0x%04lx) = 0x%08x\n", 
	     (unsigned long)&TIPp->dmaAddr - TIBase, dmaAddr);

      printf(" pcieConfigLink   (0x%04lx) = 0x%08x ", 
	     (unsigned long)&TIPp->pcieConfigLink - TIBase, pcieConfigLink);
      printf(" pcieConfigStatus (0x%04lx) = 0x%08x\n", 
	     (unsigned long)&TIPp->pcieConfigStatus - TIBase, pcieConfigStatus);

      printf(" pcieConfig       (0x%04lx) = 0x%08x ", 
	     (unsigned long)&TIPp->pcieConfig - TIBase, pcieConfig);
      printf(" pcieDevConfig    (0x%04lx) = 0x%08x\n", 
	     (unsigned long)&TIPp->pcieDevConfig - TIBase, pcieDevConfig);
      printf("\n");
    }

  printf("  Physical Memory Address = 0x%04x %08x \n",
	 dmaSetting&TIP_DMASETTING_PHYS_ADDR_HI_MASK,
	 dmaAddr);
  printf("  DMA Size = %d MB\n",
	 ((dmaSetting&TIP_DMASETTING_DMA_SIZE_MASK)>>24)==1?1:
	 ((dmaSetting&TIP_DMASETTING_DMA_SIZE_MASK)>>24)==2?2:
	 ((dmaSetting&TIP_DMASETTING_DMA_SIZE_MASK)>>24)==3?4:0);
  printf("  TLP:  Address Mode = %s   Packet Size = %d\n",
	 (dmaSetting&TIP_DMASETTING_ADDR_MODE_MASK)?
	 "64 bit / 4 header":
	 "32 bit / 3 header",
	 ((dmaSetting&TIP_DMASETTING_MAX_PACKET_SIZE_MASK)>>28)==1?128:
	 ((dmaSetting&TIP_DMASETTING_MAX_PACKET_SIZE_MASK)>>28)==2?256:
	 ((dmaSetting&TIP_DMASETTING_MAX_PACKET_SIZE_MASK)>>28)==4?512:0);

  printf("\n");
  
  printf("--------------------------------------------------------------------------------\n");
  printf("\n\n");

  return OK;
}

#ifdef NOTDONEYET

/*************************************************************
 Library Interrupt/Polling routines
*************************************************************/

/*******************************************************************************
 *
 *  tiInt
 *  - Default interrupt handler
 *    Handles the TI interrupt.  Calls a user defined routine,
 *    if it was connected with tiIntConnect()
 *    
 */
static void
tiInt(void)
{
  tiIntCount++;

  INTLOCK;

  if (tiIntRoutine != NULL)	/* call user routine */
    (*tiIntRoutine) (tiIntArg);

  /* Acknowledge trigger */
  if(tiDoAck==1)
    {
      tiIntAck();
    }
  INTUNLOCK;

}
#endif /* NOTDONEYET */

#ifdef WAITFORDATA
static int
tipWaitForData()
{
  int iwait=0, maxwait=0;
  volatile unsigned int data=0;

  if(TIPpd==NULL)
    TIPpd = (volatile unsigned int*)tipMapInfo.map_addr;


  tipWrite(&TIPp->vmeControl,1<<22);
  /* data = *TIPpd; */
  /* while((data==0) && (iwait<maxwait)) */
  /*   { */
      usleep(10);
  /*     data = *TIPpd; */
  /*     iwait++; */
  /*   } */
  tipWrite(&TIPp->vmeControl,0);

  /* if(*TIPpd==0) */
  /*   return 0; */
    
  return 1;
}
#endif

/*******************************************************************************
 *
 *  tipPoll
 *  - Default Polling Server Thread
 *    Handles the polling of latched triggers.  Calls a user
 *    defined routine if was connected with tipIntConnect.
 *
 */
static void
tipPoll(void)
{
  int tidata;
  int policy=0;
  struct sched_param sp;
/* #define DO_CPUAFFINITY */
#ifdef DO_CPUAFFINITY
  int j;
  cpu_set_t testCPU;

  if (pthread_getaffinity_np(pthread_self(), sizeof(testCPU), &testCPU) <0) 
    {
      perror("pthread_getaffinity_np");
    }
  printf("tipPoll: CPUset = ");
  for (j = 0; j < CPU_SETSIZE; j++)
    if (CPU_ISSET(j, &testCPU))
      printf(" %d", j);
  printf("\n");

  CPU_ZERO(&testCPU);
  CPU_SET(1,&testCPU);
  if (pthread_setaffinity_np(pthread_self(),sizeof(testCPU), &testCPU) <0) 
    {
      perror("pthread_setaffinity_np");
    }
  if (pthread_getaffinity_np(pthread_self(), sizeof(testCPU), &testCPU) <0) 
    {
      perror("pthread_getaffinity_np");
    }

  printf("tipPoll: CPUset = ");
  for (j = 0; j < CPU_SETSIZE; j++)
    if (CPU_ISSET(j, &testCPU))
      printf(" %d", j);

  printf("\n");


#endif
  
  /* Set scheduler and priority for this thread */
  policy=SCHED_OTHER;
  sp.sched_priority=40;
  printf("%s: Entering polling loop...\n",__FUNCTION__);
  pthread_setschedparam(pthread_self(),policy,&sp);
  pthread_getschedparam(pthread_self(),&policy,&sp);
  printf ("%s: INFO: Running at %s/%d\n",__FUNCTION__,
	  (policy == SCHED_FIFO ? "FIFO"
	   : (policy == SCHED_RR ? "RR"
	      : (policy == SCHED_OTHER ? "OTHER"
		 : "unknown"))), sp.sched_priority);  
  prctl(PR_SET_NAME,"tipPoll");

  while(1) 
    {
      usleep(1);
      pthread_testcancel();

      /* If still need Ack, don't test the Trigger Status */
      if(tipNeedAck>0) 
	{
	  continue;
	}

      tidata = 0;
	  
      tidata = tipBReady();
      if(tidata == ERROR) 
	{
	  printf("%s: ERROR: tiIntPoll returned ERROR.\n",__FUNCTION__);
	  break;
	}

      if(tidata && tipIntRunning)
	{
	  INTLOCK; 
	  tipDaqCount = tidata;
	  tipIntCount++;

#ifdef WAITFORDATA
	  int data = tipWaitForData();

	  if(data==ERROR)
	    {
	      printf("%s: tipWaitForData() returned ERROR.\n",__FUNCTION__);
	      break;
	    }
	  else if(data>0)
	    {
	      /* printf("%s: Data ready\n",__FUNCTION__); */
	    }
	  else
	    {
	      printf("**************************************************\n");
	      printf("%s: Data NOT ready\n",__FUNCTION__);
	      printf("**************************************************\n");
	    }
#endif /* WAITFORDATA */
	  
	  if (tipIntRoutine != NULL)	/* call user routine */
	    {
	      (*tipIntRoutine) (tipIntArg);
	    }
	
	  /* Write to TI to Acknowledge Interrupt */	  
	  if(tipDoAck==1) 
	    {
	      tipIntAck();
	    }


	  INTUNLOCK;
	}
    
    }
  printf("%s: Read ERROR: Exiting Thread\n",__FUNCTION__);
  pthread_exit(0);

}

/*******************************************************************************
 *
 * tipDoLibraryPollingThread - Set the decision on whether or not the
 *      TIR library should perform the trigger polling via thread.
 *
 *   choice:   0 - No Thread Polling
 *             1 - Library Thread Polling (started with tirIntEnable)
 * 
 *
 * RETURNS: OK, or ERROR .
 */

int
tipDoLibraryPollingThread(int choice)
{
  if(choice)
    tipDoIntPolling=1;
  else
    tipDoIntPolling=0;
      
  return tipDoIntPolling;
}

/*******************************************************************************
 *
 *  tipStartPollingThread
 *  - Routine that launches tiPoll in its own thread 
 *
 */
static void
tipStartPollingThread(void)
{
  int ti_status;

  ti_status = 
    pthread_create(&tippollthread,
		   NULL,
		   (void*(*)(void *)) tipPoll,
		   (void *)NULL);
  if(ti_status!=0) 
    {						
      printf("%s: ERROR: TI Polling Thread could not be started.\n",
	     __FUNCTION__);	
      printf("\t pthread_create returned: %d\n",ti_status);
    }

}

/**
 * @ingroup IntPoll
 * @brief Connect a user routine to the TI Interrupt or
 *    latched trigger, if polling.
 *
 * @param vector VME Interrupt Vector
 * @param routine Routine to call if block is available
 * @param arg argument to pass to routine
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipIntConnect(unsigned int vector, VOIDFUNCPTR routine, unsigned int arg)
{
  int status;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return(ERROR);
    }


  tipIntCount = 0;
  tipAckCount = 0;
  tipDoAck = 1;

  /* Set Vector and Level */
  if((vector < 0xFF)&&(vector > 0x40)) 
    {
      tipIntVec = vector;
    }
  else
    {
      tipIntVec = TIP_INT_VEC;
    }

  TIPLOCK;
  tipWrite(&TIPp->intsetup, (tipIntLevel<<8) | tipIntVec );
  TIPUNLOCK;

  status=0;

  switch (tipReadoutMode)
    {
    case TIP_READOUT_TS_POLL:
    case TIP_READOUT_EXT_POLL:
      break;

    case TIP_READOUT_TS_INT:
    case TIP_READOUT_EXT_INT:
#ifdef NOTDONEYET
      status = vmeIntConnect (tiIntVec, tiIntLevel,
			      tiInt,arg);
      if (status != OK) 
	{
	  printf("%s: vmeIntConnect failed with status = 0x%08x\n",
		 __FUNCTION__,status);
	  return(ERROR);
	}
      break;
#endif
    default:
      printf("%s: ERROR: TI Mode not defined (%d)\n",
	     __FUNCTION__,tipReadoutMode);
      return ERROR;
    }

  printf("%s: INFO: Interrupt Vector = 0x%x  Level = %d\n",
	 __FUNCTION__,tipIntVec,tipIntLevel);

  if(routine) 
    {
      tipIntRoutine = routine;
      tipIntArg = arg;
    }
  else
    {
      tipIntRoutine = NULL;
      tipIntArg = 0;
    }

  return(OK);

}

/**
 * @ingroup IntPoll
 * @brief Disable interrupts or kill the polling service thread
 *
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipIntDisconnect()
{
  int status;
  void *res;

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tipIntRunning) 
    {
      printf("%s: ERROR: TI is Enabled - Call tipIntDisable() first\n",
	     __FUNCTION__);
      return ERROR;
    }

  INTLOCK;

  status=0;

  switch (tipReadoutMode) 
    {
    case TIP_READOUT_TS_POLL:
    case TIP_READOUT_EXT_POLL:
      {
	if(tippollthread)
	  {
	    if(pthread_cancel(tippollthread)<0) 
	      perror("pthread_cancel");
	    if(pthread_join(tippollthread,&res)<0)
	      perror("pthread_join");
	    if (res == PTHREAD_CANCELED)
	      printf("%s: Polling thread canceled\n",__FUNCTION__);
	    else
	      printf("%s: ERROR: Polling thread NOT canceled\n",__FUNCTION__);
	  }
      }
      break;
    case TIP_READOUT_TS_INT:
    case TIP_READOUT_EXT_INT:
#ifdef NOTDONEYET
      status = vmeIntDisconnect(tiIntLevel);
      if (status != OK) 
	{
	  printf("vmeIntDisconnect failed\n");
	}
      break;
#endif
    default:
      break;
    }

  INTUNLOCK;

  printf("%s: Disconnected\n",__FUNCTION__);

  return OK;
  
}

/**
 * @ingroup IntPoll
 * @brief Connect a user routine to be executed instead of the default 
 *  TI interrupt/trigger latching acknowledge prescription
 *
 * @param routine Routine to call 
 * @param arg argument to pass to routine
 * @return OK if successful, otherwise ERROR
 */
int
tipAckConnect(VOIDFUNCPTR routine, unsigned int arg)
{
  if(routine)
    {
      tipAckRoutine = routine;
      tipAckArg = arg;
    }
  else
    {
      printf("%s: WARN: routine undefined.\n",__FUNCTION__);
      tipAckRoutine = NULL;
      tipAckArg = 0;
      return ERROR;
    }
  return OK;
}

/**
 * @ingroup IntPoll
 * @brief Acknowledge an interrupt or latched trigger.  This "should" effectively 
 *  release the "Busy" state of the TI.
 *
 *  Execute a user defined routine, if it is defined.  Otherwise, use
 *  a default prescription.
 */
void
tipIntAck()
{
  int resetbits=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }
  
  if (tipAckRoutine != NULL)
    {
      /* Execute user defined Acknowlege, if it was defined */
      TIPLOCK;
      (*tipAckRoutine) (tipAckArg);
      TIPUNLOCK;
    }
  else
    {
      TIPLOCK;
      tipDoAck = 1;
      tipAckCount++;
      resetbits = TIP_RESET_BUSYACK;

      if(!tipReadoutEnabled)
	{
	  /* Readout Acknowledge and decrease the number of available blocks by 1 */
	  resetbits |= TIP_RESET_BLOCK_READOUT;
	}
      
      if(tipDoSyncResetRequest)
	{
	  resetbits |= TIP_RESET_SYNCRESET_REQUEST;
	  tipDoSyncResetRequest=0;
	}

      tipWrite(&TIPp->reset, resetbits);

      tipNReadoutEvents = 0;
      TIPUNLOCK;
    }


}

/**
 * @ingroup IntPoll
 * @brief Enable interrupts or latching triggers (depending on set TI mode)
 *  
 * @param iflag if = 1, trigger counter will be reset
 *
 * @return OK if successful, otherwise ERROR
 */
int
tipIntEnable(int iflag)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return(-1);
    }

  TIPLOCK;
  if(iflag == 1)
    {
      tipIntCount = 0;
      tipAckCount = 0;
    }

  tipIntRunning = 1;
  tipDoAck      = 1;
  tipNeedAck    = 0;

  switch (tipReadoutMode)
    {
    case TIP_READOUT_TS_POLL:
    case TIP_READOUT_EXT_POLL:
      if(tipDoIntPolling)
	tipStartPollingThread();
      break;

    case TIP_READOUT_TS_INT:
    case TIP_READOUT_EXT_INT:
#ifdef NOTDONEYET
      printf("%s: ******* ENABLE INTERRUPTS *******\n",__FUNCTION__);
      tipWrite(&TIPp->intsetup,
	       tipRead(&TIPp->intsetup) | TIP_INTSETUP_ENABLE );
      break;
#endif
    default:
      tipIntRunning = 0;
      printf("%s: ERROR: TI Readout Mode not defined %d\n",
	     __FUNCTION__,tipReadoutMode);
      TIPUNLOCK;
      return(ERROR);
      
    }

  tipWrite(&TIPp->runningMode,0x71);
  TIPUNLOCK; /* Locks performed in tipEnableTriggerSource() */

  usleep(300000);
  tipEnableTriggerSource();

  return(OK);

}

/**
 * @ingroup IntPoll
 * @brief Disable interrupts or latching triggers
 *
*/
void 
tipIntDisable()
{

  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return;
    }

  tipDisableTriggerSource(1);

  TIPLOCK;
  tipWrite(&TIPp->intsetup,
	     tipRead(&TIPp->intsetup) & ~(TIP_INTSETUP_ENABLE));
  tipWrite(&TIPp->runningMode,0x0);
  tipIntRunning = 0;
  TIPUNLOCK;
}

/**
 * @ingroup Status
 * @brief Return current readout count
 */
unsigned int
tipGetIntCount()
{
  unsigned int rval=0;

  TIPLOCK;
  rval = tipIntCount;
  TIPUNLOCK;

  return(rval);
}

/**
 * @ingroup Status
 * @brief Return current acknowledge count
 */
unsigned int
tipGetAckCount()
{
  unsigned int rval=0;

  TIPLOCK;
  rval = tipAckCount;
  TIPUNLOCK;

  return(rval);
}

#ifdef NOTSUPPORTED
/* Module TI Routines */
int
tipRocEnable(int roc)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((roc<1) || (roc>8))
    {
      printf("%s: ERROR: Invalid roc (%d)\n",
	     __FUNCTION__,roc);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->rocEnable, (tipRead(&TIPp->rocEnable) & TIP_ROCENABLE_MASK) | 
	     TIP_ROCENABLE_ROC(roc-1));
  TIPUNLOCK;

  return OK;
}

int
tipRocEnableMask(int rocmask)
{
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(rocmask>TIP_ROCENABLE_MASK)
    {
      printf("%s: ERROR: Invalid rocmask (0x%x)\n",
	     __FUNCTION__,rocmask);
      return ERROR;
    }

  TIPLOCK;
  tipWrite(&TIPp->rocEnable, rocmask);
  TIPUNLOCK;

  return OK;
}

int
tipGetRocEnableMask()
{
  int rval=0;
  if(TIPp==NULL) 
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TIPLOCK;
  rval = tipRead(&TIPp->rocEnable) & TIP_ROCENABLE_MASK;
  TIPUNLOCK;

  return rval;
}
#endif /* NOTSUPPORTED */


static int
tipRW(PCI_IOCTL_INFO info)
{
  // printf(" command_type = %d\n",info.command_type);
  //  printf("   mem_region = %d\n",info.mem_region);
  //printf("         nreg = %d\n",info.nreg);
  //printf("       reg[0] = %d\n",info.reg[0]);
  // printf("     value[0] = 0x%x\n\n",info.value[0]);

  return ioctl(tipFD, TIPCIE_IOC_RW, &info);
}

unsigned int
tipRead(volatile unsigned int *reg)
{
  unsigned int value=0;
#ifdef OLDWAY
  int stat=0;

  int areg[1];

  areg[0] = (reg - &TIPp->boardID)<<2;

  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_READ,
      .mem_region   = 0,
      .nreg         = 1,
      .reg          = (volatile unsigned int *)&areg,
      .value        = &value
    };

  stat = tipRW(info);

  if(stat!=OK)
    return ERROR;
#else
  /* usleep(1); */
  value = *reg;
#endif

  return value;
}

int
tipWrite(volatile unsigned int *reg, unsigned int value)
{
  int stat=0;
#ifdef OLDWAY
  int areg[1];

  areg[0] = (reg - &TIPp->boardID)<<2;

  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_WRITE,
      .mem_region   = 0,
      .nreg         = 1,
      .reg          = (volatile unsigned int *)&areg,
      .value        = &value
    };

  stat = tipRW(info);
#else
  *reg = value;
#endif

  return stat;
}

unsigned int
tipJTAGRead(unsigned int reg)
{
  unsigned int value=0;
#ifdef OLDWAY2
  int stat=0;

  int areg[1];

  areg[0] = reg;

  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_READ,
      .mem_region   = 1,
      .nreg         = 1,
      .reg          = (volatile unsigned int *)&areg,
      .value        = &value
    };

  stat = tipRW(info);

  if(stat!=OK)
    return ERROR;
#else
  value = *(TIPpj+(reg>>2));
#endif
  return value;
}

int
tipJTAGWrite(unsigned int reg, unsigned int value)
{
  int stat=0;
#ifdef OLDWAY2
  int areg[1];

  areg[0] = reg;

  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_WRITE,
      .mem_region   = 1,
      .nreg         = 1,
      .reg          = (volatile unsigned int *)&areg,
      .value        = &value
    };

  stat = tipRW(info);
#else
  *(TIPpj+(reg>>2)) = value;
#endif
  return stat;
}

unsigned int
tipDataRead(unsigned int reg)
{
  unsigned int value=0;
  int stat=0;

  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_READ,
      .mem_region   = 2,
      .nreg         = 1,
      .reg          = (unsigned int *)&reg,
      .value        = &value
    };

  stat = tipRW(info);

  if(stat!=OK)
    return ERROR;

  return value;
}


int
tipReadBlock2(int bar, unsigned int *reg, unsigned int *value, int nreg)
{
  int stat=0;
  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_READ,
      .mem_region   = bar,
      .nreg         = nreg,
      .reg          = reg,
      .value        = value
    };

  stat = tipRW(info);

  return stat;
}

int
tipWriteBlock(int bar, unsigned int *reg, unsigned int *value, int nreg)
{
  int stat=0;
  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_WRITE,
      .mem_region   = bar,
      .nreg         = nreg,
      .reg          = reg,
      .value        = value
    };

  stat = tipRW(info);

  return stat;
}

int
tipOpen()
{
#ifdef ALLOCMEM
  unsigned int  size=0x200000;
  unsigned long phys_addr=0;
#endif
  off_t         dev_base;
  unsigned int  bars[3]={0,0,0};

  if(tipFD>0)
    {
      printf("%s: ERROR: TIpcie already opened.\n",
	     __FUNCTION__);
      return ERROR;
    }

  tipFD = open("/dev/TIpcie",O_RDWR);

  if(tipFD<0)
    {
      perror("tipOpen: ERROR");
      return ERROR;
    }
#ifdef ALLOCMEM
  tipMapInfo = tipAllocDmaMemory(size, &phys_addr);
#ifdef DEBUGMEM
  printf("      dmaHdl = 0x%llx\n", (uint64_t)tipMapInfo.dmaHdl);
  printf("   phys_addr = 0x%llx\n", (uint64_t)phys_addr);
  printf("    map_addr = 0x%llx\n", (uint64_t)tipMapInfo.map_addr);
#endif
  tipDmaAddrBase = phys_addr;
#endif /* ALLOCMEM */

  if(tipGetPciBar((unsigned int *)&bars)!=OK)
    {
      printf("%s: Failed to get PCI bars\n",__FUNCTION__);
      return ERROR;
    }

  dev_base = bars[0];

  tipMappedBase = mmap(0, sizeof(struct TIPCIE_RegStruct), 
		     PROT_READ|PROT_WRITE, MAP_SHARED, tipFD, dev_base);
  if (tipMappedBase == MAP_FAILED)
    {
      perror("mmap");
      return ERROR;
    }

  TIPp = (volatile struct TIPCIE_RegStruct *)tipMappedBase;

  dev_base = bars[1];

  tipJTAGMappedBase = mmap(0, 0x1000, 
		     PROT_READ|PROT_WRITE, MAP_SHARED, tipFD, dev_base);
  if (tipJTAGMappedBase == MAP_FAILED)
    {
      perror("mmap");
      return ERROR;
    }
  
  TIPpj = (volatile unsigned int *)tipJTAGMappedBase;

  return OK;
}

int
tipClose()
{
  if(TIPp==NULL)
    {
      printf("%s: ERROR: Invalid TIP File Descriptor\n",
	     __FUNCTION__);
      return ERROR;
    }

#ifdef ALLOCMEM
  tipFreeDmaMemory(tipMapInfo);
#endif /* ALLOCMEM */

  if(munmap(tipJTAGMappedBase,0x1000)<0)
     perror("munmap");

  if(munmap(tipMappedBase,sizeof(struct TIPCIE_RegStruct))<0)
     perror("munmap");

  
  close(tipFD);
  return OK;
}

static int
tipGetPciBar(unsigned int *value)
{
  int stat=0;
  unsigned int reg[3]={1,2,3};
  int nreg = 3;
  PCI_IOCTL_INFO info =
    {
      .command_type = TIPCIE_STAT,
      .mem_region   = 0,
      .nreg         = nreg,
      .reg          = reg,
      .value        = value
    };

  stat = tipRW(info);

  return stat;
}

#ifdef ALLOCMEM
static int
tipDmaMem(DMA_BUF_INFO *info)
{
  int rval=0;

  rval = ioctl(tipFD, TIPCIE_IOC_MEM, info);

#ifdef DEBUGMEM
  printf("   command_type = %d\n",(int)info->command_type);
  printf(" dma_osspec_hdl = %#lx\n",(uint64_t)info->dma_osspec_hdl);
  printf("      phys_addr = %#lx\n",(uint64_t)info->phys_addr);
  printf("      virt_addr = %#lx\n",(uint64_t)info->virt_addr);
  printf("           size = %d\n",(int)info->size);
#endif
  return rval;
}

static DMA_MAP_INFO
tipAllocDmaMemory(int size, unsigned long *phys_addr)
{
  int stat=0;
  DMA_MAP_INFO rval;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = 0x123456789ULL,
      .command_type   = TIPCIE_MEM_ALLOC,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = size
    };
  volatile char *tmp_addr;

  stat = tipDmaMem(&info);

  *phys_addr = info.phys_addr;

  /* Do an mmap here */
  tmp_addr = (volatile char *)mmap(0, size, PROT_READ | PROT_WRITE, 
		  MAP_SHARED, tipFD, info.phys_addr);

  if(tmp_addr == (void*) -1)
    {
      printf("%s: ERROR: mmap failed\n",
	     __FUNCTION__);
    }

  rval.dmaHdl = info.dma_osspec_hdl;
  rval.map_addr = (volatile unsigned long)tmp_addr;
  rval.size = size;

  return rval;
}

static int 
tipFreeDmaMemory(DMA_MAP_INFO mapInfo)
{
  int stat=0;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = mapInfo.dmaHdl,
      .command_type   = TIPCIE_MEM_FREE,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = 0
    };

  stat = tipDmaMem(&info);

  /* Do an munmap here */
  if(mapInfo.map_addr == -1)
    {
      /* Do nothing here */
    }
  else
    {
      munmap((char*)mapInfo.map_addr, mapInfo.size);
    }

  return stat;
}
#endif /* ALLOCMEM */

