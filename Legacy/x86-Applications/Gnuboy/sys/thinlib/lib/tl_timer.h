/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_timer.h
**
** DOS timer routine defines / prototypes
**
** $Id: $
*/

#ifndef _TL_TIMER_H_
#define _TL_TIMER_H_

typedef void (*timerhandler_t)(void *param);

extern int thin_timer_init(int hertz, timerhandler_t func_ptr, void *param);
extern void thin_timer_shutdown(void);
extern void thin_timer_setrate(int hertz);

#endif /* !_TL_TIMER_H_ */

/*
** $Log: $
*/
