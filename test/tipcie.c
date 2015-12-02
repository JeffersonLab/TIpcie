/*
 * File:
 *    pti_test.c
 *
 * Description:
 *    Test ioctl read and writes to the pti (pci_skel) kernel driver
 *
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define PCI_SKEL_IOC_MAGIC  'k'
#define PCI_SKEL_IOC_RW         _IO(PCI_SKEL_IOC_MAGIC, 1)
#define PCI_SKEL_IOC_MEM        _IO(PCI_SKEL_IOC_MAGIC, 2)

#define PCI_SKEL_WRITE 0
#define PCI_SKEL_READ  1
#define PCI_SKEL_DUMMY 2

#define PCI_SKEL_MEM_ALLOC 0
#define PCI_SKEL_MEM_FREE  1

// function definition
unsigned long FPGAusercode(void);
unsigned long PROMusercode(void);
void TImasterSet(unsigned int slotid, unsigned int blocksize, unsigned int crateid);
void PCIeJTAG(unsigned int jtagType, unsigned int numBits, unsigned long *jtagData);
void PROMload(unsigned int BoardNumb);
void TIpcieSlave(unsigned int SyncDelay);
int fd;
FILE *fdata;

void usage();

typedef struct pti_ioctl_struct
{
  int command_type;
  int mem_region;
  unsigned int nreg;
  unsigned int *reg;
  unsigned int *value;
} PTI_IOCTL_INFO;

typedef struct DMA_BUF_INFO_STRUCT
{
  unsigned long  dma_osspec_hdl;
  int              command_type;
  unsigned long       phys_addr;
  unsigned long       virt_addr;
  unsigned int             size;
  char                 *map_ptr;
} DMA_BUF_INFO;

typedef struct DMA_MAP_STRUCT
{
  unsigned long          dmaHdl;
  unsigned long        map_addr;
  unsigned int             size;
} DMA_MAP_INFO;

int
ptiRW(PTI_IOCTL_INFO info)
{
  // printf(" command_type = %d\n",info.command_type);
  //  printf("   mem_region = %d\n",info.mem_region);
  //printf("         nreg = %d\n",info.nreg);
  //printf("       reg[0] = %d\n",info.reg[0]);
  // printf("     value[0] = 0x%x\n\n",info.value[0]);

  return ioctl(fd, PCI_SKEL_IOC_RW, &info);
}

int
ptiRead(int bar, unsigned int reg, unsigned int *value)
{
  int stat=0;
  PTI_IOCTL_INFO info =
    {
      .command_type = PCI_SKEL_READ,
      .mem_region   = bar,
      .nreg         = 1,
      .reg          = &reg,
      .value        = value
    };

  stat = ptiRW(info);

  return stat;
}

int
ptiWrite(int bar, unsigned int reg, unsigned int value)
{
  int stat=0;
  PTI_IOCTL_INFO info =
    {
      .command_type = PCI_SKEL_WRITE,
      .mem_region   = bar,
      .nreg         = 1,
      .reg          = &reg,
      .value        = &value
    };

  stat = ptiRW(info);

  return stat;
}

int
ptiReadBlock(int bar, unsigned int *reg, unsigned int *value, int nreg)
{
  int stat=0;
  PTI_IOCTL_INFO info =
    {
      .command_type = PCI_SKEL_READ,
      .mem_region   = bar,
      .nreg         = nreg,
      .reg          = reg,
      .value        = value
    };

  stat = ptiRW(info);

  return stat;
}

int
ptiWriteBlock(int bar, unsigned int *reg, unsigned int *value, int nreg)
{
  int stat=0;
  PTI_IOCTL_INFO info =
    {
      .command_type = PCI_SKEL_WRITE,
      .mem_region   = bar,
      .nreg         = nreg,
      .reg          = reg,
      .value        = value
    };

  stat = ptiRW(info);

  return stat;
}

int
ptiDmaMem(DMA_BUF_INFO *info)
{
  int rval=0;

  rval = ioctl(fd, PCI_SKEL_IOC_MEM, info);

  printf("   command_type = %d\n",info->command_type);
  printf(" dma_osspec_hdl = %lx\n",info->dma_osspec_hdl);
  printf("      phys_addr = %lx\n",info->phys_addr);
  printf("      virt_addr = %lx\n",info->virt_addr);
  printf("           size = %d\n",info->size);
  return rval;
}

DMA_MAP_INFO
ptiAllocDmaMemory(int size, unsigned long *phys_addr)
{
  int stat=0;
  DMA_MAP_INFO rval;
  unsigned long dmaHandle = 0;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = 0,
      .command_type   = PCI_SKEL_MEM_ALLOC,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = size
    };
  char *tmp_addr;

  stat = ptiDmaMem(&info);

  dmaHandle = info.dma_osspec_hdl;

  *phys_addr = info.phys_addr;

  /* Do an mmap here */
  tmp_addr = mmap(0, size, PROT_READ | PROT_WRITE, 
		  MAP_SHARED, fd, info.phys_addr);

  if(tmp_addr == (void*) -1)
    {
      printf("%s: ERROR: mmap failed\n",
	     __FUNCTION__);
    }

  rval.dmaHdl = dmaHandle;
  rval.map_addr = (unsigned long)tmp_addr;
  rval.size = size;

  return rval;
}

int ptiFreeDmaMemory(DMA_MAP_INFO mapInfo)
{
  int stat=0;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = mapInfo.dmaHdl,
      .command_type   = PCI_SKEL_MEM_FREE,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = 0
    };

  stat = ptiDmaMem(&info);

  /* Do an munmap here */
  if(mapInfo.map_addr == -1)
    {
      /* Do nothing here */
    }
  else
    {
      munmap((char*)mapInfo.map_addr, mapInfo.size);
    }

  return stat;
}

void TIpcieSetup(unsigned int BlockLevel)  // and trigger generation
{
  unsigned long Dvalue=0;

  ptiWrite(0, 0x2c, 0); // use oscillator as clock
  ptiWrite(0, 0x24, 0x10); // set the sync source to loopback
  ptiWrite(0, 0x78, 0xdd); // Sync Reset
  ptiWrite(0, 0x78, 0x77); // stop the trigger
  ptiWrite(0, 0x78, 0xdd); // another sync reset
  ptiWrite(0, 0x78, 0x55); // trigger start
  Dvalue = 0x800 + (BlockLevel & 0xff); 
  ptiWrite(0, 0x84, Dvalue); // set DAQ readout block level
  ptiWrite(0, 0x78, 0xdd); //another sync Reset
  ptiRead(0, 0x14, &Dvalue); // readout the current setting of the block level
  printf("\n The block level is: %08x \n", Dvalue);
  ptiWrite(0, 0x20, 0x94); // set the trigger source
  ptiWrite(0, 0x28, 0); // disable the busy
  ptiWrite(0, 0x18, 0xff); // set the data format
}

void TIpcieSlave(unsigned int SyncDelay)  // and trigger generation
{
  unsigned long Dvalue=0;

  ptiWrite(0, 0x2c, 2); // use fiber clock
  usleep(10);
  ptiWrite(0,0x100, 0x100); // Clk250 DCM reset
  usleep(10);
  ptiWrite(0, 0x100, 0x200); // Clk125 DCM reset
  usleep(10);

  ptiWrite(0, 0x100, 0x2000); //fiber measure, auto adjust the return signal phase
  usleep(100);
  ptiWrite(0, 0x100, 0x8000); // measure the fiber latency
  usleep(1000);
  ptiRead(0, 0xa0, &Dvalue); // readout the measurement
  printf("\n Fiber measurement %08x \n", Dvalue);
  ptiWrite(0, 0x100, 0x800); // auto adjust the sync phase
  usleep(100);
  ptiRead(0, 0xa0, &Dvalue); // readout the measurement
  printf("\n Fiber measurement %08x \n", Dvalue);
  ptiWrite(0, 0x50, ((SyncDelay<<8)&0xff00) ); // set the delay to SyncDelay

  ptiWrite(0, 0x20, 0x92); // enable fiber trigger input
  usleep(10);
  ptiWrite(0, 0x24, 0x2); // set the sync source to fiber
  usleep(10);
  //  ptiWrite(0, 0x78, 0xdd); // Sync Reset
  // ptiWrite(0, 0x78, 0x77); // stop the trigger
  //ptiWrite(0, 0x78, 0xdd); // another sync reset
  //ptiWrite(0, 0x78, 0x55); // trigger start
  //Dvalue = 0x800 + (BlockLevel & 0xff); 
  //ptiWrite(0, 0x84, Dvalue); // set DAQ readout block level
  //ptiWrite(0, 0x78, 0xdd); //another sync Reset
  ptiRead(0, 0x14, &Dvalue); // readout the current setting of the block level
  printf("\n The block level is: %08x \n", Dvalue);
  //ptiWrite(0, 0x20, 0x94); // set the trigger source
  ptiWrite(0, 0x0, 0x6A); // set crate ID
  ptiWrite(0, 0x18, 0xff); // set the data format
  ptiWrite(0, 0x100, 0x4000); // TI IODELAY reset
  usleep(1000);
  ptiWrite(0, 0x100, 0x800); // TI sync auto alignment
  usleep(1000);
  ptiWrite(0, 0x100, 0x2000); //TI auto alignment fiber delay
  usleep(1000);
  ptiWrite(0, 0x100, 0x8000); // TI auto fiber delay measurement
  usleep(1000);
  ptiWrite(0, 0x100, 0x400); // TI MGT synchronization
  usleep(1000);
  ptiWrite(0, 0x9c, 0x7e); // set the TIpcie into running mode
  usleep(10);
  ptiRead(0, 0, &Dvalue); // readout the board code
  printf("\ The board ID is %08x \n", Dvalue);
}

int 
main(int argc, char *argv[]) 
{

  int stat;
  PTI_IOCTL_INFO info;
  unsigned long phys_addr=0;
  DMA_MAP_INFO mapInfo;
  int size=0;
  int ireg=0, jreg=0, maxblock = 0;
  unsigned int *regs;
  unsigned int *values;
  unsigned int BlockLevel, BoardNumb;

  printf("\n PCIexpress TI... ioctl Test\n");

  regs = (unsigned int*)malloc(500*sizeof(unsigned int));
  values = (unsigned int*)malloc(500*sizeof(unsigned int));
  info.nreg=1;


  if (argc == 1) 
    { BlockLevel = 10; }
  else
    { BlockLevel = strtol(argv[1], NULL, 10); }

  fd = open("/dev/pci_skel",O_RDWR);
  if(fd<0)
	{
	  perror("open");
	  goto CLOSE;
	}

  // load the PROM
  if (argc == 3) 
    { BoardNumb = strtol(argv[2], NULL, 10);  // get the board number for PROMload
      PROMload(BoardNumb);
      printf("\n\n sleeping ...\n");
      sleep(10);
    }

  // setup the TIpcie
  //  TIpcieSetup(BlockLevel);

  // setup as TI master
  //stop trigger first
  ptiWrite(0,0x88, 0);
  ptiWrite(0,0x20,0);

  // Set the TIpcie to slave mode (accepting trigger from Fiber)
  //  TIpcieSlave(120);

  // set the TIpcie to master mode (or standalone mode)
  TImasterSet(0,BlockLevel,0x5a);

  // load trigger table
  TIpcieTableLoad(3);

  // Dma access

  size = 1048576; // 1 MB,  strtol(argv[1],NULL,10);
  printf("Allocate!\n");
  mapInfo = ptiAllocDmaMemory(size, &phys_addr);

  printf("      dmaHdl = 0x%lx\n", mapInfo.dmaHdl);
  printf("   phys_addr = 0x%lx\n", phys_addr);
  printf("    map_addr = 0x%lx\n", mapInfo.map_addr);
  regs = (unsigned int*)mapInfo.map_addr;

  // load the parameters to TIpcie registers
  ptiWrite(0, 0x54, 0x21000000);  // set up the DMA, 32-bit, 256 byte packet, 1 MB, 
  ptiWrite(0, 0x58, phys_addr);  // load the DMA starting address

  // start front panel trigger table input
  ptiWrite(0, 0x44, 0x3); //enabler channel #1 and #2
  ptiWrite(0, 0x20, 0x24); // enable the front panel in, and loopback

  // start trigger
  //  ptiWrite(0, 0x8c, 0xffffffff); // trigger 0x100 events in low speed
  //  ptiWrite(0, 0x8c, 0xfff1000); // trigger 0x1000 events in higher speed
  //  ptiWrite(0,0x88, 0x80); // fast random trigger

  FPGAusercode();
  //  change the wait to a usleep
  printf("Press <Enter> to Print\n");
  getchar();
  // usleep, the setting 10000 is actually ~28ms (including fopen).
  //  usleep(10000);

  // open a data file with write permission
  fdata = fopen("TIpcieData.txt","w");

  maxblock = size/4096;
  for (jreg =0; jreg < maxblock; jreg++)
     {
       for(ireg=0; ireg<1024; ireg++)
	 fprintf(fdata, "0x%04x: (hex) value = %8x\n",4*(jreg*1024+ireg),regs[jreg*1024+ireg]);
     }

  printf("Press <Enter> to Free\n");
  getchar();
  stat = ptiFreeDmaMemory(mapInfo);

  // close the TIpciexpress
  close(fd);

  // close the data file
  fclose(fdata);
 CLOSE: 
  return(2);

}

void
usage()
{

  printf("Usage:\n");
  printf("\n");

      printf("\n\n");
      printf("    Where:\n");
      printf("      <OPTION> :  r for Register READ\n");
      printf("               :  b for Register Block Read\n");
      printf("               :  w for Register WRITE\n");
      printf("         <MEM> :  Integer indicating which memory region to perform <OPTION>\n");
      printf("         <REG> :  Register offset to perform <OPTION>\n");
      printf("       <VALUE> :  for b: Number of reads to perform, starting at offset <REG>\n");
      printf("               :  for w: Value to write to register offset <REG>\n");
      printf("\n\n");

      printf("\n\n");
      printf("    Where:\n");
      printf("      <MEM SIZE> :  Size in Bytes to Allocate (and then free)\n");

      printf("\n\n");
}

// PROM load for the TIpcie 
void PROMload(unsigned int BoardNumb) 
{
  unsigned int  forcenewID, nbreak;
  unsigned long ShiftData[64], lineRead, BoardSerialNumber;
  unsigned int jtagType, jtagBit, iloop;
  FILE *svfFile;
  int byteRead, BoardType, SerialNumber, icontinue;
  char bufRead[1024],bufRead2[256];
  unsigned int sndData[256];
  char *Word[16], *lastn;
  unsigned int nbits, nbytes, extrType, i, Count, nWords, nlines;
  unsigned long RegAdd;

  // Read out the board serial number first
  forcenewID = BoardNumb;
  BoardSerialNumber = PROMusercode();
  printf(" Board number from PROM usercode is: %8x \n", BoardSerialNumber);

  // Check the serial number and ask for input if necessary
  if (((BoardSerialNumber&0xffff0000) == 0x75000000) && (forcenewID != 99) )
    { BoardType = 1;}
  else if (((BoardSerialNumber&0xfffff000) == 0x7D000000) && (forcenewID != 99) )
    { BoardType = 2;}
  else if (((BoardSerialNumber&0xffff0000) == 0x71000000) && (forcenewID != 99))
    { BoardType = 3;}
  else if (((BoardSerialNumber&0xfffff000) == 0x71E00000) && (forcenewID != 99))
    { BoardType = 4;}
  else 
    { printf (" Enter the board type (1: TS;   2: TD;   3: TI;   4: TI_PCIe):");
    scanf ("%d", &BoardType);
    printf (" Enter the board serial number: ");
    scanf("%d",&SerialNumber);

    if (BoardType == 1) 
      {BoardSerialNumber = 0x75000000 + (SerialNumber&0x3ff);}
    else if (BoardType == 2) 
      {BoardSerialNumber = 0x7D000000 + (SerialNumber&0xfff);}
    else if (BoardType == 3)
      {BoardSerialNumber = 0x71000000 + (SerialNumber&0xfff);}
    else if (BoardType == 4) 
      {BoardSerialNumber = 0x71E00000 + (SerialNumber&0xfff);}
    else
      {BoardSerialNumber = 0x59480000 + (SerialNumber&0xfff);
      exit(1);}
    printf(" The board serial number will be set to: %8x \n",BoardSerialNumber);
    }

  //PROM JTAG reset/Idle
  printf(" PROMusercode: %08x \n", PROMusercode());
  ptiWrite(1, 0x83c, 0);
  printf("\n PCIeJTAG PROM JTAG reset IDLE \n");
  usleep(1000000);

  // manual break to debug
  //    printf (" Continue with a input number 1: ");
  //scanf("%d",&icontinue);

  //Another PROM JTAG reset/Idle
  ptiWrite(1,0x83c,0);
  printf("\n PCIeJTAG PROM JTAG reset IDLE \n");
  usleep(1000000);

  // manual break to debug
  //  printf (" Continue with a input number 2: ");
  //scanf("%d",&icontinue);


  //open the file:
  if (BoardType == 1)
    { svfFile = fopen("ts.svf","r");}
  else if (BoardType == 2)
    { svfFile = fopen("td.svf","r");}
  else if (BoardType == 3)
    { svfFile = fopen("ti.svf","r");}
  else 
    { svfFile = fopen("tie.svf","r");}
  printf("\n File is open \n");

  // manual break to debug
  //  printf (" Continue with a input number 3: ");
  //scanf("%d",&icontinue);


  //initialization
  extrType = 0;
  lineRead=0;

  //  for (nlines=0; nlines<200; nlines++)
  printf("\n Lines reading \n");
  fflush(stdout);

  // manual break to debug
  //  printf (" Continue with a input number 4: ");
  //scanf("%d",&icontinue);

    nbreak = 0;

  while (fgets(bufRead,256,svfFile) != NULL)
  { 
    lineRead +=1;

  // manual break to debug
  //  if (nbreak < 10) 
    //  { nbreak += 1;
    //  printf (" Continue with a input number 1: ");
    //	scanf("%d",&icontinue);}

    if (lineRead%1000 ==0) 
      { printf("lines read: %8d \n",lineRead );
	fflush(stdout);
      }
    //    fgets(bufRead,256,svfFile);
    if (((bufRead[0] == '/')&&(bufRead[1] == '/')) || (bufRead[0] == '!'))
      {
	//	printf(" comment lines: %c%c \n",bufRead[0],bufRead[1]);
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
		    // printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
		  }
		nWords = (nbits-1)/32+1;
		for (i=0; i<nWords; i++)
		  {
		    ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		  }

		// hijacking the PROM usercode:
		if ((nbits == 32) && (ShiftData[0] == 0x71d55948)) {ShiftData[0] = BoardSerialNumber;}

		//printf("Word[3]: %s \n",Word[3]);
		//printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
		PCIeJTAG(2+extrType,nbits,ShiftData);
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
		    //  printf("Word: %c%c, data: %x \n",Word[3][2*(nbytes-i)-1],Word[3][2*(nbytes-i)],sndData[i]);
		  }
		nWords = (nbits-1)/32+1;
		for (i=0; i<nWords; i++)
		  {
		    ShiftData[i] = ((sndData[i*4+3]<<24)&0xff000000) + ((sndData[i*4+2]<<16)&0xff0000) + ((sndData[i*4+1]<<8)&0xff00) + (sndData[i*4]&0xff);
		  }
		//printf("Word[3]: %s \n",Word[3]);
		//printf("sndData: %2x %2x %2x %2x, ShiftData: %08x \n",sndData[3],sndData[2],sndData[1],sndData[0], ShiftData[0]);
		PCIeJTAG(1+extrType,nbits,ShiftData);
	      }
	  }
	else if (strcmp(Word[0],"RUNTEST") == 0)
	  {
	    sscanf(Word[1],"%d",&nbits);
	    if (nbits > 10000) printf("RUNTEST delay: %d \n",nbits);
      	    usleep(nbits);   //delay at the software side, this may not work

	    //PCIeJTAG(5, nbits*2, ShiftData);
	    //if (nbits < 10000) PCIeJTAG(5, nbits, ShiftData); // delay more

	  }
	else if (strcmp(Word[0],"STATE") == 0)
	  {
	    if (strcmp(Word[1],"RESET") == 0) PCIeJTAG(0,0,ShiftData);
	  }
	else if (strcmp(Word[0],"ENDIR") == 0)
	  {
	    if (strcmp(Word[1],"IDLE") ==0 )
	      {
		extrType = 0;
		printf(" ExtraType: %d \n",extrType);
	      }
	    else if (strcmp(Word[1],"IRPAUSE") ==0)
	      {
		extrType = 2;
		printf(" ExtraType: %d \n",extrType);
	      }
	    else
	      {
		printf(" Unknown ENDIR type %s\n",Word[1]);
	      }
	  }
	else
	  {
	    printf(" Command type ignored: %s \n",Word[0]);
	  }

      }  //end of if (comment statement)
  } //end of while

  //close the file
  fclose(svfFile);

  printf("\n svf file is closed \n");

  CLOSE:

  exit(2);

}

void TImasterSet(unsigned int slotid, unsigned int blocksize, unsigned int crateid)
{
  unsigned long regdata;
  ptiWrite(0,0x0,(crateid & 0xff)); //set crate ID
  printf("Set crate ID \n");
  sleep(1);
  ptiWrite(0,0x2c, 0); // set the clock source to oscillator
  printf("CLock source is set to oscillator \n");
  sleep(1);
  ptiWrite(0,0x24 ,0x10); // set the sync source
  printf("Sync source is set to loopback \n");
  sleep(1);
  ptiWrite(0, 0x78, 0xdd); // SYNC reset
  printf("SyncReset issued\n");
  sleep(1);
  ptiWrite(0,0x78,0x77);  //Trigger Stop
  printf("Trigger link stopped \n");
  sleep(1);
  ptiWrite(0, 0x78, 0xdd); // SYNC reset
  printf("SyncReset issued\n");
  sleep(1);
  ptiWrite(0,0x78,0x55); //Trigger start
  printf("Trigger link started \n");
  sleep(1);
  ptiWrite(0,0x84,(0x800 + (blocksize & 0xff)) ); //trigger command
  printf("Block level is set to :%02x\n",blocksize&0xff);
  sleep(1);
  ptiWrite(0,0x78,0xdd); //SYNC reset
  printf("Sync Reset again \n");
  sleep(1);
  ptiRead(0,0x14,&regdata); //read back the block level
  printf("Block level is set to %08x \n", regdata);
  sleep(1);
  ptiWrite(0,0x20,0x94); // set the trigger source
  printf("Set the trigger source\n");
  sleep(1);
  ptiWrite(0,0x28,0); //jtag reset idle
  printf("set the BUSY source, all disabled\n");
  sleep(1);
  ptiWrite(0,0x8c, 0xffff0010); //jtag PROM usercode
  printf("send 0x10 triggers at very slow rate\n");
  printf("The data buffer status is %08x \n", regdata);
  return(0);
  }

unsigned long FPGAusercode(void)
{
  unsigned long regdata;
  ptiWrite(0, 0x100, 0x04); // JTAG reset
  printf("Send the JTAG reset command\n");
  usleep(10000);
  ptiWrite(1,0x3c,0); //jtag reset idle
  printf("send the jtag reset idle\n");
  usleep(10000);
  ptiWrite(1,0x26c,0x3c8); //jtag PROM usercode
  printf("send the PROM usercode\n");
  usleep(10000);
  ptiWrite(1,0x7dc,0); // jtag shift usercode
  printf("send the shift data \n");
  usleep(10000);
  ptiRead(1,0x00, &regdata); // read data back
  printf("The usercode is: %08x \n", regdata);
  return(regdata);
}

unsigned long PROMusercode(void) 
{
  unsigned long regdata;
  ptiWrite(0, 0x100, 0x04); // JTAG reset
  printf("Send the JTAG reset command\n");
  usleep(10000);
  ptiWrite(1,0x83c,0); //jtag reset idle
  printf("send the jtag reset idle\n");
  usleep(10000);
  ptiWrite(1,0xbec,0xfd); //jtag PROM usercode
  printf("send the PROM usercode\n");
  usleep(10000);
  ptiWrite(1,0xfdc,0); // jtag shift usercode
  printf("send the shift data \n");
  usleep(10000);
  ptiRead(1,0x800, &regdata); // read data back
  printf("The usercode is: %08x \n", regdata);
  return (regdata);
}

void PCIeJTAG(unsigned int jtagType, unsigned int numBits, unsigned long *jtagData)
{
  unsigned long *laddr;
  unsigned int iloop, iword, ibit;
  unsigned long shData;
  unsigned long RegAdd;
  int numWord, i, endWord;
  unsigned int PcieAddress;

  //  printf("type: %x, num of Bits: %x, data: \n",jtagType, numBits);
  numWord = (numBits-1)/32+1;
  endWord = 0;

  laddr = 0x9000fffc + RegAdd;  // for MVME6100 controller
  //  printf("\n laddr is redefined as %08x \n",laddr);

  if (jtagType == 0) //JTAG reset, TMS high for 5 clcoks, and low for 1 clock;
    { ptiWrite(1, 0x83c, 0);
 
      usleep(100);
    }
  else if (jtagType == 1 || jtagType == 3) // JTAG instruction shift
    {
      // Shift_IR header:
      PcieAddress = 0x82c + (((numBits-1)<<6)&0x7c0);
      usleep(100);
      //      printf(" Address: %08x, Data: %08x \n", PcieAddress, jtagData[0]);
      ptiWrite(1, PcieAddress, jtagData[0]);
      usleep(100);
    }
  else if (jtagType == 2 || jtagType ==4)  // JTAG data shift
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
	  //  printf("iloop %d, Nbits %d,  Address: %08x, Data: %08x \n", iloop, numBits, PcieAddress, jtagData[iloop-1]);
	  ptiWrite(1,PcieAddress, jtagData[iloop-1]);
	  usleep(100);
	}
      usleep(100);
    }
  else if (jtagType == 5)  // JTAG RUNTEST
    {
      //      printf(" real RUNTEST delay %d \n", numBits);
      iword = (numBits+31)/32;
      for (iloop =0; iword; iloop++)
	{ 
	  PcieAddress = 0xfd0 ;  // Shift TMS=0, TDI=0
          ptiWrite(1,PcieAddress,0);
	  usleep(100);
	}
    }


  else
    {
      printf( "\n JTAG type %d unrecognized \n",jtagType);
    }

  //  printf (" \n PCIeJTAG command executed \n");
}




void Parse(char *buf,int *Count,char **Word)
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

// TIpcie front panel trigger table setup
void TIpcieTableLoad(unsigned int imode)
{
	unsigned long RegData;
	unsigned long RegAdd;

//imode=1: TS#1,2,3 generate Trigger1, TS#4,5,6 generate Trigger2.  If both Trigger1 and Trigger2, they are SyncEvent;
//imode=2: TS#1,2,3,4,5 generate Trigger1, TS#6 generate Trigger2.  If both Trigger1 and Trigger2, they are SyncEvent;
//imode=3: samilar to imode=2, but Trigger 1 has priority.  No SYncEvent will be generated.
      if (imode == 1)  
	{
	  ptiWrite(0, 0x140, 0x43424100); //0c0c0c00;
	  ptiWrite(0, 0x144, 0x47464544); //0c0c0c0c;
	  ptiWrite(0, 0x148, 0xcbcac988); //ccccccc0;
	  ptiWrite(0, 0x14C, 0xcfcecdcc); //CCCCCCCC;
	  ptiWrite(0, 0x150, 0xd3d2d190); //CCCCCCC0;
	  ptiWrite(0, 0x154, 0xd7d6d5d4); //CCCCCCCC;
	  ptiWrite(0, 0x158, 0xdbdad998); //CCCCCCC0;
	  ptiWrite(0, 0x15C, 0xdfdedddc); //CCCCCCCC;
	  ptiWrite(0, 0x160, 0xe3e2e1a0); //CCCCCCC0;
	  ptiWrite(0, 0x164, 0xe7e6e5e4); //CCCCCCCC;
	  ptiWrite(0, 0x168, 0xebeae9a8); //CCCCCCC0;
	  ptiWrite(0, 0x16C, 0xefeeedec); //CCCCCCCC;
	  ptiWrite(0, 0x170, 0xf3f2f1b0); //CCCCCCC0;
	  ptiWrite(0, 0x174, 0xf7f6f5f4); //CCCCCCCC;
	  ptiWrite(0, 0x178, 0xfbfaf9b8); //CCCCCCC0;
	  ptiWrite(0, 0x17C, 0xfffefdfc); } //CCCCCCCC;
      else if (imode == 2) 
        {
	  ptiWrite(0, 0x140, 0x43424100); //0c0c0c00;
	  ptiWrite(0, 0x144, 0x47464544); //0c0c0c0c;
	  ptiWrite(0, 0x148, 0x4b4a4948); //ccccccc0;
	  ptiWrite(0, 0x14C, 0x4f4e4d4c); //CCCCCCCC;
	  ptiWrite(0, 0x150, 0x53525150); //CCCCCCC0;
	  ptiWrite(0, 0x154, 0x57565554); //CCCCCCCC;
	  ptiWrite(0, 0x158, 0x5b5a5958); //CCCCCCC0;
	  ptiWrite(0, 0x15C, 0x5f5e5d5c); //CCCCCCCC;
	  ptiWrite(0, 0x160, 0xe3e2e1a0); //CCCCCCC0;
	  ptiWrite(0, 0x164, 0xe7e6e5e4); //CCCCCCCC;
	  ptiWrite(0, 0x168, 0xebeae9e8); //CCCCCCC0;
	  ptiWrite(0, 0x16C, 0xefeeedec); //CCCCCCCC;
	  ptiWrite(0, 0x170, 0xf3f2f1f0); //CCCCCCC0;
	  ptiWrite(0, 0x174, 0xf7f6f5f4); //CCCCCCCC;
	  ptiWrite(0, 0x178, 0xfbfaf9f8); //CCCCCCC0;
	  ptiWrite(0, 0x17C, 0xfffefdfc); } //CCCCCCCC;
      else 
	{
	  ptiWrite(0, 0x140, 0x43424100); //0c0c0c00;
	  ptiWrite(0, 0x144, 0x47464544); //0c0c0c0c;
	  ptiWrite(0, 0x148, 0x4b4a4948); //ccccccc0;
	  ptiWrite(0, 0x14C, 0x4f4e4d4c); //CCCCCCCC;
	  ptiWrite(0, 0x150, 0x53525150); //CCCCCCC0;
	  ptiWrite(0, 0x154, 0x57565554); //CCCCCCCC;
	  ptiWrite(0, 0x158, 0x5b5a5958); //CCCCCCC0;
	  ptiWrite(0, 0x15C, 0x5f5e5d5c); //CCCCCCCC;
	  ptiWrite(0, 0x160, 0x636261a0); //CCCCCCC0;
	  ptiWrite(0, 0x164, 0x67666564); //CCCCCCCC;
	  ptiWrite(0, 0x168, 0x6b6a6968); //CCCCCCC0;
	  ptiWrite(0, 0x16C, 0x6f6e6d6c); //CCCCCCCC;
	  ptiWrite(0, 0x170, 0x73727170); //CCCCCCC0;
	  ptiWrite(0, 0x174, 0x77767574); //CCCCCCCC;
	  ptiWrite(0, 0x178, 0x7b7a7978); //CCCCCCC0;
	  ptiWrite(0, 0x17C, 0x7f7e7d7c); } //CCCCCCCC;
}


