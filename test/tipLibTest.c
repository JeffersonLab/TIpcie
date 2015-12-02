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

  tipOpen();
  
  tipInit(0,0);
  tipStatus(1);

 CLOSE:
  tipClose();

  exit(0);
}

