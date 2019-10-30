/*
 * svgalib.c
 *
 * svgalib interface.
 */


#include <stdlib.h>
#include <stdio.h>

#include <vga.h>
#include <vgakeyboard.h>

#include "fb.h"
#include "input.h"
#include "rc.h"



struct fb fb;




static int vmode[3] = { 0, 0, 8 };
static int svga_mode;
static int svga_vsync = 1;

rcvar_t vid_exports[] =
{
	RCV_VECTOR("vmode", vmode, 3),
	RCV_INT("vsync", &svga_vsync),
	RCV_INT("svga_mode", &svga_mode),
	RCV_END
};



/* keymap - mappings of the form { scancode, localcode } - from pc/keymap.c */
extern int keymap[][2];

static int mapscancode(int scan)
{
	int i;
	for (i = 0; keymap[i][0]; i++)
		if (keymap[i][0] == scan)
			return keymap[i][1];
	return 0;
}

static void kbhandler(int scan, int state)
{
	event_t ev;
	ev.type = state ? EV_PRESS : EV_RELEASE;
	ev.code = mapscancode(scan);
	ev_postevent(&ev);
}

int *rc_getvec();

static int selectmode()
{
	int i;
	int stop;
	vga_modeinfo *mi;
	int best = -1;
	int besterr = 1<<24;
	int err;
	int *vd;

	vd = vmode;
	
	stop = vga_lastmodenumber();
	for (i = 0; i <= stop; i++)
	{
		if (!vga_hasmode(i)) continue;
		mi = vga_getmodeinfo(i);
		
		/* modex is too crappy to deal with */
		if (!mi->bytesperpixel) continue;

		/* so are banked modes */
		if (mi->width * mi->height * mi->bytesperpixel > 65536)
			if (!(mi->flags & (IS_LINEAR))) continue;

		/* we can't use modes that are too small */
		if (mi->colors < 256) continue;
		if (mi->width < vd[0]) continue;
		if (mi->height < vd[1]) continue;

		/* perfect matches always win */
		if (mi->width == vd[0] && mi->height == vd[1]
			&& (mi->bytesperpixel<<3) == vd[2])
		{
			best = i;
			break;
		}

		/* compare error */
		err = mi->width * mi->height - vd[0] * vd[1]
			+ abs((mi->bytesperpixel<<3)-vd[2]);
		if (err < besterr)
		{
			best = i;
			besterr = err;
		}
	}
	if (best < 0)
		die("no suitable modes available\n");

	return best;
}



void vid_preinit()
{
	vga_init();
}

void vid_init()
{
	int m;
	vga_modeinfo *mi;

	if (!vmode[0] || !vmode[1])
	{
		int scale = rc_getint("scale");
		if (scale < 1) scale = 1;
		vmode[0] = 160 * scale;
		vmode[1] = 144 * scale;
	}
	
	m = svga_mode;
	if (!m) m = selectmode();
	
	if (!vga_hasmode(m))
		die("no such video mode: %d\n", m);

	vga_setmode(m);
	mi = vga_getmodeinfo(m);
	fb.w = mi->width;
	fb.h = mi->height;
	fb.pelsize = mi->bytesperpixel;
	fb.pitch = mi->linewidth;
	fb.ptr = vga_getgraphmem();
	fb.enabled = 1;
	fb.dirty = 0;

	switch (mi->colors)
	{
	case 256:
		fb.indexed = 1;
		fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = 8;
		fb.cc[0].l = fb.cc[1].l = fb.cc[2].l = 0;
		break;
	case 32768:
		fb.indexed = 0;
		fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = 3;
		fb.cc[0].l = 10;
		fb.cc[1].l = 5;
		fb.cc[2].l = 0;
		break;
	case 65536:
		fb.indexed = 0;
		fb.cc[0].r = fb.cc[2].r = 3;
		fb.cc[1].r = 2;
		fb.cc[0].l = 11;
		fb.cc[1].l = 5;
		fb.cc[2].l = 0;
		break;
	case 16384*1024:
		fb.indexed = 0;
		fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = 0;
		fb.cc[0].l = 16;
		fb.cc[1].l = 8;
		fb.cc[2].l = 0;
		break;
	}
	
	keyboard_init();
	keyboard_seteventhandler(kbhandler);

	joy_init();
}


void vid_close()
{
	if (!fb.ptr) return;
	memset(&fb, 0, sizeof fb);
	joy_close();
	keyboard_close();
	vga_setmode(TEXT);
}

void vid_settitle(char *title)
{
}

void vid_setpal(int i, int r, int g, int b)
{
	vga_setpalette(i, r>>2, g>>2, b>>2);
}

void vid_begin()
{
	if (svga_vsync) vga_waitretrace();
}

void vid_end()
{
}

void kb_init()
{
}

void kb_close()
{
}

void kb_poll()
{
	keyboard_update();
}

void ev_poll()
{
	kb_poll();
	joy_poll();
}














