/*
 * xlib.c
 *
 * Xlib interface.
 * dist under gnu gpl
 */
#include <ctype.h>
#include "../../sys.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#if defined(HAVE_LIBXEXT) && defined(HAVE_X11_EXTENSIONS_XSHM_H) \
 && defined(HAVE_SYS_IPC_H) && defined(HAVE_SYS_SHM_H)
#define USE_XSHM
#endif
#else
#define USE_XSHM  /* assume we have shm if no config.h - is this ok? */
#endif

#ifdef USE_XSHM
/* make sure ipc.h and shm.h will work! */
#define _SVID_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef USE_XSHM
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "fb.h"
#include "input.h"
#include "rc.h"


struct fb fb;

static int vmode[3] = { 0, 0, 0 };
static int x_shmsync = 1;

rcvar_t vid_exports[] =
{
	RCV_VECTOR("vmode", &vmode, 3),
	RCV_BOOL("x_shmsync", &x_shmsync),
	RCV_END
};


static int initok;

/* Loads of bogus Xlib crap...bleh */

static char *x_displayname;

static Display *x_display;
static int x_screen;

static struct
{
	int bits;
	int vc;
	int bytes;
} x_vissup[] =
{
	{ 8, PseudoColor, 1 },
	{ 15, TrueColor, 2 },
	{ 16, TrueColor, 2 },
	{ 32, TrueColor, 4 },
	{ 24, TrueColor, 3 },
	{ 0, 0, 0 }
};

static int x_bits, x_bytes;
static Visual *x_vis;
static XVisualInfo x_visinfo;
static int x_pseudo;
static Colormap x_cmap;
static XColor x_ctable[256];

static int x_wattrmask;
static XSetWindowAttributes x_wattr;
static int x_gcvalmask;
static XGCValues x_gcval;

static Window x_win;
static int x_win_x, x_win_y;
static int x_width, x_height;
static GC x_gc;

static XSizeHints x_size;
static XWMHints x_wmhints;
/*static XClassHint x_class;*/

#ifdef USE_XSHM
static XShmSegmentInfo x_shm;
#endif

static int x_useshm;
static int x_shmevent;
static int x_shmdone;
static XImage *x_image;
static int x_byteswap;

static XEvent x_ev;




static void freescreen()
{
	if (!initok || !x_image) return;
	if ((char *)fb.ptr != (char *)x_image->data)
		free(fb.ptr);
#ifdef USE_XSHM
	if (x_useshm)
	{
		/* FIXME - is this the right way to free shared mem? */
		XSync(x_display, False);
		if (!XShmDetach(x_display, &x_shm))
			die ("XShmDetach failed\n");
		XSync(x_display, False);
		shmdt(x_shm.shmaddr);
		shmctl(x_shm.shmid, IPC_RMID, 0);
		x_image->data = NULL;
	}
#endif
	free(x_image);
	x_image = NULL;
	fb.ptr = NULL;
}

static void allocscreen()
{
	if (initok) freescreen();
#ifdef USE_XSHM
	if (x_useshm)
	{
		x_image = XShmCreateImage(
			x_display, x_vis, x_bits, ZPixmap, 0,
			&x_shm, x_width, x_height);
		if (x_image)
		{
			x_shm.shmid = shmget(
				IPC_PRIVATE,
				x_image->bytes_per_line * x_image->height,
				IPC_CREAT | 0777);
			if (x_shm.shmid < 0)
				die("shmget failed\n");
			x_image->data = x_shm.shmaddr =
				shmat(x_shm.shmid, 0, 0);
			if (!x_image->data)
				die("shmat failed\n");
			if (!XShmAttach(x_display, &x_shm))
				die("XShmAttach failed\n");
			XSync(x_display, False);
			x_shmdone = 1;
			fb.pitch = x_image->bytes_per_line;
		}
		else
		{
			x_useshm = 0;
		}
	}
#endif
	if (!x_useshm)
	{
		x_image = XCreateImage(
			x_display, x_vis, x_bits, ZPixmap, 0,
			malloc(x_width*x_height*x_bytes),
			x_width, x_height, x_bits, x_width*x_bytes);
		if (!x_image)
			die("XCreateImage failed\n");
	}
	x_byteswap = x_image->byte_order ==
#ifdef IS_LITTLE_ENDIAN
		MSBFirst
#else
		LSBFirst
#endif
		;
	if (x_byteswap && x_bytes > 1)
		fb.ptr = malloc(x_image->bytes_per_line * x_image->height);
	else
		fb.ptr = (byte *)x_image->data;
}



void vid_resize()
{
	freescreen();
	x_width = fb.w;
	x_height = fb.h;
	XResizeWindow(x_display, x_win, x_width, x_height);
	x_size.flags = PSize | PMinSize | PMaxSize;
	x_size.min_width = x_size.max_width = x_size.base_width = x_width;
	x_size.min_height = x_size.max_height = x_size.base_height = x_height;
	XSetWMNormalHints(x_display, x_win, &x_size);
	XSync(x_display, False);
	allocscreen();
}


static void colorshifts()
{
	int i;
	int mask[3];
	int l, c;

	mask[0] = x_vis->red_mask;
	mask[1] = x_vis->green_mask;
	mask[2] = x_vis->blue_mask;

	for (i = 0; i < 3; i++)
	{
		for (l = 0; l < 32 && !((1<<l) & mask[i]); l++);
		for (c = 0; l+c < 32 && ((1<<(l+c)) & mask[i]); c++);
		fb.cc[i].l = l;
		fb.cc[i].r = 8-c;
	}
}




void vid_preinit()
{
	/* do nothing; only needed on systems where we must drop perms */
}

void vid_init()
{
	int i;
	
	if (initok) return;

	x_displayname = getenv("DISPLAY");
	if (!x_displayname)
		die("DISPLAY environment variable not set\n");
	x_display = XOpenDisplay(NULL);
	if (!x_display)
		die("failed to connect to X display\n");
	x_screen = DefaultScreen(x_display);

	for (i = 0; x_vissup[i].bits; i++)
	{
		if (XMatchVisualInfo(
			x_display, x_screen,
			x_vissup[i].bits, x_vissup[i].vc, &x_visinfo))
		{
			if (x_vissup[i].vc == PseudoColor)
				x_pseudo = 1;
			else
				x_pseudo = 0;
			x_bits = x_vissup[i].bits;
			x_bytes = x_vissup[i].bytes;
			break;
		}
	}
	if (!x_bits) die("no suitable X visuals\n");
	x_vis = x_visinfo.visual;
	if (!x_vis) die("X visual is NULL");

	if (x_pseudo)
	{
		x_cmap = XCreateColormap(
			x_display, RootWindow(x_display, x_screen),
			x_vis, AllocAll);
		for (i = 0; i < 256; i++)
		{
			x_ctable[i].pixel = i;
			x_ctable[i].flags = DoRed|DoGreen|DoBlue;
		}
	}

	x_wattrmask = CWEventMask | CWBorderPixel | CWColormap;
	x_wattr.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask
		| FocusChangeMask;
	x_wattr.border_pixel = 0;
	x_wattr.colormap = x_cmap;

	x_gcvalmask = GCGraphicsExposures;
	x_gcval.graphics_exposures = False;

	if (!vmode[0] || !vmode[1])
	{
		int scale = rc_getint("scale");
		if (scale < 1) scale = 1;
		vmode[0] = 160 * scale;
		vmode[1] = 144 * scale;
	}
	
	fb.w = vmode[0];
	fb.h = vmode[1];
	fb.pelsize = x_bytes == 3 ? 4 : x_bytes;
	fb.pitch = fb.w * fb.pelsize;
	fb.indexed = x_pseudo;
	fb.enabled = 1;
	fb.dirty = 0;
	
	x_win_x = x_win_y = 0;
	x_width = fb.w;
	x_height = fb.h;
	x_win = XCreateWindow(
		x_display, RootWindow(x_display, x_screen),
		x_win_x, x_win_y, x_width, x_height, 0, x_bits,
		InputOutput, x_vis, x_wattrmask, &x_wattr);
	if (!x_win) die("could not create X window\n");

	x_gc = XCreateGC(x_display, x_win, x_gcvalmask, &x_gcval);

	x_size.flags = PSize | PMinSize | PMaxSize;
	x_size.min_width = x_size.max_width = x_size.base_width = x_width;
	x_size.min_height = x_size.max_height = x_size.base_height = x_height;
	XSetWMNormalHints(x_display, x_win, &x_size);

	x_wmhints.initial_state = NormalState;
	x_wmhints.input = True;
	x_wmhints.flags = StateHint | InputHint;
	XSetWMHints(x_display, x_win, &x_wmhints);

	/* FIXME - set X class info stuff (with XSetClassHint)... */
	
	XMapWindow(x_display, x_win);

	for(;;)
	{
		XNextEvent(x_display, &x_ev);
		if (x_ev.type == Expose && !x_ev.xexpose.count)
			break;
	}

	XSetInputFocus(x_display, x_win, RevertToPointerRoot, CurrentTime);

#ifdef USE_XSHM
	if (XShmQueryExtension(x_display) && x_displayname[0] == ':')
	{
		x_useshm = 1;
		x_shmevent = XShmGetEventBase(x_display) + ShmCompletion;
	}
#endif

	colorshifts();
	allocscreen();

	joy_init();

	initok = 1;
}

void vid_close()
{
	joy_close();
	if (!initok) return;
	freescreen();
	XFreeGC(x_display, x_gc);
	XDestroyWindow(x_display, x_win);
	XCloseDisplay(x_display);
	initok = 0;
}







/* keymap - mappings of the form { keysym, localcode } - from x11/keymap.c */
extern int keymap[][2];

static int mapxkeycode(int xkeycode)
{
	int i;
	int sym;
	int code;

	sym = XKeycodeToKeysym(x_display, xkeycode, 0);
	for (i = 0; keymap[i][0]; i++)
		if (keymap[i][0] == sym)
			return keymap[i][1];
	if (sym < XK_space || sym > XK_asciitilde)
		return 0;
	code = sym - XK_space + ' ';
	if (code >= 'A' && code <= 'Z')
		code = tolower(code);;
	return code;
}


void vid_end();

static int nextevent(int sync)
{
	event_t ev;
	
	if (!sync && !XPending(x_display))
		return 0;

	XNextEvent(x_display, &x_ev);
	switch(x_ev.type)
	{
	case KeyPress:
		ev.type = EV_PRESS;
		ev.code = mapxkeycode(x_ev.xkey.keycode);
		break;
	case KeyRelease:
		ev.type = EV_RELEASE;
		ev.code = mapxkeycode(x_ev.xkey.keycode);
		break;
	case Expose:
		vid_end();
		return 1;
		break;
	default:
		if (x_ev.type == x_shmevent) x_shmdone = 1;
		return 1;
		break;
	}
	return ev_postevent(&ev);  /* returns 0 if queue is full */
}



void ev_poll()
{
	while (nextevent(0));
	joy_poll();
}



void vid_settitle(char *title)
{
	XStoreName(x_display, x_win, title);
	XSetIconName(x_display, x_win, title);
}

void vid_setpal(int i, int r, int g, int b)
{
	if (!initok) return;
	
	if (x_pseudo == 1)
	{
		x_ctable[i].red = r << 8;
		x_ctable[i].green = g << 8;
		x_ctable[i].blue = b << 8;
		XStoreColors(x_display, x_cmap, x_ctable, 256);
	}
}

void vid_begin()
{
	if (!x_useshm) return;

	/* XSync(x_display, False); */
	while (!x_shmdone && x_shmsync)
		nextevent(1);
}

static void endianswap()
{
	int cnt;
	un16 t16;
	un32 t32;
	un16 *src16 = (void *)fb.ptr;
	un16 *dst16 = (void *)x_image->data;
	un32 *src32 = (void *)fb.ptr;
	un32 *dst32 = (void *)x_image->data;
	
	switch (x_bytes)
	{
	case 2:
		cnt = (x_image->bytes_per_line * x_image->height)>>1;
		while (cnt--)
		{
			t16 = *(src16++);
			*(dst16++) = (t16 << 8) | (t16 >> 8);
		}
		break;
	case 4:
		cnt = (x_image->bytes_per_line * x_image->height)>>2;
		while (cnt--)
		{
			t32 = *(src32++);
			*(dst32++) = (t32 << 24) | ((t32 << 8) & 0x00FF0000) |
				((t32 >> 8) & 0x0000FF00) | (t32 >> 24);
		}
		break;
	}
}


void vid_end()
{
	if (!initok) return;

	if (x_byteswap) endianswap();
	if (x_useshm)
	{
		if (!x_shmdone) return;
#ifdef USE_XSHM
		if (!XShmPutImage(
			x_display, x_win, x_gc, x_image,
			0, 0, 0, 0, x_width, x_height, True))
			die("XShmPutImage failed\n");
#endif
		x_shmdone = 0;
	}
	else
	{
		XPutImage(x_display, x_win, x_gc, x_image,
			  0, 0, 0, 0, x_width, x_height);
	}
}





