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


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "TIpcieLib.h"
#include "TIpcieConfig.h"

#define BLOCKLEVEL 0x1

/* Interrupt Service routine */
void
mytiISR(int arg)
{
  int dCnt, idata;
  int printout = 1000;
  int dataCheck=0;
  volatile unsigned int data[120];

  unsigned int tiIntCount = tipGetIntCount();

  int tibready=0, timeout=0;

  dCnt = tipReadBlock((volatile unsigned int *)&data, 32, 0);
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

  if((tiIntCount%printout==0));
    {
      printf("Received %d triggers...\n",
	     tiIntCount);

      for(idata=0; idata<dCnt; idata++)
	{
	  if((idata%4)==0) printf("\n\t");
	  printf("  0x%08x ",(unsigned int)(data[idata]));
	}
      printf("\n\n");
    }

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

  printf("\nJLAB TI Tests\n");
  printf("----------------------------\n");

  tipOpen();

  /* Set the TI structure pointer */

  tipInit(TIP_READOUT_EXT_POLL, 0);
  tipCheckAddresses();

  tiConfigInitGlobals();

  if(argc == 2)
    tiConfig(argv[1]);

  /* tiConfigPrintParameters(); */


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


  tipClockReset();
  usleep(10000);
  tipTrigLinkReset();
  usleep(10000);
  printf("Hit enter for SyncReset\n");
  getchar();

  usleep(10000);
  tipSyncReset(1);

  tipStatus(1);
  tipPCIEStatus(1);
  tipPrintTempVolt();

  printf("Hit enter to start triggers\n");
  getchar();

  tipIntEnable(0);
  tipStatus(1);
  tipPCIEStatus(1);

  tipConfigEnablePulser();

  printf("Hit any key to Disable TID and exit.\n");
  getchar();
  tipStatus(1);
  tipPCIEStatus(1);
  tipPrintTempVolt();


  tipIntDisable();

  tipIntDisconnect();

  tipConfigDisablePulser();

 CLOSE:
  tiConfigFree();

  tipClose();
  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k tipConfigReadout "
  End:
*/
