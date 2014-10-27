/*
 * File:
 *    mmapTest.c
 *
 * Description:
 *    Test mmap to the pci_skel kernel driver
 *
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

unsigned int *regs;

int 
main(int argc, char *argv[]) 
{

  int stat;
  int fd=0;
  int i;
  unsigned int *addr;

  printf("\nJLAB pci_skel... mmap Test\n");
  printf("----------------------------\n");

  fd = open("/dev/pci_skel",O_RDWR);
  if(fd<0)
    {
      perror("open");
      goto CLOSE;
    }

  unsigned int *dupe = (unsigned int*)malloc(7*sizeof(unsigned int));

  addr = (unsigned int*)mmap(dupe,7*sizeof(unsigned int),PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(addr>0)
    {
      printf("addr = 0x%08x\n",addr);

      addr[0] = 0x12345678;

      munmap(addr,7*sizeof(unsigned int));
    }
  else
    {
      printf("nope\n");
    }


  close(fd);

 CLOSE:

  exit(0);
}

