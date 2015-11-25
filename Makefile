#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLab TIpcie Trigger Interface module
#    using PC running Linux
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG=1
#
#
BASENAME=TIpcie

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L.
INCS			= -I.

LIBS			= lib${BASENAME}.a

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= ${BASENAME}Lib.c
HDRS			= $(SRC:.c=.h)
OBJ			= ${BASENAME}Lib.o
DEPS			= $(SRC:.c=.d)

all: echoarch $(LIBS)

$(OBJ): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $(SRC)

$(LIBS): $(OBJ)
	$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)
	$(AR) ruv $@ $<
	$(RANLIB) $@

#links: $(LIBS)
#	@ln -vsf $(PWD)/$< $(LINUXVME_LIB)/$<
#	@ln -vsf $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
#	@ln -vsf ${PWD}/*Lib.h $(LINUXVME_INC)

#install: $(LIBS)
#	@cp -v $(PWD)/$< $(LINUXVME_LIB)/$<
#	@cp -v $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
#	@cp -v ${PWD}/*Lib.h $(LINUXVME_INC)

%.d: %.c
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

clean:
	@rm -vf ${BASENAME}Lib.{o,d} lib${BASENAME}.{a,so}

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
