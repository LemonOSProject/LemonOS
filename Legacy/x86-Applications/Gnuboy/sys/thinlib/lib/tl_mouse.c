/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_mouse.c
**
** DOS mouse handling routines
**
** $Id: $
*/

/* TODO: add events to motion/button presses. */
/* TODO: mouse interrupt based? */

#include <stdio.h>
#include <dpmi.h>
#include <go32.h>

#include "tl_types.h"
#include "tl_mouse.h"
#include "tl_event.h"

#define  MOUSE_FIX            8  // 24.8 fixpoint

#define  MOUSE_INT            0x33
#define  INT_GET_MICKEYS      0x0B
#define  INT_GET_BUTTONS      0x03

static struct mouse_s
{
   int xpos, ypos;
   int xdelta, ydelta;
   int maxwidth, maxheight;
   int num_buttons;
   int delta_shift;
   uint8 button;
   bool enabled;
   event_id id;
} mouse;

static void _get_mickeys(int *dx, int *dy)
{
   __dpmi_regs r;

   /* get mickeys */
   r.x.ax = INT_GET_MICKEYS;
   __dpmi_int(MOUSE_INT, &r);
   *dx = (int16) r.x.cx;
   *dy = (int16) r.x.dx;
}

static uint8 _get_buttons(void)
{
   __dpmi_regs r;
   uint8 left, middle, right;
   
   r.x.ax = INT_GET_BUTTONS;
   __dpmi_int(MOUSE_INT, &r);

   left = (r.x.bx & 1);
   right = ((r.x.bx >> 1) & 1);
   middle = ((r.x.bx >> 2) & 1);
   
   return (right << THIN_MOUSE_RIGHT
           | middle << THIN_MOUSE_MIDDLE
           | left << THIN_MOUSE_LEFT);
}

static void _mouse_poll(void)
{
   int mick_x, mick_y;
   int old_x, old_y;
   int old_button;

   if (false == mouse.enabled)
      return;

   _get_mickeys(&mick_x, &mick_y);

   mick_x <<= (MOUSE_FIX - mouse.delta_shift);
   mick_y <<= (MOUSE_FIX - mouse.delta_shift);

   old_x = mouse.xpos;
   old_y = mouse.ypos;
   mouse.xpos += mick_x;
   mouse.ypos += mick_y;

   if (mouse.xpos < 0)
      mouse.xpos = 0;
   else if (mouse.xpos > mouse.maxwidth)
      mouse.xpos = mouse.maxwidth;

   if (mouse.ypos < 0)
      mouse.ypos = 0;
   else if (mouse.ypos > mouse.maxheight)
      mouse.ypos = mouse.maxheight;

   mick_x = mouse.xpos - old_x;
   mick_y = mouse.ypos - old_y;
   mouse.xdelta += mick_x;
   mouse.ydelta += mick_y;

   old_button = mouse.button;
   mouse.button = _get_buttons();

   /* if our delta really changed, add an event */
   if (0 != mick_x || 0 != mick_y)
   {
      thin_event_t event;

      event.type = THIN_MOUSE_MOTION;
      event.data.mouse_motion.xpos = mouse.xpos;
      event.data.mouse_motion.ypos = mouse.ypos;

      thin_event_add(&event);
   }

   /* if button state changed, add applicable events */
   if (old_button != mouse.button)
   {
      thin_event_t event;
      int i;

      for (i = 0; i < THIN_MOUSE_MAX_BUTTONS; i++)
      {
         /* TODO: this is kind of krunky.  a separate event for
         ** every button, but return the state of all buttons?  
         ** bleh.
         */
         if ((old_button & (1 << i)) != (mouse.button & (1 << i)))
         {
            event.type = (mouse.button & (1 << i)) 
                         ? THIN_MOUSE_BUTTON_PRESS : THIN_MOUSE_BUTTON_RELEASE;
            event.data.mouse_button = mouse.button;

            thin_event_add(&event);
         }
      }
   }
}

uint8 thin_mouse_getmotion(int *dx, int *dy)
{
   *dx = mouse.xdelta >> MOUSE_FIX;
   *dy = mouse.ydelta >> MOUSE_FIX;
   mouse.xdelta = 0;
   mouse.ydelta = 0;
   return mouse.button;
}

uint8 thin_mouse_getpos(int *x, int *y)
{
   *x = mouse.xpos >> MOUSE_FIX;
   *y = mouse.ypos >> MOUSE_FIX;
   return mouse.button;
}


void thin_mouse_setrange(int width, int height)
{
   mouse.maxwidth = (width - 1) << MOUSE_FIX;
   mouse.maxheight = (height - 1) << MOUSE_FIX;
   mouse.xpos = (width / 2) << MOUSE_FIX;
   mouse.ypos = (height / 2) << MOUSE_FIX;
   mouse.xdelta = 0;
   mouse.ydelta = 0;
}


void thin_mouse_shutdown(void)
{
   if (-1 != mouse.id)
   {
      thin_event_remove_callback(mouse.id);
      mouse.id = -1;
      mouse.enabled = false;
   }
}


/* Set up mouse, center pointer */
int thin_mouse_init(int width, int height, int delta_shift)
{
   __dpmi_regs r;

   r.x.ax = 0x00;
   __dpmi_int(MOUSE_INT, &r);

   if (0 == r.x.ax)
   {
      mouse.enabled = false;
      mouse.id = -1;
      return -1;
   }

   mouse.enabled = true;

   mouse.num_buttons = r.x.bx;
   if (r.x.bx == 0xFFFF)
      mouse.num_buttons = 2;
   else if (mouse.num_buttons > 3)
      mouse.num_buttons = 3;

   mouse.delta_shift = delta_shift;

   mouse.button = 0;

   thin_mouse_setrange(width, height);

   /* set it up for the event handling */
   mouse.id = thin_event_add_callback((event_callback_t) _mouse_poll);
   if (-1 == mouse.id)
   {
      mouse.enabled = false;
      return -1;
   }

   return 0;
}

/*
** $Log: $
*/
