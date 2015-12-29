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

  dCnt = tipReadBlock((volatile unsigned int *)&data,8,0);
  /* dCnt = tipReadTriggerBlock((volatile unsigned int *)&data); */

  if(dCnt!=8)
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

#define READOUT
#ifdef READOUT
  if((tiIntCount%printout==0) || (dCnt!=8))
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

  if((dataCheck!=OK) || (dCnt!=8))
    {
      getchar();
    }

}


int 
main(int argc, char *argv[]) 
{

  int stat;

  printf("\nJLAB TI Tests\n");
  printf("----------------------------\n");

/*   remexSetCmsgServer("dafarm28"); */
/*   remexInit(NULL,1); */

  /* printf("Size of DMANODE    = %d (0x%x)\n",sizeof(DMANODE),sizeof(DMANODE)); */
  /* printf("Size of DMA_MEM_ID = %d (0x%x)\n",sizeof(DMA_MEM_ID),sizeof(DMA_MEM_ID)); */

  tipOpen();

  /* Set the TI structure pointer */
  tipInit(TIP_READOUT_EXT_POLL,0);
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
    
  /* tipSetTriggerHoldoff(1,4,0); */
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

  /*     tiSetTriggerSource(TI_TRIGGER_TSINPUTS); */
  tipSetTriggerSource(TIP_TRIGGER_PULSER);
  tipEnableTSInput(0x1);

  /*     tiSetFPInput(0x0); */
  /*     tiSetGenInput(0xffff); */
  /*     tiSetGTPInput(0x0); */

  tipSetBusySource(TIP_BUSY_LOOPBACK ,1);

  tipSetBlockBufferLevel(1);

/*   tiSetFiberDelay(1,2); */
/*   tiSetSyncDelayWidth(1,0x3f,1); */
    
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

  printf("Hit enter to start triggers\n");
  getchar();

  tipIntEnable(0);
  tipStatus(1);
  tipPCIEStatus(1);
#define SOFTTRIG
#ifdef SOFTTRIG
  tipSetRandomTrigger(1,0x7);
  /* taskDelay(10); */
  /* tipSoftTrig(1,1,0xffff/2,1); */
#endif

  printf("Hit any key to Disable TID and exit.\n");
  getchar();
  tipStatus(1);
  tipPCIEStatus(1);

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

