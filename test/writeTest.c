/*
 * File:
 *    mmapTest.c
 *
 * Description:
 *    Test ioctl to the pci_skel kernel driver
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

#define PCI_SKEL_IOC_SET    _IO(PCI_SKEL_IOC_MAGIC, 1)
#define PCI_SKEL_IOC_ZERO   _IO(PCI_SKEL_IOC_MAGIC, 2)

int 
main(int argc, char *argv[]) 
{

  int stat;
  int fd=0;
  int i;
  unsigned int *addr;

  printf("\nJLAB pci_skel... ioctl Test\n");
  printf("----------------------------\n");

  if(argc!=2)
    {
      printf(" %s: Must have one argument\n",argv[0]);
      exit(0);
    }

  i = atoi(argv[1]);

  fd = open("/dev/pci_skel",O_RDWR);
  if(fd<0)
    {
      perror("open");
      goto CLOSE;
    }

  if(i!=0)
    stat = ioctl(fd, PCI_SKEL_IOC_SET, 1);
  else
    stat = ioctl(fd, PCI_SKEL_IOC_ZERO, 1);

  printf(" stat = %d\n",stat);

  close(fd);

 CLOSE:

  exit(0);
}

