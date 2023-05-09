#
# File:
#    Makefile
#
# Description:
#    Makefile for the TIpcie test programs
#
#
DEBUG	?= 1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

ARCH=${shell uname -m}

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I../
CFLAGS			= -Wall -g -lstdc++ -L. -L../ -lrt -lTIpcie
ifeq ($(DEBUG),1)
	CFLAGS		+= -Wall -Wno-unused -g
endif

SRC			= $(wildcard *.c)
DEPS			= $(SRC:.c=.d)
PROGS			= $(SRC:.c=)

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~

%: %.c
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -o $@ $<

%.d: %.c
	@echo " DEP    $@"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean distclean

echoarch:
	@echo "Make for $(ARCH)"
