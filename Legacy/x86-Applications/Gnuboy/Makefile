
prefix = ./lemon
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

CC = i686-lemon-gcc
LD = $(CC)
AS = $(CC)
INSTALL = /usr/bin/install -c

CFLAGS =  -Wall -O3
LDFLAGS = $(CFLAGS)  -s
ASFLAGS = $(CFLAGS)

TARGETS =  xgnuboy fbgnuboy sdlgnuboy lemongnuboy

ASM_OBJS = 

SYS_DEFS = -DHAVE_CONFIG_H -DIS_LITTLE_ENDIAN  -DIS_LINUX -DIS_LEMON
SYS_OBJS = $(ASM_OBJS)
SYS_INCS = -I./sys/lemon

FB_OBJS = sys/linux/fbdev.o sys/linux/kb.o sys/pc/keymap.o sys/linux/joy.o sys/oss/oss.o
FB_LIBS = 

SVGA_OBJS = sys/svga/svgalib.o sys/pc/keymap.o sys/linux/joy.o sys/oss/oss.o
SVGA_LIBS = -L/usr/local/lib -lvga

LEMON_OBJS = sys/lemon/lemon.o
LEMON_LIBS = -llemon

SDL_OBJS = sys/sdl/sdl.o sys/sdl/sdl-audio.o sys/sdl/keymap.o
SDL_LIBS = -L/usr/lib -lSDL -lpthread
SDL_CFLAGS = -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT

X11_OBJS = sys/x11/xlib.o sys/x11/keymap.o sys/linux/joy.o sys/oss/oss.o
X11_LIBS =  -lX11 -lXext

all: $(TARGETS)

include Rules

fbgnuboy: $(OBJS) $(SYS_OBJS) $(FB_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(FB_OBJS) -o $@ $(FB_LIBS)

sgnuboy: $(OBJS) $(SYS_OBJS) $(SVGA_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(SVGA_OBJS) -o $@ $(SVGA_LIBS)

sdlgnuboy: $(OBJS) $(SYS_OBJS) $(SDL_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(SDL_OBJS) -o $@ $(SDL_LIBS)

sys/sdl/sdl.o: sys/sdl/sdl.c
	$(MYCC) $(SDL_CFLAGS) -c $< -o $@

sys/sdl/keymap.o: sys/sdl/keymap.c
	$(MYCC) $(SDL_CFLAGS) -c $< -o $@

xgnuboy: $(OBJS) $(SYS_OBJS) $(X11_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(X11_OBJS) -o $@ $(X11_LIBS)

lemongnuboy: $(OBJS) $(SYS_OBJS) $(LEMON_OBJS)
	$(CC) $(CFLAGS) -I. -fno-exceptions -c sys/lemon/lemonvideo.cpp -o sys/lemon/lemonvideo.o $(LEMON_LIBS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(LEMON_OBJS) sys/lemon/lemonvideo.o -o $@ $(LEMON_LIBS)

install: all
	$(INSTALL) -d $(bindir)
	$(INSTALL) -m 755 $(TARGETS) $(bindir)

clean:
	rm -f *gnuboy gmon.out *.o sys/*.o sys/*/*.o asm/*/*.o $(OBJS)

distclean: clean
	rm -f config.* sys/nix/config.h Makefile




