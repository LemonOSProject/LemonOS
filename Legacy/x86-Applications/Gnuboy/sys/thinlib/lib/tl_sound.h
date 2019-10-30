/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_sound.h
**
** thinlib sound routines
**
** $Id: $
*/

#ifndef _TL_SOUND_H_
#define _TL_SOUND_H_

#include "tl_types.h"

#define  THIN_SOUND_8BIT      0x00
#define  THIN_SOUND_16BIT     0x01
#define  THIN_SOUND_MONO      0x00
#define  THIN_SOUND_STEREO    0x02
#define  THIN_SOUND_UNSIGNED  0x00
#define  THIN_SOUND_SIGNED    0x04

typedef void (*audio_callback_t)(void *user_data, void *buffer, int samples);

typedef struct thinsound_s
{
   int sample_rate;
   int frag_size;
   int format;
   audio_callback_t callback;
   void *user_data;
} thinsound_t;

extern int thin_sound_init(thinsound_t *sound_params);
extern void thin_sound_shutdown(void);

// TODO: roll into one pause() function
extern void thin_sound_start(void);
extern void thin_sound_stop(void);

extern void thin_sound_setrate(int sample_rate);

#endif /* !_TL_SOUND_H_ */

/*
** $Log: $
*/
