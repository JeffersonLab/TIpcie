#ifndef PCI_SKEL_H
#define PCI_SKEL_H

#define	HAS_RESOURCE(u,r)	( (u) &   (r) )
#define	ADD_RESOURCE(u,r)	( (u) |=  (r) )
#define	DEL_RESOURCE(u,r)	( (u) &= ~(r) )

#define PCI_SKEL_MAJOR          210

#define PCI_SKEL_IOC_MAGIC  'k'

#define PCI_SKEL_IOC_SET    _IO(PCI_SKEL_IOC_MAGIC, 1)
#define PCI_SKEL_IOC_ZERO   _IO(PCI_SKEL_IOC_MAGIC, 2)

#define PCI_SKEL_IOC_MAXNR  2

#define PCI_SKEL_RSRC_FPGA      (1U << 0)       /* FPGA initialized     */
#define PCI_SKEL_RSRC_PCI_EN    (1U << 1)       /* PCI enabled          */
#define PCI_SKEL_RSRC_BRIDGE    (1U << 2)       /* Bridge initialized   */
#define PCI_SKEL_RSRC_DEVINIT   (1U << 3)       /* Device initialized   */
#define PCI_SKEL_RSRC_INTR      (1U << 4)       /* ISR installed	*/
#define PCI_SKEL_RSRC_CHR       (1U << 5)       /* Char dev initialized */
#define PCI_SKEL_RSRC_PROC      (1U << 6)       /* Proc initialized     */



#endif /* PCI_SKEL_H */
