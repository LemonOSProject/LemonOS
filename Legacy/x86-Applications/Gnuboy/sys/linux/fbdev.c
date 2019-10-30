
/*
 * Support for the Linux framebuffer device
 * Copyright 2001 Laguna
 * MGA BES code derived from fbtv
 * Copyright Gerd Knorr
 * This file may be distributed under the terms of the GNU GPL.
 */

#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "defs.h"
#include "fb.h"
#include "rc.h"
#include "sys.h"
#include "matrox.h"

struct fb fb;



#define FBSET_CMD "fbset"
static char *fb_mode;
static int fb_depth;
static int vmode[3];

#define FB_DEVICE "/dev/fb0"
static char *fb_device;

static int fbfd = -1;
static byte *fbmap;
static int maplen;
static byte *mmio;
static int bes;
static int base;
static int use_yuv = -1;
static int use_interp = 1;

static struct fb_fix_screeninfo fi;
static struct fb_var_screeninfo vi, initial_vi;

rcvar_t vid_exports[] =
{
	RCV_VECTOR("vmode", &vmode, 3),
	RCV_STRING("fb_device", &fb_device),
	RCV_STRING("fb_mode", &fb_mode),
	RCV_INT("fb_depth", &fb_depth),
	RCV_BOOL("yuv", &use_yuv),
	RCV_BOOL("yuvinterp", &use_interp),
	RCV_END
};



static void wrio4(int a, int v)
{
#ifndef IS_LITTLE_ENDIAN
	v = (v<<24) | ((v&0xff00)<<8) | ((v&0xff0000)>>8) | (v>>24);
#endif
	*(int*)(mmio+a) = v;
}

static void overlay_switch()
{
	int a, b;
	
	if (!fb.yuv) return;
	if (!fb.enabled)
	{
		if (bes) wrio4(BESCTL, 0);
		bes = 0;
		return;
	}
	if (bes) return;
	bes = 1;
	memset(fbmap, 0, maplen);
	
	/* color keying (turn it off) */
	mmio[PALWTADD]  = XKEYOPMODE;
	mmio[X_DATAREG] = 0;
	
	/* src */
	wrio4(BESA1ORG, base);
	wrio4(BESA2ORG, base);
	wrio4(BESB1ORG, base);
	wrio4(BESB2ORG, base);
	wrio4(BESPITCH, 320);
	
	/* dest */
	a = (vi.xres - vmode[0])>>1;
	b = vi.xres - a - 1;
	wrio4(BESHCOORD,  (a << 16) | (b - 1));
	
	/* scale horiz */
	wrio4(BESHISCAL,   320*131072/(b-a) & 0x001ffffc);
	wrio4(BESHSRCST,   0 << 16);
	wrio4(BESHSRCEND,  320 << 16);
	wrio4(BESHSRCLST,  319 << 16);

	/* dest */
	a = (vi.yres - vmode[1])>>1;
	b = vi.yres - a - 1;
	wrio4(BESVCOORD,  (a << 16) | (b - 1));
	
	/* scale vert */
	wrio4(BESVISCAL,   144*65536/(b-a) & 0x001ffffc);
	wrio4(BESV1WGHT,   0);
	wrio4(BESV2WGHT,   0);
	wrio4(BESV1SRCLST, 143);
	wrio4(BESV2SRCLST, 143);
	
	/* turn on (enable, horizontal+vertical interpolation filters */
	if (use_interp)
		wrio4(BESCTL, 0x50c01);
	else
		wrio4(BESCTL, 1);
	wrio4(BESGLOBCTL, 0x83);
}

static void overlay_init()
{
	if (!mmio | !use_yuv) return;
	if (use_yuv < 0) if ((vmode[0] < 320) || (vmode[1] < 288)) return;
	switch (fi.accel)
	{
#ifdef FB_ACCEL_MATROX_MGAG200
	case FB_ACCEL_MATROX_MGAG200:
#endif
#ifdef FB_ACCEL_MATROX_MGAG400
	case FB_ACCEL_MATROX_MGAG400:
#endif
		break;
	default:
		return;
	}
	fb.w = 160;
	fb.h = 144;
	fb.pitch = 640;
	fb.pelsize = 4;
	fb.yuv = 1;
	fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = fb.cc[3].r = 0;
	fb.cc[0].l = 0;
	fb.cc[1].l = 24;
	fb.cc[2].l = 8;
	fb.cc[3].l = 16;
	base = vi.yres * vi.xres_virtual * ((vi.bits_per_pixel+7)>>3);
	
	maplen = base + fb.pitch * fb.h;
}

static void plain_init()
{
	fb.w = vi.xres;
	fb.h = vi.yres;
	fb.pelsize = (vi.bits_per_pixel+7)>>3;
	fb.pitch = vi.xres_virtual * fb.pelsize;
	fb.indexed = fi.visual == FB_VISUAL_PSEUDOCOLOR;

	fb.cc[0].r = 8 - vi.red.length;
	fb.cc[1].r = 8 - vi.green.length;
	fb.cc[2].r = 8 - vi.blue.length;
	fb.cc[0].l = vi.red.offset;
	fb.cc[1].l = vi.green.offset;
	fb.cc[2].l = vi.blue.offset;
	
	maplen = fb.pitch * fb.h;
}

void vid_init()
{
	char cmd[256];

	kb_init();
	joy_init();

	if (!fb_device)
		if (!(fb_device = getenv("FRAMEBUFFER")))
			fb_device = strdup(FB_DEVICE);
	fbfd = open(fb_device, O_RDWR);
	if (fbfd < 0) die("cannot open %s\n", fb_device);
	
	ioctl(fbfd, FBIOGET_VSCREENINFO, &initial_vi);
	initial_vi.xoffset = initial_vi.yoffset = 0;

	if (fb_mode)
	{
		sprintf(cmd, FBSET_CMD " %.80s", fb_mode);
		system(cmd);
	}
	
	ioctl(fbfd, FBIOGET_VSCREENINFO, &vi);
	if (fb_depth) vi.bits_per_pixel = fb_depth;
	vi.xoffset = vi.yoffset = 0;
	vi.accel_flags = 0;
	vi.activate = FB_ACTIVATE_NOW;
	ioctl(fbfd, FBIOPUT_VSCREENINFO, &vi);
	ioctl(fbfd, FBIOGET_VSCREENINFO, &vi);
	ioctl(fbfd, FBIOGET_FSCREENINFO, &fi);

	if (!vmode[0] || !vmode[1])
	{
		int scale = rc_getint("scale");
		if (scale < 1) scale = 1;
		vmode[0] = 160 * scale;
		vmode[1] = 144 * scale;
	}
	if (vmode[0] > vi.xres) vmode[0] = vi.xres;
	if (vmode[1] > vi.yres) vmode[1] = vi.yres;
	
	mmio = mmap(0, fi.mmio_len, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, fi.smem_len);
	if ((long)mmio == -1) mmio = 0;

	overlay_init();

	if (!fb.yuv) plain_init();

	fbmap = mmap(0, maplen, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (!fbmap) die("cannot mmap %s (%d bytes)\n", fb_device, maplen);
	
	fb.ptr = fbmap + base;
	memset(fbmap, 0, maplen);
	fb.dirty = 0;
	fb.enabled = 1;

	overlay_switch();
}

void vid_close()
{
	fb.enabled = 0;
	overlay_switch();
	joy_close();
	kb_close();
	ioctl(fbfd, FBIOPUT_VSCREENINFO, &initial_vi);
	memset(fbmap, 0, maplen);
}

void vid_preinit()
{
}

void vid_settitle(char *title)
{
}

void vid_setpal(int i, int r, int g, int b)
{
	unsigned short rr = r<<8, gg = g<<8, bb = b<<8;
	struct fb_cmap cmap;
	memset(&cmap, 0, sizeof cmap);
	cmap.start = i;
	cmap.len = 1;
	cmap.red = &rr;
	cmap.green = &gg;
	cmap.blue = &bb;
	ioctl(fbfd, FBIOPUTCMAP, &cmap);
}

void vid_begin()
{
	overlay_switch();
}

void vid_end()
{
	overlay_switch();
}

void ev_poll()
{
	kb_poll();
	joy_poll();
}



