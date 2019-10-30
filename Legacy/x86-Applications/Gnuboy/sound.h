

#ifndef __SOUND_H__
#define __SOUND_H__


struct sndchan
{
	int on;
	unsigned pos;
	int cnt, encnt, swcnt;
	int len, enlen, swlen;
	int swfreq;
	int freq;
	int envol, endir;
};


struct snd
{
	int rate;
	struct sndchan ch[4];
	byte wave[16];
};


extern struct snd snd;

#include "defs.h"

void sound_write(byte r, byte b);
byte sound_read(byte r);
void sound_dirty();
void sound_off();
void sound_reset();
void sound_mix();
void s1_init();
void s2_init();
void s3_init();
void s4_init();



#endif


