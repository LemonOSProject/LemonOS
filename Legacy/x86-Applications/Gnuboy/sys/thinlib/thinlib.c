#include <stdlib.h>
#include <string.h>
#include "fb.h"
#include "input.h"
#include "rc.h"
#include "pcm.h"
#include "thinlib.h"

struct pcm pcm;

static volatile int audio_int;

static int samplerate = 44100;
static int sound = 1;
static int stereo = 0;

static int joystick = 1;

static int dpp = 0;
static int dpp_pad = 0;
static int dpp_port = 0x378;

struct fb fb;

static bitmap_t *screen = NULL;
static int vmode[3] = { 320, 200, 8 };

rcvar_t vid_exports[] = 
{
	RCV_VECTOR("vmode", vmode, 3),
	RCV_END
};

rcvar_t pcm_exports[] =
{
	RCV_BOOL("sound", &sound),
	RCV_INT("samplerate", &samplerate),
	RCV_INT("stereo", &stereo),
	RCV_END
};

rcvar_t joy_exports[] =
{
	RCV_BOOL("joystick", &joystick),
	RCV_BOOL("dpp", &dpp),
	RCV_INT("dpp_pad", &dpp_pad),
	RCV_INT("dpp_port", &dpp_port),
	RCV_END
};

void joy_init()
{
	if (joystick)
	{
		if (thin_joy_init())
		joystick = 0;
	}

	if (dpp)
	{
		if (thin_dpp_init())
			dpp = 0;
		else
			thin_dpp_add(dpp_port, dpp_pad);
	}
}

void joy_close()
{
	if (joystick)
		thin_joy_shutdown();
	if (dpp)
		thin_dpp_shutdown();
}

void joy_poll()
{
	/* handled by event polling */
}


/* hardware audio buffer fill */
static void _audio_callback(void *user_data, void *buf, int len)
{
	/* user_data unused */
	memcpy(buf, pcm.buf, len);
	audio_int = 1;
}

void pcm_init()
{
	thinsound_t params;
	int i;

	if (!sound)
	{
		pcm.hz = 11025;
		pcm.len = 4096;
		pcm.buf = (byte *) malloc(pcm.len);
		pcm.pos = 0;
		pcm.stereo = stereo;
		return;
	}

	params.sample_rate = samplerate;
	params.frag_size = samplerate / 60;
	for (i = 1; i < params.frag_size; i <<= 1);
	params.frag_size = i;
	params.format = THIN_SOUND_8BIT | THIN_SOUND_UNSIGNED;
	if (stereo)
		params.format |= THIN_SOUND_STEREO;
	else
		params.format |= THIN_SOUND_MONO;
	params.callback = _audio_callback;

	if (thin_sound_init(&params))
	{
		sound = 0;
		return;
	}

	pcm.hz = params.sample_rate;
	pcm.len = params.frag_size;
	pcm.stereo = (params.format & THIN_SOUND_STEREO) ? 1 : 0;
	pcm.buf = (byte *) malloc(pcm.len);
	if (!pcm.buf)
		die("failed to allocate sound buffer\n");

	memset(pcm.buf, 0, pcm.len);
	pcm.pos = 0;

	thin_sound_start();
}

void pcm_close()
{
	if (sound)
	{
		thin_sound_stop();
		thin_shutdown();
	}

	if (pcm.buf)
		free(pcm.buf);

	memset(&pcm, 0, sizeof pcm);
}

int pcm_submit()
{
	if (!sound)
	{
		pcm.pos = 0;
		return 0;
	}

	if (pcm.pos < pcm.len)
		return 1;

	while (!audio_int)
		; /* spin */

	audio_int = 0;
	pcm.pos = 0;

	return 1;
}


/* keyboard stuff... */

/* keymap - mappings of the form { scancode, localcode } - from keymap.c */
extern int keymap[][2];
static int scanmap[256];

static int mapscancode(int scan)
{
	int i;
	for (i = 0; keymap[i][0]; i++)
		if (keymap[i][0] == scan)
			return keymap[i][1];
	return 0;
}

static void buildscanmap()
{
	int key, i;

	memset(scanmap, 0, sizeof(scanmap));

	for (key = 0; key < 256; key++)
		scanmap[key] = mapscancode(key);
}

void ev_poll()
{
	thin_event_t event;
	event_t ev;

	thin_event_gather();
	
	while (thin_event_get(&event))
	{
		switch (event.type)
		{
		case THIN_KEY_PRESS:
			ev.type = EV_PRESS;
			ev.code = scanmap[event.data.keysym];
			ev_postevent(&ev);
			break;

		case THIN_KEY_RELEASE:
			ev.type = EV_RELEASE;
			ev.code = scanmap[event.data.keysym];
			ev_postevent(&ev);
			break;

		case THIN_JOY_MOTION:
			ev.type = event.data.joy_motion.state ? EV_PRESS : EV_RELEASE;
			switch (event.data.joy_motion.dir)
			{
			case THIN_JOY_LEFT:
				ev.code = K_JOYLEFT;
				break;

			case THIN_JOY_RIGHT:
				ev.code = K_JOYRIGHT;
				break;

			case THIN_JOY_UP:
				ev.code = K_JOYUP;
				break;

			case THIN_JOY_DOWN:
				ev.code = K_JOYDOWN;
				break;
			}
         
			ev_postevent(&ev);
			break;

		case THIN_JOY_BUTTON_PRESS:
			ev.type = EV_PRESS;
			ev.code = K_JOY0 + event.data.joy_button;
			ev_postevent(&ev);
			break;

		case THIN_JOY_BUTTON_RELEASE:
			ev.type = EV_RELEASE;
			ev.code = K_JOY0 + event.data.joy_button;
			ev_postevent(&ev);
			break;

		default:
			break;
		}
	}
}

void vid_preinit()
{
	int gotmask = thin_init(THIN_VIDEO | THIN_SOUND | THIN_KEY);
	if ((THIN_VIDEO | THIN_KEY) != (gotmask & (THIN_VIDEO | THIN_KEY)))
		die("thinlib initialization failed.");
	thin_key_set_repeat(false);
	buildscanmap();

	/* don't spam the graphics screen if we don't have soundcard */
	thin_setlogfunc(NULL);

	joy_init();
}

void vid_init()
{
	int red_length, green_length, blue_length;
	int red_offset, green_offset, blue_offset;

	if (thin_vid_init(vmode[0], vmode[1], vmode[2], THIN_VIDEO_HWSURFACE))
		die("could not set video mode");

	screen = thin_vid_lockwrite();
	if (NULL == screen)
		die("could not get ahold of video surface");

	fb.w = screen->width;
	fb.h = screen->height;
	fb.pitch = screen->pitch;
	fb.ptr = screen->data;

	fb.pelsize = (screen->bpp + 7) / 8;
	fb.indexed = (screen->bpp == 8) ? 1 : 0;

	switch (screen->bpp)
	{
	case 8:
		red_length = 0;
		green_length = 0;
		blue_length = 0;
		red_offset = 0;
		green_offset = 0;
		blue_offset = 0;
		break;

	case 16:
		red_length = 5;
		green_length = 6;
		blue_length = 5;
		red_offset = 11;
		green_offset = 5;
		blue_offset = 0;
		break;

	case 32:
		red_length = 8;
		green_length = 8;
		blue_length = 8;
		red_offset = 16;
		green_offset = 8;
		blue_offset = 0;
		break;

	case 15:
	case 24:
	default:
		die("i don't know what to do with %dbpp mode", screen->bpp);
		break;
	}

	fb.cc[0].r = 8 - red_length;
	fb.cc[1].r = 8 - green_length;
	fb.cc[2].r = 8 - blue_length;
	fb.cc[0].l = red_offset;
	fb.cc[1].l = green_offset;
	fb.cc[2].l = blue_offset;

	fb.enabled = 1;
	fb.dirty = 0;
}

void vid_close()
{
	fb.enabled = 0;
	joy_close();
	thin_shutdown();
}

void vid_settitle(char *title)
{
}

void vid_setpal(int i, int r, int g, int b)
{
	rgb_t color;

	color.r = r;
	color.g = g;
	color.b = b;
	thin_vid_setpalette(&color, i, 1);
}

void vid_begin()
{
	screen = thin_vid_lockwrite();

	fb.ptr = screen->data;
	fb.pitch = screen->pitch;
	fb.w = screen->width;
	fb.h = screen->height;
}

void vid_end()
{
	thin_vid_freewrite(-1, NULL);
}
