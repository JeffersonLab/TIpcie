#!/bin/bash
#
# Reload the TIpcie driver
#

#cd /home/moffit/work/pti/linux_driver/

if [ ! -e /dev/TIpcie ] ; then
    /bin/mknod /dev/TIpcie c 210 32
    /bin/chown root /dev/TIpcie
    /bin/chgrp users /dev/TIpcie
    /bin/chmod 666 /dev/TIpcie
else
    /sbin/rmmod TIpcie.ko
fi

/sbin/insmod TIpcie.ko

