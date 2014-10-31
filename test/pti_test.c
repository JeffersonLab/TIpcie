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

#define PCI_SKEL_WRITE 0
#define PCI_SKEL_READ  1
#define PCI_SKEL_DUMMY 2

char *progName;

void usage();

typedef struct pti_ioctl_struct
{
  int mem_region;
  int reg;
  int command_type;
  unsigned int value;
} PTI_IOCTL_INFO;

int 
main(int argc, char *argv[]) 
{

  int stat;
  int fd=0;
/*   int iarg=0; */
/*   int i; */
  int read=0, write=0;
/*   unsigned int *addr; */
  PTI_IOCTL_INFO info;


  printf("\nJLAB PCIexpress TI... ioctl read/write Test\n");
  printf("----------------------------\n");

  progName = argv[0];

  if((argc==1) || (argc<4))
    {
      usage();
      exit(-1);
    }

  /* Evaluate the command line arguments */
  if (strcmp(argv[1],"r") == 0)  /* Read Register */
    {
      read=1;
      info.command_type = PCI_SKEL_READ;

      if(argc != 4)
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

      info.reg = strtol(argv[3],NULL,16);
      if(info.reg & 0x3)
	{
	  printf("  Invalid Register Offset (0x%x)\n",info.reg);
	  exit(-1);
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

      info.reg = strtol(argv[3],NULL,16);
      if(info.reg & 0x3)
	{
	  printf("  Invalid Register Offset (0x%x)\n",info.reg);
	  exit(-1);
	}
      
      info.value = strtol(argv[4],NULL,16);
    }
  
  else
    {
      usage();
      exit(-1);
    }


  fd = open("/dev/pci_skel",O_RDWR);
  if(fd<0)
    {
      perror("open");
      goto CLOSE;
    }

  stat = ioctl(fd, PCI_SKEL_IOC_RW, &info);

  if(stat!=0)
    {
      printf(" Command returned ERROR!  (%d)\n",stat);
    }
  else if(read)
    {
      printf("    Read value = 0x%08x (%d)\n",
	     info.value, info.value);
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
  printf("  %s   <OPTION>  <MEM>   <REG>  <VALUE>\n",progName);

  printf("\n\n");
  printf("    Where:\n");
  printf("      <OPTION> :  r for Register READ\n");
  printf("               :  w for Register WRITE\n");
  printf("         <MEM> :  Integer indicating which memory region to perform <OPTION>\n");
  printf("         <REG> :  Register offset to perform <OPTION>\n");
  printf("       <VALUE> :  Value to write to register offset <REG> if <OPTION> == w\n");
  printf("\n\n");


}
