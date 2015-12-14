/* #include <linux/config.h> */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/pci_regs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include "TIpcie.h"


#define PCI_VENDOR_ID_DOE 0xD0E1
#define PCI_DEVICE_ID_TIPCIE 0x71E0

unsigned int TIpcie_init_flags;

static struct pci_device_id ids[] = {
  { PCI_DEVICE(PCI_VENDOR_ID_DOE, PCI_DEVICE_ID_TIPCIE), },
  { 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static void   clean_module(void);
static int TIpcie_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data);
static void register_proc( void );
static void unregister_proc( void );
/* static int probe(struct pci_dev *dev, const struct pci_device_id *id); */
static void remove(struct pci_dev *dev);
static int TIpcie_ioctl(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg);
static int TIpcie_mmap(struct file *file,struct vm_area_struct *vma);
int TIpcieAllocDmaBuf(DMA_BUF_INFO *dma_buf_info);
int TIpcieFreeDmaBuf(DMA_BUF_INFO *dma_buf_info);
static int dmaHandleListAdd(dmaHandle_t *dmahandle);
static int dmaHandleListRemove(dmaHandle_t *dmahandle);
int TIpcieFreeDmaHandles(void);
int TIpcie_read(void);

dmaHandle_t *dma_handle_list = NULL;

unsigned int pci_bar0=0;
unsigned int pci_bar1=0;
unsigned int pci_bar2=0;
char* TIpcie_resaddr0=0;
char* TIpcie_resaddr1=0;
char* TIpcie_resaddr2=0;
struct pci_dev *ti_pci_dev;
struct resource *tipcimem;
int ti_revision;
int ti_irq;
int TIpcie_interrupt_count;
int irq_flag=0;
int read_open=0;

struct semaphore dma_buffer_lock;
wait_queue_head_t  irq_queue;

/* static struct resource *iores1; */
/* static struct resource *iores2; */
static struct proc_dir_entry *TIpcie_procdir;
static struct file_operations TIpcie_fops = 
{
  ioctl:    TIpcie_ioctl,
  mmap:     TIpcie_mmap,
  read:     TIpcie_read,
/*   open:     TIpcie_open, */
/*   release:  TIpcie_release  */
};

static struct pci_driver pci_driver = {
  .name = "TIpcie",
  .id_table = ids,
/*   .probe = probe, */
  .remove = remove,
};


#ifdef NOMOREPROBE
static unsigned char 
skel_get_revision(struct pci_dev *dev)
{
  u8 revision;

  pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
  return revision;
}
#endif

irqreturn_t
ti_irqhandler(int irq, void *dev_id)
{
  ++TIpcie_interrupt_count;
  irq_flag=1;
  if(TIpcie_resaddr0)
    iowrite32(0, TIpcie_resaddr0 + 0x1C);

  if(read_open)
    wake_up_interruptible(&irq_queue);

  return IRQ_HANDLED;
}

int
TIpcie_read()
{
  unsigned long long before=0, after=0;
  read_open=1;
  
  rdtscl(before);
  wait_event_interruptible(irq_queue,irq_flag!=0);
  irq_flag=0;
  rdtscl(after);

  printk("%s:\n\tbefore = %lld\n\t after = %lld\n\tdiff = %lld\n",__FUNCTION__,
	 before,after,after-before);

  return 1;
}

static
struct pci_dev *
findTIpcie(void)
{
  struct pci_dev *ti_pci_dev = NULL;
  int pci_command;
  int status;

  if ((ti_pci_dev = pci_get_device(PCI_VENDOR_ID_DOE,
                                     PCI_DEVICE_ID_TIPCIE, 
                                     ti_pci_dev))) {
    status = pci_enable_device(ti_pci_dev);

    printk("  PCIexpress TI Found.\n");
  }

  if( ti_pci_dev == NULL) {
    printk("  PCIexpress T not found on PCI Bus.\n");
    return(ti_pci_dev);
  }

  // read revision.
  pci_read_config_dword(ti_pci_dev, PCI_REVISION_ID, &ti_revision);

  printk("  PCI_CLASS = %08x\n", ti_revision);

  ti_revision &= 0xFF;

  status = pci_enable_msi(ti_pci_dev);
  if (status) 
    {
      printk("%s: Unable to enable MSI\n", 
	     __FUNCTION__);
    } 

  // determine the TIPCIEchip IRQ number.
  if(ti_irq == 0)
    {
      ti_irq = ti_pci_dev->irq;
      
      printk("  PCI_INTLINE = %08x\n", ti_irq);
      
      ti_irq &= 0x000000FF;                    // Only a byte in size
      if(ti_irq == 0) ti_irq = 0x00000050;    // Only a byte in size
    }

  if((ti_irq == 0) || (ti_irq == 0xFF)){
    printk("Bad T IRQ number: %02x\n", ti_irq);
    return(NULL);
  }

  status = request_irq(ti_irq, 
		       ti_irqhandler, 
		       IRQF_SHARED, 
		       "TIpcie", ti_pci_dev);
  
  if (status) 
    {
      printk("  %s: can't get assigned pci irq vector %02X\n", 
	     __FUNCTION__,ti_irq);
      return(0);
    } 

  init_waitqueue_head(&irq_queue);

  // Ensure Bus mastering, IO Space, and Mem Space are enabled
  pci_read_config_dword(ti_pci_dev, PCI_COMMAND, &pci_command);
  pci_command |= (PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
  pci_write_config_dword(ti_pci_dev, PCI_COMMAND, pci_command);

  printk("  PCI_COMMAND = 0x%x\n",pci_command);

  return(ti_pci_dev);
}

int
mapInTIpcie(struct pci_dev *TIpcie_pci_dev)
{
/*   unsigned long temp; */
  unsigned long ba;

  ba = pci_resource_start (ti_pci_dev, 0);
  if(!ba)
    {
      printk("%s: ERROR: pci_resource_start failed\n",__FUNCTION__);
      return -1;
    }

  printk("  map TIpcie Mem0 to Kernel Space, physical_address: %0lx\n", (unsigned long)ba);
  pci_bar0 = ba;

  TIpcie_resaddr0 = (char *)ioremap_nocache(ba,TIPCIE_MEM0_SIZE);

  if (!TIpcie_resaddr0) {
    printk("  ioremap failed to map TIpcie to Kernel Space.\n");
    return 1;
  }

  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP0);
  printk("  mapped TIpcie Mem0 to Kernel Space, kernel_address: %0lx\n", 
            (unsigned long)TIpcie_resaddr0);

  ba = pci_resource_start (ti_pci_dev, 1);
  if(!ba)
    {
      printk("%s: ERROR: pci_resource_start failed\n",__FUNCTION__);
      return -1;
    }

  pci_bar1 = ba;
  printk("  map TIpcie Mem1 to Kernel Space, physical_address: %0lx\n", (unsigned long)ba);

  TIpcie_resaddr1 = (char *)ioremap_nocache(ba,TIPCIE_MEM1_SIZE);

  if (!TIpcie_resaddr1) {
    printk("  ioremap failed to map TIpcie to Kernel Space.\n");
    return 1;
  }

  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP1);
  printk("  mapped TIpcie Mem1 to Kernel Space, kernel_address: %0lx\n", 
            (unsigned long)TIpcie_resaddr1);

  ba = pci_resource_start (ti_pci_dev, 2);
  if(!ba)
    {
      printk("%s: ERROR: pci_resource_start failed\n",__FUNCTION__);
      return -1;
    }

  printk("  map TIpcie Mem2 to Kernel Space, physical_address: %0lx\n", (unsigned long)ba);

  pci_bar2 = ba;
  TIpcie_resaddr2 = (char *)ioremap_nocache(ba,TIPCIE_MEM2_SIZE);

  if (!TIpcie_resaddr2) {
    printk("  ioremap failed to map TIpcie to Kernel Space.\n");
    return 1;
  }

  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP2);
  printk("  mapped TIpcie Mem2 to Kernel Space, kernel_address: %0lx\n", 
            (unsigned long)TIpcie_resaddr2);

#ifdef SKIPTHIS
  // Check to see if the Mapping Worked out
  temp = ioread32(TIpcie_baseaddr) & 0x0000FFFF;
  printk("temp = 0x%0lx\n",temp);

  return 0;
  if (temp != PCI_VENDOR_ID_DOE) {
    printk("  TIpcie Failed to Return PCI_ID in Memory Map. (temp = 0x%0lx)\n",temp);
    iounmap(TIpcie_baseaddr);
    DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP0);
    return 1;
  }
  printk("mapped regs for %lx\n",temp); 
#endif


  return(0);

}

#ifdef NOMOREPROBE
/* Not using this function, for now */
static int 
probe(struct pci_dev *dev, const struct pci_device_id *id)
{
  unsigned long ba=0;
  unsigned int revision=0;
  int status;
  int pci_command=0;
  struct pci_dev *ti_pci_dev=NULL;
  /* Do probing type stuff here.  
   * Like calling request_region();
   */

  if ((ti_pci_dev = pci_get_device(PCI_VENDOR_ID_DOE,
				    PCI_DEVICE_ID_TIPCIE, 
				    ti_pci_dev))) {
    status = pci_enable_device(ti_pci_dev);
    
    printk("  PCIexpress TI Found.\n");
  }


  pci_read_config_dword(dev, PCI_COMMAND, &pci_command);
  pci_command |= (PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
  pci_write_config_dword(dev, PCI_COMMAND, pci_command);

  // read revision.
  pci_read_config_dword(dev, 0x8, &revision);

  printk("TIpcie: revision = (0x%x) %d\n",revision,revision);

  printk("TIpcie: skel_get_revision = (0x%x) %d\n",
	 skel_get_revision(dev),skel_get_revision(dev));
	
  if (skel_get_revision(dev) != 0x00)
    return -ENODEV;

  ba = pci_resource_start (dev, 2);
  printk("  ba = 0x%0lx\n",ba);

  printk("  flags = 0x%lx\n",pci_resource_flags(dev,0));
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
#endif /* NOMOREPROBE */

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

/*   if(TIpcie_baseaddr) */
/*     iounmap(TIpcie_baseaddr); */

  printk("TIpcie.ko: Unloaded\n");
}

static int 
__init TIpcie_init(void)
{
  int rval=0;
  unsigned int temp, status;
  struct resource pcimemres;
  printk("TIpcie driver loaded\n");

  rval = pci_register_driver(&pci_driver);

  printk("rval = %d\n",rval);
  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_DEVINIT);

  register_proc();

  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_PROC);

  if (register_chrdev(TIPCIE_MAJOR, "TIpcie", &TIpcie_fops)) 
    {
      printk("TIpcie:  Error getting Major Number for Drivers\n");
      return(-1);
    }

  ADD_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_CHR);

  ti_pci_dev = findTIpcie();
  if(ti_pci_dev==NULL)
    {
      printk("TIpcie: Failure finding PCIexpress TI\n");
      goto BailOut;
    }

  /* Get parent PCI resource & verify enough space is available. */
  memset(&pcimemres,0,sizeof(pcimemres));

  pcimemres.flags = IORESOURCE_MEM;
  tipcimem = pci_find_parent_resource(ti_pci_dev, &pcimemres);
  if(tipcimem == 0){
    printk("TIpcie: Can't get TI parent device PCI resource\n");
    goto BailOut;
  }
  
  printk("TIpcie: Initial PCI MEM start: 0x%0lx end: 0x%0lx\n", 
	 (unsigned long)tipcimem->start, (unsigned long)tipcimem->end);

  // Map in TIpcie registers.
  if(mapInTIpcie(ti_pci_dev)){
      printk("TIpcie: Bridge not initialized\n");
    goto BailOut;
  }

  /* Display TIpcie information */  
  pci_read_config_dword(ti_pci_dev, PCI_COMMAND, &status);

  printk("  Vendor = %04X  Device = %04X  Revision = %02X Status = %08X\n",
         ti_pci_dev->vendor,ti_pci_dev->device, ti_revision, status);
  printk("  Class = %08X\n",ti_pci_dev->class);

  pci_read_config_dword(ti_pci_dev, PCI_CACHE_LINE_SIZE, &temp);

  printk("  Misc0 = %08X\n",temp);      
  printk("  Irq = %04X\n",ti_irq);

  sema_init(&dma_buffer_lock, 1);

  return rval;

 BailOut:
  printk(" Bailing out of initialization\n");
  clean_module();
  return -1;
  
}

static void
clean_module(void)
{
  if (HAS_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_PROC)) 
    {
      unregister_proc();
      DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_PROC);
    }

  if (HAS_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_CHR)) 
    {
      unregister_chrdev(TIPCIE_MAJOR, "TIpcie");
      DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_CHR);
    }

  if (HAS_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP0)) 
    {
      iounmap(TIpcie_resaddr0);
      DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP0);
    }

  if (HAS_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP1)) 
    {
      iounmap(TIpcie_resaddr1);
      DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP1);
    }

  if (HAS_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP2)) 
    {
      iounmap(TIpcie_resaddr2);
      DEL_RESOURCE(TIpcie_init_flags, TIPCIE_RSRC_MAP2);
    }


  pci_unregister_driver(&pci_driver);
}

static void 
__exit TIpcie_exit(void)
{
  clean_module();
}

static int 
TIpcie_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
  char *p;
/*   int ireg=0; */

  p = buf;
  p += sprintf(p,"  PCIexpress TI Driver\n");

  p += sprintf(p,"  pci0 addr = %0lx\n",(unsigned long)pci_bar0);
  p += sprintf(p,"  pci1 addr = %0lx\n",(unsigned long)pci_bar1);
  p += sprintf(p,"  pci2 addr = %0lx\n",(unsigned long)pci_bar2);

  p += sprintf(p,"  mem0 addr = %0lx\n",(unsigned long)TIpcie_resaddr0);
  p += sprintf(p,"  mem1 addr = %0lx\n",(unsigned long)TIpcie_resaddr1);
  p += sprintf(p,"  mem2 addr = %0lx\n",(unsigned long)TIpcie_resaddr2);

  p += sprintf(p," interrupts = %d\n",TIpcie_interrupt_count);

#ifdef SKIPTHIS
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
#endif

  *eof = 1;
  return p - buf;
}

static void 
register_proc( void )
{
  TIpcie_procdir = create_proc_entry("TIpcie", S_IFREG | S_IRUGO, 0);
  TIpcie_procdir->read_proc = TIpcie_procinfo;
}

/*
 * The ioctl() implementation
 */

static int 
TIpcie_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
  TIPCIE_IOCTL_INFO user_info;
  DMA_BUF_INFO dma_info;
  int ireg=0, retval=0;
  unsigned int *regs = NULL;
  unsigned int *values = NULL;
  int stat=0;

  /*
   * extract the type and number bitfields, and don't decode
   * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
   */
  if (_IOC_TYPE(cmd) != TIPCIE_IOC_MAGIC) return -ENOTTY;
  if (_IOC_NR(cmd) > TIPCIE_IOC_MAXNR) return -ENOTTY;


  switch(cmd)
    {
    case TIPCIE_IOC_RW:
      {
	if(copy_from_user(&user_info, (void *)arg, sizeof(TIPCIE_IOCTL_INFO)))
	  {
	    printk("TIpcie: copy_from_user (user_info) failed\n");
	    return -EFAULT;
	  }

	regs   = (unsigned int*)kmalloc(user_info.nreg*sizeof(user_info.reg), GFP_KERNEL);
	values = (unsigned int*)kmalloc(user_info.nreg*sizeof(user_info.value), GFP_KERNEL);

	stat = copy_from_user(regs, user_info.reg, user_info.nreg*sizeof(user_info.reg));
	if(stat)
	  {
	    printk("TIpcie: copy_from_user (regs) failed (%d)\n",stat);
	    return -EFAULT;
	  }

	if(copy_from_user(values, user_info.value, user_info.nreg*sizeof(unsigned int)))
	  {
	    printk("TIpcie: copy_from_user (values) failed\n");
	    return -EFAULT;
	  }


#ifdef DEBUGDRIVER
	printk("%s:\n",__FUNCTION__);
	printk("   mem_region = 0x%x\n",user_info.mem_region);
	printk(" command_type = 0x%x\n",user_info.command_type);
	for(ireg=0; ireg<user_info.nreg; ireg++)
	  {
	    printk("          reg = 0x%x\n",regs[ireg]);
	    if(user_info.command_type==TIPCIE_RW_WRITE)
	      {
		printk("        value = 0x%x\n",values[ireg]);
	      }
	  }
#endif /* DEBUGDRIVER */

	if(user_info.command_type==TIPCIE_RW_WRITE)
	  {
	    switch(user_info.mem_region)
	      {
	      case 0:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM0_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    iowrite32(values[ireg], TIpcie_resaddr0 + regs[ireg]);
		  }
		break;

	      case 1:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM1_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    iowrite32(values[ireg], TIpcie_resaddr1 + regs[ireg]);
		  }
		break;

	      case 2:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM2_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    iowrite32(values[ireg], TIpcie_resaddr2 + regs[ireg]);
		  }
		break;

	      default:
		{
		  printk("TIpcie: Bad memory region (%d)\n",user_info.mem_region);
		  return -ENOTTY;
		}

	      }
	  }
	else if(user_info.command_type==TIPCIE_RW_READ)
	  {
	    switch(user_info.mem_region)
	      {
	      case 0:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM0_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    values[ireg] = ioread32(TIpcie_resaddr0 + regs[ireg]);
		  }
		break;

	      case 1:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM1_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    values[ireg] = ioread32(TIpcie_resaddr1 + regs[ireg]);
		  }
		break;

	      case 2:
		for(ireg=0; ireg<user_info.nreg; ireg++)
		  {
		    if(regs[ireg]>TIPCIE_MEM2_SIZE)
		      {
			printk("TIpcie: BAR%d Bad register offset (0x%x)\n",
			       user_info.mem_region,regs[ireg]);
			return -ENOTTY;
		      }
		    
		    values[ireg] = ioread32(TIpcie_resaddr2 + regs[ireg]);
		  }
		break;

	      default:
		{
		  printk("TIpcie: Bad memory region (%d)\n",user_info.mem_region);
		  return -ENOTTY;
		}
	      }

	  }
	else if(user_info.command_type==TIPCIE_RW_STAT)
	  {
	    values[0] = pci_bar0;
	    values[1] = pci_bar1;
	    values[2] = pci_bar2;
	  }
	else
	  {
	    printk("TIpcie: Bad RW option (%d)\n",user_info.command_type);
	    return -ENOTTY;
	  }

	if(copy_to_user(user_info.value, values, user_info.nreg*sizeof(unsigned int)))
	  {
	    printk("TIpcie: copy_to_user (values) failed\n");
	    return -EFAULT;
	  }
	
	
	if(copy_to_user((void *)arg, &user_info, sizeof(TIPCIE_IOCTL_INFO)))
	  {
	    printk("TIpcie: copy_to_user (user_info) failed\n");
	    return -EFAULT;
	  }
	
      }
      break;

    case TIPCIE_IOC_MEM:
      {
	if(copy_from_user(&dma_info, (void *)arg, sizeof(dmaHandle_t)))
	  {
	    printk("TIpcie: copy_from_user (dma_info) failed\n");
	    return -EFAULT;
	  }

	if(dma_info.command_type==TIPCIE_MEM_ALLOC)
	  {
	    TIpcieAllocDmaBuf(&dma_info);	
	  }
	else if(dma_info.command_type==TIPCIE_MEM_FREE)
	  {
	    TIpcieFreeDmaBuf(&dma_info);
	  }
	else
	  {
	    printk("TIpcie: Bad MEM option (%d)\n",dma_info.command_type);
	    return -ENOTTY;
	  }

	if(copy_to_user((void *)arg, &dma_info, sizeof(dmaHandle_t)))
	  {
	    printk("TIpcie: copy_to_user (dma_info) failed\n");
	    return -EFAULT;
	  }
      }
      break;

    default:  /* redundant, as cmd was checked against MAXNR */
      {
	printk("TIpcie: Unrecognized command (%d).\n",cmd);
	return -ENOTTY;
      }
    }

  kfree(values);
  kfree(regs);

  return retval;
}

static int 
TIpcie_mmap(struct file *file,struct vm_area_struct *vma)
{

  /* Don't swap these pages out */
  vma->vm_flags |= VM_RESERVED | VM_IO;

  if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		      vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
    printk("%s: remap_pfn_range failed \n",__FUNCTION__);
    return -EAGAIN;
  }

  printk("%s:   Virt = 0x%0lx, Phys = 0x%0lx\n",
	 __FUNCTION__,
	 vma->vm_start,vma->vm_pgoff);

  return 0;
}

//----------------------------------------------------------------------------
//  TIpcieAllocDmaBuf()
//    Allocate contiguous DMA buffer.
//----------------------------------------------------------------------------

int 
TIpcieAllocDmaBuf(DMA_BUF_INFO *dma_buf_info) 
{
    
  dmaHandle_t *dmahandle = NULL;
  struct page *page;
  int status;

  if (NULL == dma_buf_info)
    {
      return(-1);
    }


  if(down_interruptible(&dma_buffer_lock))
    {
      return(-1);
    }
  dmahandle = (dmaHandle_t *)kmalloc(sizeof (dmaHandle_t), GFP_KERNEL);

  if (NULL == dmahandle)
    {
      up(&dma_buffer_lock);
      return(-1);
    }

  memset(dmahandle, 0, sizeof(dmaHandle_t));
    
  dma_buf_info->dma_osspec_hdl = (unsigned long) dmahandle;
    
  dmahandle->size = dma_buf_info->size;

  dmahandle->vptr = pci_alloc_consistent(ti_pci_dev, dma_buf_info->size,
					 &dmahandle->resource);

  if ((NULL == dmahandle->vptr)) 
    {
      printk("%s: pci_alloc_consistent failed\n",
	     __FUNCTION__);
      up(&dma_buffer_lock);
      return(-1);
    } 

  printk("%s: pci_alloc_consistent virtual buffer pointer %#lx\n",
	 __FUNCTION__,(unsigned long)dmahandle->vptr);

  for (page = virt_to_page(dmahandle->vptr);
       page <= virt_to_page(dmahandle->vptr + dma_buf_info->size - 1);
       ++page)
    {
      SetPageReserved(page);		
    }
    
  dmahandle->phys_addr = virt_to_phys(dmahandle->vptr);
  dmahandle->pci_addr = dmahandle->phys_addr;

  // Set return data for user space
  dma_buf_info->virt_addr = 0;
  dma_buf_info->phys_addr = (unsigned long) dmahandle->phys_addr;


  printk("%s: %#x byte DMA buffer allocated with physical "
	 "addr %#llx pci addr %#llx resource addr %#llx\n", 
	 __FUNCTION__,
	 dma_buf_info->size,
	 (u64) dmahandle->phys_addr,
	 (u64) dmahandle->pci_addr,
	 (u64) dmahandle->resource);
  
  dmahandle->magic = TIPCIE_DMA_MAGIC;
    
  // Expects dma_buffer_lock acquired
  status = dmaHandleListAdd(dmahandle);

  up(&dma_buffer_lock);

  return (status);
    
}

//----------------------------------------------------------------------------
//  TIpcieFreeDmaBuf()
//    Free contiguous DMA buffer.
//----------------------------------------------------------------------------

int 
TIpcieFreeDmaBuf(DMA_BUF_INFO *dma_buf_info) 
{

  int status;
  struct page *page;
  dmaHandle_t *dmahandle = NULL;
    
  if (NULL == dma_buf_info)
    {
      printk("%s: failure: NULL == dma_buf_info\n",
	     __FUNCTION__); 
      return(-1);
    }
    
  dmahandle = (dmaHandle_t *) dma_buf_info->dma_osspec_hdl; 
    
  if (NULL == dmahandle)
    {
      printk("%s: failure: NULL == dmahandle\n",
	     __FUNCTION__); 
      return(-1);
    }    
    
  if (TIPCIE_DMA_MAGIC != dmahandle->magic)
    {
      printk("%s: failure: TIPCIE_DMA_MAGIC != dmahandle->magic\n",
	     __FUNCTION__); 
      return(-1);
    }
    
  printk("%s: Freeing %#llx byte DMA buffer allocated with physical "
		"addr %#llx pci addr %#llx resource addr %#llx\n", 
	 __FUNCTION__,
	 (u64) dmahandle->size,
	 (u64) dmahandle->phys_addr,
	 (u64) dmahandle->pci_addr,
	 (u64) dmahandle->resource);
     
  if(down_interruptible(&dma_buffer_lock))
    {
      return(-1);
    }
    
  for (page = virt_to_page(dmahandle->vptr);
       page <= virt_to_page(dmahandle->vptr + dmahandle->size - 1);
       ++page)              
    {
        
      ClearPageReserved(page);
    }
    
  pci_free_consistent(ti_pci_dev, dmahandle->size,
		      dmahandle->vptr, 
		      dmahandle->resource);
  

  // Expects dma_buffer_lock acquired
  status = dmaHandleListRemove(dmahandle);
    
  up(&dma_buffer_lock);
    
  return (status);
    
}

//----------------------------------------------------------------------------
//  dmaHandleListAdd
//    Add handle for contiguous kernel-allocated DMA buffer to list.
//    Expects dma_buffer_lock acquired.
//----------------------------------------------------------------------------

static int 
dmaHandleListAdd(dmaHandle_t *dmahandle)
{
  if (NULL == dmahandle) 
    {
      return(-1);
    }
    
  if (NULL == dma_handle_list)
    {
      dma_handle_list = dmahandle;
      dmahandle->next = NULL;
    } 
  else
    {
      dmahandle->next = dma_handle_list;
      dma_handle_list = dmahandle;
    }
    
  return 0;
}

//----------------------------------------------------------------------------
//  dmaHandleListRemove
//    Remove handle for contiguous kernel-allocated DMA buffer to list.
//    Expects dma_buffer_lock acquired.
//----------------------------------------------------------------------------

static int 
dmaHandleListRemove(dmaHandle_t *dmahandle)
{
  int status;
  dmaHandle_t *search_hdl;
  dmaHandle_t *prev_hdl;
  int found=0;
    
  if (NULL == dma_handle_list) 
    {
      return(-1);
    }
    
  search_hdl = dma_handle_list;
  prev_hdl = dma_handle_list;
    
  found = 0;
  while ((found == 0) && (search_hdl != NULL))
    {
      if ((search_hdl == dmahandle))
        {
	  found = 1;
        } else
        {
	  prev_hdl = search_hdl;
	  search_hdl = search_hdl->next;
        }
    }
    
  if (found == 1)
    {
      // Check for head of list
      if (prev_hdl == search_hdl)
        {
	  dma_handle_list = search_hdl->next;
        }
      else 
        {
	  prev_hdl->next = search_hdl->next;    
        }
      memset(search_hdl, 0, sizeof (dmaHandle_t));
      kfree(search_hdl);
      search_hdl = NULL;
        
        
      status = 0;
    } 
  else
    {
      status = -1;
    }
    
  return (status);
}


//----------------------------------------------------------------------------
//  TIpcieFreeDmaHandles()
//    Frees DMA buffers and handles on driver shutdown.
//----------------------------------------------------------------------------

int 
TIpcieFreeDmaHandles(void) 
{
  int status = 0;
  DMA_BUF_INFO dma_buf_info;

  printk("%s: removing DMA buffers and handles.\n",
	 __FUNCTION__);
    
  while ((NULL != dma_handle_list) && (status == 0))
    {
      dma_buf_info.dma_osspec_hdl = (unsigned long) dma_handle_list;
      status = TIpcieFreeDmaBuf(&dma_buf_info);

      if (status != 0)
        {
	  printk("TIPCIE:DMA handle removal failure: 0x%x\n",status);
        }
    }

  return(0);
}



//----------------------------------------------------------------------------
//  unregister_proc()
//----------------------------------------------------------------------------
static void unregister_proc( void )
{
  remove_proc_entry("TIpcie",0);
}

MODULE_LICENSE("GPL");

module_init(TIpcie_init);
module_exit(TIpcie_exit);
