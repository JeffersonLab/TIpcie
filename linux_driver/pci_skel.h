#ifndef PCI_SKEL_H
#define PCI_SKEL_H

#define	HAS_RESOURCE(u,r)	( (u) &   (r) )
#define	ADD_RESOURCE(u,r)	( (u) |=  (r) )
#define	DEL_RESOURCE(u,r)	( (u) &= ~(r) )

#define PCI_SKEL_MAJOR          210

#define PCI_SKEL_IOC_MAGIC  'k'

#define PCI_SKEL_IOC_RW         _IO(PCI_SKEL_IOC_MAGIC, 1)

#define PCI_SKEL_IOC_MAXNR  1

#define PCI_SKEL_RW_WRITE 0
#define PCI_SKEL_RW_READ  1

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

#define PCI_SKEL_MEM0_SIZE      512
#define PCI_SKEL_MEM1_SIZE     2048
#define PCI_SKEL_MEM2_SIZE     1024

typedef struct pti_ioctl_struct
{
  int mem_region;
  int reg;
  int command_type;
  unsigned int value;
} PTI_IOCTL_INFO;


#endif /* PCI_SKEL_H */
