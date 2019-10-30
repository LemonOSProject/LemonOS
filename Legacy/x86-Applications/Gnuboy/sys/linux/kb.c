

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/kd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/vt.h>
#include <termios.h>

#undef K_NUMLOCK

#include "defs.h"
#include "rc.h"
#include "fb.h"
#include "input.h"



#define TTY_DEVICE "/dev/tty"

static int kbfd = -1;

static int initial_kbmode;
static struct termios initial_term, term;

#define SCAN_ALT 56
#define SCAN_FBASE 58
static int alt;

static struct vt_mode vtm, initial_vtm;

extern int keymap[][2];

rcvar_t kb_exports[] =
{
	RCV_END
};


static void vcrelease(int s)
{
	signal(s, vcrelease);
	ioctl(kbfd, VT_RELDISP, VT_ACKACQ);
}

static void vcacquire(int s)
{
	signal(s, vcacquire);
	ioctl(kbfd, VT_RELDISP, VT_ACKACQ);
	fb.enabled = 1;
}

void kb_init()
{
	kbfd = open(TTY_DEVICE, O_RDWR);
	if (!kbfd) die("no controlling terminal\n");
	fcntl(kbfd, F_SETFL, O_NONBLOCK);

	if (ioctl(kbfd, KDSETMODE, KD_GRAPHICS) < 0)
		die("controlling terminal is not the graphics console\n");
	
	ioctl(kbfd, KDGKBMODE, &initial_kbmode);
	tcgetattr(kbfd, &initial_term);
	
	term = initial_term;
	term.c_lflag &= ~(ICANON | ECHO | ISIG);
	term.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 0;
	tcsetattr(kbfd, TCSAFLUSH, &term);
	ioctl(kbfd, KDSKBMODE, K_MEDIUMRAW);

	ioctl(kbfd, VT_GETMODE, &initial_vtm);

	signal(SIGUSR1, vcrelease);
	signal(SIGUSR2, vcacquire);
	vtm = initial_vtm;
	vtm.mode = VT_PROCESS;
	vtm.relsig = SIGUSR1;
	vtm.acqsig = SIGUSR2;
	ioctl(kbfd, VT_SETMODE, &vtm);
}


void kb_close()
{
	ioctl(kbfd, VT_SETMODE, &initial_vtm);
	ioctl(kbfd, KDSKBMODE, initial_kbmode);
	tcsetattr(kbfd, TCSAFLUSH, &initial_term);
	ioctl(kbfd, KDSETMODE, KD_TEXT);
}


static void vcswitch(int c)
{
	struct vt_stat vts;
	ioctl(kbfd, VT_GETSTATE, &vts);
	if (c != vts.v_active)
	{
		ioctl(kbfd, VT_ACTIVATE, c);
		fb.enabled = 0;
		fb.dirty = 1;
	}
}

void kb_poll()
{
	int i;
	event_t ev;
	byte k;
	int st;
	
	while (read(kbfd, &k, 1) > 0)
	{
		st = !(k & 0x80);
		k &= 0x7f;
		
		if (k == SCAN_ALT) alt = st;
		if (alt && k > SCAN_FBASE && k < SCAN_FBASE + 10)
			vcswitch(k - SCAN_FBASE);
		ev.type = st ? EV_PRESS : EV_RELEASE;
		for (i = 0; keymap[i][0]; i++)
			if (keymap[i][0] == k)
				break;
		if (!keymap[i][0]) continue;
		ev.code = keymap[i][1];
		ev_postevent(&ev);
	}
}

