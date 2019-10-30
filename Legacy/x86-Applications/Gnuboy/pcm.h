
#ifndef __PCM_H__
#define __PCM_H__


#include "defs.h"

struct pcm
{
	int hz, len;
	int stereo;
	byte *buf;
	int pos;
};

extern struct pcm pcm;


#endif


