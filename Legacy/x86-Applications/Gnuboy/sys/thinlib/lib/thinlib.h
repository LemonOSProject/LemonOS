/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** thinlib.h
**
** main library header
**
** $Id: $
*/

#ifndef _THINLIB_H_
#define _THINLIB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* types */
#include "tl_types.h"
#include "tl_log.h"
#include "tl_prof.h"

/* system */
#include "tl_djgpp.h"
#include "tl_int.h"
#include "tl_timer.h"

/* input/events */
#include "tl_event.h"
#include "tl_key.h"
#include "tl_mouse.h"
#include "tl_joy.h"
#include "tl_dpp.h"

/* video */
#include "tl_bmp.h"
#include "tl_video.h"

/* audio */
#include "tl_sound.h"

#define  THIN_KEY       0x0001
#define  THIN_MOUSE     0x0002
#define  THIN_JOY       0x0004
#define  THIN_DPP       0x0008
#define  THIN_TIMER     0x0010
#define  THIN_VIDEO     0x0020
#define  THIN_SOUND     0x0040


/* main interface */
extern int thin_init(int devices);
extern void thin_shutdown(void);

extern void thin_add_exit(void (*func)(void));
extern void thin_remove_exit(void (*func)(void));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_THINLIB_H */

/*
** $Log: $
*/
