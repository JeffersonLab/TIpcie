/*----------------------------------------------------------------------------*
 *  Copyright (c) 2015        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Firmware update for the Pipeline PCIE Trigger Interface (TIpcie) module.
 *
 *----------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "TIpcieLib.h"

unsigned int BoardSerialNumber;
unsigned int firmwareInfo;
char *programName;

void tipFirmwareEMload(char *filename);
static void tipFirmwareUsage();

int
main(int argc, char *argv[])
{
  int stat;
  int BoardNumber;
  char *filename;
  int inputchar=10;

  printf("\nTI firmware update via VME\n");
  printf("----------------------------\n");

  programName = argv[0];

  if(argc<2)
    {
      printf(" ERROR: Must specify one argument\n");
      tipFirmwareUsage();
      return(-1);
    }
  else
    {
      filename = argv[1];
    }

  stat = tipOpen();

  if(stat != OK)
    goto CLOSE;

  stat = tipInit(TIP_READOUT_EXT_POLL,TIP_INIT_SKIP_FIRMWARE_CHECK);
  if(stat != OK)
    {
      printf("\n");
      printf("*** Failed to initialize TI ***\nThis may indicate (either):\n");
      printf("   a) an incorrect VME Address provided\n");
      printf("   b) new firmware must be loaded at provided VME address\n");
      printf("\n");
      printf("Proceed with the update with the provided VME address?\n");
    REPEAT:
      printf(" (y/n): ");
      inputchar = getchar();

      if((inputchar == 'n') || (inputchar == 'N'))
	{
	  printf("--- Exiting without update ---\n");
	  goto CLOSE;
	}
      else if((inputchar == 'y') || (inputchar == 'Y'))
	{
	  printf("--- Continuing update, assuming VME address is correct ---\n");
	}
      else
	{
	  goto REPEAT;
	}
    }

  /* Read out the board serial number first */
  BoardSerialNumber = tipGetSerialNumber(NULL);

  printf(" Board Serial Number from PROM usercode is: 0x%08x (%d) \n", BoardSerialNumber,
	 BoardSerialNumber&0xffff);

  firmwareInfo = tipGetFirmwareVersion();
  if(firmwareInfo>0)
    {
      printf("  User ID: 0x%x \tFirmware (version - revision): 0x%X - 0x%03X\n",
	     (firmwareInfo&0xFFFF0000)>>16, (firmwareInfo&0xF000)>>12, firmwareInfo&0xFFF);
    }
  else
    {
      printf("  Error reading Firmware Version\n");
    }


  /* Check the serial number and ask for input if necessary */
  /* Force this program to only work for TIpcie */
  if (!((BoardSerialNumber&0xffff0000) == 0x71E00000))
    { 
      printf(" This TI has an invalid serial number (0x%08x)\n",BoardSerialNumber);
      printf (" Enter a new board number (0-4095), or -1 to quit: ");

      scanf("%d",&BoardNumber);

      if(BoardNumber == -1)
	{
	  printf("--- Exiting without update ---\n");
	  goto CLOSE;
	}

      /* Add the TI board ID in the MSB */
      BoardSerialNumber = 0x71E00000 | (BoardNumber&0x7ff);
      printf(" The board serial number will be set to: 0x%08x (%d)\n",
	     BoardSerialNumber,
	     BoardSerialNumber&0x7ff);
    }


  printf("Press y to load firmware (%s) to the TI via VME...\n",
	 filename);
  printf("\t or n to quit without update\n");

 REPEAT2:
  printf("(y/n): ");
  inputchar = getchar();
  
  if((inputchar == 'n') ||
     (inputchar == 'N'))
    {
      printf("--- Exiting without update ---\n");
      goto CLOSE;
    }
  else if((inputchar == 'y') ||
     (inputchar == 'Y'))
    {
    }
  else
    goto REPEAT2;

  tipFirmwareEMload(filename);

 CLOSE:

  tipClose();
  printf("\n");

  return OK;
}

static int 
Emergency(unsigned int jtagType, unsigned int numBits, unsigned long *jtagData)
{
  unsigned int iloop, iword, ibit;
  int rval=OK;
  unsigned int PcieAddress;

#ifdef DEBUG
  int numWord, i;
  printf("type: %x, num of Bits: %x, data: \n",jtagType, numBits);
  numWord = (numBits-1)/32+1;
  for (i=0; i<numWord; i++)
    {
      printf("%08x",jtagData[numWord-i-1]);
    }
  printf("\n");
#endif

  if (jtagType == 0) //JTAG reset, TMS high for 5 clcoks, and low for 1 clock;
    { 
      tipJTAGWrite(0x83c, 0);
      usleep(100);
    }
  else if ((jtagType == 1) || (jtagType == 3)) // JTAG instruction shift
    {
      // Shift_IR header:
      PcieAddress = 0x82c + (((numBits-1)<<6)&0x7c0);
      usleep(100);
#ifdef DEBUG13
      printf(" Address: %08x, Data: %08x \n", PcieAddress, jtagData[0]);
#endif
      tipJTAGWrite(PcieAddress, jtagData[0]);
      usleep(100);
    }
  else if ((jtagType == 2) || (jtagType == 4))  // JTAG data shift
    {
      //shift_DR header
      iword = (numBits+31)/32;
      
      for (iloop =1; iloop<= iword; iloop++)
	{ 
	  PcieAddress = 0x810;
          if (iloop == 1) PcieAddress = PcieAddress + 4;
          if (iloop == iword) PcieAddress = PcieAddress + 8;
	  ibit = 31;
          if (iloop == iword) ibit=(numBits-1) % 32;
          PcieAddress = PcieAddress + ((ibit<<6)&0x7c0);
          usleep(100);
#ifdef DEBUG24
	  printf("iloop %d, Nbits %d,  Address: %08x, Data: %08x \n", iloop, numBits, PcieAddress, jtagData[iloop-1]);
#endif
	  tipJTAGWrite(PcieAddress, jtagData[iloop-1]);
	  usleep(100);
	}
      usleep(100);
    }
  else if (jtagType == 5)  // JTAG RUNTEST
    {
#ifdef DEBUG5
      printf(" real RUNTEST delay %d \n", numBits);
#endif
      iword = (numBits+31)/32;
      for (iloop =0; iword; iloop++)
	{ 
	  PcieAddress = 0xfd0 ;  // Shift TMS=0, TDI=0
          tipJTAGWrite(PcieAddress,0);
	  usleep(100);
	}
    }
  else
    {
      printf( "\n JTAG type %d unrecognized \n",jtagType);
    }

  return rval;
}

static void 
Parse(char *buf,unsigned int *Count,char **Word)
{
  *Word = buf;
  *Count = 0;
  while(*buf != '\0')  
    {
      while ((*buf==' ') || (*buf=='\t') || (*buf=='\n') || (*buf=='"')) *(buf++)='\0';
      if ((*buf != '\n') && (*buf != '\0'))  
	{
	  Word[(*Count)++] = buf;
	}
      while ((*buf!=' ')&&(*buf!='\0')&&(*buf!='\n')&&(*buf!='\t')&&(*buf!='"')) 
	{
	  buf++;
	}
    }
  *buf = '\0';
}

void 
tipFirmwareEMload(char *filename)
{
  unsigned long ShiftData[64], lineRead;
  FILE *svfFile;
  char bufRead[1024],bufRead2[256];
  unsigned int sndData[256];
  char *Word[16], *lastn;
  unsigned int nbits, nbytes, extrType, i, Count, nWords, nlines=0;
  

  /* Check if TI board is readable */
#ifdef CHECKREAD
  unsigned int rval=0;
  rval = tipRead(&TIp->boardID);
  if(rval==-1)
    {
      printf("%s: ERROR: TI card not addressable\n",__FUNCTION__);
      return;
    }
#endif

  //open the file:
  svfFile = fopen(filename,"r");
  if(svfFile==NULL)
    {
      perror("fopen");
      printf("%s: ERROR: Unable to open file %s\n",__FUNCTION__,filename);

      return;
    }

#ifdef DEBUGFW
  printf("\n File is open \n");
#endif

  //PROM JTAG reset/Idle
  Emergency(0,0,ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n",__FUNCTION__);
#endif
  usleep(1000000);

  //Another PROM JTAG reset/Idle
  Emergency(0,0,ShiftData);
#ifdef DEBUGFW
  printf("%s: Emergency PROM JTAG reset IDLE \n",__FUNCTION__);
#endif
  usleep(1000000);


  //initialization
  extrType = 0;
  lineRead=0;

  printf("\n");
  fflush(stdout);

  /* Count the total number of lines */
  while (fgets(bufRead,256,svfFile) != NULL)
    { 
      nlines++;
    }

  rewind(svfFile);

  while (fgets(bufRead,256,svfFile) != NULL)
    { 
      lineRead +=1;
      if((lineRead%((int)(nlines/40))) ==0)
	{
	  printf(".");
	  fflush(stdout);
	}

      if (((bufRead[0] == '/')&&(bufRead[1] == '/')) || (bufRead[0] == '!'))
	{
#ifdef DEBUG
	  printf(" comment lines: %c%c \n",bufRead[0],bufRead[1]);
#endif
	}
      else
	{
	  if (strrchr(bufRead,';') ==0) 
	    {
	      do 
		{
		  lastn =strrchr(bufRead,'\n');
		  if (lastn !=0) lastn[0]='\0';
		  if (fgets(bufRead2,256,svfFile) != NULL)
		    {
		      strcat(bufRead,bufRead2);
		    }
		  else
		    {
		      printf("\n \n  !!! End of file Reached !!! \n \n");

		      return;
		    }
		} 
	      while (strrchr(bufRead,';') == 0);  //do while loop
	    }  //end of if
	
	  // begin to parse the data bufRead
	  Parse(bufRead,&Count,&(Word[0]));
	  if (strcmp(Word[0],"SDR") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
	      nbytes = (nbits-1)/8+1;
	      if (strcmp(Word[2],"TDI") == 0)
		{
		  for (i=0; i<nbytes; i++)
		    {
		      sscanf (&Word[3][2*(nbytes-i-1)+1],"%2x",&sndData[i]);
#ifdef DEBUG
		      printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
#endif
		    }
		  nWords = (nbits-1)/32+1;
		  for (i=0; i<nWords; i++)
		    {
		      ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		    }
		  // hijacking the PROM usercode:
		  if ((nbits == 32) && (ShiftData[0] == 0x71d55948)) {ShiftData[0] = BoardSerialNumber;}

#ifdef DEBUG
		  printf("Word[3]: %s \n",Word[3]);
		  printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
#endif
		  Emergency(2+extrType,nbits,ShiftData);
		}
	    }
	  else if (strcmp(Word[0],"SIR") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
	      nbytes = (nbits-1)/8+1;
	      if (strcmp(Word[2],"TDI") == 0)
		{
		  for (i=0; i<nbytes; i++)
		    {
		      sscanf (&Word[3][2*(nbytes-i)-1],"%2x",&sndData[i]);
#ifdef DEBUG
		      printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
#endif
		    }
		  nWords = (nbits-1)/32+1;
		  for (i=0; i<nWords; i++)
		    {
		      ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		    }
#ifdef DEBUG
		  printf("Word[3]: %s \n",Word[3]);
		  printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
#endif
		  Emergency(1+extrType,nbits,ShiftData);
		}
	    }
	  else if (strcmp(Word[0],"RUNTEST") == 0)
	    {
	      sscanf(Word[1],"%d",&nbits);
#ifdef DEBUG
	      printf("RUNTEST delay: %d \n",nbits);
#endif
	      if(nbits>100000)
		{
		  printf("Erasing (%.1f seconds): ..",((float)nbits)/2./1000000.);
		  fflush(stdout);
		}
	      usleep(nbits/2);
	      if(nbits>100000)
		{
		  printf("Done\n");
		  fflush(stdout);
		  printf("          ----------------------------------------\n");
		  printf("Updating: ");
		  fflush(stdout);
		}
	    }
	  else if (strcmp(Word[0],"STATE") == 0)
	    {
	      if (strcmp(Word[1],"RESET") == 0) Emergency(0,0,ShiftData);
	    }
	  else if (strcmp(Word[0],"ENDIR") == 0)
	    {
	      if ((strcmp(Word[1],"IDLE") ==0 ) || (strcmp(Word[1],"IDLE;") ==0 ))
		{
		  extrType = 0;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n",extrType);
#endif
		}
	      else if ((strcmp(Word[1],"IRPAUSE") ==0) || (strcmp(Word[1],"IRPAUSE;") ==0))
		{
		  extrType = 2;
#ifdef DEBUGFW
		  printf(" ExtraType: %d \n",extrType);
#endif
		}
	      else
		{
		  printf(" Unknown ENDIR type [%s]\n",Word[1]);
		}
	    }
	  else
	    {
#ifdef DEBUGFW
	      printf(" Command type ignored: %s \n",Word[0]);
#endif
	    }

	}  //end of if (comment statement)
    } //end of while

  printf("Done\n");

  printf("** Firmware Update Complete **\n");

  //close the file
  fclose(svfFile);

}


static void
tipFirmwareUsage()
{
  printf("\n");
  printf("%s <firmware svf file>\n",programName);
  printf("\n");

}
