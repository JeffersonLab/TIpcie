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

int
main(int argc, char *argv[])
{

  if(tipOpen()!=OK)
    goto CLOSE;

  tipInit(0,TIP_INIT_SKIP_FIRMWARE_CHECK | TIP_INIT_NO_INIT);
  tipStatus(1);

  tipPCIEStatus(1);

  tipGetSerialNumber(NULL);

  tipPrintTempVolt();

 CLOSE:
  tipClose();

  exit(0);
}
