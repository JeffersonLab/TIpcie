/*
 * File:
 *    tipReadoutTest
 *
 * Description:
 *    Test TIpcie readout with Linux Driver
 *    and TI library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "TIpcieLib.h"
/* #include "remexLib.h" */

#define BLOCKLEVEL 0x1

#define DO_READOUT
#define SOFTTRIG

#define USEDMA 0

/* Interrupt Service routine */
void
mytiISR(int arg)
{
  volatile unsigned short reg;
  int dCnt, len=0,idata;
  int tibready=0, timeout=0;
  int printout = 1000;
  int dataCheck=0;
  volatile unsigned int data[120];
  int DMA=0;

  unsigned int tiIntCount = tipGetIntCount();

#ifdef DO_READOUT

#ifdef DOINT
  tibready = tipBReady();
  if(tibready==ERROR)
    {
      printf("%s: ERROR: tiIntPoll returned ERROR.\n",__FUNCTION__);
      return;
    }
  if(tibready==0 && timeout<100)
    {
      printf("NOT READY!\n");
      tibready=tipBReady();
      timeout++;
    }

  if(timeout>=100)
    {
      printf("TIMEOUT!\n");
      return;
    }
#endif

  if(USEDMA)
    DMA=1;

  dCnt = tipReadBlock((volatile unsigned int *)&data,32,DMA);
  /* dCnt = tipReadTriggerBlock((volatile unsigned int *)&data); */

  if(dCnt<0)
    {
      printf("**************************************************\n");
      printf("No data or error.  dCnt = %d\n",dCnt);
      printf("**************************************************\n");
      dataCheck=ERROR;
    }
  else
    {
      /* dataCheck = tiCheckTriggerBlock(data); */
    }

/* #define READOUT */
#ifdef READOUT
  if((tiIntCount%printout==0));
    {
      printf("Received %d triggers...\n",
	     tiIntCount);

      len = dCnt;
      
      for(idata=0;idata<(len);idata++)
	{
	  if((idata%4)==0) printf("\n\t");
	  printf("  0x%08x ",(unsigned int)(data[idata]));
	}
      printf("\n\n");
    }
#endif

#else /* DO_READOUT */

#endif /* DO_READOUT */
  if(tiIntCount%printout==0)
    printf("intCount = %d\n",tiIntCount );

  if(tiIntCount%(printout*10)==0)
    tipPrintTempVolt();


  if((dataCheck!=OK))
    {
      getchar();
    }

  if(tipGetSyncEventReceived() && !tipGetSyncEventFlag())
    {
      printf("**** Sync Event Received (%8d) ****\n",tiIntCount);
    }

  if(tipGetSyncEventFlag())
      printf("**** Sync Event          (%8d) ****\n",tiIntCount);

}


int 
main(int argc, char *argv[]) 
{

  int stat;
  int DMA=0;

  printf("\nJLAB TI Tests\n");
  printf("----------------------------\n");

/*   remexSetCmsgServer("dafarm28"); */
/*   remexInit(NULL,1); */

  /* printf("Size of DMANODE    = %d (0x%x)\n",sizeof(DMANODE),sizeof(DMANODE)); */
  /* printf("Size of DMA_MEM_ID = %d (0x%x)\n",sizeof(DMA_MEM_ID),sizeof(DMA_MEM_ID)); */

  tipOpen();

  /* Set the TI structure pointer */
  if(USEDMA)
    DMA = TIP_INIT_USE_DMA;

  tipInit(TIP_READOUT_EXT_POLL,DMA);
  tipCheckAddresses();

  tipDefinePulserEventType(0xAA,0xCD);

  /* tipSetSyncEventInterval(10); */

  /* tipSetEventFormat(3); */

  /* char mySN[20]; */
  /* printf("0x%08x\n",tiGetSerialNumber((char **)&mySN)); */
  /* printf("mySN = %s\n",mySN); */

#ifndef DO_READOUT
  tipDisableDataReadout(0);
#endif

  tipLoadTriggerTable(0);
    
  tipSetTriggerHoldoff(1,1,2);
  /* tipSetTriggerHoldoff(2,4,0); */

  tipSetPrescale(0);
  tipSetBlockLevel(BLOCKLEVEL);

  stat = tipIntConnect(TIP_INT_VEC, mytiISR, 0);
  if (stat != OK) 
    {
      printf("ERROR: tiIntConnect failed \n");
      goto CLOSE;
    } 
  else 
    {
      printf("INFO: Attached TI Interrupt\n");
    }

#ifdef SOFTTRIG
  tipSetTriggerSource(TIP_TRIGGER_PULSER);
#else
  tipSetTriggerSource(TIP_TRIGGER_TSINPUTS);
#endif
  tipEnableTSInput(0xf);

  /*     tiSetFPInput(0x0); */
  /*     tiSetGenInput(0xffff); */
  /*     tiSetGTPInput(0x0); */

  tipSetBusySource(TIP_BUSY_LOOPBACK | TIP_BUSY_FP ,1);

  tipSetBlockBufferLevel(40);

/*   tiSetFiberDelay(1,2); */
/*   tiSetSyncDelayWidth(1,0x3f,1); */
  tipSetSyncEventInterval(1000);
  
  tipSetBlockLimit(0);

  printf("Hit enter to reset stuff\n");
  getchar();

  /* tipClockReset(); */
  /* usleep(10000); */
  tipTrigLinkReset();
  usleep(10000);

  int again=0;
 AGAIN:
  usleep(10000);
  tipSyncReset(1);

  usleep(10000);
    
  tipStatus(1);
  tipPCIEStatus(1);
  tipPrintTempVolt();

  printf("Hit enter to start triggers\n");
  getchar();

  tipIntEnable(0);
  tipStatus(1);
  tipPCIEStatus(1);
#ifdef SOFTTRIG
  tipSetRandomTrigger(1,0x6);
  /* taskDelay(10); */
  /* tipSoftTrig(1,1,0xffff/2,1); */
#endif

  printf("Hit any key to Disable TID and exit.\n");
  getchar();
  tipStatus(1);
  tipPCIEStatus(1);
  tipPrintTempVolt();

#ifdef SOFTTRIG
  /* No more soft triggers */
  /*     tidSoftTrig(0x0,0x8888,0); */
  tipSoftTrig(1,0,0x700,0);
  tipDisableRandomTrigger();
#endif

  tipIntDisable();

  tipIntDisconnect();

  if(again==1)
    {
      again=0;
      goto AGAIN;
    }


 CLOSE:
  tipClose();
  exit(0);
}

