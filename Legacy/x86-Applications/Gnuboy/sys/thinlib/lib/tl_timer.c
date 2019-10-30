/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_timer.c
**
** DOS timer routines
**
** $Id: $
*/

#include <go32.h>
#include <pc.h>
#include <dpmi.h>

#include "tl_types.h"
#include "tl_djgpp.h"
#include "tl_timer.h"

#define  TIMER_INT         0x08
#define  TIMER_TICKS       1193182L

#define  TIMER_CONTROL     0x43
#define  TIMER_ACCESS      0x40
#define  TIMER_DEFAULT_MODE   0x36

static _go32_dpmi_seginfo old_handler, new_handler;

static struct
{
   timerhandler_t handler;
   void *param;
   int interval, frac, current;
   uint8 current_mode;
   bool initialized;
} timer;

/* port 0x43 - write control word
** port 0x40 - read/write timer count
** control word composition:
** D7-D6 - 0=counter0, 1=counter1, 2=counter2, 3=read-back
** D5-D4 - 0=latch, 1=r/w lsb, 2=r/w msb, 3=r/2 lsb then msb
** D3-D1 - 0=m0, 1=m1, 2/6=m2, 3/7=m3, 4=m4, 5=m5
** D0    - 0=16-bit binary counter, 1=BCD counter
**
** mode 0 - interrupt on terminal count
** mode 1 - hardware retriggerable one-shot
** mode 2 - rate generator
** mode 3 - square wave mode
** mode 4 - software triggered mode
** mode 5 - hardware triggered strobe (retriggerable)
*/


/* Reprogram the PIT timer to fire at a specified value */
void thin_timer_setrate(int hertz)
{
   int time;

   if (0 == hertz)
      time = 0;
   else
      time = TIMER_TICKS / (long) hertz;

   timer.interval = time;
   timer.frac = time & 0xFFFF;

   if (0 == time)
      timer.current_mode = TIMER_DEFAULT_MODE; /* reset to standard */

   outportb(TIMER_CONTROL, timer.current_mode);
   outportb(TIMER_ACCESS, timer.frac & 0xFF);
   outportb(TIMER_ACCESS, timer.frac >> 8);
}

static void _timer_int_handler(void)
{
   timer.current += timer.frac;
   if (timer.current >= timer.interval)
   {
      timer.current -= timer.interval;
      timer.handler(timer.param);
   }

   outportb(0x20, 0x20);
}
THIN_LOCKED_STATIC_FUNC(_timer_int_handler);

/* Lock code, data, and chain an interrupt handler */
int thin_timer_init(int hertz, timerhandler_t func_ptr, void *param)
{
   if (false == timer.initialized)
   {
      THIN_LOCK_FUNC(_timer_int_handler);
      THIN_LOCK_VAR(timer);

      timer.handler = func_ptr;
      timer.param = param;

      /* chain onto old timer interrupt */
      _go32_dpmi_get_protected_mode_interrupt_vector(TIMER_INT, &old_handler);
      new_handler.pm_offset = (int) _timer_int_handler;
      new_handler.pm_selector = _go32_my_cs();
      _go32_dpmi_chain_protected_mode_interrupt_vector(TIMER_INT, &new_handler);

      timer.current = 0;

      /* Set PIC to fire at desired refresh rate */

      /* counter 0, lsb+msb, mode 3, binary counter */
      timer.current_mode = 0x36;

      timer.initialized = true;
   }

   thin_timer_setrate(hertz);

   return 0;
}

/* Remove the timer handler */
void thin_timer_shutdown(void)
{
   if (false == timer.initialized)
      return;

   /* Restore previous timer setting */
   thin_timer_setrate(0);

   /* Remove the interrupt handler */
   _go32_dpmi_set_protected_mode_interrupt_vector(TIMER_INT, &old_handler);

   timer.initialized = false;
}

/*
** $Log: $
*/
