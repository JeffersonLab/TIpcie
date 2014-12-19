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

CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -g -I. -I/usr/include \
			  -L.

PROGS			= pti_test pti_dma_test

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ *.so

pti_dma_test: pti_test.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $< -lrt

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt

.PHONY: all clean distclean
