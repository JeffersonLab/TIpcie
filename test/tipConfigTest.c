/*
 * File:
 *    tipConfigTest.c
 *
 * Description:
 *    Test the TIpcie library ini config file loading
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "TIpcieLib.h"
#include "TIpcieConfig.h"

int
main(int argc, char *argv[])
{

  printf("\nJLAB TI Library Tests\n");
  printf("----------------------------\n");

  tiConfigInitGlobals();

  if(argc == 2)
    tiConfig(argv[1]);

  tiConfigFree();

  tiConfigPrintParameters();

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k tipConfigTest "
  End:
*/
