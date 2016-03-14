#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLAB extensions to the GEFANUC Controller API
#
#
# $Date$
# $Rev$
#

ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}
# BUILD 64bit for EL7
ifneq (,$(findstring el7,${KERNEL_VERSION}))
	BUILD64=1
endif

ifdef BUILD64
CC			= gcc
else
CC			= gcc -m32
endif
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -g -I. -I../ -I/usr/include \
			  -L. -L../

PROGS			= tipReadoutTest tipLibTest pti_test pti_dma_test

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ 

pti_dma_test: pti_test.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $< -lrt

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt -lTIpcie

.PHONY: all clean distclean
