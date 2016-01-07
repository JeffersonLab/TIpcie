/******************************************************************************
*
* Header file for use General USER defined rols with CODA crl (version 2.0)
* 
*   This file implements use of the JLAB TI (pipeline) Module as a trigger interface
*
*                             Bryan Moffit  December 2012
*
*******************************************************************************/
#ifndef __GEN_ROL__
#define __GEN_ROL__

static int GEN_handlers,GENflag;
static int GEN_isAsync;
static unsigned int *GENPollAddr = NULL;
static unsigned int GENPollMask;
static unsigned int GENPollValue;
static unsigned long GEN_prescale = 1;
static unsigned long GEN_count = 0;


/* Put any global user defined variables needed here for GEN readout */
#include "TIpcieLib.h"
extern int tipDoAck;
extern int tipIntCount;
extern void *tsLiveFunc;
extern int tsLiveCalc;


void
GEN_int_handler()
{
  theIntHandler(GEN_handlers);                   /* Call our handler */
  tipDoAck=0; /* Make sure the library doesn't automatically ACK */
}



/*----------------------------------------------------------------------------
  gen_trigLib.c -- Dummy trigger routines for GENERAL USER based ROLs

 File : gen_trigLib.h

 Routines:
           void gentriglink();       link interrupt with trigger
	   void gentenable();        enable trigger
	   void gentdisable();       disable trigger
	   char genttype();          return trigger type 
	   int  genttest();          test for trigger  (POLL Routine)
------------------------------------------------------------------------------*/


static void
gentriglink(int code, VOIDFUNCPTR isr)
{
  int stat=0;

  tipIntConnect(0,isr,0);
}

static void 
gentenable(int code, int card)
{
  int iflag = 1; /* Clear Interrupt scalers */
  int lockkey;

  tsLiveCalc=1;
  tsLiveFunc = tipLive;

  if(GEN_isAsync==0)
    {
      GENflag = 1;
      tipDoLibraryPollingThread(0); /* Turn off library polling */
    }
  
  tipIntEnable(1); 
}

static void 
gentdisable(int code, int card)
{

  if(GEN_isAsync==0)
    {
      GENflag = 0;
    }
  tipIntDisable();
  tipIntDisconnect();

  tsLiveCalc=0;
  tsLiveFunc = NULL;

}

static unsigned int
genttype(int code)
{
  unsigned int tt=0;

  tt = 1;

  return(tt);
}

static int 
genttest(int code)
{
  unsigned int ret=0;

  usleep(1);
  ret = tipBReady();
  if(ret==-1)
    {
      printf("%s: ERROR: tipBReady returned ERROR\n",
	     __FUNCTION__);

    }
  if(ret)
    {
      tipIntCount++;
    }
  
  return ret;
}

static inline void 
gentack(int code, unsigned int intMask)
{
    {
      tipIntAck();
    }
}


/* Define CODA readout list specific Macro routines/definitions */

#define GEN_TEST  genttest

#define GEN_INIT {				\
    GEN_handlers =0;				\
    GEN_isAsync = 0;				\
    GENflag = 0;}

#define GEN_ASYNC(code,id)  {printf("linking async GEN trigger to id %d \n",id); \
    GEN_handlers = (id);GEN_isAsync = 1;gentriglink(code,GEN_int_handler);}

#define GEN_SYNC(code,id)   {printf("linking sync GEN trigger to id %d \n",id); \
    GEN_handlers = (id);GEN_isAsync = 0;}

#define GEN_SETA(code) GENflag = code;

#define GEN_SETS(code) GENflag = code;

#define GEN_ENA(code,val) gentenable(code, val);

#define GEN_DIS(code,val) gentdisable(code, val);

#define GEN_CLRS(code) GENflag = 0;

#define GEN_GETID(code) GEN_handlers

#define GEN_TTYPE genttype

#define GEN_START(val)	 {;}

#define GEN_STOP(val)	 {;}

#define GEN_ENCODE(code) (code)

#define GEN_ACK(code,val)   gentack(code,val);

__attribute__((destructor)) void end (void)
{
  static int ended=0;

  if(ended==0)
    {
      printf("ROC Cleanup\n");

      tsLiveCalc=0;
      tsLiveFunc = NULL;
      
      tipClose();
      ended=1;
    }

}

__attribute__((constructor)) void start (void)
{
  static int started=0;

  if(started==0)
    {
      printf("ROC Load\n");
      
      tipOpen();
      started=1;

    }

}

#endif

