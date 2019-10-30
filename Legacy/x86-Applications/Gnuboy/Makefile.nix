#
# Makefile.nix
#
# This is a *bare minimum* makefile for building gnuboy on *nix systems.
# If you have trouble with the configure script you can try using this,
# but *please* try the configure script first. This file is mostly
# unmaintained and may break.
#
# If you *do* insist on using this makefile, you at least need to check
# SYS_DEFS below and uncomment -DIS_LITTLE_ENDIAN if your system is
# little endian. Also, you may want to enable the OSS sound module if
# your system supports it.
#

prefix = /usr/local
bindir = /bin

CC = gcc
AS = $(CC)
LD = $(CC)
INSTALL = /bin/install -c

CFLAGS = -O3
LDFLAGS = 
ASFLAGS = 

SYS_DEFS = #-DIS_LITTLE_ENDIAN
ASM_OBJS = 
#SND_OBJS = sys/oss/oss.o
SND_OBJS = sys/dummy/nosound.o
JOY_OBJS = sys/dummy/nojoy.o

TARGETS = xgnuboy

SYS_OBJS = sys/nix/nix.o $(ASM_OBJS) $(SND_OBJS) $(JOY_OBJS)
SYS_INCS = -I/usr/local/include -I/usr/X11R6/include -I./sys/nix

X11_OBJS = sys/x11/xlib.o sys/x11/keymap.o
X11_LIBS = -L/usr/X11R6/lib -lX11 -lXext

all: $(TARGETS)

include Rules

xgnuboy: $(OBJS) $(SYS_OBJS) $(X11_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(X11_OBJS) -o $@ $(X11_LIBS)

install: all
	$(INSTALL) -m 755 $(TARGETS) $(prefix)$(bindir)

clean:
	rm -f *gnuboy gmon.out *.o sys/*.o sys/*/*.o asm/*/*.o












