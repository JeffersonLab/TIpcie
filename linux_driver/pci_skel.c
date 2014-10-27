/* #include <linux/config.h> */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include "pci_skel.h"

#define PCI_VENDOR_ID_MINE PCI_VENDOR_ID_XILINX
#define PCI_DEVICE_ID_MINE 0x0007

unsigned int pci_skel_init_flags;

static struct pci_device_id ids[] = {
  { PCI_DEVICE(PCI_VENDOR_ID_MINE, PCI_DEVICE_ID_MINE), },
  { 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static int pci_skel_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data);
static void register_proc( void );
static void unregister_proc( void );
static int probe(struct pci_dev *dev, const struct pci_device_id *id);
static void remove(struct pci_dev *dev);
static int pci_skel_ioctl(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg);
static int pci_skel_mmap(struct file *file,struct vm_area_struct *vma);

char* baseaddr=0;
/* char* ioaddr1=0; */
/* char* ioaddr2=0; */
/* char* iomap1=0; */
/* char* iomap2=0; */
unsigned long base=0L;//, io1=0L, io2=0L;

/* static struct resource *iores1; */
/* static struct resource *iores2; */
static struct proc_dir_entry *pci_skel_procdir;
static struct file_operations pci_skel_fops = 
{
  ioctl:    pci_skel_ioctl,
  mmap:     pci_skel_mmap
/*   open:     pci_skel_open, */
/*   release:  pci_skel_release  */
};

static struct pci_driver pci_driver = {
  .name = "pci_skel",
  .id_table = ids,
  .probe = probe,
  .remove = remove,
};


static unsigned char 
skel_get_revision(struct pci_dev *dev)
{
  u8 revision;

  pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
  return revision;
}

static int 
probe(struct pci_dev *dev, const struct pci_device_id *id)
{
  unsigned int ba=0;
  unsigned int revision=0;
  int pci_command=0;
  /* Do probing type stuff here.  
   * Like calling request_region();
   */
  pci_enable_device(dev);

  pci_read_config_dword(dev, PCI_COMMAND, &pci_command);
  pci_command |= (PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
  pci_write_config_dword(dev, PCI_COMMAND, pci_command);

  // read revision.
  pci_read_config_dword(dev, 0x8, &revision);

  printk("pci_skel: revision = (0x%x) %d\n",revision,revision);

  printk("pci_skel: skel_get_revision = (0x%x) %d\n",
	 skel_get_revision(dev),skel_get_revision(dev));
	
  if (skel_get_revision(dev) != 0x00)
    return -ENODEV;

  ba = pci_resource_start (dev, 2);
  printk("  ba = 0x%08x\n",ba);

  printk("  flags = 0x%lx\n",pci_resource_flags(dev,0));
  base = ba;
  baseaddr = (char*)ioremap_nocache(ba,2048);
  printk("  ioremap = 0x%08x\n",(unsigned int)baseaddr);

  ba = pci_resource_start (dev, 1);

/*   iores1 = request_mem_region(ba,256,"iores1"); */

/*   printk("  ba = 0x%08x\n",ba); */

/*   printk("  flags = 0x%lx\n",pci_resource_flags(dev,1)); */
/*   io1 = ba; */
/*   iomap1 = ioport_map(io1,256); */
/*   ioaddr1 = (char*)ioremap_nocache(ba,256); */

/*   printk("  ioremap = 0x%08x\n",(unsigned int)ioaddr1); */


/*   ba = pci_resource_start (dev, 2); */

/*   printk("  ba = 0x%08x\n",ba); */

/*   iores2 = request_mem_region(ba,256,"iores2"); */
/*   printk("  flags = 0x%lx\n",pci_resource_flags(dev,2)); */

/*   io2 = ba; */
/*   iomap2 = ioport_map(io2,256); */
/*   ioaddr2 = (char*)ioremap_nocache(ba,256); */

/*   printk("  ioremap = 0x%08x\n",(unsigned int)ioaddr2); */

  return 0;
}

static void 
remove(struct pci_dev *dev)
{
  /* clean up any allocated resources and stuff here.
   * like call release_region();
   */

/*   if(ioaddr1) */
/*     iounmap(ioaddr1); */

/*   if(ioaddr2) */
/*     iounmap(ioaddr2); */

/*   if(iores2) */
/*     release_mem_region(io2,256); */

/*   if(iomap2) */
/*     ioport_unmap(iomap2); */

/*   if(iores1) */
/*     release_mem_region(io1,256); */

/*   if(iomap1) */
/*     ioport_unmap(iomap1); */

  if(baseaddr)
    iounmap(baseaddr);

  printk("pci_skel.ko: Unloaded\n");
}

static int 
__init pci_skel_init(void)
{
  int rval=0;
  printk("PCI Skel driver loaded\n");

  rval = pci_register_driver(&pci_driver);

  printk("rval = %d\n",rval);
  ADD_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_DEVINIT);

  register_proc();

  ADD_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_PROC);

  if (register_chrdev(PCI_SKEL_MAJOR, "pci_skel", &pci_skel_fops)) 
    {
      printk("  Error getting Major Number for Drivers\n");
      return(-1);
    }

  ADD_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_CHR);
  
  return rval;
}

static void 
__exit pci_skel_exit(void)
{
  if (HAS_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_PROC)) 
    {
      unregister_proc();
      DEL_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_PROC);
    }

  if (HAS_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_CHR)) 
    {
      
      unregister_chrdev(PCI_SKEL_MAJOR, "pci_skel");
      DEL_RESOURCE(pci_skel_init_flags, PCI_SKEL_RSRC_CHR);
    }

  pci_unregister_driver(&pci_driver);
}

static int 
pci_skel_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
  char *p;
  int ireg=0;

  p = buf;
  p += sprintf(p,"  PCI Skeleton Driver\n");

  p += sprintf(p,"  base addr = %08X\n",(unsigned int)baseaddr);
/*   p += sprintf(p,"  io addr1  = %08X\n",(unsigned int)ioaddr1); */
/*   p += sprintf(p,"  io addr2  = %08X\n",(unsigned int)ioaddr2); */

/*   p += sprintf(p,"  io map1   = %08X\n",(unsigned int)iomap1); */
/*   p += sprintf(p,"  io map2   = %08X\n",(unsigned int)iomap2); */

  p += sprintf(p,"  Base Registers: \n");
  for(ireg=0; ireg<=0x18; ireg=ireg+0x4)
    {
      p += sprintf(p,"  0x%02x: = 0x%08X\n",ireg, ioread32(baseaddr+ireg));
    }

  for(ireg=0; ireg<=0x18; ireg=ireg+0x4)
    {
      iowrite32(0x12345678,baseaddr+ireg);
    }

  p += sprintf(p,"  After write:\n");
  for(ireg=0; ireg<=0x18; ireg=ireg+0x4)
    {
      p += sprintf(p,"  0x%02x: = 0x%08X\n",ireg, ioread32(baseaddr+ireg));
    }

/*   p += sprintf(p,"  I/O Port1 Registers: \n"); */
/*   for(ireg=0; ireg<=0x18; ireg=ireg+0x4) */
/*     p += sprintf(p,"  0x%08x: = 0x%08X\n",(unsigned int)io1+ireg, ioread32(iomap1+ireg)); */

/*   p += sprintf(p,"  I/O Port2 Registers: \n"); */
/*   for(ireg=0; ireg<=0x18; ireg=ireg+0x4) */
/*     p += sprintf(p,"  0x%08x: = 0x%08X\n",(unsigned int)io2+ireg, ioread32(iomap2+ireg)); */


  *eof = 1;
  return p - buf;
}

static void 
register_proc( void )
{
  pci_skel_procdir = create_proc_entry("pci_skel", S_IFREG | S_IRUGO, 0);
  pci_skel_procdir->read_proc = pci_skel_procinfo;
}

/*
 * The ioctl() implementation
 */

static int 
pci_skel_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
  int retval=0;
  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != PCI_SKEL_IOC_MAGIC) return -ENOTTY;
  if (_IOC_NR(cmd) > PCI_SKEL_IOC_MAXNR) return -ENOTTY;

  switch(cmd)
    {
    case PCI_SKEL_IOC_SET:
      iowrite32(0xffffffff,baseaddr);
      retval=1;
      break;

    case PCI_SKEL_IOC_ZERO:
      iowrite32(0x0,baseaddr);
      retval=2;
      break;

    default:  /* redundant, as cmd was checked against MAXNR */
      return -ENOTTY;
      
    }

  return retval;
}

static int 
pci_skel_mmap(struct file *file,struct vm_area_struct *vma)
{
/*   unsigned long req = iomap2; */
  unsigned long req = 0xd1c00000;
  unsigned long offset = (vma->vm_pgoff) << PAGE_SHIFT;
  unsigned long physical = req+offset;
  unsigned long vsize = vma->vm_end - vma->vm_start;
  unsigned long psize = req - offset;


  printk("%s: We're here\n",__FUNCTION__);

  /* Don't swap these pages out */
  vma->vm_flags |= VM_RESERVED | VM_IO;

/*   if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,  */
/* 		      vma->vm_end - vma->vm_start, vma->vm_page_prot)) { */
  if (remap_pfn_range(vma, vma->vm_start, physical, 
		      vsize, vma->vm_page_prot)) {
    printk("%s: remap_pfn_range failed \n",__FUNCTION__);
    return -EAGAIN;
  }

  printk("   Virt = 0x%08X, Phys = 0x%08X\n",
	 vma->vm_start,physical);


  return 0;
}

//----------------------------------------------------------------------------
//  unregister_proc()
//----------------------------------------------------------------------------
static void unregister_proc( void )
{
  remove_proc_entry("pci_skel",0);
}

MODULE_LICENSE("GPL");

module_init(pci_skel_init);
module_exit(pci_skel_exit);
