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
	@rm -f $(PROGS) *~ *.so

pti_dma_test: pti_test.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $< -lrt

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt -lTIpcie

.PHONY: all clean distclean
