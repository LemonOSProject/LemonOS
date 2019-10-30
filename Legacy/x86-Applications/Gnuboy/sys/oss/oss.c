#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>

#include <unistd.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef IS_FBSD
#include "machine/soundcard.h"
#define DSP_DEVICE "/dev/dsp"
#endif

#ifdef IS_OBSD
#include "soundcard.h"
#define DSP_DEVICE "/dev/sound"
#endif

#ifdef IS_LINUX
#include <sys/soundcard.h>
#define DSP_DEVICE "/dev/dsp"
#endif

#include "defs.h"
#include "pcm.h"
#include "rc.h"

/* FIXME - all this code is VERY basic, improve it! */


struct pcm pcm;

static int dsp;
static char *dsp_device;
static int stereo = 1;
static int samplerate = 44100;
static int sound = 1;

rcvar_t pcm_exports[] =
{
	RCV_BOOL("sound", &sound),
	RCV_INT("stereo", &stereo),
	RCV_INT("samplerate", &samplerate),
	RCV_STRING("oss_device", &dsp_device),
	RCV_END
};


void pcm_init()
{
	int n;

	if (!sound)
	{
		pcm.hz = 11025;
		pcm.len = 4096;
		pcm.buf = malloc(pcm.len);
		pcm.pos = 0;
		dsp = -1;
		return;
	}

	if (!dsp_device) dsp_device = strdup(DSP_DEVICE);
	dsp = open(dsp_device, O_WRONLY);

	n = 0x80009;
	ioctl(dsp, SNDCTL_DSP_SETFRAGMENT, &n);
	n = AFMT_U8;
	ioctl(dsp, SNDCTL_DSP_SETFMT, &n);
	n = stereo;
	ioctl(dsp, SNDCTL_DSP_STEREO, &n);
	pcm.stereo = n;
	n = samplerate;
	ioctl(dsp, SNDCTL_DSP_SPEED, &n);
	pcm.hz = n;
	pcm.len = n / 60;
	pcm.buf = malloc(pcm.len);
}

void pcm_close()
{
	if (pcm.buf) free(pcm.buf);
	memset(&pcm, 0, sizeof pcm);
	close(dsp);
}

int pcm_submit()
{
	if (dsp < 0)
	{
		pcm.pos = 0;
		return 0;
	}
	if (pcm.buf) write(dsp, pcm.buf, pcm.pos);
	pcm.pos = 0;
	return 1;
}






