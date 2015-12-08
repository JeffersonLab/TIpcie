#
# File:
#    Makefile
#
# Description:
#    Makefile for the coda primary and secondary readout lists 
#    running on an Intel-based controller running Linux
#
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
#

# Plug in your primary readout lists here..
ROL			= tipcie_list.so
# Add shared library dependencies here.  (TIpcie already included)
ROLLIBS			= 

# DEFs for compiling primary readout lists
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -g
else
CFLAGS			= -O3
endif
CFLAGS			+= -w -I. -I/usr/include -I${CODA}/common/include\
			  -L. -DJLAB
CFLAGS			+= -lrt -lpthread -lTIpcie $(ROLLIBS)
CFLAGS			+= -DLINUX -DDAYTIME=\""`date`"\"

# DEFs for compiling CODA readout lists
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I. -I${CODA}/common/include
CODA_LIBDIRS            = -L. 
CODA_LIBS		= -lTIpcie
CODA_DEFS		= -DLINUX -DDAYTIME=\""`date`"\"
CODA_CFLAGS		= -O -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
ifdef DEBUG
CODA_CFLAGS		+= -Wall -g
endif
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)


all: $(ROL) $(SOBJS) 

event_list.c: event_list.crl Makefile
	@echo
	@echo "Converting $< to $@"
	@${CCRL} $<

event_list.so: event_list.c
	@echo
	@echo "Building $@ from $<"
	@echo
	${CC} ${CODA_CFLAGS} -o $@ $<

%.so: %.c Makefile tiprimary_list.c
	@echo
	@echo "Building $@ from $<"
	@echo
	$(CC) -fpic -shared  $(CFLAGS) -DINIT_NAME=$(@:.so=__init) -o $@ $<

clean distclean:
	@rm -f  $(ROL) $(SOBJS) $(CFILES) *~ 

.PHONY: all
