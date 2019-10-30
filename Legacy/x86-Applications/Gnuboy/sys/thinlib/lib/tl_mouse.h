/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_mouse.h
**
** DOS mouse handling defines / prototypes
**
** $Id: $
*/

#ifndef _TL_MOUSE_H_
#define _TL_MOUSE_H_

#include "tl_types.h"

/* mouse buttons */
enum
{
   THIN_MOUSE_LEFT   = 0,
   THIN_MOUSE_MIDDLE,
   THIN_MOUSE_RIGHT,
   THIN_MOUSE_MAX_BUTTONS,
};

#define  THIN_MOUSE_BUTTON_MASK(x)  (1 << (x))

extern int thin_mouse_init(int width, int height, int delta_shift);
extern void thin_mouse_shutdown();

extern void thin_mouse_setrange(int width, int height);

extern uint8 thin_mouse_getmotion(int *dx, int *dy);
extern uint8 thin_mouse_getpos(int *x, int *y);

#endif /* !_TL_MOUSE_H_ */

/*
** $Log: $
*/
