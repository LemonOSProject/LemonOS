/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_event.h
**
** event handling routines
**
** $Id: $
*/

#ifndef _TL_EVENT_H_
#define _TL_EVENT_H_

typedef void (*event_callback_t)(void);
typedef int event_id;

enum
{
   THIN_NOEVENT = 0,
   THIN_KEY_PRESS,
   THIN_KEY_RELEASE,
   THIN_MOUSE_MOTION,
   THIN_MOUSE_BUTTON_PRESS,
   THIN_MOUSE_BUTTON_RELEASE,
   THIN_JOY_MOTION,
   THIN_JOY_BUTTON_PRESS,
   THIN_JOY_BUTTON_RELEASE,
   THIN_USER_EVENT,
};

enum
{
   THIN_JOY_LEFT,
   THIN_JOY_RIGHT,
   THIN_JOY_UP,
   THIN_JOY_DOWN,
};

typedef struct thin_event_s
{
   int type;
   union
   {
      /* keyboard */
      int keysym;
      /* mouse motion */
      struct 
      {
         int xpos;
         int ypos;
      } mouse_motion;
      /* mouse button */
      int mouse_button;
      /* joy motion */
      struct
      {
         int dir;
         int state;
      } joy_motion;
      /* joy button */
      int joy_button;
      /* user event */
      int user_data;
   } data;
} thin_event_t;

extern void       thin_event_add(thin_event_t *event);
extern int        thin_event_get(thin_event_t *event);
extern void       thin_event_gather(void);
extern event_id   thin_event_add_callback(event_callback_t callback);
extern void       thin_event_remove_callback(event_id id);
extern void       thin_event_init(void);


#endif /* !_TL_EVENT_H_ */

/*
** $Log: $
*/
