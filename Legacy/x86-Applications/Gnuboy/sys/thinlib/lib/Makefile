#
# Makefile
#
# thinlib library makefile
#
# Copyright (C) 2001 Matthew Conte (matt@conte.com)
#
# $Id: $

################################
# Configuration

CFLAGS      =  -W -Wall -Werror
DBGCFLAGS   =  -ggdb -DTHINLIB_DEBUG
OPTCFLAGS   =  -O3 -fomit-frame-pointer -ffast-math

# Assembler
ASM = nasm
ASMFLAGS    =  -f coff

DBGASMFLAGS =  -g

################################

# WANT_DEBUG = TRUE

################################

ifeq "$(WANT_DEBUG)" "TRUE"
	CFLAGS += $(DBGCFLAGS)
	ASMFLAGS += $(DBGASMFLAGS)
else
	CFLAGS += $(OPTCFLAGS)
endif

################################

CFILES = tl_main tl_log tl_timer tl_int tl_key tl_mouse tl_joy \
	tl_dpp tl_bmp tl_vesa tl_vga tl_video tl_sb tl_sound \
	tl_event tl_prof

CSRCS = $(addsuffix .c, $(CFILES))
OBJS = $(addsuffix .o, $(CFILES))

################################

.PHONY = all dep clean

all: libthin.a thintest.exe

clean:
	rm -f libthin.a thintest.exe $(OBJS) _dep

thintest.exe: thintest.cpp libthin.a
	$(CXX) -o $@ thintest.cpp -L. -lthin

libthin.a: $(OBJS)
	rm -f $@
	ar scru $@ $(OBJS)

dep: rmdep _dep

################################

rmdep:
	@rm -f _dep
	@echo "# dep file" > _dep
ifneq "$(CSRCS)" ""
	@$(foreach .a, $(CSRCS), $(CC) $(CFLAGS) -MM $(.a) >> _dep;)
endif
ifneq "$(ASMSRCS)" ""
	@$(foreach .a, $(ASMSRCS), $(ASM) $(ASMFLAGS) -M $(.a) >> _dep;)
endif

_dep:
# this is done so that we don't get all the no such file warnings
	@echo "# dep file" > _dep
ifneq "$(CSRCS)" ""
	@$(foreach .a, $(CSRCS), $(CC) $(CFLAGS) -MM $(.a) >> _dep;)
endif
ifneq "$(ASMSRCS)" ""
	@$(foreach .a, $(ASMSRCS), $(ASM) $(ASMFLAGS) -M $(.a) >> _dep;)
endif

include _dep

################################

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

################################

# $Log: $
