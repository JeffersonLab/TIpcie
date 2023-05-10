#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLab TIpcie Trigger Interface module
#    using PC running Linux
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG	?= 1
QUIET	?= 1

#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif
#
#
BASENAME=TIpcie
ARCH=${shell uname -m}
KERNEL_VERSION=${shell uname -r}

CC			= gcc
CXX			= g++
ifeq ($(ARCH),i686)
CC			+= -m32
CXX			+= -m32
endif
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -fpic -L.
INCS			= -I.

LIBS			= lib${BASENAME}.a lib${BASENAME}.so

ifdef DEBUG
CFLAGS			+= -Wall -Wno-unused -g
else
CFLAGS			+= -O2
endif
SRC			= ${BASENAME}Lib.c ${BASENAME}Config.cpp
HDRS			= ${BASENAME}Lib.h ${BASENAME}Config.h
OBJ			= $(HDRS:%.h=%.o)
DEPS			= $(HDRS:%.h=%.d)

all: echoarch $(LIBS)

%.o: %.c
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

%.o: %.cpp
	@echo " CXX    $@"
	${Q}$(CXX) $(CFLAGS) -std=c++11 $(INCS) -c -o $@ $<

%.so: $(OBJ)
	@echo " CC     $@"
	${Q}$(CC) -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(OBJ)

%.a: $(OBJ)
	@echo " AR     $@"
	${Q}$(AR) ru $@ $<
	@echo " RANLIB $@"
	${Q}$(RANLIB) $@

%.d: %.c
	@echo " DEP    $@"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d: %.cpp
	@echo " DEP    $@"
	@set -e; rm -f $@; \
	$(CXX) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

clean:
	@rm -vf ${OBJ} ${DEPS} ${LIBS}

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
