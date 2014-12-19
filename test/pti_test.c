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

char *progName;
int fd;

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
} DMA_BUF_INFO;

int
ptiRW(PTI_IOCTL_INFO info)
{
  printf(" command_type = %d\n",info.command_type);
  printf("   mem_region = %d\n",info.mem_region);
  printf("         nreg = %d\n",info.nreg);
  printf("       reg[0] = %d\n",info.reg[0]);
  printf("     value[0] = %d\n",info.value[0]);

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

unsigned long
ptiAllocDmaMemory(int size, unsigned long *phys_addr)
{
  int stat=0;
  unsigned long dmaHandle = 0;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = 0,
      .command_type   = PCI_SKEL_MEM_ALLOC,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = size
    };

  stat = ptiDmaMem(&info);

  dmaHandle = info.dma_osspec_hdl;

  *phys_addr = info.phys_addr;

  return dmaHandle;
}

int
ptiFreeDmaMemory(unsigned long dmaHandle)
{
  int stat=0;
  DMA_BUF_INFO info =
    {
      .dma_osspec_hdl = dmaHandle,
      .command_type   = PCI_SKEL_MEM_FREE,
      .phys_addr      = 0,
      .virt_addr      = 0,
      .size           = 0
    };

  stat = ptiDmaMem(&info);

  return stat;
}

int 
main(int argc, char *argv[]) 
{

  int stat;
  int read=0, write=0;
  PTI_IOCTL_INFO info;
  unsigned long dmaHdl=0, phys_addr=0;
  int size=0;
  int ireg=0;
  unsigned int *regs;
  unsigned int *values;


  progName = argv[0];

  printf("%s:\nJLAB PCIexpress TI... ioctl Test\n",progName);
  printf("----------------------------\n");

  if(strcmp(progName,"pti_test")==0)
    { /* Register read/write */

      if((argc==1) || (argc<4))
	{
	  usage();
	  exit(-1);
	}

      regs = (unsigned int*)malloc(500*sizeof(unsigned int));
      values = (unsigned int*)malloc(500*sizeof(unsigned int));
      info.nreg=1;


      /* Evaluate the command line arguments */
      if ((strcmp(argv[1],"r") == 0) || (strcmp(argv[1],"b") == 0))  /* Read Register */
	{
	  read=1;
	  info.command_type = PCI_SKEL_READ;

	  if((argc != 4) && (argc != 5))
	    {
	      usage();
	      exit(-1);
	    }

	  info.mem_region = strtol(argv[2],NULL,10);
	  if((info.mem_region<0) || (info.mem_region>2))
	    {
	      printf("  Invalid Memory Region (%d)\n",info.mem_region);
	      exit(-1);
	    }

	  regs[0] = strtol(argv[3],NULL,16);
	  if(regs[0] & 0x3)
	    {
	      printf("  Invalid Register Offset (0x%x)\n",regs[0]);
	      exit(-1);
	    }
      
	  if(strcmp(argv[1],"b") == 0)
	    {
	      if(argc != 5)
		{
		  usage();
		  exit(-1);
		}

	      info.nreg = strtol(argv[4],NULL,10);
	      for(ireg=0; ireg<info.nreg; ireg++)
		{
		  regs[ireg] = regs[0] + ireg*4;
		}

	    }

	}

      else if (strcmp(argv[1],"w") == 0)  /* Write Register */
	{
	  write=1;
	  info.command_type = PCI_SKEL_WRITE;

	  if(argc != 5)
	    {
	      usage();
	      exit(-1);
	    }

	  info.mem_region = strtol(argv[2],NULL,10);
	  if((info.mem_region<0) || (info.mem_region>2))
	    {
	      printf("  Invalid Memory Region (%d)\n",info.mem_region);
	      exit(-1);
	    }

	  regs[0] = strtol(argv[3],NULL,16);
	  if(regs[0] & 0x3)
	    {
	      printf("  Invalid Register Offset (0x%x)\n",regs[0]);
	      exit(-1);
	    }
      
	  values[0] = strtol(argv[4],NULL,16);
	}
  
      else
	{
	  usage();
	  exit(-1);
	}

      info.reg = (unsigned int *)regs;
      info.value = (unsigned int *)values;

      fd = open("/dev/pci_skel",O_RDWR);
      if(fd<0)
	{
	  perror("open");
	  goto CLOSE;
	}

      /*   stat = ioctl(fd, PCI_SKEL_IOC_RW, &info); */
      stat = ptiRW(info);

      if(stat!=0)
	{
	  printf(" Command returned ERROR!  (%d)\n",stat);
	}
      else if(read)
	{
	  for(ireg=0; ireg<info.nreg; ireg++)
	    {
	      printf("0x%04x:    Read value = 0x%08x (%10u)\n",
		     info.reg[ireg],
		     info.value[ireg], info.value[ireg]);
	    }
	}
    }
  else if(strcmp(progName,"pti_dma_test")==0)
    {
      if((argc==1) || (argc>2))
	{
	  usage();
	  exit(-1);
	}

      size = strtol(argv[1],NULL,10);

      fd = open("/dev/pci_skel",O_RDWR);
      if(fd<0)
	{
	  perror("open");
	  goto CLOSE;
	}

      /*   stat = ioctl(fd, PCI_SKEL_IOC_RW, &info); */
      printf("Allocate!\n");
      dmaHdl = ptiAllocDmaMemory(size, &phys_addr);

      printf("      dmaHdl = 0x%lx\n", dmaHdl);
      printf("   phys_addr = 0x%lx\n", phys_addr);
      printf("Press <Enter> to Free\n");
      getchar();
      stat = ptiFreeDmaMemory(dmaHdl);
    }
    

  close(fd);

 CLOSE:

  exit(0);
}

void
usage()
{

  printf("Usage:\n");
  printf("\n");

  if(strcmp(progName,"pti_test")==0)
    {
      printf("  %s   <OPTION>  <MEM>   <REG>  <VALUE>\n",progName);

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
    }
  else if(strcmp(progName,"pti_dma_test")==0)
    {
      printf("  %s   <OPTION>  <MEM SIZE>\n",progName);
      printf("\n\n");
      printf("    Where:\n");
      printf("      <MEM SIZE> :  Size in Bytes to Allocate (and then free)\n");

      printf("\n\n");
    }
}
