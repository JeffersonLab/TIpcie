#
# File:
#    Makefile
#
# Description:
#    Makefile for the coda primary and secondary readout lists 
#    running on an Intel-based controller running Linux
#
#
# $Date$
# $Rev$
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
#
TIPCIE_PATH		= /daqfs/home/moffit/work/TIpcie

# DEFs for compiling CODA readout lists
CC			= gcc -m32
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I. -I${TIPCIE_PATH} -I${CODA}/common/include
CODA_LIBDIRS            = -L. -L${TIPCIE_PATH} 
CODA_LIBS		= -lTIpcie
CODA_DEFS		= -DLINUX -DDAYTIME=\""`date`"\"
CODA_CFLAGS		= -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
ifdef DEBUG
CODA_CFLAGS		+= -Wall -g
else
CODA_CFLAGS		+= -O
endif
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)
DEPS			= $(CRLFILES:.crl=.d)

all: $(SOBJS) 

%.so: %.c
	@echo
	@echo "Building $@ from $<"
	@echo
	${CC} ${CODA_CFLAGS} -o $@ $<

%.c: %.crl
	@echo
	@echo "Converting $< to $@"
	@${CCRL} $<

clean:
	@rm -f  $(ROLS) $(SOBJS) $(CFILES) ${DEPS} ${DEPS}.* *~ 

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(CODA_INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean
