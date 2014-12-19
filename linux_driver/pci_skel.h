#ifndef PCI_SKEL_H
#define PCI_SKEL_H

#define	HAS_RESOURCE(u,r)	( (u) &   (r) )
#define	ADD_RESOURCE(u,r)	( (u) |=  (r) )
#define	DEL_RESOURCE(u,r)	( (u) &= ~(r) )

#define PCI_SKEL_MAJOR          210

#define PCI_SKEL_IOC_MAGIC  'k'

#define PCI_SKEL_IOC_RW         _IO(PCI_SKEL_IOC_MAGIC, 1)
#define PCI_SKEL_IOC_MEM        _IO(PCI_SKEL_IOC_MAGIC, 2)

#define PCI_SKEL_IOC_MAXNR  2

#define PCI_SKEL_RW_WRITE 0
#define PCI_SKEL_RW_READ  1

#define PCI_SKEL_MEM_ALLOC 0
#define PCI_SKEL_MEM_FREE  1

#define PCI_SKEL_RSRC_FPGA      (1U << 0)       /* FPGA initialized     */
#define PCI_SKEL_RSRC_PCI_EN    (1U << 1)       /* PCI enabled          */
#define PCI_SKEL_RSRC_BRIDGE    (1U << 2)       /* Bridge initialized   */
#define PCI_SKEL_RSRC_DEVINIT   (1U << 3)       /* Device initialized   */
#define PCI_SKEL_RSRC_INTR      (1U << 4)       /* ISR installed	*/
#define PCI_SKEL_RSRC_CHR       (1U << 5)       /* Char dev initialized */
#define PCI_SKEL_RSRC_PROC      (1U << 6)       /* Proc initialized     */
#define PCI_SKEL_RSRC_MAP0      (1U << 7)       /* PTI registers 0      */
#define PCI_SKEL_RSRC_MAP1      (1U << 8)       /* PTI registers 1      */
#define PCI_SKEL_RSRC_MAP2      (1U << 9)       /* PTI registers 2      */
#define PCI_SKEL_RSRC_MEM       (1U << 10)      /* Memory mapped for DMA */

#define PCI_SKEL_MEM0_SIZE      512
#define PCI_SKEL_MEM1_SIZE     4096
#define PCI_SKEL_MEM2_SIZE     1024

#define PCI_SKEL_MAX_BLOCK_SIZE 512

#define PCI_SKEL_DMA_MAGIC 0x10e30001
#if !defined (phys_addr_t)
#define phys_addr_t unsigned long
#endif
struct DMA_HANDLE_STRUCT
{
  int                      magic;
  dma_addr_t            resource;
  void                     *vptr;
  phys_addr_t          phys_addr;
  phys_addr_t           pci_addr;
  size_t                    size;
  struct DMA_HANDLE_STRUCT *next;                /* Pointer to the next handle */
};
typedef	struct DMA_HANDLE_STRUCT dmaHandle_t;

typedef struct DMA_BUF_INFO_STRUCT
{
  unsigned long  dma_osspec_hdl;
  int              command_type;
  unsigned long       phys_addr;
  unsigned long       virt_addr;
  unsigned int             size;
} DMA_BUF_INFO;

typedef struct pti_ioctl_struct
{
  int    command_type;
  int      mem_region;
  unsigned int   nreg;
  unsigned int   *reg;
  unsigned int *value;
} PTI_IOCTL_INFO;


#endif /* PCI_SKEL_H */
