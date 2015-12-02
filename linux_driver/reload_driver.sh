#!/bin/bash
#
# Reload the pci_skel driver
#

#cd /home/moffit/work/pti/linux_driver/

if [ ! -e /dev/pci_skel ] ; then
    /bin/mknod /dev/pci_skel c 210 32
    /bin/chown root /dev/pci_skel
    /bin/chgrp users /dev/pci_skel
    /bin/chmod 666 /dev/pci_skel
else
    /sbin/rmmod pci_skel.ko
fi

/sbin/insmod pci_skel.ko

