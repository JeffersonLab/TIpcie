/*
 * File:
 *    tipLibTest.c
 *
 * Description:
 *    Test program for the TIpcie Library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "TIpcieLib.h"

extern int nfadc;

int 
main(int argc, char *argv[]) 
{
  int i=0;

  if(tipOpen()!=OK)
    goto CLOSE;
  
  tipInit(0,0);
  tipStatus(1);

  tipPCIEStatus(1);

  tipGetSerialNumber(NULL);

  tipPrintTempVolt();

  printf("Break\n");
  getchar();

  /* for(i=0; i<10000; i++) */
  /*   { */
  /*     tipLatchTimers(); */
  /*     printf("%x\n",tipGetLiveTime()); */
  /*   } */

 CLOSE:
  tipClose();

  exit(0);
}

