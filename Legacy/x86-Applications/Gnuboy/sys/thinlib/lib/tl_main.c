/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_main.c
**
** main library init / shutdown code
**
** $Id: $
*/

#include "tl_types.h"
#include "thinlib.h"

#include "tl_timer.h"

#include "tl_event.h"
#include "tl_key.h"
#include "tl_joy.h"
#include "tl_dpp.h"

#include "tl_sound.h"
#include "tl_video.h"
#include "tl_djgpp.h"

#include <crt0.h>
#include <stdlib.h>

/* our global "near pointer" flag. */
int thinlib_nearptr = 0;
static int initialized_flags = 0;
static bool shutdown_called = false;

typedef void (*funcptr_t)(void);
#define  MAX_EXIT_FUNCS    32

static funcptr_t exit_func[MAX_EXIT_FUNCS];

void thin_add_exit(void (*func)(void))
{
   int i;

   for (i = 0; i < MAX_EXIT_FUNCS; i++)
   {
      if (NULL == exit_func[i] || func == exit_func[i])
      {
         exit_func[i] = func;
         break;
      }
   }
}

void thin_remove_exit(void (*func)(void))
{
   int i;

   for (i = 0; i < MAX_EXIT_FUNCS; i++)
   {
      if (func == exit_func[i])
      {
         while (i < MAX_EXIT_FUNCS - 1)
         {
            exit_func[i] = exit_func[i - 1];
            i++;
         }
         exit_func[MAX_EXIT_FUNCS - 1] = NULL;
         break;
      }
   }
}

int thin_init(int devices)
{
   int success = devices;

   /* set up our crt0 flags the way we want them.  This might be a
   ** bit too late (i.e. crt0.s/stub has already been executed), but
   ** we might as well try.
   */
   _crt0_startup_flags &= ~_CRT0_FLAG_UNIX_SBRK;
   _crt0_startup_flags |= _CRT0_FLAG_NONMOVE_SBRK;

   /* Try to enable near pointers through djgpp's default mechanism.
   ** This allows us to manipulate memory-mapped devices (sound cards,
   ** video cards, etc.) as if they were regular memory addresses, at
   ** the cost of disabling memory protection.  Win NT and 2000 strictly
   ** disallow near pointers, so we need to flag this.
   */
	thinlib_nearptr = __djgpp_nearptr_enable();

   /* open up event handler */
   thin_event_init();

   if (devices & THIN_KEY)
   {
      if (thin_key_init())
         success &= ~THIN_KEY;
   }

   if (devices & THIN_JOY)
   {
      if (thin_joy_init())
         success &= ~THIN_JOY;
   }

   if (devices & THIN_DPP)
   {
      if (thin_dpp_init())
         success &= ~THIN_DPP;
   }

   /* THIN_SOUND, THIN_VIDEO, THIN_TIMER implicitly successful.. */

   initialized_flags = success;

   /* set up our atexit routine */
   atexit(thin_shutdown);
   shutdown_called = false;

   return success;
}

void thin_shutdown(void)
{
   int i;

   /* thin_shutdown is an atexit() registered routine, so make sure
   ** that we don't get called multiple times.
   */
   if (shutdown_called)
      return;

   for (i = MAX_EXIT_FUNCS - 1; i >= 0; i--)
   {
      if (exit_func[i])
      {
         exit_func[i]();
         exit_func[i] = NULL;
      }
   }

   /* not started from thin_init */
   thin_sound_shutdown();
   thin_timer_shutdown();
   thin_vid_shutdown();

   /* started from thin_init... */
   if (initialized_flags & THIN_KEY)
      thin_key_shutdown();

   if (initialized_flags & THIN_JOY)
      thin_joy_shutdown();

   if (initialized_flags & THIN_DPP)
      thin_dpp_shutdown();

   /* back to memory protection, if need be */
   if (thinlib_nearptr)
      __djgpp_nearptr_disable();

   shutdown_called = true;
}

/* Reduce size of djgpp executable */
char **__crt0_glob_function(char *_argument) 
{
   UNUSED(_argument);
   return (char **) 0;
}

/* Reduce size of djgpp executable */
void __crt0_load_environment_file(char *_app_name)
{
   UNUSED(_app_name);
}

/*
** $Log: $
*/
