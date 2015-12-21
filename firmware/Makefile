#
# File:
#    Makefile
#
# Description:
#    Makefile for the TIpcie Firmware updating program
#
#

FWFILES	= $(shell cat .currentfw)

TIPCIELIB_PATH		= ../

CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I${TIPCIELIB_PATH}
CFLAGS			= -Wall -g ${INCS}\
			  -L. -L${TIPCIELIB_PATH}

PROGS			= TIpcieFirmwareUpdate
SRC			= $(PROGS:%=%.c)
DEPS			= $(SRC:%.c=%.d)

all: $(PROGS)

clean:
	@rm -vf $(PROGS) $(DEPS) *~

%: %.c
	@echo "Building $@ from $<"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt -lTIpcie

%.d: %.c
	@echo ""
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean
