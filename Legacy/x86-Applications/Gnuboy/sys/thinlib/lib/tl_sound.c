/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_sound.c
**
** sound driver
**
** $Id: $
*/

#include "tl_types.h"
#include "tl_sound.h"
#include "tl_sb.h"
#include "tl_log.h"


typedef struct snddriver_s
{
   const char *name;
   int  (*init)(int *sample_rate, int *frag_size, int *format);
   void (*shutdown)(void);
   int  (*start)(audio_callback_t callback, void *user_data);
   void (*stop)(void);
   void (*setrate)(int sample_rate);
   audio_callback_t callback;
   void *user_data;
} snddriver_t;

static snddriver_t sb =
{
   "Sound Blaster",
   thin_sb_init,
   thin_sb_shutdown,
   thin_sb_start,
   thin_sb_stop,
   thin_sb_setrate,
   NULL,
   NULL
};

static snddriver_t *driver_list[] =
{
   &sb,
   NULL
};

static snddriver_t snddriver;

int thin_sound_init(thinsound_t *sound_params)
{
   snddriver_t **iter;
   int sample_rate, frag_size, format;

   THIN_ASSERT(sound_params);
   
   sample_rate = sound_params->sample_rate;
   frag_size = sound_params->frag_size;
   format = sound_params->format;

   for (iter = driver_list; *iter != NULL; iter++)
   {
      if (0 == (*iter)->init(&sample_rate, &frag_size, &format))
      {
         snddriver = **iter;

         /* copy the parameters back */
         sound_params->sample_rate = sample_rate;
         sound_params->frag_size = frag_size;
         sound_params->format = format;

         /* and set the callback */
         snddriver.callback = sound_params->callback;
         snddriver.user_data = sound_params->user_data;

         return 0;
      }
   }

   snddriver.name = NULL;

   thin_printf("thin: could not find any sound drivers.\n");
   return -1;
}

void thin_sound_shutdown(void)
{
   if (NULL == snddriver.name)
      return;

   snddriver.shutdown();
   memset(&snddriver, 0, sizeof(snddriver_t));
}

void thin_sound_start(void)
{
   if (NULL == snddriver.name)
      return;

   THIN_ASSERT(snddriver.callback);
   snddriver.start(snddriver.callback, snddriver.user_data);
}

void thin_sound_stop(void)
{
   if (NULL == snddriver.name)
      return;

   snddriver.stop();
}

void thin_sound_setrate(int sample_rate)
{
   if (snddriver.setrate)
      snddriver.setrate(sample_rate);
}

/*
** $Log: $
*/
