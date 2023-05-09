/*************************************************************************
 *
 *  TIpcie_master_list.c - Library of routines for readout of
 *                         events using user defined routine in CODA 3.0
 *
 */

/* EXTernal trigger source (e.g. front panel ECL input), POLL for available data */
#define TI_MASTER
#define TI_READOUT TIP_READOUT_EXT_POLL

/* Measured longest fiber length in system */
#define FIBER_LATENCY_OFFSET 0x10

/* Event Buffer definitions */
#define MAX_EVENT_POOL     100
#define MAX_EVENT_LENGTH   1152*32      /* Size in Bytes */

#include "TIpcie_source.h" /* source required for CODA */
#include "TIpcieConfig.h"

const char *configFile = "/daqfs/home/moffit/work/TIpcie/cfg/master.ini";

/* Global Flag for debug printing */
int usrDebugFlag=0;

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  /* Configure TIpcie */
  tiConfigInitGlobals();

  tiConfig(configFile);

  tiConfigFree();


  tipStatus(1);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  usrDebugFlag=0;

  tipStatus(1);
  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{

  tipStatus(1);

  /* Last thing done in Go */
}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  tipStatus(1);

  printf("rocEnd: Ended after %d events\n",*(rol->nevents));

}

/****************************************
 *  POLLING ROUTINE
 ****************************************/
int
rocPoll()
{
  extern int tipDaqCount;
  extern int tipIntCount;

  static int count = 0;
  int tidata = 0, rval = 0;

  tidata = tipBReady();
  if(tidata == ERROR)
    {
      printf("%s: ERROR: tipBReady returned ERROR.\n",__func__);
      rval = ERROR;
    }

  if(tidata > 0)
    {
      tipDaqCount = tidata;
      tipIntCount++;
      rval = 1;
    }

  return rval;
}


/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int evno, int evtype)
{
  int32_t dCnt = 0;

  CEOPEN(ROCID, BT_BANK, blockLevel);
  dCnt = tipReadTriggerBlock((volatile unsigned int *)rol->dabufp);

  if(dCnt<0)
    {
      printf("**************************************************\n");
      printf("No data or error.  dCnt = %d\n",dCnt);
      printf("**************************************************\n");
    }
  else
    {
      rol->dabufp += dCnt;
    }


  CECLOSE;

}

void
rocTrigger_done()
{
  extern int tipDoAck;

  if(tipDoAck==1)
    {
      tipIntAck();
    }
}

void
rocReset()
{

}

void
rocLoad()
{
}

void
rocCleanup()
{

}

int
tsLive(int sflag)
{
  return tipLive(sflag);
}

/*
  Local Variables:
  compile-command: "make -k -B TIpcie_master_list.so"
  End:
 */
