#ifndef TIPCIE_H
#define TIPCIE_H

#define	HAS_RESOURCE(u,r)	( (u) &   (r) )
#define	ADD_RESOURCE(u,r)	( (u) |=  (r) )
#define	DEL_RESOURCE(u,r)	( (u) &= ~(r) )

#define TIPCIE_MAJOR          210

#define TIPCIE_IOC_MAGIC  'k'

#define TIPCIE_IOC_RW         _IO(TIPCIE_IOC_MAGIC, 1)
#define TIPCIE_IOC_MEM        _IO(TIPCIE_IOC_MAGIC, 2)

#define TIPCIE_COMPAT_IOC_RW         _IO(TIPCIE_IOC_MAGIC, 1)
#define TIPCIE_COMPAT_IOC_MEM        _IO(TIPCIE_IOC_MAGIC, 2)

#define TIPCIE_IOC_MAXNR  2

#define TIPCIE_RW_WRITE 0
#define TIPCIE_RW_READ  1
#define TIPCIE_RW_STAT  2

#define TIPCIE_MEM_ALLOC 0
#define TIPCIE_MEM_FREE  1

#define TIPCIE_RSRC_MSI       (1U << 0)       /* MIS Interrupts enabled */
#define TIPCIE_RSRC_PCI_EN    (1U << 1)       /* PCI enabled          */
#define TIPCIE_RSRC_BRIDGE    (1U << 2)       /* Bridge initialized   */
#define TIPCIE_RSRC_DEVINIT   (1U << 3)       /* Device initialized   */
#define TIPCIE_RSRC_INTR      (1U << 4)       /* ISR installed	*/
#define TIPCIE_RSRC_CHR       (1U << 5)       /* Char dev initialized */
#define TIPCIE_RSRC_PROC      (1U << 6)       /* Proc initialized     */
#define TIPCIE_RSRC_MAP0      (1U << 7)       /* TIPCIE registers 0      */
#define TIPCIE_RSRC_MAP1      (1U << 8)       /* TIPCIE registers 1      */
#define TIPCIE_RSRC_MAP2      (1U << 9)       /* TIPCIE registers 2      */
#define TIPCIE_RSRC_MEM       (1U << 10)      /* Memory mapped for DMA */
#define TIPCIE_RSRC_CLASS     (1U << 11)
#define TIPCIE_RSRC_DEV       (1U << 12)

#define TIPCIE_MEM0_SIZE      512
#define TIPCIE_MEM1_SIZE     4096
#define TIPCIE_MEM2_SIZE     1024

#define TIPCIE_MAX_BLOCK_SIZE 512

#define TIPCIE_DMA_MAGIC 0x10e30001
#if !defined (phys_addr_t)
#define phys_addr_t unsigned long
#endif
struct DMA_HANDLE_STRUCT
{
  int             magic;
  dma_addr_t          resource;
  void                      *vptr;
  phys_addr_t         phys_addr;
  phys_addr_t          pci_addr;
  size_t                    size;
  struct DMA_HANDLE_STRUCT *next;                /* Pointer to the next handle */
};
typedef	struct DMA_HANDLE_STRUCT dmaHandle_t;

typedef struct DMA_BUF_INFO_STRUCT
{
  uint64_t  dma_osspec_hdl;
  uint64_t            command_type;
  uint64_t  phys_addr;
  uint64_t  virt_addr;
  uint64_t   size;
} DMA_BUF_INFO;

typedef struct DMA_BUF_COMPAT_INFO_STRUCT
{
  uint64_t  dma_osspec_hdl;
  uint64_t    command_type;
  uint64_t  phys_addr;
  uint64_t  virt_addr;
  uint64_t   size;
} DMA_BUF_COMPAT_INFO;

typedef struct TIpcie_ioctl_struct
{
  int             command_type;
  int             mem_region;
  unsigned int    nreg;
  unsigned int   *reg;
  unsigned int   *value;
} TIPCIE_IOCTL_INFO;

typedef struct TIpcie_compat_ioctl_struct
{
  compat_int_t   command_type;
  compat_int_t   mem_region;
  compat_uint_t  nreg;
  compat_uptr_t  reg;
  compat_uptr_t  value;
} TIPCIE_COMPAT_IOCTL_INFO;


#endif /* TIPCIE_H */
