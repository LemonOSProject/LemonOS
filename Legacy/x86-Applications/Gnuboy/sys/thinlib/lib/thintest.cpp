/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** thintest.cpp
**
** thinlib test
** $Id: thintest.cpp,v 1.3 2001/03/12 06:06:55 matt Exp $
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "thinlib.h"

//#define  TEST_STEREO
#define  TEST_DPP

class test
{
public:
   test();
   ~test();
   void  run();

private:
   enum
   {
      SAMPLE_RATE = 44100,
      FRAGSIZE    = 256,
      VID_WIDTH   = 320,
      VID_HEIGHT  = 240,
      VID_BPP     = 8
   };

   int testSound();
   int testVideo();
   int testTimer();
   int testEvents();
};

test::test()
{
   int ret = thin_init(THIN_KEY | THIN_MOUSE | THIN_TIMER
                       | THIN_VIDEO | THIN_SOUND);
   THIN_ASSERT(-1 != ret);
}

test::~test()
{
   thin_shutdown();
}

static void fillbuf(void *user_data, void *buf, int size)
{
   static int pos = 0;
   UNUSED(user_data);

   while (size--)
   {
      *((uint8 *) buf)++ = 127 + (int8)(127.0 * sin(2 * PI * pos / 128));
#ifdef TEST_STEREO
      *((uint8 *) buf)++ = 127 + (int8)(127.0 * cos(2 * PI * pos / 128));
#endif
      pos = (pos + 1) & 1023;
   }
}

int test::testSound()
{
   thinsound_t params;

   params.sample_rate = SAMPLE_RATE;
   params.frag_size = FRAGSIZE;
#ifdef TEST_STEREO
   params.format = THIN_SOUND_STEREO | THIN_SOUND_8BIT;
#else
   params.format = THIN_SOUND_MONO | THIN_SOUND_8BIT;
#endif
   params.callback = fillbuf;
   params.user_data = NULL;

   if (thin_sound_init(&params))
      return -1;

   thin_sound_start();
   thin_sound_stop();

   return 0;
}

int test::testVideo()
{
   int i, x, y;
   bitmap_t *screen;
   bitmap_t *buffer;

   /* set up video */
   if (thin_vid_init(VID_WIDTH, VID_HEIGHT, VID_BPP, 0/*THIN_VIDEO_HWSURFACE*/))
      return -1;

   buffer = thin_bmp_create(VID_WIDTH, VID_HEIGHT, VID_BPP, 0);
   if (NULL == buffer)
      return -1;

   /* fill it up with something interesting */
   for (y = 0; y < buffer->height; y++)
      for (x = 0; x < buffer->width; x++)
         buffer->line[y][x] = x ^ y;

   /* blit it out 1000 times */
   for (i = 0; i < 1000; i++)
   {
      screen = thin_vid_lockwrite();
      if (NULL == screen)
         return -1;

      for (y = 0; y < screen->height; y++)
         memcpy(screen->line[y], buffer->line[y], screen->width);
         
      thin_vid_freewrite(-1, NULL);
   }

   thin_vid_shutdown();

   if (1000 != i)
      return -1;

   return 0;
}

static volatile int timer_ticks = 0;
static void timer_handler(void *param)
{
   (void) param;
   timer_ticks++;
}
THIN_LOCKED_STATIC_FUNC(timer_handler)

int test::testTimer()
{
   int last_ticks;

   THIN_LOCK_FUNC(timer_handler);
   THIN_LOCK_VAR(timer_ticks);

   /* one second intervals... */
   if (thin_timer_init(60, timer_handler, NULL))
      return -1;

   timer_ticks = last_ticks = 0;
   while (timer_ticks <= 60)
   {
      if (last_ticks != timer_ticks)
      {
         last_ticks = timer_ticks;
         thin_printf("%d 60 hz tick\n", last_ticks);
      }
   }

   thin_timer_shutdown();

   return 0;
}

int test::testEvents()
{
   thin_event_t event;
   bool done = false;

   thin_mouse_init(80, 20, 1);
   thin_joy_init();
   thin_dpp_init();
   thin_dpp_add(0x378, 0);

   thin_printf("event test: press ESC...");

   while (!done)
   {
      thin_event_gather();

      while (thin_event_get(&event))
      {
         switch (event.type)
         {
         case THIN_KEY_PRESS:
            if (event.data.keysym == THIN_KEY_ESC)
               done = true;
            thin_printf("key press\n");
            break;

         case THIN_KEY_RELEASE:
            thin_printf("key release\n");
            break;

         case THIN_MOUSE_MOTION:
            thin_printf("mouse motion\n");
            break;

         case THIN_MOUSE_BUTTON_PRESS:
            thin_printf("mouse button press\n");
            break;

         case THIN_MOUSE_BUTTON_RELEASE:
            thin_printf("mouse button release\n");
            break;

         case THIN_JOY_MOTION:
            thin_printf("joy motion\n");
            break;

         case THIN_JOY_BUTTON_PRESS:
            thin_printf("joy button press\n");
            break;

         case THIN_JOY_BUTTON_RELEASE:
            thin_printf("joy button release\n");
            break;

         default:
            break;
         }
      }
   }

   thin_dpp_shutdown();
   thin_joy_shutdown();
   thin_mouse_shutdown();

   return 0;
}

void test::run()
{
   if (testSound())
      return;

   if (testVideo())
      return;

   if (testTimer())
      return;

   if (testEvents())
      return;

   thin_printf("\ntest complete.\n");
}

int main(void)
{
   test *pTest = new test;

   pTest->run();

   delete pTest;

   return 0;
}

/*
** $Log: thintest.cpp,v $
** Revision 1.3  2001/03/12 06:06:55  matt
** better keyboard driver, support for bit depths other than 8bpp
**
** Revision 1.2  2001/02/01 06:28:26  matt
** thinlib now works under NT/2000
**
** Revision 1.1  2001/01/15 05:27:43  matt
** initial revision
**
*/
