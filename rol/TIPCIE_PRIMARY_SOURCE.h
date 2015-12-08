/******************************************************************************
*
* header file for use Linux VME defined rols with CODA
*
*                             DJA   Nov 2000
*
* SVN: $Rev$
*
*******************************************************************************/
#ifndef __TIPCIE_PRIMARY_ROL__
#define __TIPCIE_PRIMARY_ROL__

static int TIPCIE_PRIMARY_handlers,TIPCIE_PRIMARYflag;
static int TIPCIE_PRIMARY_isAsync;
static unsigned int *TIPCIE_PRIMARYPollAddr = NULL;
static unsigned int TIPCIE_PRIMARYPollMask;
static unsigned int TIPCIE_PRIMARYPollValue;
static unsigned long TIPCIE_PRIMARY_prescale = 1;
static unsigned long TIPCIE_PRIMARY_count = 0;


/* Put any global user defined variables needed here for TIPCIE_PRIMARY readout */
extern DMA_MEM_ID vmeOUT, vmeIN;

/*----------------------------------------------------------------------------
  tipcie_primary_trigLib.c -- Dummy trigger routines for GENERAL USER based ROLs

 File : tipcie_primary_trigLib.h

 Routines:
	   void tipcie_primarytenable();        enable trigger
	   void tipcie_primarytdisable();       disable trigger
	   char tipcie_primaryttype();          return trigger type 
	   int  tipcie_primaryttest();          test for trigger  (POLL Routine)
------------------------------------------------------------------------------*/

static void 
tipcie_primarytenable(int val)
{
  TIPCIE_PRIMARYflag = 1;
}

static void 
tipcie_primarytdisable(int val)
{
  TIPCIE_PRIMARYflag = 0;
}

static unsigned long 
tipcie_primaryttype()
{
  return(1);
}

static int 
tipcie_primaryttest()
{
  if(dmaPEmpty(vmeOUT)) 
    return (0);
  else
    return (1);
}



/* Define CODA readout list specific Macro routines/definitions */

#define TIPCIE_PRIMARY_TEST  tipcie_primaryttest

#define TIPCIE_PRIMARY_INIT { TIPCIE_PRIMARY_handlers =0;TIPCIE_PRIMARY_isAsync = 0;TIPCIE_PRIMARYflag = 0;}

#define TIPCIE_PRIMARY_ASYNC(code,id)  {printf("*** No Async mode is available for TIPCIE_PRIMARY ***\n"); \
                              printf("linking sync TIPCIE_PRIMARY trigger to id %d \n",id); \
			       TIPCIE_PRIMARY_handlers = (id);TIPCIE_PRIMARY_isAsync = 0;}

#define TIPCIE_PRIMARY_SYNC(code,id)   {printf("linking sync TIPCIE_PRIMARY trigger to id %d \n",id); \
			       TIPCIE_PRIMARY_handlers = (id);TIPCIE_PRIMARY_isAsync = 1;}

#define TIPCIE_PRIMARY_SETA(code) TIPCIE_PRIMARYflag = code;

#define TIPCIE_PRIMARY_SETS(code) TIPCIE_PRIMARYflag = code;

#define TIPCIE_PRIMARY_ENA(code,val) tipcie_primarytenable(val);

#define TIPCIE_PRIMARY_DIS(code,val) tipcie_primarytdisable(val);

#define TIPCIE_PRIMARY_CLRS(code) TIPCIE_PRIMARYflag = 0;

#define TIPCIE_PRIMARY_GETID(code) TIPCIE_PRIMARY_handlers

#define TIPCIE_PRIMARY_TTYPE tipcie_primaryttype

#define TIPCIE_PRIMARY_START(val)	 {;}

#define TIPCIE_PRIMARY_STOP(val)	 {tipcie_primarytdisable(val);}

#define TIPCIE_PRIMARY_ENCODE(code) (code)


#endif

